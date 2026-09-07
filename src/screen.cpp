#include "screen.h"

void ScreenBuffer::setLine(size_t index, const char *text) {
  if (index >= kMaxLines || text == nullptr) {
    return;
  }
  std::strncpy(lines_[index], text, kMaxLineLength);
  lines_[index][kMaxLineLength] = '\0';
}

void ScreenBuffer::clear() {
  for (size_t i = 0; i < kMaxLines; ++i) {
    lines_[i][0] = '\0';
  }
}

const char *ScreenBuffer::line(size_t index) const {
  return index < kMaxLines ? lines_[index] : "";
}
