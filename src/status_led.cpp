#include "status_led.h"

RgbColor StatusLedController::colorFor(SystemState state) const {
  switch (state) {
    case SystemState::kBooting:
      return {0, 0, 255};        // blue
    case SystemState::kConnected:
      return {0, 255, 0};        // green
    case SystemState::kDisconnected:
      return {255, 255, 0};      // yellow
    case SystemState::kCharging:
      return {255, 165, 0};      // orange
    case SystemState::kError:
      return {255, 0, 0};        // red
  }
  return {0, 0, 0};
}

namespace {

// Roughly 4% / 12% / 30% duty cycle. LED current scales with duty
// cycle, while perceived brightness falls off much more slowly. Full
// duty was uncomfortably bright on hardware, so even kHigh stays well
// below it; was 20% / 50% / 100%. Tune on hardware.
constexpr uint8_t kLowScale = 10;
constexpr uint8_t kMediumScale = 31;
constexpr uint8_t kHighScale = 77;

uint8_t scaleChannel(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>((value * scale + 127) / 255);
}

}  // namespace

LedBrightness nextLedBrightness(LedBrightness current) {
  const int count = static_cast<int>(LedBrightness::kCount);
  const int index = static_cast<int>(current);
  if (index < 0 || index >= count) {
    return LedBrightness::kLow;
  }
  return static_cast<LedBrightness>((index + 1) % count);
}

uint8_t ledBrightnessScale(LedBrightness brightness) {
  switch (brightness) {
    case LedBrightness::kMedium: return kMediumScale;
    case LedBrightness::kHigh: return kHighScale;
    case LedBrightness::kLow:
    default: return kLowScale;
  }
}

RgbColor scaleColor(const RgbColor &color, uint8_t scale) {
  return {scaleChannel(color.r, scale), scaleChannel(color.g, scale),
          scaleChannel(color.b, scale)};
}
