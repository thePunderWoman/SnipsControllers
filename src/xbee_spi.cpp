#include "xbee_spi.h"

#include <Arduino.h>
#include <SPI.h>

#include "pin_assignment.h"
#include "xbee_frame.h"

namespace {

// Matches thePunderWoman/Amidala's proven-working XBee3 SPI config (same
// module family) — see its xbee_spi.cpp.
const SPISettings kXbeeSpiSettings(3000000, MSBFIRST, SPI_MODE0);

uint8_t xbeeTransfer() { return SPI.transfer(0xFF); }
void xbeeDrain(uint16_t n) {
  while (n--) SPI.transfer(0xFF);
}

}  // namespace

void XbeeSpi::begin() {
  pinMode(PinAssignment::kXbeeSpiCs, OUTPUT);
  digitalWrite(PinAssignment::kXbeeSpiCs, HIGH);
  pinMode(PinAssignment::kXbeeSpiAttn, INPUT);
  SPI.begin(PinAssignment::kXbeeSpiSck, PinAssignment::kXbeeSpiMiso,
            PinAssignment::kXbeeSpiMosi, PinAssignment::kXbeeSpiCs);
}

bool XbeeSpi::frameAvailable() {
  return digitalRead(PinAssignment::kXbeeSpiAttn) == LOW;
}

int32_t XbeeSpi::readFrame(uint8_t *buf, uint16_t maxLen) {
  SPI.beginTransaction(kXbeeSpiSettings);
  digitalWrite(PinAssignment::kXbeeSpiCs, LOW);

  // Skip idle 0xFF bytes (the module pads before the start delimiter).
  uint8_t b = 0xFF;
  for (int i = 0; i < 32 && b != 0x7E; i++) {
    b = xbeeTransfer();
  }
  if (b != 0x7E) {
    digitalWrite(PinAssignment::kXbeeSpiCs, HIGH);
    SPI.endTransaction();
    return -1;
  }

  const uint16_t length =
      (static_cast<uint16_t>(xbeeTransfer()) << 8) | xbeeTransfer();
  if (length == 0 || length > maxLen) {
    xbeeDrain(length + 1);  // drain data + checksum so the stream stays in sync
    digitalWrite(PinAssignment::kXbeeSpiCs, HIGH);
    SPI.endTransaction();
    return 0;
  }

  for (uint16_t i = 0; i < length; i++) {
    buf[i] = xbeeTransfer();
  }
  const uint8_t checksum = xbeeTransfer();

  digitalWrite(PinAssignment::kXbeeSpiCs, HIGH);
  SPI.endTransaction();

  if (!XbeeFrame::checksumValid(buf, length, checksum)) {
    return 0;
  }
  return static_cast<int32_t>(length);
}

void XbeeSpi::writeFrame(const uint8_t *frameData, uint16_t length) {
  const uint8_t checksum = XbeeFrame::computeChecksum(frameData, length);

  SPI.beginTransaction(kXbeeSpiSettings);
  digitalWrite(PinAssignment::kXbeeSpiCs, LOW);

  SPI.transfer(0x7E);
  SPI.transfer(static_cast<uint8_t>(length >> 8));
  SPI.transfer(static_cast<uint8_t>(length & 0xFF));
  for (uint16_t i = 0; i < length; i++) {
    SPI.transfer(frameData[i]);
  }
  SPI.transfer(checksum);

  digitalWrite(PinAssignment::kXbeeSpiCs, HIGH);
  SPI.endTransaction();
}

bool XbeeSpi::sendAtCommand(const char *atCmd, const uint8_t *value,
                            uint8_t valueLength, uint8_t *outValue,
                            uint8_t outValueCapacity,
                            uint8_t *outValueLength,
                            unsigned long timeoutMs) {
  constexpr uint8_t kFrameId = 0x01;
  uint8_t requestFrame[64];
  const uint16_t requestLength = XbeeFrame::buildAtCommandFrame(
      requestFrame, sizeof(requestFrame), kFrameId, atCmd, value,
      valueLength);
  if (requestLength == 0) {
    return false;
  }
  writeFrame(requestFrame, requestLength);

  const unsigned long start = millis();
  uint8_t responseFrame[64];
  while (millis() - start < timeoutMs) {
    if (!frameAvailable()) {
      yield();  // let the ESP32's background tasks/watchdog run
      continue;
    }

    const int32_t length = readFrame(responseFrame, sizeof(responseFrame));
    if (length <= 0) {
      continue;  // no delimiter yet, or a bad frame — keep waiting
    }

    XbeeFrame::AtCommandResponse response;
    if (!XbeeFrame::parseAtCommandResponse(
            responseFrame, static_cast<uint16_t>(length), &response)) {
      continue;  // some other frame type was queued — not our response
    }
    if (response.frameId != kFrameId) {
      continue;
    }
    if (response.status != XbeeFrame::AtCommandStatus::kOk) {
      return false;
    }

    const uint8_t copyLength = response.valueLength < outValueCapacity
                                    ? response.valueLength
                                    : outValueCapacity;
    for (uint8_t i = 0; i < copyLength; i++) {
      outValue[i] = response.value[i];
    }
    if (outValueLength != nullptr) {
      *outValueLength = copyLength;
    }
    return true;
  }
  return false;  // timed out
}
