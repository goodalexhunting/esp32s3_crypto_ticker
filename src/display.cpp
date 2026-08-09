#include "display.h"

#include <Arduino.h>

#include "app_config.h"

namespace cryptoapp {

namespace {

// Graph layout geometry (px)
constexpr uint8_t GRAPH_INSET_X       = 12;
constexpr uint8_t GRAPH_TOP           = 40;
constexpr uint8_t GRAPH_BOTTOM_MARGIN = 8;
constexpr uint8_t GRAPH_PAD           = 4;
constexpr uint8_t DETAIL_LABEL_SIZE   = 2;
constexpr uint8_t DETAIL_PRICE_SIZE   = 2;
constexpr uint8_t DETAIL_QUOTE_SIZE   = 1;

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

// Runtime storage for the latest fetched prices and 24h changes.
PriceData coinData[MAX_TICKERS] = {};

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

void drawCoinTable(const Rect& content, const ConfigManager& config) {
    // Table bounds
    Rect table = {content.x + TABLE_INSET,
                  content.y + TABLE_INSET,
                  content.w - TABLE_INSET * 2,
                  content.h - TABLE_INSET * 2};

    int colSplit = table.x + table.w / 2;  // SYMBOL | PRICE divider

    // Row height adapts to the available space when the screen is smaller
    // than the original target display.
    const size_t numCoins = config.count();
    const int    rowH =
        (table.h - HEADER_H) / numCoins > ROW_H ? ROW_H : (table.h - HEADER_H) / numCoins;

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
    for (size_t i = 0; i < numCoins; i++) {
        int rowY = table.y + HEADER_H + i * rowH;

        char buf[16];
        formatPrice(coinData[i].price, buf, sizeof(buf));

        char priceStr[20];
        snprintf(priceStr, sizeof(priceStr), "$%s", buf);

        // Symbol (brand color)
        tft.setTextColor(config.get(i).color, TFT_BLACK);
        tft.setCursor(table.x + TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
        tft.print(config.get(i).label);

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

    // Firmware version in the bottom-right corner.
    char version[24];
    snprintf(version, sizeof(version), "FW %s", FW_VERSION);
    display.setTextDatum(TR_DATUM);
    display.setCursor(footer.x + footer.w - FOOTER_INSET_X, footer.y + FOOTER_INSET_Y);
    display.print(version);
    display.setTextDatum(TL_DATUM);
}

void update_prices_display(const PriceData* data, size_t count, const ConfigManager& config) {
    size_t n = count < config.count() ? count : config.count();
    for (size_t i = 0; i < n; i++) {
        coinData[i] = data[i];
    }

    // Redraw the full layout (header/footer) to wipe any leftover
    // AP-mode QR screen content, then draw the table.
    render_layout(tft);
    Rect content = contentArea();
    tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);

    drawCoinTable(content, config);
    Serial.println("Display updated");
}

void update_ticker_display(const TickerConfig&  ticker,
                           const PriceData&     data,
                           const HistoryBuffer& history) {
    // Start from a clean slate every time so the header/footer etc.
    // are rebuilt and the content area is blanked.
    render_layout(tft);

    LayoutManager layout(tft.width(), tft.height());
    Rect          content = contentArea();
    tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);

    // --- Ticker pair (label + quote) on one line ---
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(ticker.color, TFT_BLACK);
    tft.setTextSize(DETAIL_LABEL_SIZE);
    int pairX = content.x + GRAPH_INSET_X;
    int pairY = content.y + 4;
    tft.setCursor(pairX, pairY);
    tft.print(ticker.label);

    // Quote currency immediately after the label (small, dimmed),
    // vertically centred against the size-2 label text.
    int labelW = tft.textWidth(ticker.label);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setTextSize(DETAIL_QUOTE_SIZE);
    tft.setCursor(pairX + labelW + 6, content.y + 8);
    tft.print("/ ");
    tft.print(ticker.quote);

    // --- Current price ---
    char buf[16];
    formatPrice(data.price, buf, sizeof(buf));
    char priceStr[24];
    snprintf(priceStr, sizeof(priceStr), "$%s", buf);

    // --- 24h change (right-aligned, same row as the price) ---
    char changeStr[16];
    snprintf(changeStr, sizeof(changeStr), "%+.2f%%", data.change24h);

    uint16_t changeColor = (data.change24h >= 0.0f) ? TFT_GREEN : TFT_RED;
    int      rightX      = content.x + content.w - GRAPH_INSET_X;
    int      rightY      = content.y + 8;

    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(changeColor, TFT_BLACK);
    tft.setTextSize(DETAIL_QUOTE_SIZE);
    tft.setCursor(rightX, rightY);
    tft.print(changeStr);

    // Price right-aligned to the left of the change with a small gap,
    // so both stay on the same row.
    int changeW = tft.textWidth(changeStr);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(DETAIL_PRICE_SIZE);
    tft.setCursor(rightX - changeW - 8, content.y + 4);
    tft.print(priceStr);
    tft.setTextDatum(TL_DATUM);

    // --- Graph ---
    int gx = content.x + GRAPH_INSET_X;
    int gy = content.y + GRAPH_TOP;
    int gw = content.w - GRAPH_INSET_X * 2;
    int gh = content.y + content.h - gy - GRAPH_BOTTOM_MARGIN;

    // Graph border
    tft.drawRect(gx, gy, gw, gh, TFT_DARKGREY);

    if (history.size() < 2) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(DETAIL_QUOTE_SIZE);
        tft.setCursor(gx + GRAPH_PAD, gy + 6);
        tft.print("No history yet");
        Serial.println("Ticker display (no history)");
        return;
    }

