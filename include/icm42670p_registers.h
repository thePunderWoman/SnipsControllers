#pragma once

#include <cstdint>

// ICM-42670-P register addresses and the values accelerometer.cpp writes to
// them, from TDK InvenSense's "ICM-42670-P Datasheet" (DS-000451, Rev 1.1),
// register map pp. 42-44 and register descriptions pp. 46-101. Kept out of the
// hardware adapter so native tests can check every value against the
// datasheet's bit layout.
namespace Icm42670p {

// I2C address: AP_AD0 (pin 1) tied to GND (p. 32) — see
// PCB/v1.x/GPIO_table.md.
constexpr uint8_t kI2cAddress = 0x68;

// WHO_AM_I (0x75) reads 0x67 on this part.
constexpr uint8_t kWhoAmIValue = 0x67;

// User bank 0 registers. Only the ones this driver touches.
constexpr uint8_t kRegMclkRdy = 0x00;          // MCLK_RDY, bit 3
constexpr uint8_t kRegSignalPathReset = 0x02;  // SOFT_RESET_DEVICE_CONFIG, bit 4
constexpr uint8_t kRegIntConfig = 0x06;        // INT1_MODE/DRIVE/POLARITY, bits 2-0
constexpr uint8_t kRegPwrMgmt0 = 0x1F;         // IDLE bit 4, ACCEL_MODE bits 1-0
constexpr uint8_t kRegAccelConfig0 = 0x21;     // ACCEL_UI_FS_SEL 6-5, ACCEL_ODR 3-0
constexpr uint8_t kRegWomConfig = 0x27;        // WOM_INT_DUR 4-3, WOM_MODE 1, WOM_EN 0
constexpr uint8_t kRegIntSource0 = 0x2B;       // INT1 routing; RESET_DONE on by default
constexpr uint8_t kRegIntSource1 = 0x2C;       // WOM_{Z,Y,X}_INT1_EN, bits 2-0
constexpr uint8_t kRegIntStatus = 0x3A;        // clears on read
constexpr uint8_t kRegIntStatus2 = 0x3B;       // WOM_{X,Y,Z}_INT, clears on read
constexpr uint8_t kRegWhoAmI = 0x75;
constexpr uint8_t kRegBlkSelW = 0x79;          // MREG write access (p. 41)
constexpr uint8_t kRegMaddrW = 0x7A;
constexpr uint8_t kRegMW = 0x7B;

// MREG1 registers, written indirectly through BLK_SEL_W/MADDR_W/M_W.
constexpr uint8_t kMreg1Block = 0x00;
constexpr uint8_t kMreg1AccelWomXThr = 0x4B;
constexpr uint8_t kMreg1AccelWomYThr = 0x4C;
constexpr uint8_t kMreg1AccelWomZThr = 0x4D;

// SIGNAL_PATH_RESET: SOFT_RESET_DEVICE_CONFIG (auto-clears). Registers are
// usable 1 ms after reset (Table 4, p. 13, "Start-up time for
// register read/write").
constexpr uint8_t kSoftReset = 1 << 4;

// MCLK_RDY must read 1 before any MREG access (p. 41).
constexpr uint8_t kMclkRdyBit = 1 << 3;

// INT_CONFIG bits (p. 51). Reset value 0x00 is pulsed, open-drain,
// active-low, so INT1 doesn't drive GPIO46 (a strap pin) until this
// driver configures it.
constexpr uint8_t kInt1PolarityActiveHigh = 1 << 0;
constexpr uint8_t kInt1DrivePushPull = 1 << 1;
constexpr uint8_t kInt1ModeLatched = 1 << 2;

// INT1 latched, push-pull, active-high: the same signal the QMA6100P
// driver gives GPIO46, which has no external pull-up. Latched means a
// short movement can't be missed between loop passes or during light
// sleep: INT1 stays high until motionDetected() reads INT_STATUS2.
constexpr uint8_t kIntConfigValue =
    kInt1ModeLatched | kInt1DrivePushPull | kInt1PolarityActiveHigh;

// PWR_MGMT0: IDLE keeps the RC oscillator running with the sensors off,
// which MREG access needs from sleep (p. 41). ACCEL_MODE 10 is
// low-power mode; ACCEL_LP_CLK_SEL (bit 7) stays 0 so LP mode runs on the
// wake-up oscillator, the datasheet's recommended lowest-power setting.
constexpr uint8_t kPwrMgmt0Idle = 1 << 4;
constexpr uint8_t kPwrMgmt0AccelLowPower = 0x02;

// ACCEL_CONFIG0: ACCEL_UI_FS_SEL 11 -> +/-2g, ACCEL_ODR 1010 -> 50 Hz (LP
// or LN mode). WoM thresholds are in fixed mg whatever the range. 50 Hz
// with ACCEL_CONFIG1's default 16x averaging is a permitted LP-mode
// combination (PWR_MGMT0's ACCEL_MODE notes).
constexpr uint8_t kAccelFs2g = 0x03 << 5;
constexpr uint8_t kAccelOdr50Hz = 0x0A;
constexpr uint8_t kAccelConfig0Value = kAccelFs2g | kAccelOdr50Hz;

// INT_SOURCE0's reset value (0x10) routes the reset-done interrupt to INT1.
// With INT1 latched it would hold the pin high, so route nothing here.
constexpr uint8_t kIntSource0None = 0x00;

// INT_SOURCE1: route WoM on all three axes to INT1.
constexpr uint8_t kWomXInt1En = 1 << 0;
constexpr uint8_t kWomYInt1En = 1 << 1;
constexpr uint8_t kWomZInt1En = 1 << 2;
constexpr uint8_t kIntSource1WomAllAxes = kWomXInt1En | kWomYInt1En | kWomZInt1En;

// INT_STATUS2 WoM bits (bit 2 X, bit 1 Y, bit 0 Z).
constexpr uint8_t kIntStatus2WomMask = 0x07;

// WOM_CONFIG. WOM_MODE 1 compares each sample with the previous one (like
// the QMA6100P's any-motion slope), WOM_INT_DUR 01 asserts on the second
// over-threshold event so a single-sample glitch doesn't count, and WOM_EN
// turns it on. WOM_INT_DUR and WOM_MODE can't change once WOM_EN is set,
// so all three go in one write.
constexpr uint8_t kWomEnable = 1 << 0;
constexpr uint8_t kWomModeComparePrevious = 1 << 1;
constexpr uint8_t kWomIntDurSecondEvent = 0x01 << 3;
constexpr uint8_t kWomConfigValue =
    kWomIntDurSecondEvent | kWomModeComparePrevious | kWomEnable;

// ACCEL_WOM_{X,Y,Z}_THR: 1g/256 (~3.9 mg) per LSB. 8 -> ~31 mg between
// successive samples, matching the QMA6100P's tuned any-motion threshold
// (also ~31 mg). Tune against the once-a-second "active:" log line.
constexpr uint8_t kWomThreshold = 0x08;

// True if an INT_STATUS2 reading shows wake-on-motion on any axis.
constexpr bool womFired(uint8_t intStatus2) {
  return (intStatus2 & kIntStatus2WomMask) != 0;
}

}  // namespace Icm42670p
