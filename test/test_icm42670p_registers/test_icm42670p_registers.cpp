#include <unity.h>

#include "icm42670p_registers.h"

// Values checked against TDK InvenSense's ICM-42670-P Datasheet (DS-000451,
// Rev 1.1): register map pp. 42-44, descriptions pp. 46-101.

void setUp(void) {}
void tearDown(void) {}

void test_identity() {
  // AP_AD0 low -> b1101000 (p. 32); WHO_AM_I (0x75) reset value 0x67 (p. 70).
  TEST_ASSERT_EQUAL_HEX8(0x68, Icm42670p::kI2cAddress);
  TEST_ASSERT_EQUAL_HEX8(0x75, Icm42670p::kRegWhoAmI);
  TEST_ASSERT_EQUAL_HEX8(0x67, Icm42670p::kWhoAmIValue);
}

void test_bank0_register_addresses() {
  TEST_ASSERT_EQUAL_HEX8(0x00, Icm42670p::kRegMclkRdy);
  TEST_ASSERT_EQUAL_HEX8(0x02, Icm42670p::kRegSignalPathReset);
  TEST_ASSERT_EQUAL_HEX8(0x06, Icm42670p::kRegIntConfig);
  TEST_ASSERT_EQUAL_HEX8(0x1F, Icm42670p::kRegPwrMgmt0);
  TEST_ASSERT_EQUAL_HEX8(0x21, Icm42670p::kRegAccelConfig0);
  TEST_ASSERT_EQUAL_HEX8(0x27, Icm42670p::kRegWomConfig);
  TEST_ASSERT_EQUAL_HEX8(0x2B, Icm42670p::kRegIntSource0);
  TEST_ASSERT_EQUAL_HEX8(0x2C, Icm42670p::kRegIntSource1);
  TEST_ASSERT_EQUAL_HEX8(0x3A, Icm42670p::kRegIntStatus);
  TEST_ASSERT_EQUAL_HEX8(0x3B, Icm42670p::kRegIntStatus2);
}

void test_mreg1_access_and_wom_thresholds() {
  // BLK_SEL_W/MADDR_W/M_W at 0x79-0x7B; MREG1 is block 0x00 (p. 41).
  TEST_ASSERT_EQUAL_HEX8(0x79, Icm42670p::kRegBlkSelW);
  TEST_ASSERT_EQUAL_HEX8(0x7A, Icm42670p::kRegMaddrW);
  TEST_ASSERT_EQUAL_HEX8(0x7B, Icm42670p::kRegMW);
  TEST_ASSERT_EQUAL_HEX8(0x00, Icm42670p::kMreg1Block);
  // ACCEL_WOM_{X,Y,Z}_THR in MREG1 at 0x4B-0x4D (p. 91).
  TEST_ASSERT_EQUAL_HEX8(0x4B, Icm42670p::kMreg1AccelWomXThr);
  TEST_ASSERT_EQUAL_HEX8(0x4C, Icm42670p::kMreg1AccelWomYThr);
  TEST_ASSERT_EQUAL_HEX8(0x4D, Icm42670p::kMreg1AccelWomZThr);
}

void test_soft_reset_and_clock_ready_bits() {
  // SIGNAL_PATH_RESET bit 4 (p. 47); MCLK_RDY bit 3 (p. 46).
  TEST_ASSERT_EQUAL_HEX8(0x10, Icm42670p::kSoftReset);
  TEST_ASSERT_EQUAL_HEX8(0x08, Icm42670p::kMclkRdyBit);
}

// INT1 must be push-pull active-high: GPIO46 has no external pull-up, so
// an open-drain or active-low INT1 would float or read as constant motion
// (the bug the QMA6100P driver once had).
void test_int1_is_latched_push_pull_active_high() {
  // INT_CONFIG (p. 51): bit 2 INT1_MODE, bit 1 INT1_DRIVE_CIRCUIT, bit 0
  // INT1_POLARITY.
  TEST_ASSERT_EQUAL_HEX8(0x01, Icm42670p::kInt1PolarityActiveHigh);
  TEST_ASSERT_EQUAL_HEX8(0x02, Icm42670p::kInt1DrivePushPull);
  TEST_ASSERT_EQUAL_HEX8(0x04, Icm42670p::kInt1ModeLatched);
  TEST_ASSERT_EQUAL_HEX8(0x07, Icm42670p::kIntConfigValue);
}

