#pragma once

// Abstraction over "the ability to control network membership on the
// XBee module" — leave the current network, set a new PAN ID, and
// rejoin. XbeeControl (xbee_control.h) is the real, hardware-dependent
// implementation; tests use a fake.
class XbeeTransport {
 public:
  virtual ~XbeeTransport() = default;
  virtual bool leaveNetwork() = 0;
  virtual bool setPanId(const char *panId) = 0;
  virtual bool rejoinNetwork() = 0;
};

enum class DroidSwitchResult {
  kSuccess,
  kLeaveFailed,
  kSetPanFailed,
  kRejoinFailed,
  kNoTransport,
};

// Pure orchestration of the leave/set-PAN/rejoin sequence — testable
// against any XbeeTransport, fake or real. Kept in its own file (separate
// from XbeeControl) so it stays compiled and covered natively even though
// XbeeControl itself is hardware-dependent and excluded from that build.
class DroidSwitcher {
 public:
  // transport may be null (returns kNoTransport without touching it).
  static DroidSwitchResult switchTo(const char *panId,
                                    XbeeTransport *transport);
};
