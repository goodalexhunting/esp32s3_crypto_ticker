#include <unity.h>

#include "../../src/layout_manager.cpp"

void test_grid_calculates_cell_and_span_geometry() {
    LayoutManager layout(320, 180);
    Rect          actual = layout.grid(4, 3, 1, 1, 2, 2);

    TEST_ASSERT_EQUAL_INT(80, actual.x);
    TEST_ASSERT_EQUAL_INT(60, actual.y);
    TEST_ASSERT_EQUAL_INT(160, actual.w);
    TEST_ASSERT_EQUAL_INT(120, actual.h);
}

void test_inset_reduces_size_and_moves_origin() {
    LayoutManager layout(320, 180);
    Rect          actual = layout.inset({10, 20, 100, 80}, 5);

    TEST_ASSERT_EQUAL_INT(15, actual.x);
    TEST_ASSERT_EQUAL_INT(25, actual.y);
    TEST_ASSERT_EQUAL_INT(90, actual.w);
    TEST_ASSERT_EQUAL_INT(70, actual.h);
}

void test_safe_area_insets_the_full_screen() {
    LayoutManager layout(320, 180);
    Rect          actual = layout.safeArea(8);

    TEST_ASSERT_EQUAL_INT(8, actual.x);
    TEST_ASSERT_EQUAL_INT(8, actual.y);
    TEST_ASSERT_EQUAL_INT(304, actual.w);
    TEST_ASSERT_EQUAL_INT(164, actual.h);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_grid_calculates_cell_and_span_geometry);
    RUN_TEST(test_inset_reduces_size_and_moves_origin);
    RUN_TEST(test_safe_area_insets_the_full_screen);
    return UNITY_END();
}
