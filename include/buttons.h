#pragma once

#include <cstddef>

#include "pin_assignment.h"

// The full set of digital buttons on the controller and pure (hardware-
// agnostic) per-button debounce logic. No gesture classification lives
// here — Amidala owns single/double/long-press and alt-button semantics
// centrally (see the firmware rewrite plan's Context section); this layer
// only turns a noisy raw pin reading into a stable press/release state.
namespace Buttons {

enum Index : size_t {
  kMacro1 = 0,
  kMacro2,
  kMacro3,
  kMacro4,
  kMacro5,
  kMacro6,
  kBumper,       // "Digital trigger" in PCB/GPIO_table.md
  kStickClick,
  // The two generic, symmetric up/down button pairs — see
  // PCB/GPIO_table.md. What each pair does is assigned by Amidala, not
  // fixed in firmware. "Left"/"Right" here is just a fixed, arbitrary
  // application-level label: Left = the "Vol" pair, Right = the "Trigger"
  // pair, matching PinAssignment's hardware-truth names.
  kLeftUp,
  kLeftDown,
  kRightUp,
  kRightDown,
  kCount
};

constexpr int kPins[kCount] = {
    PinAssignment::kMacro1,       PinAssignment::kMacro2,
    PinAssignment::kMacro3,       PinAssignment::kMacro4,
    PinAssignment::kMacro5,       PinAssignment::kMacro6,
    PinAssignment::kDigitalTrigger,
    PinAssignment::kThumbstickClick,
    PinAssignment::kVolUp,        PinAssignment::kVolDown,
    PinAssignment::kTriggerUp,    PinAssignment::kTriggerDown,
};

}  // namespace Buttons

// Debounces a single button's raw (noisy) pin reading over time. A state
// change is only accepted once the new raw reading has held steady for at
// least `debounceMs` — robust to variable loop timing, unlike a fixed
// sample-count scheme.
class ButtonDebouncer {
 public:
  explicit ButtonDebouncer(unsigned long debounceMs = 20);

  // Call every loop tick with the current raw (undebounced) reading and
  // the current time. Returns the current debounced state.
  bool update(bool rawPressed, unsigned long nowMs);

  bool isPressed() const { return debouncedState_; }

 private:
  unsigned long debounceMs_;
  bool debouncedState_ = false;
  bool lastRaw_ = false;
  unsigned long lastRawChangeMs_ = 0;
};

// Owns one ButtonDebouncer per button in Buttons::Index. Hardware-agnostic:
// the caller (SnipsController.ino) is responsible for the actual
// digitalRead() per pin and for translating the board's active-low wiring
// into a "pressed" boolean before calling update().
class ButtonPanel {
 public:
  // Feed this tick's raw reading for one button.
  void update(size_t index, bool rawPressed, unsigned long nowMs);

  bool isPressed(size_t index) const;

 private:
  ButtonDebouncer debouncers_[Buttons::kCount];
};
