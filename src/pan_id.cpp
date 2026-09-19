#include "pan_id.h"

#include <cstring>

namespace PanId {

bool normalize(const char *in, char *out) {
  if (in == nullptr) return false;
  const size_t length = std::strlen(in);
  if (length == 0 || length > kHexLength) return false;

  char padded[kHexLength + 1];
  const size_t padding = kHexLength - length;
  std::memset(padded, '0', padding);
  for (size_t i = 0; i < length; ++i) {
    const char c = in[i];
    if (c >= '0' && c <= '9') {
      padded[padding + i] = c;
    } else if (c >= 'A' && c <= 'F') {
      padded[padding + i] = c;
    } else if (c >= 'a' && c <= 'f') {
      padded[padding + i] = static_cast<char>(c - 'a' + 'A');
    } else {
      return false;
    }
  }
  padded[kHexLength] = '\0';
  std::memcpy(out, padded, kHexLength + 1);
  return true;
}

bool equivalent(const char *a, const char *b) {
  char normalizedA[kHexLength + 1];
  char normalizedB[kHexLength + 1];
  return normalize(a, normalizedA) && normalize(b, normalizedB) &&
         std::strcmp(normalizedA, normalizedB) == 0;
}

}  // namespace PanId
