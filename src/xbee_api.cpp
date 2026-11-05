#include "xbee_api.h"

#include "xbee_frame.h"

bool XbeeApi::sendAtCommand(const char *atCmd, const uint8_t *value,
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
  link_->writeFrame(requestFrame, requestLength);

  const unsigned long start = clock_();
  uint8_t responseFrame[64];
  while (clock_() - start < timeoutMs) {
    if (!link_->frameAvailable()) {
      idle_();  // let the ESP32's background tasks/watchdog run
      continue;
    }

    const int32_t length = link_->readFrame(responseFrame, sizeof(responseFrame));
    if (length <= 0) {
      if (length < 0) idle_();
      continue;  // nothing complete yet, or a bad frame — keep waiting
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

void XbeeApi::sendPacket(const uint8_t *payload, uint16_t payloadLength) {
  constexpr uint8_t kFrameId = 0x02;  // distinct from sendAtCommand's 0x01
  uint8_t frame[48];
  const uint16_t length = XbeeFrame::buildTransmitRequestFrame(
      frame, sizeof(frame), kFrameId, payload, payloadLength);
  if (length == 0) {
    return;  // payload too large for the frame buffer — drop it
  }
  link_->writeFrame(frame, length);
}

bool XbeeApi::pollForPacket(uint8_t *outPayload, uint16_t outPayloadCapacity,
                            uint16_t *outPayloadLength) {
  if (!link_->frameAvailable()) {
    return false;
  }

  uint8_t frame[64];
  const int32_t length = link_->readFrame(frame, sizeof(frame));
  if (length <= 0) {
    return false;  // nothing complete yet, or a bad frame was drained
  }

  XbeeFrame::ReceivePacket packet;
  if (!XbeeFrame::parseReceivePacket(frame, static_cast<uint16_t>(length),
                                     &packet)) {
    return false;  // some other frame type — not what we're looking for
  }

  const uint16_t copyLength = packet.payloadLength < outPayloadCapacity
                                  ? packet.payloadLength
                                  : outPayloadCapacity;
  for (uint16_t i = 0; i < copyLength; i++) {
    outPayload[i] = packet.payload[i];
  }
  *outPayloadLength = copyLength;
  return true;
}
