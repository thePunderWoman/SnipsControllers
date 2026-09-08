#pragma once

#include "calibration.h"

// Thin adapter persisting CalibrationData to NVS (ESP32 Preferences).
// No calibration math lives here — see calibration.h. Excluded from
// native build/coverage (see platformio.ini's [env:native]
// build_src_filter).
namespace CalibrationStore {

// Returns the stored calibration, or CalibrationData's defaults if none
// has been saved yet.
CalibrationData load();

void save(const CalibrationData &data);

}  // namespace CalibrationStore
