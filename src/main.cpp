#include <Arduino.h>
#include <ESPmDNS.h>
#include <app_config.h>
#include <config_mgr.h>
#include <config_server.h>
#include <crypto.h>
#include <display.h>
#include <display_cycle.h>
#include <display_power.h>
#include <history.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>
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

// Independent, bounded history buffers for every configured ticker.
cryptoapp::HistoryBuffer histories[MAX_TICKERS];

// Latest fetched prices (one per configured ticker).
float latestPrices[MAX_TICKERS] = {};

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
        cryptoapp::update_prices_display(latestPrices, config.count(), config);
    } else {
        size_t idx = displayCycle.tickerIndex();
        cryptoapp::update_ticker_display(config.get(idx), latestPrices[idx], histories[idx]);
    }
}

// Fetch prices, update the display, and unmount the filesystem once the
// first successful update has happened. Returns true on success.
bool attemptUpdate() {
    float values[MAX_TICKERS];
    if (!cryptoapp::fetch_prices(values, config.count(), config)) {
        cryptoapp::show_message("Fetch failed");
        return false;
    }

    for (size_t i = 0; i < config.count(); i++) {
        latestPrices[i] = values[i];
        // Append to the history ring buffer for this ticker.
        histories[i].push(values[i]);
    }

    renderCurrentCycle();
    wifi.unmountFileSystem();
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
    }
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n[BOOT] ESP32-S3 Crypto Ticker");

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

        // Fetch the initial price data and historical graphs.
        if (attemptUpdate()) {
            fetchAllHistory();
            renderCurrentCycle();
        }
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

    if (wifi.isConnected()) {
        // Handle the ticker configuration web server.
        configServer.handle();

        // Handle button navigation events.
        cryptoapp::ButtonEvent btn;
        if (displayPower.consumeButtonEvent(btn)) {
            if (btn == cryptoapp::ButtonEvent::BUTTON_1) {
                displayCycle.previous();
            } else if (btn == cryptoapp::ButtonEvent::BUTTON_2) {
                displayCycle.next();
            }
            renderCurrentCycle();
        }

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