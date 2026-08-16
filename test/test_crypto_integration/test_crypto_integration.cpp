#include <unity.h>

#include <cstring>

#include "../../src/config_mgr.cpp"
#include "../../src/crypto.cpp"
#include "../../src/crypto_utils.cpp"
#include "../../src/history.cpp"
#include "Arduino.h"
#include "config_mgr.h"
#include "crypto.h"
#include "crypto_utils.h"
#include "history.h"
#include "preferences_mock.h"

// ---------------------------------------------------------------------------
// Fake HTTP transport
// ---------------------------------------------------------------------------

class FakeCryptoHttp : public cryptoapp::CryptoHttp {
   public:
    std::string lastUrl;
    std::string body;
    bool        returnOk  = true;
    bool        called    = false;
    int         callCount = 0;

    bool getJson(const String& url, JsonDocument& doc) override {
        called = true;
        callCount++;
        lastUrl = url.c_str();
        if (!returnOk) {
            return false;
        }
        DeserializationError err = deserializeJson(doc, body);
        if (err) {
            return false;
        }
        return true;
    }
};

void setUp() {
    Preferences::clear();
    mockResetMillis();
}

void tearDown() {}

// ---------------------------------------------------------------------------
// fetch_prices via injected transport
// ---------------------------------------------------------------------------

void test_fetch_prices_fetches_built_url_and_parses() {
    FakeCryptoHttp http;
    http.body = R"({
        "bitcoin": {"usd": 61234.5, "usd_24h_change": 2.31},
        "solana": {"usd": 145.2, "usd_24h_change": -1.5}
    })";

    cryptoapp::ConfigManager cfg;
    cfg.begin();  // defaults: BTC, SOL, SUI (all usd)

    cryptoapp::PriceData data[3] = {};
    TEST_ASSERT_TRUE(cryptoapp::fetch_prices(data, 3, cfg, &http));

    // The full builder -> transport path must request the composed URL.
    TEST_ASSERT_TRUE(http.called);
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin,solana,sui&vs_currencies=usd&"
        "include_24hr_change=true",
        http.lastUrl.c_str());

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 61234.5f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.31f, data[0].change24h);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 145.2f, data[1].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.5f, data[1].change24h);
    // Third default ticker (sui) missing from body -> defaults 0.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, data[2].price);
}

void test_fetch_prices_transport_failure_returns_false() {
    FakeCryptoHttp http;
    http.returnOk = false;

    cryptoapp::ConfigManager cfg;
    cfg.begin();

    cryptoapp::PriceData data[3] = {};
    TEST_ASSERT_FALSE(cryptoapp::fetch_prices(data, 3, cfg, &http));
    TEST_ASSERT_TRUE(http.called);
}

void test_fetch_prices_malformed_json_returns_false() {
    FakeCryptoHttp http;
    http.body = "{ not json ]]";

    cryptoapp::ConfigManager cfg;
    cfg.begin();

    cryptoapp::PriceData data[3] = {};
    TEST_ASSERT_FALSE(cryptoapp::fetch_prices(data, 3, cfg, &http));
}

void test_fetch_prices_uses_config_quote_currency() {
    // A config whose first ticker uses usdc.
    Preferences::put("ticker_cfg", "count", "1");
    Preferences::put("ticker_cfg", "t0label", "BTC");
    Preferences::put("ticker_cfg", "t0apiid", "bitcoin");
    Preferences::put("ticker_cfg", "t0quote", "usdc");
    Preferences::put("ticker_cfg", "t0color", "65535");

    FakeCryptoHttp http;
    http.body = R"({"bitcoin": {"usdc": 60000.0, "usdc_24h_change": 1.0}})";

    cryptoapp::ConfigManager cfg;
    cfg.begin();

    cryptoapp::PriceData data[1] = {};
    TEST_ASSERT_TRUE(cryptoapp::fetch_prices(data, 1, cfg, &http));
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usdc&"
        "include_24hr_change=true",
        http.lastUrl.c_str());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 60000.0f, data[0].price);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, data[0].change24h);
}

// ---------------------------------------------------------------------------
// fetch_history via injected transport
// ---------------------------------------------------------------------------

void test_fetch_history_parses_market_chart() {
    FakeCryptoHttp http;
    http.body = R"({"prices": [[1000, 10.0], [2000, 11.0], [3000, 12.0]]})";

    cryptoapp::TickerConfig  ticker{"BTC", "bitcoin", "usd", 0xFFE0};
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::fetch_history(ticker, h, &http));
    TEST_ASSERT_EQUAL_STRING(
        "https://api.coingecko.com/api/v3/coins/bitcoin/market_chart?vs_currency=usd&days=7",
        http.lastUrl.c_str());
    TEST_ASSERT_EQUAL(3u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, h.at(2));
}

void test_fetch_history_empty_prices_returns_false() {
    FakeCryptoHttp http;
    http.body = R"({"prices": []})";

    cryptoapp::TickerConfig  ticker{"BTC", "bitcoin", "usd", 0xFFE0};
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FALSE(cryptoapp::fetch_history(ticker, h, &http));
}

void test_fetch_history_transport_failure_returns_false() {
    FakeCryptoHttp http;
    http.returnOk = false;

    cryptoapp::TickerConfig  ticker{"BTC", "bitcoin", "usd", 0xFFE0};
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FALSE(cryptoapp::fetch_history(ticker, h, &http));
}

void test_fetch_history_large_series_downsampled_through_full_path() {
    // End-to-end confirmation that the injected transport path applies the
    // same downsampling as parseHistoryJson (checked in detail in the unit
    // suite): step = 2, last point appended.
    FakeCryptoHttp http;
    JsonDocument   doc;
    JsonArray      prices = doc["prices"].to<JsonArray>();
    const size_t   n      = HISTORY_POINTS * 2 + 3;
    for (size_t i = 0; i < n; i++) {
        JsonArray p = prices.add<JsonArray>();
        p.add((double)i * 1000.0);
        p.add((double)i);
    }
    serializeJson(doc, http.body);

    cryptoapp::TickerConfig  ticker{"BTC", "bitcoin", "usd", 0xFFE0};
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(cryptoapp::fetch_history(ticker, h, &http));
    // step = 2 yields 146 sampled points, but HistoryBuffer is bounded at
    // HISTORY_POINTS (144): the two oldest are evicted.
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(n - 1), h.at(h.size() - 1));
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_fetch_prices_fetches_built_url_and_parses);
    RUN_TEST(test_fetch_prices_transport_failure_returns_false);
    RUN_TEST(test_fetch_prices_malformed_json_returns_false);
    RUN_TEST(test_fetch_prices_uses_config_quote_currency);

    RUN_TEST(test_fetch_history_parses_market_chart);
    RUN_TEST(test_fetch_history_empty_prices_returns_false);
    RUN_TEST(test_fetch_history_transport_failure_returns_false);
    RUN_TEST(test_fetch_history_large_series_downsampled_through_full_path);

    return UNITY_END();
}