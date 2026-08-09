#include <Arduino.h>
#include <ESPmDNS.h>
#include <api_health.h>
#include <app_config.h>
#include <config_mgr.h>
#include <config_server.h>
#include <crypto.h>
#include <display.h>
#include <display_cycle.h>
#include <display_power.h>
#include <esp_task_wdt.h>
#include <history.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>
#include <ota_mgr.h>
#include <qr_display.h>
#include <wifi_mgr.h>

// Vertical layout for the AP-mode QR screen, expressed relative to the
// configured screen size so it adapts to other displays.
constexpr uint8_t  QR_TOP_MARGIN    = 10;                 // px from the top to the prompt text
constexpr uint16_t QR_CENTER_Y      = SCREEN_HEIGHT / 2;  // vertical centre of the QR
constexpr uint8_t  QR_BOTTOM_MARGIN = 10;                 // px from the bottom to the AP text
constexpr uint16_t QR_TITLE_Y       = QR_TOP_MARGIN;
constexpr uint16_t QR_TEXT_Y        = SCREEN_HEIGHT - QR_BOTTOM_MARGIN;

cryptoapp::WifiManager   wifi;
cryptoapp::ConfigManager config;
cryptoapp::ConfigServer  configServer(config);
LGFX                     tft;
cryptoapp::DisplayPower  displayPower;
cryptoapp::DisplayCycle  displayCycle(config);
cryptoapp::ApiHealth     apiHealth;
cryptoapp::OtaManager    otaManager;

// Independent, bounded history buffers for every configured ticker.
cryptoapp::HistoryBuffer histories[MAX_TICKERS];

// Latest fetched prices and 24h changes (one per configured ticker).
cryptoapp::PriceData latestData[MAX_TICKERS] = {};

// Put the whole device to sleep: power down the WiFi radio, then enter
// deep sleep until one of the wake buttons is pressed. A wake restarts
// the ESP32, so setup() reconnects WiFi and refreshes prices.
void goToDeepSleep() {
    Serial.println("[SLEEP] Display OFF - entering deep sleep");
    Serial.flush();

    wifi.sleep();

    esp_sleep_enable_ext1_wakeup(DEEP_SLEEP_WAKEUP_MASK, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();  // does not return
}

void handleSerialCommand() {
    if (!Serial.available()) {
        return;
    }
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "clearwifi") {
        Serial.println("[CMD] Clearing WiFi credentials...");
        wifi.clearCredentials();
        delay(200);
        ESP.restart();
    }
}

// Redraw the display according to the current cycle.
void renderCurrentCycle() {
    if (displayCycle.isTable()) {
        cryptoapp::update_prices_display(latestData, config.count(), config);
    } else {
        size_t idx = displayCycle.tickerIndex();
        cryptoapp::update_ticker_display(config.get(idx), latestData[idx], histories[idx]);
    }
    cryptoapp::draw_api_status(apiHealth.status());
}

// Fetch prices and update the display. Returns true on success.
bool attemptUpdate() {
    cryptoapp::PriceData data[MAX_TICKERS];
    if (!cryptoapp::fetch_prices(data, config.count(), config)) {
        apiHealth.recordFailure();
        cryptoapp::draw_api_status(apiHealth.status());
        cryptoapp::show_message("Fetch failed");
        return false;
    }

    apiHealth.recordSuccess();

    for (size_t i = 0; i < config.count(); i++) {
        latestData[i] = data[i];
        // Append to the history ring buffer for this ticker.
        histories[i].push(data[i].price);
    }

    renderCurrentCycle();
    return true;
}

// Fetch historical data for a single ticker (used on first boot and
// when a ticker is added via the web config).
void fetchHistoryFor(size_t idx) {
    if (idx >= config.count()) {
        return;
    }
    cryptoapp::fetch_history(config.get(idx), histories[idx]);
}

// Fetch historical data for all configured tickers.
void fetchAllHistory() {
    for (size_t i = 0; i < config.count(); i++) {
        fetchHistoryFor(i);
        // Feed the boot watchdog between blocking market_chart requests so
        // a slow link across many tickers cannot falsely trip the rollback
        // window. Each fetch is individually bounded (~13s max).
        esp_task_wdt_reset();
    }
}

// Forward declaration - defined below setup().
void checkForUpdates();

