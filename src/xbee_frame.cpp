#include "xbee_frame.h"

uint8_t XbeeFrame::computeChecksum(const uint8_t *frameData, uint16_t length) {
  uint8_t sum = 0;
  for (uint16_t i = 0; i < length; i++) {
    sum = static_cast<uint8_t>(sum + frameData[i]);
  }
  return static_cast<uint8_t>(0xFF - sum);
}

bool XbeeFrame::checksumValid(const uint8_t *frameData, uint16_t length,
                              uint8_t checksum) {
  return computeChecksum(frameData, length) == checksum;
}

uint16_t XbeeFrame::buildAtCommandFrame(uint8_t *outFrameData,
                                        uint16_t outCapacity,
                                        uint8_t frameId, const char *atCmd,
                                        const uint8_t *value,
                                        uint8_t valueLength) {
  const uint16_t total = 4 + valueLength;
  if (total > outCapacity) {
    return 0;
  }
  outFrameData[0] = kFrameTypeAtCommand;
  outFrameData[1] = frameId;
  outFrameData[2] = static_cast<uint8_t>(atCmd[0]);
  outFrameData[3] = static_cast<uint8_t>(atCmd[1]);
  for (uint8_t i = 0; i < valueLength; i++) {
    outFrameData[4 + i] = value[i];
  }
  return total;
}

bool XbeeFrame::parseAtCommandResponse(const uint8_t *frameData,
                                       uint16_t length,
                                       AtCommandResponse *out) {
  constexpr uint16_t kMinLength = 5;  // type + frameId + cmd(2) + status
  if (length < kMinLength || frameData[0] != kFrameTypeAtCommandResponse) {
    return false;
  }
  out->frameId = frameData[1];
  out->atCmd[0] = static_cast<char>(frameData[2]);
  out->atCmd[1] = static_cast<char>(frameData[3]);
  out->status = static_cast<AtCommandStatus>(frameData[4]);
  out->valueLength = static_cast<uint8_t>(length - kMinLength);
  out->value = out->valueLength > 0 ? frameData + kMinLength : nullptr;
  return true;
}

uint16_t XbeeFrame::buildTransmitRequestFrame(uint8_t *outFrameData,
                                              uint16_t outCapacity,
                                              uint8_t frameId,
                                              const uint8_t *payload,
                                              uint16_t payloadLength,
                                              uint64_t dest64,
                                              uint16_t dest16) {
  constexpr uint16_t kHeaderLength = 14;
  const uint16_t total = kHeaderLength + payloadLength;
  if (total > outCapacity) {
    return 0;
  }
  outFrameData[0] = kFrameTypeTransmitRequest;
  outFrameData[1] = frameId;
  for (int i = 0; i < 8; i++) {
    outFrameData[2 + i] =
        static_cast<uint8_t>(dest64 >> (8 * (7 - i)));
  }
  outFrameData[10] = static_cast<uint8_t>(dest16 >> 8);
  outFrameData[11] = static_cast<uint8_t>(dest16 & 0xFF);
  outFrameData[12] = 0;  // broadcast radius: 0 = max hops
  outFrameData[13] = 0;  // options: default
  for (uint16_t i = 0; i < payloadLength; i++) {
    outFrameData[kHeaderLength + i] = payload[i];
  }
  return total;
}

bool XbeeFrame::parseReceivePacket(const uint8_t *frameData, uint16_t length,
                                   ReceivePacket *out) {
  constexpr uint16_t kHeaderLength = 12;  // type + src64(8) + src16(2) + options
  if (length < kHeaderLength || frameData[0] != kFrameTypeReceivePacket) {
    return false;
  }
  uint64_t source64 = 0;
  for (int i = 0; i < 8; i++) {
    source64 = (source64 << 8) | frameData[1 + i];
  }
  out->sourceAddress64 = source64;
  out->payloadLength = static_cast<uint16_t>(length - kHeaderLength);
  out->payload = out->payloadLength > 0 ? frameData + kHeaderLength : nullptr;
  return true;
}
