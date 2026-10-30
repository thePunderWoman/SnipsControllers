#pragma once

#include <cstdint>

// Thin I2C hardware adapter over the board's motion sensor: the TDK
// ICM-42670-P 6-axis IMU (LGA-14, I2C address 0x68, AP_AD0 tied to GND)
// from board V1.2 on, or the QST QMA6100P accelerometer (LGA-12, I2C
// address 0x12, AD0 tied to GND) on V1.1 and earlier — see
// PCB/v1.x/GPIO_table.md. begin() probes for the ICM-42670-P first, then the
// QMA6100P. Either way it uses the chip's own motion interrupt engine
// (ICM-42670-P wake-on-motion, QMA6100P ANY_MOT_INT), mapped to INT1
// (host GPIO46), rather than polling raw acceleration data; the values
// written are in icm42670p_registers.h and qma6100p_registers.h.
// The interrupt is latched: INT1 stays high after motion until
// motionDetected() reads the status register to clear it, so a short
// movement can't be missed between loop passes or during light sleep.
//
// Thresholds are still being tuned on real hardware.
//
// Gracefully absent on boards that don't have either part populated:
// begin() returns false and every other call becomes a safe no-op, so
// callers can treat "no accelerometer" as just one more input source
// that never fires rather than a special case. Excluded from native
// build/coverage (see platformio.ini's [env:native] build_src_filter).
class Accelerometer {
 public:
  // Probes the I2C bus for either chip (a failed/NACKed transaction means
  // nothing is populated at that address) and, if found, configures its
  // motion interrupt engine and starts sampling. Returns false (leaving
  // the bus otherwise untouched) if nothing responds.
  bool begin();

  bool isPresent() const { return chip_ != Chip::kNone; }

  // True if any motion has latched since the last call; clears the latch
  // (an I2C read, only when INT1 is high). Always false if begin() found
  // nothing (or was never called).
  bool motionDetected();

 private:
  enum class Chip { kNone, kIcm42670p, kQma6100p };
  Chip chip_ = Chip::kNone;
};
