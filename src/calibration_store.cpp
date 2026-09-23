#include "calibration_store.h"

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "snips_cal";
}  // namespace

CalibrationData CalibrationStore::load() {
  CalibrationData data;  // defaults if nothing has been saved yet

  Preferences prefs;
  // Not readOnly: on a device that's never saved this namespace yet
  // (first boot, or after a flash-partition wipe), NVS_READONLY makes
  // nvs_open() fail with ESP_ERR_NVS_NOT_FOUND, which the Preferences
  // library logs as an alarming-looking error regardless of how
  // gracefully the caller handles the resulting begin() == false (we
  // just fall back to defaults either way). Opening read-write instead
  // creates the empty namespace silently — no behavior change, no false
  // "hardware is broken" errors on real hardware's first boot.
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return data;
  }

  data.triggerMin = prefs.getInt("trigMin", data.triggerMin);
  data.triggerMax = prefs.getInt("trigMax", data.triggerMax);
  data.stickXMin = prefs.getInt("xMin", data.stickXMin);
  data.stickXMax = prefs.getInt("xMax", data.stickXMax);
  data.stickXCenter = prefs.getInt("xCenter", data.stickXCenter);
  data.stickYMin = prefs.getInt("yMin", data.stickYMin);
  data.stickYMax = prefs.getInt("yMax", data.stickYMax);
  data.stickYCenter = prefs.getInt("yCenter", data.stickYCenter);
  data.stickDeadzonePercent =
      prefs.getInt("deadzone", data.stickDeadzonePercent);
  prefs.end();

  return data;
}

void CalibrationStore::save(const CalibrationData &data) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  prefs.putInt("trigMin", data.triggerMin);
  prefs.putInt("trigMax", data.triggerMax);
  prefs.putInt("xMin", data.stickXMin);
  prefs.putInt("xMax", data.stickXMax);
  prefs.putInt("xCenter", data.stickXCenter);
  prefs.putInt("yMin", data.stickYMin);
  prefs.putInt("yMax", data.stickYMax);
  prefs.putInt("yCenter", data.stickYCenter);
  prefs.putInt("deadzone", data.stickDeadzonePercent);
  prefs.end();
}
