#include "menu.h"

#include <cstdio>

const char *mainMenuItemLabel(MainMenuItem item) {
  switch (item) {
    case MainMenuItem::kCalibrateStick: return "Calibrate Stick";
    case MainMenuItem::kCalibrateTrigger: return "Calibrate Trigger";
    case MainMenuItem::kDeviceInfo: return "Device Info";
    case MainMenuItem::kFactoryReset: return "Factory Reset";
    default: return "Unknown";
  }
}

MainMenuItem MenuController::selectedMainMenuItem() const {
  return static_cast<MainMenuItem>(mainMenuIndex_);
}

void MenuController::open() {
  if (screen_ != MenuScreen::kInactive) return;
  screen_ = MenuScreen::kMainMenu;
  mainMenuIndex_ = 0;
}

void MenuController::updateOpenCombo(bool comboButtonAPressed,
                                     bool comboButtonBPressed,
                                     unsigned long nowMs) {
  if (comboButtonAPressed && comboButtonBPressed) {
    if (!comboHeld_) {
      comboHeld_ = true;
      comboStartMs_ = nowMs;
    } else if (nowMs - comboStartMs_ >= kOpenComboHoldMs) {
      open();
      comboHeld_ = false;  // avoid immediately retriggering
    }
  } else {
    comboHeld_ = false;
  }
}

void MenuController::onUp() {
  if (screen_ != MenuScreen::kMainMenu) return;
  const int count = static_cast<int>(MainMenuItem::kCount);
  mainMenuIndex_ = (mainMenuIndex_ - 1 + count) % count;
}

void MenuController::onDown() {
  if (screen_ != MenuScreen::kMainMenu) return;
  const int count = static_cast<int>(MainMenuItem::kCount);
  mainMenuIndex_ = (mainMenuIndex_ + 1) % count;
}

void MenuController::onBack() {
  switch (screen_) {
    case MenuScreen::kMainMenu:
      screen_ = MenuScreen::kInactive;
      break;
    case MenuScreen::kCalibrateStick:
      stickFlow_ = StickCalibrationFlow();
      screen_ = MenuScreen::kMainMenu;
      break;
    case MenuScreen::kCalibrateTrigger:
      triggerFlow_ = TriggerCalibrationFlow();
      screen_ = MenuScreen::kMainMenu;
      break;
    case MenuScreen::kDeviceInfo:
    case MenuScreen::kFactoryResetConfirm:
      screen_ = MenuScreen::kMainMenu;
      break;
    case MenuScreen::kInactive:
      break;
  }
}

void MenuController::enterMainMenuItem(MainMenuItem item) {
  switch (item) {
    case MainMenuItem::kCalibrateStick:
      stickFlow_ = StickCalibrationFlow();
      screen_ = MenuScreen::kCalibrateStick;
      break;
    case MainMenuItem::kCalibrateTrigger:
      triggerFlow_ = TriggerCalibrationFlow();
      screen_ = MenuScreen::kCalibrateTrigger;
      break;
    case MainMenuItem::kDeviceInfo:
      screen_ = MenuScreen::kDeviceInfo;
      break;
    case MainMenuItem::kFactoryReset:
      screen_ = MenuScreen::kFactoryResetConfirm;
      break;
    case MainMenuItem::kCount:
      break;
  }
}

void MenuController::onEnter(int rawTrigger, int rawStickX, int rawStickY) {
  switch (screen_) {
    case MenuScreen::kMainMenu:
      enterMainMenuItem(selectedMainMenuItem());
      break;

    case MenuScreen::kCalibrateTrigger:
      triggerFlow_.confirmStep(rawTrigger);
      if (triggerFlow_.currentStep() ==
          TriggerCalibrationFlow::Step::kDone) {
        hasNewTriggerCalibration_ = true;
        pendingTriggerMin_ = triggerFlow_.min();
        pendingTriggerMax_ = triggerFlow_.max();
        screen_ = MenuScreen::kMainMenu;
      }
      break;

    case MenuScreen::kCalibrateStick:
      if (stickFlow_.currentStep() ==
          StickCalibrationFlow::Step::kAwaitingCenter) {
        stickFlow_.confirmCenter(rawStickX, rawStickY);
      } else if (stickFlow_.currentStep() ==
                 StickCalibrationFlow::Step::kRolling) {
        stickFlow_.confirmDone();
        if (stickFlow_.currentStep() == StickCalibrationFlow::Step::kDone) {
          hasNewStickCalibration_ = true;
          pendingStickCenterX_ = stickFlow_.centerX();
          pendingStickCenterY_ = stickFlow_.centerY();
          pendingStickMinX_ = stickFlow_.minX();
          pendingStickMaxX_ = stickFlow_.maxX();
          pendingStickMinY_ = stickFlow_.minY();
          pendingStickMaxY_ = stickFlow_.maxY();
          screen_ = MenuScreen::kMainMenu;
        }
      }
      break;

    case MenuScreen::kDeviceInfo:
      screen_ = MenuScreen::kMainMenu;
      break;

    case MenuScreen::kFactoryResetConfirm:
      factoryResetConfirmed_ = true;
      screen_ = MenuScreen::kMainMenu;
      break;

    case MenuScreen::kInactive:
      break;
  }
}

