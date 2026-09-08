#include "battery.h"

ChargeState BatteryMonitor::chargeStateFor(bool stat1High,
                                            bool stat2High) const {
  if (stat1High && stat2High) return ChargeState::kDone;
  if (stat1High && !stat2High) return ChargeState::kCharging;
  if (!stat1High && stat2High) return ChargeState::kRecoverableFault;
  return ChargeState::kLatchedFault;
}

int BatteryMonitor::percentFor(int rawAdc) const {
  if (rawAdc < 0) rawAdc = 0;
  if (rawAdc > kAdcMaxCounts) rawAdc = kAdcMaxCounts;

  const float pinVolts =
      (static_cast<float>(rawAdc) / kAdcMaxCounts) * kAdcFullScaleVolts;
  const float batteryVolts = pinVolts * kVsysDividerRatio;

  float percent = (batteryVolts - kEmptyVoltage) /
                  (kFullVoltage - kEmptyVoltage) * 100.0f;
  if (percent < 0.0f) percent = 0.0f;
  if (percent > 100.0f) percent = 100.0f;
  return static_cast<int>(percent + 0.5f);
}
