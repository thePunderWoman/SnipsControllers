#include "complication_persistence.h"

#include <cstdio>

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "snips_disp";
}  // namespace

void ComplicationPersistence::load(ComplicationRegistry *registry) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return;
  }

  char key[8];
  for (int i = 0; i < ComplicationRegistry::kSlotCount; ++i) {
    std::snprintf(key, sizeof(key), "slot%d", i);
    const int defaultValue = static_cast<int>(registry->slotSource(i));
    const int stored = prefs.getInt(key, defaultValue);
    registry->setSlotSource(i, static_cast<ComplicationSource>(stored));
  }
  prefs.end();
}

void ComplicationPersistence::save(const ComplicationRegistry &registry) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  char key[8];
  for (int i = 0; i < ComplicationRegistry::kSlotCount; ++i) {
    std::snprintf(key, sizeof(key), "slot%d", i);
    prefs.putInt(key, static_cast<int>(registry.slotSource(i)));
  }
  prefs.end();
}
