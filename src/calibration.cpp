#include "calibration.h"

#include <cstdlib>

namespace {

int roundToInt(float value) {
  return value >= 0.0f ? static_cast<int>(value + 0.5f)
                       : static_cast<int>(value - 0.5f);
}

int clamp(int value, int lo, int hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

}  // namespace

int AnalogCalibration::calibrateTrigger(int raw, const CalibrationData &data) {
  if (data.triggerMax <= data.triggerMin) {
    return 0;  // uncalibrated/degenerate range
  }
  const float percent = static_cast<float>(raw - data.triggerMin) /
                        (data.triggerMax - data.triggerMin) * 100.0f;
  return clamp(roundToInt(percent), 0, 100);
}

int AnalogCalibration::calibrateStickAxis(int raw, int min, int center,
                                           int max) {
  if (max <= center || center <= min) {
    return 0;  // uncalibrated/degenerate range
  }

  const float deadzoneHalfWidth =
      static_cast<float>(max - min) * kDeadzonePercentOfRange / 100.0f;
  if (std::abs(raw - center) <= deadzoneHalfWidth) {
    return 0;
  }

  if (raw > center) {
    const float percent =
        static_cast<float>(raw - center) / (max - center) * 100.0f;
    return clamp(roundToInt(percent), 0, 100);
  }

  const float percent =
      static_cast<float>(raw - center) / (center - min) * 100.0f;
  return clamp(roundToInt(percent), -100, 0);
}

void TriggerCalibrationFlow::confirmStep(int rawAdc) {
  switch (step_) {
    case Step::kAwaitingRelease:
      min_ = rawAdc;
      step_ = Step::kAwaitingFullPull;
      break;
    case Step::kAwaitingFullPull:
      max_ = rawAdc;
      step_ = Step::kDone;
      break;
    case Step::kDone:
      break;
  }
}

void StickCalibrationFlow::confirmCenter(int rawX, int rawY) {
  if (step_ != Step::kAwaitingCenter) {
    return;
  }
  centerX_ = rawX;
  centerY_ = rawY;
  minX_ = maxX_ = rawX;
  minY_ = maxY_ = rawY;
  step_ = Step::kRolling;
}

void StickCalibrationFlow::sample(int rawX, int rawY) {
  if (step_ != Step::kRolling) {
    return;
  }
  if (rawX < minX_) minX_ = rawX;
  if (rawX > maxX_) maxX_ = rawX;
  if (rawY < minY_) minY_ = rawY;
  if (rawY > maxY_) maxY_ = rawY;
}

void StickCalibrationFlow::confirmDone() {
  if (step_ != Step::kRolling) {
    return;
  }
  step_ = Step::kDone;
}
