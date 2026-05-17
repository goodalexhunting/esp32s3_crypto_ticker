#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

/*ESP32S3*/
#include "lgfx_config.h"
extern LGFX    tft;
extern void    parse_data(String Payload);
extern uint8_t bitcoin[];
extern uint8_t sui[];
extern uint8_t solana[];
void           update_crypto() {
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

    tft.fillScreen(TFT_BLACK);

    // icons
    tft.drawBitmap(5, 5, bitcoin, 45, 45, TFT_ORANGE);
    tft.drawBitmap(5, 55, sui, 45, 45, TFT_BLUE);
    tft.drawBitmap(5, 105, solana, 45, 45, TFT_PURPLE);

    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);

    // BTC
    tft.setCursor(60, 15);
    tft.print("$");
    tft.println(String(btc, 3));

    // SUI
    tft.setCursor(60, 70);
    tft.print("$");
    tft.println(String(sui_price, 3));

    // SOL
    tft.setCursor(60, 125);
    tft.print("$");
    tft.println(String(sol, 3));

    Serial.println("Display updated");
}
