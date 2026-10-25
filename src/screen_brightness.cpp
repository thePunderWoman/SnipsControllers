#include "screen_brightness.h"

#include "brightness_level.h"

namespace {

// SSD1306 contrast steps are much subtler than their numbers suggest
// (perceived brightness changes slowly with contrast). Tune on hardware.
constexpr int kMaxContrast = 0xFF;

// What Adafruit_SSD1306::begin(SSD1306_SWITCHCAPVCC) sets.
constexpr uint8_t kNormalPrecharge = 0xF1;
constexpr uint8_t kNormalVcomh = 0x40;

// Dim: lowest contrast, one-clock precharge phases, VCOMH ~0.65 x VCC.
// Tune on hardware.
constexpr ScreenDrive kDimDrive = {0x00, 0x11, 0x00};

}  // namespace

ScreenDrive screenDriveFor(int brightnessPercent, bool dimmed) {
  if (dimmed) return kDimDrive;
  const int percent = BrightnessLevel::sanitize(brightnessPercent);
  const uint8_t contrast = static_cast<uint8_t>(
      (kMaxContrast * percent + BrightnessLevel::kMaxPercent / 2) /
      BrightnessLevel::kMaxPercent);
  return {contrast, kNormalPrecharge, kNormalVcomh};
}
