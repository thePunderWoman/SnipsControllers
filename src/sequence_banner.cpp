#include "sequence_banner.h"

#include <cstring>

void SequenceBanner::update(const DownlinkPacket &downlink,
                            unsigned long nowMs) {
  const bool hasName = downlink.sequenceName[0] != '\0';
  const bool hasProgress =
      downlink.sequenceProgressPercent != DownlinkPacket::kUnknown;
  const bool isNew = !seenAny_ || downlink.sequenceId != lastId_;
  seenAny_ = true;
  lastId_ = downlink.sequenceId;

  if (hasName && isNew) {
    active_ = true;
    startMs_ = nowMs;
  }
  if (!active_) return;

  if (hasProgress) {
    if (!hasName) {
      active_ = false;
      return;
    }
    progressPercent_ = downlink.sequenceProgressPercent > 100
                           ? 100
                           : downlink.sequenceProgressPercent;
  } else {
    progressPercent_ = -1;
  }
  if (hasName) {
    std::strncpy(name_, downlink.sequenceName, sizeof(name_) - 1);
    name_[sizeof(name_) - 1] = '\0';
  }
}

bool SequenceBanner::visible(unsigned long nowMs) const {
  if (!active_) return false;
  return progressPercent_ >= 0 || nowMs - startMs_ < kShowMs;
}
