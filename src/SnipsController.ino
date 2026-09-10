#include <Arduino.h>
#include <cstring>

#include "accelerometer.h"
#include "battery.h"
#include "buttons.h"
#include "calibration.h"
#include "calibration_store.h"
#include "complication_persistence.h"
#include "complications.h"
#include "droid_persistence.h"
#include "menu.h"
#include "oled.h"
#include "packet.h"
#include "pin_assignment.h"
#include "power_config_store.h"
#include "power_latch.h"
#include "power_management.h"
#include "rgb_led.h"
#include "screen.h"
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

// Applies one power tier's real hardware effects (see power_management.h)
// — called only on transition, not every tick. Driving kXbeeSleepRq high
// on boards where it isn't wired to anything (the first production run —
// see PCB/GPIO_table.md's Accelerometer section) is a harmless no-op, so
// this needs no capability check of its own.
void applyPowerTier(PowerTier tier) {
  const bool oledShouldBeOn =
      tier != PowerTier::kOff && tier != PowerTier::kXbeeAsleep;
  oledDisplay.setPowerOn(oledShouldBeOn);
  oledDisplay.setDimmed(tier == PowerTier::kDim);
  digitalWrite(PinAssignment::kXbeeSleepRq,
              tier == PowerTier::kXbeeAsleep ? HIGH : LOW);

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

const char *buttonName(size_t index) {
  switch (index) {
    case Buttons::kMacro1: return "Macro1";
    case Buttons::kMacro2: return "Macro2";
    case Buttons::kMacro3: return "Macro3";
    case Buttons::kMacro4: return "Macro4";
    case Buttons::kMacro5: return "Macro5";
    case Buttons::kMacro6: return "Macro6";
    case Buttons::kBumper: return "Bumper";
    case Buttons::kStickClick: return "StickClick";
    case Buttons::kLeftUp: return "LeftUp";
    case Buttons::kLeftDown: return "LeftDown";
    case Buttons::kRightUp: return "RightUp";
    case Buttons::kRightDown: return "RightDown";
    default: return "Unknown";
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

  Serial.begin(115200);

  // Polarity of the power-button sense pin isn't documented anywhere in the
  // PCB docs (it's part of the soft-latch circuit, not a simple
  // switch-to-GND button like the others) — assumed active-high (HIGH while
  // held), no internal pull needed. Confirm against real hardware during
  // this PR's bring-up milestone and flip here if wrong.
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

  xbeeControl.begin();
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
    for (size_t i = 0; i < droidStore.count(); ++i) {
      if (std::strcmp(droidStore.at(i).panId, currentPanId) == 0) {
        menuController.setCurrentDroidName(droidStore.at(i).name);
        break;
      }
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

  // Brief boot confirmation — loop() takes over with the real complications
  // screen once real sensor data starts flowing.
  if (oledDisplay.begin()) {
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
    rgbLed.show(statusLedController.colorFor(state));
    delay(300);
  }
  rgbLed.show(statusLedController.colorFor(SystemState::kDisconnected));
}

void loop() {
  const unsigned long now = millis();

  const bool powerButtonHeld =
      digitalRead(PinAssignment::kPowerButtonSense) == HIGH;

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

  bool justPressed[Buttons::kCount] = {};
  for (size_t i = 0; i < Buttons::kCount; ++i) {
    const bool rawPressed = digitalRead(Buttons::kPins[i]) == LOW;
    buttonPanel.update(i, rawPressed, now);

    const bool pressed = buttonPanel.isPressed(i);
    justPressed[i] = pressed && !lastReportedPressed[i];
    if (pressed != lastReportedPressed[i]) {
      lastReportedPressed[i] = pressed;
      Serial.print(buttonName(i));
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
  menuController.tick(rawStickX, rawStickY);
  if (justPressed[Buttons::kLeftUp]) menuController.onUp();
  if (justPressed[Buttons::kLeftDown]) menuController.onDown();
  if (justPressed[Buttons::kStickClick]) {
    menuController.onEnter(rawTrigger, rawStickX, rawStickY);
  }
  if (justPressed[Buttons::kBumper]) menuController.onBack();

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
  const bool menuStateChanged =
      menuController.currentScreen() != previousMenuScreen ||
      menuController.selectedMainMenuItem() != previousMainMenuItem;
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

  const int rawVsys = analogRead(PinAssignment::kVsysSense);
  const bool stat1High = digitalRead(PinAssignment::kChargeStat1) == HIGH;
  const bool stat2High = digitalRead(PinAssignment::kChargeStat2) == HIGH;

  const int triggerPercent =
      AnalogCalibration::calibrateTrigger(rawTrigger, calibrationData);
  const int stickXPercent = AnalogCalibration::calibrateStickAxis(
      rawStickX, calibrationData.stickXMin, calibrationData.stickXCenter,
      calibrationData.stickXMax);
  const int stickYPercent = AnalogCalibration::calibrateStickAxis(
      rawStickY, calibrationData.stickYMin, calibrationData.stickYCenter,
      calibrationData.stickYMax);

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

  // Uplink: sent periodically over the radio, faster than the
  // human-readable Serial telemetry below — this is what Amidala
  // actually sees.
  if (now - lastUplinkSendMs >= kUplinkSendIntervalMs) {
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
    if (xbeeControl.queryLocalRssiDbm(&rssiDbm)) {
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
    rgbLed.show(statusLedController.colorFor(ledState));

    if (menuController.currentScreen() == MenuScreen::kInactive) {
      showOperatingScreen();
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
}
