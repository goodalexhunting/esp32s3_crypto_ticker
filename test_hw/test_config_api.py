# Copyright (c) 2026. See parent project license.

"""Hardware-in-the-loop test for the ticker configuration REST API.

Prereq: the device must be connected to a Wi-Fi network (not AP mode) and
reachable at --device. The firmware serves the AsyncWebServer config API on
port 80.

Run:
    pytest -s test_hw/test_config_api.py --device 192.168.1.50
"""

import json

import pytest
import requests


def _base(request):
    return f"http://{request.config.getoption('--device')}"


def pytest_addoption(parser):
    parser.addoption("--device", required=True, help="IP address of the configured device")


def _get(request, path, **kw):
    return requests.get(_base(request) + path, timeout=5, **kw)


def _post(request, path, **kw):
    return requests.post(_base(request) + path, timeout=5, **kw)


def _delete(request, path, **kw):
    return requests.delete(_base(request) + path, timeout=5, **kw)


def _tickers(request):
    r = _get(request, "/api/tickers")
    assert r.status_code == 200
    return json.loads(r.text)


# ---------------------------------------------------------------------------
# Discovery / landing page
# ---------------------------------------------------------------------------


def test_root_redirects_to_github_pages(request):
    r = _get(request, "/", allow_redirects=False)
    assert r.status_code == 302
    loc = r.headers["Location"]
    assert "goodalexhunting.github.io" in loc
    assert "device=crypto-ticker.local" in loc


def test_config_page_served(request):
    r = _get(request, "/config")
    assert r.status_code == 200
    assert "text/html" in r.headers["Content-Type"]
    assert "<html" in r.text.lower()


def test_unknown_route_404(request):
    r = _get(request, "/no/such/route")
    assert r.status_code == 404


# ---------------------------------------------------------------------------
# /api/tickers CRUD
# ---------------------------------------------------------------------------

UID_COUNTER = [0]


def _fresh_id() -> str:
    """A unique CoinGecko id so repeat runs never collide with an existing one."""
    UID_COUNTER[0] += 1
    return f"testcoin{UID_COUNTER[0]}"


def test_list_tickers(request):
    arr = _tickers(request)
    assert isinstance(arr, list)
    assert len(arr) >= 1
    for t in arr:
        assert "label" in t
        assert "apiId" in t
        assert "quote" in t
        assert "color" in t


def test_add_ticker_success(request):
    uid = _fresh_id()
    r = _post(
        request,
        "/api/tickers",
        data={"label": "TST", "apiId": uid, "quote": "usd", "color": "0xFFFF"},
    )
    assert r.status_code == 200
    arr = json.loads(r.text)
    assert any(t["apiId"] == uid for t in arr)

    # Clean up so the same device can be re-tested.
    idx = next(i for i, t in enumerate(arr) if t["apiId"] == uid)
    r2 = _delete(request, f"/api/tickers?id={idx}")
    assert r2.status_code == 200


def test_add_ticker_missing_fields_400(request):
    r = _post(request, "/api/tickers", data={"label": "X", "apiId": "x"})
    assert r.status_code == 400


def test_add_duplicate_ticker_400(request):
    arr = _tickers(request)
    if not arr:
        pytest.skip("no tickers configured to duplicate")
    first = arr[0]
    r = _post(
        request,
        "/api/tickers",
        data={"label": first["label"], "apiId": first["apiId"], "quote": first["quote"]},
    )
    assert r.status_code == 400


def test_remove_invalid_id_400(request):
    r = _delete(request, "/api/tickers?id=9999")
    assert r.status_code == 400


def test_move_invalid_from_400(request):
    r = _post(request, "/api/tickers/move", data={"from": "9999", "to": "0"})
    assert r.status_code == 400


def test_move_valid(request):
    arr = _tickers(request)
    if len(arr) < 2:
        pytest.skip("need at least 2 configured tickers to move")
    from_i = 0
    to_i = len(arr) - 1
    r = _post(request, "/api/tickers/move", data={"from": str(from_i), "to": str(to_i)})
    assert r.status_code == 200
    moved = json.loads(r.text)
    # The ticker that was at 0 must now be at the end.
    assert moved[-1]["apiId"] == arr[0]["apiId"]
    # Move it back to keep the device state unchanged.
    r2 = _post(request, "/api/tickers/move", data={"from": str(to_i), "to": "0"})
    assert r2.status_code == 200


def test_reset_returns_defaults(request):
    r = _post(request, "/api/tickers/reset")
    assert r.status_code == 200
    arr = json.loads(r.text)
    assert isinstance(arr, list)
    assert any(t["apiId"] == "bitcoin" for t in arr)