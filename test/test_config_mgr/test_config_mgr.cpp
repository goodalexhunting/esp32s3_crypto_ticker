#include <unity.h>

#include <cstring>

#include "../../src/config_mgr.cpp"
#include "Arduino.h"
#include "config_mgr.h"
#include "preferences_mock.h"


void setUp() {
    Preferences::clear();
    mockResetMillis();
}

void tearDown() {}

// ---------------------------------------------------------------------------
// begin / load
// ---------------------------------------------------------------------------

void test_begin_seeds_defaults_when_empty() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg.count());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("bitcoin", cfg.get(0).apiId.c_str());
    TEST_ASSERT_EQUAL_STRING("usd", cfg.get(0).quote.c_str());
    TEST_ASSERT_EQUAL(0xFFE0, cfg.get(0).color);
}

void test_begin_loads_persisted_config() {
    Preferences::put("ticker_cfg", "count", "2");
    Preferences::put("ticker_cfg", "t0label", "ETH");
    Preferences::put("ticker_cfg", "t0apiid", "ethereum");
    Preferences::put("ticker_cfg", "t0quote", "usd");
    Preferences::put("ticker_cfg", "t0color", "65535");
    Preferences::put("ticker_cfg", "t1label", "DOGE");
    Preferences::put("ticker_cfg", "t1apiid", "dogecoin");
    Preferences::put("ticker_cfg", "t1quote", "usdc");
    Preferences::put("ticker_cfg", "t1color", "0");

    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_EQUAL(2u, cfg.count());
    TEST_ASSERT_EQUAL_STRING("ETH", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("ethereum", cfg.get(0).apiId.c_str());
    TEST_ASSERT_EQUAL_STRING("usd", cfg.get(0).quote.c_str());
    TEST_ASSERT_EQUAL(65535u, cfg.get(0).color);
    TEST_ASSERT_EQUAL_STRING("DOGE", cfg.get(1).label.c_str());
    TEST_ASSERT_EQUAL_STRING("dogecoin", cfg.get(1).apiId.c_str());
    TEST_ASSERT_EQUAL_STRING("usdc", cfg.get(1).quote.c_str());
    TEST_ASSERT_EQUAL(0u, cfg.get(1).color);
}

void test_begin_invalid_count_seeds_defaults() {
    Preferences::put("ticker_cfg", "count", "99");
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg.count());
}

void test_begin_incomplete_ticker_seeds_defaults() {
    Preferences::put("ticker_cfg", "count", "1");
    Preferences::put("ticker_cfg", "t0label", "ETH");
    Preferences::put("ticker_cfg", "t0apiid", "");
    Preferences::put("ticker_cfg", "t0quote", "usd");

    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg.count());
}

void test_begin_zero_count_seeds_defaults() {
    Preferences::put("ticker_cfg", "count", "0");
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg.count());
}

// ---------------------------------------------------------------------------
// add
// ---------------------------------------------------------------------------

void test_add_success_and_revision_bump() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    uint32_t rev = cfg.revision();

    TEST_ASSERT_TRUE(cfg.add("  ETH  ", "  ethereum  ", " usd ", 0x07E0));
    TEST_ASSERT_EQUAL(rev + 1, cfg.revision());
    TEST_ASSERT_EQUAL(4u, cfg.count());
    TEST_ASSERT_EQUAL_STRING("ETH", cfg.get(3).label.c_str());
    TEST_ASSERT_EQUAL_STRING("ethereum", cfg.get(3).apiId.c_str());
    TEST_ASSERT_EQUAL_STRING("usd", cfg.get(3).quote.c_str());
    TEST_ASSERT_EQUAL(0x07E0, cfg.get(3).color);
}

void test_add_rejects_empty_fields() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    size_t before = cfg.count();
    TEST_ASSERT_FALSE(cfg.add("", "ethereum", "usd", 0xFFFF));
    TEST_ASSERT_FALSE(cfg.add("ETH", "", "usd", 0xFFFF));
    TEST_ASSERT_FALSE(cfg.add("ETH", "ethereum", "", 0xFFFF));
    TEST_ASSERT_FALSE(cfg.add("   ", "ethereum", "usd", 0xFFFF));
    TEST_ASSERT_EQUAL(before, cfg.count());
}

