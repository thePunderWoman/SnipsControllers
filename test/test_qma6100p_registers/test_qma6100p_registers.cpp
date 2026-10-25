#include <unity.h>

#include "qma6100p_registers.h"

// Values checked against QST's QMA6100P Preliminary Datasheet (Doc
// #13-52-20, Rev A1), register map section 9.

void setUp(void) {}
void tearDown(void) {}

void test_intpin_conf_bit_layout_matches_datasheet() {
  // Register 0x20 (INTPIN_CONF), p. 32: bit 0 INT1_LVL, bit 1 INT1_OD,
  // bit 2 INT2_LVL, bit 3 INT2_OD.
  TEST_ASSERT_EQUAL_HEX8(0x01, Qma6100p::kInt1Lvl);
  TEST_ASSERT_EQUAL_HEX8(0x02, Qma6100p::kInt1Od);
  TEST_ASSERT_EQUAL_HEX8(0x04, Qma6100p::kInt2Lvl);
  TEST_ASSERT_EQUAL_HEX8(0x08, Qma6100p::kInt2Od);
}

// Regression: INTPIN_CONF was written as 0x02, which made INT1
// open-drain and active-low. GPIO46 has no pull-up, so the pin floated,
// read as constant motion, and low-power mode never saw idle.
void test_int1_is_active_high_push_pull() {
  TEST_ASSERT_TRUE(Qma6100p::kIntPinConfValue & Qma6100p::kInt1Lvl);
  TEST_ASSERT_FALSE(Qma6100p::kIntPinConfValue & Qma6100p::kInt1Od);
}

void test_intpin_conf_keeps_reset_default() {
  // Reset default 0x05; the upper bits (SENB pull-up, AD0 input, 3-wire
  // SPI, step-count peak) stay at their defaults too.
  TEST_ASSERT_EQUAL_HEX8(0x05, Qma6100p::kIntPinConfValue);
}

void test_any_motion_routed_to_int1() {
  // Register 0x1A (INT_MAP1): bit 0 INT1_ANY_MOT.
  TEST_ASSERT_EQUAL_HEX8(0x1A, Qma6100p::kRegIntMap1);
  TEST_ASSERT_EQUAL_HEX8(0x01, Qma6100p::kInt1AnyMotBit);
  // Register 0x18 (INT_EN2): bits 0-2 ANY_MOT_EN_X/Y/Z.
  TEST_ASSERT_EQUAL_HEX8(0x18, Qma6100p::kRegIntEn2);
  TEST_ASSERT_EQUAL_HEX8(0x07, Qma6100p::kAnyMotEnAllAxes);
}

void test_range_is_2g() {
  // Register 0x0F (FSR): RANGE<3:0> = 0001 -> +/-2g.
  TEST_ASSERT_EQUAL_HEX8(0x0F, Qma6100p::kRegRange);
  TEST_ASSERT_EQUAL_HEX8(0x01, Qma6100p::kRangeValue2g);
}

void test_any_motion_duration_and_threshold_registers() {
  // Register 0x2C (MOT_CONF0): ANY_MOT_DUR<1:0> in bits 0-1, so the
  // value must fit there and leave NO_MOT_DUR (bits 2-7) at 0.
  TEST_ASSERT_EQUAL_HEX8(0x2C, Qma6100p::kRegMotConf0);
  TEST_ASSERT_EQUAL_HEX8(0, Qma6100p::kAnyMotDuration2Samples & ~0x03);
  TEST_ASSERT_EQUAL_HEX8(0x01, Qma6100p::kAnyMotDuration2Samples);
  // Register 0x2E (MOT_CONF2): ANY_MOT_TH<7:0>.
  TEST_ASSERT_EQUAL_HEX8(0x2E, Qma6100p::kRegAnyMotTh);
}

// Regression: with INT1 non-latched and INT_ST never read, motion
// registered once on hardware and never again. INT1 is now latched and
// cleared by the INT_ST0 read in motionDetected().
void test_int_cfg_latches_and_clears_on_status_read() {
  // Register 0x21 (INT_CFG), p. 33: bit 7 INT_RD_CLR, bit 0 LATCH_INT.
  TEST_ASSERT_EQUAL_HEX8(0x21, Qma6100p::kRegIntCfg);
  TEST_ASSERT_EQUAL_HEX8(0x01, Qma6100p::kLatchInt);
  TEST_ASSERT_EQUAL_HEX8(0x80, Qma6100p::kIntRdClr);
  TEST_ASSERT_EQUAL_HEX8(0x81, Qma6100p::kIntCfgLatchedClearOnRead);
  // Register 0x09 (INT_ST0) is within the 0x09-0x0D range INT_RD_CLR
  // clears on.
  TEST_ASSERT_EQUAL_HEX8(0x09, Qma6100p::kRegIntSt0);
}

void test_soft_reset_sequence() {
  // Register 0x36 (S_RESET): writing 0xB6 resets all registers to default
  // (section 6.1).
  TEST_ASSERT_EQUAL_HEX8(0x36, Qma6100p::kRegSoftReset);
  TEST_ASSERT_EQUAL_HEX8(0xB6, Qma6100p::kSoftReset);
  TEST_ASSERT_EQUAL_HEX8(0x00, Qma6100p::kSoftResetRelease);
}

void test_power_mode_sets_active_bit() {
  // Register 0x11 (PM): bit 7 MODE_BIT, 1 = active.
  TEST_ASSERT_EQUAL_HEX8(0x11, Qma6100p::kRegPowerMode);
  TEST_ASSERT_EQUAL_HEX8(0x80, Qma6100p::kModeBitActive);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_intpin_conf_bit_layout_matches_datasheet);
  RUN_TEST(test_int1_is_active_high_push_pull);
  RUN_TEST(test_intpin_conf_keeps_reset_default);
  RUN_TEST(test_any_motion_routed_to_int1);
  RUN_TEST(test_range_is_2g);
  RUN_TEST(test_any_motion_duration_and_threshold_registers);
  RUN_TEST(test_int_cfg_latches_and_clears_on_status_read);
  RUN_TEST(test_soft_reset_sequence);
  RUN_TEST(test_power_mode_sets_active_bit);
  return UNITY_END();
}
