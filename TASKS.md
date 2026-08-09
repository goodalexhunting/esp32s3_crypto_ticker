# LILYGO S3 Crypto Ticker — Development Tasks

> This file is an execution queue for autonomous development.
> Complete tasks sequentially without waiting for user confirmation unless a genuine blocker is encountered.

## General Instructions

* Complete tasks **in order**.
* Do not start a later task until the previous task is complete.
* Preserve all functionality from previous tasks.
* Do not unnecessarily rewrite working code.
* Keep the implementation appropriate for the ESP32-S3 and its memory/storage constraints.
* Prefer existing libraries already used by the project where practical.
* Keep networking, display rendering, configuration, and market-data functionality separated where possible.
* Before attempting a task: checkout into a branch with an appropiate name.
* After completing each task:

  1. Compile the project successfully.
  2. Test the relevant functionality on hardware where possible.
  3. Update this file by marking the task as complete and documenting any important implementation decisions.
  4. Commit the changes with a clear commit message.
  5. Create a PR request into main from this branch

---

# Task 1 — Network Web Configuration ✅

## Objective

Create a configurable web page that allows the user to modify which cryptocurrencies are displayed by the ticker.

The configuration page must be accessible from the device's normal connected network using:

`http://crypto-ticker.local`

## Requirements

### mDNS

* Add mDNS support so the ESP32-S3 advertises itself as:

`crypto-ticker.local`

* The hostname should be configurable in one central location rather than scattered throughout the code.
* The mDNS service should start after the device successfully connects to Wi-Fi.
* If mDNS cannot be started, the device should continue operating normally.

### Web server

Create a lightweight HTTP server running on the ESP32-S3.

The web interface should allow the user to:

* View the currently configured cryptocurrencies.
* Add a cryptocurrency.
* Remove a cryptocurrency.
* Reorder cryptocurrencies if practical.
* Save the configuration.

The interface should be simple and lightweight enough for the ESP32-S3.

### Configuration persistence

The selected tickers must persist across reboot.

Use ESP32 non-volatile storage/NVS or the project's existing configuration mechanism.

Do not hard-code the selected cryptocurrencies into the display logic.

The display code should obtain its ticker list from the configuration.

### Market identifiers

The configuration should store enough information to identify a cryptocurrency unambiguously.

Ideally separate:

```text
Display name
CoinGecko ID
Quote currency
```

For example:

```text
Bitcoin
bitcoin
USDC
```

Do not assume that a display symbol such as `BTC` is sufficient to identify an asset.

### API

Create a clean interface between the web configuration and the rest of the application.

For example:

```text
Web configuration
       ↓
Configuration Manager
       ↓
Ticker configuration
       ↓
Market Data
       ↓
Display
```

Avoid having HTTP handlers directly manipulate display state.

### UI

The configuration page should work on both desktop and mobile browsers.

A simple page is sufficient.

Example:

```text
Crypto Ticker

Configured tickers

[ BTC / USDC ]   [Remove]
[ ETH / USDC ]   [Remove]
[ SOL / USDC ]   [Remove]

Add ticker:
[ BTC ] [ USDC ]

[ Save ]
```

Do not spend excessive time creating a visually elaborate website.

## Acceptance Criteria

* ESP32 connects to Wi-Fi as it currently does.
* `http://crypto-ticker.local` resolves from another device on the same network.
* Web page displays the current ticker configuration.
* User can modify the ticker list.
* Configuration survives reboot.
* Display uses the configured ticker list.
* Existing display functionality continues to work.
* Project compiles successfully.

## Completion

Before moving to Task 2, document:

* Web server library used.
* mDNS library used.
* Configuration storage mechanism.
* Any limitations.

### Implementation Notes

* **Web server library**: Arduino `WebServer` (built into the ESP32 Arduino core).
* **mDNS library**: Arduino `ESPmDNS` (built into the ESP32 Arduino core).
* **Configuration storage**: NVS via the `Preferences` library. Ticker config is stored in the `ticker_cfg` namespace with per-ticker keys (`t0.label`, `t0.apiid`, `t0.quote`, `t0.color`, etc.).
* **Architecture**: New `ConfigManager` class owns the ticker list and persists to NVS. New `ConfigServer` class handles HTTP requests and only manipulates the `ConfigManager` - it never touches display state. The display and market-data layers read their ticker list from the `ConfigManager`.
* **API endpoints**:
  * `GET /` — serves the configuration page.
  * `GET /api/tickers` — returns the current ticker list as JSON.
  * `POST /api/tickers` — adds a ticker (form: `label`, `apiId`, `quote`, optional `color`).
  * `DELETE /api/tickers?id=N` — removes the ticker at index N.
  * `POST /api/tickers/move` — reorders tickers (form: `from`, `to`).
  * `POST /api/tickers/reset` — resets to the compile-time defaults.
