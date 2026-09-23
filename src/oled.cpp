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

void OledDisplay::renderBitmapCentered(const uint8_t *bitmap, int width,
                                       int height) {
  display_.clearDisplay();
  const int16_t x = static_cast<int16_t>((Oled::kWidth - width) / 2);
  const int16_t y = static_cast<int16_t>((Oled::kHeight - height) / 2);
  display_.drawBitmap(x, y, bitmap, width, height, SSD1306_WHITE);
  display_.display();
}

void OledDisplay::setDimmed(bool dimmed) { display_.dim(dimmed); }

void OledDisplay::setPowerOn(bool on) {
  display_.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}
