#include "xbee_spi.h"

// V1 boards only; V2 talks to its XBee over UART (xbee_uart.h).
#if !defined(SNIPS_BOARD_V2)

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

#endif  // !SNIPS_BOARD_V2