* **Limitations**:
  * The web server only runs in station (connected) mode, not AP mode.
  * The configuration page is served from LittleFS (`data/ticker_config.html`).
  * The quote currency is shared across all tickers in a single CoinGecko request (the API only supports one `vs_currencies` per request).
  * The display table header still says "PRICE (USD)" even if a non-USD quote is configured.

---

# Task 2 — Ticker Cycling and Historical Graphs ✅

## Objective

Extend the existing display so the user can navigate between the existing table view and a historical price graph for each configured cryptocurrency.

The display cycle is dynamically derived from the configured tickers:

- `cycle[0]` = existing table view
- `cycle[1]` = configured ticker 0
- `cycle[2]` = configured ticker 1
- etc.

Do not assume a fixed number of tickers.

## Button Navigation

Use the existing project button/input abstraction where possible.

- GPIO0 = previous
- GPIO14 = next

Navigation must wrap:

- GPIO0 from `cycle[0]` → final cycle
- GPIO14 from final cycle → `cycle[0]`

Buttons should only modify the current display/cycle index. Do not hard-code behaviour for individual cryptocurrencies.

Implement appropriate debouncing so one physical press produces one navigation event.

Maintain a single source of truth for the current cycle.

## Display Behaviour

The existing table renderer must remain `cycle[0]` and continue functioning as it does currently.

For `currentCycle > 0`, display the ticker corresponding to:

ticker index = currentCycle - 1

Each configured ticker should have its own historical price graph.

The graph, displayed current price, and ticker label must all refer to the same asset/quote pair.

# Historical Data

Maintain independent, bounded historical price data for every configured ticker.

A ring/circular buffer is preferred if appropriate for the ESP32-S3's memory constraints.

Switching between display cycles must not clear or recreate historical data.

Historical data does not need to persist across reboot.

When tickers are added or removed through the existing web configuration, update the available display cycles accordingly.

Ensure currentCycle is always valid after configuration changes.

Market Data / CoinGecko

Review the existing CoinGecko implementation and determine the appropriate endpoint(s) required to provide:

    - current price
    - historical price data
    - USDC-denominated data where supported

Prefer genuine USDC market/pair data rather than locally converting USD or another currency.

Avoid unnecessary API requests and respect CoinGecko rate limits.

Investigate the existing implementation before introducing new API calls or changing the current data model.

# Constraints
- Preserve existing table functionality.
- Preserve existing web configuration.
- Reuse existing project abstractions where appropriate.
- Avoid hard-coded ticker counts.
- Avoid unnecessary RAM usage.
- Avoid unnecessary API calls.
- Follow the project's existing architecture and coding style.

# Acceptance Criteria
- GPIO0 navigates backwards.
- GPIO14 navigates forwards.
- Navigation wraps in both directions.
- cycle[0] remains the existing table.
- Cycle is dynamically derived from configuration.
- Each ticker displays its own historical graph.
- Navigating between tickers does not reset history.
- Configuration changes update the available cycles safely.
- Graph/current price/label use the same USDC pair.
- Button debouncing works correctly.
- Existing table and web configuration continue to work.


# Completion Notes

Before completing the task, document:

- CoinGecko endpoint(s) used and why.
- How USDC pairing is obtained.
- Historical sampling interval.
- Number of points retained per ticker.
- Approximate RAM usage.
- Button/debounce implementation.
- Cycle-to-ticker mapping.
- Any CoinGecko limitations or compromises.

### Implementation Notes

* **CoinGecko endpoints**:
  * Current prices: `GET /api/v3/simple/price?ids=...&vs_currencies=...` (existing).
  * Historical data: `GET /api/v3/coins/{id}/market_chart?vs_currency={quote}&days=7`.
