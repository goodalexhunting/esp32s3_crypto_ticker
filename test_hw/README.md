# Hardware-in-the-Loop Tests

These tests exercise the real ESP32-S3 device and can only run with physical
hardware. They are **not** part of the CI `pio test -e native` job.

## Requirements

- Python 3.9+ with `pytest`, `requests`, and `pyserial`
  (`pip install pytest requests pyserial`)
- A flashed LilyGo T-Display-S3 running the firmware from this repo
- For the OTA tests: a local HTTPS server the device can reach on the same
  network (see `test_ota_e2e/`)
- A test Wi-Fi network for the captive-portal /connect tests

## Running

```sh
# 1. Captive portal (device must be in AP mode, no stored credentials)
pytest -s test_hw/test_captive_portal.py

# 2. Ticker config API (device must be connected to your Wi-Fi)
pytest -s test_hw/test_config_api.py --device 192.168.1.50

# 3. Real CoinGecko data (device must be connected to your Wi-Fi)
pytest -s test_hw/test_crypto_real.py --device 192.168.1.50

# 4. OTA end-to-end (device must be connected to your Wi-Fi)
pytest -s test_hw/test_ota_e2e/test_ota.py \
  --device 192.168.1.50 \
  --ota-url https://<local-server-ip>:8443
```

The OTA tests need the bundled HTTPS fixture server:

```sh
# Serve a fake firmware + manifest over HTTPS on :8443
python test_hw/test_ota_e2e/ota_server.py --firmware test_hw/test_ota_e2e/fw.bin \
  --version 9.9.9 --port 8443
```

> **NOTE:** the device firmware must be compiled with an OTA manifest URL
> pointing at your fixture server (`-DOTA_MANIFEST_URL=...`) so the OTA
> manager polls it instead of the production GitHub release asset. The
> device verifies OTA TLS connections against the pinned CA in
> `include/ota_certs.h` (`OTA_CA_CERT_PEM`), so the fixture's self-signed
> certificate must also be embedded in the build via `-DOTA_CA_CERT_PEM`.
> Generate the cert first, then build with `PLATFORMIO_BUILD_FLAGS` and
> start the server with `--cert/--key` using that same cert. See the test
> docstring in `test_ota.py` for exact flags.

## Test matrix

| Test | Covers | Key assertions |
|------|--------|----------------|
| `test_captive_portal.py` | AP-mode web UI, captive portal redirect, /connect flow, /scan JSON | `GET /` + `Cache-Control: no-store`, 302 redirect to `http://<ap-ip>/`, 200/400/401 statuses, /scan parses as JSON |
| `test_ota_e2e/test_ota.py` | Full HTTPS OTA download + SHA-256 verify + flash + reboot | new version boots and disarms rollback; checksum mismatch aborts leaving old firmware; stale manifest -> `UP_TO_DATE` (no download) |
| `test_config_api.py` | Async config server REST API on a live device | GET/POST/DELETE/move/reset tickers, 400 on bad input, 404, `/` redirect to GitHub Pages |
| `test_crypto_real.py` | Real CoinGecko end-to-end | `BTC price > 0`, 24h change present, history buffer populated for 7d |