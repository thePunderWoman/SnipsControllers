#include "rgb_led.h"

void RgbLed::begin() {
  pixel_.begin();
  pixel_.show();  // off until told otherwise
}

void RgbLed::show(const RgbColor &color) {
  pixel_.setPixelColor(0, pixel_.Color(color.r, color.g, color.b));
  pixel_.show();
}