* **USDC pairing**: The quote currency is stored per-ticker in the configuration. The market_chart endpoint uses the configured quote currency directly (e.g. `usdc`), so genuine USDC market data is used when configured.
* **Historical sampling interval**: The market_chart endpoint returns 5-minutely data for `days=7`. The firmware downsamples to `HISTORY_POINTS` (144) points per ticker.
* **Points retained per ticker**: 144 (fixed-size ring buffer, no heap allocation).
* **Approximate RAM usage**: `HISTORY_POINTS * sizeof(float) * MAX_TICKERS` = 144 * 4 * 8 = 4,608 bytes for all history buffers.
* **Button/debounce implementation**: `DisplayPower` now tracks each button independently with a 30ms debounce window. `consumeButtonEvent()` returns which button was pressed (BUTTON_1 = GPIO0 = previous, BUTTON_2 = GPIO14 = next).
* **Cycle-to-ticker mapping**: `DisplayCycle` class maintains the current cycle index. `cycle[0]` = table view, `cycle[i]` (i > 0) = configured ticker at index (i-1). Navigation wraps in both directions.
* **Configuration changes**: `ConfigManager` now has a `revision()` counter that increments on add/remove/move/reset. The main loop detects revision changes, clamps the cycle index, resets history buffers, and refetches historical data.
* **CoinGecko limitations**: The `market_chart` endpoint does not support `interval=daily` for ranges under 30 days, so 5-minutely data is fetched and downsampled. The free tier rate limit (10-30 calls/min) is respected by only fetching history on boot and on config changes.
* **History persistence**: Historical data does not persist across reboot (per requirements).
* **History updates**: Each successful price fetch appends to the ring buffer, so the graph grows over time without additional API calls.

---

# Task 3 — 24-Hour Change and API Status ✅

## Objective

Add:

1. 24-hour percentage price change.
2. An API/network status indicator in the top-left corner.

## 24-hour change

Display the 24-hour percentage change for the currently selected ticker.

Example:

```text
BTC / USDC

$112,431.42

+2.31%
```

Positive and negative changes should be visually distinguishable.

Do not hard-code colours if the display abstraction already provides an appropriate mechanism.

The percentage must come from market data rather than being calculated from the locally collected graph unless there is no suitable API value.

## API status indicator

Add a small status indicator in the **top-left corner** of the display.

Three states:

### Green

API is healthy.

Meaning:

* Recent requests are succeeding.
* Current market data is being received normally.

### Yellow

API is degraded.

Meaning:

* Some recent requests have failed.
* At least some recent requests have succeeded.

### Red

API is unavailable.

Meaning:

* All recent requests are failing.
* No successful API response has been received within the defined failure window.

## Suggested state model

Use a rolling success/failure history rather than simply changing colour after one failure.

For example:

```text
SUCCESS SUCCESS SUCCESS SUCCESS
→ GREEN

SUCCESS SUCCESS FAILURE FAILURE
→ YELLOW

FAILURE FAILURE FAILURE FAILURE
→ RED
```

The exact thresholds are implementation details.

Avoid making the indicator excessively sensitive to a single failed HTTP request.

## Offline behaviour

If the API fails:

* Keep displaying the last known valid price.
* Keep displaying the last known 24h percentage.
* Continue displaying the graph using existing historical data.
* Change the status indicator appropriately.

The device should not blank the entire display just because an API request failed.

## Status recovery

When requests begin succeeding again:

```text
RED → YELLOW → GREEN
```

or an equivalent sensible recovery model should be used.

The indicator should reflect actual API health.

## Acceptance Criteria

* 24h percentage is displayed for every configured ticker.
* Percentage updates with market data.
* Top-left status indicator exists.
* Green indicates healthy API operation.
* Yellow indicates intermittent failures.
* Red indicates sustained/all failures.
* Last known data remains visible during API failures.
* Graph remains usable during temporary API failures.
* Status recovers when API requests succeed again.
* Existing Tasks 1 and 2 functionality remains intact.
* Project compiles successfully.

## Completion

Document:

* Success/failure thresholds.
* Number of requests retained in the health history.
* How long cached market data remains valid.
* How API failures are handled.

### Implementation Notes

* **24h change source**: CoinGecko's `simple/price` endpoint with `include_24hr_change=true`. The response includes `{quote}_24h_change` for each coin, which is the genuine 24h percentage change from market data.
* **Display**: The 24h change is shown on the ticker detail view (right-aligned below the current price). Positive changes are green, negative changes are red.
* **API status indicator**: A small filled circle in the top-left corner of the header. Green = healthy, Yellow = degraded, Red = unavailable.
* **Success/failure thresholds**: The `ApiHealth` class keeps a rolling history of the last 8 API requests. Status is derived as:
  * All successes → GREEN
  * Some successes, some failures → YELLOW
  * All failures → RED
