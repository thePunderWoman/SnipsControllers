#pragma once

#include <cstddef>
#include <cstdint>

#include "xbee_api.h"
#include "xbee_frame_reader.h"
#include "xbee_link.h"
#include "xbee_uart_setup.h"

// Thin hardware adapter for the V2 board's optional XBee carrier, over
// UART (Serial1) in API mode (AP=1) at 115200 baud. The setup sequence
// (xbee_uart_setup.h), frame assembly (xbee_frame_reader.h) and API
// operations (xbee_api.h) are pure and tested; this file only moves bytes
// and pins. Excluded from native build/coverage (see platformio.ini's
// [env:native] build_src_filter).
//
// No carrier is a supported setup (the ESP32's own radio can be used
// instead, later), so begin() checks for one first: a fitted XBee drives
// CTS low (its default D7=1), while with no carrier the pin floats and its
// pull-up reads high. With no XBee every call is a no-op and AT commands
// fail, which the rest of the firmware already treats as "no XBee".
//
// Hardware flow control is off: traffic is a ~31-byte frame each way every
// 50 ms, far below what 115200 baud and the XBee's buffers handle, and
// enabling it would let a missing or sleeping module block every write.
class XbeeUart : public XbeeLink, public XbeeUartPort {
 public:
  void begin();
  bool present() const { return present_; }

  bool frameAvailable() override;
  int32_t readFrame(uint8_t *buf, uint16_t maxLen) override;
  void writeFrame(const uint8_t *frameData, uint16_t length) override;

  void setBaud(uint32_t baud) override;
  bool apiAtCommand(const char *cmd, const uint8_t *value, uint8_t valueLength,
                    uint8_t *outValue, uint8_t outValueCapacity,
                    uint8_t *outValueLength) override;
  bool enterCommandMode() override;
  bool commandModeLine(const char *line) override;

 private:
  bool readLine(char *out, size_t capacity, unsigned long timeoutMs);

  bool present_ = false;
  bool setUp_ = false;  // writes allowed during setup, before present_
  XbeeFrameReader reader_;
};
