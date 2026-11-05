#include "xbee_uart.h"

#include <Arduino.h>
#include <driver/gpio.h>

#include <cstring>

#include "pin_assignment.h"
#include "xbee_frame.h"

#if defined(SNIPS_BOARD_V2)

namespace {

// Command mode needs this much silence before and after "+++" (the
// default GT is 1 s).
constexpr unsigned long kGuardTimeMs = 1100;
constexpr unsigned long kCommandReplyMs = 1500;

unsigned long clockMs() { return millis(); }
void idle() { yield(); }

}  // namespace

void XbeeUart::begin() {
  // RTS is the XBee's input; it ignores it at the default D6=0, but keep it
  // at "clear to send" rather than floating. RESET is open drain with the
  // pull-up on the carrier, so leave it released.
  pinMode(PinAssignment::kXbeeRts, OUTPUT);
  digitalWrite(PinAssignment::kXbeeRts, LOW);
  pinMode(PinAssignment::kXbeeReset, INPUT);
  pinMode(PinAssignment::kXbeeCts, INPUT_PULLUP);
  pinMode(PinAssignment::kXbeeOnSleep, INPUT_PULLUP);

  // Give a module that's still booting time to assert CTS.
  delay(20);
  if (digitalRead(PinAssignment::kXbeeCts) == HIGH) {
    Serial.println("XBee carrier not found (CTS high).");
    return;
  }

  Serial1.begin(XbeeUartSetup::kApiBaud, SERIAL_8N1, PinAssignment::kXbeeUartRx,
                PinAssignment::kXbeeUartTx);
  gpio_pullup_en(static_cast<gpio_num_t>(PinAssignment::kXbeeUartRx));
  setUp_ = true;
  switch (XbeeUartSetup::run(this)) {
    case XbeeUartSetup::Result::kReady:
      present_ = true;
      break;
    case XbeeUartSetup::Result::kReconfigured:
      Serial.println("XBee set to API mode at 115200 baud.");
      present_ = true;
      break;
    case XbeeUartSetup::Result::kNotFound:
      Serial.println("XBee carrier fitted but not answering.");
      break;
  }
  setUp_ = false;
}

bool XbeeUart::frameAvailable() {
  return (present_ || setUp_) && Serial1.available() > 0;
}

int32_t XbeeUart::readFrame(uint8_t *buf, uint16_t maxLen) {
  while (Serial1.available() > 0) {
    switch (reader_.feed(static_cast<uint8_t>(Serial1.read()))) {
      case XbeeFrameReader::Result::kNeedMore:
        break;
      case XbeeFrameReader::Result::kBadFrame:
        return 0;
      case XbeeFrameReader::Result::kFrame:
        if (reader_.length() > maxLen) return 0;
        std::memcpy(buf, reader_.frame(), reader_.length());
        return reader_.length();
    }
  }
  return -1;
}

void XbeeUart::writeFrame(const uint8_t *frameData, uint16_t length) {
  if (!present_ && !setUp_) return;
  const size_t total = static_cast<size_t>(length) + 4;
  if (static_cast<size_t>(Serial1.availableForWrite()) < total) return;
  const uint8_t header[3] = {0x7E, static_cast<uint8_t>(length >> 8),
                             static_cast<uint8_t>(length & 0xFF)};
  Serial1.write(header, sizeof(header));
  Serial1.write(frameData, length);
  Serial1.write(XbeeFrame::computeChecksum(frameData, length));
}

void XbeeUart::setBaud(uint32_t baud) {
  Serial1.flush();
  Serial1.updateBaudRate(baud);
  delay(5);
  while (Serial1.available() > 0) Serial1.read();
  reader_.reset();
}

bool XbeeUart::apiAtCommand(const char *cmd, const uint8_t *value,
                            uint8_t valueLength, uint8_t *outValue,
                            uint8_t outValueCapacity, uint8_t *outValueLength) {
  XbeeApi api(this, clockMs, idle);
  return api.sendAtCommand(cmd, value, valueLength, outValue, outValueCapacity,
                           outValueLength, 300);
}

bool XbeeUart::enterCommandMode() {
  delay(kGuardTimeMs);
  while (Serial1.available() > 0) Serial1.read();
  Serial1.write(reinterpret_cast<const uint8_t *>("+++"), 3);
  char reply[8];
  return readLine(reply, sizeof(reply), kGuardTimeMs + kCommandReplyMs) &&
         std::strcmp(reply, "OK") == 0;
}

bool XbeeUart::commandModeLine(const char *line) {
  Serial1.write(reinterpret_cast<const uint8_t *>(line), std::strlen(line));
  Serial1.write('\r');
  char reply[8];
  return readLine(reply, sizeof(reply), kCommandReplyMs) &&
         std::strcmp(reply, "OK") == 0;
}

// Reads up to a '\r' (command mode's line ending) into `out`.
bool XbeeUart::readLine(char *out, size_t capacity, unsigned long timeoutMs) {
  size_t length = 0;
  const unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (Serial1.available() <= 0) {
      yield();
      continue;
    }
    const char c = static_cast<char>(Serial1.read());
    if (c == '\r') {
      out[length] = '\0';
      return true;
    }
    if (length + 1 < capacity) out[length++] = c;
  }
  return false;
}

#endif  // SNIPS_BOARD_V2
