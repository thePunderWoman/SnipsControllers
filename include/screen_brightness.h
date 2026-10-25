#pragma once

#include <cstdint>

// User-selectable OLED brightness, cycled from the Brightness menu.
// Knows nothing about the real display — OledDisplay::setContrast() sends
// the value screenContrastFor() picks. Ordered dimmest first, like
// LedBrightness.
enum class ScreenBrightness {
  kLow = 0,
  kMedium,
  kHigh,
  kCount,
};

// Cycles to the next brightness, wrapping around. An out-of-range value
// (e.g. corrupt NVS data) restarts at kLow.
ScreenBrightness nextScreenBrightness(ScreenBrightness current);

// The SSD1306 contrast (0x81 command, 0-255) for a brightness. Low-power
// mode's dim tier always uses the panel's lowest contrast, whatever the
// setting, and waking returns to the setting's own level. kHigh is 0xCF,
// what Adafruit_SSD1306 sets at begin(), so the default look is
// unchanged. An out-of-range value gets kHigh's contrast, the panel's
// normal look.
uint8_t screenContrastFor(ScreenBrightness brightness, bool dimmed);
