#include "power_management.h"

namespace {

// Finds `current` in `options` and returns the next entry, wrapping to
// the first if it's the last (or not found at all, e.g. a value loaded
// from NVS that predates an option-list change).
int nextInList(int current, const int *options, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (options[i] == current) {
      return options[(i + 1) % count];
    }
  }
  return options[0];
}

bool isNever(int timeoutSec) {
  return timeoutSec <= PowerConfigOptions::kNever;
}

unsigned long toMs(int timeoutSec) {
  return static_cast<unsigned long>(timeoutSec) * 1000UL;
}

}  // namespace

int nextCascadeTimeoutSec(int currentSec) {
  return nextInList(currentSec, PowerConfigOptions::kCascadeTimeoutsSec,
                     PowerConfigOptions::kCascadeTimeoutCount);
}

int nextPoweroffTimeoutSec(int currentSec) {
  return nextInList(currentSec, PowerConfigOptions::kPoweroffTimeoutsSec,
                     PowerConfigOptions::kPoweroffTimeoutCount);
}

void applyLegacyPowerMode(int legacyMode, PowerConfig *config) {
  if (legacyMode < 1) config->dimTimeoutSec = PowerConfigOptions::kNever;
  if (legacyMode < 2) config->offTimeoutSec = PowerConfigOptions::kNever;
  if (legacyMode < 3) {
    config->xbeeSleepTimeoutSec = PowerConfigOptions::kNever;
  }
}

void PowerManager::update(bool activityThisTick, unsigned long nowMs) {
  if (activityThisTick) {
    lastActivityMs_ = nowMs;
  }
  const unsigned long idleMs = nowMs - lastActivityMs_;

  // Each stage in use fires its own timeout after the previous one in
  // use; a Never stage is skipped and adds no delay.
  state_ = PowerState();
  unsigned long stageAtMs = 0;
  if (!isNever(config_.dimTimeoutSec)) {
    stageAtMs += toMs(config_.dimTimeoutSec);
    if (idleMs >= stageAtMs) state_.screen = ScreenPower::kDim;
  }
  if (!isNever(config_.offTimeoutSec)) {
    stageAtMs += toMs(config_.offTimeoutSec);
    if (idleMs >= stageAtMs) state_.screen = ScreenPower::kOff;
  }
  if (!isNever(config_.xbeeSleepTimeoutSec)) {
    stageAtMs += toMs(config_.xbeeSleepTimeoutSec);
    if (idleMs >= stageAtMs) state_.xbeeAsleep = true;
  }

  // Independent of the idle stages — see header comment.
  if (!isNever(config_.autoPoweroffTimeoutSec) &&
      idleMs >= toMs(config_.autoPoweroffTimeoutSec)) {
    poweroffPending_ = true;
  }
}

bool PowerManager::consumeShouldPowerOff() {
  if (poweroffPending_) {
    poweroffPending_ = false;
    return true;
  }
  return false;
}

const char *powerStateName(const PowerState &state) {
  switch (state.screen) {
    case ScreenPower::kFull:
      return state.xbeeAsleep ? "full + XBee asleep" : "full";
    case ScreenPower::kDim:
      return state.xbeeAsleep ? "dim + XBee asleep" : "dim";
    case ScreenPower::kOff:
      return state.xbeeAsleep ? "display off + XBee asleep" : "display off";
  }
  return "unknown";
}

bool shouldXbeeSleep(const PowerState &state, bool inLowBatteryCountdown) {
  return state.xbeeAsleep || inLowBatteryCountdown;
}

bool shouldLightSleep(const PowerState &state, bool usbHostConnected,
                      bool charging, bool inLowBatteryCountdown) {
  return state.screen == ScreenPower::kOff && state.xbeeAsleep &&
         !usbHostConnected && !charging && !inLowBatteryCountdown;
}
