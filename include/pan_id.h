#pragma once

#include <cstddef>

// Pure helpers for PAN IDs entered as short hex strings (e.g. "4133").
// The XBee's ID parameter is a full 64-bit value and reports it back as
// 16 zero-padded hex characters, so anything that hands a user-entered
// PAN ID to the radio or compares one against what the radio reports has
// to go through normalize() first.
namespace PanId {

constexpr size_t kHexLength = 16;  // 64-bit PAN ID, hex

// Uppercases and left-pads with zeros to kHexLength characters, writing
// them plus a null terminator into out (needs kHexLength + 1 bytes).
// Returns false, leaving out untouched, for null, empty, over-long, or
// non-hex input.
bool normalize(const char *in, char *out);

// True if both normalize to the same value ("4133" == "0000000000004133").
// False if either is invalid.
bool equivalent(const char *a, const char *b);

}  // namespace PanId
