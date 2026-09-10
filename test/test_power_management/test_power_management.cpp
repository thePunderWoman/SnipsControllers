#include <unity.h>

#include "power_management.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

PowerConfig testConfig() {
  PowerConfig config;
  config.mode = PowerManagementMode::kDimOffAndXbeeSleep;
  config.dimTimeoutSec = 5;
  config.offTimeoutSec = 10;
  config.xbeeSleepTimeoutSec = 15;
  config.autoPoweroffTimeoutSec = 60;
  return config;
}

}  // namespace

// ---- cascade tiers, respecting mode ---------------------------------------

void test_stays_full_with_no_activity_gap() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  TEST_ASSERT_TRUE(PowerTier::kFull == manager.currentTier());
}

void test_dims_at_exactly_dim_timeout() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 4999);
  TEST_ASSERT_TRUE(PowerTier::kFull == manager.currentTier());
  manager.update(false, 5000);
  TEST_ASSERT_TRUE(PowerTier::kDim == manager.currentTier());
}

void test_turns_off_after_dim_plus_off_timeout() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 14999);  // dim(5) + off(10) - 1ms
  TEST_ASSERT_TRUE(PowerTier::kDim == manager.currentTier());
  manager.update(false, 15000);
  TEST_ASSERT_TRUE(PowerTier::kOff == manager.currentTier());
}

void test_sleeps_xbee_after_full_cascade() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 29999);  // dim(5) + off(10) + xbee(15) - 1ms
  TEST_ASSERT_TRUE(PowerTier::kOff == manager.currentTier());
  manager.update(false, 30000);
  TEST_ASSERT_TRUE(PowerTier::kXbeeAsleep == manager.currentTier());
}

void test_activity_returns_immediately_to_full_from_any_tier() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 30000);
  TEST_ASSERT_TRUE(PowerTier::kXbeeAsleep == manager.currentTier());

  manager.update(true, 30001);
  TEST_ASSERT_TRUE(PowerTier::kFull == manager.currentTier());

  // And the cascade restarts fresh from this new activity time.
  manager.update(false, 30001 + 4999);
  TEST_ASSERT_TRUE(PowerTier::kFull == manager.currentTier());
  manager.update(false, 30001 + 5000);
  TEST_ASSERT_TRUE(PowerTier::kDim == manager.currentTier());
}

void test_mode_dim_only_never_goes_past_dim() {
  PowerConfig config = testConfig();
  config.mode = PowerManagementMode::kDimOnly;
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, 1000000);
  TEST_ASSERT_TRUE(PowerTier::kDim == manager.currentTier());
}

void test_mode_dim_and_off_never_sleeps_xbee() {
  PowerConfig config = testConfig();
  config.mode = PowerManagementMode::kDimAndOff;
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, 1000000);
  TEST_ASSERT_TRUE(PowerTier::kOff == manager.currentTier());
}

void test_mode_always_on_never_leaves_full() {
  PowerConfig config = testConfig();
  config.mode = PowerManagementMode::kAlwaysOn;
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, 1000000);
  TEST_ASSERT_TRUE(PowerTier::kFull == manager.currentTier());
}

// ---- auto power-off, independent of mode -----------------------------------

void test_auto_poweroff_fires_at_threshold_even_at_always_on_mode() {
  PowerConfig config = testConfig();
  config.mode = PowerManagementMode::kAlwaysOn;
  config.autoPoweroffTimeoutSec = 60;
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, 59999);
  TEST_ASSERT_FALSE(manager.consumeShouldPowerOff());
  manager.update(false, 60000);
  TEST_ASSERT_TRUE(manager.consumeShouldPowerOff());
}

void test_auto_poweroff_only_reports_once() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 60000);
  TEST_ASSERT_TRUE(manager.consumeShouldPowerOff());
  TEST_ASSERT_FALSE(manager.consumeShouldPowerOff());
}

void test_activity_before_poweroff_threshold_resets_it() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 59000);
  manager.update(true, 59500);  // fresh activity, resets the idle clock
  manager.update(false, 59500 + 59999);
  TEST_ASSERT_FALSE(manager.consumeShouldPowerOff());
  manager.update(false, 59500 + 60000);
  TEST_ASSERT_TRUE(manager.consumeShouldPowerOff());
}

// ---- cycling helpers (used by the menu) ------------------------------------

void test_next_power_management_mode_cycles_and_wraps() {
  TEST_ASSERT_TRUE(PowerManagementMode::kDimOnly ==
                   nextPowerManagementMode(PowerManagementMode::kAlwaysOn));
  TEST_ASSERT_TRUE(
      PowerManagementMode::kDimAndOff ==
      nextPowerManagementMode(PowerManagementMode::kDimOnly));
  TEST_ASSERT_TRUE(
      PowerManagementMode::kDimOffAndXbeeSleep ==
      nextPowerManagementMode(PowerManagementMode::kDimAndOff));
  TEST_ASSERT_TRUE(
      PowerManagementMode::kAlwaysOn ==
      nextPowerManagementMode(PowerManagementMode::kDimOffAndXbeeSleep));
}

void test_next_cascade_timeout_cycles_through_options_and_wraps() {
  TEST_ASSERT_EQUAL(10, nextCascadeTimeoutSec(5));
  TEST_ASSERT_EQUAL(15, nextCascadeTimeoutSec(10));
  TEST_ASSERT_EQUAL(30, nextCascadeTimeoutSec(15));
  TEST_ASSERT_EQUAL(60, nextCascadeTimeoutSec(30));
  TEST_ASSERT_EQUAL(5, nextCascadeTimeoutSec(60));
}

void test_next_cascade_timeout_recovers_from_unknown_value() {
  // A value that predates an option-list change (e.g. loaded from NVS)
  // falls back to the first option rather than getting stuck.
  TEST_ASSERT_EQUAL(5, nextCascadeTimeoutSec(999));
}

void test_next_poweroff_timeout_cycles_through_options_and_wraps() {
  TEST_ASSERT_EQUAL(300, nextPoweroffTimeoutSec(60));
  TEST_ASSERT_EQUAL(600, nextPoweroffTimeoutSec(300));
  TEST_ASSERT_EQUAL(900, nextPoweroffTimeoutSec(600));
  TEST_ASSERT_EQUAL(60, nextPoweroffTimeoutSec(900));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_stays_full_with_no_activity_gap);
  RUN_TEST(test_dims_at_exactly_dim_timeout);
  RUN_TEST(test_turns_off_after_dim_plus_off_timeout);
  RUN_TEST(test_sleeps_xbee_after_full_cascade);
  RUN_TEST(test_activity_returns_immediately_to_full_from_any_tier);
  RUN_TEST(test_mode_dim_only_never_goes_past_dim);
  RUN_TEST(test_mode_dim_and_off_never_sleeps_xbee);
  RUN_TEST(test_mode_always_on_never_leaves_full);
  RUN_TEST(test_auto_poweroff_fires_at_threshold_even_at_always_on_mode);
  RUN_TEST(test_auto_poweroff_only_reports_once);
  RUN_TEST(test_activity_before_poweroff_threshold_resets_it);
  RUN_TEST(test_next_power_management_mode_cycles_and_wraps);
  RUN_TEST(test_next_cascade_timeout_cycles_through_options_and_wraps);
  RUN_TEST(test_next_cascade_timeout_recovers_from_unknown_value);
  RUN_TEST(test_next_poweroff_timeout_cycles_through_options_and_wraps);
  return UNITY_END();
}
