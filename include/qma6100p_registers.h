#pragma once

#include <cstdint>

// QMA6100P register addresses and the values accelerometer.cpp writes to
// them, from QST's "QMA6100P Preliminary Datasheet" (Doc #13-52-20, Rev
// A1), register map section 9. Kept out of the hardware adapter so native
// tests can check every value against the datasheet's bit layout.
namespace Qma6100p {

// I2C address: AD0 (pin 1) tied to GND — see PCB/v1.x/GPIO_table.md.
constexpr uint8_t kI2cAddress = 0x12;

// Register addresses. Only the ones this driver touches.
constexpr uint8_t kRegChipId = 0x00;
constexpr uint8_t kRegIntSt0 = 0x09;      // INT_ST0: ANY_MOT_FIRST_{X,Y,Z}
constexpr uint8_t kRegRange = 0x0F;       // FSR: RANGE<3:0>
constexpr uint8_t kRegPowerMode = 0x11;   // PM: MODE_BIT, bit 7
constexpr uint8_t kRegIntEn2 = 0x18;      // ANY_MOT_EN_{X,Y,Z}, bits 0-2
constexpr uint8_t kRegIntMap1 = 0x1A;     // INT1_ANY_MOT, bit 0
constexpr uint8_t kRegIntPinConf = 0x20;  // INT1_LVL bit 0, INT1_OD bit 1
constexpr uint8_t kRegIntCfg = 0x21;      // INT_RD_CLR bit 7, LATCH_INT bit 0
constexpr uint8_t kRegMotConf0 = 0x2C;    // ANY_MOT_DUR<1:0>, bits 0-1
constexpr uint8_t kRegAnyMotTh = 0x2E;    // ANY_MOT_TH<7:0>
constexpr uint8_t kRegSoftReset = 0x36;   // S_RESET

// Writing kSoftReset to S_RESET returns every register to its default
// (section 6.1); writing kSoftResetRelease afterwards lets it run again.
constexpr uint8_t kSoftReset = 0xB6;
constexpr uint8_t kSoftResetRelease = 0x00;

// INTPIN_CONF (0x20) bits, p. 32. LVL: 1 = active-high. OD: 1 =
// open-drain, 0 = push-pull.
constexpr uint8_t kInt1Lvl = 1 << 0;
constexpr uint8_t kInt1Od = 1 << 1;
constexpr uint8_t kInt2Lvl = 1 << 2;
constexpr uint8_t kInt2Od = 1 << 3;

// INT_CFG (0x21) bits, p. 33. LATCH_INT holds INT1 until it's cleared;
// INT_RD_CLR clears it on any read of INT_ST0-INT_ST4 (0x09-0x0D).
constexpr uint8_t kLatchInt = 1 << 0;
constexpr uint8_t kIntRdClr = 1 << 7;

// Latched, cleared by reading INT_ST0 — the setup Meshtastic's
// wake-on-motion uses for this chip. Non-latched mode was tried first,
// but the datasheet contradicts itself on whether a non-latched interrupt
// clears without a status read (section 8 vs INT_CFG's INT_RD_CLR
// description), and on hardware motion registered once and never again.
// Latched also means a short movement can't be missed between loop
// passes or during light sleep: INT1 stays high until motionDetected()
// reads INT_ST0, and re-latches on the next sample if motion continues.
constexpr uint8_t kIntCfgLatchedClearOnRead = kLatchInt | kIntRdClr;

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
// +/-2g (244ug/LSB) that's ~3.9mg per step, so 0x08 is ~31mg: well above
// a resting controller's noise, well below handling it. 0x30 (~190mg)
// over 4 samples was tried and never fired on real hardware; the
// datasheet gives no sample rate to work the threshold out from, so tune
// this against the once-a-second "active:" log line (Meshtastic leaves
// it at its default of 0).
constexpr uint8_t kAnyMotThreshold = 0x08;

// MOT_CONF0: the slope must exceed the threshold for ANY_MOT_DUR + 1
// consecutive samples. 1 -> 2 samples, so a single-sample glitch doesn't
// count. The register's NO_MOT_DUR bits stay 0; no-motion isn't used.
constexpr uint8_t kAnyMotDuration2Samples = 0x01;

}  // namespace Qma6100p
