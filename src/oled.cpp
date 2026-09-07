#include "oled.h"

#include "pin_assignment.h"

bool OledDisplay::begin() {
  Wire.begin(PinAssignment::kI2cSda, PinAssignment::kI2cScl);
  return display_.begin(SSD1306_SWITCHCAPVCC, Oled::kI2cAddress);
}

void OledDisplay::render(const ScreenBuffer &content) {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  for (size_t i = 0; i < ScreenBuffer::lineCount(); ++i) {
    display_.setCursor(0, static_cast<int16_t>(i * 8));
    display_.print(content.line(i));
  }
  display_.display();
}
