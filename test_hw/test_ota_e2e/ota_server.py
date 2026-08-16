# Copyright (c) 2026. See parent project license.

"""Local HTTPS fixture server for the OTA hardware test.

Serves:
  /ota_manifest.json  - the OTA manifest pointing at /firmware.bin
  /firmware.bin       - the (synthetic) firmware image

The firmware under test must be built with
  -DOTA_MANIFEST_URL=https://<host>:<port>/ota_manifest.json
so the OTA manager polls this server instead of the production GitHub
release asset. The server uses a self-signed certificate generated on the
fly (via openssl) or supplied via --cert/--key.

Run:
    python test_hw/test_ota_e2e/ota_server.py \
        --firmware test_hw/test_ota_e2e/fw.bin --version 9.9.9 --port 8443
"""

import argparse
import hashlib
import http.server
import json
import os
import ssl
import subprocess
import sys
import tempfile


class OtaServer(http.server.ThreadingHTTPServer):
    """ThreadingHTTPServer with the fixture state attached."""

    def __init__(self, addr, handler, firmware, manifest, redirect):
        super().__init__(addr, handler)
        self.firmware = firmware
        self.manifest = manifest
        self.redirect = redirect


class OtaHandler(http.server.BaseHTTPRequestHandler):
    server: OtaServer

    def do_GET(self):  # noqa: N802
        if self.path == "/ota_manifest.json":
            self._send_json(self.server.manifest)
        elif self.path in ("/firmware.bin", "/firmware.bin?redirect=1"):
            if self.server.redirect:
                # Simulate the GitHub release-asset 302 the firmware follows.
                self.send_response(302)
                self.send_header("Location", "/firmware.bin")
                self.end_headers()
                return
            self._send_bytes(self.server.firmware, "application/octet-stream")
        else:
            self.send_error(404)

    def _send_json(self, obj):
        body = json.dumps(obj).encode()
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def _send_bytes(self, data, content_type):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def log_message(self, fmt, *args):  # noqa: A003
        sys.stderr.write("[ota-server] " + (fmt % args) + "\n")


def _make_self_signed(cert_dir):
    """Generate a self-signed cert valid for localhost/127.0.0.1 via openssl."""
    cert = os.path.join(cert_dir, "cert.pem")
    key = os.path.join(cert_dir, "key.pem")
    subprocess.run(
        [
            "openssl",
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-keyout",
            key,
            "-out",
            cert,
            "-days",
            "30",
            "-nodes",
            "-subj",
            "/CN=localhost",
            "-addext",
            "subjectAltName=DNS:localhost,IP:127.0.0.1",
        ],
        check=True,
    )
    return cert, key


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", required=True, help="path to the synthetic firmware binary")
    parser.add_argument("--version", default="9.9.9", help="version in the manifest")
    parser.add_argument("--port", type=int, default=8443)
    parser.add_argument("--sha256", default="", help="override the manifest SHA-256")
    parser.add_argument(
        "--corrupt-sha",
        action="store_true",
        help="publish a wrong SHA-256 to exercise the abort path",
    )
    parser.add_argument(
        "--redirect", action="store_true", help="emit a 302 before the firmware (GitHub-style)"
    )
    parser.add_argument("--cert", default="")
    parser.add_argument("--key", default="")
    args = parser.parse_args()

    with open(args.firmware, "rb") as f:
        firmware = f.read()
    sha = args.sha256 or hashlib.sha256(firmware).hexdigest()
    if args.corrupt_sha:
        sha = "0" * 64

    manifest = {
        "version": args.version,
        "firmware_url": f"https://127.0.0.1:{args.port}/firmware.bin",
        "sha256": sha,
    }

    tmp = tempfile.mkdtemp(prefix="ota-srv-")
    cert, key = args.cert, args.key
    if not cert or not key:
        cert, key = _make_self_signed(tmp)

    httpd = OtaServer(("0.0.0.0", args.port), OtaHandler, firmware, manifest, args.redirect)

    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)

    print(f"[ota-server] listening on https://0.0.0.0:{args.port}", flush=True)
    print(f"[ota-server] manifest: {json.dumps(manifest)}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()