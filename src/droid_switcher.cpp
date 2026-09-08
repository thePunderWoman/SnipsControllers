#include "droid_switcher.h"

DroidSwitchResult DroidSwitcher::switchTo(const char *panId,
                                          XbeeTransport *transport) {
  if (transport == nullptr) {
    return DroidSwitchResult::kNoTransport;
  }
  if (!transport->leaveNetwork()) {
    return DroidSwitchResult::kLeaveFailed;
  }
  if (!transport->setPanId(panId)) {
    return DroidSwitchResult::kSetPanFailed;
  }
  if (!transport->rejoinNetwork()) {
    return DroidSwitchResult::kRejoinFailed;
  }
  return DroidSwitchResult::kSuccess;
}
