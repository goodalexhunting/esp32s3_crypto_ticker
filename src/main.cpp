/**
 * Crypto Ticker
 *
 *  Created on: 24.05.2015 - https://github.com/osbock/AssetTicker
 *  Updated on: 16.05.2026 - https://github.com/goodalexhunting
 *
 */

#include <lgfx_user_setup.h>

#include "HTTPClient.h"
#include "layout_manager.h"
#include "wifi_mgr.h"


#define UPDATE_INTERVAL 1000 * 60 * 1

extern void            update_crypto();
extern void            render_layout(LovyanGFX& tft);
unsigned long          last_update = 0L;
Rect                   btcRect;
Rect                   suiRect;
Rect                   solRect;
cryptoapp::WifiManager wifi;
LGFX                   tft;

void setup() {
    Serial.println("BOOT 1");
    Serial.begin(115200);
    tft.setBrightness(150);
    Serial.println("BOOT 2: WiFi starting");
    wifi.connectWifi();
    // TODO: when this fails, we should not continue with the rest of the setup, but instead show an
    // error message on the display and retry connecting to WiFi after some time

    Serial.println("BOOT 3: TFT Config and first update");
    tft.init();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    render_layout(tft);
    tft.setTextColor(TFT_WHITE);
    tft.setTextWrap(true);
    tft.setCursor(0, 0);
    tft.setTextSize(3);
    update_crypto();
}

void render_layout(LovyanGFX& tft) {
    LayoutManager layout = LayoutManager(tft.width(), tft.height());

    Rect          header  = layout.grid(1, 6, 0, 0, 1, 1);
    Rect          content = layout.grid(1, 6, 0, 1, 1, 4);
    Rect          footer  = layout.grid(1, 6, 0, 5, 1, 1);
    LayoutManager contentLayout(content.w, content.h);

    btcRect = contentLayout.inset(contentLayout.grid(1, 3, 0, 0), 0);
    suiRect = contentLayout.inset(contentLayout.grid(1, 3, 0, 1), 0);
    solRect = contentLayout.inset(contentLayout.grid(1, 3, 0, 2), 0);

    // Offset by content.x and content.y
    btcRect.x += content.x;
    btcRect.y += content.y;
    suiRect.x += content.x;
    suiRect.y += content.y;
    solRect.x += content.x;
    solRect.y += content.y;

    // Draw header
    tft.fillRect(header.x, header.y, header.w, header.h, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(header.x + 10, header.y + 10);
    tft.print("ESP32S3 Crypto Ticker");

    // Draw footer
    tft.fillRect(footer.x, footer.y, footer.w, footer.h, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(footer.x + 10, footer.y + 10);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.print("By github.com/goodalexhunting");
}

void loop() {
    unsigned long currenttime = millis();
    if ((currenttime - last_update) > UPDATE_INTERVAL) {
        last_update = currenttime;
        update_crypto();
    }
}
