#include "complications.h"

#include <cstdio>

const char *complicationSourceLabel(ComplicationSource source) {
  switch (source) {
    case ComplicationSource::kBattery: return "Battery";
    case ComplicationSource::kLeftSlot: return "Left Slot";
    case ComplicationSource::kRightSlot: return "Right Slot";
    case ComplicationSource::kSignal: return "Signal";
    case ComplicationSource::kDroidName: return "Droid Name";
    case ComplicationSource::kHandedness: return "Handedness";
    default: return "Unknown";
  }
}

ComplicationSource ComplicationRegistry::slotSource(int slotIndex) const {
  return (slotIndex >= 0 && slotIndex < kSlotCount)
             ? slots_[slotIndex]
             : ComplicationSource::kBattery;
}

void ComplicationRegistry::setSlotSource(int slotIndex,
                                        ComplicationSource source) {
  if (slotIndex < 0 || slotIndex >= kSlotCount) return;
  slots_[slotIndex] = source;
}

void ComplicationRegistry::cycleSlotSource(int slotIndex) {
  if (slotIndex < 0 || slotIndex >= kSlotCount) return;
  const int next = (static_cast<int>(slots_[slotIndex]) + 1) %
                    static_cast<int>(ComplicationSource::kCount);
  slots_[slotIndex] = static_cast<ComplicationSource>(next);
}

void ComplicationRegistry::render(ScreenBuffer *screen) const {
  screen->clear();
  char line[ScreenBuffer::kMaxLineLength + 1];

  for (int i = 0; i < kSlotCount; ++i) {
    switch (slots_[i]) {
      case ComplicationSource::kBattery:
        if (data_.batteryIndicatorVisible) {
          std::snprintf(line, sizeof(line), "Battery: %d%%",
                        data_.batteryPercent);
        } else {
          line[0] = '\0';
        }
        break;
      case ComplicationSource::kLeftSlot:
        std::snprintf(line, sizeof(line), "%s: %s",
                      data_.leftLabel[0] != '\0' ? data_.leftLabel : "Left",
                      data_.leftValue);
        break;
      case ComplicationSource::kRightSlot:
        std::snprintf(line, sizeof(line), "%s: %s",
                      data_.rightLabel[0] != '\0' ? data_.rightLabel
                                                  : "Right",
                      data_.rightValue);
        break;
      case ComplicationSource::kSignal:
        if (data_.signalKnown) {
          std::snprintf(line, sizeof(line), "Signal: %ddBm",
                        data_.signalDbm);
        } else {
          std::snprintf(line, sizeof(line), "Signal: --");
        }
        break;
      case ComplicationSource::kDroidName:
        std::snprintf(line, sizeof(line), "%s",
                      data_.droidName[0] != '\0' ? data_.droidName
                                                  : "(no droid)");
        break;
      case ComplicationSource::kHandedness:
        switch (data_.handedness) {
          case DownlinkPacket::Handedness::kLeft:
            std::snprintf(line, sizeof(line), "Hand: Left");
            break;
          case DownlinkPacket::Handedness::kRight:
            std::snprintf(line, sizeof(line), "Hand: Right");
            break;
          default:
            std::snprintf(line, sizeof(line), "Hand: ?");
            break;
        }
        break;
      default:
        line[0] = '\0';
        break;
    }
    screen->setLine(i, line);
  }
}
