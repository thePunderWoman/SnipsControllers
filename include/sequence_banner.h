#pragma once

#include <cstdint>

#include "packet.h"

// Decides when the status screen shows a sequence's name, from the
// sequence fields of each downlink (see DownlinkPacket). Pure: fed every
// decoded downlink plus the current time.
//
// Amidala bumps sequenceId each time a sequence starts, so starting the
// same sequence twice still counts as new. Builds that report progress
// keep the banner up for as long as they keep sending the name with a
// progress value; everything else gets a fixed window from the start,
// whether or not later downlinks still carry the name.
class SequenceBanner {
 public:
  static constexpr unsigned long kShowMs = 5000;

  void update(const DownlinkPacket &downlink, unsigned long nowMs);

  bool visible(unsigned long nowMs) const;
  const char *name() const { return name_; }
  // -1 when the running sequence has no progress to show.
  int progressPercent() const { return progressPercent_; }

 private:
  bool seenAny_ = false;
  uint8_t lastId_ = 0;
  bool active_ = false;
  unsigned long startMs_ = 0;
  char name_[DownlinkPacket::kSequenceNameLength + 1] = {};
  int progressPercent_ = -1;
};
