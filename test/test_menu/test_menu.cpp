#include <unity.h>
#include <cstring>

#include "menu.h"

void setUp(void) {}
void tearDown(void) {}

// ---- open combo -----------------------------------------------------------

void test_starts_inactive() {
  MenuController menu;
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_combo_held_shorter_than_threshold_does_not_open() {
  MenuController menu;
  menu.updateOpenCombo(true, true, 0);
  menu.updateOpenCombo(true, true, 999);
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_combo_held_past_threshold_opens_main_menu() {
  MenuController menu;
  menu.updateOpenCombo(true, true, 0);
  menu.updateOpenCombo(true, true, 1000);
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

void test_combo_releasing_before_threshold_resets_timer() {
  MenuController menu;
  menu.updateOpenCombo(true, true, 0);
  menu.updateOpenCombo(true, false, 500);  // released early
  menu.updateOpenCombo(true, true, 1000);  // re-held, only 0ms elapsed so far
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
  menu.updateOpenCombo(true, true, 1999);
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
  menu.updateOpenCombo(true, true, 2000);
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

namespace {
void openMenu(MenuController *menu) {
  menu->updateOpenCombo(true, true, 0);
  menu->updateOpenCombo(true, true, 1000);
}
}  // namespace

// ---- main menu navigation --------------------------------------------------

void test_up_down_noop_while_inactive() {
  MenuController menu;
  menu.onUp();
  menu.onDown();
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_down_wraps_around_main_menu() {
  MenuController menu;
  openMenu(&menu);
  TEST_ASSERT_TRUE(MainMenuItem::kCalibrateStick ==
                    menu.selectedMainMenuItem());
  menu.onDown();
  TEST_ASSERT_TRUE(MainMenuItem::kCalibrateTrigger ==
                    menu.selectedMainMenuItem());
  menu.onDown();
  TEST_ASSERT_TRUE(MainMenuItem::kDeviceInfo == menu.selectedMainMenuItem());
  menu.onDown();
  TEST_ASSERT_TRUE(MainMenuItem::kFactoryReset ==
                    menu.selectedMainMenuItem());
  menu.onDown();  // wraps back to the first item
  TEST_ASSERT_TRUE(MainMenuItem::kCalibrateStick ==
                    menu.selectedMainMenuItem());
}

void test_up_wraps_around_main_menu() {
  MenuController menu;
  openMenu(&menu);
  menu.onUp();  // wraps to the last item
  TEST_ASSERT_TRUE(MainMenuItem::kFactoryReset ==
                    menu.selectedMainMenuItem());
}

void test_back_from_main_menu_closes() {
  MenuController menu;
  openMenu(&menu);
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_back_and_enter_are_noop_while_inactive() {
  MenuController menu;
  menu.onBack();
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_back_during_stick_calibration_cancels_and_resets() {
  MenuController menu;
  openMenu(&menu);
  menu.onEnter(0, 0, 0);         // -> kCalibrateStick
  menu.onEnter(0, 2000, 2100);  // confirm center, now rolling
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  // Re-entering starts a fresh flow, not resuming the cancelled one.
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kAwaitingCenter ==
                    menu.stickCalibrationStep());
}

// ---- entering leaf screens --------------------------------------------------

void test_enter_device_info_from_main_menu() {
  MenuController menu;
  openMenu(&menu);
  menu.onDown();
  menu.onDown();  // -> kDeviceInfo
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kDeviceInfo == menu.currentScreen());
}

void test_device_info_enter_or_back_returns_to_main_menu() {
  MenuController menu;
  openMenu(&menu);
  menu.onDown();
  menu.onDown();
  menu.onEnter(0, 0, 0);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

void test_enter_factory_reset_confirm_then_back_cancels() {
  MenuController menu;
  openMenu(&menu);
  menu.onUp();  // -> kFactoryReset (wraps to last item)
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kFactoryResetConfirm == menu.currentScreen());

  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
  TEST_ASSERT_FALSE(menu.consumeFactoryResetConfirmed());
}

void test_factory_reset_confirmed_via_enter() {
  MenuController menu;
  openMenu(&menu);
  menu.onUp();
  menu.onEnter(0, 0, 0);  // -> confirm screen
  menu.onEnter(0, 0, 0);  // confirms

  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
  TEST_ASSERT_TRUE(menu.consumeFactoryResetConfirmed());
  // Consuming is edge-triggered — a second call returns false.
  TEST_ASSERT_FALSE(menu.consumeFactoryResetConfirmed());
}

// ---- trigger calibration ---------------------------------------------------

void test_trigger_calibration_full_flow() {
  MenuController menu;
  openMenu(&menu);
  menu.onDown();  // -> kCalibrateTrigger
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kCalibrateTrigger == menu.currentScreen());
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingRelease ==
                    menu.triggerCalibrationStep());

  menu.onEnter(50, 0, 0);  // capture release
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingFullPull ==
                    menu.triggerCalibrationStep());

  menu.onEnter(4000, 0, 0);  // capture full pull -> done
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  int min = -1, max = -1;
  TEST_ASSERT_TRUE(menu.consumeNewTriggerCalibration(&min, &max));
  TEST_ASSERT_EQUAL_INT(50, min);
  TEST_ASSERT_EQUAL_INT(4000, max);
  TEST_ASSERT_FALSE(menu.consumeNewTriggerCalibration(&min, &max));
}

void test_back_during_trigger_calibration_cancels_and_resets() {
  MenuController menu;
  openMenu(&menu);
  menu.onDown();
  menu.onEnter(0, 0, 0);
  menu.onEnter(50, 0, 0);  // partway through
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  // Re-entering starts a fresh flow, not resuming the cancelled one.
  menu.onDown();
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingRelease ==
                    menu.triggerCalibrationStep());
}

// ---- stick calibration ------------------------------------------------------

void test_stick_calibration_full_flow() {
  MenuController menu;
  openMenu(&menu);
  menu.onEnter(0, 0, 0);  // -> kCalibrateStick (first item)
  TEST_ASSERT_TRUE(MenuScreen::kCalibrateStick == menu.currentScreen());

  menu.onEnter(0, 2000, 2100);  // confirm center
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kRolling ==
                    menu.stickCalibrationStep());

  menu.tick(1500, 2600);
  menu.tick(2500, 1800);
  menu.onEnter(0, 9999, 9999);  // confirm done (values here are ignored)

  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  int centerX, centerY, minX, maxX, minY, maxY;
  TEST_ASSERT_TRUE(menu.consumeNewStickCalibration(&centerX, &centerY, &minX,
                                                   &maxX, &minY, &maxY));
  TEST_ASSERT_EQUAL_INT(2000, centerX);
  TEST_ASSERT_EQUAL_INT(2100, centerY);
  TEST_ASSERT_EQUAL_INT(1500, minX);
  TEST_ASSERT_EQUAL_INT(2500, maxX);
  TEST_ASSERT_EQUAL_INT(1800, minY);
  TEST_ASSERT_EQUAL_INT(2600, maxY);
  TEST_ASSERT_FALSE(
      menu.consumeNewStickCalibration(&centerX, &centerY, &minX, &maxX,
                                      &minY, &maxY));
}