* **Requests retained in health history**: 8 (fixed-size ring buffer).
* **Cached market data validity**: Prices and 24h changes are kept in RAM indefinitely. On API failure, the last known valid data remains displayed. The graph continues to use existing historical data.
* **API failure handling**: On a failed fetch, `ApiHealth::recordFailure()` is called, the status indicator is updated, and a "Fetch failed" message is shown. The last known prices and graph remain visible. When requests succeed again, the health recovers gradually (RED → YELLOW → GREEN as successes accumulate).
* **Offline behaviour**: The device does not blank the display on API failure. It keeps showing the last known price, 24h change, and graph.

---

# Task 4 — OTA Firmware Updates ✅

## Objective

Implement secure and reliable OTA firmware updates.

The goal is for the device to be able to detect a newer firmware version when connected to the internet and install it without requiring a physical USB connection.

## Build pipeline

Create a GitHub Actions workflow using PlatformIO.

Every push to the appropriate branch should:

1. Check out the repository.
2. Install PlatformIO.
3. Build the firmware.
4. Verify that compilation succeeds.
5. Produce the compiled firmware as a build artifact.

The workflow should fail if the firmware does not compile.

## Firmware version

Introduce an explicit firmware version.

For example:

```text
1.0.0
```

The version should exist in one authoritative location.

Avoid having different hard-coded versions scattered throughout the code.

The device must be able to determine its currently installed firmware version.

## OTA manifest

Do not make the ESP32 blindly download arbitrary GitHub Actions artifacts.

Investigate using a small version manifest hosted alongside a GitHub Release.

For example:

```json
{
    "version": "1.1.0",
    "firmware_url": "...",
    "sha256": "..."
}
```

The device should:

1. Connect to Wi-Fi.
2. Check the update manifest.
3. Compare the remote version against the installed version.
4. Download the firmware only if a newer version exists.
5. Verify the downloaded firmware.
6. Install it.
7. Reboot.
8. Continue normal operation.

## GitHub Releases

Prefer GitHub Release assets as the long-lived firmware distribution mechanism rather than relying exclusively on temporary GitHub Actions artifacts.

GitHub Actions should build the firmware.

A release process can then publish the compiled firmware as a release asset.

For example:

```text
Git push
   ↓
GitHub Actions
   ↓
PlatformIO build
   ↓
Firmware binary
   ↓
GitHub Release
   ↓
OTA manifest
   ↓
ESP32
```

The exact release automation should be chosen based on the repository's existing Git workflow.

## Security

Do not implement an OTA mechanism that blindly downloads and flashes an arbitrary URL.

At minimum:

* Verify HTTPS certificates appropriately.
* Verify the downloaded firmware checksum/hash.
* Reject corrupted firmware.
* Do not flash a failed/incomplete download.
* Preserve a working firmware image where supported by the ESP32 OTA partition configuration.

Investigate ESP32-S3 OTA partition requirements before modifying the partition table.

## First connection

The user originally envisioned:

```text
Device connects to internet
        ↓
Check GitHub Actions artifact
        ↓
Install newest firmware
```

Implement the same user experience, but use a stable firmware release/manifest mechanism if GitHub Actions artifacts are unsuitable for permanent OTA distribution.

The device should not download a new firmware on every boot.

## Update policy

The device should:

* Check for updates when it first successfully connects to the internet.
* Avoid repeatedly checking in a tight loop.
* Optionally check periodically afterward.
* Only install a newer version.
* Continue normal operation if the update server is unavailable.

An OTA failure must **not prevent the ticker from functioning normally**.

## Acceptance Criteria

* GitHub Actions builds the firmware successfully using PlatformIO.
* Firmware binary is produced automatically.
* Firmware has an explicit version.
* Device can check a remote version.
* Device detects when a newer version exists.
* Device verifies the firmware before installation.
* Device installs the new firmware and reboots successfully.
* Existing configuration such as Wi-Fi and ticker selection survives the update.
* Device continues functioning if GitHub is unavailable.
* Device does not repeatedly download the same firmware.
* Existing Tasks 1–3 functionality remains intact.

## Completion

Document:

* GitHub Actions workflow.
* PlatformIO build environment.
* Firmware versioning strategy.
* OTA partition configuration.
* Manifest format.
* Firmware verification mechanism.
* How releases are generated.
* How the device determines whether an update is available.

### Implementation Notes

