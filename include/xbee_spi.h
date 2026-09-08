#pragma once

#include <cstdint>

// Thin hardware adapter for the XBee3's SPI interface (AP=1/unescaped API
// mode). Frame envelope/checksum logic lives in xbee_frame.h (pure,
// tested); this file only does real SPI transfers, CS toggling, and ATTN
// polling — mirrors thePunderWoman/Amidala's proven xbee_spi.cpp read
// path (same module family, same SPI settings), extended with a write
// path and a blocking AT-command round trip. Excluded from native
// build/coverage (see platformio.ini's [env:native] build_src_filter).
class XbeeSpi {
 public:
  void begin();

  // True if the module has at least one frame queued (ATTN asserted).
  bool frameAvailable();

  // Reads one queued frame's data (excludes the 0x7E/length header and
  // trailing checksum) into buf. Returns the length on success, -1 if no
  // start delimiter was found at all (ATTN may be stuck low with nothing
  // actually queued — caller should stop draining), or 0 if a delimiter
  // was found but the frame was unusable (bad length or checksum — the
  // SPI stream is still in sync, so the caller should keep going).
  int32_t readFrame(uint8_t *buf, uint16_t maxLen);

  // Writes one complete frame (0x7E + length + frameData + checksum).
  void writeFrame(const uint8_t *frameData, uint16_t length);

  // Sends a local AT command and blocks (up to timeoutMs) reading frames
  // until the matching (by frameId) AT Command Response arrives. Returns
  // false on send failure, an unmatched/malformed response, or timeout.
  bool sendAtCommand(const char *atCmd, const uint8_t *value,
                     uint8_t valueLength, uint8_t *outValue,
                     uint8_t outValueCapacity, uint8_t *outValueLength,
                     unsigned long timeoutMs = 200);
};
