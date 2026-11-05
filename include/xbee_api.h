#pragma once

#include <cstdint>

#include "xbee_link.h"

// The XBee API operations this firmware uses, over any XbeeLink: local AT
// commands, sending our application payload to the coordinator, and
// polling for payloads from it. Pure: the clock and the idle hook are
// passed in, so native tests can drive it with a fake link.
class XbeeApi {
 public:
  using ClockFn = unsigned long (*)();
  using IdleFn = void (*)();

  XbeeApi(XbeeLink *link, ClockFn clock, IdleFn idle)
      : link_(link), clock_(clock), idle_(idle) {}

  // Sends a local AT command and waits (up to timeoutMs) for the matching
  // (by frame ID) AT Command Response. Returns false on a build failure,
  // a non-OK status, or timeout. outValue/outValueLength may be null.
  bool sendAtCommand(const char *atCmd, const uint8_t *value,
                     uint8_t valueLength, uint8_t *outValue,
                     uint8_t outValueCapacity, uint8_t *outValueLength,
                     unsigned long timeoutMs = 200);

  // Sends our own application payload to the coordinator as a Transmit
  // Request (0x10). Fire-and-forget: the XBee's own ACK/retry handles
  // reliability at the radio level.
  void sendPacket(const uint8_t *payload, uint16_t payloadLength);

  // Reads at most one queued frame, returning the payload of a Receive
  // Packet (0x90). Frames of any other type are discarded. Returns false
  // if nothing usable was read this call.
  bool pollForPacket(uint8_t *outPayload, uint16_t outPayloadCapacity,
                     uint16_t *outPayloadLength);

 private:
  XbeeLink *link_;
  ClockFn clock_;
  IdleFn idle_;
};
