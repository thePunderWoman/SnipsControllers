#pragma once

#include <cstdint>

// Abstraction over reading/writing the XBee's CE ("Coordinator Enable")
// parameter — 0 = join a network (router), 1 = form one (coordinator).
// XbeeControl (xbee_control.h) is the real, hardware-dependent
// implementation; tests use a fake.
class XbeeRoleTransport {
 public:
  virtual ~XbeeRoleTransport() = default;
  virtual bool queryCoordinatorEnable(uint8_t *outValue) = 0;
  // Writes CE, commits it to the module's flash, and applies it.
  virtual bool setCoordinatorEnable(uint8_t value) = 0;
};

enum class XbeeRoleResult {
  kAlreadyRouter,
  kSwitchedToRouter,
  kQueryFailed,
  kSetFailed,
  kNoTransport,
};

// Every controller must be a router: a coordinator would form its own
// network instead of joining a droid's. Run once at boot — reads CE and,
// if it's anything other than 0, sets it to 0 (see XbeeRoleTransport).
// transport may be null (returns kNoTransport without touching it).
XbeeRoleResult ensureRouterRole(XbeeRoleTransport *transport);
