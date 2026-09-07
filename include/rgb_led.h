#pragma once

#include <Adafruit_NeoPixel.h>

#include "pin_assignment.h"
#include "status_led.h"

// Thin hardware adapter over Adafruit_NeoPixel — drives the single
// SK6812 status LED (RMT-driven). No state->color decisions happen here;
// that's StatusLedController's job (status_led.h), which stays
// hardware-agnostic and unit tested. This file is excluded from native
// build/coverage (see platformio.ini's [env:native] build_src_filter).
class RgbLed {
 public:
  void begin();
  void show(const RgbColor &color);

 private:
  Adafruit_NeoPixel pixel_{1, PinAssignment::kRgbLedData, NEO_GRB + NEO_KHZ800};
};