void test_power_modes() {
  // PWR_MGMT0 (p. 56): IDLE bit 4; ACCEL_MODE 10 = low power, with
  // ACCEL_LP_CLK_SEL (bit 7) 0 for the wake-up oscillator; gyro off.
  TEST_ASSERT_EQUAL_HEX8(0x10, Icm42670p::kPwrMgmt0Idle);
  TEST_ASSERT_EQUAL_HEX8(0x02, Icm42670p::kPwrMgmt0AccelLowPower);
}

void test_accel_config_is_2g_at_50hz() {
  // ACCEL_CONFIG0 (p. 58): FS_SEL 11 in bits 6-5, ODR 1010 in bits 3-0.
  TEST_ASSERT_EQUAL_HEX8(0x60, Icm42670p::kAccelFs2g);
  TEST_ASSERT_EQUAL_HEX8(0x0A, Icm42670p::kAccelOdr50Hz);
  TEST_ASSERT_EQUAL_HEX8(0x6A, Icm42670p::kAccelConfig0Value);
}

// INT_SOURCE0 resets to 0x10, routing reset-done to INT1 (p. 64). Left in
// place, a latched INT1 would read as constant motion after every reset.
void test_reset_done_not_routed_to_int1() {
  TEST_ASSERT_EQUAL_HEX8(0x00, Icm42670p::kIntSource0None);
}

void test_wom_routed_to_int1_on_all_axes() {
  // INT_SOURCE1 (p. 64): bit 2 Z, bit 1 Y, bit 0 X.
  TEST_ASSERT_EQUAL_HEX8(0x07, Icm42670p::kIntSource1WomAllAxes);
}

void test_wom_config() {
  // WOM_CONFIG (p. 62): WOM_INT_DUR bits 4-3 (01 = second event),
  // WOM_MODE bit 1 (1 = compare with previous sample), WOM_EN bit 0.
  TEST_ASSERT_EQUAL_HEX8(0x08, Icm42670p::kWomIntDurSecondEvent);
  TEST_ASSERT_EQUAL_HEX8(0x02, Icm42670p::kWomModeComparePrevious);
  TEST_ASSERT_EQUAL_HEX8(0x01, Icm42670p::kWomEnable);
  TEST_ASSERT_EQUAL_HEX8(0x0B, Icm42670p::kWomConfigValue);
}

void test_wom_threshold_about_31mg() {
  // 1g/256 per LSB (p. 91): 8 LSB is ~31 mg.
  TEST_ASSERT_EQUAL_HEX8(0x08, Icm42670p::kWomThreshold);
}

void test_wom_fired_reads_only_wom_bits() {
  // INT_STATUS2 (p. 69): bit 3 SMD, bits 2-0 WoM X/Y/Z.
  TEST_ASSERT_FALSE(Icm42670p::womFired(0x00));
  TEST_ASSERT_TRUE(Icm42670p::womFired(0x01));
  TEST_ASSERT_TRUE(Icm42670p::womFired(0x02));
  TEST_ASSERT_TRUE(Icm42670p::womFired(0x04));
  TEST_ASSERT_FALSE(Icm42670p::womFired(0x08));
  TEST_ASSERT_FALSE(Icm42670p::womFired(0xF0));
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_identity);
  RUN_TEST(test_bank0_register_addresses);
  RUN_TEST(test_mreg1_access_and_wom_thresholds);
  RUN_TEST(test_soft_reset_and_clock_ready_bits);
  RUN_TEST(test_int1_is_latched_push_pull_active_high);
  RUN_TEST(test_power_modes);
  RUN_TEST(test_accel_config_is_2g_at_50hz);
  RUN_TEST(test_reset_done_not_routed_to_int1);
  RUN_TEST(test_wom_routed_to_int1_on_all_axes);
  RUN_TEST(test_wom_config);
  RUN_TEST(test_wom_threshold_about_31mg);
  RUN_TEST(test_wom_fired_reads_only_wom_bits);
  return UNITY_END();
}
