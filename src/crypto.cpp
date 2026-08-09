#include "crypto.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "layout_manager.h"

namespace {

struct CoinUI {
    const char* label;  // Display label, e.g. "BTC"
    uint16_t    color;  // Brand color for the label
    const char* apiId;  // CoinGecko id, e.g. "bitcoin"
    float       value;
};

CoinUI coins[] = {
    {"BTC", TFT_YELLOW, "bitcoin", 0},
    {"SOL", TFT_PURPLE, "solana", 0},
    {"SUI", TFT_CYAN, "sui", 0},
};

constexpr int NUM_COINS = sizeof(coins) / sizeof(coins[0]);

const char* COINGECKO_URL = "https://api.coingecko.com/api/v3/simple/price?ids=";

// Table geometry (px) derived from the content grid region
constexpr int TABLE_INSET   = 10;
constexpr int HEADER_H      = 20;
constexpr int ROW_H         = 24;
constexpr int TEXT_OFFSET_X = 8;
constexpr int TEXT_OFFSET_Y = 6;

void formatPrice(float value, char* buf, size_t len) {
    if (value >= 1000.0f) {
        dtostrf(value, 1, 0, buf);
    } else if (value >= 1.0f) {
        dtostrf(value, 1, 2, buf);
    } else {
        dtostrf(value, 1, 3, buf);
    }
}

Rect contentArea() {
    LayoutManager layout(tft.width(), tft.height());
    return layout.grid(1, 6, 0, 1, 1, 4);
}

void drawCoinTable(const Rect& content) {
    // Table bounds
    Rect table = {content.x + TABLE_INSET,
                  content.y + TABLE_INSET,
                  content.w - TABLE_INSET * 2,
                  content.h - TABLE_INSET * 2};

    int colSplit = table.x + table.w / 2;  // SYMBOL | PRICE divider

    // Borders via cheap primitives
    tft.drawRect(table.x, table.y, table.w, table.h, TFT_WHITE);
    tft.drawLine(colSplit, table.y, colSplit, table.y + table.h, TFT_WHITE);
    tft.drawLine(table.x, table.y + HEADER_H, table.x + table.w, table.y + HEADER_H, TFT_WHITE);

    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);

    // Header row
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(table.x + TEXT_OFFSET_X, table.y + TEXT_OFFSET_Y);
    tft.print("SYMBOL");
    tft.setCursor(colSplit + TEXT_OFFSET_X, table.y + TEXT_OFFSET_Y);
    tft.print("PRICE (USD)");

    // Coin rows
    for (int i = 0; i < NUM_COINS; i++) {
        int rowY = table.y + HEADER_H + i * ROW_H;

        char buf[16];
        formatPrice(coins[i].value, buf, sizeof(buf));

        char priceStr[20];
        snprintf(priceStr, sizeof(priceStr), "$%s", buf);

        // Symbol (brand color)
        tft.setTextColor(coins[i].color, TFT_BLACK);
        tft.setCursor(table.x + TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
        tft.print(coins[i].label);

        // Price (right-aligned in the price column)
        int priceW = tft.textWidth(priceStr);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(colSplit + table.w / 2 - priceW - TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
        tft.print(priceStr);
    }
}

void buildUrl(String& url) {
    url = COINGECKO_URL;
    for (int i = 0; i < NUM_COINS; i++) {
        if (i > 0) url += ",";
        url += coins[i].apiId;
    }
    url += "&vs_currencies=usd";
}

}  // namespace

void render_layout(LovyanGFX& display) {
    LayoutManager layout(display.width(), display.height());

    Rect header = layout.grid(1, 6, 0, 0, 1, 1);
    Rect footer = layout.grid(1, 6, 0, 5, 1, 1);

    // Header
    display.fillRect(header.x, header.y, header.w, header.h, TFT_BLACK);
    display.setTextDatum(TL_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(2);
    display.setCursor(header.x + 10, header.y + 14);
    display.print("ESP32S3 Crypto Ticker");

    // Footer
    display.fillRect(footer.x, footer.y, footer.w, footer.h, TFT_BLACK);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setTextSize(1);
    display.setCursor(footer.x + 8, footer.y + 20);
    display.print("By github.com/goodalexhunting");
}

bool update_crypto() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    String url;
    buildUrl(url);

    HTTPClient http;
    http.begin(url);
    http.addHeader("User-Agent", "ESP32");
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();

            JsonDocument         doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error) {
                Serial.print("deserializeJson() failed: ");
                Serial.println(error.c_str());
                http.end();
                return false;
            }

            for (int i = 0; i < NUM_COINS; i++) {
                coins[i].value = doc[coins[i].apiId]["usd"] | 0.0f;
            }

            // Clear the content area and redraw the table
            Rect content = contentArea();
            tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);

            drawCoinTable(content);
            Serial.println("Display updated");
            http.end();
            return true;
        }
    } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    return false;
}
