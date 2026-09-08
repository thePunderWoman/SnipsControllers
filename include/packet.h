#pragma once

#include <cstddef>
#include <cstdint>

#include "battery.h"

// Pure encode/decode for Snips' own application-level payload — what goes
// inside an XBee Transmit Request (0x10) / Receive Packet (0x90), defined
// fresh since Amidala has no existing schema for this controller type
// (see thePunderWoman/Amidala#204). Knows nothing about XBee framing
// itself — see xbee_frame.h for that — or about what any of these values
// mean to the rest of the firmware or to Amidala.
//
// Field widths and the wire format (fixed-width, big-endian for
// multi-byte fields) are this rewrite's own choice, not dictated by any
// existing spec.

// Controller -> Amidala, sent periodically. Raw per-button state (no
// gesture classification — see the rewrite plan's Context section) plus
// calibrated analog and battery/charge status.
struct UplinkPacket {
  static constexpr uint8_t kFlagShuttingDown = 0x01;

  uint16_t buttonMask = 0;  // bit i = Buttons::Index i is pressed
  uint8_t triggerPercent = 0;      // 0-100
  int8_t stickXPercent = 0;        // -100..100
  int8_t stickYPercent = 0;        // -100..100
  uint8_t batteryPercent = 0;      // 0-100
  ChargeState chargeState = ChargeState::kDone;
  // Bitfield, currently just kFlagShuttingDown — set on the final few
  // uplinks before power cuts so Amidala can mark this controller
  // disconnected immediately instead of waiting out a timeout.
  uint8_t flags = 0;
};

// Amidala -> controller. Handedness is sent once at connect and is static
// for the session; the Left/Right slot label+value are generic — Amidala
// assigns their meaning (volume, throttle, or anything else) and echoes
// back whatever the current label/value should read.
struct DownlinkPacket {
  enum class Handedness : uint8_t { kUnknown = 0, kLeft = 1, kRight = 2 };

  static constexpr size_t kFieldLength = 8;

  Handedness handedness = Handedness::kUnknown;
  char leftLabel[kFieldLength + 1] = {};
  char leftValue[kFieldLength + 1] = {};
  char rightLabel[kFieldLength + 1] = {};
  char rightValue[kFieldLength + 1] = {};
};

namespace Packet {

constexpr size_t kUplinkEncodedSize = 8;
constexpr size_t kDownlinkEncodedSize =
    1 + 4 * DownlinkPacket::kFieldLength;  // 33

// Returns the number of bytes written (always kUplinkEncodedSize), or 0
// if outCapacity is too small.
size_t encodeUplink(const UplinkPacket &packet, uint8_t *outBuf,
                    size_t outCapacity);

// Returns false if length is too short to hold a full uplink packet.
bool decodeUplink(const uint8_t *buf, size_t length, UplinkPacket *out);

// Returns the number of bytes written (always kDownlinkEncodedSize), or 0
// if outCapacity is too small.
size_t encodeDownlink(const DownlinkPacket &packet, uint8_t *outBuf,
                     size_t outCapacity);

// Returns false if length is too short to hold a full downlink packet.
bool decodeDownlink(const uint8_t *buf, size_t length, DownlinkPacket *out);

}  // namespace Packet
