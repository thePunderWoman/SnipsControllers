#include <unity.h>

#include <climits>

#include "loop_pacer.h"

void setUp(void) {}
void tearDown(void) {}

void test_fast_pass_idles_for_rest_of_period() {
  TEST_ASSERT_EQUAL_UINT32(4, LoopPacer::idleMsAfterPass(1000, 1001, 5));
}

void test_instant_pass_idles_full_period() {
  TEST_ASSERT_EQUAL_UINT32(5, LoopPacer::idleMsAfterPass(1000, 1000, 5));
}

void test_pass_exactly_one_period_does_not_idle() {
  TEST_ASSERT_EQUAL_UINT32(0, LoopPacer::idleMsAfterPass(1000, 1005, 5));
}

void test_slow_pass_does_not_idle() {
  // e.g. the once-a-second RSSI query blocking for up to ~200 ms.
  TEST_ASSERT_EQUAL_UINT32(0, LoopPacer::idleMsAfterPass(1000, 1200, 5));
}

void test_handles_millis_rollover() {
  // Pass started at ULONG_MAX - 1 and ended after millis() wrapped to 1:
  // 3 ms elapsed.
  TEST_ASSERT_EQUAL_UINT32(
      2, LoopPacer::idleMsAfterPass(ULONG_MAX - 1, 1, 5));
}

void test_default_period_is_five_ms() {
  TEST_ASSERT_EQUAL_UINT32(5, LoopPacer::kPassPeriodMs);
  TEST_ASSERT_EQUAL_UINT32(3, LoopPacer::idleMsAfterPass(0, 2));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_fast_pass_idles_for_rest_of_period);
  RUN_TEST(test_instant_pass_idles_full_period);
  RUN_TEST(test_pass_exactly_one_period_does_not_idle);
  RUN_TEST(test_slow_pass_does_not_idle);
  RUN_TEST(test_handles_millis_rollover);
  RUN_TEST(test_default_period_is_five_ms);
  return UNITY_END();
}
