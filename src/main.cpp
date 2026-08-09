#include <Arduino.h>
#include <app_config.h>
#include <crypto.h>
#include <display.h>
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

cryptoapp::WifiManager wifi;
LGFX                   tft;

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

// Fetch prices, update the display, and unmount the filesystem once the
// first successful update has happened. Returns true on success.
bool attemptUpdate() {
    float values[NUM_COINS];
    if (!fetch_prices(values, NUM_COINS)) {
        show_message("Fetch failed");
        return false;
    }

    update_prices_display(values, NUM_COINS);
    wifi.unmountFileSystem();
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n[BOOT] ESP32-S3 Crypto Ticker");

    tft.setBrightness(150);
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextWrap(true);
    tft.setTextSize(2);
    tft.setCursor(0, 0);

    Serial.println("[BOOT] WiFi starting");
    bool connected = wifi.begin();
    Serial.printf("[MAIN] wifi.begin() -> %s\n", connected ? "CONNECTED" : "AP_MODE");

    render_layout(tft);

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
        attemptUpdate();
    }
}

void loop() {
    handleSerialCommand();

    wifi.handle();

    if (wifi.isConnected()) {
        static constexpr unsigned long UPDATE_INTERVAL = 60UL * 1000UL;  // 1 minute
        // Initialize so the first fetch happens immediately once connected,
        // rather than waiting a full interval after boot.
        static unsigned long last_update = millis() - UPDATE_INTERVAL;
        unsigned long        now         = millis();
        if ((now - last_update) > UPDATE_INTERVAL) {
            last_update = now;
            attemptUpdate();
        }
    }
}