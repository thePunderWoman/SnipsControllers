#include "power_config_store.h"

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "snips_pwr";
}  // namespace

PowerConfig PowerConfigStore::load() {
  PowerConfig config;  // defaults if nothing has been saved yet

  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return config;
  }

  config.mode = static_cast<PowerManagementMode>(
      prefs.getInt("mode", static_cast<int>(config.mode)));
  config.dimTimeoutSec = prefs.getInt("dimSec", config.dimTimeoutSec);
  config.offTimeoutSec = prefs.getInt("offSec", config.offTimeoutSec);
  config.xbeeSleepTimeoutSec =
      prefs.getInt("xbeeSec", config.xbeeSleepTimeoutSec);
  config.autoPoweroffTimeoutSec =
      prefs.getInt("offAllSec", config.autoPoweroffTimeoutSec);
  prefs.end();

  return config;
}

void PowerConfigStore::save(const PowerConfig &config) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  prefs.putInt("mode", static_cast<int>(config.mode));
  prefs.putInt("dimSec", config.dimTimeoutSec);
  prefs.putInt("offSec", config.offTimeoutSec);
  prefs.putInt("xbeeSec", config.xbeeSleepTimeoutSec);
  prefs.putInt("offAllSec", config.autoPoweroffTimeoutSec);
  prefs.end();
}
