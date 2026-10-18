#include "xbee_role.h"

#include <cstddef>

namespace {

struct RoleParameter {
  const char *command;
  uint8_t value;
};

constexpr RoleParameter kRoleParameters[] = {
    {"CE", XbeeRole::kCoordinatorEnable},
    {"SM", XbeeRole::kSleepMode},
    {"ET", XbeeRole::kEndDeviceTimeout},
};
constexpr size_t kRoleParameterCount =
    sizeof(kRoleParameters) / sizeof(kRoleParameters[0]);

}  // namespace

XbeeRoleResult ensureSleepyEndDeviceRole(XbeeRoleTransport *transport) {
  if (transport == nullptr) {
    return XbeeRoleResult::kNoTransport;
  }

  bool needsWrite[kRoleParameterCount] = {};
  bool anyNeedsWrite = false;
  for (size_t i = 0; i < kRoleParameterCount; ++i) {
    uint8_t current = 0;
    if (!transport->queryByteParameter(kRoleParameters[i].command, &current)) {
      return XbeeRoleResult::kQueryFailed;
    }
    needsWrite[i] = current != kRoleParameters[i].value;
    anyNeedsWrite = anyNeedsWrite || needsWrite[i];
  }
  if (!anyNeedsWrite) {
    return XbeeRoleResult::kAlreadyConfigured;
  }

  for (size_t i = 0; i < kRoleParameterCount; ++i) {
    if (needsWrite[i] &&
        !transport->writeByteParameter(kRoleParameters[i].command,
                                       kRoleParameters[i].value)) {
      return XbeeRoleResult::kSetFailed;
    }
  }
  if (!transport->commitAndApply()) {
    return XbeeRoleResult::kSetFailed;
  }
  return XbeeRoleResult::kReconfigured;
}
