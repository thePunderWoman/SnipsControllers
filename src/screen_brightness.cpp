#include "screen_brightness.h"

namespace {

// SSD1306 contrast steps are much subtler than their numbers suggest
// (perceived brightness changes slowly with contrast), so the levels are
// spread wide. Tune on hardware.
constexpr uint8_t kDimContrast = 0x00;
constexpr uint8_t kLowContrast = 0x10;
constexpr uint8_t kMediumContrast = 0x60;
constexpr uint8_t kHighContrast = 0xCF;

}  // namespace

ScreenBrightness nextScreenBrightness(ScreenBrightness current) {
  const int next = static_cast<int>(current) + 1;
  if (next <= 0 || next >= static_cast<int>(ScreenBrightness::kCount)) {
    return ScreenBrightness::kLow;
  }
  return static_cast<ScreenBrightness>(next);
}

uint8_t screenContrastFor(ScreenBrightness brightness, bool dimmed) {
  if (dimmed) return kDimContrast;
  switch (brightness) {
    case ScreenBrightness::kLow: return kLowContrast;
    case ScreenBrightness::kMedium: return kMediumContrast;
    case ScreenBrightness::kHigh:
    default: return kHighContrast;
  }
}
