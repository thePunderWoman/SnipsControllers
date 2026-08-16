# Snips Controller PCB

A wireless handheld controller for R2-D2 droid operation, designed for use at public events and conventions. Communicates with the droid via XBee3 Zigbee radio in a packetized state-report architecture.

## Hardware Overview

| Component | Part | Notes |
|---|---|---|
| MCU | Raspberry Pi Pico 2 W | RP2350, castellated pads soldered to PCB |
| Display | SSD1306 1.3" OLED | 128x64, monochrome, I2C |
| Radio | XBee3 | Zigbee, SPI1 mode, THT socket (proto) |
| Hall trigger | DRV5055A2 | Ratiometric linear hall effect, 3.3V |
| Thumbstick | GuliKit hall effect module | Native 3.3V, analog X/Y + click |
| Buttons | Omron B3F series | Tactile momentary clicky |
| RGB LED | SK6812 NeoPixel-compatible | PIO-driven, 3.3V native, single LED status indicator |
| Charger | bq25185 | USB/DC input, power path, 4.2V/500mA |
| Buck | TLV62569 | 3.3V output, 1A |
| Battery | 18650 or 14500 Li-ion | Swappable single cell, TBD empirically |
| Power switch | DMG2305UX + 2N7002 | Soft latch, hybrid hardware+firmware |

## GPIO Assignment

### Reserved — Do Not Use
| GPIO | Reason |
|---|---|
| GP16–GP19 | CYW43439 WiFi SPI (internal) |
| GP23 | SMPS power control (internal) |
| GP24 | VBUS sense (internal) |
| GP25 | Onboard LED via CYW43439 (internal) |
| GP29 | VSYS sense — firmware battery voltage read only |

### SPI1 (XBee)
| Function | GPIO |
|---|---|
| XBee SCK | GP10 |
| XBee MOSI | GP11 |
| XBee MISO | GP12 |
| XBee CS | GP13 |

### I2C0 (OLED)
| Function | GPIO |
|---|---|
| SDA | GP4 |
| SCL | GP5 |

### ADC
| Function | GPIO |
|---|---|
| Analog trigger (DRV5055) | GP26 |
| Thumbstick X (GuliKit) | GP27 |
| Thumbstick Y (GuliKit) | GP28 |

### Digital Inputs / Outputs
| Function | GPIO | Notes |
|---|---|---|
| Vol up | GP0 | Active low, internal pull-up |
| Vol down | GP1 | Active low, internal pull-up |
| Digital trigger (bumper) | GP2 | Active low, internal pull-up |
| Thumbstick click (KEY) | GP3 | Active low, internal pull-up |
| Matrix Row 0 | GP6 | Drive low to scan |
| Matrix Row 1 | GP7 | Drive low to scan |
| Matrix Col 0 | GP8 | Input, internal pull-up |
| Matrix Col 1 | GP9 | Input, internal pull-up |
| Matrix Col 2 | GP14 | Input, internal pull-up |
| Power button sense | GP15 | Input, 3-second hold detect |
| Charge status STAT1 | GP20 | Open-drain, 10kΩ pull-up to 3V3 |
| Power latch hold | GP21 | Output, HIGH on boot to hold latch |
| RGB LED DIN (WS2812) | GP22 | PIO-driven |

### Macro Button Matrix Layout
6 macro buttons in a 2x3 matrix. Each button requires a 1N4148 diode (anode to switch, cathode to column) to prevent ghosting.

```
           Col0 (GP8)   Col1 (GP9)   Col2 (GP14)
Row0 (GP6)  MACRO1       MACRO2       MACRO3
Row1 (GP7)  MACRO4       MACRO5       MACRO6
```

### Spare GPIO
None — all 22 available user GPIO are assigned.

---

## Passives & Support Components

### Decoupling Caps
| Component | Value | Notes |
|---|---|---|
| DRV5055 VCC | 100nF ceramic | Close to supply pin |
| GuliKit VCC | 100nF ceramic | Close to supply pin |
| WS2812B VCC | 100nF ceramic | Close to supply pin |
| bq25185 VIN | 10µF + 100nF ceramic | X7R/X5R, 25V rated |
| bq25185 SYS | 10µF + 100nF ceramic | X7R/X5R, 25V rated |
| bq25185 BAT | 10µF ceramic | |
| TLV62569 VIN | 10µF ceramic | |
| TLV62569 VOUT | 22µF ceramic | |

