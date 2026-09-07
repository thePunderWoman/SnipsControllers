#include <unity.h>

#include "power_latch.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_shutdown_when_never_pressed() {
  PowerOffDetector detector(3000);
  TEST_ASSERT_FALSE(detector.update(false, 0));
  TEST_ASSERT_FALSE(detector.update(false, 5000));
}

void test_no_shutdown_before_threshold() {
  PowerOffDetector detector(3000);
  TEST_ASSERT_FALSE(detector.update(true, 0));
  TEST_ASSERT_FALSE(detector.update(true, 1500));
  TEST_ASSERT_FALSE(detector.update(true, 2999));
}

void test_fires_exactly_at_threshold() {
  PowerOffDetector detector(3000);
  TEST_ASSERT_FALSE(detector.update(true, 0));
  TEST_ASSERT_FALSE(detector.update(true, 2999));
  TEST_ASSERT_TRUE(detector.update(true, 3000));
}

void test_does_not_fire_again_while_still_held() {
  PowerOffDetector detector(3000);
  detector.update(true, 0);
  TEST_ASSERT_TRUE(detector.update(true, 3000));
  TEST_ASSERT_FALSE(detector.update(true, 3001));
  TEST_ASSERT_FALSE(detector.update(true, 10000));
}

void test_release_resets_and_can_fire_again_on_next_hold() {
  PowerOffDetector detector(3000);
  detector.update(true, 0);
  TEST_ASSERT_TRUE(detector.update(true, 3000));
  TEST_ASSERT_FALSE(detector.update(false, 3100));

  // A fresh press starts a new hold window from scratch.
  TEST_ASSERT_FALSE(detector.update(true, 4000));
  TEST_ASSERT_FALSE(detector.update(true, 6999));
  TEST_ASSERT_TRUE(detector.update(true, 7000));
}

void test_short_presses_do_not_accumulate_across_releases() {
  PowerOffDetector detector(3000);
  // Held 0..2000 (2s), released, held again 2100..4100 (2s) — neither
  // continuous hold reaches the 3s threshold, even though total pressed
  // time across both presses does.
  TEST_ASSERT_FALSE(detector.update(true, 0));
  TEST_ASSERT_FALSE(detector.update(true, 2000));
  TEST_ASSERT_FALSE(detector.update(false, 2050));
  TEST_ASSERT_FALSE(detector.update(true, 2100));
  TEST_ASSERT_FALSE(detector.update(true, 4100));
}

void test_zero_threshold_fires_on_first_pressed_tick() {
  PowerOffDetector detector(0);
  TEST_ASSERT_TRUE(detector.update(true, 1234));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_no_shutdown_when_never_pressed);
  RUN_TEST(test_no_shutdown_before_threshold);
  RUN_TEST(test_fires_exactly_at_threshold);
  RUN_TEST(test_does_not_fire_again_while_still_held);
  RUN_TEST(test_release_resets_and_can_fire_again_on_next_hold);
  RUN_TEST(test_short_presses_do_not_accumulate_across_releases);
  RUN_TEST(test_zero_threshold_fires_on_first_pressed_tick);
  return UNITY_END();
}
