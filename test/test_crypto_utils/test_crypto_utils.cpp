#include <unity.h>

#include "../../src/crypto_utils.cpp"
#include "../../src/history.cpp"
#include "Arduino.h"
#include "crypto_utils.h"

void setUp() {}

void tearDown() {}

// ---------------------------------------------------------------------------
// buildUrl
// ---------------------------------------------------------------------------

void test_buildUrl_single_ticker() {
    cryptoapp::TickerConfig t{"BTC", "bitcoin", "usd", 0xFFE0};
    String                  url;
    cryptoapp::buildUrl(url, &t, 1);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd&include_24hr_"
        "change=true",
        url.c_str());
}

void test_buildUrl_multiple_tickers() {
    cryptoapp::TickerConfig tickers[2] = {{"BTC", "bitcoin", "usd", 0xFFE0},
                                          {"SOL", "solana", "usd", 0xF81F}};
    String                  url;
    cryptoapp::buildUrl(url, tickers, 2);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,solana&vs_currencies=usd&"
        "include_24hr_change=true",
        url.c_str());
}

void test_buildUrl_quote_currency_from_first() {
    cryptoapp::TickerConfig tickers[2] = {{"BTC", "bitcoin", "usdc", 0xFFE0},
                                          {"SOL", "solana", "usd", 0xF81F}};
    String                  url;
    cryptoapp::buildUrl(url, tickers, 2);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,solana&vs_currencies=usdc&"
        "include_24hr_change=true",
        url.c_str());
}

void test_buildUrl_empty_config_falls_back_to_usd() {
    String url;
    cryptoapp::buildUrl(url, nullptr, 0);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=&vs_currencies=usd&include_24hr_change="
        "true",
        url.c_str());
}

// ---------------------------------------------------------------------------
// buildMarketChartUrl
// ---------------------------------------------------------------------------

void test_build_market_chart_url() {
    cryptoapp::TickerConfig t{"BTC", "bitcoin", "usd", 0xFFE0};
    String                  url = cryptoapp::buildMarketChartUrl(t);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart?vs_currency=usd&days=7",
        url.c_str());
}

void test_build_market_chart_url_other_quote() {
    cryptoapp::TickerConfig t{"ETH", "ethereum", "usdc", 0xFFFF};
    String                  url = cryptoapp::buildMarketChartUrl(t);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/coins/ethereum/market_chart?vs_currency=usdc&days=7",
        url.c_str());
}

// ---------------------------------------------------------------------------
// parsePricesJson
// ---------------------------------------------------------------------------

void test_parse_prices_normal() {
    JsonDocument doc;
    deserializeJson(doc, R"({
        "bitcoin": {"usd": 61234.5, "usd_24h_change": 2.31},
        "solana": {"usd": 145.2, "usd_24h_change": -1.5}
    })");

    cryptoapp::TickerConfig tickers[2] = {{"BTC", "bitcoin", "usd", 0xFFE0},
                                          {"SOL", "solana", "usd", 0xF81F}};
    cryptoapp::PriceData    data[2]    = {};
    TEST_ASSERT_TRUE(cryptoapp::parsePricesJson(doc, data, 2, tickers, 2));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 61234.5f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.31f, data[0].change24h);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 145.2f, data[1].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, data[1].change24h);
}

void test_parse_prices_missing_coin_defaults_zero() {
    JsonDocument doc;
    deserializeJson(doc, R"({"bitcoin": {"usd": 1.0, "usd_24h_change": 0.5}})");

    cryptoapp::TickerConfig tickers[2] = {{"BTC", "bitcoin", "usd", 0xFFE0},
                                          {"SOL", "solana", "usd", 0xF81F}};
    cryptoapp::PriceData    data[2]    = {{99.0f, 99.0f}, {99.0f, 99.0f}};
    TEST_ASSERT_TRUE(cryptoapp::parsePricesJson(doc, data, 2, tickers, 2));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, data[0].change24h);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[1].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[1].change24h);
}

void test_parse_prices_missing_change_field_defaults_zero() {
    JsonDocument doc;
    deserializeJson(doc, R"({"bitcoin": {"usd": 5.0}})");

    cryptoapp::TickerConfig tickers[1] = {{"BTC", "bitcoin", "usd", 0xFFE0}};
    cryptoapp::PriceData    data[1]    = {};
    TEST_ASSERT_TRUE(cryptoapp::parsePricesJson(doc, data, 1, tickers, 1));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[0].change24h);
}

void test_parse_prices_quote_not_object() {
    JsonDocument doc;
    deserializeJson(doc, R"({"bitcoin": "not-an-object"})");

    cryptoapp::TickerConfig tickers[1] = {{"BTC", "bitcoin", "usd", 0xFFE0}};
    cryptoapp::PriceData    data[1]    = {};
    TEST_ASSERT_TRUE(cryptoapp::parsePricesJson(doc, data, 1, tickers, 1));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[0].change24h);
}

