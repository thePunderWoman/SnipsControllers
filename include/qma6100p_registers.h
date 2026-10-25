#pragma once

#include <cstdint>

// QMA6100P register addresses and the values accelerometer.cpp writes to
// them, from QST's "QMA6100P Preliminary Datasheet" (Doc #13-52-20, Rev
// A1), register map section 9. Kept out of the hardware adapter so native
// tests can check every value against the datasheet's bit layout.
namespace Qma6100p {

// I2C address: AD0 (pin 1) tied to GND — see PCB/GPIO_table.md.
constexpr uint8_t kI2cAddress = 0x12;

// Register addresses. Only the ones this driver touches.
constexpr uint8_t kRegChipId = 0x00;
constexpr uint8_t kRegRange = 0x0F;       // FSR: RANGE<3:0>
constexpr uint8_t kRegPowerMode = 0x11;   // PM: MODE_BIT, bit 7
constexpr uint8_t kRegIntEn2 = 0x18;      // ANY_MOT_EN_{X,Y,Z}, bits 0-2
constexpr uint8_t kRegIntMap1 = 0x1A;     // INT1_ANY_MOT, bit 0
constexpr uint8_t kRegIntPinConf = 0x20;  // INT1_LVL bit 0, INT1_OD bit 1
constexpr uint8_t kRegMotConf0 = 0x2C;    // ANY_MOT_DUR<1:0>, bits 0-1
constexpr uint8_t kRegAnyMotTh = 0x2E;    // ANY_MOT_TH<7:0>

// INTPIN_CONF (0x20) bits, p. 32. LVL: 1 = active-high. OD: 1 =
// open-drain, 0 = push-pull.
constexpr uint8_t kInt1Lvl = 1 << 0;
constexpr uint8_t kInt1Od = 1 << 1;
constexpr uint8_t kInt2Lvl = 1 << 2;
constexpr uint8_t kInt2Od = 1 << 3;

// RANGE<3:0> = 0001 -> +/-2g (244ug/LSB) — plenty of resolution for
// "was the controller picked up," and the smallest/lowest-current range.
constexpr uint8_t kRangeValue2g = 0x01;

// Enable ANY_MOT on all three axes (bits 0-2).
constexpr uint8_t kAnyMotEnAllAxes = 0x07;

// Map ANY_MOT_INT to the INT1 pin (bit 0 of INT_MAP1).
constexpr uint8_t kInt1AnyMotBit = 0x01;

// INT1 active-high, push-pull — no external pull-up needed on GPIO46 for
// this signal. This was once 0x02, which is INT1_OD (open-drain) with
// INT1_LVL clear (active-low): with nothing pulling it up, the floating
// pin read as constant motion and low-power mode never saw idle. INT2 is
// left at its reset state (active-high push-pull; not connected), so this
// equals the register's 0x05 reset default.
constexpr uint8_t kIntPinConfValue = kInt1Lvl | kInt2Lvl;

// MODE_BIT=1 -> active mode (device defaults to standby at power-on).
constexpr uint8_t kModeBitActive = 0x80;

// Any-motion compares the change between successive samples against
// ANY_MOT_TH * 16 LSB (MOT_CONF2, with ANY_MOT_IN_SEL left at 0). At
// +/-2g (244ug/LSB) that's ~3.9mg per step, so 0x30 is ~190mg — up from
// an untuned 0x20 (~125mg), to ignore small wobbles. Still a best guess:
// tune against the "Activity: motion" log lines on real hardware.
constexpr uint8_t kAnyMotThreshold = 0x30;

// MOT_CONF0: the slope must exceed the threshold for ANY_MOT_DUR + 1
// consecutive samples. 3 -> 4 samples (the maximum), so a single jolt
// doesn't count, only sustained motion like picking the controller up.
// The register's NO_MOT_DUR bits stay 0; no-motion isn't used.
constexpr uint8_t kAnyMotDuration4Samples = 0x03;

}  // namespace Qma6100p
