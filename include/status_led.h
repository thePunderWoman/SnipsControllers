#pragma once

#include <cstdint>

// Pure logic mapping overall system state to an RGB status color. Knows
// nothing about the real LED hardware — src/rgb_led.cpp is the thin
// adapter that actually drives it.
enum class SystemState {
  kBooting,
  kConnected,
  kDisconnected,
  kCharging,
  kError,
};

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

class StatusLedController {
 public:
  RgbColor colorFor(SystemState state) const;
};

// The 0-255 scale factor a status LED brightness percentage (see
// brightness_level.h) applies to every channel. Full drive costs ~12-24
// mA depending on color (one or two LED channels lit), a large share of
// the whole controller's draw, and was too bright, so 100% is capped well
// below it (about 30% duty) and the default is 10%. Scales linearly in
// between. An off-grid or out-of-range percentage is sanitized first.
uint8_t ledBrightnessScale(int brightnessPercent);

// Scales each channel by scale/255, rounded to nearest. A scale of 255
// returns the color unchanged.
RgbColor scaleColor(const RgbColor &color, uint8_t scale);
