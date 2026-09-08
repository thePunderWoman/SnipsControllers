#include <Arduino.h>

#include "battery.h"
#include "buttons.h"
#include "calibration.h"
#include "calibration_store.h"
#include "oled.h"
#include "pin_assignment.h"
#include "power_latch.h"
#include "rgb_led.h"
#include "screen.h"
#include "status_led.h"

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
bool lastReportedPressed[Buttons::kCount] = {};
unsigned long lastTelemetryLogMs = 0;
constexpr unsigned long kTelemetryLogIntervalMs = 1000;

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
  // debounced raw press/release (packet protocol lands in a later PR, so
  // for now state changes are just logged for bring-up).
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

  // Real screen content (menus, complications, gesture feedback) lands in
  // later PRs. For now this just proves the display works end to end.
  if (oledDisplay.begin()) {
    ScreenBuffer bootScreen;
    bootScreen.setLine(0, "Snips Controller");
    bootScreen.setLine(1, "OLED OK");
    oledDisplay.render(bootScreen);
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
    // then drive the latch pin low) lands in a later PR once the packet
    // protocol and display exist. For now, just prove the hold is detected.
    Serial.println(
        "Power button held 3s - shutdown sequence would run here (PR 9).");
  }

  for (size_t i = 0; i < Buttons::kCount; ++i) {
    const bool rawPressed = digitalRead(Buttons::kPins[i]) == LOW;
    buttonPanel.update(i, rawPressed, now);

    const bool pressed = buttonPanel.isPressed(i);
    if (pressed != lastReportedPressed[i]) {
      lastReportedPressed[i] = pressed;
      Serial.print(buttonName(i));
      Serial.println(pressed ? " pressed" : " released");
    }
  }

  // Packet protocol lands in PR 8 — for now, just log periodically (not
  // every tick) so bring-up can confirm these readings look right.
  if (now - lastTelemetryLogMs >= kTelemetryLogIntervalMs) {
    lastTelemetryLogMs = now;

    const int rawTrigger = analogRead(PinAssignment::kAnalogTrigger);
    const int rawStickX = analogRead(PinAssignment::kThumbstickX);
    const int rawStickY = analogRead(PinAssignment::kThumbstickY);
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
