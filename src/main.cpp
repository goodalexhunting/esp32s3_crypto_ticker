#include <Arduino.h>
#include <crypto.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>
#include <wifi_mgr.h>

cryptoapp::WifiManager wifi;
LGFX                   tft;

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

    render_layout(tft);

    if (!connected) {
        // AP mode setup page shown on display
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(10, 20);
        tft.println("WiFi not configured");
        tft.println();
        tft.println("Connect phone to AP:");
        tft.setTextColor(TFT_CYAN);
        tft.println(" CryptoTicker-XXXX");
        tft.setTextColor(TFT_WHITE);
        tft.println();
        tft.println("Then visit:");
        tft.setTextColor(TFT_CYAN);
        tft.println(" http://192.168.4.1");
        tft.setTextColor(TFT_WHITE);
        tft.println();
        tft.println("Setup WiFi and the");
        tft.println("ticker will start.");
    } else {
        if (update_crypto()) {
            wifi.unmountFileSystem();
        }
    }
}

void loop() {
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
