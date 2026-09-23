#include "oled.h"

#include "pin_assignment.h"

namespace {

// An address-only write with no data is the standard I2C presence probe
// — endTransmission() returns 0 only if a device at that address
// actually ACKed it. Adafruit_SSD1306::begin() never does this check
// itself (see below), so this is the only real signal we have.
bool i2cAck(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Bring-up diagnostic: neither expected address ACKed, so scan the whole
// bus and report whatever's actually there (or confirm nothing is) —
// much faster to narrow down over Serial than guessing addresses one at
// a time by re-flashing.
void logI2cScan() {
  Serial.println("OLED not found at 0x3C or 0x3D -- scanning I2C bus:");
  bool foundAny = false;
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    if (i2cAck(addr)) {
      foundAny = true;
      Serial.print("  Found device at 0x");
      Serial.println(addr, HEX);
    }
  }
  if (!foundAny) {
    Serial.println(
        "  Nothing responded at any address -- bus is dead (check "
        "SDA/SCL continuity and pull-ups, not just VCC/GND).");
  }
}

}  // namespace

bool OledDisplay::begin() {
  Wire.begin(PinAssignment::kI2cSda, PinAssignment::kI2cScl);

  // Adafruit_SSD1306::begin() never actually checks whether any of its
  // I2C writes were ACKed — it always runs the full init sequence and
  // returns true unless memory allocation fails (which never happens in
  // practice). So it can't tell us whether the display is really there;
  // do a real presence check ourselves first, trying both common
  // addresses before giving up.
  uint8_t addr = Oled::kI2cAddress;
  if (!i2cAck(addr)) {
    addr = Oled::kI2cAddressAlt;
    if (!i2cAck(addr)) {
      logI2cScan();
      return false;
    }
  }

  ready_ = display_.begin(SSD1306_SWITCHCAPVCC, addr);
  return ready_;
}

void OledDisplay::render(const ScreenBuffer &content) {
  if (!ready_) return;
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
  if (!ready_) return;
  display_.clearDisplay();
  const int16_t x = static_cast<int16_t>((Oled::kWidth - width) / 2);
  const int16_t y = static_cast<int16_t>((Oled::kHeight - height) / 2);
  display_.drawBitmap(x, y, bitmap, width, height, SSD1306_WHITE);
  display_.display();
}

void OledDisplay::setDimmed(bool dimmed) {
  if (!ready_) return;
  display_.dim(dimmed);
}

void OledDisplay::setPowerOn(bool on) {
  if (!ready_) return;
  display_.ssd1306_command(on ? SSD1306_DISPLAYON : SSD1306_DISPLAYOFF);
}
