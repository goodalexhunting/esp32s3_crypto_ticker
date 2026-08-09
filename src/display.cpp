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
constexpr uint8_t DETAIL_TEXT_SIZE    = 2;  // shared by all three detail columns

// Detail-view subtitle frame (px): a bordered panel split into three
// fixed compartments: | pair | 24h change | price |.
constexpr uint8_t DETAIL_FRAME_TOP    = 6;    // offset from the content area top
constexpr uint8_t DETAIL_FRAME_H      = 30;   // panel height
constexpr uint8_t DETAIL_FRAME_PAD    = 8;    // inner padding from the panel edges
constexpr uint8_t DETAIL_COL_PAIR_W   = 112;  // 1st compartment: ticker pair
constexpr uint8_t DETAIL_COL_CHANGE_W = 92;   // 2nd compartment: 24h change
// 3rd compartment (price) takes the remaining frame width.

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

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------

void formatPrice(float value, char* buf, size_t len) {
    if (value >= 1000.0f) {
        dtostrf(value, 1, 0, buf);
    } else if (value >= 1.0f) {
        dtostrf(value, 1, 2, buf);
    } else {
        dtostrf(value, 1, 3, buf);
    }
}

// Format a price as a "$"-prefixed string (shared by the table and the
// ticker detail view).
void formatPriceDollar(float value, char* buf, size_t len) {
    char raw[16];
    formatPrice(value, raw, sizeof(raw));
    snprintf(buf, len, "$%s", raw);
}

// ---------------------------------------------------------------------------
// Layout helpers
// ---------------------------------------------------------------------------

Rect contentArea() {
    LayoutManager layout(tft.width(), tft.height());
    return layout.grid(GRID_COLS, GRID_ROWS, 0, CONTENT_ROW, 1, CONTENT_ROWSPAN);
}

// Blank the content area and return its bounds.
Rect blankContentArea() {
    Rect content = contentArea();
    tft.fillRect(content.x, content.y, content.w, content.h, TFT_BLACK);
    return content;
}

// Redraw the static chrome (header/footer), then blank the content area.
Rect renderContentFrame() {
    render_layout(tft);
    return blankContentArea();
}

// Y offset that vertically centres the currently configured font size
// inside a box of the given height.
int centeredTextY(int boxY, int boxH) {
    return boxY + (boxH - tft.fontHeight()) / 2;
}

// ---------------------------------------------------------------------------
// Coin table
// ---------------------------------------------------------------------------

void drawTableFrame(const Rect& table, int colSplit) {
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
}

