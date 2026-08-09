#include "display.h"

#include <Arduino.h>

#include "app_config.h"

namespace {

// ---------------------------------------------------------------------------
// Layout geometry (px)
// ---------------------------------------------------------------------------
constexpr uint8_t TABLE_INSET   = 10;
constexpr uint8_t HEADER_H      = 24;
constexpr uint8_t ROW_H         = 24;
constexpr uint8_t TEXT_OFFSET_X = 8;
constexpr uint8_t TEXT_OFFSET_Y = 6;

// Number of columns rows in the page grid
constexpr uint8_t GRID_COLS       = 1;
constexpr uint8_t GRID_ROWS       = 6;
constexpr uint8_t HEADER_ROW      = 0;
constexpr uint8_t CONTENT_ROW     = 1;
constexpr uint8_t CONTENT_ROWSPAN = 4;
constexpr uint8_t FOOTER_ROW      = 5;

// Header / footer text insets
constexpr uint8_t HEADER_INSET_X = 10;
constexpr uint8_t HEADER_INSET_Y = 14;
constexpr uint8_t FOOTER_INSET_X = 8;
constexpr uint8_t FOOTER_INSET_Y = 20;

// Body/stale message insets relative to the content area
constexpr uint8_t MESSAGE_INSET = 8;

// Text sizes
constexpr uint8_t TITLE_TEXT_SIZE = 2;
constexpr uint8_t BODY_TEXT_SIZE  = 1;
constexpr uint8_t TABLE_TEXT_SIZE = 2;

// Runtime storage for the latest fetched prices.
float coinValues[NUM_COINS] = {};

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
    return layout.grid(GRID_COLS, GRID_ROWS, 0, CONTENT_ROW, 1, CONTENT_ROWSPAN);
}

void drawCoinTable(const Rect& content) {
    // Table bounds
    Rect table = {content.x + TABLE_INSET,
                  content.y + TABLE_INSET,
                  content.w - TABLE_INSET * 2,
                  content.h - TABLE_INSET * 2};

    int colSplit = table.x + table.w / 2;  // SYMBOL | PRICE divider

    // Row height adapts to the available space when the screen is smaller
    // than the original target display.
    const int rowH =
        (table.h - HEADER_H) / NUM_COINS > ROW_H ? ROW_H : (table.h - HEADER_H) / NUM_COINS;

    // Borders via cheap primitives
    tft.drawRect(table.x, table.y, table.w, table.h, TFT_WHITE);
    tft.drawLine(colSplit, table.y, colSplit, table.y + table.h, TFT_WHITE);
    tft.drawLine(table.x, table.y + HEADER_H, table.x + table.w, table.y + HEADER_H, TFT_WHITE);

    tft.setTextSize(TABLE_TEXT_SIZE);
    tft.setTextDatum(TL_DATUM);

    // Header row
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(table.x + TEXT_OFFSET_X, table.y + TEXT_OFFSET_Y);
    tft.print("SYMBOL");
    tft.setCursor(colSplit + TEXT_OFFSET_X, table.y + TEXT_OFFSET_Y);
    tft.print("PRICE (USD)");

    // Coin rows
    for (int i = 0; i < NUM_COINS; i++) {
        int rowY = table.y + HEADER_H + i * rowH;

        char buf[16];
        formatPrice(coinValues[i], buf, sizeof(buf));

        char priceStr[20];
        snprintf(priceStr, sizeof(priceStr), "$%s", buf);

        // Symbol (brand color)
        tft.setTextColor(COINS[i].color, TFT_BLACK);
        tft.setCursor(table.x + TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
        tft.print(COINS[i].label);

        // Price (right-aligned in the price column)
        int priceW = tft.textWidth(priceStr);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(colSplit + table.w / 2 - priceW - TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
        tft.print(priceStr);
    }
}

}  // namespace

void render_layout(LovyanGFX& display) {
    LayoutManager layout(display.width(), display.height());

    Rect header = layout.grid(GRID_COLS, GRID_ROWS, 0, HEADER_ROW, 1, 1);
    Rect footer = layout.grid(GRID_COLS, GRID_ROWS, 0, FOOTER_ROW, 1, 1);

    // Header
    display.fillRect(header.x, header.y, header.w, header.h, TFT_BLACK);
    display.setTextDatum(TL_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setTextSize(TITLE_TEXT_SIZE);
    display.setCursor(header.x + HEADER_INSET_X, header.y + HEADER_INSET_Y);
    display.print("ESP32S3 Crypto Ticker");

    // Footer
    display.fillRect(footer.x, footer.y, footer.w, footer.h, TFT_BLACK);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setTextSize(BODY_TEXT_SIZE);
    display.setCursor(footer.x + FOOTER_INSET_X, footer.y + FOOTER_INSET_Y);
    display.print("By github.com/goodalexhunting");
}

void update_prices_display(const float* values, size_t count) {
    if (count > NUM_COINS) count = NUM_COINS;
    for (size_t i = 0; i < count; i++) {
        coinValues[i] = values[i];
    }

    // Redraw the full layout (header/footer) to wipe any leftover
    // AP-mode QR screen content, then draw the table.
    render_layout(tft);
    Rect content = contentArea();
    tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);

    drawCoinTable(content);
    Serial.println("Display updated");
}

void show_message(const char* msg) {
    Rect content = contentArea();
    tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(BODY_TEXT_SIZE);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(content.x + MESSAGE_INSET, content.y + MESSAGE_INSET);
    tft.print(msg);
}