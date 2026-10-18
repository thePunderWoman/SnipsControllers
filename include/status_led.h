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

// User-selectable status LED brightness, cycled from the Power Management
// menu. Full brightness costs ~12-24 mA depending on color (one or two
// LED channels lit), a large share of the whole controller's draw, so the
// default is kLow. Ordered dimmest first.
enum class LedBrightness {
  kLow = 0,
  kMedium,
  kHigh,
  kCount,
};

// Cycles to the next brightness, wrapping around. An out-of-range value
// (e.g. corrupt NVS data) restarts at kLow.
LedBrightness nextLedBrightness(LedBrightness current);

// The 0-255 scale factor a brightness applies to every channel. An
// out-of-range value gets kLow's scale, the safe choice for battery life.
uint8_t ledBrightnessScale(LedBrightness brightness);

// Scales each channel by scale/255, rounded to nearest. A scale of 255
// returns the color unchanged.
RgbColor scaleColor(const RgbColor &color, uint8_t scale);
