#pragma once

#include <cstddef>
#include <cstring>

// Pure content model for the OLED — decides *what* text should be on
// screen. Hardware-agnostic: the thin `oled.cpp` adapter just draws
// whatever a ScreenBuffer currently holds. Sized for a 128x64 SSD1306 at
// Adafruit_GFX's default 6x8px font (128/6 = 21 usable columns,
// 64/8 = 8 usable rows).
class ScreenBuffer {
 public:
  static constexpr size_t kMaxLines = 8;
  static constexpr size_t kMaxLineLength = 21;

  // Sets one line's text, truncating if it's too long for the display.
  // Out-of-range indices are ignored.
  void setLine(size_t index, const char *text);

  void clear();

  const char *line(size_t index) const;

  static size_t lineCount() { return kMaxLines; }

 private:
  char lines_[kMaxLines][kMaxLineLength + 1] = {};
};