void test_parse_prices_writes_at_most_min_count() {
    JsonDocument doc;
    deserializeJson(doc, R"({
        "bitcoin": {"usd": 1.0, "usd_24h_change": 0.1},
        "solana": {"usd": 2.0, "usd_24h_change": 0.2},
        "sui": {"usd": 3.0, "usd_24h_change": 0.3}
    })");

    cryptoapp::TickerConfig tickers[3] = {{"BTC", "bitcoin", "usd", 0xFFE0},
                                          {"SOL", "solana", "usd", 0xF81F},
                                          {"SUI", "sui", "usd", 0x07FF}};
    cryptoapp::PriceData    data[2]    = {};
    TEST_ASSERT_TRUE(cryptoapp::parsePricesJson(doc, data, 2, tickers, 3));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, data[1].price);
}

// ---------------------------------------------------------------------------
// parseHistoryJson
// ---------------------------------------------------------------------------

void test_parse_history_downsample_fewer_than_limit() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": [[1000, 1.0], [2000, 2.0], [3000, 3.0]]})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    TEST_ASSERT_EQUAL(3u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, h.at(2));
}

void test_parse_history_downsample_to_limit() {
    JsonDocument doc;
    JsonArray    prices = doc["prices"].to<JsonArray>();
    for (size_t i = 0; i < HISTORY_POINTS; i++) {
        JsonArray p = prices.add<JsonArray>();
        p.add((double)i * 1000.0);
        p.add((double)i);
    }
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(HISTORY_POINTS - 1), h.at(h.size() - 1));
}

void test_parse_history_downsample_over_limit_keeps_last() {
    JsonDocument doc;
    JsonArray    prices = doc["prices"].to<JsonArray>();
    const size_t n      = HISTORY_POINTS + 1;  // step = 1, all pushed
    for (size_t i = 0; i < n; i++) {
        JsonArray p = prices.add<JsonArray>();
        p.add((double)i * 1000.0);
        p.add((double)i);
    }
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    // 145 pushes, but the ring buffer caps at HISTORY_POINTS: the oldest
    // (0.0) is evicted, so size is 144 and the newest (n-1) is retained.
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(n - 1), h.at(h.size() - 1));
}

void test_parse_history_downsample_large_keeps_every_nth_and_last() {
    JsonDocument doc;
    JsonArray    prices = doc["prices"].to<JsonArray>();
    const size_t n      = HISTORY_POINTS * 2 + 3;  // step = 2
    for (size_t i = 0; i < n; i++) {
        JsonArray p = prices.add<JsonArray>();
        p.add((double)i * 1000.0);
        p.add((double)i);
    }
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    // step = n / HISTORY_POINTS = 2 -> sampled indices 0, 2, 4, ..., 290
    // (146 values; (n-1)%2 == 0 so no separate last append). The ring
    // buffer caps at HISTORY_POINTS, evicting the two oldest samples
    // (0.0 and 2.0), so the first retained value is 4.0 and the newest is
    // still n-1 (290.0).
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(n - 1), h.at(h.size() - 1));
}

void test_parse_history_single_point() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": [[1000, 42.0]]})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    TEST_ASSERT_EQUAL(1u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, h.back());
}

void test_parse_history_bad_points_skipped() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": [[1000], [2000, 5.0], "bad", [3000, 6.0]]})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    TEST_ASSERT_EQUAL(2u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, h.at(0));
}

void test_parse_history_missing_prices_returns_false() {
    JsonDocument doc;
    deserializeJson(doc, R"({"other": 1})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FALSE(cryptoapp::parseHistoryJson(doc, h));
}

void test_parse_history_empty_prices_returns_false() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": []})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FALSE(cryptoapp::parseHistoryJson(doc, h));
}

void test_parse_history_non_array_returns_false() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": "nope"})");
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FALSE(cryptoapp::parseHistoryJson(doc, h));
}

void test_parse_history_resets_buffer() {
    JsonDocument doc;
    deserializeJson(doc, R"({"prices": [[1000, 1.0], [2000, 2.0]]})");
    cryptoapp::HistoryBuffer h;
    h.push(99.0f);
    TEST_ASSERT_TRUE(cryptoapp::parseHistoryJson(doc, h));
    TEST_ASSERT_EQUAL(2u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.at(0));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_buildUrl_single_ticker);
    RUN_TEST(test_buildUrl_multiple_tickers);
    RUN_TEST(test_buildUrl_quote_currency_from_first);
    RUN_TEST(test_buildUrl_empty_config_falls_back_to_usd);

    RUN_TEST(test_build_market_chart_url);
    RUN_TEST(test_build_market_chart_url_other_quote);

    RUN_TEST(test_parse_prices_normal);
    RUN_TEST(test_parse_prices_missing_coin_defaults_zero);
    RUN_TEST(test_parse_prices_missing_change_field_defaults_zero);
    RUN_TEST(test_parse_prices_quote_not_object);
    RUN_TEST(test_parse_prices_writes_at_most_min_count);

    RUN_TEST(test_parse_history_downsample_fewer_than_limit);
    RUN_TEST(test_parse_history_downsample_to_limit);
    RUN_TEST(test_parse_history_downsample_over_limit_keeps_last);
    RUN_TEST(test_parse_history_downsample_large_keeps_every_nth_and_last);
    RUN_TEST(test_parse_history_single_point);
    RUN_TEST(test_parse_history_bad_points_skipped);
    RUN_TEST(test_parse_history_missing_prices_returns_false);
    RUN_TEST(test_parse_history_empty_prices_returns_false);
    RUN_TEST(test_parse_history_non_array_returns_false);
    RUN_TEST(test_parse_history_resets_buffer);

    return UNITY_END();
}