# Snips Controller — RP2350A GPIO Table

> **MCU:** Bare RP2350A (QFN-60) — not a Pico 2 W module. Custom USB-C +
> external flash/crystal + a separate Murata Type 1YN WiFi/BT module.
> See `MCU_Core` and `Wireless` sections of `snips_controller.yaml` for why.

## Reserved / Do Not Use
| GPIO | Reason |
|---|---|
| GP16 | WL_ON — enables both WL_REG_ON and BT_REG_ON on the Murata Type 1YN wireless module |
| GP23 | WL_D — bit-banged data bus to the wireless module |
| GP24 | WL_CLK — bit-banged clock to the wireless module |
| GP25 | WL_CS — bit-banged chip select to the wireless module |
| GP29 | VSYS_SENSE — divide-by-3 battery voltage sense (ADC3) |

The wireless interface deliberately replicates the official Pico 2 W's own
bit-banged 3-wire RP2350↔CYW43439 protocol (not real 4-bit SDIO — RP2350
has no SDIO host controller), so the existing pico-sdk `cyw43` PIO driver
applies with just a pin remap.

GP17-GP19 are free for future use.

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
| VSYS battery sense | GP29 | ADC3, divide-by-3 via R_VSYS1/R_VSYS2 |

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
| WL_ON (wireless enable) | GP16 | Output, drives WL_REG_ON + BT_REG_ON together |
| WL_D (wireless data) | GP23 | Bit-banged, PIO-driven |
| WL_CLK (wireless clock) | GP24 | Bit-banged, PIO-driven |
| WL_CS (wireless chip select) | GP25 | Bit-banged, PIO-driven |

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

## MCU Support Circuitry (RP2350A)

Bare RP2350A needs external circuitry the Pico 2 W module used to provide
internally. All values replicate the official Pico 2 W reference schematic
(RPI-PICO2W) for the RP2350 portion — see `MCU_Core` in the yaml.

| Function | Components | Notes |
|---|---|---|
| Core regulator (1V1 rail) | L_VREG (3.3µH), C_VREG_1/2 (4.7µF) | External LC filter for RP2350's internal switching regulator (VREG_LX/FB) |
| Crystal | X1 (12MHz), C16/C17 (15pF) | XIN/XOUT |
| QSPI flash | U_FLASH (W25Q32RVXHJQ) | 4MiB, XSON-8 2x3mm |
| BOOTSEL | SW_BOOTSEL, R11 (1kΩ) | Pulls QSPI_SS low to force USB boot |
| USB series resistors | R_USB_DP/R_USB_DM (27Ω) | Between RP2350 and the external USB-C connector |
| ADC_AVDD filter | R_ADC_AVDD1 (200Ω), R_ADC_AVDD2 (1Ω), C_ADC_AVDD1/2 | Two-stage RC filter off 3V3 |
| VSYS battery sense | R_VSYS1 (200kΩ), R_VSYS2 (100kΩ) | Divide-by-3 into GP29/ADC3 |
| IOVDD/DVDD decoupling | C_IOVDD_1/2, C_1V1_1/2, C_QSPI_IOVDD, C_VREG_AVDD | 100nF each |

**IOVDD/DVDD have multiple physically separate pins on the QFN-60 package**
that all share the same pin name — they're addressed individually by pin
number in the yaml (not just by name) so every physical pad gets tied to
the right net.

## Wireless (Murata Type 1YN)

Replaces the Pico 2 W module's onboard CYW43439 + Abracon "Niche" patented
antenna (which requires a separate license from Abracon). Type 1YN is a
pre-certified (FCC/CE) module using the same CYW43439 die, with antenna
matching and RF filtering already done.

| Type 1YN pin | Net | Notes |
|---|---|---|
| SDIO_CLK | WL_CLK (GP24) | Direct |
| SDIO_CMD + SDIO_DATA_0 | WL_D_BUS0 | Shorted together, through R22 (470Ω) to WL_D |
| SDIO_DATA_1 + SDIO_DATA_2 | WL_D_BUS1 | Shorted together, through R23 (10kΩ) to WL_D |
| SDIO_DATA_3 | WL_CS (GP25) | Direct |
| WL_REG_ON + BT_REG_ON | WL_ON (GP16) | Tied together — WiFi+BT enable as one unit |
| VBAT, VIN_LDO | 3V3 | C_WL_VBAT1/2 (4.7µF) decoupling |
| SR_VLX | via L_WL_VBAT (2.2µH) to 3V3 | Internal buck LC filter |
| VIO | 3V3 | C_WL_VIO (2.2µF) decoupling |
| LPO_IN | U_LPO output | Driven 32.768kHz oscillator IC (not a passive crystal — LPO_IN is a clock input) |
| BT_DEV_WAKE | GND | Unused, tied low (matches Pico 2 W reference) |
| GND(SR_PVSS) (pins 32/33) | GND | **Layout note:** must be an isolated ground pour per Murata's datasheet, not merged into the general ground plane |

**Layout note:** the antenna trace geometry must exactly copy Murata's
Hardware Application Note Figure 4 (Trace Antenna Guideline) — this is a
PCB-layout-stage requirement that can't be captured in the schematic yaml.

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
Reusing Amidala's verified `XB3-24Z8UT-J` symbol (`PCB/libraries/Xbee3.kicad_sym`).
THT socket footprint used for prototyping; SMT module footprint TBD for final PCB.

### Power Control Circuit
See power control section of schematic. Uses GP15 (power button sense input) and GP21 (latch hold output).
GP21 must be driven HIGH as the very first instruction in firmware or power will cut on button release.

### Battery Voltage Sense
GP29 reads VSYS/3 via an external R_VSYS1/R_VSYS2 divider (added when the
Pico 2 W module — which had this divider built in — was replaced by a bare
RP2350A). bq25185 STAT1 on GP20 provides charge state (HIGH=idle/done,
LOW=charging or fault).

> **Firmware constraint carried over from the original design:** if WiFi
> traffic and VSYS ADC reads ever contend for timing (both ultimately touch
> GP29-adjacent circuitry historically), gate battery ADC reads to avoid
> overlapping with active WL_D bus transactions. Verify against the actual
> pico-sdk `cyw43` PIO driver behavior once bring-up firmware exists — this
> is inherited caution, not a confirmed conflict on the new wiring.

---

## Spare GPIO
GP17, GP18, GP19 — free for future use.
