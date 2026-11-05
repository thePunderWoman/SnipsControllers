#pragma once

#include <cstdint>

// One complete XBee API frame at a time, over whichever wire the board
// uses: SPI on V1 (xbee_spi.h), UART on V2 (xbee_uart.h). Frame data here
// excludes the 0x7E delimiter, length and checksum; the link adds and
// checks those. XbeeApi (xbee_api.h) builds everything else on top.
class XbeeLink {
 public:
  virtual ~XbeeLink() = default;

  // True if at least one frame may be waiting to be read.
  virtual bool frameAvailable() = 0;

  // Reads one frame's data into buf. Returns its length; -1 if no frame is
  // ready after all (the caller should stop draining); or 0 if a frame was
  // found but unusable (bad length or checksum), with the stream still in
  // sync, so the caller should keep going.
  virtual int32_t readFrame(uint8_t *buf, uint16_t maxLen) = 0;

  // Writes one complete frame (delimiter, length, data, checksum).
  virtual void writeFrame(const uint8_t *frameData, uint16_t length) = 0;
};
