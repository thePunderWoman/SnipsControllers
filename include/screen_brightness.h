#pragma once

#include <cstdint>

// The SSD1306 drive settings for a screen brightness. Knows nothing about
// the real display — OledDisplay::setDrive() sends them.
struct ScreenDrive {
  uint8_t contrast;   // 0x81 command, 0-255.
  uint8_t precharge;  // 0xD9 command: phase 2 in the high nibble, phase 1 low.
  uint8_t vcomh;      // 0xDB command: VCOMH deselect level.

  bool operator==(const ScreenDrive &other) const {
    return contrast == other.contrast && precharge == other.precharge &&
           vcomh == other.vcomh;
  }
};

// The drive for a screen brightness percentage (see brightness_level.h).
// Contrast scales linearly up to 0xFF at 100%; the default, 80%, is close
// to the 0xCF that Adafruit_SSD1306 sets at begin(), so the default look
// is about unchanged. Precharge and VCOMH stay at Adafruit_SSD1306's
// internal-charge-pump values, so the setting is contrast alone.
//
// Low-power mode's dim stage ignores the setting: contrast 0 alone still
// looked fairly bright (Adafruit_SSD1306::dim() does only that), so dim
// also shortens precharge and drops VCOMH to its lowest level, aiming for
// roughly a tenth of normal brightness. Waking returns to the setting's
// own drive. An off-grid or out-of-range percentage is sanitized first.
ScreenDrive screenDriveFor(int brightnessPercent, bool dimmed);
