#include "status_led.h"

RgbColor StatusLedController::colorFor(SystemState state) const {
  switch (state) {
    case SystemState::kBooting:
      return {0, 0, 255};        // blue
    case SystemState::kConnected:
      return {0, 255, 0};        // green
    case SystemState::kDisconnected:
      return {255, 255, 0};      // yellow
    case SystemState::kCharging:
      return {255, 165, 0};      // orange
    case SystemState::kError:
      return {255, 0, 0};        // red
  }
  return {0, 0, 0};
}
