#include <Arduino.h>

#include "pin_assignment.h"
#include "power_latch.h"

// Pure entry point — wiring only. All real logic lives in dedicated
// subsystem files under src/ + include/; this file just owns real
// peripherals and feeds their raw readings into that logic each tick.

namespace {

PowerOffDetector powerOffDetector;

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
}

void loop() {
  const bool powerButtonHeld =
      digitalRead(PinAssignment::kPowerButtonSense) == HIGH;

  if (powerOffDetector.update(powerButtonHeld, millis())) {
    // The real graceful-shutdown sequence (notify Amidala, OLED message,
    // then drive the latch pin low) lands in a later PR once the packet
    // protocol and display exist. For now, just prove the hold is detected.
    Serial.println(
        "Power button held 3s - shutdown sequence would run here (PR 9).");
  }
}
