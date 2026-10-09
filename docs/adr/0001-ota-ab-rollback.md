# OTA updates use A/B slots with a 30-second boot self-test and automatic rollback

OTA updates write the verified new firmware to the inactive of two interchangeable OTA slots (`ota_0`/`ota_1`), reboot into it, and the freshly booted image must confirm its own health via self-test verification within a 30-second boot watchdog window; if it doesn't, the bootloader rolls back to the previously working slot. The factory slot is never updated over the air — it holds the USB-flashed firmware and is the bootloader's last resort when no OTA slot boots.

## Considered Options

- **Single OTA slot, OTA into the factory slot** — rejected: one bad update would destroy the only known-good image. Rolling back to factory would also lose every previously applied update.
- **`esp_https_ota` convenience wrapper** — rejected: its internal redirect handling broke against GitHub's release CDN (double redirect → Fastly 618 errors; fixed in commit `65a8658` by moving to manual redirects), and it offered no place to verify the SHA-256 before flashing. Manual redirects plus the raw `esp_ota` streaming API restore both.
- **No watchdog — verify opportunistically** — rejected: if the new image hangs before it reaches verification, the device stays bricked until a USB reflash, defeating the purpose of OTA.

## Consequences

- The entire boot path — Wi-Fi connect, mDNS, config server, initial price fetch with retries — must reach `selfTestVerification()` within the 30-second window, or a *healthy* image is rolled back. Every boot step added later spends this budget.
- Rollback returns to the *other OTA slot*, not to factory; the factory firmware is only as recent as the last USB flash and is exercised only when neither OTA slot boots.
- Rollback is enabled by the Arduino prebuilt SDK (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`), so this safety net depends on the prebuilt SDK keeping that config rather than on anything in `platformio.ini`.
