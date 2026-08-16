#include <unity.h>

#include "../../src/history.cpp"
#include "history.h"

void setUp() {}

void tearDown() {}

// ---------------------------------------------------------------------------
// push / size
// ---------------------------------------------------------------------------

void test_push_and_size() {
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_TRUE(h.empty());
    TEST_ASSERT_EQUAL(0u, h.size());

    h.push(1.0f);
    h.push(2.0f);
    h.push(3.0f);
    TEST_ASSERT_FALSE(h.empty());
    TEST_ASSERT_EQUAL(3u, h.size());
}

void test_push_evicts_oldest_when_full() {
    cryptoapp::HistoryBuffer h;
    for (size_t i = 0; i < HISTORY_POINTS; i++) {
        h.push((float)i);
    }
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h.front());

    // Overwriting the last slot drops the oldest (0.0) and appends.
    h.push(12345.0f);
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.front());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 12345.0f, h.back());
}

void test_push_wraps_head_after_full_cycle() {
    cryptoapp::HistoryBuffer h;
    const size_t             total = HISTORY_POINTS * 2 + 5;
    for (size_t i = 0; i < total; i++) {
        h.push((float)i);
    }
    // Logical order must be oldest..newest despite the wrapped head.
    TEST_ASSERT_EQUAL(HISTORY_POINTS, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(total - HISTORY_POINTS), h.front());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(total - 1), h.back());

    // Every element is the expected increasing sequence.
    for (size_t i = 0; i < HISTORY_POINTS; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.001f, (float)(total - HISTORY_POINTS + i), h.at(i));
    }
}

// ---------------------------------------------------------------------------
// reset
// ---------------------------------------------------------------------------

void test_reset_clears() {
    cryptoapp::HistoryBuffer h;
    h.push(1.0f);
    h.push(2.0f);
    h.reset();
    TEST_ASSERT_TRUE(h.empty());
    TEST_ASSERT_EQUAL(0u, h.size());
    // Can push again after reset.
    h.push(3.0f);
    TEST_ASSERT_EQUAL(1u, h.size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, h.front());
}

// ---------------------------------------------------------------------------
// front / back / at
// ---------------------------------------------------------------------------

void test_front_back_at_after_wrap() {
    cryptoapp::HistoryBuffer h;
    // Fill fully so head wraps on the next push.
    for (size_t i = 0; i < HISTORY_POINTS; i++) {
        h.push((float)i);
    }
    h.push(100.0f);  // evicts 0.0; head now at index 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.front());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, h.back());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.at(0));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, h.at(h.size() - 1));
}

// ---------------------------------------------------------------------------
// min / max
// ---------------------------------------------------------------------------

void test_min_max_empty_returns_zero() {
    cryptoapp::HistoryBuffer h;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h.min());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, h.max());
}

void test_min_max_basic() {
    cryptoapp::HistoryBuffer h;
    h.push(3.0f);
    h.push(1.0f);
    h.push(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, h.min());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, h.max());
}

void test_min_max_after_wrap() {
    cryptoapp::HistoryBuffer h;
    // Fill fully, then push values that wrap the head.
    for (size_t i = 0; i < HISTORY_POINTS; i++) {
        h.push((float)i);
    }
    // Values 0..143 -> min 0, max 143. Now push 200 and -5 to wrap.
    h.push(200.0f);  // evicts 0
    h.push(-5.0f);   // evicts 1
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -5.0f, h.min());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 200.0f, h.max());
}

void test_min_max_single_element() {
    cryptoapp::HistoryBuffer h;
    h.push(42.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, h.min());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, h.max());
}

int main(int, char**) {
    UNITY_BEGIN();

    RUN_TEST(test_push_and_size);
    RUN_TEST(test_push_evicts_oldest_when_full);
    RUN_TEST(test_push_wraps_head_after_full_cycle);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_front_back_at_after_wrap);
    RUN_TEST(test_min_max_empty_returns_zero);
    RUN_TEST(test_min_max_basic);
    RUN_TEST(test_min_max_after_wrap);
    RUN_TEST(test_min_max_single_element);

    return UNITY_END();
}