# Snips Controller PCB

A wireless handheld controller for R2-D2 droid operation, designed for use at public events and conventions. Communicates with the droid via XBee3 Zigbee radio in a packetized state-report architecture.

## Hardware Overview

| Component | Part | Notes |
|---|---|---|
| MCU | Bare RP2350A (QFN-60) | Not a Pico module — external USB-C, flash, crystal, and wireless module. See `GPIO_table.md` for the full support circuitry |
| Flash | Winbond W25Q32RVXHJQ | 4MiB QSPI, XSON-8 2x3mm |
| Wireless | Murata Type 1YN (LBEE5KL1YN) | WiFi/BT module (same CYW43439 die as Pico 2 W's onboard radio), bit-banged 3-wire interface |
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

> See `GPIO_table.md` for the full pin-by-pin table, including bare-RP2350A
> support circuitry and Murata wireless module wiring — this section is a
> summary.

### Reserved — Do Not Use
| GPIO | Reason |
|---|---|
| GP16 | WL_ON — enables WL_REG_ON + BT_REG_ON on the Murata wireless module |
| GP17 | XBee ON_SLEEP status input |
| GP18 | XBee SPI_ATTN — data-ready interrupt |
| GP23 | WL_D — bit-banged data bus to the wireless module |
| GP24 | WL_CLK — bit-banged clock to the wireless module |
| GP25 | WL_CS — bit-banged chip select to the wireless module |
| GP29 | VSYS_SENSE — divide-by-3 battery voltage sense (ADC3) |

The wireless interface deliberately replicates the official Pico 2 W's own
bit-banged 3-wire RP2350↔CYW43439 protocol (not real 4-bit SDIO — RP2350 has
no SDIO host controller), so the existing pico-sdk `cyw43` PIO driver applies
with just a pin remap.

### SPI1 (XBee)
| Function | GPIO |
|---|---|
| XBee SCK | GP10 |
| XBee MOSI | GP11 |
| XBee MISO | GP12 |
| XBee CS | GP13 |
| XBee ON_SLEEP | GP17 |
| XBee SPI_ATTN | GP18 |

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
| WL_ON (wireless enable) | GP16 | Output, drives WL_REG_ON + BT_REG_ON together |
| WL_D (wireless data) | GP23 | Bit-banged, PIO-driven |
| WL_CLK (wireless clock) | GP24 | Bit-banged, PIO-driven |
| WL_CS (wireless chip select) | GP25 | Bit-banged, PIO-driven |

### Macro Button Matrix Layout
6 macro buttons in a 2x3 matrix. Each button requires a 1N4148 diode (anode to switch, cathode to column) to prevent ghosting.

```
           Col0 (GP8)   Col1 (GP9)   Col2 (GP14)
Row0 (GP6)  MACRO1       MACRO2       MACRO3
Row1 (GP7)  MACRO4       MACRO5       MACRO6
```

### Spare GPIO
GP19 — free for future use.

---

## MCU Support Circuitry (RP2350A)

Bare RP2350A needs external circuitry the Pico 2 W module used to provide
internally: core regulator LC filter, crystal, QSPI flash, BOOTSEL, USB
series resistors, ADC_AVDD filter, and the VSYS battery-sense divider. All
values replicate the official Pico 2 W reference schematic (RPI-PICO2W) for
the RP2350 portion. See `GPIO_table.md` for the full component list.

## Wireless (Murata Type 1YN)

Replaces the Pico 2 W module's onboard CYW43439 + Abracon "Niche" patented
antenna (which requires a separate license from Abracon). Type 1YN is a
pre-certified (FCC/CE) module using the same CYW43439 die, with antenna
matching and RF filtering already done. See `GPIO_table.md` for the full
pin-by-pin wiring.

> **Layout note:** the antenna trace geometry must exactly copy Murata's
> Hardware Application Note Figure 4 (Trace Antenna Guideline).

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

Beyond the 4-wire SPI bus (SCK/MOSI/MISO/CS) and RESET, three more pins are
wired per Amidala's actual netlist: DTR tied to GND, ON_SLEEP and SPI_ATTN
to GP17/GP18. SPI_ATTN in particular is the XBee's data-ready interrupt —
needed for reliable SPI transfers, not just a nice-to-have.

---

## Communication Architecture

Controllers communicate with the droid via XBee3 in **packetized state-report** mode. The RP2350A firmware reads all inputs locally (button states, ADC values for analog trigger and thumbstick) and assembles them into a structured payload transmitted periodically over SPI1 to the XBee3 radio. The receiving AmidalaShield deserializes the packet and interprets controller state.

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
1. RP2350A + external flash alone — verify USB enumeration, flash blink sketch, confirm GP21 latch hold works
2. OLED on I2C — verify I2C on GP4/GP5, get something on screen
3. WS2812 RGB LED — verify PIO on GP22, cycle colors
4. Buttons — verify GP0/1/2/3 direct inputs and 2x3 matrix scan on GP6-9/14
5. Charge status — verify STAT1 on GP20 reads correctly
6. DRV5055 + GuliKit thumbstick — verify ADC readings on GP26/27/28
7. XBee over UART — verify basic communication with serial prototyping board before switching to SPI1

---

## Power Architecture

- Battery → bq25185 (charger + power path, 4.2V VBATREG, 500mA) → TLV62569 (3.3V buck) → RP2350A VSYS and all 3.3V logic
- Power switch: soft latch (DMG2305UX PFET + 2N7002 NFET) controls TLV62569 EN pin
- Boot: press power button → hardware latch enables 3V3 → RP2350A boots → GP21 driven HIGH to hold latch
- Shutdown: firmware detects 3-second hold on GP15 → graceful shutdown → GP21 LOW → power cut
- bq25185 STAT1 → GP20 (open-drain, 10kΩ pull-up to 3V3)
- Battery voltage sense → GP29 via external R_VSYS1/R_VSYS2 divider (reads VSYS/3 via ADC) — added when the Pico 2 W module's built-in divider was replaced by a bare RP2350A
- All logic is 3.3V native — RP2350A, DRV5055, GuliKit thumbstick, SK6812, Murata wireless module

> **Firmware constraint:** if WiFi traffic and VSYS ADC reads ever contend for timing, gate battery ADC reads to avoid overlapping with active WL_D bus transactions. This is inherited caution from the Pico 2 W reference design, not a confirmed conflict on the new wiring — verify against actual pico-sdk `cyw43` PIO driver behavior once bring-up firmware exists.

## Power Management

Firmware implements tiered power saving:
- **Active**: full brightness OLED, normal polling rate
- **Idle (short timeout)**: OLED dimmed via SSD1306 contrast command
- **Idle (long timeout)**: OLED off
- **Sleep**: MCU sleep, wake on any button press via GPIO interrupt