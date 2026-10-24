#pragma once

#include <cstddef>
#include <cstdint>

#include "calibration.h"

// Pure logic for what counts as "activity" to low-power mode (see
// power_management.h). Knows nothing about real buttons, ADC reads, or
// the accelerometer — SnipsController.ino feeds in this tick's readings
// and gets back which inputs are active, so it can both drive
// PowerManager and log what is keeping the device awake.
//
// An analog input only counts once it means something: an uncalibrated
// trigger never counts (on a board without the trigger mechanism
// installed, the hall sensor rests mid-scale, which the default 0-4095
// calibration reads as ~50% pulled — permanent "activity"), and an
// uncalibrated stick counts only when it actually moves, since its real
// rest position is unknown.

// Bitmask of activity sources.
namespace ActivitySources {
constexpr uint8_t kButton = 1 << 0;
constexpr uint8_t kStick = 1 << 1;
constexpr uint8_t kTrigger = 1 << 2;
constexpr uint8_t kMotion = 1 << 3;
}  // namespace ActivitySources

// Writes a comma-separated list of the sources in `mask` (e.g.
// "stick, motion") into `out`, or "none" if `mask` is 0. Always
// null-terminates (truncating if `outCapacity` is too small).
void describeActivitySources(uint8_t mask, char *out, size_t outCapacity);

// True once the user has run that calibration (i.e. the values differ
// from CalibrationData's uncalibrated defaults).
bool isTriggerCalibrated(const CalibrationData &data);
bool isStickCalibrated(const CalibrationData &data);

struct ActivityInputs {
  bool anyButtonHeld = false;
  int rawStickX = 0;
  int rawStickY = 0;
  // Calibrated readings, as from AnalogCalibration.
  int stickXPercent = 0;
  int stickYPercent = 0;
  int triggerPercent = 0;
  bool motion = false;
};

class ActivityDetector {
 public:
  // A calibrated stick or trigger counts while held at least this far
  // from rest — above stray noise at the edge of a 0% deadzone, well
  // below any deliberate push or pull. Held (not just moved) so steadily
  // driving with the stick or trigger keeps the device awake.
  static constexpr int kHeldPercent = 5;

  // An uncalibrated stick counts when either raw axis moves at least this
  // far from where it last counted — ADC noise is tens of counts, a real
  // push is 1500+.
  static constexpr int kUncalibratedMoveCounts = 200;

  // Call at boot and after every calibration change.
  void setCalibration(const CalibrationData &data);

  // Call every tick. Returns this tick's active sources (see
  // ActivitySources); nonzero means activity.
  uint8_t update(const ActivityInputs &inputs);

  // Sources active this tick that weren't active last tick — for logging
  // what starts keeping the device awake without printing every tick.
  uint8_t newlyActiveSources() const { return newlyActive_; }

 private:
  bool stickActive(const ActivityInputs &inputs);

  bool stickCalibrated_ = false;
  bool triggerCalibrated_ = false;
  bool hasStickReference_ = false;
  int stickReferenceX_ = 0;
  int stickReferenceY_ = 0;
  uint8_t previousActive_ = 0;
  uint8_t newlyActive_ = 0;
};
