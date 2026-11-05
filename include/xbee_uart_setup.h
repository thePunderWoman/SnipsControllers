#pragma once

#include <cstdint>

// Gets the V2 carrier's XBee talking API frames (AP=1) at kApiBaud over
// UART, whatever state it arrives in. Pure: the hardware adapter
// (xbee_uart.h) implements XbeeUartPort, and native tests fake it.
//
// A factory XBee 3 is in transparent mode (AP=0) at 9600 baud. Firmware
// that already set it up finds it in API mode at 115200. In between, a
// module might be in API mode at 9600 (e.g. set up over XCTU). Each is
// tried in that order, and anything not already right is fixed with WR so
// the module keeps the setting across power cycles.
class XbeeUartPort {
 public:
  virtual ~XbeeUartPort() = default;

  virtual void setBaud(uint32_t baud) = 0;

  // A local AT command as an API frame (see XbeeApi::sendAtCommand()).
  // outValue may be null.
  virtual bool apiAtCommand(const char *cmd, const uint8_t *value,
                            uint8_t valueLength, uint8_t *outValue,
                            uint8_t outValueCapacity,
                            uint8_t *outValueLength) = 0;

  // Transparent-mode command mode: guard time, "+++", then "OK\r".
  virtual bool enterCommandMode() = 0;

  // Sends one command-mode line (e.g. "ATAP1") and expects "OK\r".
  virtual bool commandModeLine(const char *line) = 0;
};

namespace XbeeUartSetup {

constexpr uint32_t kApiBaud = 115200;
constexpr uint32_t kFactoryBaud = 9600;
constexpr uint8_t kApiModeUnescaped = 1;  // AP=1
constexpr uint8_t kBaud115200 = 7;        // BD=7

enum class Result {
  kReady,         // already in API mode at kApiBaud
  kReconfigured,  // changed to API mode at kApiBaud and saved
  kNotFound,      // no XBee answered in any mode
};

Result run(XbeeUartPort *port);

}  // namespace XbeeUartSetup