void drawCoinRow(
    const TickerConfig& ticker, const Rect& table, int colSplit, int rowY, const char* priceStr) {
    // Symbol (brand color)
    tft.setTextColor(ticker.color, TFT_BLACK);
    tft.setCursor(table.x + TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
    tft.print(ticker.label);

    // Price (right-aligned in the price column)
    int priceW = tft.textWidth(priceStr);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(colSplit + table.w / 2 - priceW - TEXT_OFFSET_X, rowY + TEXT_OFFSET_Y);
    tft.print(priceStr);
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

    drawTableFrame(table, colSplit);

    // Coin rows
    for (size_t i = 0; i < numCoins; i++) {
        char priceStr[20];
        formatPriceDollar(coinData[i].price, priceStr, sizeof(priceStr));

        int rowY = table.y + HEADER_H + i * rowH;
        drawCoinRow(config.get(i), table, colSplit, rowY, priceStr);
    }
}

// ---------------------------------------------------------------------------
// Ticker detail view
// ---------------------------------------------------------------------------

void drawTickerPair(const TickerConfig& ticker, int frameX, int frameY, int frameH) {
    tft.setTextColor(ticker.color, TFT_BLACK);

    int pairX = frameX + DETAIL_FRAME_PAD;
    int pairY = centeredTextY(frameY, frameH);
    tft.setCursor(pairX, pairY);
    tft.print(ticker.label);

    // Quote currency immediately after the label (dimmed).
    int labelW = tft.textWidth(ticker.label);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.setCursor(pairX + labelW + 6, pairY);
    tft.print("/ ");
    tft.print(ticker.quote);
}

void drawTickerChange(const PriceData& data, int col2X, int frameY, int frameH) {
    char changeStr[16];
    snprintf(changeStr, sizeof(changeStr), "%+.2f%%", data.change24h);

    uint16_t changeColor = (data.change24h >= 0.0f) ? TFT_GREEN : TFT_RED;
    tft.setTextColor(changeColor, TFT_BLACK);
    int changeY = centeredTextY(frameY, frameH);
    tft.setCursor(col2X + DETAIL_FRAME_PAD, changeY);
    tft.print(changeStr);
}

void drawTickerPrice(const PriceData& data, int col3X, int frameY, int frameH) {
    char priceStr[24];
    formatPriceDollar(data.price, priceStr, sizeof(priceStr));

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int priceY = centeredTextY(frameY, frameH);
    tft.setCursor(col3X + DETAIL_FRAME_PAD, priceY);
    tft.print(priceStr);
}

// Subtitle frame: a bordered panel split into three fixed compartments:
// | pair | 24h change | price |.
void drawTickerFrame(const Rect& content, const TickerConfig& ticker, const PriceData& data) {
    int frameX = content.x + GRAPH_INSET_X;
    int frameY = content.y + DETAIL_FRAME_TOP;
    int frameW = content.w - GRAPH_INSET_X * 2;
    int frameH = DETAIL_FRAME_H;

    tft.drawRect(frameX, frameY, frameW, frameH, TFT_DARKGREY);

    // Fixed column boundaries.
    int col2X = frameX + DETAIL_COL_PAIR_W;
    int col3X = col2X + DETAIL_COL_CHANGE_W;

    // Vertical dividers between the compartments.
    tft.drawLine(col2X, frameY, col2X, frameY + frameH, TFT_DARKGREY);
    tft.drawLine(col3X, frameY, col3X, frameY + frameH, TFT_DARKGREY);

    // All three columns share the same text size and left alignment.
    tft.setTextSize(DETAIL_TEXT_SIZE);
    tft.setTextDatum(TL_DATUM);

    drawTickerPair(ticker, frameX, frameY, frameH);
    drawTickerChange(data, col2X, frameY, frameH);
    drawTickerPrice(data, col3X, frameY, frameH);
}

// Map a history index to the graph's X pixel coordinate.
int scaledX(size_t index, size_t count, int gxInner, int innerW) {
    float t = index / (float)(count - 1);
    return gxInner + (int)(t * (innerW - 1));
}

// Map a price sample to the graph's Y pixel coordinate.
int scaledY(float value, float minVal, float range, int gyBottom, int innerH) {
    float f = (value - minVal) / range;
    return gyBottom - (int)(f * (innerH - 1));
}

void drawPriceGraph(const Rect& content, const TickerConfig& ticker, const HistoryBuffer& history) {
    int gx = content.x + GRAPH_INSET_X;
    int gy = content.y + GRAPH_TOP;
    int gw = content.w - GRAPH_INSET_X * 2;
    int gh = content.y + content.h - gy - GRAPH_BOTTOM_MARGIN;

    // Graph border
    tft.drawRect(gx, gy, gw, gh, TFT_DARKGREY);

    if (history.size() < 2) {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.setTextSize(BODY_TEXT_SIZE);
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
        int x0 = scaledX(i - 1, history.size(), gxInner, innerW);
        int x1 = scaledX(i, history.size(), gxInner, innerW);
        int y0 = scaledY(history.at(i - 1), minVal, range, gyBottom, innerH);
        int y1 = scaledY(history.at(i), minVal, range, gyBottom, innerH);

        tft.drawLine(x0, y0, x1, y1, ticker.color);
    }

    Serial.println("Ticker display updated");
}

// ---------------------------------------------------------------------------
// Price storage
// ---------------------------------------------------------------------------

void storePrices(const PriceData* data, size_t count, const ConfigManager& config) {
    size_t n = count < config.count() ? count : config.count();
    for (size_t i = 0; i < n; i++) {
        coinData[i] = data[i];
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

    // Footer (single line: attribution | FW version).
    display.fillRect(footer.x, footer.y, footer.w, footer.h, TFT_BLACK);
    display.setTextDatum(TL_DATUM);
    display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    display.setTextSize(BODY_TEXT_SIZE);
    display.setCursor(footer.x + FOOTER_INSET_X, footer.y + FOOTER_INSET_Y);
    display.print("By github.com/goodalexhunting | FW ");
    display.print(FW_VERSION);
}

void update_prices_display(const PriceData* data, size_t count, const ConfigManager& config) {
    storePrices(data, count, config);

    Rect content = renderContentFrame();
    drawCoinTable(content, config);
    Serial.println("Display updated");
}

void update_ticker_display(const TickerConfig&  ticker,
                           const PriceData&     data,
                           const HistoryBuffer& history) {
    Rect content = renderContentFrame();

    drawTickerFrame(content, ticker, data);
    drawPriceGraph(content, ticker, history);
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
    Rect content = blankContentArea();
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextSize(BODY_TEXT_SIZE);
    tft.setTextDatum(TL_DATUM);
    tft.setCursor(content.x + MESSAGE_INSET, content.y + MESSAGE_INSET);
    tft.print(msg);
}

}  // namespace cryptoapp
