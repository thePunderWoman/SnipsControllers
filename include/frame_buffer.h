#pragma once

#include <cstddef>
#include <cstdint>

// Pure 1-bit pixel buffer for the 128x64 OLED, used by the status screens
// (status_screen.h). Hardware-agnostic and unit tested; the thin
// `oled.cpp` adapter copies it to the panel as-is. The bytes are kept in
// the SSD1306's own GDRAM layout (each byte is a column of 8 vertical
// pixels, LSB on top, pages of 8 rows), the same layout as
// Adafruit_SSD1306's internal buffer, so no conversion is needed there.
class FrameBuffer {
 public:
  static constexpr int kWidth = 128;
  static constexpr int kHeight = 64;
  static constexpr size_t kByteCount = kWidth * kHeight / 8;

  void clear();

  // Off-screen coordinates read as off and are ignored on write, so
  // callers can draw shapes that run past an edge.
  bool pixel(int x, int y) const;
  void setPixel(int x, int y, bool on = true);

  void fillRect(int x, int y, int width, int height, bool on = true);
  void drawRect(int x, int y, int width, int height, bool on = true);

  // Draws `pattern`'s '#' cells, one string per row, each cell as a
  // scale x scale block. Other characters are left untouched
  // (transparent), so a pattern can be drawn over anything.
  void drawPattern(int x, int y, const char *const *rows, int rowCount,
                   bool on = true, int scale = 1);

  // Draws a row-major, MSB-first-per-byte bitmap (the format
  // Adafruit_GFX::drawBitmap() takes, and snips_logo_bitmap.h's). Only set
  // bits are drawn.
  void drawBitmap(int x, int y, const uint8_t *bitmap, int width,
                  int height, bool on = true);

  const uint8_t *bytes() const { return bytes_; }

  bool operator==(const FrameBuffer &other) const;
  bool operator!=(const FrameBuffer &other) const { return !(*this == other); }

 private:
  uint8_t bytes_[kByteCount] = {};
};
