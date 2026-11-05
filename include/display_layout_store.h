#pragma once

#include "status_screen.h"

// Thin adapter persisting the status screen layout chosen on the Display
// Config menu screen to NVS (ESP32 Preferences). No logic lives here —
// statusLayoutFromStored() (status_screen.h) sanitizes what comes back.
// Excluded from native build/coverage (see platformio.ini's [env:native]
// build_src_filter).
namespace DisplayLayoutStore {

// The saved layout, or StatusLayout::kStatusBars if none has been saved.
StatusLayout load();

void save(StatusLayout layout);

}  // namespace DisplayLayoutStore