void test_tick_is_noop_outside_rolling_stick_calibration() {
  MenuController menu;
  openMenu(&menu);
  // Menu is open but not even in the stick screen yet.
  menu.tick(1234, 5678);
  menu.onEnter(0, 0, 0);  // -> kCalibrateStick, awaiting center
  menu.tick(1234, 5678);  // still awaiting center, not rolling
  menu.onEnter(0, 2000, 2100);  // confirm center -> rolling
  int centerX, centerY, minX, maxX, minY, maxY;
  menu.onEnter(0, 0, 0);  // confirm done immediately, no samples taken
  TEST_ASSERT_TRUE(menu.consumeNewStickCalibration(&centerX, &centerY, &minX,
                                                   &maxX, &minY, &maxY));
  // With no rolling samples, min/max should stay collapsed to the center.
  TEST_ASSERT_EQUAL_INT(2000, minX);
  TEST_ASSERT_EQUAL_INT(2000, maxX);
}

// ---- labels ------------------------------------------------------------------

void test_main_menu_item_labels() {
  TEST_ASSERT_EQUAL_STRING("Calibrate Stick",
                           mainMenuItemLabel(MainMenuItem::kCalibrateStick));
  TEST_ASSERT_EQUAL_STRING("Calibrate Trigger",
                           mainMenuItemLabel(MainMenuItem::kCalibrateTrigger));
  TEST_ASSERT_EQUAL_STRING("Device Info",
                           mainMenuItemLabel(MainMenuItem::kDeviceInfo));
  TEST_ASSERT_EQUAL_STRING("Factory Reset",
                           mainMenuItemLabel(MainMenuItem::kFactoryReset));
  TEST_ASSERT_EQUAL_STRING("Unknown",
                           mainMenuItemLabel(MainMenuItem::kCount));
}

