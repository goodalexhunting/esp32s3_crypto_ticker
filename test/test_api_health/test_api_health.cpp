#include <unity.h>

#include "../../src/api_health.cpp"

using cryptoapp::ApiHealth;
using cryptoapp::ApiStatus;

void test_empty_history_is_unavailable() {
    ApiHealth health;
    TEST_ASSERT_EQUAL(ApiStatus::UNAVAILABLE, health.status());
}

void test_all_successes_are_healthy() {
    ApiHealth health;
    health.recordSuccess();
    health.recordSuccess();
    TEST_ASSERT_EQUAL(ApiStatus::HEALTHY, health.status());
}

void test_mixed_results_are_degraded() {
    ApiHealth health;
    health.recordSuccess();
    health.recordFailure();
    TEST_ASSERT_EQUAL(ApiStatus::DEGRADED, health.status());
}

void test_all_failures_are_unavailable() {
    ApiHealth health;
    health.recordFailure();
    health.recordFailure();
    TEST_ASSERT_EQUAL(ApiStatus::UNAVAILABLE, health.status());
}

void test_rolling_window_evicts_old_failures() {
    ApiHealth health;
    for (size_t i = 0; i < ApiHealth::HISTORY_SIZE; ++i) {
        health.recordFailure();
    }
    TEST_ASSERT_EQUAL(ApiStatus::UNAVAILABLE, health.status());

    for (size_t i = 0; i < ApiHealth::HISTORY_SIZE; ++i) {
        health.recordSuccess();
    }
    TEST_ASSERT_EQUAL(ApiStatus::HEALTHY, health.status());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_history_is_unavailable);
    RUN_TEST(test_all_successes_are_healthy);
    RUN_TEST(test_mixed_results_are_degraded);
    RUN_TEST(test_all_failures_are_unavailable);
    RUN_TEST(test_rolling_window_evicts_old_failures);
    return UNITY_END();
}
