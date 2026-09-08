#pragma once

#include <cstddef>

#include "droid_switcher.h"
#include "xbee_spi.h"

// Real XbeeTransport (see droid_switcher.h), over the SPI transport in
// xbee_spi.h. Exact AT command sequencing (NR for leave, ID for the new
// PAN, AC to apply and trigger rejoin) is this class's best-effort
// reading of Digi's XBee3 manual, not yet validated against real
// hardware — confirm during this PR's bring-up and adjust here if the
// sequence needs correcting.
class XbeeControl : public XbeeTransport {
 public:
  void begin() { spi_.begin(); }

  bool leaveNetwork() override;
  bool setPanId(const char *panId) override;
  bool rejoinNetwork() override;

  // Queries the module's own 64-bit address (low 32 bits, "SL") for
  // display in the Device Info menu screen — this is what a user reads
  // off-screen to enter into Amidala. Writes up to 8 hex chars + a null
  // terminator into outHex (needs a 9-byte buffer). Returns false on
  // query failure, leaving outHex untouched.
  bool querySerialLow(char *outHex, size_t outHexCapacity);

  // Passthroughs to the owned XbeeSpi — see xbee_spi.h. XbeeControl is
  // kept as the single facade over the one physical XBee connection
  // rather than SnipsController.ino owning a second XbeeSpi instance.
  void sendPacket(const uint8_t *payload, uint16_t payloadLength) {
    spi_.sendPacket(payload, payloadLength);
  }
  bool pollForPacket(uint8_t *outPayload, uint16_t outPayloadCapacity,
                     uint16_t *outPayloadLength) {
    return spi_.pollForPacket(outPayload, outPayloadCapacity,
                              outPayloadLength);
  }

 private:
  XbeeSpi spi_;
};
