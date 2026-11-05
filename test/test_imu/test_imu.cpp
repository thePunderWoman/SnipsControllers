#include <unity.h>

#include "imu.h"

void setUp(void) {}
void tearDown(void) {}

// Raw 12-byte sample with chip-frame accel (in LSB) and gyro (in LSB).
struct Raw {
  uint8_t bytes[12];
  Raw(int ax, int ay, int az, int gx, int gy, int gz) {
    const int words[6] = {ax, ay, az, gx, gy, gz};
    for (int i = 0; i < 6; ++i) {
      const uint16_t w = static_cast<uint16_t>(static_cast<int16_t>(words[i]));
      bytes[2 * i] = static_cast<uint8_t>(w >> 8);
      bytes[2 * i + 1] = static_cast<uint8_t>(w & 0xFF);
    }
  }
};

constexpr int k1g = 16384;
constexpr int k10dps = 1310;

ImuSample still(float pitchUp = 0, float rightUp = 0) {
  ImuSample s;
  s.accelG[0] = rightUp;
  s.accelG[1] = pitchUp;
  s.accelG[2] = 1.0f;
  return s;
}

// ---- mounting ------------------------------------------------------------

void test_v1_2_mapping() {
  // V1.2: board (X, Y, Z) = (-x, +y, -z).
  const Raw raw(k1g, k1g / 2, -k1g, k10dps, 0, -k10dps);
  const ImuSample s = imuSampleFromRaw(raw.bytes, ImuMountings::kV1_2);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, s.accelG[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f, s.accelG[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, s.accelG[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, s.gyroDps[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, s.gyroDps[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, s.gyroDps[2]);
}

void test_v2_mapping() {
  // V2: board (X, Y, Z) = (-y, -x, -z).
  const Raw raw(k1g, k1g / 2, -k1g, k10dps, -k10dps, 0);
  const ImuSample s = imuSampleFromRaw(raw.bytes, ImuMountings::kV2);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, s.accelG[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -1.0f, s.accelG[1]);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, s.accelG[2]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, s.gyroDps[0]);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, s.gyroDps[1]);
}

void test_mountings_are_rotations_not_mirrors() {
  // A mirror would swap the sense of rotation; both boards must have a
  // determinant of +1.
  const ImuMounting mountings[] = {ImuMountings::kV1_2, ImuMountings::kV2};
  for (const ImuMounting &m : mountings) {
    int matrix[3][3] = {};
    for (int row = 0; row < 3; ++row) matrix[row][m.from[row]] = m.sign[row];
    const int det =
        matrix[0][0] * (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) -
        matrix[0][1] * (matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0]) +
        matrix[0][2] * (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]);
    TEST_ASSERT_EQUAL_INT(1, det);
  }
}

// ---- tilt ----------------------------------------------------------------

void test_flat_is_level() {
  TEST_ASSERT_EQUAL_INT(0, imuPitchDeg(still()));
  TEST_ASSERT_EQUAL_INT(0, imuRollDeg(still()));
}

void test_pitch_and_roll_signs_and_range() {
  ImuSample s;
  s.accelG[1] = 1.0f;  // stick end straight up
  TEST_ASSERT_EQUAL_INT(90, imuPitchDeg(s));
  TEST_ASSERT_EQUAL_INT(0, imuRollDeg(s));

  s = ImuSample();
  s.accelG[0] = -0.5f;  // left edge raised 30 degrees
  s.accelG[2] = 0.866f;
  TEST_ASSERT_EQUAL_INT(-30, imuRollDeg(s));
  TEST_ASSERT_EQUAL_INT(0, imuPitchDeg(s));
}

void test_tilt_of_zero_vector_is_zero() {
  TEST_ASSERT_EQUAL_INT(0, imuPitchDeg(ImuSample()));
}

// ---- gyro bias -----------------------------------------------------------

void test_bias_learns_only_after_holding_still() {
  GyroBias bias;
  ImuSample s = still();
  s.gyroDps[2] = 1.0f;
  for (int i = 0; i < GyroBias::kStillSamples; ++i) bias.update(s);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bias.bias(2));
  for (int i = 0; i < 200; ++i) bias.update(s);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, bias.bias(2));

  bias.remove(&s);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, s.gyroDps[2]);
}

void test_bias_ignores_turning_and_shaking() {
  GyroBias bias;
  ImuSample turning = still();
  turning.gyroDps[0] = 20.0f;
  ImuSample shaken = still();
  shaken.accelG[2] = 1.3f;
  for (int i = 0; i < 200; ++i) {
    bias.update(turning);
    bias.update(shaken);
  }
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bias.bias(0));
}

void test_movement_restarts_the_still_count() {
  GyroBias bias;
  ImuSample s = still();
  s.gyroDps[1] = 2.0f;
  ImuSample moving = still();
  moving.gyroDps[1] = 50.0f;
  for (int i = 0; i < 100; ++i) {
    for (int j = 0; j < GyroBias::kStillSamples; ++j) bias.update(s);
    bias.update(moving);
  }
  TEST_ASSERT_EQUAL_FLOAT(0.0f, bias.bias(1));
}

// ---- motion ----------------------------------------------------------------

void test_motion_threshold() {
  ImuSample s = still();
  s.gyroDps[2] = 5.0f;
  TEST_ASSERT_FALSE(imuShowsMotion(s));
  s.gyroDps[2] = -30.0f;
  TEST_ASSERT_TRUE(imuShowsMotion(s));
}

// ---- uplink ----------------------------------------------------------------

void test_fill_uplink() {
  ImuSample s = still(1.0f, 0);  // 45 degrees stick end up
  s.gyroDps[0] = 12.34f;
  s.gyroDps[1] = -400.0f;  // clamps
  s.gyroDps[2] = 5000.0f;
  UplinkPacket uplink;
  fillUplinkImu(s, &uplink);
  TEST_ASSERT_EQUAL_HEX8(UplinkPacket::kImuTiltValid | UplinkPacket::kImuRatesValid,
                         uplink.imuFlags);
  TEST_ASSERT_EQUAL_INT8(45, uplink.pitchDeg);
  TEST_ASSERT_EQUAL_INT8(0, uplink.rollDeg);
  TEST_ASSERT_EQUAL_INT16(123, uplink.rateDeciDps[0]);
  TEST_ASSERT_EQUAL_INT16(-4000, uplink.rateDeciDps[1]);
  TEST_ASSERT_EQUAL_INT16(32767, uplink.rateDeciDps[2]);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  RUN_TEST(test_v1_2_mapping);
  RUN_TEST(test_v2_mapping);
  RUN_TEST(test_mountings_are_rotations_not_mirrors);
  RUN_TEST(test_flat_is_level);
  RUN_TEST(test_pitch_and_roll_signs_and_range);
  RUN_TEST(test_tilt_of_zero_vector_is_zero);
  RUN_TEST(test_bias_learns_only_after_holding_still);
  RUN_TEST(test_bias_ignores_turning_and_shaking);
  RUN_TEST(test_movement_restarts_the_still_count);
  RUN_TEST(test_motion_threshold);
  RUN_TEST(test_fill_uplink);
  return UNITY_END();
}
