#pragma once

// Pure logic for battery percentage and bq25185 charge-status decoding.
// Knows nothing about real ADC/GPIO reads — SnipsController.ino does the
// actual analogRead()/digitalRead() and passes raw values in.
enum class ChargeState {
  kDone,               // charge complete, sleep mode, or charging disabled
  kCharging,           // normal charging in progress (incl. auto-recharge)
  kRecoverableFault,   // VIN_OVP, TS hot/cold, or system short
  kLatchedFault,       // ILIM/ISET short, BATOCP, or safety timer expired
};

// Whether the controller is on USB, as far as the board can tell. VBUS
// reaches only the charger's IN pin (no GPIO senses it), so USB shows up
// only indirectly: a computer's USB host is seen by the ESP32-S3's
// native USB, and the charger reporting kCharging means VBUS is present.
// A power-only supply (wall charger, power bank) with the charger
// reporting done is indistinguishable from running on battery: STAT
// reads the same for "charge complete" and "no input power".
enum class UsbPower {
  kNotDetected,  // On battery, or USB power the board can't see.
  kPowerOnly,    // Charging, so VBUS is present, but no USB host.
  kHost,         // A USB host (computer) is connected.
};

UsbPower usbPowerFor(bool usbHostConnected, ChargeState chargeState);

// Short lowercase description for Serial logging, e.g. "host connected".
const char *usbPowerName(UsbPower usbPower);

class BatteryMonitor {
 public:
  // Truth table is bq25185 datasheet (SLUSF65A) Table 7-2 "Status Pins
  // State Table" — stat1High/stat2High are the STAT1/STAT2 pin states as
  // read directly (both are open-drain with an external pull-up to 3V3,
  // so HIGH/LOW here is the real electrical state, no polarity inversion).
  ChargeState chargeStateFor(bool stat1High, bool stat2High) const;

  // Raw ADC (0-4095, ESP32 12-bit) -> battery percentage (0-100), via the
  // VSYS divide-by-3 network (R_VSYS1/R_VSYS2, see PCB/GPIO_table.md) and a
  // linear approximation between empty/full cell voltage. Assumes the ADC
  // is configured for its default ~3.3V full-scale range — confirm against
  // real hardware, and note a linear curve is a simplification of a real
  // Li-ion discharge curve, good enough for a v1 estimate.
  int percentFor(int rawAdc) const;

 private:
  static constexpr int kAdcMaxCounts = 4095;
  static constexpr float kAdcFullScaleVolts = 3.3f;
  static constexpr float kVsysDividerRatio = 3.0f;
  static constexpr float kEmptyVoltage = 3.0f;
  static constexpr float kFullVoltage = 4.2f;
};
