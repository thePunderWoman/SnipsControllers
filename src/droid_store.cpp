#include "droid_store.h"

#include <cstring>

namespace {
const DroidEntry kEmptyEntry;
}  // namespace

const DroidEntry &DroidStore::at(size_t index) const {
  return index < count_ ? entries_[index] : kEmptyEntry;
}

bool DroidStore::add(const char *name, const char *panId) {
  if (count_ >= kMaxDroids) {
    return false;
  }
  DroidEntry &entry = entries_[count_];
  std::strncpy(entry.name, name, DroidEntry::kMaxNameLength);
  entry.name[DroidEntry::kMaxNameLength] = '\0';
  std::strncpy(entry.panId, panId, DroidEntry::kMaxPanIdLength);
  entry.panId[DroidEntry::kMaxPanIdLength] = '\0';
  ++count_;
  return true;
}

bool DroidStore::remove(size_t index) {
  if (index >= count_) {
    return false;
  }
  for (size_t i = index; i + 1 < count_; ++i) {
    entries_[i] = entries_[i + 1];
  }
  --count_;
  return true;
}
