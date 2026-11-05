#include "frame_buffer.h"

#include <cstring>

void FrameBuffer::clear() { std::memset(bytes_, 0, sizeof(bytes_)); }

bool FrameBuffer::pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return false;
  return (bytes_[x + (y / 8) * kWidth] >> (y & 7)) & 1;
}

void FrameBuffer::setPixel(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
  uint8_t &byte = bytes_[x + (y / 8) * kWidth];
  const uint8_t mask = static_cast<uint8_t>(1u << (y & 7));
  byte = on ? static_cast<uint8_t>(byte | mask)
            : static_cast<uint8_t>(byte & ~mask);
}

void FrameBuffer::fillRect(int x, int y, int width, int height, bool on) {
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      setPixel(x + col, y + row, on);
    }
  }
}

void FrameBuffer::drawRect(int x, int y, int width, int height, bool on) {
  if (width <= 0 || height <= 0) return;
  fillRect(x, y, width, 1, on);
  fillRect(x, y + height - 1, width, 1, on);
  fillRect(x, y, 1, height, on);
  fillRect(x + width - 1, y, 1, height, on);
}

void FrameBuffer::drawPattern(int x, int y, const char *const *rows,
                              int rowCount, bool on, int scale) {
  for (int row = 0; row < rowCount; ++row) {
    for (int col = 0; rows[row][col] != '\0'; ++col) {
      if (rows[row][col] == '#') {
        fillRect(x + col * scale, y + row * scale, scale, scale, on);
      }
    }
  }
}

void FrameBuffer::drawBitmap(int x, int y, const uint8_t *bitmap, int width,
                             int height, bool on) {
  const int byteWidth = (width + 7) / 8;
  for (int row = 0; row < height; ++row) {
    for (int col = 0; col < width; ++col) {
      if (bitmap[row * byteWidth + col / 8] & (0x80 >> (col & 7))) {
        setPixel(x + col, y + row, on);
      }
    }
  }
}

bool FrameBuffer::operator==(const FrameBuffer &other) const {
  return std::memcmp(bytes_, other.bytes_, sizeof(bytes_)) == 0;
}
