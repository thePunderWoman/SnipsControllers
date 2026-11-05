#include "xbee_uart_setup.h"

namespace XbeeUartSetup {

namespace {

bool apiModeIsUnescaped(XbeeUartPort *port) {
  uint8_t ap = 0;
  uint8_t length = 0;
  return port->apiAtCommand("AP", nullptr, 0, &ap, 1, &length) && length == 1 &&
         ap == kApiModeUnescaped;
}

bool apiCommand(XbeeUartPort *port, const char *cmd, const uint8_t *value,
                uint8_t valueLength) {
  return port->apiAtCommand(cmd, value, valueLength, nullptr, 0, nullptr);
}

}  // namespace

Result run(XbeeUartPort *port) {
  port->setBaud(kApiBaud);
  if (apiModeIsUnescaped(port)) return Result::kReady;

  // API mode, but at the factory baud: raise it. BD takes effect on AC.
  port->setBaud(kFactoryBaud);
  if (apiModeIsUnescaped(port)) {
    const uint8_t bd = kBaud115200;
    if (apiCommand(port, "BD", &bd, 1) && apiCommand(port, "WR", nullptr, 0) &&
        apiCommand(port, "AC", nullptr, 0)) {
      port->setBaud(kApiBaud);
      if (apiModeIsUnescaped(port)) return Result::kReconfigured;
    }
    return Result::kNotFound;
  }

  // Transparent mode, the factory state, at either baud.
  const uint32_t bauds[] = {kFactoryBaud, kApiBaud};
  for (uint32_t baud : bauds) {
    port->setBaud(baud);
    if (!port->enterCommandMode()) continue;
    if (port->commandModeLine("ATAP1") && port->commandModeLine("ATBD7") &&
        port->commandModeLine("ATWR") && port->commandModeLine("ATCN")) {
      port->setBaud(kApiBaud);
      if (apiModeIsUnescaped(port)) return Result::kReconfigured;
    }
    return Result::kNotFound;
  }
  return Result::kNotFound;
}

}  // namespace XbeeUartSetup
