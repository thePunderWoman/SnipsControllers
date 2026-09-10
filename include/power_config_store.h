#pragma once

#include "power_management.h"

// Thin adapter persisting PowerConfig to NVS (ESP32 Preferences). No
// power-management logic lives here — see power_management.h. Excluded
// from native build/coverage (see platformio.ini's [env:native]
// build_src_filter).
namespace PowerConfigStore {

// Returns the stored config, or PowerConfig's defaults if none has been
// saved yet.
PowerConfig load();

void save(const PowerConfig &config);

}  // namespace PowerConfigStore
