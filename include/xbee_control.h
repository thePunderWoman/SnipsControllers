#pragma once

// Abstraction over "the ability to control network membership on the
// XBee module" — leave the current network, set a new PAN ID, and
// rejoin. A real implementation using XBee SPI/AT commands lands in
// PR 8; for now XbeeControl below is an honest stub reporting failure,
// so the menu's Switch Droid flow is fully wired end to end but not yet
// functional over real radio.
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
// against any XbeeTransport, fake or real.
class DroidSwitcher {
 public:
  // transport may be null (returns kNoTransport without touching it).
  static DroidSwitchResult switchTo(const char *panId,
                                    XbeeTransport *transport);
};

// Stub XbeeTransport — PR 8 replaces the method bodies with the real
// XBee SPI/API-mode implementation.
class XbeeControl : public XbeeTransport {
 public:
  bool leaveNetwork() override;
  bool setPanId(const char *panId) override;
  bool rejoinNetwork() override;
};