### Pull-up Resistors
| Signal | Value | Notes |
|---|---|---|
| bq25185 STAT1 | 10kΩ | Open-drain, pull up to 3V3 |
| bq25185 TS/MR | 10kΩ to GND | REQUIRED if no thermistor — floating = no charging |
| XBee RESET | 10kΩ to 3V3 | Plus 100Ω + 100nF RC filter at pin |
| I2C SDA/SCL | 4.7kΩ | **DNP for prototyping** — Adafruit #938 has onboard pull-ups. Populate only on final PCB with bare OLED panel |

### Series Resistors
| Signal | Value | Notes |
|---|---|---|
| WS2812B DIN (GP22) | 330Ω | Signal integrity, place close to GP22 pad |

### Button Matrix Diodes
| Component | Value | Notes |
|---|---|---|
| D_MACRO1–D_MACRO6 | 1N4148 SOD-123 | Anode to switch, cathode to column — mandatory for ghosting prevention |

### Direct Buttons
No external components needed. Wire to GND; enable RP2350 internal pull-ups in firmware.

### XBee
Refer to AmidalaShield V1.2 for XBee3 support circuitry. THT socket for prototyping; update footprint to SMT module for final PCB if size requires.

---

## Communication Architecture

Controllers communicate with the droid via XBee3 in **packetized state-report** mode. The Pico 2 W firmware reads all inputs locally (button states, ADC values for analog trigger and thumbstick) and assembles them into a structured payload transmitted periodically over SPI1 to the XBee3 radio. The receiving AmidalaShield deserializes the packet and interprets controller state.

This approach is preferred over per-signal transmission as it is more efficient, easier to deserialize, and robust to timing issues.

### Network Configuration
- Each droid has its own coordinator with a unique PAN ID
- All coordinators share the same AES encryption key
- Controllers are configured as routers
- "Switch droid" functionality is implemented via XBee API commands to leave the current network and rejoin a different PAN ID on demand
- Active droid name and connection status are shown on the OLED display

---

## Bringup Sequence

For breadboard bringup, GP0 (TX) and GP1 (RX) can be temporarily used for XBee UART communication with a serial XBee prototyping board, freeing SPI1 pins. Vol up and Vol down are remapped to spare GPIO during this phase.

Recommended bringup order:
1. Pico 2 W alone — verify USB enumeration, flash blink sketch, confirm GP21 latch hold works
2. OLED on I2C — verify I2C on GP4/GP5, get something on screen
3. WS2812 RGB LED — verify PIO on GP22, cycle colors
4. Buttons — verify GP0/1/2/3 direct inputs and 2x3 matrix scan on GP6-9/14
5. Charge status — verify STAT1 on GP20 reads correctly
6. DRV5055 + GuliKit thumbstick — verify ADC readings on GP26/27/28
7. XBee over UART — verify basic communication with serial prototyping board before switching to SPI1

---

## Power Architecture

- Battery → bq25185 (charger + power path, 4.2V VBATREG, 500mA) → TLV62569 (3.3V buck) → Pico 2 W VSYS and all 3.3V logic
- Power switch: soft latch (DMG2305UX PFET + 2N7002 NFET) controls TLV62569 EN pin
- Boot: press power button → hardware latch enables 3V3 → Pico boots → GP21 driven HIGH to hold latch
- Shutdown: firmware detects 3-second hold on GP15 → graceful shutdown → GP21 LOW → power cut
- bq25185 STAT1 → GP20 (open-drain, 10kΩ pull-up to 3V3)
- Battery voltage sense → GP29 onboard VSYS divider (reads VSYS/3 via ADC)
- All logic is 3.3V native — Pico 2 W, DRV5055, GuliKit thumbstick, SK6812

> **Firmware constraint:** CYW43439 WiFi SPI shares the GP29 ADC path via the SPI CLK line. Battery voltage reads on GP29 ADC are unreliable while a WiFi SPI transaction is in progress. When WiFi is in use (e.g. syncing data with Amidala, future accessories), firmware must gate battery ADC reads to occur only when WiFi SPI is idle.

## Power Management

Firmware implements tiered power saving:
- **Active**: full brightness OLED, normal polling rate
- **Idle (short timeout)**: OLED dimmed via SSD1306 contrast command
- **Idle (long timeout)**: OLED off
- **Sleep**: MCU sleep, wake on any button press via GPIO interrupt