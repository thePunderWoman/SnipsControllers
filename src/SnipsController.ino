#include <Arduino.h>

#include "battery.h"
#include "buttons.h"
#include "calibration.h"
#include "calibration_store.h"
#include "droid_persistence.h"
#include "menu.h"
#include "oled.h"
#include "packet.h"
#include "pin_assignment.h"
#include "power_latch.h"
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
char deviceSerialLowBuf[9] = {};  // must outlive setup() — see its use below
bool lastReportedPressed[Buttons::kCount] = {};
unsigned long lastTelemetryLogMs = 0;
constexpr unsigned long kTelemetryLogIntervalMs = 1000;
unsigned long lastUplinkSendMs = 0;
constexpr unsigned long kUplinkSendIntervalMs = 50;
MenuScreen previousMenuScreen = MenuScreen::kInactive;
MainMenuItem previousMainMenuItem = MainMenuItem::kSwitchDroid;

void showBootScreen() {
  ScreenBuffer bootScreen;
  bootScreen.setLine(0, "Snips Controller");
  bootScreen.setLine(1, "OLED OK");
  oledDisplay.render(bootScreen);
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
  // uncalibrated full ADC range if none has been saved yet. The guided
  // calibration flows that produce new values live in calibration.h and
  // get wired to the on-device menu in a later PR.
  calibrationData = CalibrationStore::load();

  // Restores any previously-saved droid list; defaults to empty if none
  // has been saved yet.
  menuController.setDroidStore(DroidPersistence::load());

  xbeeControl.begin();
  menuController.setXbeeTransport(&xbeeControl);

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

  // Real "normal operating" screen content (complications) lands in a
  // later PR. For now this just proves the display works end to end, and
  // is what's restored whenever the on-device menu closes.
  if (oledDisplay.begin()) {
    showBootScreen();
  } else {
    Serial.println("OLED not found at boot.");
  }

  // Bring-up check per PCB/README.md's recommended order: cycle through
  // every status color once to prove RMT output on the real LED. Real
  // state (connected/charging/error) gets driven by later PRs once there's
  // an XBee link and charge-status reading to base it on; for now this
  // just settles on "disconnected," which is accurate today.
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
    // The real graceful-shutdown sequence (notify Amidala, OLED message,
    // then drive the latch pin low) lands in PR 10. For now, just prove
    // the hold is detected.
    Serial.println(
        "Power button held 3s - shutdown sequence would run here (PR 10).");
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
  // Amidala never sees them any differently either way, since the packet
  // protocol (PR 8) doesn't exist yet.
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
      showBootScreen();
    }
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
  const int batteryPercent = batteryMonitor.percentFor(rawVsys);
  const ChargeState chargeState =
      batteryMonitor.chargeStateFor(stat1High, stat2High);

  // Uplink: sent periodically over the radio, faster than the
  // human-readable Serial telemetry below — this is what Amidala
  // actually sees.
  if (now - lastUplinkSendMs >= kUplinkSendIntervalMs) {
    lastUplinkSendMs = now;

    UplinkPacket uplink;
    for (size_t i = 0; i < Buttons::kCount; ++i) {
      if (buttonPanel.isPressed(i)) {
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

  // Downlink: non-blocking poll every tick. Nothing consumes handedness
  // or the Left/Right label+value yet — that's the complications system,
  // PR 10 — so for now this just proves the round trip works.
  uint8_t downlinkBuf[Packet::kDownlinkEncodedSize];
  uint16_t downlinkLength = 0;
  if (xbeeControl.pollForPacket(downlinkBuf, sizeof(downlinkBuf),
                                &downlinkLength)) {
    DownlinkPacket downlink;
    if (Packet::decodeDownlink(downlinkBuf, downlinkLength, &downlink)) {
      Serial.print("Downlink: hand=");
      Serial.print(static_cast<int>(downlink.handedness));
      Serial.print(" L=");
      Serial.print(downlink.leftLabel);
      Serial.print(":");
      Serial.print(downlink.leftValue);
      Serial.print(" R=");
      Serial.print(downlink.rightLabel);
      Serial.print(":");
      Serial.println(downlink.rightValue);
    }
  }

  // Human-readable Serial telemetry — not what Amidala sees, just a
  // slower-cadence bring-up check that the values above look right.
  if (now - lastTelemetryLogMs >= kTelemetryLogIntervalMs) {
    lastTelemetryLogMs = now;

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
