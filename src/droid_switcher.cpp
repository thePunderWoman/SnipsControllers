#include "droid_switcher.h"

#include "pan_id.h"

DroidSwitchResult DroidSwitcher::switchTo(const char *panId,
                                          XbeeTransport *transport) {
  if (transport == nullptr) {
    return DroidSwitchResult::kNoTransport;
  }
  char fullPanId[PanId::kHexLength + 1];
  if (!PanId::normalize(panId, fullPanId)) {
    return DroidSwitchResult::kInvalidPanId;
  }
  if (!transport->leaveNetwork()) {
    return DroidSwitchResult::kLeaveFailed;
  }
  if (!transport->setPanId(fullPanId)) {
    return DroidSwitchResult::kSetPanFailed;
  }
  if (!transport->rejoinNetwork()) {
    return DroidSwitchResult::kRejoinFailed;
  }
  return DroidSwitchResult::kSuccess;
}
