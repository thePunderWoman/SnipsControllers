#pragma once

// Raw GPIO pin assignments for the Snips Controller (ESP32-S3-WROOM-1).
// Values here are Arduino GPIO numbers (e.g. 16 means GPIO16), not the
// module's physical pin numbers used in the PCB docs.
//
// This file is pure reference data — it should stay a 1:1 mirror of
// PCB/GPIO_table.md. Cross-reference that file for the physical-pin-to-GPIO
// mapping, reserved pins, and the rationale behind each assignment.

namespace PinAssignment {

// ---- SPI (XBee) --------------------------------------------------------
constexpr int kXbeeSpiSck = 10;
constexpr int kXbeeSpiMosi = 11;
constexpr int kXbeeSpiMiso = 12;
constexpr int kXbeeSpiCs = 13;
constexpr int kXbeeOnSleep = 14;
constexpr int kXbeeSpiAttn = 15;

// ---- I2C (OLED) ---------------------------------------------------------
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;

// ---- ADC1 (analog trigger, thumbstick, battery sense) -------------------
constexpr int kAnalogTrigger = 4;
constexpr int kThumbstickX = 5;
constexpr int kThumbstickY = 6;
constexpr int kVsysSense = 7;

// ---- Digital buttons -----------------------------------------------------
// "Vol" and "Trigger" up/down are two generic, symmetric button pairs —
// see PCB/GPIO_table.md. What each pair does is assigned by Amidala, not
// fixed in firmware; these names just match the PCB's schematic/net names.
constexpr int kVolUp = 40;
constexpr int kVolDown = 39;
constexpr int kTriggerUp = 16;
constexpr int kTriggerDown = 17;
constexpr int kDigitalTrigger = 18;  // bumper
constexpr int kThumbstickClick = 21;
constexpr int kMacro1 = 47;
constexpr int kMacro2 = 48;
constexpr int kMacro3 = 35;
constexpr int kMacro4 = 36;
constexpr int kMacro5 = 37;
constexpr int kMacro6 = 38;

// ---- Power ---------------------------------------------------------------
constexpr int kPowerButtonSense = 2;
constexpr int kPowerLatchHold = 44;

// ---- Charging (bq25185) ---------------------------------------------------
constexpr int kChargeStat1 = 1;
constexpr int kChargeStat2 = 41;

// ---- RGB status LED (SK6812/WS2812, RMT-driven) ---------------------------
constexpr int kRgbLedData = 43;

}  // namespace PinAssignment
