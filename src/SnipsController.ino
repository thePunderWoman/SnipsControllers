#include <Arduino.h>

#include "buttons.h"
#include "oled.h"
#include "pin_assignment.h"
#include "power_latch.h"
#include "screen.h"

// Pure entry point — wiring only. All real logic lives in dedicated
// subsystem files under src/ + include/; this file just owns real
// peripherals and feeds their raw readings into that logic each tick.

namespace {

PowerOffDetector powerOffDetector;
ButtonPanel buttonPanel;
OledDisplay oledDisplay;
bool lastReportedPressed[Buttons::kCount] = {};

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
}
