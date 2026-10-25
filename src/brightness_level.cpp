#include "brightness_level.h"

namespace BrightnessLevel {

int sanitize(int percent) {
  if (percent <= kMinPercent) return kMinPercent;
  if (percent >= kMaxPercent) return kMaxPercent;
  return (percent + kStepPercent / 2) / kStepPercent * kStepPercent;
}

int increase(int percent) { return sanitize(sanitize(percent) + kStepPercent); }

int decrease(int percent) { return sanitize(sanitize(percent) - kStepPercent); }

int cycle(int percent) {
  const int current = sanitize(percent);
  return current >= kMaxPercent ? kMinPercent : current + kStepPercent;
}

}  // namespace BrightnessLevel
