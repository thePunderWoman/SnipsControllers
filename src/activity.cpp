#include "activity.h"

#include <cstdio>
#include <cstdlib>

namespace {

struct SourceName {
  uint8_t bit;
  const char *name;
};

constexpr SourceName kSourceNames[] = {
    {ActivitySources::kButton, "button"},
    {ActivitySources::kStick, "stick"},
    {ActivitySources::kTrigger, "trigger"},
    {ActivitySources::kMotion, "motion"},
};

}  // namespace

void describeActivitySources(uint8_t mask, char *out, size_t outCapacity) {
  if (outCapacity == 0) return;
  out[0] = '\0';
  if (mask == 0) {
    std::snprintf(out, outCapacity, "none");
    return;
  }
  size_t used = 0;
  for (const SourceName &source : kSourceNames) {
    if ((mask & source.bit) == 0) continue;
    const int written = std::snprintf(out + used, outCapacity - used, "%s%s",
                                      used == 0 ? "" : ", ", source.name);
    if (written < 0 || used + written >= outCapacity) return;  // truncated
    used += written;
  }
}

bool isTriggerCalibrated(const CalibrationData &data) {
  const CalibrationData defaults;
  return data.triggerMin != defaults.triggerMin ||
         data.triggerMax != defaults.triggerMax;
}

bool isStickCalibrated(const CalibrationData &data) {
  const CalibrationData defaults;
  return data.stickXMin != defaults.stickXMin ||
         data.stickXMax != defaults.stickXMax ||
         data.stickXCenter != defaults.stickXCenter ||
         data.stickYMin != defaults.stickYMin ||
         data.stickYMax != defaults.stickYMax ||
         data.stickYCenter != defaults.stickYCenter;
}

void ActivityDetector::setCalibration(const CalibrationData &data) {
  stickCalibrated_ = isStickCalibrated(data);
  triggerCalibrated_ = isTriggerCalibrated(data);
  hasStickReference_ = false;
}

bool ActivityDetector::stickActive(const ActivityInputs &inputs) {
  if (stickCalibrated_) {
    return std::abs(inputs.stickXPercent) >= kHeldPercent ||
           std::abs(inputs.stickYPercent) >= kHeldPercent;
  }
  // The first reading just sets where the stick rests.
  if (!hasStickReference_ ||
      std::abs(inputs.rawStickX - stickReferenceX_) >=
          kUncalibratedMoveCounts ||
      std::abs(inputs.rawStickY - stickReferenceY_) >=
          kUncalibratedMoveCounts) {
    const bool moved = hasStickReference_;
    hasStickReference_ = true;
    stickReferenceX_ = inputs.rawStickX;
    stickReferenceY_ = inputs.rawStickY;
    return moved;
  }
  return false;
}

uint8_t ActivityDetector::update(const ActivityInputs &inputs) {
  uint8_t active = 0;
  if (inputs.anyButtonHeld) active |= ActivitySources::kButton;
  if (stickActive(inputs)) active |= ActivitySources::kStick;
  if (triggerCalibrated_ && inputs.triggerPercent >= kHeldPercent) {
    active |= ActivitySources::kTrigger;
  }
  if (inputs.motion) active |= ActivitySources::kMotion;

  newlyActive_ = active & ~previousActive_;
  previousActive_ = active;
  return active;
}