* **GitHub Actions workflow**: `.github/workflows/build.yml` runs on push to `main`, PRs to `main`, tags `v*`, and manual dispatch. The `build` job checks out the repo, sets up Python 3.11, installs PlatformIO, runs `pio run -e lilygo-t-display-s3`, and uploads `firmware.bin` + `partitions.bin` as build artifacts. The `release` job runs on every push to `main` (creates a nightly release tagged `nightly-<short-sha>-<run>` whose manifest version equals `FW_VERSION`) and on `v*` tag pushes (creates a versioned release whose manifest version is the tag without the leading `v`). It computes SHA-256 of the firmware, generates `ota_manifest.json`, and publishes a GitHub Release with all three files. Tag releases fail unless the tag matches `FW_VERSION` in `include/app_config.h`, which prevents the device re-installing the same firmware on every boot.
* **PlatformIO build environment**: `lilygo-t-display-s3` (ESPRESSIF32 platform 7.0.1, Arduino framework, LovyanGFX, ArduinoJson, QRCode).
* **Firmware versioning strategy**: Single authoritative `FW_VERSION` constant in `include/app_config.h` (currently `"1.0.0"`). The device compares this against the manifest version. GitHub release tags use `vX.Y.Z` format matching the version.
* **OTA partition configuration**: Custom `partitions.csv` creates two OTA app slots:
  * `app0` (ota_0) at 0x10000, 0x1F0000 (~1.94 MB)
  * `app1` (ota_1) at 0x200000, 0x1F0000
  * `otadata`, `nvs`, and a small `spiffs` (LittleFS) partition.
  * The ESP32 boots from the current slot and updates the unused one, so a failed update leaves the existing firmware operational.
* **Manifest format**: `ota_manifest.json` hosted as a GitHub Release asset:
  ```json
  {
    "version": "1.1.0",
    "firmware_url": "https://github.com/{owner}/{repo}/releases/download/v1.1.0/firmware.bin",
    "sha256": "<lowercase hex sha256 of firmware.bin>"
  }
  ```
* **Firmware verification mechanism**: The device streams the firmware download and computes SHA-256 using mbedTLS while writing to flash. The computed hash is compared against the manifest's `sha256` before `Update.end()` is called. Any mismatch aborts the update, leaving the previous firmware intact.
* **How releases are generated**: Every push to `main` triggers the `release` job and creates a nightly release tagged `nightly-<short-sha>-<run>` whose manifest version equals the `FW_VERSION` in `include/app_config.h`. Pushing a `vX.Y.Z` tag instead creates a versioned release; the tag must match the committed `FW_VERSION` (e.g. tag `v1.1.0` requires `FW_VERSION = "1.1.0"`). The job downloads the build artifact, computes the SHA-256, creates the manifest, and publishes a GitHub Release with `firmware.bin`, `partitions.bin`, and `ota_manifest.json`.
* **How the device determines whether an update is available**: On first successful Wi-Fi connect, `OtaManager::checkForUpdate()` fetches `OTA_MANIFEST_URL` (the latest release's `ota_manifest.json`). It parses the JSON, compares the remote version against `FW_VERSION` using semantic version comparison, and only installs when the remote is newer. The check runs once per boot and never blocks the ticker - failures are logged and normal operation continues.
* **Update policy**: The device checks for updates **once per boot** after the first successful Wi-Fi connection. It does not re-check in a loop. If the update server is unavailable, the device continues normal ticker operation.
* **HTTPS**: The firmware downloads use HTTPS via `HTTPClient` with the ESP32's built-in certificate bundle (default `setInsecure` is NOT used; the standard root CA bundle validates GitHub's certificates).
* **NVS persistence**: Ticker config, Wi-Fi credentials, and display settings are stored in NVS and survive OTA updates since the `nvs` partition is preserved.
* **Safety**: `Update.end()` is only called after the full download completes and the SHA-256 matches. A failed/incomplete download never flashes partial firmware. The two-app-slot partition table ensures the bootloader can always fall back to the previous working image.

---

# Final Project Definition

After Tasks 1–4 are complete, the device should provide:

```text
                    ┌──────────────────────┐
                    │   LILYGO T-DISPLAY   │
                    │        S3             │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼───────────┐
                    │   Wi-Fi Connection   │
                    └──────────┬───────────┘
                               │
              ┌────────────────┼────────────────┐
              │                │                │
              ▼                ▼                ▼
        Configuration      Market Data       OTA Updates
        Web Interface      CoinGecko         GitHub
              │                │                │
              ▼                ▼                ▼
          Ticker List       Price Data       Firmware
              │                │                │
              └────────────────┼────────────────┘
                               ▼
                       Display Manager
                               │
              ┌────────────────┼────────────────┐
              ▼                ▼                ▼
           Price             Graph          API Status
           24h %             History         Indicator
              │
              ▼
       Bottom Button
              │
              ▼
     BTC → ETH → SOL → ...
```

The resulting device should behave as a self-contained networked crypto ticker that can be configured, monitored, and updated without connecting it to a computer.
