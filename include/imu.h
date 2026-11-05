#pragma once

#include <cstdint>

#include "packet.h"

// Pure math for the ICM-42670-P's accelerometer and gyroscope readings,
// which the uplink sends to Amidala so a builder can bind controller tilt
// or rotation to something on the droid. Knows nothing about I2C: the
// Accelerometer adapter (accelerometer.h) hands over the raw sample bytes.
//
// Everything is reported in the controller's own frame, seen from the top
// face (the side with the stick):
//   +X  to the right
//   +Y  toward the stick end, away from USB
//   +Z  out of the top face
// Rotation rates follow the right-hand rule about those axes, so +Z is
// counterclockwise seen from the top.

// How the chip's axes sit on a board. Board axis i reads `sign[i]` times
// chip axis `from[i]` (0 = X, 1 = Y, 2 = Z).
struct ImuMounting {
  int8_t from[3];
  int8_t sign[3];
};

namespace ImuMountings {
// Both boards put U_IMU1 on the bottom, face down, so chip +Z points into
// the board. KiCad stores the bottom footprint mirrored top to bottom
// against the datasheet's top view (Figure 4, p. 18, pin 1 top-left, +X
// right, +Y up), so before rotation chip +X is board +X and chip +Y is
// board -Y.
//   V1.2: rotated 180 degrees -> board (X, Y, Z) = (-x, +y, -z).
//   V2:   rotated -90 degrees -> board (X, Y, Z) = (-y, -x, -z).
// Check on the bench by tilting the stick end up (pitch should go
// positive) and turning the controller counterclockwise flat on a table
// (rate Z should go positive).
constexpr ImuMounting kV1_2 = {{0, 1, 2}, {-1, 1, -1}};
constexpr ImuMounting kV2 = {{1, 0, 2}, {-1, -1, -1}};
}  // namespace ImuMountings

struct ImuSample {
  float accelG[3] = {};
  float gyroDps[3] = {};
};

// Converts a raw 12-byte sample (Icm42670p::kRegAccelDataX1 onward) into
// the board frame, in g and degrees per second.
ImuSample imuSampleFromRaw(const uint8_t *bytes, const ImuMounting &mounting);

// Tilt from gravity alone, so only meaningful while the controller isn't
// being shaken. Each is -90..90 degrees and independent of the other.
// Pitch: + with the stick end raised. Roll: + with the right edge raised.
int imuPitchDeg(const ImuSample &sample);
int imuRollDeg(const ImuSample &sample);

// Tracks the gyroscope's zero-rate offset (around +/-1 deg/s on this part)
// so a still controller reads still. It only learns while the controller
// has been held still for kStillSamples in a row: every axis under
// kStillRateDps and total acceleration within kStillAccelToleranceG of 1 g.
class GyroBias {
 public:
  static constexpr float kStillRateDps = 5.0f;
  static constexpr float kStillAccelToleranceG = 0.05f;
  static constexpr int kStillSamples = 20;  // about 1 s at the uplink rate
  static constexpr float kLearnRate = 0.05f;

  // Feed every raw sample (bias not yet removed).
  void update(const ImuSample &sample);

  // Subtracts the learned offset from `sample`'s gyro rates.
  void remove(ImuSample *sample) const;

  float bias(int axis) const { return bias_[axis]; }

 private:
  float bias_[3] = {};
  int stillCount_ = 0;
};

// True if the gyro shows the controller being turned, used as activity for
// low-power mode while the gyro runs (the accelerometer's wake-on-motion is
// only relied on with the gyro off).
bool imuShowsMotion(const ImuSample &sample);

// Puts a bias-corrected sample into the uplink's IMU fields and sets both
// valid flags.
void fillUplinkImu(const ImuSample &sample, UplinkPacket *uplink);
