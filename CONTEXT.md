# ESP32-S3 Crypto Ticker

Context for the crypto-ticker firmware: a self-contained device that tracks configured cryptocurrencies, displays their prices with 24h change and 7-day graphs, is configured over Wi-Fi, and updates itself over the air.

## Language

### Tickers & prices

**Ticker**:
A cryptocurrency the device tracks and displays. Each ticker binds a display label to a CoinGecko API id, a quote currency, and a brand colour; the label never implies which asset it is.
_Avoid_: coin, token, asset, symbol

**Quote currency**:
The currency a ticker's prices are expressed in, e.g. `usd`. A batch price fetch shares one quote currency across all tickers.
_Avoid_: vs_currency, fiat

**24h change**:
A ticker's percentage price change over the last 24 hours, shown beside its current price.

**Price history**:
A ticker's rolling window of recent price samples, drawn as its 7-day graph. Rebuilt from CoinGecko on every boot — never persisted across reboots.
_Avoid_: chart data, series

**Backfill**:
Rebuilding a ticker's price history from CoinGecko's historical market data rather than from live samples.

### Device & display

**Display cycle**:
The ordered set of views shown on the screen: the table view first, then one view per configured ticker. Navigation wraps in both directions.
_Avoid_: screen, page, slide

**Display power state**:
One of `ON`, `DIMMED`, `OFF` — the brightness steps the display falls through as idle time passes. Reaching `OFF` puts the device into deep sleep.
_Avoid_: standby

**Deep sleep**:
The device state entered when the display turns `OFF`: Wi-Fi and the main loop power down until a button press reboots the device.

**Buttons**:
The two hardware buttons (GPIO0, GPIO14) used to navigate the display cycle and to wake the device from deep sleep.

### Configuration & Wi-Fi

**Config UI**:
The web page served by the device at `/config`, where tickers and settings are edited.
_Avoid_: settings page, web config

**Landing page**:
The GitHub Pages site the device's mDNS address redirects to; it links back into the device's own Config UI.

**Config revision**:
A counter that increases whenever the configured ticker list changes; watching it is how the running device detects edits made from the Config UI.

**mDNS hostname**:
The name the device advertises on the local network — `crypto-ticker.local` — the entry point to its configuration flow.

**AP mode**:
The Wi-Fi state in which the device hosts its own network with a captive portal for Wi-Fi setup. The QR setup screen stays lit and the Config UI is not served.
_Avoid_: setup mode

**Station mode**:
The Wi-Fi state in which the device joins the user's network with stored credentials. Serves the Config UI and runs price and OTA updates.

### Updates

**OTA slot**:
One of the two interchangeable flash slots holding a runnable firmware image. An update is written to the inactive slot and activated by reboot.
_Avoid_: partition (when meaning a single slot)

**Factory slot**:
The first flash slot, holding the firmware as flashed over USB. Never updated over the air; the bootloader's last resort when no OTA slot boots.

**Rollback**:
The bootloader's automatic switch back to the previously working OTA slot after a fresh update fails its boot self-test.

**Self-test verification**:
The running firmware's confirmation that it booted successfully, made within the boot window to cancel a pending rollback.

**Boot watchdog**:
The short window after boot in which self-test verification must happen; if it lapses, the device reboots into the previous OTA slot.

**OTA manifest**:
The JSON file — firmware version, URL, and SHA-256 — the device polls to decide whether a newer firmware exists.

**Firmware version**:
The `MAJOR.MINOR.PATCH` string identifying a build, compared against the OTA manifest to detect updates.

**Release branch**:
One of the three promotion branches — `dev`, `staging`, `prod`. Only `prod` pushes publish releases.
_Avoid_: channel, environment
