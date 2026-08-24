# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Self-contained crypto ticker for the **LilyGO T-Display-S3** (ESP32-S3), built with PlatformIO (Arduino framework). Fork of osbock/AssetTicker. Fetches CoinGecko prices (default BTC/SOL/SUI), shows 24h change + 7-day per-ticker graphs, is configurable over Wi-Fi (`http://crypto-ticker.local` → GitHub Pages landing page → device-served `/config` UI), and self-updates OTA from GitHub Releases.

Branch model: **`dev` → `staging` → `prod`**. Only pushes to `prod` publish releases (see CI/CD below).

## Commands

### Firmware

```sh
pio run -e lilygo-t-display-s3            # build
pio run -e lilygo-t-display-s3 -t upload  # build + flash
pio run -e lilygo-t-display-s3 -t monitor # serial monitor (115200)
```

The web UI is embedded as PROGMEM strings in `include/embedded_ui.h` — there is **no LittleFS filesystem image**; a single `upload` flashes everything. The custom partition table (`partitions.csv` — factory slot + A/B OTA slots) is required for OTA and is wired via `board_build.partitions`.

### Host tests (no board, run in CI)

```sh
pio test -e native                  # full unit + integration suite
pio test -e native -f test_ota_utils   # single suite (name = directory under test/)
```

`[env:native]` compiles each `test/test_<name>/` as a standalone Unity binary against `test/mocks/` (shadows `<Arduino.h>`, `<WiFi.h>`, `<WebServer.h>`, `<DNSServer.h>`, `<Preferences.h>`, `<HTTPClient.h>`).

### Hardware-in-the-loop tests (manual, real device)

See `test_hw/README.md`. Require pytest + a flashed device on the network; **not** run in CI.

```sh
pytest -s test_hw/test_config_api.py --device 192.168.1.50
pytest -s test_hw/test_ota_e2e/test_ota.py --device 192.168.1.50 --ota-url https://<ip>:8443
```

OTA HIL tests need the device firmware built with `-DOTA_MANIFEST_URL=https://<local-server-ip>:8443/ota_manifest.json` plus the bundled fixture server (`python test_hw/test_ota_e2e/ota_server.py`).

### Formatting

No lint tooling; `clang-format` config in `.clang-format` (Google base, 4-space indent, no tabs, 100-col limit, aligned consecutive declarations/assignments). Match it when editing C++.

## Architecture

### Firmware layout

Each module is `src/<name>.cpp` with its header in `include/<name>.h` (namespace `cryptoapp`). `src/main.cpp` declares all modules as global singletons and owns the boot sequence and the cooperative `loop()`:

- **Boot (`setup()`)**: arm 30s self-test watchdog → `config.begin()` (NVS) → `wifi.begin()` (station or AP mode) → display power → mDNS + `configServer.begin()` → initial price fetch with retries + history fetch → `otaManager.selfTestVerification()` (cancels OTA rollback, disarms watchdog). If init hangs, the watchdog reboots and the bootloader rolls back to the previous A/B partition.
- **Loop**: serial commands (`clearwifi` clears credentials and reboots into AP mode), `wifi.handle()`, display power state machine (ON → DIMMED → OFF → deep sleep; wake buttons GPIO0/GPIO14), button-driven display cycling, config server, 60s price refresh, and the hourly OTA check (first check ~30s after boot).

### Concurrency model

The web server is async (`ESPAsyncWebServer`), but **all shared state is single-threaded**: async handlers never touch `ConfigManager` directly — they enqueue operations that the main loop drains. The loop detects config changes via `config.revision()` and resets history/display state. Follow this pattern for any new async entry point.

### Configuration

`include/app_config.h` is the single source of truth for all tunables: screen size, `FW_VERSION_STR`, mDNS hostname, OTA manifest URL, CoinGecko URL, default tickers, history sizing, power/timeout constants, pins, feature flags. `include/embedded_ui.h` holds the config + WiFi-setup web pages as PROGMEM strings (regenerate by editing there; no build step).

Runtime config persists in NVS via `Preferences` (ticker list in `ticker_cfg`, WiFi credentials separately). Each ticker stores label, CoinGecko API id, quote currency, and optional RGB565 brand color — never infer an asset from its symbol.

### OTA / release pipeline

