# Crypto Ticker

Self-contained networked crypto ticker for the **LilyGO T-Display-S3** (ESP32-S3).

Forked from [osbock/AssetTicker](https://github.com/osbock/AssetTicker).

The device shows live cryptocurrency prices from the [CoinGecko API](https://www.coingecko.com/), can be configured over Wi-Fi from your phone or browser, keeps a historical graph for each coin, and can update its own firmware over the air (OTA) from GitHub Releases.

## Features

### Market data (CoinGecko)

- Live prices for **BTC**, **SOL** and **SUI** out of the box — add/remove/reorder tickers freely (up to 8).
- Brand-colored ticker labels: `$BTC` (yellow), `$SOL` (purple), `$SUI` (cyan).
- Smart price formatting — up to 3 decimal places (e.g. `$67,234`, `$150.45`, `$0.001`).
- **24h percentage change** for every ticker, sourced from CoinGecko market data (`include_24hr_change`). Positive changes are shown green, negative red.

### Network web configuration 

- mDNS: the device advertises itself as **`http://crypto-ticker.local`** on your network.
- A lightweight web page (served from LittleFS) to **view, add, remove, reorder and reset** the configured tickers.
- Clean JSON API (`/api/tickers`) behind the web UI — HTTP handlers only talk to the `ConfigManager`, never to the display directly.
- Configuration persists across reboot in **NVS** (`Preferences`), so nothing is hard-coded into the display logic.
- Each ticker stores its display label, CoinGecko API ID, quote currency and optional brand colour, so a symbol like `BTC` is never assumed to identify an asset by itself.

### Ticker cycling and historical graphs

- The display cycles dynamically from the configuration: **view 0 = the prices table**, views 1..N = one detailed view per configured ticker.
- Navigate with the device buttons (debounced, wraps around in both directions):
  - **GPIO0** = previous view
  - **GPIO14** = next view
- Each ticker detail view shows the current price, the 24h change, and a **7-day historical graph** (144 points, 5-minute CoinGecko `market_chart` data downsampled into fixed-size ring buffers — no heap allocation).
- History is fetched on boot and on config changes; every successful price fetch appends to the buffer, so the graph keeps growing without extra API calls.

### Display power management

- The backlight dims after an idle period and the panel sleeps (then the whole device enters **deep sleep**) to prevent burn-in. Any button press wakes it and forces an immediate price refresh.

## First-Time Setup (WiFi)

1. Power on the device. If no WiFi is configured (or the saved network is unreachable), it starts an open access point named `CryptoTicker-XXXX`.
2. On your phone, join that network — the display shows a **QR code** you can scan to join instantly.
3. Open a browser — the captive portal should redirect you to `http://192.168.4.1`.
4. Pick your network from the list (or type the SSID), enter the password, and tap **Connect**.
5. The device saves the credentials to flash, joins your network, and the ticker starts.

> To reconfigure WiFi later: clear the saved credentials with the serial command `clearwifi` (the device reboots into AP mode), or the AP will re-appear automatically if the saved network becomes unreachable.

## Configuring Tickers

While connected to your network, open **`http://crypto-ticker.local`** (or the device IP) in any desktop or mobile browser.

- See the currently configured tickers.
- Add a ticker: display label, CoinGecko API ID (e.g. `bitcoin`, `solana`, `sui`), quote currency (e.g. `usd`, `usdc`), optional colour.
- Remove, reorder, or reset to the default tickers.
- Changes are saved to NVS and applied to the display immediately.

API endpoints: `GET /` (page), `GET /api/tickers`, `POST /api/tickers` (add), `DELETE /api/tickers?id=N` (remove), `POST /api/tickers/move` (reorder), `POST /api/tickers/reset`.

## OTA Updates and the Release Pipeline

### How the device updates

1. On the first successful Wi-Fi connection it fetches the version manifest:
   `https://github.com/goodalexhunting/esp32s3_crypto_ticker/releases/latest/download/ota_manifest.json`
2. It compares the manifest version against the installed `FW_VERSION` (single authoritative constant in `include/app_config.h`, currently `1.0.0`).
3. If the remote is newer, it downloads `firmware.bin`, verifies the SHA-256 checksum, flashes the inactive OTA slot, and reboots.
4. The check runs **once per boot** and never blocks the ticker — if the update server is unavailable the device just continues normal operation.

### How releases are generated (GitHub Actions)

`.github/workflows/build.yml` uses the three-stage promotion model **`dev` → `staging` → `prod`**:

- Pushes/PRs to `dev` and `staging` are **build-verified only** — no releases are published.
- **Every push/merge into `prod`** builds the firmware **and** the LittleFS filesystem image, then auto-increments the **PATCH** number (e.g. `1.0.0` → `1.0.1`). `MAJOR.MINOR` is user-managed via `FW_VERSION` in `include/app_config.h` and is never auto-incremented.
- The computed version is baked into the firmware at build time, a git tag `vX.Y.Z` is created, and a stable GitHub Release is published.

Production releases carry four assets: `firmware.bin`, `partitions.bin`, `littlefs.bin`, and `ota_manifest.json` (version + firmware URL + SHA-256).

### Rolling out a firmware update

- **Patch release** — just merge into `prod`; CI auto-increments the patch (e.g. `1.0.0` → `1.0.1`) and publishes.
- **Major/minor release** — bump `MAJOR.MINOR` in `include/app_config.h` on `dev` (e.g. `1.0.1` → `1.1.0`), then promote `dev → staging → prod`. The next push to `prod` publishes `1.1.0`.

## Building

Requires [PlatformIO](https://platformio.org/). Environment: `lilygo-t-display-s3` (espressif32, Arduino framework).

```sh
# Build firmware
pio run -e lilygo-t-display-s3

# Upload firmware to the device
pio run -e lilygo-t-display-s3 -t upload

# Build and upload the LittleFS filesystem image (web pages in data/)
pio run -e lilygo-t-display-s3 -t buildfs
pio run -e lilygo-t-display-s3 -t uploadfs
```

> The web configuration pages (`data/wifi_config.html`, `data/ticker_config.html`) live in the LittleFS partition. Upload the filesystem image (`uploadfs`) if the pages are missing or outdated.

## Project Layout

- `src/main.cpp` — boot flow, WiFi/config wiring, main loop, OTA check trigger.
- `src/wifi_mgr.cpp` — WiFi manager: NVS credentials, AP mode + captive portal + QR, mDNS, reconnection, LittleFS mount.
- `src/config_mgr.cpp` — ticker configuration list, NVS persistence (`ticker_cfg`), revision tracking.
- `src/config_server.cpp` — HTTP configuration page + JSON API (`ConfigServer`).
- `src/crypto.cpp` — CoinGecko fetches: current prices, 24h changes, historical `market_chart`.
- `src/history.cpp` — fixed-size price ring buffers (144 points/ticker).
- `src/display.cpp` — rendering: prices table, ticker detail view with graph, API status indicator.
- `src/display_cycle.cpp` — display-cycle navigation state (single source of truth).
- `src/display_power.cpp` — backlight/panel power state machine, button debouncing, deep-sleep hook.
- `src/api_health.cpp` — rolling API health history (green/yellow/red).
- `src/ota_mgr.cpp` — OTA manifest check, HTTPS download, SHA-256 verification, flashing.
- `src/layout_mgr.cpp` — grid-based layout helper.
- `src/qr_display.cpp` — QR rendering for the AP-mode setup screen.
- `include/app_config.h` — central configuration (screen size, `FW_VERSION`, mDNS hostname, OTA manifest URL, default tickers, timing).
- `data/` — LittleFS web pages (`wifi_config.html`, `ticker_config.html`).
- `.github/workflows/build.yml` — CI build (`dev`/`staging`) + auto-published production releases on `prod`.

## Known Limitations

- The configuration web server only runs in station (connected) mode, not in AP mode.
- CoinGecko allows a single `vs_currencies` per request, so all tickers in one fetch share a quote currency (per-ticker quotes work per ticker detail/history fetch).
- Historical data is not persisted across reboot (it is refetched on boot).