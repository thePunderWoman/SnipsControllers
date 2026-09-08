#pragma once

#include <cstddef>
#include <cstdint>

// Pure logic for XBee API frame framing (AP=1, unescaped) — checksums and
// the two frame types this firmware needs: local AT Command (0x08) request
// / response (0x88). Deliberately free of any Arduino/SPI dependency, like
// thePunderWoman/Amidala's xbee_frame_checksum.h, so it's unit-testable
// natively. See xbee_spi.h for the SPI-driven transport that uses this.
//
// All functions here operate on a frame's data — the frame-type byte
// through the end, NOT including the leading 0x7E/length header or the
// trailing checksum byte.
namespace XbeeFrame {

// 0xFF minus the low 8 bits of the sum of all bytes in the frame.
uint8_t computeChecksum(const uint8_t *frameData, uint16_t length);

// True if `checksum` matches what computeChecksum() gives for this frame.
bool checksumValid(const uint8_t *frameData, uint16_t length,
                    uint8_t checksum);

constexpr uint8_t kFrameTypeAtCommand = 0x08;
constexpr uint8_t kFrameTypeAtCommandResponse = 0x88;

// Builds a local AT Command frame's data (type 0x08) into `outFrameData`.
// `atCmd` is the two-character command name (e.g. "ID", not
// null-terminated — only the first 2 bytes are read). `value` may be
// null/zero-length for a query. Returns the number of bytes written, or 0
// if outCapacity is too small.
uint16_t buildAtCommandFrame(uint8_t *outFrameData, uint16_t outCapacity,
                             uint8_t frameId, const char *atCmd,
                             const uint8_t *value, uint8_t valueLength);

enum class AtCommandStatus : uint8_t {
  kOk = 0,
  kError = 1,
  kInvalidCommand = 2,
  kInvalidParameter = 3,
};

struct AtCommandResponse {
  uint8_t frameId;
  char atCmd[2];
  AtCommandStatus status;
  const uint8_t *value;  // points into the buffer passed to parse(); may be
                         // null if valueLength == 0
  uint8_t valueLength;
};

// Parses an AT Command Response frame's data (type 0x88). `frameData` must
// stay valid as long as `out->value` is used. Returns false if `frameData`
// isn't a recognized/well-formed AT Command Response.
bool parseAtCommandResponse(const uint8_t *frameData, uint16_t length,
                            AtCommandResponse *out);

}  // namespace XbeeFrame
