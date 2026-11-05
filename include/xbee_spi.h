#pragma once

#include <cstdint>

#include "xbee_link.h"

// Thin hardware adapter for the XBee3's SPI interface (AP=1/unescaped API
// mode), used on V1 boards. Frame envelope/checksum logic lives in
// xbee_frame.h and the API operations in xbee_api.h (both pure, tested);
// this file only does real SPI transfers, CS toggling, and ATTN polling —
// mirrors thePunderWoman/Amidala's proven xbee_spi.cpp read path (same
// module family, same SPI settings), extended with a write path. Excluded
// from native build/coverage (see platformio.ini's [env:native]
// build_src_filter).
class XbeeSpi : public XbeeLink {
 public:
  void begin();

  // True if the module has at least one frame queued (ATTN asserted).
  bool frameAvailable() override;

  // See XbeeLink. -1 means no start delimiter was found at all (ATTN may be
  // stuck low with nothing actually queued).
  int32_t readFrame(uint8_t *buf, uint16_t maxLen) override;

  void writeFrame(const uint8_t *frameData, uint16_t length) override;
};
