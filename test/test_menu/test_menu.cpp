#include <unity.h>
#include <cstring>

#include "menu.h"

void setUp(void) {}
void tearDown(void) {}

namespace {

void openMenu(MenuController *menu) {
  menu->updateOpenCombo(true, true, 0);
  menu->updateOpenCombo(true, true, 1000);
}

// Robust to reordering MainMenuItem — scrolls down from the first item
// rather than hardcoding a specific number of onDown() calls.
void selectMainMenuItem(MenuController *menu, MainMenuItem item) {
  openMenu(menu);
  for (int i = 0; i < static_cast<int>(item); ++i) {
    menu->onDown();
  }
}

DroidStore twoDroidStore() {
  DroidStore store;
  store.add("R2-D2", "1111111111111111");
  store.add("BB-8", "2222222222222222");
  return store;
}

class FakeTransport : public XbeeTransport {
 public:
  bool leaveNetwork() override { return true; }
  bool setPanId(const char * /*panId*/) override { return true; }
  bool rejoinNetwork() override { return true; }
};

}  // namespace

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

// ---- main menu navigation --------------------------------------------------

void test_up_down_noop_while_inactive() {
  MenuController menu;
  menu.onUp();
  menu.onDown();
  TEST_ASSERT_TRUE(MenuScreen::kInactive == menu.currentScreen());
}

void test_down_cycles_through_every_main_menu_item_in_order() {
  MenuController menu;
  openMenu(&menu);
  TEST_ASSERT_TRUE(MainMenuItem::kSwitchDroid == menu.selectedMainMenuItem());
  menu.onDown();
  TEST_ASSERT_TRUE(MainMenuItem::kManageDroids ==
                    menu.selectedMainMenuItem());
  menu.onDown();
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
  TEST_ASSERT_TRUE(MainMenuItem::kSwitchDroid == menu.selectedMainMenuItem());
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

// ---- entering leaf screens --------------------------------------------------

void test_enter_device_info_from_main_menu() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kDeviceInfo);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kDeviceInfo == menu.currentScreen());
}

void test_device_info_enter_or_back_returns_to_main_menu() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kDeviceInfo);
  menu.onEnter(0, 0, 0);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

void test_enter_factory_reset_confirm_then_back_cancels() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kFactoryReset);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kFactoryResetConfirm == menu.currentScreen());

  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
  TEST_ASSERT_FALSE(menu.consumeFactoryResetConfirmed());
}

void test_factory_reset_confirmed_via_enter() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kFactoryReset);
  menu.onEnter(0, 0, 0);  // -> confirm screen
  menu.onEnter(0, 0, 0);  // confirms

  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
  TEST_ASSERT_TRUE(menu.consumeFactoryResetConfirmed());
  // Consuming is edge-triggered — a second call returns false.
  TEST_ASSERT_FALSE(menu.consumeFactoryResetConfirmed());

  // Factory reset also clears the droid list, and reports that change.
  TEST_ASSERT_EQUAL_INT(0, menu.droidStore().count());
  TEST_ASSERT_TRUE(menu.consumeDroidStoreChanged());
}

// ---- trigger calibration ---------------------------------------------------

void test_trigger_calibration_full_flow() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateTrigger);
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
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateTrigger);
  menu.onEnter(0, 0, 0);
  menu.onEnter(50, 0, 0);  // partway through
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  // Re-entering starts a fresh flow, not resuming the cancelled one.
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateTrigger);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(TriggerCalibrationFlow::Step::kAwaitingRelease ==
                    menu.triggerCalibrationStep());
}

// ---- stick calibration ------------------------------------------------------

void test_stick_calibration_full_flow() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
  menu.onEnter(0, 0, 0);
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
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
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

void test_back_during_stick_calibration_cancels_and_resets() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
  menu.onEnter(0, 0, 0);         // -> kCalibrateStick
  menu.onEnter(0, 2000, 2100);  // confirm center, now rolling
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());

  // Re-entering starts a fresh flow, not resuming the cancelled one.
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(StickCalibrationFlow::Step::kAwaitingCenter ==
                    menu.stickCalibrationStep());
}

// ---- switch droid ------------------------------------------------------------

