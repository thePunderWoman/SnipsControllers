#pragma once

#include <cstddef>

#include "packet.h"
#include "screen.h"

// Maps OLED display slots to data sources for the "normal operating"
// screen (shown whenever the on-device menu is closed) — smartwatch-style
// complications, user-assignable via the Display Config menu screen.
// Pure logic: knows nothing about how the underlying data is obtained
// (XBee queries, ADC reads, etc.) — SnipsController.ino feeds current
// values in every tick via setData().
enum class ComplicationSource {
  kBattery,
  kLeftSlot,
  kRightSlot,
  kSignal,
  kDroidName,
  kHandedness,
  kCount,
};

const char *complicationSourceLabel(ComplicationSource source);

struct ComplicationData {
  int batteryPercent = 0;
  char leftLabel[DownlinkPacket::kFieldLength + 1] = {};
  char leftValue[DownlinkPacket::kFieldLength + 1] = {};
  char rightLabel[DownlinkPacket::kFieldLength + 1] = {};
  char rightValue[DownlinkPacket::kFieldLength + 1] = {};
  int signalDbm = 0;
  bool signalKnown = false;  // distinguishes "0dBm" from "never queried"
  char droidName[17] = {};
  DownlinkPacket::Handedness handedness =
      DownlinkPacket::Handedness::kUnknown;
};

class ComplicationRegistry {
 public:
  static constexpr int kSlotCount = 4;

  ComplicationSource slotSource(int slotIndex) const;

  // Both are bounds-checked no-ops on an out-of-range slotIndex.
  void setSlotSource(int slotIndex, ComplicationSource source);
  void cycleSlotSource(int slotIndex);

  void setData(const ComplicationData &data) { data_ = data; }

  // Renders every slot into consecutive ScreenBuffer lines, one per slot.
  void render(ScreenBuffer *screen) const;

 private:
  ComplicationSource slots_[kSlotCount] = {
      ComplicationSource::kBattery,
      ComplicationSource::kDroidName,
      ComplicationSource::kLeftSlot,
      ComplicationSource::kRightSlot,
  };
  ComplicationData data_;
};
