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
  kInvalidPanId,
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
  // panId may be short ("4133") — it's normalized to the full 16-digit
  // form (see pan_id.h) before reaching the transport, and rejected with
  // kInvalidPanId (before touching the network) if it isn't valid hex.
  // transport may be null (returns kNoTransport without touching it).
  static DroidSwitchResult switchTo(const char *panId,
                                    XbeeTransport *transport);
};
