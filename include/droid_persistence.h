#pragma once

#include "droid_store.h"

// Thin adapter persisting a DroidStore to NVS (ESP32 Preferences). No
// droid-list logic lives here — see droid_store.h. Excluded from native
// build/coverage (see platformio.ini's [env:native] build_src_filter).
namespace DroidPersistence {

// Returns the stored droid list, or an empty DroidStore if none has been
// saved yet.
DroidStore load();

void save(const DroidStore &store);

}  // namespace DroidPersistence
