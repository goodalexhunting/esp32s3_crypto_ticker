#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <layout_manager.h>
#include <rects.h>

#include "TFT_eSPI.h"
extern TFT_eSPI tft;
extern uint8_t  bitcoin[];
extern uint8_t  sui[];
extern uint8_t  solana[];
extern void     parse_data(String Payload);

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

JsonDocument doc;

void parse_data(String input) {
    DeserializationError error = deserializeJson(doc, input);

    if (error) {
        Serial.println("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    float btc       = doc["bitcoin"]["usd"];
    float sol       = doc["solana"]["usd"];
    float sui_price = doc["sui"]["usd"];

    // BTC
    tft.fillRect(btcRect.x, btcRect.y, btcRect.w, btcRect.h, TFT_DARKGREY);
    tft.drawBitmap(btcRect.x + 10, btcRect.y + 10, bitcoin, 45, 45, TFT_ORANGE);
    tft.setCursor(btcRect.x + 60, btcRect.y + 20);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.print("BTC: $");
    tft.println(String(btc, 2));

    // SUI
    tft.fillRect(suiRect.x, suiRect.y, suiRect.w, suiRect.h, TFT_DARKGREY);
    tft.drawBitmap(suiRect.x + 10, suiRect.y + 10, sui, 45, 45, TFT_NAVY);
    tft.setCursor(suiRect.x + 60, suiRect.y + 20);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.print("SUI: $");
    tft.println(String(sui_price, 2));

    // SOL
    tft.fillRect(solRect.x, solRect.y, solRect.w, solRect.h, TFT_DARKGREY);
    tft.drawBitmap(solRect.x + 10, solRect.y + 10, solana, 45, 45, TFT_PURPLE);
    tft.setCursor(solRect.x + 60, solRect.y + 20);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.print("SOL: $");
    tft.println(String(sol, 2));

    Serial.println("Display updated");
}
