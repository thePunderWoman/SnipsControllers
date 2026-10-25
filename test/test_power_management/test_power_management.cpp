#include <unity.h>

#include "power_management.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

PowerConfig testConfig() {
  PowerConfig config;
  config.dimTimeoutSec = 5;
  config.offTimeoutSec = 10;
  config.xbeeSleepTimeoutSec = 15;
  config.autoPoweroffTimeoutSec = 60;
  return config;
}

PowerState state(ScreenPower screen, bool xbeeAsleep) {
  PowerState result;
  result.screen = screen;
  result.xbeeAsleep = xbeeAsleep;
  return result;
}

const PowerState kFull = state(ScreenPower::kFull, false);
const PowerState kDim = state(ScreenPower::kDim, false);
const PowerState kOff = state(ScreenPower::kOff, false);
const PowerState kOffAsleep = state(ScreenPower::kOff, true);

PowerState stateAfterIdle(const PowerConfig &config, unsigned long idleMs) {
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, idleMs);
  return manager.currentState();
}

}  // namespace

// ---- idle stages -------------------------------------------------------------

void test_stays_full_with_no_activity_gap() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  TEST_ASSERT_TRUE(kFull == manager.currentState());
}

void test_dims_at_exactly_dim_timeout() {
  TEST_ASSERT_TRUE(kFull == stateAfterIdle(testConfig(), 4999));
  TEST_ASSERT_TRUE(kDim == stateAfterIdle(testConfig(), 5000));
}

void test_turns_off_after_dim_plus_off_timeout() {
  // dim(5) + off(10)
  TEST_ASSERT_TRUE(kDim == stateAfterIdle(testConfig(), 14999));
  TEST_ASSERT_TRUE(kOff == stateAfterIdle(testConfig(), 15000));
}

void test_sleeps_xbee_after_full_cascade() {
  // dim(5) + off(10) + xbee(15)
  TEST_ASSERT_TRUE(kOff == stateAfterIdle(testConfig(), 29999));
  TEST_ASSERT_TRUE(kOffAsleep == stateAfterIdle(testConfig(), 30000));
}

void test_activity_returns_immediately_to_full_from_any_state() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 0);
  manager.update(false, 30000);
  TEST_ASSERT_TRUE(kOffAsleep == manager.currentState());

  manager.update(true, 30001);
  TEST_ASSERT_TRUE(kFull == manager.currentState());

  // And the cascade restarts fresh from this new activity time.
  manager.update(false, 30001 + 4999);
  TEST_ASSERT_TRUE(kFull == manager.currentState());
  manager.update(false, 30001 + 5000);
  TEST_ASSERT_TRUE(kDim == manager.currentState());
}

void test_default_config_never_leaves_full() {
  // Matches the controller's behavior before low-power mode existed.
  TEST_ASSERT_TRUE(kFull == stateAfterIdle(PowerConfig(), 3600000));
}

void test_never_off_and_xbee_sleep_stays_dimmed() {
  PowerConfig config = testConfig();
  config.offTimeoutSec = PowerConfigOptions::kNever;
  config.xbeeSleepTimeoutSec = PowerConfigOptions::kNever;
  TEST_ASSERT_TRUE(kDim == stateAfterIdle(config, 1000000));
}

void test_never_xbee_sleep_stays_off_with_xbee_awake() {
  PowerConfig config = testConfig();
  config.xbeeSleepTimeoutSec = PowerConfigOptions::kNever;
  TEST_ASSERT_TRUE(kOff == stateAfterIdle(config, 1000000));
}

void test_never_dim_skips_it_and_adds_no_delay() {
  PowerConfig config = testConfig();
  config.dimTimeoutSec = PowerConfigOptions::kNever;
  TEST_ASSERT_TRUE(kFull == stateAfterIdle(config, 9999));
  TEST_ASSERT_TRUE(kOff == stateAfterIdle(config, 10000));  // off(10)
  TEST_ASSERT_TRUE(kOff == stateAfterIdle(config, 24999));
  TEST_ASSERT_TRUE(kOffAsleep == stateAfterIdle(config, 25000));  // + xbee(15)
}

void test_never_off_still_sleeps_xbee_with_screen_dimmed() {
  PowerConfig config = testConfig();
  config.offTimeoutSec = PowerConfigOptions::kNever;
  TEST_ASSERT_TRUE(kDim == stateAfterIdle(config, 19999));
  // dim(5) + xbee(15), with the screen left on.
  TEST_ASSERT_TRUE(state(ScreenPower::kDim, true) ==
                   stateAfterIdle(config, 20000));
}

void test_only_xbee_sleep_leaves_screen_at_full() {
  PowerConfig config = testConfig();
  config.dimTimeoutSec = PowerConfigOptions::kNever;
  config.offTimeoutSec = PowerConfigOptions::kNever;
  TEST_ASSERT_TRUE(kFull == stateAfterIdle(config, 14999));
  TEST_ASSERT_TRUE(state(ScreenPower::kFull, true) ==
                   stateAfterIdle(config, 15000));
}