void test_add_rejects_duplicate_case_insensitive() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    size_t before = cfg.count();
    TEST_ASSERT_FALSE(cfg.add("Bitcoin", "BITCOIN", "USD", 0xFFFF));
    TEST_ASSERT_EQUAL(before, cfg.count());
}

void test_add_rejects_when_full() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    cfg.add("A", "a", "usd", 0);
    cfg.add("B", "b", "usd", 0);
    cfg.add("C", "c", "usd", 0);
    cfg.add("D", "d", "usd", 0);
    cfg.add("E", "e", "usd", 0);
    TEST_ASSERT_EQUAL(MAX_TICKERS, cfg.count());
    TEST_ASSERT_FALSE(cfg.add("F", "f", "usd", 0));
}

void test_add_truncates_long_fields() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    std::string longLabel("L");
    longLabel.append(40, 'x');
    std::string longApi("A");
    longApi.append(50, 'y');
    std::string longQuote("Q");
    longQuote.append(20, 'z');

    TEST_ASSERT_TRUE(cfg.add(
        String(longLabel.c_str()), String(longApi.c_str()), String(longQuote.c_str()), 0xFFFF));
    const cryptoapp::TickerConfig& t = cfg.get(cfg.count() - 1);
    TEST_ASSERT_EQUAL(16u, t.label.length());
    TEST_ASSERT_EQUAL(32u, t.apiId.length());
    TEST_ASSERT_EQUAL(8u, t.quote.length());
}

// ---------------------------------------------------------------------------
// remove
// ---------------------------------------------------------------------------

void test_remove_valid_and_invalid() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    size_t before = cfg.count();

    TEST_ASSERT_TRUE(cfg.remove(1));
    TEST_ASSERT_EQUAL(before - 1, cfg.count());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SUI", cfg.get(1).label.c_str());

    TEST_ASSERT_FALSE(cfg.remove(cfg.count()));
    TEST_ASSERT_FALSE(cfg.remove(999));
}

void test_remove_last_element() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    cfg.remove(2);
    cfg.remove(1);
    TEST_ASSERT_EQUAL(1u, cfg.count());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(0).label.c_str());
    TEST_ASSERT_TRUE(cfg.remove(0));
    TEST_ASSERT_EQUAL(0u, cfg.count());
    TEST_ASSERT_FALSE(cfg.remove(0));
}

// ---------------------------------------------------------------------------
// move
// ---------------------------------------------------------------------------

void test_move_forward() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_TRUE(cfg.move(0, 2));
    TEST_ASSERT_EQUAL_STRING("SOL", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SUI", cfg.get(1).label.c_str());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(2).label.c_str());
}

void test_move_backward() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_TRUE(cfg.move(2, 0));
    TEST_ASSERT_EQUAL_STRING("SUI", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(1).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SOL", cfg.get(2).label.c_str());
}

void test_move_invalid() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    TEST_ASSERT_FALSE(cfg.move(0, 0));
    TEST_ASSERT_FALSE(cfg.move(0, 99));
    TEST_ASSERT_FALSE(cfg.move(99, 0));
    TEST_ASSERT_FALSE(cfg.move(0, cfg.count()));
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SOL", cfg.get(1).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SUI", cfg.get(2).label.c_str());
}

// ---------------------------------------------------------------------------
// save / load roundtrip
// ---------------------------------------------------------------------------

