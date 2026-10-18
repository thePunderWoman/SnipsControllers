#pragma once

#include <cstdint>

// Abstraction over the XBee's one-byte AT parameters. XbeeControl
// (xbee_control.h) is the real, hardware-dependent implementation; tests
// use a fake.
class XbeeRoleTransport {
 public:
  virtual ~XbeeRoleTransport() = default;
  // Reads a one-byte AT parameter, e.g. "CE".
  virtual bool queryByteParameter(const char *command, uint8_t *outValue) = 0;
  // Writes a one-byte AT parameter without committing or applying it.
  virtual bool writeByteParameter(const char *command, uint8_t value) = 0;
  // "WR" then "AC": commits pending writes to the module's flash and
  // applies them now.
  virtual bool commitAndApply() = 0;
};

enum class XbeeRoleResult {
  kAlreadyConfigured,
  kReconfigured,
  kQueryFailed,
  kSetFailed,
  kNoTransport,
};

// The role every controller's XBee must have: a sleepy Zigbee end
// device.
namespace XbeeRole {

// CE=0: join a droid's network, never form one (a coordinator would
// strand the controller on a PAN of its own).
constexpr uint8_t kCoordinatorEnable = 0;

// SM=1: pin sleep, controlled by SLEEP_RQ. With the factory default
// SM=0 the module never sleeps and ignores SLEEP_RQ. Any nonzero SM also
// makes the module join as an end device instead of a router. Controllers
// don't need to relay traffic, and an end device's parent holds its
// downlinks while it sleeps and passes them on when it polls (every
// 100 ms while awake).
constexpr uint8_t kSleepMode = 1;

// ET=4: the parent keeps a silent end device in its child table for
// 16 minutes (ET n >= 1 means 2^n minutes; the default ET=1 is only
// 2 minutes). Longer than the longest possible sleep, which auto
// power-off caps at 15 minutes of idle, so waking never needs a slow
// rejoin. On an all-XBee3 network the end device sends this to its parent
// when it joins.
constexpr uint8_t kEndDeviceTimeout = 4;

}  // namespace XbeeRole

// Run once at boot. Queries CE, SM and ET, writes only the ones that
// differ from XbeeRole, then commits and applies once, so the module
// leaves and rejoins at most one time. Writes nothing if any query
// fails. transport may be null (returns kNoTransport).
XbeeRoleResult ensureSleepyEndDeviceRole(XbeeRoleTransport *transport);
