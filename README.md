# ESP32-S3 Crypto Ticker

A live cryptocurrency price ticker for the [LilyGO T-Display-S3](https://www.lilygo.cc/products/t-display-s3). It shows BTC, SOL, and SUI by default, supports up to eight configurable tickers, and can update its firmware over Wi-Fi.

## Install a release

Download the latest production release from [GitHub Releases](https://github.com/goodalexhunting/esp32s3_crypto_ticker/releases/latest). Releases include:

- `firmware.bin` — application firmware.
- `partitions.bin` — the partition layout used by this firmware.
- `ota_manifest.json` — version and checksum information used by the device's automatic updater. You do not flash this file.

### First installation over USB

The release files are raw binaries, not a one-click installer. Use Espressif's [esptool](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/) and a USB data cable. The board must have a compatible ESP32-S3 bootloader at the standard flash location. The release does not include a bootloader image, so a completely blank or differently provisioned board may need the source-based PlatformIO installation instead (see [Build from source](#build-from-source)).

For a board with a compatible bootloader:

1. Download `firmware.bin` and `partitions.bin` from the same release.
2. Connect the T-Display-S3 to your computer by USB. Find its serial port (for example, `COM5` on Windows or `/dev/ttyACM0` on Linux).
3. Install esptool with Python if needed: `python -m pip install esptool`.
4. Put the board in download mode if it is not detected automatically: hold **BOOT**, press and release **RESET**, then release **BOOT**.
5. Erase the flash, then write the partition table and firmware at their offsets:

   ```sh
   python -m esptool --chip esp32s3 --port PORT erase-flash
   python -m esptool --chip esp32s3 --port PORT write-flash 0x8000 partitions.bin 0x10000 firmware.bin
   ```

   Replace `PORT` with the serial port for your computer. Erasing flash clears saved Wi-Fi credentials and configuration. The firmware image is written to the factory app slot; the partition table reserves two additional slots for future OTA updates.
6. Reset or power-cycle the board. Continue with [First-time Wi-Fi setup](#first-time-wi-fi-setup).

If esptool cannot connect, check the USB cable and port, close other programs using the serial port, and retry with the board in download mode. For a completely fresh board, use [Build from source](#build-from-source), which lets PlatformIO supply the matching bootloader and other required images.

### Updating an existing installation over USB

For a device already running this ticker with its current partition layout, you can manually install a release with esptool without erasing the device:

```sh
python -m esptool --chip esp32s3 --port PORT write-flash 0x10000 firmware.bin
```

This updates the factory slot. It preserves the active OTA slot and saved settings. For routine updates, prefer the automatic OTA process described below.

## First-time Wi-Fi setup

1. Power on the device. If it has no saved Wi-Fi network, it starts an open access point named `CryptoTicker-XXXX`.
2. Join that network on your phone or computer. The display also shows a QR code to make joining easier.
3. Open the captive portal when prompted, or browse to `http://192.168.4.1`.
4. Choose your Wi-Fi network (or enter its name), provide the password, and tap **Connect**.
5. Once connected, the ticker starts and saves the network for future restarts.

If the saved network cannot be reached, the device starts its setup access point again. You can also clear saved Wi-Fi credentials over the serial console with the command `clearwifi`.

## Configure tickers

When the device and your phone or computer are on the same Wi-Fi network, open [http://crypto-ticker.local](http://crypto-ticker.local). If that name does not resolve, use the device's IP address shown by your router. The project page links to the configuration screen hosted by the device.

From the configuration screen you can add, remove, reorder, or reset tickers. Each ticker has a display label, a CoinGecko API ID, a quote currency, and an optional color. For example, the label `BTC` uses the CoinGecko ID `bitcoin`. Up to eight tickers can be configured. Changes are saved and applied without reflashing.

The display cycles through a price overview and a detail view for each ticker. The detail view includes its price, 24-hour change, and recent price history. The buttons move between views. The display dims after inactivity and sleeps to reduce power use; pressing either button wakes it.

## Automatic OTA updates

After connecting to Wi-Fi, the device checks for a release shortly after startup and about once an hour while it remains awake and connected. When a newer release is available, it downloads the firmware, verifies its SHA-256 checksum, and installs it to the inactive OTA slot. The device then restarts into the update. Keep it powered during this process.

The firmware checks that startup completed successfully. If the new version fails its startup check, the bootloader automatically returns to the previously working OTA version. The factory image is a separate recovery fallback and is not changed by OTA updates.

## Troubleshooting and limitations

- **No setup access point appears:** allow the device time to boot, then reset it. If it has working saved Wi-Fi credentials, it joins that network instead of creating an access point.
- **The local address does not open:** try the device's IP address. Some networks and browsers do not support `.local` names reliably.
- **Prices are missing or delayed:** the device needs internet access to reach CoinGecko. API rate limits or network outages can temporarily interrupt updates.
- **Quote currencies:** CoinGecko's batch price endpoint accepts one quote currency per request, so tickers in the overview share a quote currency. Individual ticker detail/history requests use that ticker's configured quote.
- **History after reboot:** historical graph data is fetched again after startup and is not retained across power cycles.

## Build from source

This section is for people who want to build or install the firmware from the source code. Install [PlatformIO Core](https://platformio.org/install/cli), clone this repository, connect the board by USB, then run:

```sh
pio run -e lilygo-t-display-s3 -t upload
```

PlatformIO builds the firmware and flashes the matching bootloader, partition table, and application. This is the recommended route for a blank board. The embedded configuration pages are included in the firmware; no separate filesystem image is required.

## License

This project embeds [ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) and [AsyncTCP](https://github.com/ESP32Async/AsyncTCP), licensed under LGPL-3.0. The rest of the project retains its original license.
