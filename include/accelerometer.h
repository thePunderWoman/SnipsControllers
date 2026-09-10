#pragma once

#include <cstdint>

// Thin I2C hardware adapter over the QMA6100P accelerometer (QST, LGA-12,
// I2C address 0x12 — AD0 tied to GND per PCB/GPIO_table.md). Configures
// the chip's own ANY_MOT_INT hardware interrupt engine, mapped to INT1
// (host GPIO46), rather than polling raw acceleration data — see QST
// "QMA6100P Preliminary Datasheet" (Doc #13-52-20, Rev A1) sections
// 7.3/7.9 and the register map (9.1/9.2) for the bits used in the .cpp.
// Non-latched interrupt mode is used deliberately: INT1 then directly
// tracks "is the any-motion condition true right now," so motionDetected()
// is a plain digitalRead with no register read-and-clear needed.
//
// Register-level values (in particular ANY_MOT_TH's sensitivity) are a
// best-effort reading of a preliminary datasheet, not yet validated
// against real hardware — same caveat as XbeeControl's AT command
// sequencing. Confirm/tune during this part's hardware bring-up (the
// first production boards don't have this part populated at all — see
// PCB/GPIO_table.md's Accelerometer section — so that bring-up happens
// on a later board revision).
//
// Gracefully absent on boards that don't have this part populated:
// begin() returns false and every other call becomes a safe no-op, so
// callers can treat "no accelerometer" as just one more input source
// that never fires rather than a special case. Excluded from native
// build/coverage (see platformio.ini's [env:native] build_src_filter).
class Accelerometer {
 public:
  // Probes the I2C bus for the chip (a failed/NACKed transaction means
  // nothing is populated at this address) and, if found, configures its
  // any-motion interrupt engine and enters active mode. Returns false
  // (leaving the device otherwise untouched) if nothing responds.
  bool begin();

  bool isPresent() const { return present_; }

  // True if the any-motion interrupt condition is currently active.
  // Always false if begin() found nothing (or was never called).
  bool motionDetected() const;

 private:
  bool present_ = false;
};
