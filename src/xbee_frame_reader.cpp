#include "xbee_frame_reader.h"

#include "xbee_frame.h"

XbeeFrameReader::Result XbeeFrameReader::feed(uint8_t byte) {
  switch (state_) {
    case State::kDelimiter:
      if (byte == 0x7E) state_ = State::kLengthHigh;
      return Result::kNeedMore;

    case State::kLengthHigh:
      length_ = static_cast<uint16_t>(byte) << 8;
      state_ = State::kLengthLow;
      return Result::kNeedMore;

    case State::kLengthLow:
      length_ |= byte;
      if (length_ == 0 || length_ > kMaxFrameLength) {
        state_ = State::kDelimiter;
        return Result::kBadFrame;
      }
      received_ = 0;
      state_ = State::kData;
      return Result::kNeedMore;

    case State::kData:
      data_[received_++] = byte;
      if (received_ == length_) state_ = State::kChecksum;
      return Result::kNeedMore;

    case State::kChecksum:
      state_ = State::kDelimiter;
      return XbeeFrame::checksumValid(data_, length_, byte) ? Result::kFrame
                                                            : Result::kBadFrame;
  }
  return Result::kNeedMore;
}
