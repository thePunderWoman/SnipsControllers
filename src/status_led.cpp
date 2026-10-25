#include "status_led.h"

#include "brightness_level.h"

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

// The scale at 100%: about 30% duty cycle. LED current scales with duty
// cycle, while perceived brightness falls off much more slowly. Full
// duty was uncomfortably bright on hardware. Tune on hardware.
constexpr int kMaxScale = 77;

uint8_t scaleChannel(uint8_t value, uint8_t scale) {
  return static_cast<uint8_t>((value * scale + 127) / 255);
}

}  // namespace

uint8_t ledBrightnessScale(int brightnessPercent) {
  const int percent = BrightnessLevel::sanitize(brightnessPercent);
  return static_cast<uint8_t>(
      (kMaxScale * percent + BrightnessLevel::kMaxPercent / 2) /
      BrightnessLevel::kMaxPercent);
}

RgbColor scaleColor(const RgbColor &color, uint8_t scale) {
  return {scaleChannel(color.r, scale), scaleChannel(color.g, scale),
          scaleChannel(color.b, scale)};
}
