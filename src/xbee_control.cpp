#include "xbee_control.h"

namespace {

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

// Parses exactly byteCount*2 hex characters from hex into outBytes.
// Returns false (leaving outBytes untouched) on any non-hex character or
// a string shorter than expected.
bool hexStringToBytes(const char *hex, uint8_t *outBytes, size_t byteCount) {
  for (size_t i = 0; i < byteCount; i++) {
    const int hi = hexNibble(hex[i * 2]);
    const int lo = hi >= 0 ? hexNibble(hex[i * 2 + 1]) : -1;
    if (hi < 0 || lo < 0) return false;
    outBytes[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

void bytesToHexString(const uint8_t *bytes, size_t byteCount, char *outHex) {
  constexpr char kDigits[] = "0123456789ABCDEF";
  for (size_t i = 0; i < byteCount; i++) {
    outHex[i * 2] = kDigits[bytes[i] >> 4];
    outHex[i * 2 + 1] = kDigits[bytes[i] & 0x0F];
  }
  outHex[byteCount * 2] = '\0';
}

}  // namespace

bool XbeeControl::leaveNetwork() {
  // "NR0": local network reset — forces the module to leave its current
  // network and search again per its (about-to-change) ID setting.
  uint8_t value[] = {0};
  return spi_.sendAtCommand("NR", value, sizeof(value), nullptr, 0, nullptr);
}

bool XbeeControl::setPanId(const char *panId) {
  uint8_t panIdBytes[8];
  if (!hexStringToBytes(panId, panIdBytes, sizeof(panIdBytes))) {
    return false;
  }
  if (!spi_.sendAtCommand("ID", panIdBytes, sizeof(panIdBytes), nullptr, 0,
                          nullptr)) {
    return false;
  }
  // "WR": commit to the module's own flash so it independently remembers
  // this PAN ID across a power cycle too — a backstop alongside Snips'
  // own persisted current-selection (see droid_persistence.h), which is
  // what actually reapplies it at boot regardless of what the module's
  // flash holds.
  return spi_.sendAtCommand("WR", nullptr, 0, nullptr, 0, nullptr);
}

bool XbeeControl::rejoinNetwork() {
  // "AC": apply pending parameter changes now, triggering the module to
  // act on the new ID and (re)associate rather than waiting for its next
  // natural re-read of that setting.
  return spi_.sendAtCommand("AC", nullptr, 0, nullptr, 0, nullptr);
}

bool XbeeControl::querySerialLow(char *outHex, size_t outHexCapacity) {
  if (outHexCapacity < 9) {
    return false;  // 8 hex chars + null
  }
  uint8_t value[4];
  uint8_t valueLength = 0;
  if (!spi_.sendAtCommand("SL", nullptr, 0, value, sizeof(value),
                          &valueLength) ||
      valueLength != sizeof(value)) {
    return false;
  }
  bytesToHexString(value, sizeof(value), outHex);
  return true;
}

bool XbeeControl::queryLocalRssiDbm(int *outDbm) {
  uint8_t value[1];
  uint8_t valueLength = 0;
  if (!spi_.sendAtCommand("DB", nullptr, 0, value, sizeof(value),
                          &valueLength) ||
      valueLength != sizeof(value)) {
    return false;
  }
  *outDbm = -static_cast<int>(value[0]);
  return true;
}
