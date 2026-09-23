#pragma once

// Injected by scripts/firmware_version.py as a `git describe` string for
// the esp32s3 build; the native test build never defines it, so it falls
// back to a fixed placeholder there.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

namespace Firmware {
constexpr const char *kVersion = FIRMWARE_VERSION;
}  // namespace Firmware
