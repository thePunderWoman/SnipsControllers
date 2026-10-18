#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <cstring>

#include "accelerometer.h"
#include "battery.h"
#include "buttons.h"
#include "calibration.h"
#include "calibration_store.h"
#include "complication_persistence.h"
#include "complications.h"
#include "droid_persistence.h"
#include "loop_pacer.h"
#include "low_battery.h"
#include "menu.h"
#include "oled.h"
#include "packet.h"
#include "pin_assignment.h"
#include "power_config_store.h"
#include "power_latch.h"
#include "power_management.h"
#include "rgb_led.h"
#include "screen.h"
#include "snips_logo_bitmap.h"
#include "status_led.h"
#include "xbee_control.h"

// Pure entry point — wiring only. All real logic lives in dedicated
// subsystem files under src/ + include/; this file just owns real
// peripherals and feeds their raw readings into that logic each tick.

namespace {

PowerOffDetector powerOffDetector;
ButtonPanel buttonPanel;
OledDisplay oledDisplay;
RgbLed rgbLed;
StatusLedController statusLedController;
BatteryMonitor batteryMonitor;
CalibrationData calibrationData;
MenuController menuController;
XbeeControl xbeeControl;
ComplicationRegistry complications;
ComplicationData complicationData;
Accelerometer accelerometer;
PowerManager powerManager;
PowerConfig powerConfig;
PowerTier previousPowerTier = PowerTier::kFull;
bool xbeeSleepRequested = false;
LowBatteryMonitor lowBatteryMonitor;
bool wasInLowBatteryCountdown = false;
char deviceSerialLowBuf[9] = {};  // must outlive setup() — see its use below
bool lastReportedPressed[Buttons::kCount] = {};
unsigned long lastTelemetryLogMs = 0;
constexpr unsigned long kTelemetryLogIntervalMs = 1000;
unsigned long lastUplinkSendMs = 0;
constexpr unsigned long kUplinkSendIntervalMs = 50;
unsigned long lastDownlinkMs = 0;  // 0 = never received one
constexpr unsigned long kConnectionTimeoutMs = 5000;
MenuScreen previousMenuScreen = MenuScreen::kInactive;
MainMenuItem previousMainMenuItem = MainMenuItem::kSwitchDroid;

void showBootScreen() {
  ScreenBuffer bootScreen;
  bootScreen.setLine(0, "Snips Controller");
  bootScreen.setLine(1, "OLED OK");
  oledDisplay.render(bootScreen);
}

// The "normal operating" screen, shown whenever the on-device menu is
// closed — smartwatch-style complications, user-assignable via Display
// Config.
void showOperatingScreen() {
  ScreenBuffer screen;
  complications.render(&screen);
  oledDisplay.render(screen);
}

bool oledOnFor(PowerTier tier) {
  return tier != PowerTier::kOff && tier != PowerTier::kXbeeAsleep;
}

// Applies one power tier's OLED effects (see power_management.h) — called
// only on transition, not every tick. The XBee's sleep follows the tier
// too, but through setXbeeSleep() each tick, since the low-battery
// countdown also controls it.
void applyPowerTier(PowerTier tier) {
  const bool oledShouldBeOn = oledOnFor(tier);
  oledDisplay.setPowerOn(oledShouldBeOn);
  oledDisplay.setDimmed(tier == PowerTier::kDim);

  // Coming back from OLED-off: the panel's GDRAM still has whatever was
  // last drawn before it powered off, which is fine for the operating
  // screen (redrawn every second regardless) but could be a stale menu
  // screen if the user backed out while it was off. Force one fresh
  // redraw on wake to be safe.
  if (oledShouldBeOn) {
    if (menuController.currentScreen() != MenuScreen::kInactive) {
      ScreenBuffer menuScreen;
      renderMenuScreen(menuController, &menuScreen);
      oledDisplay.render(menuScreen);
    } else {
      showOperatingScreen();
    }
  }
}

// Drives SLEEP_RQ only when the requested state changes. Takes effect
// because ensureSleepyEndDeviceRole() sets SM=1 at boot (with the factory
// SM=0 the module ignores this pin). Harmless on boards where the pin
// isn't wired to the XBee (see PCB/GPIO_table.md's Accelerometer
// section).
void setXbeeSleep(bool asleep) {
  if (asleep == xbeeSleepRequested) return;
  xbeeSleepRequested = asleep;
  digitalWrite(PinAssignment::kXbeeSleepRq, asleep ? HIGH : LOW);
}

// Light-sleep wake sources (see shouldLightSleep() in power_management.h):
// any button or the power button pressed (all active-low), or
// accelerometer motion (INT1, active-high and non-latched, so it's high
// for as long as motion lasts). Stick and trigger movement can't raise a
// GPIO wake; lightSleepFor()'s timer wake covers them.
void configureLightSleepWakeSources() {
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    gpio_wakeup_enable(static_cast<gpio_num_t>(Buttons::kPins[i]),
                       GPIO_INTR_LOW_LEVEL);
  }
  gpio_wakeup_enable(
      static_cast<gpio_num_t>(PinAssignment::kPowerButtonSense),
      GPIO_INTR_LOW_LEVEL);
  // Only with the chip present: on boards without it, INT1's pin could
  // float high and wake the ESP32 constantly.
  if (accelerometer.isPresent()) {
    gpio_wakeup_enable(static_cast<gpio_num_t>(PinAssignment::kAccelInterrupt),
                       GPIO_INTR_HIGH_LEVEL);
  }
  esp_sleep_enable_gpio_wakeup();
}

