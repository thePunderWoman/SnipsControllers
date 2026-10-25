#include <unity.h>

#include "brightness_level.h"

void setUp(void) {}
void tearDown(void) {}

void test_sanitize_keeps_every_valid_level() {
  for (int percent = 10; percent <= 100; percent += 10) {
    TEST_ASSERT_EQUAL(percent, BrightnessLevel::sanitize(percent));
  }
}

void test_sanitize_clamps_out_of_range_values() {
  TEST_ASSERT_EQUAL(10, BrightnessLevel::sanitize(0));
  TEST_ASSERT_EQUAL(10, BrightnessLevel::sanitize(-50));
  TEST_ASSERT_EQUAL(100, BrightnessLevel::sanitize(101));
  TEST_ASSERT_EQUAL(100, BrightnessLevel::sanitize(9999));
}

void test_sanitize_rounds_off_grid_values_to_nearest_step() {
  TEST_ASSERT_EQUAL(30, BrightnessLevel::sanitize(34));
  TEST_ASSERT_EQUAL(40, BrightnessLevel::sanitize(35));
  TEST_ASSERT_EQUAL(90, BrightnessLevel::sanitize(86));
}

void test_increase_steps_up_by_ten_and_stops_at_100() {
  TEST_ASSERT_EQUAL(20, BrightnessLevel::increase(10));
  TEST_ASSERT_EQUAL(100, BrightnessLevel::increase(90));
  TEST_ASSERT_EQUAL(100, BrightnessLevel::increase(100));
}

void test_decrease_steps_down_by_ten_and_stops_at_10() {
  TEST_ASSERT_EQUAL(90, BrightnessLevel::decrease(100));
  TEST_ASSERT_EQUAL(10, BrightnessLevel::decrease(20));
  TEST_ASSERT_EQUAL(10, BrightnessLevel::decrease(10));
}

void test_steps_from_off_grid_values_land_on_grid() {
  TEST_ASSERT_EQUAL(40, BrightnessLevel::increase(34));
  TEST_ASSERT_EQUAL(20, BrightnessLevel::decrease(34));
  TEST_ASSERT_EQUAL(90, BrightnessLevel::decrease(500));
  TEST_ASSERT_EQUAL(20, BrightnessLevel::increase(-5));
}

void test_cycle_steps_up_and_wraps_from_100_to_10() {
  TEST_ASSERT_EQUAL(20, BrightnessLevel::cycle(10));
  TEST_ASSERT_EQUAL(100, BrightnessLevel::cycle(90));
  TEST_ASSERT_EQUAL(10, BrightnessLevel::cycle(100));
  TEST_ASSERT_EQUAL(10, BrightnessLevel::cycle(500));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_sanitize_keeps_every_valid_level);
  RUN_TEST(test_sanitize_clamps_out_of_range_values);
  RUN_TEST(test_sanitize_rounds_off_grid_values_to_nearest_step);
  RUN_TEST(test_increase_steps_up_by_ten_and_stops_at_100);
  RUN_TEST(test_decrease_steps_down_by_ten_and_stops_at_10);
  RUN_TEST(test_steps_from_off_grid_values_land_on_grid);
  RUN_TEST(test_cycle_steps_up_and_wraps_from_100_to_10);
  return UNITY_END();
}
