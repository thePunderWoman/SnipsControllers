#include "accelerometer.h"

#include <Arduino.h>
#include <Wire.h>

#include "pin_assignment.h"

namespace {

// I2C address: AD0 (pin 1) tied to GND — see PCB/GPIO_table.md.
constexpr uint8_t kI2cAddress = 0x12;

// Register addresses, from the QMA6100P datasheet's register map
// (section 9.1/9.2). Only the ones this driver touches.
constexpr uint8_t kRegChipId = 0x00;
constexpr uint8_t kRegRange = 0x0F;         // FSR: RANGE<3:0>
constexpr uint8_t kRegIntEn2 = 0x18;        // ANY_MOT_EN_{X,Y,Z}, bits 0-2
constexpr uint8_t kRegIntMap1 = 0x1A;       // INT1_ANY_MOT, bit 0
constexpr uint8_t kRegIntPinConf = 0x20;    // INT1_LVL (active-high), bit 1
constexpr uint8_t kRegAnyMotTh = 0x2E;      // ANY_MOT_TH<7:0>
constexpr uint8_t kRegPowerMode = 0x11;     // PM: MODE_BIT, bit 7

// RANGE<3:0> = 0001 -> +/-2g (244ug/LSB) — plenty of resolution for
// "was the controller picked up," and the smallest/lowest-current range.
constexpr uint8_t kRangeValue2g = 0x01;

// Enable ANY_MOT on all three axes (bits 0-2).
constexpr uint8_t kAnyMotEnAllAxes = 0x07;

// Map ANY_MOT_INT to the INT1 pin (bit 0 of INT_MAP1).
constexpr uint8_t kInt1AnyMotBit = 0x01;

// INT1_LVL=1 (active-high), INT1_OD=0 (push-pull) — no external pull-up
// needed on GPIO46 for this signal.
constexpr uint8_t kInt1ActiveHighPushPull = 0x02;

// MODE_BIT=1 -> active mode (device defaults to standby at power-on).
constexpr uint8_t kModeBitActive = 0x80;

// Placeholder sensitivity — needs real-hardware tuning during this
// part's bring-up (see accelerometer.h). Roughly a light nudge rather
// than a hair-trigger, at the default ANY_MOT_DUR (1 sample).
constexpr uint8_t kAnyMotThresholdPlaceholder = 0x20;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kI2cAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// Returns true and fills `outValue` on success; false (untouched
// `outValue`) if the device didn't respond.
bool readRegister(uint8_t reg, uint8_t *outValue) {
  Wire.beginTransmission(kI2cAddress);
  Wire.write(reg);
  if (Wire.endTransmission(/*sendStop=*/false) != 0) {
    return false;
  }
  if (Wire.requestFrom(static_cast<uint8_t>(kI2cAddress),
                        static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  *outValue = static_cast<uint8_t>(Wire.read());
  return true;
}

}  // namespace

bool Accelerometer::begin() {
  Wire.begin(PinAssignment::kI2cSda, PinAssignment::kI2cScl);

  uint8_t chipId = 0;
  if (!readRegister(kRegChipId, &chipId)) {
    present_ = false;
    return false;
  }

  pinMode(PinAssignment::kAccelInterrupt, INPUT);

  writeRegister(kRegRange, kRangeValue2g);
  writeRegister(kRegIntEn2, kAnyMotEnAllAxes);
  writeRegister(kRegAnyMotTh, kAnyMotThresholdPlaceholder);
  writeRegister(kRegIntMap1, kInt1AnyMotBit);
  writeRegister(kRegIntPinConf, kInt1ActiveHighPushPull);
  writeRegister(kRegPowerMode, kModeBitActive);

  present_ = true;
  return true;
}

bool Accelerometer::motionDetected() const {
  if (!present_) {
    return false;
  }
  return digitalRead(PinAssignment::kAccelInterrupt) == HIGH;
}