// Light-sleeps for up to `ms`, returning early on any wake source above.
// millis() keeps counting through the sleep. The power latch and SLEEP_RQ
// are held at their current levels while asleep: the latch dropping would
// cut power, and SLEEP_RQ dropping would wake the XBee.
void lightSleepFor(unsigned long ms) {
  const auto latchPin = static_cast<gpio_num_t>(PinAssignment::kPowerLatchHold);
  const auto sleepRqPin = static_cast<gpio_num_t>(PinAssignment::kXbeeSleepRq);
  gpio_hold_en(latchPin);
  gpio_hold_en(sleepRqPin);
  esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(ms) * 1000ULL);
  esp_light_sleep_start();
  gpio_hold_dis(sleepRqPin);
  gpio_hold_dis(latchPin);
}

// The status color for `state` at the user's chosen LED brightness (see
// status_led.h).
RgbColor statusColor(SystemState state) {
  return scaleColor(statusLedController.colorFor(state),
                    ledBrightnessScale(powerConfig.ledBrightness));
}

void copyField(char *dest, size_t destCapacity, const char *src) {
  std::strncpy(dest, src, destCapacity - 1);
  dest[destCapacity - 1] = '\0';
}

// Notifies Amidala this controller is powering off intentionally (so it
// doesn't wait out a stale-connection timeout), shows a brief message,
// then cuts power. Capped total duration (a few hundred ms) so a
// non-responsive radio can't hang the shutdown indefinitely.
void performGracefulShutdown() {
  Serial.println("Shutting down...");

  ScreenBuffer shutdownScreen;
  shutdownScreen.setLine(0, "Powering Off...");
  oledDisplay.render(shutdownScreen);

  // Wake the XBee if it's asleep (deepest idle tier, or the low-battery
  // countdown), or the shutdown packets below never leave. The retry
  // delays give it time to wake and poll its parent.
  setXbeeSleep(false);

  UplinkPacket shutdownPacket;
  shutdownPacket.flags = UplinkPacket::kFlagShuttingDown;
  uint8_t buf[Packet::kUplinkEncodedSize];
  const size_t length =
      Packet::encodeUplink(shutdownPacket, buf, sizeof(buf));

  constexpr int kShutdownRetries = 3;
  constexpr unsigned long kShutdownRetryDelayMs = 100;
  for (int i = 0; i < kShutdownRetries; ++i) {
    if (length > 0) {
      xbeeControl.sendPacket(buf, static_cast<uint16_t>(length));
    }
    delay(kShutdownRetryDelayMs);
  }

  digitalWrite(PinAssignment::kPowerLatchHold, LOW);
  // The rail should collapse almost immediately; spin here rather than
  // falling back into loop() in an undefined half-shutdown state in case
  // it doesn't.
  while (true) {
  }
}

const char *chargeStateName(ChargeState state) {
  switch (state) {
    case ChargeState::kDone: return "done";
    case ChargeState::kCharging: return "charging";
    case ChargeState::kRecoverableFault: return "recoverable-fault";
    case ChargeState::kLatchedFault: return "latched-fault";
    default: return "unknown";
  }
}

}  // namespace