void test_switch_droid_list_empty_stays_put_on_enter() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onEnter(0, 0, 0);  // list is empty — should stay put, not crash
  TEST_ASSERT_TRUE(MenuScreen::kSwitchDroidList == menu.currentScreen());
}

void test_switch_droid_navigates_and_reports_no_transport_by_default() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  TEST_ASSERT_EQUAL_INT(0, menu.selectedDroidListIndex());
  menu.onDown();
  TEST_ASSERT_EQUAL_INT(1, menu.selectedDroidListIndex());

  menu.onEnter(0, 0, 0);  // select BB-8, attempt switch
  TEST_ASSERT_TRUE(MenuScreen::kSwitchDroidResult == menu.currentScreen());
  TEST_ASSERT_TRUE(DroidSwitchResult::kNoTransport ==
                    menu.lastSwitchResult());

  menu.onEnter(0, 0, 0);
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

void test_switch_droid_list_up_wraps_and_back_returns_to_main_menu() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onUp();            // wraps to the last entry
  TEST_ASSERT_EQUAL_INT(1, menu.selectedDroidListIndex());

  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kMainMenu == menu.currentScreen());
}

void test_switch_droid_succeeds_with_transport_set() {
  MenuController menu;
  FakeTransport transport;
  menu.setXbeeTransport(&transport);
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onEnter(0, 0, 0);  // select R2-D2, switch
  TEST_ASSERT_TRUE(DroidSwitchResult::kSuccess == menu.lastSwitchResult());
}

// ---- manage droids: add -------------------------------------------------------

void test_manage_droids_add_flow_creates_entry() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());

  menu.onEnter(0, 0, 0);  // only "+ Add New" exists when empty
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsEnterName ==
                    menu.currentScreen());

  menu.onDown();          // space -> 'A'
  menu.onEnter(0, 0, 0);  // commit 'A'
  menu.onUp();            // space -> wraps to DONE
  menu.onEnter(0, 0, 0);  // finish name -> PAN ID entry

  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsEnterPanId ==
                    menu.currentScreen());

  menu.onDown();          // '0' -> '1'
  menu.onEnter(0, 0, 0);  // commit '1'
  menu.onUp();            // '0' -> wraps to DONE
  menu.onEnter(0, 0, 0);  // finish PAN ID -> saved

  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());
  TEST_ASSERT_EQUAL_INT(1, menu.droidStore().count());
  TEST_ASSERT_EQUAL_STRING("A", menu.droidStore().at(0).name);
  TEST_ASSERT_EQUAL_STRING("1", menu.droidStore().at(0).panId);
  TEST_ASSERT_TRUE(menu.consumeDroidStoreChanged());
  TEST_ASSERT_FALSE(menu.consumeDroidStoreChanged());
}

void test_manage_droids_back_with_empty_name_cancels_add() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name, empty buffer
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());
  TEST_ASSERT_EQUAL_INT(0, menu.droidStore().count());
}

void test_manage_droids_back_with_text_backspaces_instead_of_cancelling() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name
  menu.onDown();          // space -> 'A'
  menu.onEnter(0, 0, 0);  // commit 'A'
  menu.onBack();          // backspace, not cancel — buffer wasn't empty
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsEnterName ==
                    menu.currentScreen());
  TEST_ASSERT_EQUAL_STRING("", menu.nameEntry().text());
}

void test_manage_droids_list_navigation_wraps_over_entries_and_add_new() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  TEST_ASSERT_EQUAL_INT(0, menu.selectedDroidListIndex());

  menu.onUp();  // wraps to "+ Add New" (index == count)
  TEST_ASSERT_EQUAL_INT(2, menu.selectedDroidListIndex());

  menu.onDown();  // wraps back to the first entry
  TEST_ASSERT_EQUAL_INT(0, menu.selectedDroidListIndex());
}

