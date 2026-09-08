#pragma once

#include "calibration.h"
#include "droid_store.h"
#include "screen.h"
#include "text_entry.h"
#include "droid_switcher.h"

// Pure on-device menu state machine. Knows nothing about real buttons or
// the display — SnipsController.ino translates physical button edges into
// the nav calls below (and detects the open combo via updateOpenCombo()),
// and renderMenuScreen() turns the current state into a ScreenBuffer for
// the (already-existing, hardware-touching) OledDisplay to draw.
//
// Display Config (PR 9) isn't part of this menu tree yet — it needs the
// packet protocol's label/value data, which doesn't exist yet.
enum class MenuScreen {
  kInactive,
  kMainMenu,
  kSwitchDroidList,
  kSwitchDroidResult,
  kManageDroidsList,
  kManageDroidsEnterName,
  kManageDroidsEnterPanId,
  kManageDroidsDeleteConfirm,
  kCalibrateStick,
  kCalibrateTrigger,
  kDeviceInfo,
  kFactoryResetConfirm,
};

enum class MainMenuItem {
  kSwitchDroid = 0,
  kManageDroids,
  kCalibrateStick,
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
  // button-press edge (not while held). Meaning depends on the current
  // screen: list navigation on list screens, character scroll on text
  // entry screens.
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

  // Droid management — the store is owned here so rendering/navigation
  // can see it directly; SnipsController.ino restores it from
  // DroidPersistence once at boot and re-persists it whenever
  // consumeDroidStoreChanged() reports a change.
  void setDroidStore(const DroidStore &store) { droidStore_ = store; }
  const DroidStore &droidStore() const { return droidStore_; }
  bool consumeDroidStoreChanged();

  // Switch Droid needs a way to actually talk to the radio — set once at
  // boot. May be left null (switching then always reports kNoTransport).
  void setXbeeTransport(XbeeTransport *transport) {
    xbeeTransport_ = transport;
  }

  // The XBee's own SL (queried once at boot, since it's a fixed hardware
  // address) for the Device Info screen — plain string storage, no
  // hardware access here.
  void setDeviceSerialLow(const char *hex) { deviceSerialLow_ = hex; }
  const char *deviceSerialLow() const { return deviceSerialLow_; }

  int selectedDroidListIndex() const { return droidListIndex_; }
  DroidSwitchResult lastSwitchResult() const { return lastSwitchResult_; }
  const TextEntryWidget &nameEntry() const { return nameEntry_; }
  const TextEntryWidget &panIdEntry() const { return panIdEntry_; }

 private:
  static constexpr unsigned long kOpenComboHoldMs = 1000;

  void open();
  void enterMainMenuItem(MainMenuItem item);
  static int wrapIndex(int index, int count);

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

  DroidStore droidStore_;
  int droidListIndex_ = 0;
  bool droidStoreChanged_ = false;
  TextEntryWidget nameEntry_{TextEntryWidget::CharSet::kAlphanumeric};
  TextEntryWidget panIdEntry_{TextEntryWidget::CharSet::kHex};
  DroidSwitchResult lastSwitchResult_ = DroidSwitchResult::kSuccess;
  XbeeTransport *xbeeTransport_ = nullptr;
  const char *deviceSerialLow_ = "(unknown)";
};

// Decides what text should be on screen for the menu's current state.
// Pure — takes no display dependency, just fills in a ScreenBuffer.
void renderMenuScreen(const MenuController &menu, ScreenBuffer *screen);
