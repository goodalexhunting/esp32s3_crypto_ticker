# Copyright (c) 2026. See parent project license.

"""Hardware-in-the-loop test for the AP-mode captive portal.

Prereq: the device must be in AP mode (no stored WiFi credentials, or after
an erase). The AP SSID is 'CryptoTicker-XXXX' where XXXX is the tail of the
device's efuse MAC printed on the QR screen.

Run:
    pytest -s test_hw/test_captive_portal.py \
        [--ap-ip 192.168.4.1] [--test-ssid MyNet --test-pass secret]
"""

import json

import pytest
import requests


def _ap_ip(request):
    return request.config.getoption("--ap-ip")


def _session():
    return requests.Session()


def pytest_addoption(parser):
    parser.addoption("--ap-ip", default="192.168.4.1", help="AP IP address of the device")
    parser.addoption("--test-ssid", default="", help="A real Wi-Fi network to join")
    parser.addoption("--test-pass", default="", help="Its password")


@pytest.fixture(scope="session")
def test_wifi_ssid(request):
    v = request.config.getoption("--test-ssid")
    if not v:
        pytest.skip("--test-ssid not provided")
    return v


@pytest.fixture(scope="session")
def test_wifi_pass(request):
    return request.config.getoption("--test-pass") or ""


# ---------------------------------------------------------------------------
# Root page
# ---------------------------------------------------------------------------


def test_root_serves_config_page(request):
    ip = _ap_ip(request)
    r = _session().get(f"http://{ip}/", timeout=5)
    assert r.status_code == 200
    assert r.headers.get("Cache-Control") == "no-store"
    assert "text/html" in r.headers["Content-Type"]
    assert "<html" in r.text.lower()


# ---------------------------------------------------------------------------
# Captive portal redirect
# ---------------------------------------------------------------------------


def test_unknown_path_redirects_to_ap_root(request):
    """Any non-registered path must 302 to http://<ap-ip>/."""
    ip = _ap_ip(request)
    r = _session().get(f"http://{ip}/captive-portal/whatever", timeout=5, allow_redirects=False)
    assert r.status_code == 302
    assert r.headers["Location"] == f"http://{ip}/"


# ---------------------------------------------------------------------------
# /scan
# ---------------------------------------------------------------------------


def test_scan_returns_valid_json(request):
    ip = _ap_ip(request)
    r = _session().get(f"http://{ip}/scan", timeout=10)
    assert r.status_code == 200
    assert r.headers.get("Cache-Control") == "no-store"
    assert "application/json" in r.headers["Content-Type"]
    nets = json.loads(r.text)
    assert isinstance(nets, list)
    for net in nets:
        assert "ssid" in net
        assert "rssi" in net
        assert "encrypt" in net


# ---------------------------------------------------------------------------
# /connect
# ---------------------------------------------------------------------------


def test_connect_missing_ssid_400(request):
    ip = _ap_ip(request)
    r = _session().post(f"http://{ip}/connect", data={"pass": "x"}, timeout=5)
    assert r.status_code == 400


def test_connect_wrong_credentials_401(request, test_wifi_ssid, test_wifi_pass):
    """POST with bad credentials must 401 and keep the AP alive."""
    ip = _ap_ip(request)
    r = _session().post(
        f"http://{ip}/connect",
        data={"ssid": test_wifi_ssid, "pass": test_wifi_pass + "-definitely-wrong"},
        timeout=30,
    )
    assert r.status_code == 401


def test_connect_valid_credentials_200(request, test_wifi_ssid, test_wifi_pass):
    """POST with correct credentials must 200 and the device joins the STA.

    After this test the device leaves AP mode and joins test_wifi_ssid, so
    run it last.
    """
    ip = _ap_ip(request)
    r = _session().post(
        f"http://{ip}/connect",
        data={"ssid": test_wifi_ssid, "pass": test_wifi_pass},
        timeout=60,
    )
    assert r.status_code == 200
    assert r.text == "OK"