// Arm the boot-time hardware task watchdog. If the app fails to initialise
// and never reaches OtaManager::selfTestVerification() within
// BOOT_SELF_TEST_TIMEOUT_MS, the watchdog forces a reboot and the
// bootloader (with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) rolls back to
// the previously working partition. This is what prevents bricking after
// an OTA update.
void armBootWatchdog() {
    // ESP-IDF v4.4 API: init takes the timeout in SECONDS, not ms.
    // BOOT_SELF_TEST_TIMEOUT_MS is the 30-second rollback window.
    esp_task_wdt_init(BOOT_SELF_TEST_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(nullptr);  // subscribe the current (loop task) handle
    Serial.printf("[BOOT] Self-test watchdog armed (%lu ms)\n",
                  (unsigned long)BOOT_SELF_TEST_TIMEOUT_MS);
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n[BOOT] ESP32-S3 Crypto Ticker");

    // Arm the boot self-test watchdog first so that every following init
    // step is covered: if anything hangs, the watchdog reboots into the
    // previously working partition.
    armBootWatchdog();

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextWrap(true);
    tft.setTextSize(2);
    tft.setCursor(0, 0);

    // Load the ticker configuration from NVS (or seed defaults).
    config.begin();

    Serial.println("[BOOT] WiFi starting");
    bool connected = wifi.begin();
    Serial.printf("[MAIN] wifi.begin() -> %s\n", connected ? "CONNECTED" : "AP_MODE");
    // Feed the boot watchdog after the blocking WiFi connect (up to ~10s),
    // so a slow but healthy network can never falsely trigger a rollback.
    esp_task_wdt_reset();

    // Power management only runs in normal ticker mode; in AP mode the QR
    // setup screen must stay permanently lit.
    displayPower.begin(connected && DISPLAY_POWER_ENABLED);

    cryptoapp::render_layout(tft);

    if (!connected) {
        // AP mode: show a QR code that invites the user to join the open AP.
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(middle_center);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.drawString("Scan QR to join WiFi", SCREEN_WIDTH / 2, QR_TITLE_Y);

        // Standard Wi-Fi QR payload for an open (passwordless) network.
        String wifiQr = "WIFI:T:nopass;S:" + wifi.getAPName() + ";;";
        cryptoapp::drawQrCode(tft, wifiQr.c_str(), SCREEN_WIDTH / 2, QR_CENTER_Y, 4, false);

        tft.setTextColor(TFT_WHITE);
        tft.drawString("Join AP: " + wifi.getAPName(), SCREEN_WIDTH / 2, QR_TEXT_Y);
        tft.setTextDatum(top_left);
    } else {
        // Start mDNS so the device is reachable at http://crypto-ticker.local
        if (MDNS.begin(MDNS_HOSTNAME)) {
            MDNS.addService("http", "tcp", 80);
            Serial.printf("[MDNS] Started: http://%s.local\n", MDNS_HOSTNAME);
        } else {
            Serial.println("[MDNS] Failed to start - continuing without mDNS");
        }

        // Start the ticker configuration web server.
        configServer.begin();

        // Fetch the initial price data and historical graphs. The network
        // stack may not be fully ready for an immediate HTTPS request right
        // after WL_CONNECTED, so retry briefly before showing the error.
        bool fetched = false;
        for (int attempt = 0; attempt < 3 && !fetched; attempt++) {
            if (attempt > 0) {
                delay(2000);  // allow the network stack to settle
            }
            fetched = attemptUpdate();
            // Feed the boot watchdog between blocking HTTPS requests. Each
            // fetch is individually bounded (~13s max), but several in a row
            // on a slow link could otherwise exceed the 30s rollback window.
            esp_task_wdt_reset();
        }
        if (fetched) {
            fetchAllHistory();
            renderCurrentCycle();
        }

        // The OTA check is deferred to loop() (see below) so a slow or
        // unreachable update server can never block boot, the first
        // render, or button handling.
    }

    // Boot self-test passed: cancel the pending OTA rollback and disarm the
    // boot watchdog. If a hung component above had prevented this call, the
    // watchdog would have rebooted back into the known-good partition.
    otaManager.selfTestVerification();
}

// Perform one OTA update check. Runs on a fixed hourly schedule, gated on
// WiFi connectivity, so a slow or unreachable update server can never
// block the ticker loop.
void checkForUpdates() {
    Serial.println("[OTA] Checking for firmware updates...");
    cryptoapp::OtaManager::CheckResult result = otaManager.checkForUpdate(OTA_MANIFEST_URL);

    switch (result) {
        case cryptoapp::OtaManager::CheckResult::UP_TO_DATE:
            Serial.println("[OTA] No update needed");
            break;
        case cryptoapp::OtaManager::CheckResult::UPDATE_AVAILABLE:
            Serial.println("[OTA] New firmware available - installing");
            otaManager.performUpdate(otaManager.firmwareUrl(), otaManager.sha256());
            break;
        case cryptoapp::OtaManager::CheckResult::ERROR:
        default:
            Serial.println("[OTA] Update check failed - continuing with current firmware");
            break;
    }
}

void loop() {
    handleSerialCommand();

    wifi.handle();

    // Poll the wake buttons and run the display idle state machine
    // (ON -> DIMMED -> OFF).
    displayPower.handle();

    // When the display turns fully off, sleep the WiFi radio and the
    // main loop until a wake button is pressed.
    if (displayPower.consumeSleepEvent() && DEVICE_DEEP_SLEEP_ENABLED) {
        goToDeepSleep();
    }

    // Button navigation is local-only (the cycle derives from NVS config),
    // so consume events regardless of the network state. This keeps the
    // buttons responsive while connecting, and prevents a press during
    // AP mode/reconnect from leaving a stale event that would otherwise
    // fire once the network comes back.
    cryptoapp::ButtonEvent btn;
    if (displayPower.consumeButtonEvent(btn) && wifi.isConnected()) {
        if (btn == cryptoapp::ButtonEvent::BUTTON_1) {
            displayCycle.previous();
        } else if (btn == cryptoapp::ButtonEvent::BUTTON_2) {
            displayCycle.next();
        }
        renderCurrentCycle();
    }

    if (wifi.isConnected()) {
        // Handle the ticker configuration web server.
        configServer.handle();

        // If the ticker configuration changed (add/remove/move/reset),
        // update the display cycles and history buffers.
        static uint32_t lastConfigRevision = config.revision();
        if (config.revision() != lastConfigRevision) {
            lastConfigRevision = config.revision();
            displayCycle.clamp();

            // Reset history for tickers that may have changed.
            for (size_t i = 0; i < MAX_TICKERS; i++) {
                histories[i].reset();
            }
            fetchAllHistory();
            renderCurrentCycle();
        }

        // Firmware update check, deferred out of setup() and run shortly
        // after boot inside loop(), so a slow or unreachable update server
        // can never block boot, the first render, or the button handler.
        // The check then repeats every OTA_CHECK_INTERVAL_MS (hourly) for
        // the lifetime of the device.
        static constexpr unsigned long OTA_FIRST_CHECK_DELAY_MS = 30UL * 1000UL;
        static unsigned long           otaCheckAt  = millis() + OTA_FIRST_CHECK_DELAY_MS;
        static bool                    otaCheckDue = false;
        if (!otaCheckDue && (long)(millis() - otaCheckAt) >= 0) {
            otaCheckDue = true;
        }
        if (otaCheckDue && (long)(millis() - otaCheckAt) >= 0) {
            otaCheckAt += OTA_CHECK_INTERVAL_MS;
            checkForUpdates();
        }

        static constexpr unsigned long UPDATE_INTERVAL = 60UL * 1000UL;  // 1 minute
        // Initialize so the first fetch happens immediately once connected,
        // rather than waiting a full interval after boot.
        static unsigned long last_update = millis() - UPDATE_INTERVAL;
        unsigned long        now         = millis();

        // The screen just woke from dimmed/off: refresh immediately so the
        // displayed prices are current, then let the normal 60s cycle resume.
        if (displayPower.consumeWakeEvent()) {
            last_update = now - UPDATE_INTERVAL;
        }

        // Skip periodic fetching while the display is fully off; the screen
        // is invisible and a wake event above forces an immediate refresh.
        if (!displayPower.isOff() && (now - last_update) > UPDATE_INTERVAL) {
            last_update = now;
            attemptUpdate();
        }
    }
}