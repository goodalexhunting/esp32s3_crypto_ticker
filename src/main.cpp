#include <Arduino.h>
#include <crypto.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>
#include <qr_display.h>
#include <wifi_mgr.h>

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
        // AP mode: show a QR code pointing at the captive-portal landing page
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(middle_center);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(1);
        tft.drawString("Scan QR to configure WiFi", 160, 10);

        cryptoapp::drawQrCode(tft, "http://192.168.4.1", 160, 90, 4, false);

        tft.setTextColor(TFT_WHITE);
        tft.drawString("Join AP: " + wifi.getAPName(), 160, 160);
        tft.setTextDatum(top_left);
    } else {
        if (update_crypto()) {
            wifi.unmountFileSystem();
        }
    }
}

void loop() {
    handleSerialCommand();

    wifi.handle();

    if (wifi.isConnected()) {
        static unsigned long           last_update     = 0UL;
        static constexpr unsigned long UPDATE_INTERVAL = 60UL * 1000UL;  // 1 minute
        unsigned long                  now             = millis();
        if ((now - last_update) > UPDATE_INTERVAL) {
            last_update = now;
            if (update_crypto()) {
                wifi.unmountFileSystem();
            }
        }
    }
}
