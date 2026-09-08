#pragma once

#include "complications.h"

// Thin adapter persisting a ComplicationRegistry's slot assignments to
// NVS (ESP32 Preferences). No complication logic lives here — see
// complications.h. Excluded from native build/coverage (see
// platformio.ini's [env:native] build_src_filter).
namespace ComplicationPersistence {

// Applies any previously-saved slot assignments onto `registry` (leaving
// its defaults in place for any slot that's never been saved).
void load(ComplicationRegistry *registry);

void save(const ComplicationRegistry &registry);

}  // namespace ComplicationPersistence
