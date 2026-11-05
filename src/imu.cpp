#include "imu.h"

#include <cmath>

#include "icm42670p_registers.h"

namespace {

constexpr float kRadToDeg = 57.29578f;

// Turning faster than this counts as activity. Well above the gyro's
// zero-rate offset, so a still controller on a table doesn't keep the
// controller awake.
constexpr float kMotionRateDps = 15.0f;

int tiltDeg(float along, float other1, float other2) {
  const float deg =
      std::atan2(along, std::sqrt(other1 * other1 + other2 * other2)) * kRadToDeg;
  return static_cast<int>(std::lround(deg));
}

}  // namespace

ImuSample imuSampleFromRaw(const uint8_t *bytes, const ImuMounting &mounting) {
  ImuSample sample;
  for (int axis = 0; axis < 3; ++axis) {
    const int from = mounting.from[axis];
    const float sign = mounting.sign[axis];
    sample.accelG[axis] =
        sign * Icm42670p::sampleWord(bytes, from) / Icm42670p::kAccelLsbPerG;
    sample.gyroDps[axis] =
        sign * Icm42670p::sampleWord(bytes, 3 + from) / Icm42670p::kGyroLsbPerDps;
  }
  return sample;
}

int imuPitchDeg(const ImuSample &s) {
  return tiltDeg(s.accelG[1], s.accelG[0], s.accelG[2]);
}

int imuRollDeg(const ImuSample &s) {
  return tiltDeg(s.accelG[0], s.accelG[1], s.accelG[2]);
}

void GyroBias::update(const ImuSample &sample) {
  const float *a = sample.accelG;
  const float gMagnitude = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  bool still = std::fabs(gMagnitude - 1.0f) <= kStillAccelToleranceG;
  for (int axis = 0; axis < 3 && still; ++axis) {
    still = std::fabs(sample.gyroDps[axis]) < kStillRateDps;
  }
  if (!still) {
    stillCount_ = 0;
    return;
  }
  if (stillCount_ < kStillSamples) {
    ++stillCount_;
    return;
  }
  for (int axis = 0; axis < 3; ++axis) {
    bias_[axis] += kLearnRate * (sample.gyroDps[axis] - bias_[axis]);
  }
}

void GyroBias::remove(ImuSample *sample) const {
  for (int axis = 0; axis < 3; ++axis) sample->gyroDps[axis] -= bias_[axis];
}

bool imuShowsMotion(const ImuSample &sample) {
  for (float rate : sample.gyroDps) {
    if (std::fabs(rate) > kMotionRateDps) return true;
  }
  return false;
}

void fillUplinkImu(const ImuSample &sample, UplinkPacket *uplink) {
  uplink->imuFlags = UplinkPacket::kImuTiltValid | UplinkPacket::kImuRatesValid;
  uplink->pitchDeg = static_cast<int8_t>(imuPitchDeg(sample));
  uplink->rollDeg = static_cast<int8_t>(imuRollDeg(sample));
  for (int axis = 0; axis < 3; ++axis) {
    long deci = std::lround(sample.gyroDps[axis] * 10.0f);
    if (deci > 32767) deci = 32767;
    if (deci < -32768) deci = -32768;
    uplink->rateDeciDps[axis] = static_cast<int16_t>(deci);
  }
}
