#include "power_config_store.h"

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "snips_pwr";
}  // namespace

PowerConfig PowerConfigStore::load() {
  PowerConfig config;  // defaults if nothing has been saved yet

  Preferences prefs;
  // Not readOnly: avoids a misleading "nvs_open failed: NOT_FOUND" log
  // on a device that's never saved this namespace yet — see
  // calibration_store.cpp's load() for the full explanation.
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return config;
  }

  config.dimTimeoutSec = prefs.getInt("dimSec", config.dimTimeoutSec);
  config.offTimeoutSec = prefs.getInt("offSec", config.offTimeoutSec);
  config.xbeeSleepTimeoutSec =
      prefs.getInt("xbeeSec", config.xbeeSleepTimeoutSec);
  config.autoPoweroffTimeoutSec =
      prefs.getInt("offAllSec", config.autoPoweroffTimeoutSec);
  // Saved before each stage had its own Never option; save() drops it.
  if (prefs.isKey("mode")) {
    applyLegacyPowerMode(prefs.getInt("mode", 0), &config);
  }
  // New keys for the percentage settings: the old "ledBright" held a
  // Low/Medium/High index, which would read as a near-zero percentage.
  config.ledBrightnessPercent = BrightnessLevel::sanitize(
      prefs.getInt("ledPct", config.ledBrightnessPercent));
  config.screenBrightnessPercent = BrightnessLevel::sanitize(
      prefs.getInt("scrPct", config.screenBrightnessPercent));
  prefs.end();

  return config;
}

void PowerConfigStore::save(const PowerConfig &config) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  // The Never timeouts now carry what the old mode did (see load()).
  prefs.remove("mode");
  prefs.putInt("dimSec", config.dimTimeoutSec);
  prefs.putInt("offSec", config.offTimeoutSec);
  prefs.putInt("xbeeSec", config.xbeeSleepTimeoutSec);
  prefs.putInt("offAllSec", config.autoPoweroffTimeoutSec);
  prefs.putInt("ledPct", config.ledBrightnessPercent);
  prefs.putInt("scrPct", config.screenBrightnessPercent);
  prefs.end();
}
