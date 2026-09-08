#pragma once

#include <cstddef>

// Pure in-memory droid list (name + PAN ID pairs), the data a user builds
// up via the Manage Droids menu screen. Knows nothing about persistence —
// see droid_persistence.h for the thin NVS adapter that saves/restores it.
struct DroidEntry {
  static constexpr size_t kMaxNameLength = 16;
  static constexpr size_t kMaxPanIdLength = 16;  // 64-bit PAN ID, hex

  char name[kMaxNameLength + 1] = {};
  char panId[kMaxPanIdLength + 1] = {};
};

class DroidStore {
 public:
  static constexpr size_t kMaxDroids = 8;

  size_t count() const { return count_; }

  // Bounds-checked; returns a reference to a shared empty entry for an
  // out-of-range index rather than a null/dangling reference.
  const DroidEntry &at(size_t index) const;

  // Returns false (no-op) if already at kMaxDroids capacity.
  bool add(const char *name, const char *panId);

  // Returns false (no-op) if index is out of range.
  bool remove(size_t index);

 private:
  DroidEntry entries_[kMaxDroids];
  size_t count_ = 0;
};
