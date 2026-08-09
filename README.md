# Crypto Ticker

Crypto Ticker on Lilygo T-Display-S3.

Forked from https://github.com/osbock/AssetTicker

## Features

- Displays live USD prices for **BTC**, **SOL** and **SUI** (CoinGecko API).
- Brand-colored ticker labels: `$BTC` (yellow), `$SOL` (purple), `$SUI` (cyan).
- Smart price formatting — up to 3 decimal places (e.g. `$67,234`, `$150.45`, `$0.001`).
- **No hardcoded WiFi credentials.** On first boot the device starts an access point with a captive portal so you can configure WiFi from your phone.

## First-Time Setup (WiFi)

1. Power on the device. If no WiFi is configured, it starts an access point named `CryptoTicker-XXXX`.
2. On your phone, join that network.
3. Open a browser — the captive portal should redirect you to `http://192.168.4.1`.
4. Pick your network from the list (or type the SSID), enter the password, and tap **Connect**.
5. The device saves the credentials to flash and joins your network. The ticker starts automatically.

> To reconfigure WiFi later, hold the device's button on boot to force AP mode (if wired up), or the AP will also re-appear automatically if the saved network is unreachable.

## Building

Requires [PlatformIO](https://platformio.org/).

```sh
pio run -e lilygo-t-display-s3
```

## Project Layout

- `src/main.cpp` — boot flow, WiFi handling, main loop.
- `src/wifi_mgr.cpp` — WiFi manager: NVS credentials, AP mode + captive portal, reconnection.
- `src/crypto.cpp` — CoinGecko fetch, JSON parsing, display rendering.
- `src/layout_mgr.cpp` — grid-based layout helper.
- `include/` — headers (display config, layout, crypto, wifi).