- `partitions.csv` defines a factory slot + A/B OTA slots; OTA downloads `firmware.bin` to the inactive OTA slot, verifies SHA-256, reboots; the 30s boot watchdog + `selfTestVerification()` give automatic rollback to the other OTA slot on failed boot. The factory slot is never OTA'd (USB reflash only) and is the bootloader's last resort when no OTA slot boots.
- Device polls `https://github.com/goodalexhunting/esp32s3_crypto_ticker/releases/latest/download/ota_manifest.json` (version + firmware URL + SHA-256) hourly.
- CI (`.github/workflows/build.yml`):
  - `test` job (`pio test -e native`) gates PRs to `staging` and pushes to `staging`/`prod`; a failed run blocks the build and therefore the release.
  - `build` job compiles firmware for all push/PR events.
  - `release` + `pages` jobs run **only on `prod` pushes**: PATCH auto-increments from the highest existing `v<MAJOR.MINOR>.x` tag (MAJOR.MINOR read from `FW_VERSION_STR` in `app_config.h`, never auto-incremented), version baked in via `-DFW_VERSION_STR`, then tag + GitHub Release (firmware.bin, partitions.bin, ota_manifest.json) + GitHub Pages deploy of `site/`.
  - Prod releases delete all non-`vX.Y.Z` releases so `/releases/latest` always points at a stable build for OTA.
- **Rolling out**: patch release = merge to `prod`. Minor/major = bump `FW_VERSION_STR` in `include/app_config.h` on `dev`, then promote `dev → staging → prod`.

### Test architecture (important pattern)

Pure logic is deliberately extracted into `*_utils.cpp` modules (`crypto_utils`, `ota_utils`) so host tests can exercise it without hardware. Each test `test/test_<name>/test_<name>.cpp` is a standalone binary that **`#include`s the source file under test directly** (e.g. `#include "../../src/ota_utils.cpp"`) — `build_src_filter = -<*>` excludes `src/` from normal compilation. To add a host test for a module: create the `test/test_<name>/` directory, include the `.cpp` plus the mocks you need, and `pio test -e native -f test_<name>`.

Note the firmware-only test seams: `OTA_MANIFEST_URL` is an `#ifndef` macro in `app_config.h` so a build flag can redirect OTA checks to a local fixture server, and `OTA_CA_CERT_PEM` in `include/ota_certs.h` can be overridden so the device trusts the fixture server's self-signed TLS cert (see `test_hw/README.md`).

## Working Rules (from `.clinerules/`)

- **Autonomy**: complete tasks without stopping for permission or minor ambiguity; make reasonable engineering decisions. Ask the user only for: directly contradictory requirements, materially different outcomes of a destructive migration, missing credentials/secrets, undeterminable hardware capability, a fundamentally different design choice, or risk of destroying data/history.
- **Architecture**: don't introduce new architectural patterns without a clear reason; no speculative refactoring unrelated to the task; don't rewrite functioning code just because another implementation could be cleaner; prefer extending existing functionality; keep business logic, presentation, and data access separate.
- **Git**: inspect the working tree before starting; use `--no-pager` on git commands; work around existing changes rather than over them — stop and report only if they genuinely conflict.
- **CI/CD**: never publish GitHub Releases from `dev` or `staging`; keep firmware version, GitHub Release version, and OTA manifest version consistent; don't redesign the versioning convention without inspecting the existing CI configuration first.
- **Style**: match surrounding naming/formatting; small focused functions; prefer the standard loop: inspect → edit → build → diagnose → fix → test → review → commit.

## Known Limitations (design constraints, not bugs)

- The config web server only runs in station mode, not AP mode (power management is also disabled in AP mode — the QR setup screen stays lit).
- TLS trust: the OTA manifest + firmware download are verified against pinned CA roots in `include/ota_certs.h` (currently USERTrust ECC + ISRG Root X1 — the anchors behind GitHub's Sectigo/Let's Encrypt chains, cross-signed). The price fetches (`crypto.cpp`) use HTTPClient's default TLS behaviour (unverified — matches released firmware); don't rely on them for integrity.
- CoinGecko allows a single `vs_currencies` per request, so one batch price fetch shares a quote currency; per-ticker quotes apply only to per-ticker detail/history fetches.
- History is not persisted across reboot — it is refetched on boot.
- ESPAsyncWebServer/AsyncTCP are LGPL-3.0 (see README Licenses).
