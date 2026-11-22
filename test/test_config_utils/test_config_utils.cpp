#include <unity.h>

#include "../../src/config_utils.cpp"

void test_parseTickerIndex_accepts_decimal_index() {
    size_t index = 99;
    TEST_ASSERT_TRUE(cryptoapp::parseTickerIndex("7", index));
    TEST_ASSERT_EQUAL_UINT(7, index);
}

void test_parseTickerIndex_accepts_leading_zeroes() {
    size_t index = 99;
    TEST_ASSERT_TRUE(cryptoapp::parseTickerIndex("0003", index));
    TEST_ASSERT_EQUAL_UINT(3, index);
}

void test_parseTickerIndex_rejects_empty_and_non_decimal_values() {
    size_t index = 99;
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex("", index));
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex("abc", index));
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex("0x2", index));
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex("-1", index));
    TEST_ASSERT_EQUAL_UINT(99, index);
}

void test_parseTickerIndex_rejects_out_of_capacity_index() {
    size_t index = 99;
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex("8", index));
    TEST_ASSERT_EQUAL_UINT(99, index);
}

void test_parseTickerIndex_rejects_null_input() {
    size_t index = 99;
    TEST_ASSERT_FALSE(cryptoapp::parseTickerIndex(nullptr, index));
    TEST_ASSERT_EQUAL_UINT(99, index);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parseTickerIndex_accepts_decimal_index);
    RUN_TEST(test_parseTickerIndex_accepts_leading_zeroes);
    RUN_TEST(test_parseTickerIndex_rejects_empty_and_non_decimal_values);
    RUN_TEST(test_parseTickerIndex_rejects_out_of_capacity_index);
    RUN_TEST(test_parseTickerIndex_rejects_null_input);
    return UNITY_END();
}
