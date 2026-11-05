#pragma once

// Raw GPIO pin assignments for the Snips Controller. Values here are
// Arduino GPIO numbers (e.g. 16 means GPIO16), not the module's physical
// pin numbers used in the PCB docs.
//
// Two boards, picked at build time:
//   - V1.x (ESP32-S3-WROOM-1, `pio run -e esp32s3`): a 1:1 mirror of
//     PCB/v1.x/GPIO_table.md. The XBee is on SPI.
//   - V2 (ESP32-S3-MINI-1U, `pio run -e esp32s3_v2`, which defines
//     SNIPS_BOARD_V2): a 1:1 mirror of PCB/v2.x/snips/GPIO_table.md. The
//     XBee is on an optional carrier board, over UART.
// The two maps are not interchangeable: V1 firmware on a V2 board would
// read the wrong buttons. Cross-reference the GPIO tables for the
// physical-pin-to-GPIO mapping, reserved pins, and the rationale behind
// each assignment.

namespace PinAssignment {

#if defined(SNIPS_BOARD_V2)

// ---- UART (XBee carrier, pads 1-6) ----------------------------------------
// The ESP32 transmits on the XBee's DIN and receives on its DOUT. CTS is
// the XBee's output (low = ready); RTS is its input. With no carrier fitted
// these float, so the transport pulls CTS, DOUT and ON/SLEEP up and treats
// CTS high as "no XBee" (see xbee_uart.h).
constexpr int kXbeeUartTx = 35;   // carrier pad 2, XBee DIN
constexpr int kXbeeUartRx = 36;   // carrier pad 3, XBee DOUT
constexpr int kXbeeCts = 34;      // carrier pad 1
constexpr int kXbeeRts = 39;      // carrier pad 4
constexpr int kXbeeOnSleep = 41;  // carrier pad 5
// Carrier pad 6. Drive open drain only: low, or released (input). The
// pull-up is on the carrier.
constexpr int kXbeeReset = 42;
// Carrier pad 7, XBee DTR/SLEEP_RQ. Low = stay awake; set it as one of the
// first instructions in setup(), same as on V1.
constexpr int kXbeeSleepRq = 1;

// ---- I2C (OLED, ICM-42670-P IMU) ------------------------------------------
constexpr int kI2cSda = 12;
constexpr int kI2cScl = 13;

// ---- IMU interrupt (ICM-42670-P INT1) --------------------------------------
// No longer on a strap pin, unlike V1's GPIO46.
constexpr int kAccelInterrupt = 37;

// ---- ADC1 (analog trigger, thumbstick, battery sense) -----------------------
constexpr int kAnalogTrigger = 9;
constexpr int kThumbstickX = 7;
constexpr int kThumbstickY = 6;
constexpr int kVsysSense = 10;  // divide-by-3, same as V1

// ---- Digital buttons -------------------------------------------------------
// Same names and meanings as V1; only the GPIOs moved. V2 has one bumper.
constexpr int kVolUp = 38;
constexpr int kVolDown = 40;
constexpr int kTriggerUp = 33;
constexpr int kTriggerDown = 18;
constexpr int kDigitalTrigger = 47;  // bumper
constexpr int kThumbstickClick = 17;
constexpr int kMacro1 = 16;
constexpr int kMacro2 = 5;
constexpr int kMacro3 = 4;
constexpr int kMacro4 = 11;
constexpr int kMacro5 = 8;
constexpr int kMacro6 = 2;

// ---- Power -------------------------------------------------------------------
// RTC-domain pin, like V1's GPIO2.
constexpr int kPowerButtonSense = 21;
// RXD0. Its pull-up holds the latch through any reset or USB upload.
// Firmware drives it high early in boot and low to power off, and must
// never start UART0 receive on it (Serial is native USB, so it doesn't).
constexpr int kPowerLatchHold = 44;

// ---- Charging (bq25185) ----------------------------------------------------
constexpr int kChargeStat1 = 15;
constexpr int kChargeStat2 = 14;

// ---- RGB status LED (SK6812, RMT-driven) -------------------------------------
// Moved off TXD0, which prints the boot log.
constexpr int kRgbLedData = 48;

#else  // V1.x


// ---- SPI (XBee) --------------------------------------------------------
constexpr int kXbeeSpiSck = 10;
constexpr int kXbeeSpiMosi = 11;
constexpr int kXbeeSpiMiso = 12;
constexpr int kXbeeSpiCs = 13;
constexpr int kXbeeOnSleep = 14;
constexpr int kXbeeSpiAttn = 15;
// XBee DTR/SLEEP_RQ, driven by the host to request pin sleep (drive high =
// request sleep once idle, low = wake immediately). Was hardwired to GND
// (permanently deasserted) on earlier revisions; must be set to a known
// level (low = stay awake) as one of the first instructions in setup(),
// same idiom as kPowerLatchHold, since it's replacing a hard GND tie
// rather than a pull resistor.
constexpr int kXbeeSleepRq = 42;

// ---- I2C (OLED, ICM-42670-P IMU or QMA6100P accelerometer) -------------
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;

// ---- Accelerometer (ICM-42670-P or QMA6100P, shares the I2C bus above) --
// INT1 output, wired so the MCU can light-sleep and wake on the
// accelerometer's hardware motion interrupt instead of polling. Strapping
// pin (GPIO46, boot message control only) — safe here since either chip's
// interrupt output stays inactive (never driven high) through boot until
// firmware configures it.
constexpr int kAccelInterrupt = 46;

// ---- ADC1 (analog trigger, thumbstick, battery sense) -------------------
constexpr int kAnalogTrigger = 4;
constexpr int kThumbstickX = 5;
constexpr int kThumbstickY = 6;
constexpr int kVsysSense = 7;

// ---- Digital buttons -----------------------------------------------------
// "Vol" and "Trigger" up/down are two generic, symmetric button pairs —
// see PCB/v1.x/GPIO_table.md. What each pair does is assigned by Amidala, not
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

#endif  // SNIPS_BOARD_V2

}  // namespace PinAssignment
