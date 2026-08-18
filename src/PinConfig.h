#pragma once

// GPIO assignments for the Snips Controller firmware (ESP32-S3-WROOM-1).
// Cross-reference: PCB/GPIO_table.md.
//
// Pins below are Arduino GPIO numbers (e.g. 16 means GPIO16), not the
// module's physical pin numbers used in the PCB docs — see GPIO_table.md
// for the physical-pin-to-GPIO mapping.
//
// Prototyping note: the real PCB drives the XBee over SPI (GPIO10-13).
// Until that board exists, the XBee talks over UART instead, on a pair of
// spare GPIO (39/40) that carry no reserved role on this module — unlike
// RP2040, the ESP32-S3 UART peripherals aren't tied to fixed default pins,
// so any free GPIO works and no other signal needs to move to make room.

namespace PinConfig {

// XBee UART (prototyping only — real PCB uses SPI instead, see below)
constexpr int kXbeeUartRx = 39;
constexpr int kXbeeUartTx = 40;

// Digital buttons
constexpr int kDigitalTrigger = 18;
constexpr int kThumbstickClick = 21;
constexpr int kMacro1 = 47;
constexpr int kMacro2 = 48;
constexpr int kMacro3 = 35;
constexpr int kMacro4 = 36;
constexpr int kMacro5 = 37;
constexpr int kMacro6 = 38;
constexpr int kVolUp = 16;
constexpr int kVolDown = 17;

// RGB status LED (SK6812, RMT-driven)
constexpr int kRgbLedData = 43;

// SPI (XBee) pins for the real PCB — unused while the XBee is on UART
constexpr int kXbeeSpiSck = 10;
constexpr int kXbeeSpiMosi = 11;
constexpr int kXbeeSpiMiso = 12;
constexpr int kXbeeSpiCs = 13;

}  // namespace PinConfig
