#pragma once

#include <cstddef>

#include "brightness_level.h"

// Pure logic for the low-power mode feature. Knows nothing about real
// buttons, the display, the XBee, or the accelerometer — callers feed in
// "did anything happen this tick" and get back what state the device
// should be in, plus a one-shot "time to fully power off" flag.
// power_config_store.h persists PowerConfig to NVS; SnipsController.ino
// translates state changes into real hardware actions (OLED
// dim/off, XBee sleep pin, performGracefulShutdown()).
//
// Progressive enhancement by design, not by capability-detection: a
// board without the accelerometer or the XBee sleep pin doesn't need
// special-casing here at all. Callers on such a board simply never pass
// true for accelerometer activity, and the XBee sleep stage's hardware
// action (driving the sleep pin) is a no-op on unwired hardware — see
// PCB/GPIO_table.md's Accelerometer section.
//
// The idle stages (dim, OLED off, XBee sleep) run in that order, each
// after its own timeout counted from the previous stage that's in use.
// Any stage can be set to Never, which skips just that stage: the
// others still happen, and a skipped stage adds no delay. So Off on
// Never with XBee sleep set leaves the screen on (dimmed, if Dim is set)
// while the radio sleeps.

// What the OLED should currently be doing.
enum class ScreenPower {
  kFull,  // At the user's brightness.
  kDim,   // Dimmed well below it.
  kOff,
};

// What the device should currently be doing, independent of *why*.
struct PowerState {
  ScreenPower screen = ScreenPower::kFull;
  bool xbeeAsleep = false;

  bool operator==(const PowerState &other) const {
    return screen == other.screen && xbeeAsleep == other.xbeeAsleep;
  }
  bool operator!=(const PowerState &other) const { return !(*this == other); }
};

// Short lowercase name for logging state changes, e.g. "dim" or
// "display off + XBee asleep".
const char *powerStateName(const PowerState &state);

// The discrete option lists each configurable timeout cycles through via
// the menu — small, fixed sets rather than freeform numeric entry, same
// spirit as everything else in this on-device menu. Each list ends with
// kNever.
namespace PowerConfigOptions {

// A timeout that never fires: that stage (or auto power-off) is skipped.
constexpr int kNever = 0;

// Cascade stage timeouts (dim / off / XBee-sleep) — each independently
// one of these values.
constexpr int kCascadeTimeoutsSec[] = {5, 10, 15, 30, 60, kNever};
constexpr size_t kCascadeTimeoutCount =
    sizeof(kCascadeTimeoutsSec) / sizeof(kCascadeTimeoutsSec[0]);

// Auto full-power-off timeout — user-specified fixed set (1/5/10/15 min),
// with 1 minute as the minimum.
constexpr int kPoweroffTimeoutsSec[] = {60, 300, 600, 900, kNever};
constexpr size_t kPoweroffTimeoutCount =
    sizeof(kPoweroffTimeoutsSec) / sizeof(kPoweroffTimeoutsSec[0]);

}  // namespace PowerConfigOptions

// Persisted settings — see power_config_store.h. The idle stages default
// to Never, so a device that's never had this configured behaves exactly
// as it did before this feature existed, with a 5-minute auto power-off.
struct PowerConfig {
  int dimTimeoutSec = PowerConfigOptions::kNever;
  int offTimeoutSec = PowerConfigOptions::kNever;
  int xbeeSleepTimeoutSec = PowerConfigOptions::kNever;
  int autoPoweroffTimeoutSec = PowerConfigOptions::kPoweroffTimeoutsSec[1];
  // Brightness settings, edited from the Brightness menu rather than
  // Power Management but stored here with the rest. Not idle stages: they
  // apply whatever the PowerState (the dim stage dims the screen below
  // its setting; see screenDriveFor()). Percentages, see
  // brightness_level.h.
  int ledBrightnessPercent = BrightnessLevel::kMinPercent;
  int screenBrightnessPercent = 80;
};

// Cycles to the next value in a fixed option list, wrapping around —
// used by the menu's Enter action on each Power Config row. Pure
// functions so the menu (and its tests) don't need to know the option
// lists themselves.
int nextCascadeTimeoutSec(int currentSec);
int nextPoweroffTimeoutSec(int currentSec);

// Settings saved before the idle stages had a Never option also stored a
// mode (0 always on, 1 dim only, 2 dim + OLED off, 3 dim + off + XBee
// sleep) that switched stages on in order. Sets each stage that mode
// left out to Never, so an updated controller keeps doing what it did.
// Out-of-range modes match what the old code did with them: negative
// left everything on, above 3 enabled every stage.
void applyLegacyPowerMode(int legacyMode, PowerConfig *config);

// Whether the XBee should be in pin sleep: in the XBee sleep stage, or
// during the low-battery shutdown countdown (every bit of saved current
// extends the safety margin). While it sleeps, loop() sends no uplinks
// and skips the RSSI query.
bool shouldXbeeSleep(const PowerState &state, bool inLowBatteryCountdown);

// How long each light sleep lasts before a timer wake. Buttons, the power
// button and accelerometer motion wake the ESP32 immediately; the stick
// and trigger can't raise a GPIO wake, so they are sampled once per
// period instead.
constexpr unsigned long kLightSleepPeriodMs = 100;

// Whether loop() should light-sleep the ESP32 between passes instead of
// just idling. Only when the OLED is off and the XBee is asleep, so
// nothing needs the CPU between passes. Never
// while a USB host is connected (light sleep drops the USB serial link),
// while charging (USB is powering the controller anyway), or during the
// low-battery countdown (its screen updates every pass).
bool shouldLightSleep(const PowerState &state, bool usbHostConnected,
                      bool charging,
                      bool inLowBatteryCountdown);

class PowerManager {
 public:
  void setConfig(const PowerConfig &config) { config_ = config; }
  const PowerConfig &config() const { return config_; }

  // Call every loop tick. `activityThisTick` is true if any button,
  // thumbstick movement, analog trigger movement, or (if present)
  // accelerometer motion happened this tick — the caller decides what
  // counts, this class only needs to know whether *something* did.
  void update(bool activityThisTick, unsigned long nowMs);

  PowerState currentState() const { return state_; }

  // How long since the last tick with activity, for logging.
  unsigned long idleMs(unsigned long nowMs) const {
    return nowMs - lastActivityMs_;
  }

  // True exactly once, the tick the idle time first crosses the
  // auto-poweroff threshold — independent of the idle stages, per design
  // (a forgotten, fully-idle controller should eventually shut itself off
  // even with every stage on Never), unless it too is set to Never.
  bool consumeShouldPowerOff();

 private:
  PowerConfig config_;
  unsigned long lastActivityMs_ = 0;
  PowerState state_;
  bool poweroffPending_ = false;
};
