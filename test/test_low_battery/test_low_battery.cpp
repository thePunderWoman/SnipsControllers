#include <unity.h>

#include "low_battery.h"
#include "screen.h"

void setUp(void) {}
void tearDown(void) {}

// ---- warning (10%..6%, above critical) -------------------------------------

void test_no_warning_above_ten_percent() {
  LowBatteryMonitor monitor;
  monitor.update(11, false, 0);
  TEST_ASSERT_FALSE(monitor.isWarning());
}

void test_warning_at_exactly_ten_percent() {
  LowBatteryMonitor monitor;
  monitor.update(10, false, 0);
  TEST_ASSERT_TRUE(monitor.isWarning());
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
}

void test_no_warning_while_charging_even_if_low() {
  LowBatteryMonitor monitor;
  monitor.update(3, true, 0);
  TEST_ASSERT_FALSE(monitor.isWarning());
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
}

void test_blink_toggles_on_a_two_second_cycle() {
  LowBatteryMonitor monitor;
  monitor.update(8, false, 0);
  TEST_ASSERT_TRUE(monitor.blinkOn());
  monitor.update(8, false, 999);
  TEST_ASSERT_TRUE(monitor.blinkOn());
  monitor.update(8, false, 1000);
  TEST_ASSERT_FALSE(monitor.blinkOn());
  monitor.update(8, false, 1999);
  TEST_ASSERT_FALSE(monitor.blinkOn());
  monitor.update(8, false, 2000);
  TEST_ASSERT_TRUE(monitor.blinkOn());
}

// ---- critical / countdown ---------------------------------------------------

void test_no_countdown_immediately_at_critical_without_debounce_elapsed() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
  monitor.update(5, false, 2999);
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
}

void test_countdown_starts_once_debounce_elapses() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 2999);
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
  monitor.update(5, false, 3000);
  TEST_ASSERT_TRUE(monitor.isInShutdownCountdown());
  TEST_ASSERT_EQUAL_INT(30, monitor.secondsRemaining());
}

void test_debounce_resets_if_percent_recovers_before_it_elapses() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 2000);
  monitor.update(6, false, 2500);  // recovers above critical, resets timer
  monitor.update(5, false, 2500);
  monitor.update(5, false, 5499);  // would have fired at 2500+3000=5500
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
}

void test_countdown_ticks_down_by_elapsed_seconds() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 3000);  // countdown starts, 30s remaining
  monitor.update(5, false, 3000 + 10000);
  TEST_ASSERT_TRUE(monitor.isInShutdownCountdown());
  TEST_ASSERT_EQUAL_INT(20, monitor.secondsRemaining());
}

void test_poweroff_fires_once_countdown_reaches_zero() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 3000);  // countdown starts
  monitor.update(5, false, 3000 + 29999);
  TEST_ASSERT_FALSE(monitor.consumeShouldPowerOff());
  monitor.update(5, false, 3000 + 30000);
  TEST_ASSERT_TRUE(monitor.consumeShouldPowerOff());
  TEST_ASSERT_FALSE(monitor.consumeShouldPowerOff());
}

void test_countdown_cancelled_by_charging() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 3000);  // countdown starts
  monitor.update(5, true, 10000);  // plugged in mid-countdown
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
  monitor.update(5, true, 3000 + 30000);
  TEST_ASSERT_FALSE(monitor.consumeShouldPowerOff());
}

void test_countdown_cancelled_by_recovery_hysteresis() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 3000);  // countdown starts (critical = 5)
  monitor.update(6, false, 10000);  // recovered, but not past hysteresis yet
  TEST_ASSERT_TRUE(monitor.isInShutdownCountdown());
  monitor.update(7, false, 11000);  // critical(5) + hysteresis(2) = 7
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());
}

void test_countdown_can_restart_after_a_cancelled_one() {
  LowBatteryMonitor monitor;
  monitor.update(5, false, 0);
  monitor.update(5, false, 3000);  // countdown starts
  monitor.update(8, false, 4000);  // cancelled via hysteresis recovery
  TEST_ASSERT_FALSE(monitor.isInShutdownCountdown());

  monitor.update(5, false, 4000);
  monitor.update(5, false, 7000);  // fresh 3s debounce from 4000
  TEST_ASSERT_TRUE(monitor.isInShutdownCountdown());
}

// ---- countdown screen rendering --------------------------------------------

void test_render_countdown_screen_shows_message_and_seconds() {
  ScreenBuffer screen;
  renderLowBatteryCountdownScreen(30, &screen);
  TEST_ASSERT_EQUAL_STRING("Low Battery!", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("Powering down in", screen.line(1));
  TEST_ASSERT_EQUAL_STRING("30 seconds", screen.line(2));
}

void test_render_countdown_screen_updates_seconds() {
  ScreenBuffer screen;
  renderLowBatteryCountdownScreen(5, &screen);
  TEST_ASSERT_EQUAL_STRING("5 seconds", screen.line(2));
}

void test_render_battery_empty_screen() {
  ScreenBuffer screen;
  renderBatteryEmptyScreen(&screen);
  TEST_ASSERT_EQUAL_STRING("Battery Empty", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("Please charge", screen.line(1));
}

void test_render_charging_fault_screen() {
  ScreenBuffer screen;
  renderChargingFaultScreen(&screen);
  TEST_ASSERT_EQUAL_STRING("Charging Fault!", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("Unplug & replug", screen.line(1));
  TEST_ASSERT_EQUAL_STRING("charger", screen.line(2));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_no_warning_above_ten_percent);
  RUN_TEST(test_warning_at_exactly_ten_percent);
  RUN_TEST(test_no_warning_while_charging_even_if_low);
  RUN_TEST(test_blink_toggles_on_a_two_second_cycle);
  RUN_TEST(test_no_countdown_immediately_at_critical_without_debounce_elapsed);
  RUN_TEST(test_countdown_starts_once_debounce_elapses);
  RUN_TEST(test_debounce_resets_if_percent_recovers_before_it_elapses);
  RUN_TEST(test_countdown_ticks_down_by_elapsed_seconds);
  RUN_TEST(test_poweroff_fires_once_countdown_reaches_zero);
  RUN_TEST(test_countdown_cancelled_by_charging);
  RUN_TEST(test_countdown_cancelled_by_recovery_hysteresis);
  RUN_TEST(test_countdown_can_restart_after_a_cancelled_one);
  RUN_TEST(test_render_countdown_screen_shows_message_and_seconds);
  RUN_TEST(test_render_countdown_screen_updates_seconds);
  RUN_TEST(test_render_battery_empty_screen);
  RUN_TEST(test_render_charging_fault_screen);
  return UNITY_END();
}
