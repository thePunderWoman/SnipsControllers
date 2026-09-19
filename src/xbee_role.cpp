#include "xbee_role.h"

namespace {
constexpr uint8_t kRouterCoordinatorEnable = 0;
}  // namespace

XbeeRoleResult ensureRouterRole(XbeeRoleTransport *transport) {
  if (transport == nullptr) {
    return XbeeRoleResult::kNoTransport;
  }
  uint8_t coordinatorEnable = 0;
  if (!transport->queryCoordinatorEnable(&coordinatorEnable)) {
    return XbeeRoleResult::kQueryFailed;
  }
  if (coordinatorEnable == kRouterCoordinatorEnable) {
    return XbeeRoleResult::kAlreadyRouter;
  }
  if (!transport->setCoordinatorEnable(kRouterCoordinatorEnable)) {
    return XbeeRoleResult::kSetFailed;
  }
  return XbeeRoleResult::kSwitchedToRouter;
}
