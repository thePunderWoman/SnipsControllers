#include <Arduino.h>

#include "ButtonState.h"
#include "Buttons.h"
#include "PinConfig.h"

namespace {

// Prototyping only — the real PCB puts the XBee on SPI0 instead.
constexpr unsigned long kXbeeUartBaud = 9600;
constexpr unsigned long kPollIntervalMs = 20;

ButtonState buttonState;
unsigned long lastPollMs = 0;

}  // namespace

void setup() {
  Serial.begin(115200);

  // ESP32-S3 UART pins aren't fixed to a default, so the RX/TX GPIO the
  // XBee is wired to for prototyping must be passed explicitly.
  Serial1.begin(kXbeeUartBaud, SERIAL_8N1, PinConfig::kXbeeUartRx,
                PinConfig::kXbeeUartTx);

  for (size_t i = 0; i < Buttons::kCount; ++i) {
    pinMode(Buttons::kPins[i], INPUT_PULLUP);
  }
}

void loop() {
  const unsigned long now = millis();
  if (now - lastPollMs < kPollIntervalMs) {
    return;
  }
  lastPollMs = now;

  for (size_t i = 0; i < Buttons::kCount; ++i) {
    // Buttons wire to GND with the internal pull-up enabled, so LOW = pressed.
    buttonState.setPressed(i, digitalRead(Buttons::kPins[i]) == LOW);
  }
}
