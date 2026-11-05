#include "display_layout_store.h"

#include <Preferences.h>

namespace {
// Same namespace the old Display Config slot assignments used; their
// "slotN" keys are simply never read again.
constexpr const char *kNamespace = "snips_disp";
constexpr const char *kLayoutKey = "layout";
}  // namespace

StatusLayout DisplayLayoutStore::load() {
  Preferences prefs;
  // Not readOnly: avoids a misleading "nvs_open failed: NOT_FOUND" log
  // on a device that's never saved this namespace yet — see
  // calibration_store.cpp's load() for the full explanation.
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return StatusLayout::kStatusBars;
  }
  const int stored =
      prefs.getInt(kLayoutKey, static_cast<int>(StatusLayout::kStatusBars));
  prefs.end();
  return statusLayoutFromStored(stored);
}

void DisplayLayoutStore::save(StatusLayout layout) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }
  prefs.putInt(kLayoutKey, static_cast<int>(layout));
  prefs.end();
}
