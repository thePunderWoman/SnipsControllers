#pragma once

// Pure logic for analog trigger/thumbstick calibration. Knows nothing
// about real ADC reads or persistence — SnipsController.ino (and, later,
// the menu system) supply raw ADC samples and user confirm/advance
// events; calibration_store.h persists the result to NVS.

// Persisted calibration values. Defaults assume an uncalibrated 12-bit ADC
// (0-4095) so trigger/stick still produce a reasonable (if unrefined)
// reading before the user ever runs calibration.
struct CalibrationData {
  int triggerMin = 0;
  int triggerMax = 4095;
  int stickXMin = 0;
  int stickXMax = 4095;
  int stickXCenter = 2048;
  int stickYMin = 0;
  int stickYMax = 4095;
  int stickYCenter = 2048;
};

class AnalogCalibration {
 public:
  // Raw trigger ADC -> 0-100 (0 = released, 100 = fully pulled).
  static int calibrateTrigger(int raw, const CalibrationData &data);

  // Raw stick-axis ADC -> -100..100 (0 = center), with a small deadzone
  // around center so a physically-resting stick reads as exactly 0.
  static int calibrateStickAxis(int raw, int min, int center, int max);

 private:
  static constexpr int kDeadzonePercentOfRange = 3;
};

// Guided two-step trigger calibration: release, then full pull.
class TriggerCalibrationFlow {
 public:
  enum class Step { kAwaitingRelease, kAwaitingFullPull, kDone };

  Step currentStep() const { return step_; }

  // Call when the user confirms ("Enter") at the current step, with the
  // current raw trigger ADC reading to capture. No-op once kDone.
  void confirmStep(int rawAdc);

  int min() const { return min_; }
  int max() const { return max_; }

 private:
  Step step_ = Step::kAwaitingRelease;
  int min_ = 0;
  int max_ = 4095;
};

// Guided stick calibration: center first, then roll to every extreme
// while samples are continuously tracked, then an explicit "done".
class StickCalibrationFlow {
 public:
  enum class Step { kAwaitingCenter, kRolling, kDone };

  Step currentStep() const { return step_; }

  // Step 1: call once when the user confirms the stick is at rest.
  void confirmCenter(int rawX, int rawY);

  // Step 2: call every tick while rolling; no-op outside kRolling.
  void sample(int rawX, int rawY);

  // Finishes step 2; no-op outside kRolling.
  void confirmDone();

  int centerX() const { return centerX_; }
  int centerY() const { return centerY_; }
  int minX() const { return minX_; }
  int maxX() const { return maxX_; }
  int minY() const { return minY_; }
  int maxY() const { return maxY_; }

 private:
  Step step_ = Step::kAwaitingCenter;
  int centerX_ = 0;
  int centerY_ = 0;
  int minX_ = 0;
  int maxX_ = 4095;
  int minY_ = 0;
  int maxY_ = 4095;
};
