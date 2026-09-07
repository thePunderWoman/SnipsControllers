#include "buttons.h"

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