void test_manage_droids_pan_id_backspace_and_cancel() {
  MenuController menu;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name
  menu.onUp();            // space -> DONE
  menu.onEnter(0, 0, 0);  // finish name -> enter PAN ID

  menu.onDown();          // '0' -> '1'
  menu.onEnter(0, 0, 0);  // commit '1'
  menu.onBack();          // backspace, not cancel — buffer wasn't empty
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsEnterPanId ==
                    menu.currentScreen());
  TEST_ASSERT_EQUAL_STRING("", menu.panIdEntry().text());

  menu.onBack();  // now empty — cancels the whole add flow
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());
  TEST_ASSERT_EQUAL_INT(0, menu.droidStore().count());
}

// ---- manage droids: delete -----------------------------------------------------

void test_manage_droids_delete_flow_removes_entry() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // select the first existing entry (R2-D2)
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsDeleteConfirm ==
                    menu.currentScreen());

  menu.onEnter(0, 0, 0);  // confirm delete
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());
  TEST_ASSERT_EQUAL_INT(1, menu.droidStore().count());
  TEST_ASSERT_EQUAL_STRING("BB-8", menu.droidStore().at(0).name);
  TEST_ASSERT_TRUE(menu.consumeDroidStoreChanged());
}

void test_manage_droids_delete_confirm_back_cancels() {
  MenuController menu;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> kManageDroidsDeleteConfirm
  menu.onBack();
  TEST_ASSERT_TRUE(MenuScreen::kManageDroidsList == menu.currentScreen());
  TEST_ASSERT_EQUAL_INT(2, menu.droidStore().count());
  TEST_ASSERT_FALSE(menu.consumeDroidStoreChanged());
}

// ---- labels ------------------------------------------------------------------

void test_main_menu_item_labels() {
  TEST_ASSERT_EQUAL_STRING("Switch Droid",
                           mainMenuItemLabel(MainMenuItem::kSwitchDroid));
  TEST_ASSERT_EQUAL_STRING("Manage Droids",
                           mainMenuItemLabel(MainMenuItem::kManageDroids));
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
  menu.onDown();  // select Manage Droids
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("  Switch Droid", screen.line(1));
  TEST_ASSERT_EQUAL_STRING("> Manage Droids", screen.line(2));
}

void test_render_trigger_calibration_step_text() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateTrigger);
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
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Center stick,", screen.line(1));
}

void test_render_stick_calibration_rolling_step_text() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kCalibrateStick);
  menu.onEnter(0, 0, 0);
  menu.onEnter(0, 2000, 2100);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Roll to extremes,", screen.line(1));
}

void test_render_device_info() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kDeviceInfo);
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Device Info", screen.line(0));
}

void test_render_factory_reset_confirm() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kFactoryReset);
  menu.onEnter(0, 0, 0);
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Factory Reset?", screen.line(0));
}

void test_render_switch_droid_list_empty() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("No droids saved", screen.line(1));
}

void test_render_switch_droid_list_marks_selected() {
  MenuController menu;
  ScreenBuffer screen;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onDown();
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("  R2-D2", screen.line(1));
  TEST_ASSERT_EQUAL_STRING("> BB-8", screen.line(2));
}

void test_render_switch_droid_result() {
  MenuController menu;
  ScreenBuffer screen;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onEnter(0, 0, 0);  // select R2-D2, attempt switch
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("No XBee link (PR 8)", screen.line(1));
}

void test_render_manage_droids_list_shows_add_new() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("> + Add New", screen.line(1));
}

void test_render_manage_droids_enter_name_shows_highlight() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("[ ]", screen.line(1));

  menu.onDown();  // space -> 'A'
  menu.onEnter(0, 0, 0);  // commit 'A', highlight resets to space
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("A[ ]", screen.line(1));
}

void test_render_manage_droids_enter_name_shows_done() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name
  menu.onUp();            // space -> wraps to DONE
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("[DONE]", screen.line(1));
}

void test_render_manage_droids_enter_pan_id() {
  MenuController menu;
  ScreenBuffer screen;
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // -> enter name
  menu.onUp();
  menu.onEnter(0, 0, 0);  // finish name -> enter PAN ID
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Add Droid: PAN ID", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("[0]", screen.line(1));
}

void test_render_switch_droid_result_variants() {
  MenuController menu;
  ScreenBuffer screen;
  FakeTransport transport;
  menu.setXbeeTransport(&transport);
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kSwitchDroid);
  menu.onEnter(0, 0, 0);  // -> kSwitchDroidList
  menu.onEnter(0, 0, 0);  // succeeds (FakeTransport always succeeds)
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Success!", screen.line(1));
}