// ---- rendering ----------------------------------------------------------------

void test_render_inactive_leaves_screen_blank() {
  MenuController menu;
  ScreenBuffer screen;
  screen.setLine(0, "stale content");
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("", screen.line(0));
}

void test_render_main_menu_marks_selected_item() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onDown();  // select Calibrate Trigger
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("  Calibrate Stick", screen.line(1));
  TEST_ASSERT_EQUAL_STRING("> Calibrate Trigger", screen.line(2));
}

void test_render_trigger_calibration_step_text() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onDown();
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Release trigger,", screen.line(1));

  menu.onEnter(50, 0, 0);  // advance to the full-pull step
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Pull fully,", screen.line(1));
}

void test_render_stick_calibration_awaiting_center_step_text() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onEnter(0, 0, 0);  // -> kCalibrateStick, awaiting center
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Center stick,", screen.line(1));
}

void test_render_stick_calibration_rolling_step_text() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onEnter(0, 0, 0);        // -> kCalibrateStick
  menu.onEnter(0, 2000, 2100);  // confirm center, now rolling
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Roll to extremes,", screen.line(1));
}

void test_render_device_info() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onDown();
  menu.onDown();
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Device Info", screen.line(0));
}

void test_render_factory_reset_confirm() {
  MenuController menu;
  ScreenBuffer screen;
  openMenu(&menu);
  menu.onUp();
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Factory Reset?", screen.line(0));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_inactive);
  RUN_TEST(test_combo_held_shorter_than_threshold_does_not_open);
  RUN_TEST(test_combo_held_past_threshold_opens_main_menu);
  RUN_TEST(test_combo_releasing_before_threshold_resets_timer);
  RUN_TEST(test_up_down_noop_while_inactive);
  RUN_TEST(test_down_wraps_around_main_menu);
  RUN_TEST(test_up_wraps_around_main_menu);
  RUN_TEST(test_back_from_main_menu_closes);
  RUN_TEST(test_back_and_enter_are_noop_while_inactive);
  RUN_TEST(test_back_during_stick_calibration_cancels_and_resets);
  RUN_TEST(test_enter_device_info_from_main_menu);
  RUN_TEST(test_device_info_enter_or_back_returns_to_main_menu);
  RUN_TEST(test_enter_factory_reset_confirm_then_back_cancels);
  RUN_TEST(test_factory_reset_confirmed_via_enter);
  RUN_TEST(test_trigger_calibration_full_flow);
  RUN_TEST(test_back_during_trigger_calibration_cancels_and_resets);
  RUN_TEST(test_stick_calibration_full_flow);
  RUN_TEST(test_tick_is_noop_outside_rolling_stick_calibration);
  RUN_TEST(test_main_menu_item_labels);
  RUN_TEST(test_render_inactive_leaves_screen_blank);
  RUN_TEST(test_render_main_menu_marks_selected_item);
  RUN_TEST(test_render_trigger_calibration_step_text);
  RUN_TEST(test_render_stick_calibration_awaiting_center_step_text);
  RUN_TEST(test_render_stick_calibration_rolling_step_text);
  RUN_TEST(test_render_device_info);
  RUN_TEST(test_render_factory_reset_confirm);
  return UNITY_END();
}
