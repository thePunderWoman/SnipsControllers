#include "menu.h"

#include <cstdio>

const char *mainMenuItemLabel(MainMenuItem item) {
  switch (item) {
    case MainMenuItem::kSwitchDroid: return "Switch Droid";
    case MainMenuItem::kManageDroids: return "Manage Droids";
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

int MenuController::wrapIndex(int index, int count) {
  if (count <= 0) return 0;
  return (index % count + count) % count;
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
  switch (screen_) {
    case MenuScreen::kMainMenu:
      mainMenuIndex_ = wrapIndex(mainMenuIndex_ - 1,
                                static_cast<int>(MainMenuItem::kCount));
      break;
    case MenuScreen::kSwitchDroidList:
      if (droidStore_.count() > 0) {
        droidListIndex_ = wrapIndex(droidListIndex_ - 1,
                                    static_cast<int>(droidStore_.count()));
      }
      break;
    case MenuScreen::kManageDroidsList:
      droidListIndex_ = wrapIndex(
          droidListIndex_ - 1, static_cast<int>(droidStore_.count()) + 1);
      break;
    case MenuScreen::kManageDroidsEnterName:
      nameEntry_.scrollPrev();
      break;
    case MenuScreen::kManageDroidsEnterPanId:
      panIdEntry_.scrollPrev();
      break;
    default:
      break;
  }
}

void MenuController::onDown() {
  switch (screen_) {
    case MenuScreen::kMainMenu:
      mainMenuIndex_ = wrapIndex(mainMenuIndex_ + 1,
                                static_cast<int>(MainMenuItem::kCount));
      break;
    case MenuScreen::kSwitchDroidList:
      if (droidStore_.count() > 0) {
        droidListIndex_ = wrapIndex(droidListIndex_ + 1,
                                    static_cast<int>(droidStore_.count()));
      }
      break;
    case MenuScreen::kManageDroidsList:
      droidListIndex_ = wrapIndex(
          droidListIndex_ + 1, static_cast<int>(droidStore_.count()) + 1);
      break;
    case MenuScreen::kManageDroidsEnterName:
      nameEntry_.scrollNext();
      break;
    case MenuScreen::kManageDroidsEnterPanId:
      panIdEntry_.scrollNext();
      break;
    default:
      break;
  }
}

void MenuController::onBack() {
  switch (screen_) {
    case MenuScreen::kMainMenu:
      screen_ = MenuScreen::kInactive;
      break;
    case MenuScreen::kSwitchDroidList:
    case MenuScreen::kSwitchDroidResult:
    case MenuScreen::kManageDroidsList:
      screen_ = MenuScreen::kMainMenu;
      break;
    case MenuScreen::kManageDroidsDeleteConfirm:
      screen_ = MenuScreen::kManageDroidsList;
      break;
    case MenuScreen::kManageDroidsEnterName:
      if (nameEntry_.length() > 0) {
        nameEntry_.backspace();
      } else {
        screen_ = MenuScreen::kManageDroidsList;
      }
      break;
    case MenuScreen::kManageDroidsEnterPanId:
      if (panIdEntry_.length() > 0) {
        panIdEntry_.backspace();
      } else {
        screen_ = MenuScreen::kManageDroidsList;
      }
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
    case MainMenuItem::kSwitchDroid:
      droidListIndex_ = 0;
      screen_ = MenuScreen::kSwitchDroidList;
      break;
    case MainMenuItem::kManageDroids:
      droidListIndex_ = 0;
      screen_ = MenuScreen::kManageDroidsList;
      break;
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

    case MenuScreen::kSwitchDroidList:
      if (droidStore_.count() > 0) {
        lastSwitchResult_ = DroidSwitcher::switchTo(
            droidStore_.at(droidListIndex_).panId, xbeeTransport_);
        screen_ = MenuScreen::kSwitchDroidResult;
      }
      break;

    case MenuScreen::kSwitchDroidResult:
      screen_ = MenuScreen::kMainMenu;
      break;

    case MenuScreen::kManageDroidsList:
      if (droidListIndex_ == static_cast<int>(droidStore_.count())) {
        nameEntry_.reset(TextEntryWidget::CharSet::kAlphanumeric,
                         DroidEntry::kMaxNameLength);
        screen_ = MenuScreen::kManageDroidsEnterName;
      } else if (droidStore_.count() > 0) {
        screen_ = MenuScreen::kManageDroidsDeleteConfirm;
      }
      break;

    case MenuScreen::kManageDroidsEnterName:
      nameEntry_.commitChar();
      if (nameEntry_.done()) {
        panIdEntry_.reset(TextEntryWidget::CharSet::kHex,
                          DroidEntry::kMaxPanIdLength);
        screen_ = MenuScreen::kManageDroidsEnterPanId;
      }
      break;

    case MenuScreen::kManageDroidsEnterPanId:
      panIdEntry_.commitChar();
      if (panIdEntry_.done()) {
        droidStore_.add(nameEntry_.text(), panIdEntry_.text());
        droidStoreChanged_ = true;
        droidListIndex_ = 0;
        screen_ = MenuScreen::kManageDroidsList;
      }
      break;

    case MenuScreen::kManageDroidsDeleteConfirm:
      droidStore_.remove(droidListIndex_);
      droidStoreChanged_ = true;
      droidListIndex_ = 0;
      screen_ = MenuScreen::kManageDroidsList;
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
      droidStore_ = DroidStore();
      droidStoreChanged_ = true;
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

bool MenuController::consumeDroidStoreChanged() {
  if (!droidStoreChanged_) return false;
  droidStoreChanged_ = false;
  return true;
}

namespace {

void renderTextEntryLine(const TextEntryWidget &widget, ScreenBuffer *screen,
                         int lineIndex) {
  char line[ScreenBuffer::kMaxLineLength + 1];
  if (widget.isDoneSelected()) {
    std::snprintf(line, sizeof(line), "%s[DONE]", widget.text());
  } else {
    std::snprintf(line, sizeof(line), "%s[%c]", widget.text(),
                  widget.currentChar());
  }
  screen->setLine(lineIndex, line);
}

const char *switchResultText(DroidSwitchResult result) {
  switch (result) {
    case DroidSwitchResult::kSuccess: return "Success!";
    case DroidSwitchResult::kLeaveFailed: return "Leave failed";
    case DroidSwitchResult::kSetPanFailed: return "Set PAN failed";
    case DroidSwitchResult::kRejoinFailed: return "Rejoin failed";
    case DroidSwitchResult::kNoTransport: return "No XBee link (PR 8)";
    default: return "Unknown";
  }
}

}  // namespace

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

    case MenuScreen::kSwitchDroidList: {
      screen->setLine(0, "Switch Droid");
      if (menu.droidStore().count() == 0) {
        screen->setLine(1, "No droids saved");
        break;
      }
      char line[ScreenBuffer::kMaxLineLength + 1];
      // Only as many entries as fit fit on screen below the header — see
      // DroidStore::kMaxDroids vs. ScreenBuffer::kMaxLines.
      const size_t visible =
          menu.droidStore().count() < ScreenBuffer::kMaxLines - 1
              ? menu.droidStore().count()
              : ScreenBuffer::kMaxLines - 1;
      for (size_t i = 0; i < visible; ++i) {
        std::snprintf(line, sizeof(line), "%s%s",
                      static_cast<int>(i) == menu.selectedDroidListIndex()
                          ? "> "
                          : "  ",
                      menu.droidStore().at(i).name);
        screen->setLine(1 + i, line);
      }
      break;
    }

    case MenuScreen::kSwitchDroidResult:
      screen->setLine(0, "Switch Droid");
      screen->setLine(1, switchResultText(menu.lastSwitchResult()));
      screen->setLine(2, "Press Enter");
      break;

    case MenuScreen::kManageDroidsList: {
      screen->setLine(0, "Manage Droids");
      char line[ScreenBuffer::kMaxLineLength + 1];
      const size_t count = menu.droidStore().count();
      const size_t visible =
          count + 1 < ScreenBuffer::kMaxLines - 1
              ? count + 1
              : ScreenBuffer::kMaxLines - 1;
      for (size_t i = 0; i < visible; ++i) {
        const char *label =
            i < count ? menu.droidStore().at(i).name : "+ Add New";
        std::snprintf(line, sizeof(line), "%s%s",
                      static_cast<int>(i) == menu.selectedDroidListIndex()
                          ? "> "
                          : "  ",
                      label);
        screen->setLine(1 + i, line);
      }
      break;
    }

    case MenuScreen::kManageDroidsEnterName:
      screen->setLine(0, "Add Droid: Name");
      renderTextEntryLine(menu.nameEntry(), screen, 1);
      break;

    case MenuScreen::kManageDroidsEnterPanId:
      screen->setLine(0, "Add Droid: PAN ID");
      renderTextEntryLine(menu.panIdEntry(), screen, 1);
      break;

    case MenuScreen::kManageDroidsDeleteConfirm:
      screen->setLine(0, "Delete Droid?");
      screen->setLine(1, menu.droidStore().at(menu.selectedDroidListIndex()).name);
      screen->setLine(2, "Enter = confirm");
      screen->setLine(3, "Back = cancel");
      break;

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
