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

bool modeAtLeast(PowerManagementMode mode, PowerManagementMode threshold) {
  return static_cast<int>(mode) >= static_cast<int>(threshold);
}

}  // namespace

PowerManagementMode nextPowerManagementMode(PowerManagementMode current) {
  const int next = (static_cast<int>(current) + 1) %
                    static_cast<int>(PowerManagementMode::kCount);
  return static_cast<PowerManagementMode>(next);
}

int nextCascadeTimeoutSec(int currentSec) {
  return nextInList(currentSec, PowerConfigOptions::kCascadeTimeoutsSec,
                     PowerConfigOptions::kCascadeTimeoutCount);
}

int nextPoweroffTimeoutSec(int currentSec) {
  return nextInList(currentSec, PowerConfigOptions::kPoweroffTimeoutsSec,
                     PowerConfigOptions::kPoweroffTimeoutCount);
}

void PowerManager::update(bool activityThisTick, unsigned long nowMs) {
  if (activityThisTick) {
    lastActivityMs_ = nowMs;
  }
  const unsigned long idleMs = nowMs - lastActivityMs_;

  tier_ = PowerTier::kFull;
  const unsigned long dimAtMs =
      static_cast<unsigned long>(config_.dimTimeoutSec) * 1000UL;
  const unsigned long offAtMs =
      dimAtMs + static_cast<unsigned long>(config_.offTimeoutSec) * 1000UL;
  const unsigned long xbeeSleepAtMs =
      offAtMs +
      static_cast<unsigned long>(config_.xbeeSleepTimeoutSec) * 1000UL;

  if (modeAtLeast(config_.mode, PowerManagementMode::kDimOnly) &&
      idleMs >= dimAtMs) {
    tier_ = PowerTier::kDim;
  }
  if (modeAtLeast(config_.mode, PowerManagementMode::kDimAndOff) &&
      idleMs >= offAtMs) {
    tier_ = PowerTier::kOff;
  }
  if (modeAtLeast(config_.mode, PowerManagementMode::kDimOffAndXbeeSleep) &&
      idleMs >= xbeeSleepAtMs) {
    tier_ = PowerTier::kXbeeAsleep;
  }

  // Independent of mode — see header comment.
  const unsigned long poweroffAtMs =
      static_cast<unsigned long>(config_.autoPoweroffTimeoutSec) * 1000UL;
  if (idleMs >= poweroffAtMs) {
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
