#pragma once

#include <cstdint>

// Assembles XBee API frames (AP=1, unescaped) from a byte stream, one byte
// at a time, for the UART link (xbee_uart.h), where bytes arrive whenever
// they arrive rather than a frame per SPI read. Pure. Bytes before a 0x7E
// delimiter are skipped; a 0x7E inside a frame is just data, since the
// length says where the frame ends.
class XbeeFrameReader {
 public:
  static constexpr uint16_t kMaxFrameLength = 128;

  enum class Result {
    kNeedMore,  // no complete frame yet
    kFrame,     // frame() / length() hold a frame with a valid checksum
    kBadFrame,  // a frame ended with a bad checksum, or its length was
                // impossible; dropped, and the reader is looking for the
                // next delimiter
  };

  Result feed(uint8_t byte);

  // Only meaningful right after feed() returns kFrame.
  const uint8_t *frame() const { return data_; }
  uint16_t length() const { return length_; }

  // Drops any partial frame, e.g. after changing the baud rate.
  void reset() { state_ = State::kDelimiter; }

 private:
  enum class State { kDelimiter, kLengthHigh, kLengthLow, kData, kChecksum };
  State state_ = State::kDelimiter;
  uint8_t data_[kMaxFrameLength] = {};
  uint16_t length_ = 0;
  uint16_t received_ = 0;
};
