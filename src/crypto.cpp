#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <layout_manager.h>
#include <lgfx_user_setup.h>
#include <rects.h>

extern const uint8_t bitcoin[];
extern const uint8_t sui[];
extern const uint8_t solana[];
extern void          parse_data(String Payload);
extern LGFX          tft;

struct CoinUI {
    Rect*          rect;
    const uint8_t* icon;
    uint16_t       iconColor;
    const char*    label;
    float          value;
};

CoinUI coins[] = {{&btcRect, bitcoin, TFT_ORANGE, "BTC", 0},
                  {&suiRect, sui, TFT_NAVY, "SUI", 0},
                  {&solRect, solana, TFT_PURPLE, "SOL", 0}};

int iconW = 45;
int iconH = 45;

void update_crypto() {
    if ((WiFi.status() != WL_CONNECTED)) {
        return;
    }
    HTTPClient http;
    http.begin(
        "https://api.coingecko.com/api/v3/simple/"
        "price?ids=bitcoin,solana,sui&vs_currencies=usd");
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();
    if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            parse_data(payload);
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
}

void drawCoin(const CoinUI& c) {
    Rect r = *c.rect;
    // int x = r.x + (r.w - iconW) / 2;
    // int y = r.y + (r.h - iconH) / 2;
    tft.fillRect(r.x, r.y, r.w, r.h, TFT_DARKGREY);
    tft.drawBitmap(r.x, r.y, c.icon, iconW, iconH, c.iconColor);
    char buf[16];
    dtostrf(c.value, 1, 2, buf);
    tft.setCursor(r.x + 60, r.y + 10);
    tft.print(c.label);
    tft.print(": $");
    tft.print(buf);
}

JsonDocument doc;

void parse_data(String input) {
    DeserializationError error = deserializeJson(doc, input);

    if (error) {
        Serial.println("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    coins[0].value = doc["bitcoin"]["usd"];
    coins[1].value = doc["sui"]["usd"];
    coins[2].value = doc["solana"]["usd"];

    for (int i = 0; i < 3; i++) {
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
        tft.setTextSize(2);
        drawCoin(coins[i]);
    }
    Serial.println("Display updated");
}
