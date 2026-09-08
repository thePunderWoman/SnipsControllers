#include <unity.h>

#include "battery.h"

void setUp(void) {}
void tearDown(void) {}

// ---- chargeStateFor — bq25185 datasheet Table 7-2 -----------------------

void test_charge_state_both_high_is_done() {
  BatteryMonitor monitor;
  TEST_ASSERT_TRUE(ChargeState::kDone == monitor.chargeStateFor(true, true));
}

void test_charge_state_stat1_high_stat2_low_is_charging() {
  BatteryMonitor monitor;
  TEST_ASSERT_TRUE(ChargeState::kCharging ==
                    monitor.chargeStateFor(true, false));
}

void test_charge_state_stat1_low_stat2_high_is_recoverable_fault() {
  BatteryMonitor monitor;
  TEST_ASSERT_TRUE(ChargeState::kRecoverableFault ==
                    monitor.chargeStateFor(false, true));
}

void test_charge_state_both_low_is_latched_fault() {
  BatteryMonitor monitor;
  TEST_ASSERT_TRUE(ChargeState::kLatchedFault ==
                    monitor.chargeStateFor(false, false));
}

// ---- percentFor -----------------------------------------------------------

void test_percent_at_raw_zero_is_zero() {
  BatteryMonitor monitor;
  TEST_ASSERT_EQUAL_INT(0, monitor.percentFor(0));
}

void test_percent_near_empty_cutoff_is_zero() {
  BatteryMonitor monitor;
  // ~3.0V at the battery (this class's empty-cell cutoff).
  TEST_ASSERT_EQUAL_INT(0, monitor.percentFor(1241));
}

void test_percent_at_midpoint_is_about_fifty() {
  BatteryMonitor monitor;
  // ~3.6V at the battery — halfway between the 3.0V/4.2V empty/full range.
  TEST_ASSERT_EQUAL_INT(50, monitor.percentFor(1489));
}

void test_percent_at_raw_max_clamps_to_hundred() {
  BatteryMonitor monitor;
  TEST_ASSERT_EQUAL_INT(100, monitor.percentFor(4095));
}

void test_percent_clamps_negative_raw_to_zero() {
  BatteryMonitor monitor;
  TEST_ASSERT_EQUAL_INT(0, monitor.percentFor(-500));
}

void test_percent_clamps_raw_beyond_range_to_hundred() {
  BatteryMonitor monitor;
  // Out-of-range high input (beyond the 12-bit ADC's max) should still
  // clamp safely rather than doing anything undefined.
  TEST_ASSERT_EQUAL_INT(100, monitor.percentFor(999999));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_charge_state_both_high_is_done);
  RUN_TEST(test_charge_state_stat1_high_stat2_low_is_charging);
  RUN_TEST(test_charge_state_stat1_low_stat2_high_is_recoverable_fault);
  RUN_TEST(test_charge_state_both_low_is_latched_fault);
  RUN_TEST(test_percent_at_raw_zero_is_zero);
  RUN_TEST(test_percent_near_empty_cutoff_is_zero);
  RUN_TEST(test_percent_at_midpoint_is_about_fifty);
  RUN_TEST(test_percent_at_raw_max_clamps_to_hundred);
  RUN_TEST(test_percent_clamps_negative_raw_to_zero);
  RUN_TEST(test_percent_clamps_raw_beyond_range_to_hundred);
  return UNITY_END();
}
