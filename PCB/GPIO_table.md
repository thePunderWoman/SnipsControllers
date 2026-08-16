# Snips Controller — Pico 2 W GPIO Table

> **MCU:** Raspberry Pi Pico 2 W (RP2350)
> Soldered via castellated pads directly to custom PCB.

## Reserved / Do Not Use
| GPIO | Reason |
|---|---|
| GP16 | CYW43439 WiFi SPI (internal) |
| GP17 | CYW43439 WiFi SPI (internal) |
| GP18 | CYW43439 WiFi SPI (internal) |
| GP19 | CYW43439 WiFi SPI (internal) |
| GP23 | SMPS power control (internal) |
| GP24 | VBUS sense (internal) |
| GP25 | Onboard LED via CYW43439 (internal) |
| GP29 | VSYS sense — used by firmware for battery voltage reading, not user GPIO |

---

## SPI1 (XBee)
| Function | GPIO |
|---|---|
| XBee SCK | GP10 |
| XBee MOSI | GP11 |
| XBee MISO | GP12 |
| XBee CS | GP13 |

## I2C0 (OLED)
| Function | GPIO |
|---|---|
| SDA | GP4 |
| SCL | GP5 |

## ADC
| Function | GPIO | Notes |
|---|---|---|
| Analog trigger (DRV5055) | GP26 | ADC0 |
| Thumbstick X (GuliKit) | GP27 | ADC1 |
| Thumbstick Y (GuliKit) | GP28 | ADC2 |

## Digital Inputs / Outputs
| Function | GPIO | Notes |
|---|---|---|
| Vol up | GP0 | Internal pull-up, active low |
| Vol down | GP1 | Internal pull-up, active low |
| Digital trigger (bumper) | GP2 | Internal pull-up, active low |
| Thumbstick click (KEY) | GP3 | Internal pull-up, active low |
| Macro button matrix Row 0 | GP6 | Drive low to scan |
| Macro button matrix Row 1 | GP7 | Drive low to scan |
| Macro button matrix Col 0 | GP8 | Input, internal pull-up |
| Macro button matrix Col 1 | GP9 | Input, internal pull-up |
| Macro button matrix Col 2 | GP14 | Input, internal pull-up |
| Power button sense | GP15 | Input, monitors for 3-second hold |
| Charge status STAT1 (bq25185) | GP20 | Open-drain input, 10kΩ pull-up to 3V3 |
| Power latch hold | GP21 | Output, driven HIGH on boot to hold soft latch |
| RGB LED (WS2812 NeoPixel) | GP22 | PIO-driven, data line |

## Macro Button Matrix Layout
6 macro buttons in a 2x3 matrix (2 rows × 3 columns).
Each button has a 1N4148 diode (anode to switch, cathode to column) to prevent ghosting.

```
         Col 0 (GP8)   Col 1 (GP9)   Col 2 (GP14)
Row 0 (GP6)  BTN_MACRO1    BTN_MACRO2    BTN_MACRO3
Row 1 (GP7)  BTN_MACRO4    BTN_MACRO5    BTN_MACRO6
```

Scan: drive one row LOW at a time, read columns. Active low with internal pull-ups on columns.

---

## Passives & Support Components

### Decoupling Caps
| Component | Value | Notes |
|---|---|---|
| DRV5055 VCC | 100nF ceramic | Close to supply pin |
| GuliKit VCC | 100nF ceramic | Close to supply pin |
| WS2812 VCC | 100nF ceramic | Close to supply pin |

### Pull-up Resistors
| Signal | Value | Notes |
|---|---|---|
| bq25185 STAT1 pin | 10kΩ | Open-drain, pull up to 3V3 |
| I2C SDA/SCL | 4.7kΩ | **DNP for prototyping** — Adafruit #938 has onboard pull-ups. Populate only on final PCB with bare OLED panel |

### Series Resistors
| Signal | Value | Notes |
|---|---|---|
| SK6812 DIN (GP22) | 300–500Ω | Signal integrity protection, standard NeoPixel best practice |

### Button Matrix Diodes
| Component | Value | Notes |
|---|---|---|
| D_MACRO1–D_MACRO6 | 1N4148 | One per macro button, anode to switch, cathode to column |

### Buttons (non-matrix)
No external components needed — RP2350 internal pull-ups enabled in firmware, wire to GND.

### XBee
Refer to AmidalaShield V1.2 design for XBee3 support circuitry (decoupling, RESET pull-up, RC filter).
THT socket variant used for prototyping; SMT module footprint TBD for final PCB.

### Power Control Circuit
See power control section of schematic. Uses GP15 (power button sense input) and GP21 (latch hold output).
GP21 must be driven HIGH as the very first instruction in firmware or power will cut on button release.

### Battery Voltage Sense
GP29 onboard VSYS divider used in firmware (reads VSYS/3). No external components needed.
bq25185 STAT1 on GP20 provides charge state (HIGH=idle/done, LOW=charging or fault).

> **Firmware constraint:** CYW43439 WiFi SPI shares the GP29 ADC path via the SPI CLK line. Battery voltage reads on GP29 ADC are unreliable while a WiFi SPI transaction is in progress. When WiFi is in use (e.g. syncing data with Amidala, future accessories), firmware must gate battery ADC reads to occur only when WiFi SPI is idle.

---

## Spare GPIO
None — all 22 available user GPIO are assigned.