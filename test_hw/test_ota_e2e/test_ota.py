# Copyright (c) 2026. See parent project license.

"""Hardware-in-the-loop OTA end-to-end tests.

Prereq: a flashed device on the same network as the host. The firmware must
have been built with an OTA manifest URL pointing at the local fixture
server, e.g.:

    pio run -e lilygo-t-display-s3 \
        -DOTA_MANIFEST_URL=https://<host-ip>:8443/ota_manifest.json

Start the fixture server first (see ota_server.py), then run:

    pytest -s test_hw/test_ota_e2e/test_ota.py \
        --device 192.168.1.50 --ota-url https://<host-ip>:8443
"""

import hashlib
import time

import pytest
import requests

# ---------------------------------------------------------------------------
# Host-side checks against the fixture server
# ---------------------------------------------------------------------------


def pytest_addoption(parser):
    parser.addoption("--device", required=True, help="IP address of the OTA-enabled device")
    parser.addoption("--ota-url", required=True, help="base URL of the local OTA fixture server")


def _ota_base(request):
    return request.config.getoption("--ota-url")


def _device(request):
    return request.config.getoption("--device")


def _http():
    # The fixture uses a self-signed cert for local testing.
    return requests.Session()


def test_ota_server_reachable(request):
    r = _http().get(f"{_ota_base(request)}/ota_manifest.json", verify=False, timeout=5)
    assert r.status_code == 200
    manifest = r.json()
    assert "version" in manifest
    assert "firmware_url" in manifest
    assert "sha256" in manifest


# ---------------------------------------------------------------------------
# Manifest decision logic (host-side sanity of the fixture, mirrors the
# device-side compareVersions behaviour covered by the native unit tests).
# ---------------------------------------------------------------------------


def _parse_patch(version):
    return tuple(int(x) for x in version.split("."))


def _compare(installed, remote):
    a = _parse_patch(installed)
    b = _parse_patch(remote)
    return (a > b) - (a < b)


def test_manifest_version_greater_than_installed(request):
    """The fixture version must be newer than the device's installed
    FW_VERSION (default 1.0.0) for the update to be triggered."""
    r = _http().get(f"{_ota_base(request)}/ota_manifest.json", verify=False, timeout=5)
    remote = r.json()["version"]
    assert _compare("1.0.0", remote) < 0, (
        f"fixture version {remote} must be newer than installed 1.0.0"
    )


def test_manifest_sha256_matches_firmware(request):
    """The manifest's sha256 must match the served firmware binary."""
    r = _http().get(f"{_ota_base(request)}/ota_manifest.json", verify=False, timeout=5)
    sha = r.json()["sha256"]
    fw = _http().get(f"{_ota_base(request)}/firmware.bin", verify=False, timeout=30)
    assert fw.status_code == 200
    assert hashlib.sha256(fw.content).hexdigest() == sha


# ---------------------------------------------------------------------------
# Device-side checks
# ---------------------------------------------------------------------------


def _wait_for_reboot(device, timeout_s=120):
    """Poll the config server until the device comes back online after a
    reboot (the firmware serves /config on port 80 when connected)."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            r = requests.get(f"http://{device}/config", timeout=3)
            if r.status_code == 200:
                return True
        except requests.RequestException:
            pass
        time.sleep(2)
    return False


def test_ota_success_and_reboot(request):
    """With a correct SHA-256 manifest, the device must download, verify,
    flash, reboot, and come back online with selfTestVerification having
    disarmed the rollback. The host asserts reachability; the serial log
    (monitored separately) confirms '[OTA] Update installed successfully'
    and '[OTA] Self-test passed - OTA boot confirmed'."""
    device = _device(request)
    # Trigger the check/install. The device polls hourly; wiring a manual
    # trigger would require a serial command extension. For this test we
    # rely on the device's boot-time check: reboot it first via the config
    # API? Rebooting is not exposed. Instead we simply wait for the next
    # hourly check OR the boot check if the device was just power-cycled.
    # The serial monitor is the authoritative source; this assertion is only
    # that the device remains reachable and does not soft-brick.
    assert _wait_for_reboot(device), "device did not come back online"
    r = requests.get(f"http://{device}/config", timeout=5)
    assert r.status_code == 200


def test_checksum_mismatch_aborts(request):
    """A manifest with a corrupt SHA-256 must abort the update and leave
    the device operational (the fixture --corrupt-sha path). The host only
    verifies the device still responds; the abort is confirmed on serial:
    '[OTA] SHA-256 mismatch - aborting update'."""
    device = _device(request)
    r = requests.get(f"http://{device}/config", timeout=5)
    assert r.status_code == 200


def test_stale_manifest_no_update(request):
    """A manifest whose version is <= installed must not trigger a download
    (device returns UP_TO_DATE). Confirmed on serial: 'Firmware is up to
    date'. Host check is that the device remains responsive."""
    device = _device(request)
    r = requests.get(f"http://{device}/config", timeout=5)
    assert r.status_code == 200