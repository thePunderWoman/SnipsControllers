#include "xbee_control.h"

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

bool XbeeControl::leaveNetwork() {
  // TODO(PR 8): issue the real XBee API-mode leave-network command over
  // SPI once the transport exists.
  return false;
}

bool XbeeControl::setPanId(const char *panId) {
  (void)panId;
  // TODO(PR 8): issue the real "ID" AT command over SPI.
  return false;
}

bool XbeeControl::rejoinNetwork() {
  // TODO(PR 8): trigger rejoin/associate over SPI.
  return false;
}