void test_power_state_equality() {
  TEST_ASSERT_TRUE(kOff == state(ScreenPower::kOff, false));
  TEST_ASSERT_FALSE(kOff == kOffAsleep);
  TEST_ASSERT_TRUE(kOff != kOffAsleep);
  TEST_ASSERT_TRUE(kOff != kDim);
}

// ---- auto power-off, independent of the idle stages ------------------------

void test_auto_poweroff_fires_at_threshold_even_with_every_stage_never() {
  PowerConfig config;  // every idle stage Never
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

void test_auto_poweroff_never_does_not_fire() {
  PowerConfig config = testConfig();
  config.autoPoweroffTimeoutSec = PowerConfigOptions::kNever;
  PowerManager manager;
  manager.setConfig(config);
  manager.update(true, 0);
  manager.update(false, 0);
  manager.update(false, 100000000);
  TEST_ASSERT_FALSE(manager.consumeShouldPowerOff());
}

// ---- cycling helpers (used by the menu) ------------------------------------

void test_next_cascade_timeout_cycles_through_options_and_never() {
  TEST_ASSERT_EQUAL(10, nextCascadeTimeoutSec(5));
  TEST_ASSERT_EQUAL(15, nextCascadeTimeoutSec(10));
  TEST_ASSERT_EQUAL(30, nextCascadeTimeoutSec(15));
  TEST_ASSERT_EQUAL(60, nextCascadeTimeoutSec(30));
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, nextCascadeTimeoutSec(60));
  TEST_ASSERT_EQUAL(5, nextCascadeTimeoutSec(PowerConfigOptions::kNever));
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
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, nextPoweroffTimeoutSec(900));
  TEST_ASSERT_EQUAL(60, nextPoweroffTimeoutSec(PowerConfigOptions::kNever));
}

// ---- settings saved with the old mode --------------------------------------

void test_legacy_modes_turn_off_the_stages_they_left_out() {
  const int never = PowerConfigOptions::kNever;
  const int expected[][3] = {
      {never, never, never},  // 0: always on
      {5, never, never},      // 1: dim only
      {5, 10, never},         // 2: dim + off
      {5, 10, 15},            // 3: dim + off + XBee sleep
  };
  for (int mode = 0; mode < 4; ++mode) {
    PowerConfig config = testConfig();
    applyLegacyPowerMode(mode, &config);
    TEST_ASSERT_EQUAL(expected[mode][0], config.dimTimeoutSec);
    TEST_ASSERT_EQUAL(expected[mode][1], config.offTimeoutSec);
    TEST_ASSERT_EQUAL(expected[mode][2], config.xbeeSleepTimeoutSec);
    TEST_ASSERT_EQUAL(60, config.autoPoweroffTimeoutSec);
  }
}

void test_out_of_range_legacy_modes_match_old_behavior() {
  PowerConfig negative = testConfig();
  applyLegacyPowerMode(-1, &negative);
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, negative.dimTimeoutSec);
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, negative.xbeeSleepTimeoutSec);

  PowerConfig large = testConfig();
  applyLegacyPowerMode(99, &large);
  TEST_ASSERT_EQUAL(5, large.dimTimeoutSec);
  TEST_ASSERT_EQUAL(15, large.xbeeSleepTimeoutSec);
}

void test_default_config_has_every_idle_stage_never() {
  PowerConfig config;
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, config.dimTimeoutSec);
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, config.offTimeoutSec);
  TEST_ASSERT_EQUAL(PowerConfigOptions::kNever, config.xbeeSleepTimeoutSec);
  TEST_ASSERT_EQUAL(300, config.autoPoweroffTimeoutSec);
}

void test_default_config_uses_lowest_led_brightness() {
  PowerConfig config;
  TEST_ASSERT_EQUAL(10, config.ledBrightnessPercent);
}

void test_default_config_uses_80_percent_screen_brightness() {
  PowerConfig config;
  TEST_ASSERT_EQUAL(80, config.screenBrightnessPercent);
}

void test_xbee_sleeps_only_in_its_stage_or_countdown() {
  TEST_ASSERT_FALSE(shouldXbeeSleep(kFull, false));
  TEST_ASSERT_FALSE(shouldXbeeSleep(kDim, false));
  TEST_ASSERT_FALSE(shouldXbeeSleep(kOff, false));
  TEST_ASSERT_TRUE(shouldXbeeSleep(kOffAsleep, false));
  TEST_ASSERT_TRUE(shouldXbeeSleep(state(ScreenPower::kDim, true), false));
  TEST_ASSERT_TRUE(shouldXbeeSleep(kFull, true));
}

