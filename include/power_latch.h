#pragma once

// Pure logic for the power-button hold-to-shutdown gesture. See
// PCB/GPIO_table.md and PCB/README.md's "Power Architecture" section: the
// soft-latch circuit powers the 3.3V rail on any press of the power button,
// and firmware must drive the latch-hold pin HIGH immediately on boot or
// the rail collapses when the button is released. This class only handles
// the other half — detecting a 3-second hold while running, so firmware
// knows when to run its graceful shutdown sequence (added in a later PR)
// and finally let the latch go.
//
// Hardware-agnostic: callers feed in the already-debounced button state and
// a monotonic timestamp (e.g. millis()); nothing here touches real pins.
class PowerOffDetector {
 public:
  explicit PowerOffDetector(unsigned long holdThresholdMs = 3000);

  // Call once per loop tick with the current raw power-button state (true =
  // currently held) and the current time. Returns true on the single tick
  // where the continuous hold first crosses the threshold.
  bool update(bool buttonPressed, unsigned long nowMs);

 private:
  unsigned long holdThresholdMs_;
  bool wasPressed_ = false;
  unsigned long pressStartMs_ = 0;
  bool firedForThisHold_ = false;
};
