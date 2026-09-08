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

  // Queries the module's current PAN ID ("ID"). Once setPanId() commits
  // one via "WR", the module remembers it in its own flash across power
  // cycles on its own — this exists so SnipsController.ino can derive
  // which saved droid (by name) that PAN ID corresponds to at boot,
  // rather than to re-apply anything. Writes up to 16 hex chars + a null
  // terminator into outHex (needs a 17-byte buffer). Returns false on
  // query failure, leaving outHex untouched.
  bool queryPanId(char *outHex, size_t outHexCapacity);

  // Queries the local module's own last-hop received signal strength
  // ("DB" AT command — a single byte, the RSSI magnitude in dBm, e.g. a
  // response of 0x2A means -42dBm). Purely local: no round trip to
  // Amidala needed, unlike everything else this controller displays.
  // Returns false on query failure, leaving outDbm untouched.
  bool queryLocalRssiDbm(int *outDbm);

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
