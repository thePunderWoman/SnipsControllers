#pragma once

#include "screen.h"

// Pure logic for the low-battery warning and safe-shutdown countdown.
// Knows nothing about real hardware — SnipsController.ino feeds in the
// already-computed battery percent/charge state each tick and applies
// the real effects (forcing the OLED on, blinking it and the status LED,
// sleeping the XBee, and calling performGracefulShutdown()).
namespace BatteryThresholds {

// Below this (and above critical), the battery indicator and status LED
// blink — still fully usable, just a heads-up.
constexpr int kWarningPercent = 10;

// At/below this, a safe-shutdown countdown starts. Deliberately more
// conservative than "0%" in battery.h's linear model (which is already
// only 3.0V): this design has no separate battery-protection/fuel-gauge
// IC (the bq25185 only charges), so firmware is the last line of
// defense, and needs margin for ADC noise, voltage sag under load, and
// the countdown itself continuing to draw power for 30 more seconds.
constexpr int kCriticalPercent = 5;

// How far a recovering reading must climb back above kCriticalPercent
// to cancel an in-progress countdown on its own (as opposed to actually
// starting to charge, which cancels immediately) — avoids the countdown
// flapping on/off from a reading bouncing right at the threshold.
constexpr int kRecoveryHysteresisPercent = 2;

}  // namespace BatteryThresholds

class LowBatteryMonitor {
 public:
  // Call about once a second (this project's existing telemetry/LED
  // cadence is plenty of resolution for a 30-second countdown and a
  // multi-second blink) with the current battery percent, whether the
  // device is currently charging, and the current time. Charging
  // suppresses the warning and cancels/prevents the countdown entirely.
  void update(int batteryPercent, bool isCharging, unsigned long nowMs);

  // True while percent is at/below kWarningPercent and above critical
  // (never true while charging or during the countdown itself).
  bool isWarning() const { return warning_; }

  bool isInShutdownCountdown() const { return countdownActive_; }

  // Only meaningful while isInShutdownCountdown().
  int secondsRemaining() const { return secondsRemaining_; }

  // Shared blink phase for both the OLED indicator and the status LED,
  // so they flash in lockstep rather than independently. Only
  // meaningful while isWarning() or isInShutdownCountdown().
  bool blinkOn() const { return blinkOn_; }

  // True exactly once, the tick the countdown reaches zero.
  bool consumeShouldPowerOff();

 private:
  static constexpr unsigned long kBlinkHalfPeriodMs = 1000;  // 2s full cycle
  static constexpr unsigned long kCriticalDebounceMs = 3000;
  static constexpr int kCountdownSeconds = 30;

  bool warning_ = false;
  bool blinkOn_ = false;

  bool belowCriticalPending_ = false;
  unsigned long belowCriticalSinceMs_ = 0;

  bool countdownActive_ = false;
  unsigned long countdownStartMs_ = 0;
  int secondsRemaining_ = 0;

  bool poweroffPending_ = false;
};

// Fills `screen` with the safe-shutdown countdown message. Pure —
// SnipsController.ino calls this instead of the normal menu/operating
// screen render whenever LowBatteryMonitor::isInShutdownCountdown().
void renderLowBatteryCountdownScreen(int secondsRemaining,
                                     ScreenBuffer *screen);

// Fills `screen` with the boot-time "too low to safely run" message.
// SnipsController.ino shows this and powers off immediately (no
// countdown — no session has started yet) if the battery is already at
// or below kCriticalPercent the moment the device is turned on.
void renderBatteryEmptyScreen(ScreenBuffer *screen);
