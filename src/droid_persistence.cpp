#include "droid_persistence.h"

#include <cstdio>

#include <Preferences.h>

namespace {
constexpr const char *kNamespace = "snips_droids";
}  // namespace

DroidStore DroidPersistence::load() {
  DroidStore store;

  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/true)) {
    return store;
  }

  const int count = prefs.getInt("count", 0);
  char nameKey[8];
  char panKey[8];
  char name[DroidEntry::kMaxNameLength + 1];
  char panId[DroidEntry::kMaxPanIdLength + 1];

  for (int i = 0; i < count && i < static_cast<int>(DroidStore::kMaxDroids);
       ++i) {
    std::snprintf(nameKey, sizeof(nameKey), "name%d", i);
    std::snprintf(panKey, sizeof(panKey), "pan%d", i);
    prefs.getString(nameKey, name, sizeof(name));
    prefs.getString(panKey, panId, sizeof(panId));
    store.add(name, panId);
  }

  prefs.end();
  return store;
}

void DroidPersistence::save(const DroidStore &store) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, /*readOnly=*/false)) {
    return;
  }

  prefs.clear();  // drop any entries from a previously-longer list
  prefs.putInt("count", static_cast<int>(store.count()));

  char nameKey[8];
  char panKey[8];
  for (size_t i = 0; i < store.count(); ++i) {
    std::snprintf(nameKey, sizeof(nameKey), "name%zu", i);
    std::snprintf(panKey, sizeof(panKey), "pan%zu", i);
    prefs.putString(nameKey, store.at(i).name);
    prefs.putString(panKey, store.at(i).panId);
  }

  prefs.end();
}
