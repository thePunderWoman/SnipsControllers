#include "low_battery.h"

#include <cstdio>

void LowBatteryMonitor::update(int batteryPercent, bool isCharging,
                               unsigned long nowMs) {
  blinkOn_ = ((nowMs / kBlinkHalfPeriodMs) % 2) == 0;

  if (isCharging) {
    warning_ = false;
    countdownActive_ = false;
    belowCriticalPending_ = false;
    return;
  }

  if (countdownActive_) {
    if (batteryPercent >= BatteryThresholds::kCriticalPercent +
                              BatteryThresholds::kRecoveryHysteresisPercent) {
      countdownActive_ = false;
      belowCriticalPending_ = false;
    } else {
      const unsigned long elapsedMs = nowMs - countdownStartMs_;
      const long remaining = static_cast<long>(kCountdownSeconds) -
                             static_cast<long>(elapsedMs / 1000UL);
      if (remaining <= 0) {
        secondsRemaining_ = 0;
        poweroffPending_ = true;
      } else {
        secondsRemaining_ = static_cast<int>(remaining);
      }
      return;
    }
  }

  warning_ = batteryPercent <= BatteryThresholds::kWarningPercent &&
             batteryPercent > BatteryThresholds::kCriticalPercent;

  if (batteryPercent <= BatteryThresholds::kCriticalPercent) {
    if (!belowCriticalPending_) {
      belowCriticalPending_ = true;
      belowCriticalSinceMs_ = nowMs;
    } else if (nowMs - belowCriticalSinceMs_ >= kCriticalDebounceMs) {
      countdownActive_ = true;
      countdownStartMs_ = nowMs;
      secondsRemaining_ = kCountdownSeconds;
      warning_ = false;
      belowCriticalPending_ = false;
    }
  } else {
    belowCriticalPending_ = false;
  }
}

bool LowBatteryMonitor::consumeShouldPowerOff() {
  if (poweroffPending_) {
    poweroffPending_ = false;
    return true;
  }
  return false;
}

void renderLowBatteryCountdownScreen(int secondsRemaining,
                                     ScreenBuffer *screen) {
  screen->clear();
  screen->setLine(0, "Low Battery!");
  screen->setLine(1, "Powering down in");
  char line[ScreenBuffer::kMaxLineLength + 1];
  std::snprintf(line, sizeof(line), "%d seconds", secondsRemaining);
  screen->setLine(2, line);
}

void renderBatteryEmptyScreen(ScreenBuffer *screen) {
  screen->clear();
  screen->setLine(0, "Battery Empty");
  screen->setLine(1, "Please charge");
}

void renderChargingFaultScreen(ScreenBuffer *screen) {
  screen->clear();
  screen->setLine(0, "Charging Fault!");
  screen->setLine(1, "Unplug & replug");
  screen->setLine(2, "charger");
}
