#include "buttons.h"

const char *Buttons::name(size_t index) {
  switch (index) {
    case Buttons::kMacro1: return "Macro1";
    case Buttons::kMacro2: return "Macro2";
    case Buttons::kMacro3: return "Macro3";
    case Buttons::kMacro4: return "Macro4";
    case Buttons::kMacro5: return "Macro5";
    case Buttons::kMacro6: return "Macro6";
    case Buttons::kBumper: return "Bumper";
    case Buttons::kStickClick: return "StickClick";
    case Buttons::kLeftUp: return "LeftUp";
    case Buttons::kLeftDown: return "LeftDown";
    case Buttons::kRightUp: return "RightUp";
    case Buttons::kRightDown: return "RightDown";
    default: return "Unknown";
  }
}

ButtonDebouncer::ButtonDebouncer(unsigned long debounceMs)
    : debounceMs_(debounceMs) {}

bool ButtonDebouncer::update(bool rawPressed, unsigned long nowMs) {
  if (rawPressed != lastRaw_) {
    lastRaw_ = rawPressed;
    lastRawChangeMs_ = nowMs;
  } else if (nowMs - lastRawChangeMs_ >= debounceMs_) {
    debouncedState_ = lastRaw_;
  }
  return debouncedState_;
}

void ButtonPanel::update(size_t index, bool rawPressed, unsigned long nowMs) {
  if (index < Buttons::kCount) {
    debouncers_[index].update(rawPressed, nowMs);
  }
}

bool ButtonPanel::isPressed(size_t index) const {
  return index < Buttons::kCount ? debouncers_[index].isPressed() : false;
}
