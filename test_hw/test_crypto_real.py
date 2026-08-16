# Copyright (c) 2026. See parent project license.

"""Hardware-in-the-loop smoke test against the real CoinGecko API.

Prereq: the device must be connected to a Wi-Fi network and reachable at
--device. The firmware's /config page is a good reachability check first.

This test does NOT exercise the device's HTTP client directly (that path is
internal). Instead it verifies the same public CoinGecko endpoints the
firmware uses return the data the firmware depends on, so a live device
running normally is confirmed to have sane data to render.

Run:
    pytest -s test_hw/test_crypto_real.py --device 192.168.1.50
"""

import requests


def _device_ok(request):
    """Quick reachability check: the config server must respond."""
    r = requests.get(f"http://{request.config.getoption('--device')}/config", timeout=5)
    assert r.status_code == 200


def pytest_addoption(parser):
    parser.addoption("--device", required=True, help="IP address of the configured device")


def test_device_reachable(request):
    _device_ok(request)


def test_coingecko_simple_price_has_btc():
    """The simple/price endpoint used by fetch_prices must return BTC>0."""
    r = requests.get(
        "https://api.coingecko.com/api/v3/simple/price",
        params={"ids": "bitcoin", "vs_currencies": "usd", "include_24hr_change": "true"},
        timeout=15,
    )
    assert r.status_code == 200
    data = r.json()
    assert "bitcoin" in data
    price = data["bitcoin"]["usd"]
    assert price > 0
    # The 24h change field must be present (fetch_prices reads it).
    assert "usd_24h_change" in data["bitcoin"]


def test_coingecko_market_chart_7d():
    """The market_chart endpoint used by fetch_history must return >=1 point."""
    r = requests.get(
        "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart",
        params={"vs_currency": "usd", "days": "7"},
        timeout=30,
    )
    assert r.status_code == 200
    prices = r.json().get("prices", [])
    assert len(prices) > 0
    # Each point is [timestamp_ms, price].
    assert len(prices[0]) == 2