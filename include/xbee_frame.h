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

constexpr uint8_t kFrameTypeTransmitRequest = 0x10;
constexpr uint8_t kFrameTypeReceivePacket = 0x90;

// The ZigBee coordinator's network address is always 0x0000; per Digi's
// convention, a 64-bit destination of all-zero paired with this means
// "route by 16-bit address, 64-bit unknown" — i.e. exactly "send to the
// coordinator" without needing to know its actual 64-bit address. Not yet
// validated against real hardware — flagged for this PR's bring-up.
constexpr uint64_t kCoordinatorAddress64 = 0;
constexpr uint16_t kCoordinatorAddress16 = 0x0000;

// Builds a Transmit Request frame's data (type 0x10) into `outFrameData`,
// addressed to dest64/dest16 (default: the coordinator, see above).
// Returns the number of bytes written, or 0 if outCapacity is too small.
uint16_t buildTransmitRequestFrame(uint8_t *outFrameData,
                                   uint16_t outCapacity, uint8_t frameId,
                                   const uint8_t *payload,
                                   uint16_t payloadLength,
                                   uint64_t dest64 = kCoordinatorAddress64,
                                   uint16_t dest16 = kCoordinatorAddress16);

struct ReceivePacket {
  uint64_t sourceAddress64;
  const uint8_t *payload;  // points into the buffer passed to parse()
  uint16_t payloadLength;
};

// Parses a Receive Packet frame's data (type 0x90). `frameData` must stay
// valid as long as `out->payload` is used. Returns false if `frameData`
// isn't a recognized/well-formed Receive Packet.
bool parseReceivePacket(const uint8_t *frameData, uint16_t length,
                        ReceivePacket *out);

}  // namespace XbeeFrame