void test_light_sleep_only_with_screen_off_and_xbee_asleep() {
  TEST_ASSERT_FALSE(shouldLightSleep(kFull, false, false, false));
  TEST_ASSERT_FALSE(shouldLightSleep(kDim, false, false, false));
  TEST_ASSERT_FALSE(shouldLightSleep(kOff, false, false, false));
  // The screen still needs its once-a-second redraw.
  TEST_ASSERT_FALSE(
      shouldLightSleep(state(ScreenPower::kDim, true), false, false, false));
  TEST_ASSERT_TRUE(shouldLightSleep(kOffAsleep, false, false, false));
}

void test_no_light_sleep_with_usb_host_charging_or_countdown() {
  TEST_ASSERT_FALSE(shouldLightSleep(kOffAsleep, true, false, false));
  TEST_ASSERT_FALSE(shouldLightSleep(kOffAsleep, false, true, false));
  TEST_ASSERT_FALSE(shouldLightSleep(kOffAsleep, false, false, true));
}

void test_light_sleep_period_is_short_enough_for_stick_sampling() {
  TEST_ASSERT_EQUAL_UINT32(100, kLightSleepPeriodMs);
}

void test_idle_ms_counts_from_last_activity() {
  PowerManager manager;
  manager.setConfig(testConfig());
  manager.update(true, 1000);
  manager.update(false, 3500);
  TEST_ASSERT_EQUAL_UINT32(2500, manager.idleMs(3500));
  manager.update(true, 4000);
  TEST_ASSERT_EQUAL_UINT32(0, manager.idleMs(4000));
}

void test_power_state_names() {
  TEST_ASSERT_EQUAL_STRING("full", powerStateName(kFull));
  TEST_ASSERT_EQUAL_STRING("dim", powerStateName(kDim));
  TEST_ASSERT_EQUAL_STRING("display off", powerStateName(kOff));
  TEST_ASSERT_EQUAL_STRING("display off + XBee asleep",
                           powerStateName(kOffAsleep));
  TEST_ASSERT_EQUAL_STRING("full + XBee asleep",
                           powerStateName(state(ScreenPower::kFull, true)));
  TEST_ASSERT_EQUAL_STRING("dim + XBee asleep",
                           powerStateName(state(ScreenPower::kDim, true)));
  TEST_ASSERT_EQUAL_STRING(
      "unknown", powerStateName(state(static_cast<ScreenPower>(99), false)));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_stays_full_with_no_activity_gap);
  RUN_TEST(test_dims_at_exactly_dim_timeout);
  RUN_TEST(test_turns_off_after_dim_plus_off_timeout);
  RUN_TEST(test_sleeps_xbee_after_full_cascade);
  RUN_TEST(test_activity_returns_immediately_to_full_from_any_state);
  RUN_TEST(test_default_config_never_leaves_full);
  RUN_TEST(test_never_off_and_xbee_sleep_stays_dimmed);
  RUN_TEST(test_never_xbee_sleep_stays_off_with_xbee_awake);
  RUN_TEST(test_never_dim_skips_it_and_adds_no_delay);
  RUN_TEST(test_never_off_still_sleeps_xbee_with_screen_dimmed);
  RUN_TEST(test_only_xbee_sleep_leaves_screen_at_full);
  RUN_TEST(test_power_state_equality);
  RUN_TEST(test_auto_poweroff_fires_at_threshold_even_with_every_stage_never);
  RUN_TEST(test_auto_poweroff_only_reports_once);
  RUN_TEST(test_activity_before_poweroff_threshold_resets_it);
  RUN_TEST(test_auto_poweroff_never_does_not_fire);
  RUN_TEST(test_next_cascade_timeout_cycles_through_options_and_never);
  RUN_TEST(test_next_cascade_timeout_recovers_from_unknown_value);
  RUN_TEST(test_next_poweroff_timeout_cycles_through_options_and_wraps);
  RUN_TEST(test_legacy_modes_turn_off_the_stages_they_left_out);
  RUN_TEST(test_out_of_range_legacy_modes_match_old_behavior);
  RUN_TEST(test_default_config_has_every_idle_stage_never);
  RUN_TEST(test_default_config_uses_lowest_led_brightness);
  RUN_TEST(test_default_config_uses_80_percent_screen_brightness);
  RUN_TEST(test_xbee_sleeps_only_in_its_stage_or_countdown);
  RUN_TEST(test_light_sleep_only_with_screen_off_and_xbee_asleep);
  RUN_TEST(test_no_light_sleep_with_usb_host_charging_or_countdown);
  RUN_TEST(test_light_sleep_period_is_short_enough_for_stick_sampling);
  RUN_TEST(test_idle_ms_counts_from_last_activity);
  RUN_TEST(test_power_state_names);
  return UNITY_END();
}
