#include "power_latch.h"

PowerOffDetector::PowerOffDetector(unsigned long holdThresholdMs)
    : holdThresholdMs_(holdThresholdMs) {}

bool PowerOffDetector::update(bool buttonPressed, unsigned long nowMs) {
  if (!buttonPressed) {
    wasPressed_ = false;
    firedForThisHold_ = false;
    return false;
  }

  if (!wasPressed_) {
    wasPressed_ = true;
    pressStartMs_ = nowMs;
    firedForThisHold_ = false;
    // A threshold of 0 means "fire immediately on press" — handled here so
    // the very first tick of a press can still cross it.
  }

  if (firedForThisHold_) {
    return false;
  }

  if (nowMs - pressStartMs_ >= holdThresholdMs_) {
    firedForThisHold_ = true;
    return true;
  }

  return false;
}
