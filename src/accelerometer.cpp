#include "accelerometer.h"

#include <Arduino.h>
#include <Wire.h>

#include "pin_assignment.h"
#include "qma6100p_registers.h"

namespace {

using namespace Qma6100p;

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

  // Plain presence probe (address-only write, real stop condition)
  // before attempting the repeated-start register read below. On boards
  // without this part (the first production run — see
  // PCB/GPIO_table.md's Accelerometer section) endTransmission(false)'s
  // repeated-start check doesn't reliably report the missing ACK by
  // itself — a known ESP32 I2C driver quirk — so without this,
  // Wire.requestFrom() gets called against a device that was never
  // there, which the Wire library logs as an alarming-looking error.
  Wire.beginTransmission(kI2cAddress);
  if (Wire.endTransmission() != 0) {
    present_ = false;
    return false;
  }

  uint8_t chipId = 0;
  if (!readRegister(kRegChipId, &chipId)) {
    present_ = false;
    return false;
  }

  pinMode(PinAssignment::kAccelInterrupt, INPUT);

  writeRegister(kRegRange, kRangeValue2g);
  writeRegister(kRegIntEn2, kAnyMotEnAllAxes);
  writeRegister(kRegAnyMotTh, kAnyMotThreshold);
  writeRegister(kRegMotConf0, kAnyMotDuration4Samples);
  writeRegister(kRegIntMap1, kInt1AnyMotBit);
  writeRegister(kRegIntPinConf, kIntPinConfValue);
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
