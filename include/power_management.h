#pragma once

#include <cstddef>

// Pure logic for the low-power mode feature. Knows nothing about real
// buttons, the display, the XBee, or the accelerometer — callers feed in
// "did anything happen this tick" and get back which tier the device
// should be in, plus a one-shot "time to fully power off" flag.
// power_config_store.h persists PowerConfig to NVS; SnipsController.ino
// translates tier transitions into real hardware actions (OLED
// dim/off, XBee sleep pin, performGracefulShutdown()).
//
// Progressive enhancement by design, not by capability-detection: a
// board without the accelerometer or the XBee sleep pin doesn't need
// special-casing here at all. Callers on such a board simply never pass
// true for accelerometer activity, and the kDimOffAndXbeeSleep tier's
// hardware action (driving the sleep pin) is a no-op on unwired
// hardware — see PCB/GPIO_table.md's Accelerometer section.

// How aggressively the device manages power while otherwise running.
// Each level includes everything the previous level does, plus one more
// step. Ordered so relational comparison (`mode >= kDimOnly`) works.
enum class PowerManagementMode {
  kAlwaysOn = 0,           // No power management at all.
  kDimOnly,                // + dim the OLED after dimTimeoutSec idle.
  kDimAndOff,              // + turn the OLED off after offTimeoutSec more.
  kDimOffAndXbeeSleep,     // + sleep the XBee after xbeeSleepTimeoutSec more.
  kCount,
};

// What the device should currently be doing, independent of *why*.
enum class PowerTier {
  kFull,        // Normal operation: OLED at full brightness, XBee awake.
  kDim,         // OLED dimmed.
  kOff,         // OLED off.
  kXbeeAsleep,  // OLED off and XBee asleep.
};

// The discrete option lists each configurable timeout cycles through via
// the menu — small, fixed sets rather than freeform numeric entry, same
// spirit as everything else in this on-device menu.
namespace PowerConfigOptions {

// Cascade stage timeouts (dim / off / XBee-sleep) — each independently
// one of these values.
constexpr int kCascadeTimeoutsSec[] = {5, 10, 15, 30, 60};
constexpr size_t kCascadeTimeoutCount =
    sizeof(kCascadeTimeoutsSec) / sizeof(kCascadeTimeoutsSec[0]);

// Auto full-power-off timeout — user-specified fixed set (1/5/10/15 min),
// with 1 minute as the minimum.
constexpr int kPoweroffTimeoutsSec[] = {60, 300, 600, 900};
constexpr size_t kPoweroffTimeoutCount =
    sizeof(kPoweroffTimeoutsSec) / sizeof(kPoweroffTimeoutsSec[0]);

}  // namespace PowerConfigOptions

// Persisted settings — see power_config_store.h. Defaults are the
// shortest cascade timeouts and a 5-minute auto power-off; mode defaults
// to kAlwaysOn so a device that's never had this configured behaves
// exactly as it did before this feature existed.
struct PowerConfig {
  PowerManagementMode mode = PowerManagementMode::kAlwaysOn;
  int dimTimeoutSec = PowerConfigOptions::kCascadeTimeoutsSec[0];
  int offTimeoutSec = PowerConfigOptions::kCascadeTimeoutsSec[0];
  int xbeeSleepTimeoutSec = PowerConfigOptions::kCascadeTimeoutsSec[0];
  int autoPoweroffTimeoutSec = PowerConfigOptions::kPoweroffTimeoutsSec[1];
};

// Cycles to the next value in a fixed option list, wrapping around —
// used by the menu's Enter action on each Power Config row. Pure
// functions so the menu (and its tests) don't need to know the option
// lists themselves.
PowerManagementMode nextPowerManagementMode(PowerManagementMode current);
int nextCascadeTimeoutSec(int currentSec);
int nextPoweroffTimeoutSec(int currentSec);

class PowerManager {
 public:
  void setConfig(const PowerConfig &config) { config_ = config; }
  const PowerConfig &config() const { return config_; }

  // Call every loop tick. `activityThisTick` is true if any button,
  // thumbstick movement, analog trigger movement, or (if present)
  // accelerometer motion happened this tick — the caller decides what
  // counts, this class only needs to know whether *something* did.
  void update(bool activityThisTick, unsigned long nowMs);

  PowerTier currentTier() const { return tier_; }

  // True exactly once, the tick the idle time first crosses the
  // auto-poweroff threshold — independent of `mode`, per design (a
  // forgotten, fully-idle controller should eventually shut itself off
  // even at kAlwaysOn).
  bool consumeShouldPowerOff();

 private:
  PowerConfig config_;
  unsigned long lastActivityMs_ = 0;
  PowerTier tier_ = PowerTier::kFull;
  bool poweroffPending_ = false;
};