    // Scale the samples into the graph rect.
    float minVal = history.min();
    float maxVal = history.max();
    if (maxVal - minVal < 0.0001f) {
        // Flat line: give a small pad so the line renders at mid-height.
        minVal -= 1.0f;
        maxVal += 1.0f;
    }

    float range  = maxVal - minVal;
    int   innerW = gw - GRAPH_PAD * 2;
    int   innerH = gh - GRAPH_PAD * 2;

    // Draw the line graph.
    int gxInner  = gx + GRAPH_PAD;
    int gyBottom = gy + gh - GRAPH_PAD;

    for (size_t i = 1; i < history.size(); i++) {
        float f0 = (history.at(i - 1) - minVal) / range;
        float f1 = (history.at(i) - minVal) / range;

        float a = (i - 1) / (float)(history.size() - 1);
        float b = i / (float)(history.size() - 1);

        int x0 = gxInner + (int)(a * (innerW - 1));
        int x1 = gxInner + (int)(b * (innerW - 1));
        int y0 = gyBottom - (int)(f0 * (innerH - 1));
        int y1 = gyBottom - (int)(f1 * (innerH - 1));

        tft.drawLine(x0, y0, x1, y1, ticker.color);
    }

    Serial.println("Ticker display updated");
}

void draw_api_status(ApiStatus status) {
    uint16_t color;
    switch (status) {
        case ApiStatus::HEALTHY:
            color = TFT_GREEN;
            break;
        case ApiStatus::DEGRADED:
            color = TFT_YELLOW;
            break;
        case ApiStatus::UNAVAILABLE:
        default:
            color = TFT_RED;
            break;
    }

    // Small filled circle in the top-right corner of the header, vertically
    // centred on the header title text.
    constexpr uint8_t STATUS_RADIUS = 4;

    LayoutManager layout(tft.width(), tft.height());
    Rect          header = layout.grid(GRID_COLS, GRID_ROWS, 0, HEADER_ROW, 1, 1);

    // Centre the icon at the same height as the header title text.
    tft.setTextSize(TITLE_TEXT_SIZE);
    int statusY = header.y + HEADER_INSET_Y + tft.fontHeight() / 2;
    int statusX = header.x + header.w - STATUS_RADIUS - HEADER_INSET_X;

    tft.fillCircle(statusX, statusY, STATUS_RADIUS, color);
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

}  // namespace cryptoapp