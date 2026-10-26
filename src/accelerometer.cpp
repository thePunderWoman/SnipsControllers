#include "accelerometer.h"

#include <Arduino.h>
#include <Wire.h>

#include "icm42670p_registers.h"
#include "pin_assignment.h"
#include "qma6100p_registers.h"

namespace {

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

// Returns true and fills `outValue` on success; false (untouched
// `outValue`) if the device didn't respond.
bool readRegister(uint8_t address, uint8_t reg, uint8_t *outValue) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(/*sendStop=*/false) != 0) {
    return false;
  }
  if (Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  *outValue = static_cast<uint8_t>(Wire.read());
  return true;
}

// Plain presence probe (address-only write, real stop condition) before
// attempting a repeated-start register read. On boards without a part at
// this address endTransmission(false)'s repeated-start check doesn't
// reliably report the missing ACK by itself — a known ESP32 I2C driver
// quirk — so without this, Wire.requestFrom() gets called against a
// device that was never there, which the Wire library logs as an
// alarming-looking error.
bool probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Indirect write to an MREG1 register (ICM-42670-P datasheet p. 41). The
// host must not touch any other register for 10 us afterwards.
void writeIcmMreg1(uint8_t reg, uint8_t value) {
  using namespace Icm42670p;
  writeRegister(kI2cAddress, kRegBlkSelW, kMreg1Block);
  writeRegister(kI2cAddress, kRegMaddrW, reg);
  writeRegister(kI2cAddress, kRegMW, value);
  delayMicroseconds(10);
}

void configureIcm42670p() {
  using namespace Icm42670p;
  // Start from the chip's defaults: it stays powered across an ESP32
  // reset, so it can still hold an earlier boot's settings or a latched
  // interrupt.
  writeRegister(kI2cAddress, kRegSignalPathReset, kSoftReset);
  delay(2);

  // Run the RC oscillator so MREG1 can be written, and wait for it.
  writeRegister(kI2cAddress, kRegPwrMgmt0, kPwrMgmt0Idle);
  uint8_t mclk = 0;
  for (int i = 0; i < 10 && !(mclk & kMclkRdyBit); ++i) {
    delay(1);
    readRegister(kI2cAddress, kRegMclkRdy, &mclk);
  }

  writeIcmMreg1(kMreg1AccelWomXThr, kWomThreshold);
  writeIcmMreg1(kMreg1AccelWomYThr, kWomThreshold);
  writeIcmMreg1(kMreg1AccelWomZThr, kWomThreshold);

  writeRegister(kI2cAddress, kRegAccelConfig0, kAccelConfig0Value);
  writeRegister(kI2cAddress, kRegIntSource0, kIntSource0None);
  writeRegister(kI2cAddress, kRegIntSource1, kIntSource1WomAllAxes);
  // Clear the reset-done status before INT1 becomes a latched output.
  uint8_t status = 0;
  readRegister(kI2cAddress, kRegIntStatus, &status);
  writeRegister(kI2cAddress, kRegIntConfig, kIntConfigValue);

  // Accelerometer on in low-power mode (wake-up oscillator). No register
  // writes for 200 us after, and the first valid data takes 10 ms (Table
  // 2), so wait before WoM starts comparing samples.
  writeRegister(kI2cAddress, kRegPwrMgmt0, kPwrMgmt0AccelLowPower);
  delay(20);
  writeRegister(kI2cAddress, kRegWomConfig, kWomConfigValue);
}

void configureQma6100p() {
  using namespace Qma6100p;
  // Start from the chip's defaults, as above.
  writeRegister(kI2cAddress, kRegSoftReset, kSoftReset);
  delay(20);
  writeRegister(kI2cAddress, kRegSoftReset, kSoftResetRelease);
  delay(20);

  writeRegister(kI2cAddress, kRegRange, kRangeValue2g);
  writeRegister(kI2cAddress, kRegIntEn2, kAnyMotEnAllAxes);
  writeRegister(kI2cAddress, kRegAnyMotTh, kAnyMotThreshold);
  writeRegister(kI2cAddress, kRegMotConf0, kAnyMotDuration2Samples);
  writeRegister(kI2cAddress, kRegIntMap1, kInt1AnyMotBit);
  writeRegister(kI2cAddress, kRegIntPinConf, kIntPinConfValue);
  // Read-modify-write: INT_CFG's reset value has bits the datasheet
  // doesn't describe, so keep them.
  uint8_t intCfg = 0;
  readRegister(kI2cAddress, kRegIntCfg, &intCfg);
  writeRegister(kI2cAddress, kRegIntCfg, intCfg | kIntCfgLatchedClearOnRead);
  writeRegister(kI2cAddress, kRegPowerMode, kModeBitActive);
}

}  // namespace

bool Accelerometer::begin() {
  Wire.begin(PinAssignment::kI2cSda, PinAssignment::kI2cScl);

  // Boards from V1.2 on carry the ICM-42670-P; V1.1 and earlier the
  // QMA6100P, or nothing on the first production run.
  uint8_t id = 0;
  if (probe(Icm42670p::kI2cAddress) &&
      readRegister(Icm42670p::kI2cAddress, Icm42670p::kRegWhoAmI, &id) &&
      id == Icm42670p::kWhoAmIValue) {
    chip_ = Chip::kIcm42670p;
  } else if (probe(Qma6100p::kI2cAddress) &&
             readRegister(Qma6100p::kI2cAddress, Qma6100p::kRegChipId, &id)) {
    chip_ = Chip::kQma6100p;
  } else {
    chip_ = Chip::kNone;
    return false;
  }

  pinMode(PinAssignment::kAccelInterrupt, INPUT);
  if (chip_ == Chip::kIcm42670p) {
    configureIcm42670p();
  } else {
    configureQma6100p();
  }
  return true;
}

bool Accelerometer::motionDetected() {
  if (chip_ == Chip::kNone ||
      digitalRead(PinAssignment::kAccelInterrupt) != HIGH) {
    return false;
  }
  uint8_t status = 0;
  if (chip_ == Chip::kIcm42670p) {
    // Reading INT_STATUS2 clears the latch, but in LP mode INT1 can take
    // up to one sample period to fall afterwards (INT_CONFIG1's
    // INT_ASYNC_RESET, p. 77), so go by the status bits, not the pin.
    readRegister(Icm42670p::kI2cAddress, Icm42670p::kRegIntStatus2, &status);
    return Icm42670p::womFired(status);
  }
  // Reading INT_ST0 clears the latch (INT_RD_CLR); INT1 latches again on
  // the next sample if the motion is still going.
  readRegister(Qma6100p::kI2cAddress, Qma6100p::kRegIntSt0, &status);
  return true;
}