void setup() {
  // Must be the very first thing that happens: the soft-latch circuit only
  // keeps the 3.3V rail up because the power button is physically held.
  // If this pin isn't driven HIGH before the user releases the button,
  // power collapses before the rest of setup() ever runs. See
  // PCB/README.md's "Power Architecture" section.
  pinMode(PinAssignment::kPowerLatchHold, OUTPUT);
  digitalWrite(PinAssignment::kPowerLatchHold, HIGH);

  // XBee SLEEP_RQ: must be driven low (stay awake) before anything else
  // touches it — see pin_assignment.h. Harmless on boards where this pin
  // isn't wired to the XBee at all (see PCB/GPIO_table.md).
  pinMode(PinAssignment::kXbeeSleepRq, OUTPUT);
  digitalWrite(PinAssignment::kXbeeSleepRq, LOW);

  // 80 MHz instead of the default 240 MHz. Wi-Fi and Bluetooth are never
  // started, and nothing here is compute-bound: the SPI, I2C, RMT and USB
  // peripherals run from the 80 MHz APB clock either way, so only the CPU
  // core slows down. 80 MHz is the lowest speed that keeps the APB at
  // 80 MHz (below it, peripheral clocks drop too).
  setCpuFrequencyMhz(80);

  Serial.begin(115200);

  // Bring-up aid: this board has no external USB-UART bridge chip (bare
  // WROOM-1 module, see PCB/GPIO_table.md) — Serial goes over the
  // ESP32-S3's native USB-CDC peripheral, which has to fully
  // re-enumerate with the host after every reset. That consistently
  // takes longer than this firmware needs to reach its early boot
  // prints, so they were getting lost even with a monitor already open
  // and waiting -- 2000ms wasn't long enough to reliably cover a real
  // dev machine's reconnect time (observed several seconds on one
  // laptop). HWCDC's bool operator reflects the real host-connection
  // state, not just whether begin() was called.
  //
  // TODO(bring-up): this wait fires on every boot with no host attached
  // too -- i.e. every real-world power-on once this ships, not just
  // during debugging. 5s of dead time before the splash even shows up
  // is fine for now but not acceptable in the field; shorten this back
  // down (or make it conditional on something cheap to detect, like a
  // held button) before this goes to an actual user.
  constexpr unsigned long kSerialWaitMs = 5000;
  const unsigned long serialWaitStartMs = millis();
  while (!Serial && millis() - serialWaitStartMs < kSerialWaitMs) {
    delay(10);
  }

  // If the battery is already at/below the critical threshold the
  // instant the device is turned on (e.g. it sat unused long enough to
  // self-discharge), don't run a full boot — show a brief message and
  // cut power immediately rather than starting a session on a battery
  // that's already past the safe-shutdown line. No countdown here (see
  // low_battery.h): unlike the running-countdown case, no session has
  // started yet, so there's nothing to gracefully wind down.
  {
    const int rawVsysAtBoot = analogRead(PinAssignment::kVsysSense);
    const int bootBatteryPercent = batteryMonitor.percentFor(rawVsysAtBoot);
    if (bootBatteryPercent <= BatteryThresholds::kCriticalPercent) {
      Serial.println("Battery critical at boot -- powering off.");
      if (oledDisplay.begin()) {
        ScreenBuffer emptyScreen;
        renderBatteryEmptyScreen(&emptyScreen);
        oledDisplay.render(emptyScreen);
      }
      constexpr unsigned long kBatteryEmptyMessageMs = 3000;
      delay(kBatteryEmptyMessageMs);
      digitalWrite(PinAssignment::kPowerLatchHold, LOW);
      while (true) {
      }
    }
  }

  // Confirmed against real hardware during bring-up: this line idles HIGH
  // (100kOhm R_PWR_SENSE pull-up to 3V3, per PCB/stackup/board.kdl)
  // and the switch pulls it to GND when held — active-low, same sense as
  // every other button, just via an external pull-up instead of the
  // internal one. The original active-high assumption had this backwards,
  // which read the button as continuously held from boot and triggered an
  // unwanted graceful-shutdown 3 seconds into every session.
  pinMode(PinAssignment::kPowerButtonSense, INPUT);

  // All buttons wire to GND with the internal pull-up enabled, so LOW =
  // pressed. No classification happens here — Amidala owns single/double/
  // long-press and alt semantics centrally; this firmware only reports
  // debounced raw press/release, via the uplink packet below.
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    pinMode(Buttons::kPins[i], INPUT_PULLUP);
  }

  // bq25185 STAT1/STAT2: open-drain, external 10kOhm pull-up to 3V3 (see
  // PCB/GPIO_table.md) — plain INPUT, no internal pull needed.
  pinMode(PinAssignment::kChargeStat1, INPUT);
  pinMode(PinAssignment::kChargeStat2, INPUT);

  // Restores any previously-run trigger/stick calibration; defaults to an
  // uncalibrated full ADC range if none has been saved yet.
  calibrationData = CalibrationStore::load();
  menuController.setStickDeadzonePercent(calibrationData.stickDeadzonePercent);

  // Restores any previously-saved droid list; defaults to empty if none
  // has been saved yet.
  menuController.setDroidStore(DroidPersistence::load());

  // Restores any previously-saved Display Config slot assignments;
  // defaults are left in place for anything never saved.
  ComplicationPersistence::load(&complications);
  menuController.setComplications(&complications);

  // Restores any previously-saved power management settings; defaults to
  // kAlwaysOn (no behavior change) if none has been saved yet.
  powerConfig = PowerConfigStore::load();
  powerManager.setConfig(powerConfig);
  menuController.setPowerConfig(&powerConfig);

  // Absent entirely on the first production boards (see
  // PCB/GPIO_table.md's Accelerometer section) — motion just never
  // counts as activity in that case, no special-casing needed elsewhere.
  if (!accelerometer.begin()) {
    Serial.println("Accelerometer not found at boot (expected on boards "
                   "without it).");
  }

  // After the buttons' pinMode() and accelerometer.begin(), so the wake
  // pins are configured and isPresent() is known.
  configureLightSleepWakeSources();

  xbeeControl.begin();

  // Bring-up diagnostic: with R_XBEE_ATTN_PU1 (10kOhm to 3V3) in place,
  // ATTN should read HIGH here (idle, no frame queued) whenever the
  // module is actually connected and powered — regardless of whether any
  // AT command below succeeds. A LOW or erratic reading instead points at
  // a physical connection problem (e.g. the module's THT socket, used
  // for prototyping per PCB/GPIO_table.md) rather than at command
  // sequencing/timing.
  Serial.print("XBee ATTN pin at boot: ");
  Serial.println(digitalRead(PinAssignment::kXbeeSpiAttn) == HIGH
                     ? "HIGH (expected idle)"
                     : "LOW (unexpected -- check module seating/wiring)");

  // A controller must join a droid's network as a sleepy end device —
  // see xbee_role.h for CE/SM/ET.
  switch (ensureSleepyEndDeviceRole(&xbeeControl)) {
    case XbeeRoleResult::kReconfigured:
      Serial.println("XBee reconfigured as a sleepy end device (CE/SM/ET).");
      break;
    case XbeeRoleResult::kQueryFailed:
      Serial.println("XBee role (CE/SM/ET) query failed at boot.");
      break;
    case XbeeRoleResult::kSetFailed:
      Serial.println("XBee role (CE/SM/ET) could not be set at boot.");
      break;
    case XbeeRoleResult::kAlreadyConfigured:
    case XbeeRoleResult::kNoTransport:
      break;
  }

  menuController.setXbeeTransport(&xbeeControl);

  // The XBee module remembers its own PAN ID across power cycles once
  // XbeeControl::setPanId() commits one via "WR" — it never needs to be
  // re-applied here. This just derives which saved droid (by name) that
  // PAN ID corresponds to, for the "Droid Name" complication; no match
  // (e.g. first boot, or after a Factory Reset clears the list) leaves
  // the "(none)" default in place.
  char currentPanId[17];
  if (xbeeControl.queryPanId(currentPanId, sizeof(currentPanId))) {
    const DroidStore &droidStore = menuController.droidStore();
    size_t matchIndex;
    if (droidStore.findByPanId(currentPanId, &matchIndex)) {
      menuController.setCurrentDroidName(droidStore.at(matchIndex).name);
    }
  } else {
    Serial.println("XBee PAN ID query failed at boot.");
  }

  // The module's SL is fixed hardware, so querying it once at boot (for
  // the Device Info screen) is enough — no need to re-query per menu
  // visit. deviceSerialLowBuf must outlive setup() since MenuController
  // only stores the pointer it's given, not a copy.
  if (xbeeControl.querySerialLow(deviceSerialLowBuf,
                                 sizeof(deviceSerialLowBuf))) {
    menuController.setDeviceSerialLow(deviceSerialLowBuf);
  } else {
    Serial.println("XBee SL query failed at boot.");
  }

  // Power-on splash, so it's obvious the device is booting rather than
  // just dark/unresponsive, followed by the existing brief boot
  // confirmation text — loop() takes over with the real complications
  // screen once real sensor data starts flowing.
  if (oledDisplay.begin()) {
    oledDisplay.renderBitmapCentered(SnipsLogo::kBitmap, SnipsLogo::kWidth,
                                     SnipsLogo::kHeight);
    constexpr unsigned long kBootSplashMs = 1200;
    delay(kBootSplashMs);
    showBootScreen();
  } else {
    Serial.println("OLED not found at boot.");
  }

  // Bring-up check per PCB/README.md's recommended order: cycle through
  // every status color once to prove RMT output on the real LED. Real
  // state takes over once loop() starts running (see the once-a-second
  // block below).
  rgbLed.begin();
  const SystemState bringUpSequence[] = {
      SystemState::kBooting, SystemState::kConnected,
      SystemState::kDisconnected, SystemState::kCharging,
      SystemState::kError,
  };
  for (SystemState state : bringUpSequence) {
    rgbLed.show(statusColor(state));
    delay(300);
  }
  rgbLed.show(statusColor(SystemState::kDisconnected));
}