void test_save_and_reload_roundtrip() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    cfg.add("ETH", "ethereum", "usd", 0x07E0);
    cfg.remove(0);
    cfg.save();

    cryptoapp::ConfigManager cfg2;
    cfg2.begin();
    TEST_ASSERT_EQUAL(cfg.count(), cfg2.count());
    for (size_t i = 0; i < cfg2.count(); i++) {
        TEST_ASSERT_EQUAL_STRING(cfg.get(i).label.c_str(), cfg2.get(i).label.c_str());
        TEST_ASSERT_EQUAL_STRING(cfg.get(i).apiId.c_str(), cfg2.get(i).apiId.c_str());
        TEST_ASSERT_EQUAL_STRING(cfg.get(i).quote.c_str(), cfg2.get(i).quote.c_str());
        TEST_ASSERT_EQUAL(cfg.get(i).color, cfg2.get(i).color);
    }
}

void test_save_removes_stale_entries_beyond_count() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    Preferences::put("ticker_cfg", "t4label", "STALE");
    Preferences::put("ticker_cfg", "t4apiid", "stale");
    Preferences::put("ticker_cfg", "t4quote", "usd");
    Preferences::put("ticker_cfg", "t4color", "1");

    cfg.save();
    TEST_ASSERT_EQUAL_STRING("", Preferences::get("ticker_cfg", "t4label").c_str());
    TEST_ASSERT_EQUAL_STRING("", Preferences::get("ticker_cfg", "t4apiid").c_str());
}

// ---------------------------------------------------------------------------
// resetToDefaults
// ---------------------------------------------------------------------------

void test_reset_to_defaults() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    cfg.add("ETH", "ethereum", "usd", 0x07E0);
    cfg.remove(0);
    cfg.resetToDefaults();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg.count());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg.get(0).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SOL", cfg.get(1).label.c_str());
    TEST_ASSERT_EQUAL_STRING("SUI", cfg.get(2).label.c_str());

    cryptoapp::ConfigManager cfg2;
    cfg2.begin();
    TEST_ASSERT_EQUAL(NUM_DEFAULT_TICKERS, cfg2.count());
    TEST_ASSERT_EQUAL_STRING("BTC", cfg2.get(0).label.c_str());
}

// ---------------------------------------------------------------------------
// revision
// ---------------------------------------------------------------------------

void test_revision_bumps_on_mutations_only() {
    cryptoapp::ConfigManager cfg;
    cfg.begin();
    uint32_t base = cfg.revision();
    cfg.add("ETH", "ethereum", "usd", 0);
    TEST_ASSERT_EQUAL(base + 1, cfg.revision());
    cfg.remove(0);
    TEST_ASSERT_EQUAL(base + 2, cfg.revision());
    cfg.move(0, 1);
    TEST_ASSERT_EQUAL(base + 3, cfg.revision());
    cfg.resetToDefaults();
    TEST_ASSERT_EQUAL(base + 4, cfg.revision());

    TEST_ASSERT_FALSE(cfg.add("BTC", "bitcoin", "usd", 0));
    TEST_ASSERT_EQUAL(base + 4, cfg.revision());
    TEST_ASSERT_FALSE(cfg.remove(99));
    TEST_ASSERT_EQUAL(base + 4, cfg.revision());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_begin_seeds_defaults_when_empty);
    RUN_TEST(test_begin_loads_persisted_config);
    RUN_TEST(test_begin_invalid_count_seeds_defaults);
    RUN_TEST(test_begin_incomplete_ticker_seeds_defaults);
    RUN_TEST(test_begin_zero_count_seeds_defaults);

    RUN_TEST(test_add_success_and_revision_bump);
    RUN_TEST(test_add_rejects_empty_fields);
    RUN_TEST(test_add_rejects_duplicate_case_insensitive);
    RUN_TEST(test_add_rejects_when_full);
    RUN_TEST(test_add_truncates_long_fields);

    RUN_TEST(test_remove_valid_and_invalid);
    RUN_TEST(test_remove_last_element);

    RUN_TEST(test_move_forward);
    RUN_TEST(test_move_backward);
    RUN_TEST(test_move_invalid);

    RUN_TEST(test_save_and_reload_roundtrip);
    RUN_TEST(test_save_removes_stale_entries_beyond_count);
    RUN_TEST(test_reset_to_defaults);
    RUN_TEST(test_revision_bumps_on_mutations_only);

    return UNITY_END();
}