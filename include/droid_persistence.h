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

// Note on the *currently selected* PAN ID (as opposed to the list above):
// there's deliberately no persistence for it here. Once XbeeControl::
// setPanId() commits a PAN ID via "WR", the XBee module remembers it in
// its own flash across power cycles on its own — it never needs to be
// re-applied at boot, and never changes except when a Switch Droid action
// explicitly calls setPanId() again. SnipsController.ino derives the
// display name for it at boot by querying the module's current PAN ID
// (XbeeControl::queryPanId()) and matching it against the list above.
