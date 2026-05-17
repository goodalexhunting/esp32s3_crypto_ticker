/**
 * Crypto Ticker
 *
 *  Created on: 24.05.2015 - https://github.com/osbock/AssetTicker
 *  Updated on: 16.05.2026 - https://github.com/goodalexhunting
 *
 */

#include <HTTPClient.h>

#include "config.h"
#include "lgfx_config.h"
#include "wifi_mgr.h"

#define UPDATE_INTERVAL 1000 * 60 * 1

extern void   update_crypto();
unsigned long last_update = 0L;

cryptoapp::WifiManager wifi;
LGFX                   tft = LGFX();

void setup() {
    Serial.begin(115200);

    Serial.println("BOOT 1");
    pinMode(PIN_POWER_ON, OUTPUT);
    digitalWrite(PIN_POWER_ON, HIGH);
    pinMode(PIN_LCD_BL, OUTPUT);
    digitalWrite(PIN_LCD_BL, HIGH);
    pinMode(PIN_BUTTON_1, INPUT_PULLUP);

    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_WHITE);

    Serial.println("BOOT 2: WiFi starting");
    wifi.connectWifi();
    // TODO: when this fails, we should not continue with the rest of the setup, but instead show an
    // error message on the display and retry connecting to WiFi after some time

    Serial.println("BOOT 3: TFT Config and first update");
    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextWrap(true);
    tft.setCursor(0, 170);
    tft.setTextSize(3);
    update_crypto();
}

void loop() {
    unsigned long currenttime = millis();
    if ((currenttime - last_update) > UPDATE_INTERVAL) {
        last_update = currenttime;
        update_crypto();
    }
}
