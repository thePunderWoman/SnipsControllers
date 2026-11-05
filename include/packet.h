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

// Amidala -> controller, sent after every uplink. Carries what the
// status screen (status_screen.h) shows about the droid. A percentage of
// kUnknown means Amidala has nothing to report for it, so the screen
// shows "--" (or, for droid battery, leaves it off).
//
// Wire format (23 bytes): version, handedness, volume, throttle, flags,
// droid battery, sequence id, sequence progress, then the sequence name
// as a fixed 15-byte, zero-padded field. The version byte lets the
// controller ignore a downlink in some other format instead of showing
// garbage; kVersion is deliberately not 0-2, so the previous format
// (which started with a handedness byte) never matches it.
struct DownlinkPacket {
  enum class Handedness : uint8_t { kUnknown = 0, kLeft = 1, kRight = 2 };

  static constexpr uint8_t kVersion = 0x10;
  static constexpr uint8_t kUnknown = 0xFF;

  static constexpr uint8_t kFlagEstop = 0x01;
  static constexpr uint8_t kFlagMotorsDisabled = 0x02;
  static constexpr uint8_t kFlagMuse = 0x04;
  static constexpr uint8_t kFlagAutoDome = 0x08;

  static constexpr size_t kSequenceNameLength = 15;

  Handedness handedness = Handedness::kUnknown;
  uint8_t volumePercent = kUnknown;
  uint8_t throttlePercent = kUnknown;  // the speed cap, not live speed
  uint8_t flags = 0;
  uint8_t droidBatteryPercent = kUnknown;
  // Amidala increments this each time a sequence starts (wrapping at
  // 255), so the same sequence started twice still reads as new. See
  // sequence_banner.h.
  uint8_t sequenceId = 0;
  uint8_t sequenceProgressPercent = kUnknown;
  char sequenceName[kSequenceNameLength + 1] = {};  // empty = none
};

namespace Packet {

constexpr size_t kUplinkEncodedSize = 8;
constexpr size_t kDownlinkEncodedSize =
    8 + DownlinkPacket::kSequenceNameLength;  // 23

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

// Returns false if length is too short to hold a full downlink packet, or
// its version byte isn't DownlinkPacket::kVersion.
bool decodeDownlink(const uint8_t *buf, size_t length, DownlinkPacket *out);

}  // namespace Packet