void loop() {
  const unsigned long now = millis();

  const bool powerButtonHeld =
      digitalRead(PinAssignment::kPowerButtonSense) == LOW;

  if (powerOffDetector.update(powerButtonHeld, now)) {
    performGracefulShutdown();  // never returns
  }

  // Read every tick (not just on the telemetry throttle below) — the menu
  // needs a fresh sample at the exact moment of each button press for
  // calibration, and the stick's "roll to extremes" step needs continuous
  // per-tick sampling.
  const int rawTrigger = analogRead(PinAssignment::kAnalogTrigger);
  const int rawStickX = analogRead(PinAssignment::kThumbstickX);
  const int rawStickY = analogRead(PinAssignment::kThumbstickY);

  // Calibrated early (rather than down with the rest of the telemetry
  // below) because the menu's stick-based navigation needs the
  // calibrated percentage, not the raw ADC reading, to pick a threshold
  // that means the same thing on every controller regardless of that
  // unit's own calibration.
  const int triggerPercent =
      AnalogCalibration::calibrateTrigger(rawTrigger, calibrationData);
  const int stickXPercent = AnalogCalibration::calibrateStickAxis(
      rawStickX, calibrationData.stickXMin, calibrationData.stickXCenter,
      calibrationData.stickXMax, calibrationData.stickDeadzonePercent);
  const int stickYPercent = AnalogCalibration::calibrateStickAxis(
      rawStickY, calibrationData.stickYMin, calibrationData.stickYCenter,
      calibrationData.stickYMax, calibrationData.stickDeadzonePercent);

  bool justPressed[Buttons::kCount] = {};
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    const bool rawPressed = digitalRead(Buttons::kPins[i]) == LOW;
    buttonPanel.update(i, rawPressed, now);

    const bool pressed = buttonPanel.isPressed(i);
    justPressed[i] = pressed && !lastReportedPressed[i];
    if (pressed != lastReportedPressed[i]) {
      lastReportedPressed[i] = pressed;
      Serial.print(Buttons::name(i));
      Serial.println(pressed ? " pressed" : " released");
    }
  }

  // On-device menu: Left Up+Down held together opens it; once open, Left
  // Up/Down scroll, Stick Click confirms, Bumper backs out. These four
  // buttons are "stolen" for navigation only while the menu is active —
  // their uplink bits get suppressed below so Amidala doesn't see spurious
  // presses from menu use (e.g. nudging whatever the Left slot is assigned
  // to while the user is just scrolling a menu).
  menuController.updateOpenCombo(buttonPanel.isPressed(Buttons::kLeftUp),
                                 buttonPanel.isPressed(Buttons::kLeftDown),
                                 now);
  // True if anything below actually calls a MenuController mutator this
  // tick — the only reliable way to know the active screen's content may
  // have changed. currentScreen()/selectedMainMenuItem() alone miss any
  // in-place change that doesn't also switch screens (list scrolling,
  // text-entry letter changes, calibration step progress, Display/Power
  // Config value cycling, Stick Deadzone's value, ...); this and
  // menuStateChanged below both feed the redraw decision that follows.
  bool menuInputHandled = false;

  const bool stickNavFired =
      menuController.tick(rawStickX, rawStickY, stickYPercent);
  if (stickNavFired) menuInputHandled = true;

  // Button Test (see menu.h's onButtonTestPress()) wants to see every raw
  // button press, including the four normally "stolen" for menu nav
  // below — so while it's active, route all of them there instead of
  // through the usual nav calls. Bumper is the one exception: it still
  // exits via onBack() same as every other screen, which doubles as the
  // test for Bumper itself.
  if (menuController.currentScreen() == MenuScreen::kButtonTest) {
    for (size_t i = 0; i < Buttons::kCount; ++i) {
      if (justPressed[i]) {
        menuController.onButtonTestPress(i);
        menuInputHandled = true;
      }
    }
  } else {
    if (justPressed[Buttons::kLeftUp]) {
      menuController.onUp();
      menuInputHandled = true;
    }
    if (justPressed[Buttons::kLeftDown]) {
      menuController.onDown();
      menuInputHandled = true;
    }
    // Calibration confirms with Macro1 instead of Stick Click: clicking
    // the stick itself risks nudging it right as its center/extreme
    // position is being captured, and there's no button actually
    // labeled "Enter" on this controller for Stick Click to stand in
    // for. Every other screen still confirms with Stick Click.
    const bool onCalibrationScreen =
        menuController.currentScreen() == MenuScreen::kCalibrateStick ||
        menuController.currentScreen() == MenuScreen::kCalibrateTrigger;
    const bool confirmPressed = onCalibrationScreen
                                    ? justPressed[Buttons::kMacro1]
                                    : justPressed[Buttons::kStickClick];
    if (confirmPressed) {
      menuController.onEnter(rawTrigger, rawStickX, rawStickY);
      menuInputHandled = true;
    }
  }
  if (justPressed[Buttons::kBumper]) {
    menuController.onBack();
    menuInputHandled = true;
  }

  int newTriggerMin, newTriggerMax;
  if (menuController.consumeNewTriggerCalibration(&newTriggerMin,
                                                  &newTriggerMax)) {
    calibrationData.triggerMin = newTriggerMin;
    calibrationData.triggerMax = newTriggerMax;
    CalibrationStore::save(calibrationData);
    Serial.println("Trigger calibration saved.");
  }

  int newCenterX, newCenterY, newMinX, newMaxX, newMinY, newMaxY;
  if (menuController.consumeNewStickCalibration(&newCenterX, &newCenterY,
                                                &newMinX, &newMaxX, &newMinY,
                                                &newMaxY)) {
    calibrationData.stickXCenter = newCenterX;
    calibrationData.stickYCenter = newCenterY;
    calibrationData.stickXMin = newMinX;
    calibrationData.stickXMax = newMaxX;
    calibrationData.stickYMin = newMinY;
    calibrationData.stickYMax = newMaxY;
    CalibrationStore::save(calibrationData);
    Serial.println("Stick calibration saved.");
  }

  if (menuController.consumeFactoryResetConfirmed()) {
    calibrationData = CalibrationData();
    CalibrationStore::save(calibrationData);
    Serial.println("Factory reset: calibration cleared.");
  }

  // MenuController clears its in-memory droid list as part of factory
  // reset too, and reports that here like any other droid-list change.
  if (menuController.consumeDroidStoreChanged()) {
    DroidPersistence::save(menuController.droidStore());
    Serial.println("Droid list saved.");
  }

  // Only touch the display when something actually changed — a full
  // redraw every tick would be needless I2C traffic for static text.
  // currentScreen()/selectedMainMenuItem() catch entering a new screen
  // (including the menu-open transition, which isn't tied to a button
  // edge — see updateOpenCombo()); menuInputHandled catches everything
  // else that can change a screen's content without changing screens
  // (list scrolling, text-entry letters, calibration progress, Display/
  // Power Config/Stick Deadzone values, Button Test's last-pressed
  // button, ...). Relying on currentScreen()/selectedMainMenuItem()
  // alone missed all of those — a screen rendered once on entry and
  // then silently never updated again.
  const bool menuStateChanged =
      menuController.currentScreen() != previousMenuScreen ||
      menuController.selectedMainMenuItem() != previousMainMenuItem ||
      menuInputHandled;
  previousMenuScreen = menuController.currentScreen();
  previousMainMenuItem = menuController.selectedMainMenuItem();

  if (menuStateChanged) {
    if (menuController.currentScreen() != MenuScreen::kInactive) {
      ScreenBuffer menuScreen;
      renderMenuScreen(menuController, &menuScreen);
      oledDisplay.render(menuScreen);
    } else {
      showOperatingScreen();
    }
  }

  if (menuController.consumeComplicationsChanged()) {
    ComplicationPersistence::save(complications);
    Serial.println("Display config saved.");
  }

  if (menuController.consumePowerConfigChanged()) {
    PowerConfigStore::save(powerConfig);
    powerManager.setConfig(powerConfig);
    Serial.println("Power config saved.");
  }

  if (menuController.consumeStickDeadzoneChanged()) {
    calibrationData.stickDeadzonePercent = menuController.stickDeadzonePercent();
    CalibrationStore::save(calibrationData);
    Serial.println("Stick deadzone saved.");
  }

  const int rawVsys = analogRead(PinAssignment::kVsysSense);
  const bool stat1High = digitalRead(PinAssignment::kChargeStat1) == HIGH;
  const bool stat2High = digitalRead(PinAssignment::kChargeStat2) == HIGH;

  // Low-power mode's idea of "activity" — any button held, the stick or
  // trigger moved off center/released, or (if present) accelerometer
  // motion. Buttons use level (isPressed), not just the press edge, so
  // holding one down doesn't let the screen dim out from under a long
  // press.
  bool anyButtonHeld = false;
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    if (buttonPanel.isPressed(i)) {
      anyButtonHeld = true;
      break;
    }
  }
  const bool activityThisTick =
      anyButtonHeld || stickXPercent != 0 || stickYPercent != 0 ||
      triggerPercent != 0 ||
      (accelerometer.isPresent() && accelerometer.motionDetected());
  powerManager.update(activityThisTick, now);

  const PowerTier currentPowerTier = powerManager.currentTier();
  if (currentPowerTier != previousPowerTier) {
    applyPowerTier(currentPowerTier);
    previousPowerTier = currentPowerTier;
  }

  if (powerManager.consumeShouldPowerOff()) {
    performGracefulShutdown();  // never returns
  }

  const int batteryPercent = batteryMonitor.percentFor(rawVsys);
  const ChargeState chargeState =
      batteryMonitor.chargeStateFor(stat1High, stat2High);

  // Low battery warning/safe-shutdown — see low_battery.h. Checked every
  // tick (not just the once-a-second block below) so the countdown's
  // poweroff fires promptly once it reaches zero.
  lowBatteryMonitor.update(batteryPercent,
                           chargeState == ChargeState::kCharging, now);
  if (lowBatteryMonitor.consumeShouldPowerOff()) {
    performGracefulShutdown();  // never returns
  }

  const bool inLowBatteryCountdown = lowBatteryMonitor.isInShutdownCountdown();
  setXbeeSleep(shouldXbeeSleep(currentPowerTier, inLowBatteryCountdown));

  // Uplink: sent periodically over the radio, faster than the
  // human-readable Serial telemetry below — this is what Amidala
  // actually sees.
  // Nothing to send through while the XBee sleeps; Amidala sees the
  // controller go quiet, the same as if it were switched off.
  if (!xbeeSleepRequested && now - lastUplinkSendMs >= kUplinkSendIntervalMs) {
    lastUplinkSendMs = now;

    const bool menuActive =
        menuController.currentScreen() != MenuScreen::kInactive;

    UplinkPacket uplink;
    for (size_t i = 0; i < Buttons::kCount; ++i) {
      const bool isMenuNavButton = i == Buttons::kLeftUp ||
                                   i == Buttons::kLeftDown ||
                                   i == Buttons::kStickClick ||
                                   i == Buttons::kBumper;
      if (buttonPanel.isPressed(i) && !(menuActive && isMenuNavButton)) {
        uplink.buttonMask |= static_cast<uint16_t>(1u << i);
      }
    }
    uplink.triggerPercent = static_cast<uint8_t>(triggerPercent);
    // X sign convention confirmed against this controller's own hardware
    // (positive = physically left, negative = right) but NOT against
    // what Amidala expects -- as of this writing, SnipsRemote just
    // stores stickXPercent as telemetry (see its header comment) and
    // nothing there actually reads it for steering yet, so there's no
    // established convention to match. Whoever wires this up to real
    // driving logic on that side needs to confirm which sign it wants,
    // and either that code or this line adapts -- not both silently
    // assuming different things.
    uplink.stickXPercent = static_cast<int8_t>(stickXPercent);
    uplink.stickYPercent = static_cast<int8_t>(stickYPercent);
    uplink.batteryPercent = static_cast<uint8_t>(batteryPercent);
    uplink.chargeState = chargeState;

    uint8_t uplinkBuf[Packet::kUplinkEncodedSize];
    const size_t uplinkLength =
        Packet::encodeUplink(uplink, uplinkBuf, sizeof(uplinkBuf));
    if (uplinkLength > 0) {
      xbeeControl.sendPacket(uplinkBuf, static_cast<uint16_t>(uplinkLength));
    }
  }

  // Downlink: non-blocking poll every tick. Handedness and the Left/Right
  // label+value feed the complications system directly — a received
  // field is sticky (kept displayed) until a newer downlink updates it.
  uint8_t downlinkBuf[Packet::kDownlinkEncodedSize];
  uint16_t downlinkLength = 0;
  if (xbeeControl.pollForPacket(downlinkBuf, sizeof(downlinkBuf),
                                &downlinkLength)) {
    DownlinkPacket downlink;
    if (Packet::decodeDownlink(downlinkBuf, downlinkLength, &downlink)) {
      lastDownlinkMs = now;
      complicationData.handedness = downlink.handedness;
      copyField(complicationData.leftLabel, sizeof(complicationData.leftLabel),
               downlink.leftLabel);
      copyField(complicationData.leftValue, sizeof(complicationData.leftValue),
               downlink.leftValue);
      copyField(complicationData.rightLabel,
               sizeof(complicationData.rightLabel), downlink.rightLabel);
      copyField(complicationData.rightValue,
               sizeof(complicationData.rightValue), downlink.rightValue);
      Serial.println("Downlink packet received.");
    }
  }

  // Feed the complications system every tick — cheap, and keeps the
  // operating screen's next scheduled redraw (below) always showing
  // current data.
  complicationData.batteryPercent = batteryPercent;
  complicationData.batteryIndicatorVisible =
      !lowBatteryMonitor.isWarning() || lowBatteryMonitor.blinkOn();
  copyField(complicationData.droidName, sizeof(complicationData.droidName),
           menuController.currentDroidName());
  complications.setData(complicationData);

  // Once a second: query local signal strength (blocking, up to ~200ms —
  // too slow to do every tick), update the status LED, refresh the
  // operating screen so it doesn't just sit stale between menu-state
  // changes, and log human-readable telemetry (not what Amidala sees,
  // just a bring-up check that the values above look right).
  if (now - lastTelemetryLogMs >= kTelemetryLogIntervalMs) {
    lastTelemetryLogMs = now;

    int rssiDbm = 0;
    if (!xbeeSleepRequested && xbeeControl.queryLocalRssiDbm(&rssiDbm)) {
      complicationData.signalDbm = rssiDbm;
      complicationData.signalKnown = true;
      complications.setData(complicationData);
    }

    SystemState ledState;
    if (chargeState == ChargeState::kLatchedFault) {
      ledState = SystemState::kError;
    } else if (chargeState == ChargeState::kCharging) {
      ledState = SystemState::kCharging;
    } else if (lastDownlinkMs != 0 &&
               now - lastDownlinkMs < kConnectionTimeoutMs) {
      ledState = SystemState::kConnected;
    } else {
      ledState = SystemState::kDisconnected;
    }
    // Blinks the status LED in lockstep with the on-screen battery
    // indicator (see low_battery.h) rather than showing its normal
    // color solidly — already suppressed while charging internally.
    const bool blinkLedOff =
        (lowBatteryMonitor.isWarning() ||
         lowBatteryMonitor.isInShutdownCountdown()) &&
        !lowBatteryMonitor.blinkOn();
    rgbLed.show(blinkLedOff ? RgbColor{0, 0, 0} : statusColor(ledState));

    // Skipped while the OLED is off: applyPowerTier() redraws on wake.
    if (menuController.currentScreen() == MenuScreen::kInactive &&
        !inLowBatteryCountdown && oledOnFor(currentPowerTier)) {
      // A charging fault takes over the operating screen (but not active
      // menu navigation) for as long as it persists — this checks once a
      // second like the rest of this block rather than every tick, since
      // unlike the bounded 30-second countdown above, a fault can persist
      // for an entire session and redrawing every tick would hog I2C.
      if (chargeState == ChargeState::kLatchedFault) {
        ScreenBuffer faultScreen;
        renderChargingFaultScreen(&faultScreen);
        oledDisplay.render(faultScreen);
      } else {
        showOperatingScreen();
      }
    }

    Serial.print("Battery ");
    Serial.print(batteryPercent);
    Serial.print("% (");
    Serial.print(chargeStateName(chargeState));
    Serial.println(")");
    Serial.print("Trigger ");
    Serial.print(triggerPercent);
    Serial.println("%");
    Serial.print("Stick X ");
    Serial.print(stickXPercent);
    Serial.print(" Y ");
    Serial.println(stickYPercent);
  }

  // Low battery countdown takes over the display unconditionally, as the
  // very last thing that can touch it this tick — it must win over
  // anything the menu or the once-a-second operating-screen refresh
  // above just drew. Redrawn every tick (not just on change) rather than
  // tracked for cheapness; a few extra I2C writes for at most 30 seconds
  // is a non-issue.
  if (inLowBatteryCountdown) {
    if (!wasInLowBatteryCountdown) {
      // Force the OLED on regardless of the current low-power tier. The
      // XBee is already asleep: see setXbeeSleep() above.
      oledDisplay.setPowerOn(true);
    }
    ScreenBuffer countdownScreen;
    renderLowBatteryCountdownScreen(lowBatteryMonitor.secondsRemaining(),
                                    &countdownScreen);
    oledDisplay.render(countdownScreen);
  } else if (wasInLowBatteryCountdown) {
    // Cancelled (charging plugged in, or the reading recovered) —
    // restore whatever the current power tier and menu state say should
    // actually be showing/powered.
    applyPowerTier(previousPowerTier);
  }
  wasInLowBatteryCountdown = inLowBatteryCountdown;

  // `Serial` is true while a USB host is connected (see the serial wait in
  // setup()).
  if (shouldLightSleep(currentPowerTier, static_cast<bool>(Serial),
                       chargeState == ChargeState::kCharging,
                       inLowBatteryCountdown)) {
    lightSleepFor(kLightSleepPeriodMs);
  } else {
    // Idle out the rest of this pass's 5 ms slot (see loop_pacer.h).
    // delay() blocks this task, so the FreeRTOS idle task halts the CPU
    // until the next tick interrupt instead of spinning through loop().
    delay(LoopPacer::idleMsAfterPass(now, millis()));
  }
}
