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

// The currently-selected droid (name + PAN ID) — independent of the list
// above (a separate NVS namespace, so saving the list can never clobber
// it) and independent of list membership: it stays the "current"
// selection even if later removed from the list via Manage Droids. Meant
// to be reapplied to the radio at boot so the controller reconnects to
// the same droid across a power cycle, and overwritten (not
// cleared-then-written — a save already replaces whatever was there)
// every time a Switch Droid action succeeds.
void saveCurrentSelection(const DroidEntry &entry);

// Returns true and fills outEntry if a selection was previously saved.
bool loadCurrentSelection(DroidEntry *outEntry);

// Used by Factory Reset.
void clearCurrentSelection();

}  // namespace DroidPersistence
