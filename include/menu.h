#pragma once

#include "calibration.h"
#include "screen.h"

// Pure on-device menu state machine. Knows nothing about real buttons or
// the display — SnipsController.ino translates physical button edges into
// the four nav calls below (and detects the open combo via
// updateOpenCombo()), and renderMenuScreen() turns the current state into
// a ScreenBuffer for the (already-existing, hardware-touching) OledDisplay
// to draw.
//
// Only screens with everything they need already built land here: Manage/
// Switch Droid (PR 7) and Display Config (PR 9) aren't part of this menu
// tree yet.
enum class MenuScreen {
  kInactive,
  kMainMenu,
  kCalibrateStick,
  kCalibrateTrigger,
  kDeviceInfo,
  kFactoryResetConfirm,
};

enum class MainMenuItem {
  kCalibrateStick = 0,
  kCalibrateTrigger,
  kDeviceInfo,
  kFactoryReset,
  kCount,
};

const char *mainMenuItemLabel(MainMenuItem item);

class MenuController {
 public:
  MenuScreen currentScreen() const { return screen_; }
  MainMenuItem selectedMainMenuItem() const;

  // Held-combo detection to open the menu from kInactive — call every
  // loop tick with the current debounced state of the two buttons this
  // controller uses to enter the menu, regardless of currentScreen().
  void updateOpenCombo(bool comboButtonAPressed, bool comboButtonBPressed,
                       unsigned long nowMs);

  // Discrete nav events — call at most once per tick, only on a fresh
  // button-press edge (not while held).
  void onUp();
  void onDown();
  void onBack();

  // Enter needs the current raw analog readings so the calibration
  // screens can capture a sample at the moment of confirmation; ignored
  // by every other screen.
  void onEnter(int rawTrigger, int rawStickX, int rawStickY);

  // Call every loop tick regardless of button edges, so the stick
  // calibration's "roll to extremes" step can continuously track
  // min/max. No-op unless currentScreen() == kCalibrateStick and its
  // internal flow is in the rolling step.
  void tick(int rawStickX, int rawStickY);

  TriggerCalibrationFlow::Step triggerCalibrationStep() const {
    return triggerFlow_.currentStep();
  }
  StickCalibrationFlow::Step stickCalibrationStep() const {
    return stickFlow_.currentStep();
  }

  // Each returns true exactly once, the tick a new result becomes ready
  // to persist, and writes it into the output params.
  bool consumeNewTriggerCalibration(int *outMin, int *outMax);
  bool consumeNewStickCalibration(int *outCenterX, int *outCenterY,
                                   int *outMinX, int *outMaxX, int *outMinY,
                                   int *outMaxY);
  bool consumeFactoryResetConfirmed();

 private:
  static constexpr unsigned long kOpenComboHoldMs = 1000;

  void open();
  void enterMainMenuItem(MainMenuItem item);

  MenuScreen screen_ = MenuScreen::kInactive;
  int mainMenuIndex_ = 0;

  bool comboHeld_ = false;
  unsigned long comboStartMs_ = 0;

  TriggerCalibrationFlow triggerFlow_;
  StickCalibrationFlow stickFlow_;

  bool hasNewTriggerCalibration_ = false;
  int pendingTriggerMin_ = 0;
  int pendingTriggerMax_ = 0;

  bool hasNewStickCalibration_ = false;
  int pendingStickCenterX_ = 0;
  int pendingStickCenterY_ = 0;
  int pendingStickMinX_ = 0;
  int pendingStickMaxX_ = 0;
  int pendingStickMinY_ = 0;
  int pendingStickMaxY_ = 0;

  bool factoryResetConfirmed_ = false;
};

// Decides what text should be on screen for the menu's current state.
// Pure — takes no display dependency, just fills in a ScreenBuffer.
void renderMenuScreen(const MenuController &menu, ScreenBuffer *screen);
