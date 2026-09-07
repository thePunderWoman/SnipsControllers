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
