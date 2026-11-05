#pragma once

#include "frame_buffer.h"

// The status screens' text font: 5x7 glyphs on a 6 px advance, the same
// metrics as Adafruit_GFX's default font (which the menu screens use), so
// both kinds of screen read alike. Covers printable ASCII; anything else
// draws as '?'. Each size multiplies every pixel, the way
// Adafruit_GFX::setTextSize() does: size 2 fits 10 characters across the
// 128 px screen.
namespace StatusFont {

constexpr int kGlyphWidth = 5;
constexpr int kGlyphHeight = 7;
constexpr int kAdvance = 6;

// Width in pixels of `text` at `size`, without the trailing gap.
int textWidth(const char *text, int size = 1);

// The most characters of a `size` line that fit in `maxWidth` pixels.
int maxChars(int maxWidth, int size = 1);

// Draws `text` with its top-left corner at (x, y). Only glyph pixels are
// drawn, so text can sit on a filled area when drawn with on = false.
void drawText(FrameBuffer *frame, int x, int y, const char *text,
              int size = 1, bool on = true);

}  // namespace StatusFont