void MenuController::tick(int rawStickX, int rawStickY) {
  if (screen_ == MenuScreen::kCalibrateStick &&
      stickFlow_.currentStep() == StickCalibrationFlow::Step::kRolling) {
    stickFlow_.sample(rawStickX, rawStickY);
  }
}

bool MenuController::consumeNewTriggerCalibration(int *outMin, int *outMax) {
  if (!hasNewTriggerCalibration_) return false;
  *outMin = pendingTriggerMin_;
  *outMax = pendingTriggerMax_;
  hasNewTriggerCalibration_ = false;
  return true;
}

bool MenuController::consumeNewStickCalibration(int *outCenterX,
                                                 int *outCenterY,
                                                 int *outMinX, int *outMaxX,
                                                 int *outMinY, int *outMaxY) {
  if (!hasNewStickCalibration_) return false;
  *outCenterX = pendingStickCenterX_;
  *outCenterY = pendingStickCenterY_;
  *outMinX = pendingStickMinX_;
  *outMaxX = pendingStickMaxX_;
  *outMinY = pendingStickMinY_;
  *outMaxY = pendingStickMaxY_;
  hasNewStickCalibration_ = false;
  return true;
}

bool MenuController::consumeFactoryResetConfirmed() {
  if (!factoryResetConfirmed_) return false;
  factoryResetConfirmed_ = false;
  return true;
}

void renderMenuScreen(const MenuController &menu, ScreenBuffer *screen) {
  screen->clear();

  switch (menu.currentScreen()) {
    case MenuScreen::kInactive:
      break;  // caller decides what else to show when the menu is closed

    case MenuScreen::kMainMenu: {
      screen->setLine(0, "== Menu ==");
      char line[ScreenBuffer::kMaxLineLength + 1];
      for (int i = 0; i < static_cast<int>(MainMenuItem::kCount); ++i) {
        const auto item = static_cast<MainMenuItem>(i);
        std::snprintf(line, sizeof(line), "%s%s",
                      i == static_cast<int>(menu.selectedMainMenuItem())
                          ? "> "
                          : "  ",
                      mainMenuItemLabel(item));
        screen->setLine(1 + i, line);
      }
      break;
    }

    case MenuScreen::kCalibrateTrigger:
      screen->setLine(0, "Calibrate Trigger");
      switch (menu.triggerCalibrationStep()) {
        case TriggerCalibrationFlow::Step::kAwaitingRelease:
          screen->setLine(1, "Release trigger,");
          screen->setLine(2, "press Enter");
          break;
        case TriggerCalibrationFlow::Step::kAwaitingFullPull:
          screen->setLine(1, "Pull fully,");
          screen->setLine(2, "press Enter");
          break;
        case TriggerCalibrationFlow::Step::kDone:
          screen->setLine(1, "Done!");
          break;
      }
      break;

    case MenuScreen::kCalibrateStick:
      screen->setLine(0, "Calibrate Stick");
      switch (menu.stickCalibrationStep()) {
        case StickCalibrationFlow::Step::kAwaitingCenter:
          screen->setLine(1, "Center stick,");
          screen->setLine(2, "press Enter");
          break;
        case StickCalibrationFlow::Step::kRolling:
          screen->setLine(1, "Roll to extremes,");
          screen->setLine(2, "Enter when done");
          break;
        case StickCalibrationFlow::Step::kDone:
          screen->setLine(1, "Done!");
          break;
      }
      break;

    case MenuScreen::kDeviceInfo:
      screen->setLine(0, "Device Info");
      screen->setLine(1, "XBee SL:");
      screen->setLine(2, "(needs PR 8)");
      break;

    case MenuScreen::kFactoryResetConfirm:
      screen->setLine(0, "Factory Reset?");
      screen->setLine(1, "Enter = confirm");
      screen->setLine(2, "Back = cancel");
      break;
  }
}