void test_render_manage_droids_delete_confirm_shows_name() {
  MenuController menu;
  ScreenBuffer screen;
  menu.setDroidStore(twoDroidStore());
  selectMainMenuItem(&menu, MainMenuItem::kManageDroids);
  menu.onEnter(0, 0, 0);  // -> kManageDroidsList
  menu.onEnter(0, 0, 0);  // select R2-D2 -> delete confirm
  renderMenuScreen(menu, &screen);
  TEST_ASSERT_EQUAL_STRING("Delete Droid?", screen.line(0));
  TEST_ASSERT_EQUAL_STRING("R2-D2", screen.line(1));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_inactive);
  RUN_TEST(test_combo_held_shorter_than_threshold_does_not_open);
  RUN_TEST(test_combo_held_past_threshold_opens_main_menu);
  RUN_TEST(test_combo_releasing_before_threshold_resets_timer);
  RUN_TEST(test_up_down_noop_while_inactive);
  RUN_TEST(test_down_cycles_through_every_main_menu_item_in_order);
  RUN_TEST(test_up_wraps_around_main_menu);
  RUN_TEST(test_back_from_main_menu_closes);
  RUN_TEST(test_back_and_enter_are_noop_while_inactive);
  RUN_TEST(test_enter_device_info_from_main_menu);
  RUN_TEST(test_device_info_enter_or_back_returns_to_main_menu);
  RUN_TEST(test_enter_factory_reset_confirm_then_back_cancels);
  RUN_TEST(test_factory_reset_confirmed_via_enter);
  RUN_TEST(test_trigger_calibration_full_flow);
  RUN_TEST(test_back_during_trigger_calibration_cancels_and_resets);
  RUN_TEST(test_stick_calibration_full_flow);
  RUN_TEST(test_tick_is_noop_outside_rolling_stick_calibration);
  RUN_TEST(test_back_during_stick_calibration_cancels_and_resets);
  RUN_TEST(test_switch_droid_list_empty_stays_put_on_enter);
  RUN_TEST(test_switch_droid_navigates_and_reports_no_transport_by_default);
  RUN_TEST(test_switch_droid_list_up_wraps_and_back_returns_to_main_menu);
  RUN_TEST(test_switch_droid_succeeds_with_transport_set);
  RUN_TEST(test_manage_droids_add_flow_creates_entry);
  RUN_TEST(test_manage_droids_back_with_empty_name_cancels_add);
  RUN_TEST(test_manage_droids_back_with_text_backspaces_instead_of_cancelling);
  RUN_TEST(test_manage_droids_list_navigation_wraps_over_entries_and_add_new);
  RUN_TEST(test_manage_droids_pan_id_backspace_and_cancel);
  RUN_TEST(test_manage_droids_delete_flow_removes_entry);
  RUN_TEST(test_manage_droids_delete_confirm_back_cancels);
  RUN_TEST(test_main_menu_item_labels);
  RUN_TEST(test_render_inactive_leaves_screen_blank);
  RUN_TEST(test_render_main_menu_marks_selected_item);
  RUN_TEST(test_render_trigger_calibration_step_text);
  RUN_TEST(test_render_stick_calibration_awaiting_center_step_text);
  RUN_TEST(test_render_stick_calibration_rolling_step_text);
  RUN_TEST(test_render_device_info);
  RUN_TEST(test_render_factory_reset_confirm);
  RUN_TEST(test_render_switch_droid_list_empty);
  RUN_TEST(test_render_switch_droid_list_marks_selected);
  RUN_TEST(test_render_switch_droid_result);
  RUN_TEST(test_render_manage_droids_list_shows_add_new);
  RUN_TEST(test_render_manage_droids_enter_name_shows_highlight);
  RUN_TEST(test_render_manage_droids_enter_name_shows_done);
  RUN_TEST(test_render_manage_droids_enter_pan_id);
  RUN_TEST(test_render_switch_droid_result_variants);
  RUN_TEST(test_render_manage_droids_delete_confirm_shows_name);
  return UNITY_END();
}
