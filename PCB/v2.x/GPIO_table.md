# Snips Controller — ESP32-S3-WROOM-1 GPIO Table

> The current circuit and footprint choices are in `PCB/v2.x/stackup/board.kdl`
> and `PCB/v2.x/stackup/parts.kdl`. References to the older YAML and KiCad schematic
> below describe the design history; use Stackup for changes.

> **MCU:** ESP32-S3-WROOM-1 module (pre-certified, integrated flash +
> crystal + WiFi/BT radio + PCB antenna). No external RF design, flash,
> or crystal needed — the module handles all of that internally. Custom
> USB-C for firmware flashing, charging, and future accessory expansion
> (the module's native USB is genuinely exposed, unlike a Pico-family
> module whose USB is hardwired to its own onboard connector).
> Pins referenced by GPIO number, not name — the module's pins are
> heavily multi-function (e.g. pin 4 is "GPIO4/TOUCH4/ADC1_CH3") and
> numbers are unambiguous. The wiring source is `PCB/v2.x/stackup/board.kdl`.

## Reserved / Do Not Use
| Pin | Reason |
|---|---|
| 1, 40, 41 | GND |
| 2 | 3V3 |
| 3 | EN — reset/enable, pulled up via R_EN_PU (10kΩ) |
| 13 | USB_D- (native USB) |
| 14 | USB_D+ (native USB) |
| 27 | GPIO0/BOOT — pulled up via R_BOOT_PU (10kΩ), SW_BOOT pulls low to force UF2/download mode |
| 26-32 (internal) | Internal SPI flash — not present on this symbol/module at all |
| 33-34 (internal) | Exist on the bare ESP32-S3 chip but not broken out on the WROOM-1 module |

**Use with care if ever needed** (strapping pins, sampled at boot — fine
for a static signal, avoid anything that toggles during power-up):
| Pin | Note |
|---|---|
| 15 (GPIO3) | Strapping — JTAG signal source select |
| 16 (GPIO46) | Strapping — boot message control. Used for the ICM-42670-P IMU's INT1 line (QMA6100P on V1.1 and earlier; see Accelerometer section below) — safe since INT1 is never driven high through boot until firmware configures it. |
| 26 (GPIO45) | Strapping — flash voltage select |

**Spare, no caveats** (default JTAG — fully reusable as plain GPIO if you
don't need hardware debugging):
| Pin | Default function |
|---|---|
| 32 (GPIO39) | MTCK |
| 33 (GPIO40) | MTDO |
| 35 (GPIO42) | MTMS — now used for XBee SLEEP_RQ (see below) |

## XBee Auxiliary Pins
| Pin | Reason |
|---|---|
| 22 (GPIO14) | XBee ON_SLEEP status input |
| 35 (GPIO42) | XBee SLEEP_RQ (DTR) — host-driven pin-sleep request, see below |
| 8 (GPIO15) | XBee SPI_ATTN — data-ready interrupt, needed for reliable SPI transfers |

Pulled directly from Amidala's actual AmidalaShield netlist (IPC-2581
export), not guessed: ON_SLEEP and SPI_ATTN go to host GPIO on that board
too (ESP32 GPIO15/16 there — same MCU family as this board now, so the
pin roles map over conceptually).

DTR diverges from Amidala's netlist on purpose: Amidala ties it straight
to GND (permanently deasserted, no host-commanded sleep). This board
wires it to host GPIO42 instead (see SPI (XBee) table below) so firmware
can pin-sleep the module during low-power mode rather than keeping the
radio powered continuously — see the "Low-Power Mode" rationale in the
Accelerometer section below.

---

## SPI (XBee)
| Function | Pin |
|---|---|
| XBee SCK | 18 (GPIO10) |
| XBee MOSI | 19 (GPIO11) |
| XBee MISO | 20 (GPIO12) |
| XBee CS | 21 (GPIO13) |
| XBee ON_SLEEP | 22 (GPIO14) |
| XBee SPI_ATTN | 8 (GPIO15) |
| XBee SLEEP_RQ (DTR) | 35 (GPIO42) |

## I2C (OLED, ICM-42670-P IMU, TCA9555 expander)
| Function | Pin |
|---|---|
| SDA | 12 (GPIO8) |
| SCL | 17 (GPIO9) |

The TCA9555 (U_EXP1) is at 0x20 (A0-A2 grounded). R_I2C_SDA1/R_I2C_SCL1 (4.7k) are fitted on the board, because every button now needs the bus even with no OLED attached.

Shared bus — the ICM-42670-P IMU joins at I2C address 0x68 (AP_AD0 tied
to GND), distinct from the OLED's 0x3C. V1.1 and earlier boards have the
QMA6100P accelerometer at 0x12 instead. See Accelerometer section below.

## ADC — ADC1 only, see note below
| Function | Pin | Notes |
|---|---|---|
| Analog trigger (DRV5055) | 4 (GPIO4) | ADC1_CH3 |
| Thumbstick X (GuliKit) | 5 (GPIO5) | ADC1_CH4 |
| Thumbstick Y (GuliKit) | 6 (GPIO6) | ADC1_CH5 |
| VSYS battery sense | 7 (GPIO7) | ADC1_CH6, divide-by-3 via R_VSYS1/R_VSYS2 |

> **Why ADC1 only:** ESP32-S3's ADC2 is unusable while WiFi is active,
> and this design keeps WiFi on. All 4 analog signals are deliberately
> placed on ADC1-capable pins to avoid any contention — unlike the old
> RP2350 design, there's no firmware-side gating workaround needed here.

## Digital Inputs / Outputs
| Function | Pin | Notes |
|---|---|---|
| Macro button 1-6 | U_EXP1 P00-P05 | Active low, expander's internal 100k pull-up |
| Thumbstick click (KEY) | U_EXP1 P06 | Active low, expander's internal 100k pull-up |
| Trigger down | U_EXP1 P07 | Active low, expander's internal 100k pull-up |
| Digital trigger (bumper) | U_EXP1 P10 | Active low, expander's internal 100k pull-up. One switch (SW_TRIG_DIG1), centered on the top edge |
| Trigger up | U_EXP1 P11 | Active low, expander's internal 100k pull-up |
| Vol up | U_EXP1 P12 | Active low, expander's internal 100k pull-up |
| Vol down | U_EXP1 P13 | Active low, expander's internal 100k pull-up |
| Button interrupt (BTN_INT) | 11 (GPIO18) | TCA9555 open-drain INT, 10k pull-up to 3V3. Read both ports: each clears its own interrupt |
| XBee DIN | 23 (GPIO21) | ESP32 TX, for the USB-to-XBee bridge |
| XBee DOUT | 24 (GPIO47) | ESP32 RX, for the USB-to-XBee bridge |
| XBee RTS | 31 (GPIO38) | Needed with DTR (GPIO42) for serial firmware update and recovery |
| Power button sense | 38 (GPIO2) | Input, monitors for 3-second hold |
| Charge status STAT1 (bq25185) | 39 (GPIO1) | Open-drain input, 10kΩ pull-up to 3V3 |
| Charge status STAT2 (bq25185) | 34 (GPIO41) | Open-drain input, 10kΩ pull-up to 3V3 — with STAT1, fully decodes charging/done/fault |
| Power latch hold | 36 (GPIO44) | Output, driven HIGH on boot to hold soft latch |
| RGB LED (WS2812/SK6812) | 37 (GPIO43) | RMT-driven, data line |

## Buttons — I2C Expander
All 12 buttons connect to the TCA9555 (U_EXP1), so simultaneous presses need no diodes. A change on any input pulls BTN_INT low.

## MCU Support Circuitry (ESP32-S3-WROOM-1)

Far simpler than the bare-RP2350A design it replaced — the module
integrates flash, crystal, and the WiFi/BT radio + antenna internally.
What's left is standard ESP32 practice, matching Espressif's own
reference designs (including their DevKitC boards):

| Function | Components | Notes |
|---|---|---|
| EN pull-up | R_EN_PU (10kΩ) | EN has an internal weak pull-up too; external is standard practice for reliable reset timing |
| BOOT pull-up + button | R_BOOT_PU (10kΩ), SW_BOOT | GPIO0 pulled up, button pulls low to force UF2/download mode at boot — same role as the old RP2350 BOOTSEL button |
| USB series resistors | R_USB_DP/R_USB_DM (27Ω) | Between the module's native USB pins (13/14) and the external USB-C connector |
| 3V3 decoupling | C_3V3_1 (10µF bulk), C_3V3_2 (100nF) | Standard module decoupling |
| VSYS battery sense | R_VSYS1 (200kΩ), R_VSYS2 (100kΩ) | Divide-by-3 into pin 7 (ADC1_CH6) |

**No longer needed at all** (this is the point of the pivot): external
QSPI flash chip, crystal + load caps, RP2350 core-regulator LC filter
(VREG_LX/FB), two-stage ADC_AVDD RC filter, per-pin IOVDD/DVDD
decoupling network, separate wireless module + its bit-banged interface
+ antenna licensing question.

## Wireless — Fully Integrated

WiFi/BT radio, antenna, and RF matching are all internal to the
ESP32-S3-WROOM-1 module — no external wireless component, no bit-banged
interface consuming GPIO, nothing to wire in the schematic at all.

**Layout note:** the module's footprint (Espressif's own official
`Espressif.pretty` library) includes the required antenna keepout area —
keep copper/components out of it per the module's datasheet. Much
simpler than the old Murata design's custom PCB trace antenna, but still
a real layout-stage requirement, not just a schematic one.

---

## Passives & Support Components

### Decoupling Caps
| Component | Value | Notes |
|---|---|---|
| DRV5055 VCC | 100nF ceramic | Close to supply pin |
| GuliKit VCC | 100nF ceramic | Close to supply pin |
| WS2812/SK6812 VCC | 100nF ceramic | Close to supply pin |

### Pull-up Resistors
| Signal | Value | Notes |
|---|---|---|
| bq25185 STAT1 pin | 10kΩ | Open-drain, pull up to 3V3 |
| bq25185 STAT2 pin | 10kΩ | Open-drain, pull up to 3V3 |
| I2C SDA/SCL | 4.7kΩ | R_I2C_SDA1, R_I2C_SCL1, fitted: buttons depend on the bus |

### Series Resistors
| Signal | Value | Notes |
|---|---|---|
| RGB DIN (pin 37) | 300–500Ω | Signal integrity protection, standard NeoPixel/SK6812 best practice |

### Buttons (on the TCA9555 expander, except the power button)
No external components needed — the TCA9555's internal 100k pull-ups hold each input high at power-up, and each switch wires to GND. Applies to vol up/down, digital trigger, stick click, and all 6 macro buttons.

### XBee
XBee 3 PRO Micro, `XB3-24Z8UM`, soldered to the top side (`Xbee3:XB3-24Z8UM`, pads per Digi 90001543). The -J part is the +8 dBm non-PRO module.

Beyond the 4-wire SPI bus (SCK/MOSI/MISO/CS) and RESET, three more pins are
wired per Amidala's actual netlist: DTR tied to GND, ON_SLEEP and SPI_ATTN
to pins 22/8. SPI_ATTN in particular is the XBee's data-ready interrupt —
needed for reliable SPI transfers, not just a nice-to-have.

SPI_ATTN also doubles as the module's BOOTMODE strap; Digi's XBee3 Hardware
Reference Manual (doc 90001543) says not to let it be tied/pulled low at
reset. R_XBEE_ATTN_PU1 (10kΩ to 3V3) holds it high before firmware
configures the pin, same pattern as R_XBEE_RST1 on RESET.

VCC decoupling: 100nF + 1µF + 47pF + 10µF (C_XBEE_VCC1-4), per the Digi
XBee3 RF Module Hardware Reference Manual (doc 90001543, p.52) rather than
guessed — the manual calls for 1.0µF + 47pF near VCC plus a 10µF, placed
smallest-value-closest to the module. AmidalaShield's schematic used 8.2pF
here instead of 47pF, which the current XBee3-specific manual doesn't
support (that value traces to older XBee/S2C-family guidance); corrected
here rather than copied.

### Power Control Circuit
See power control section of schematic. Uses pin 38 (power button sense
input) and pin 36 (latch hold output). Pin 36 must be driven HIGH as the
very first instruction in firmware or power will cut on button release.

### Battery Voltage Sense
Pin 7 (ADC1_CH6) reads VSYS/3 via an external R_VSYS1/R_VSYS2 divider —
same role as the RP2350 design's divider, still needed since
ESP32-S3-WROOM-1 has no VSYS-style pin of its own either. bq25185 STAT1
(pin 39) and STAT2 (pin 34) together fully decode the charger's state:
charging, done, recoverable fault, or latched fault — see the bq25185
datasheet's status pin table.

No firmware ADC-gating workaround needed here (unlike the old bit-banged
RP2350+CYW43439 design) — WiFi is fully on-die and doesn't share any
exposed GPIO/ADC path with this sense pin.

---

## Accelerometer (ICM-42670-P IMU) — Low-Power Mode

Added to support an auto low-power mode: after a configurable idle
timeout (no buttons, no motion), firmware turns the OLED off and puts
the XBee into pin sleep (see XBee SLEEP_RQ above) instead of keeping the
radio powered continuously; after a much longer configurable timeout, it
does a full soft-latch power-off. Button or accelerometer motion wakes
the device back up.

**Why ICM-42670-P (from board V1.2):** a 6-axis IMU (3-axis gyroscope +
3-axis accelerometer) replacing the QMA6100P, so later firmware can use
rotation as well as motion. It was the cheapest current-production 6-axis
part with deep stock when chosen (2026-10-25, LCSC): ~$1.65 @10 vs $2.52
BMI270, $3.38 LSM6DSOX, and $1.33 LSM6DS3TR-C (cheaper, but Mouser lists
it obsolete). It keeps the single-pin wake design: a hardware
wake-on-motion interrupt on INT1, so no extra GPIO is needed. Cost:
~$1.20 more per board than the QMA6100P (~$0.47 @10), and higher
low-power current. The datasheet gives 3.5µA full-chip sleep and 0.20mA
for the accelerometer in low-noise mode, but no figure for the
low-power-mode accelerometer that wake-on-motion runs on — **measure the
idle current on the first board.** Source: TDK InvenSense "ICM-42670-P
Datasheet" (DS-000451, Rev 1.1).

**I2C wiring** (per the datasheet's Figure 5, I3C/I2C Interface to
Host): shares the existing OLED bus (GPIO8/9).
| ICM-42670-P pin | Connection |
|---|---|
| AP_AD0 (1) | GND — sets I2C address to 0x68 |
| RESV (2, 3) | GND (datasheet allows open, GND or VDDIO) |
| INT1 (4) | Host GPIO46 (pin 16) — see Reserved/Do Not Use above |
| VDDIO (5) | 3V3 |
| GND (6) | GND |
| FSYNC (7) | GND (unused; the datasheet says tie it to GND) |
| VDD (8) | 3V3 |
| INT2 (9) | Unused |
| RESV (10, 11) | GND |
| AP_CS (12) | 3V3 — selects I2C (tied to VDDIO) |
| AP_SCL (13) | I2C_SCL (shared with OLED) |
| AP_SDA (14) | I2C_SDA (shared with OLED) |

Decoupling per the datasheet's Table 10: 100nF and 2.2µF X7R on VDD,
10nF X7R on VDDIO (C_IMU_1..3).

INT1 resets to open-drain, active-low (INT_CONFIG = 0x00), and the
reset-done interrupt is routed to it by default, so before firmware runs
it can only pull GPIO46 low or leave it to the ESP32's default pull-down
— never high. Firmware turns the reset-done routing off and switches INT1
to push-pull, active-high and latched, the same signal the QMA6100P
gave.

**Footprint:** KiCad's stock
`Package_LGA:LGA-14_3x2.5mm_P0.5mm_LayoutBorder3x4y`, checked against the
datasheet's package drawing (section 10.2): pad centers at ±1.1625mm
(4-pad sides) and ±0.9125mm (3-pad sides), 0.5mm pitch, 0.625×0.35mm pads
over 0.475×0.25mm leads, pin 1 top-left, numbered counter-clockwise like
the datasheet's Figure 4.

### V1.1 and earlier: QMA6100P

**Why QMA6100P (at the time):** chosen over MSA311/LIS3DH/ADXL345/MPU-6050 after
checking JLCPCB's actual parts library (as of 2026-09) — none of the
candidates are Basic-library (that framing was wrong when first floated;
corrected after verifying against JLCPCB's own part pages and the
community-maintained `jlcpcb-parts-database` mirror of it), so library
tier was a wash. QMA6100P won on the remaining criteria: cheapest
($0.61 @1pc vs $1.88 LIS3DH / $5.98 ADXL345, LCSC pricing), smallest
(2x2x0.95mm, tied with MSA311), and by far the lowest standby current
(500nA typ., vs ~2µA+ for the alternatives) — the one spec that matters
most here, since this chip has to stay active watching for motion
through the entire low-power period. It also exposes real hardware
any-motion/no-motion interrupts (not just raw acceleration), which is
what makes MCU light-sleep-and-wake-on-interrupt possible instead of
busy-polling. Source: QST "QMA6100P Preliminary Datasheet" (Doc #13-52-20,
Rev A1).

**I2C wiring** (per the datasheet's Figure 4, I2C Single Supply
Connection): shares the existing OLED bus (GPIO8/9).
| QMA6100P pin | Connection |
|---|---|
| AD0 (1) | GND — sets I2C address to 0x12 |
| SDX (2) | I2C_SDA (shared with OLED) |
| VDD (3) | 3V3 |
| RESV1 (4) | GND |
| INT1 (5) | Host GPIO46 (pin 16) — see Reserved/Do Not Use above |
| INT2 (6) | Unused |
| NC (7) | No connect |
| GNDIO (8) | GND |
| GND (9) | GND |
| SENB (10) | 3V3 — selects I2C protocol (tied high rather than left floating) |
| RESV2 (11) | GND |
| SCX (12) | I2C_SCL (shared with OLED) |

Decoupling per the datasheet's Figure 4 reference schematic: three caps
on VDD (10nF, 100pF, 2.2nF) rather than this board's usual single
100nF — **the two smallest values are a best-effort read of small print
in a preliminary-datasheet figure and should be visually re-checked
against the datasheet before fab.**

**Status: in Stackup** (`PCB/v2.x/stackup/board.kdl`), with
a hand-authored symbol + footprint (`SnipsControllers_Custom:QMA6100P`) —
pad geometry pulled from JLCPCB/LCSC's own EasyEDA component data (LCSC
C2887190) rather than estimated from the datasheet's low-resolution
mechanical drawing. ERC-clean (`kicad-cli sch erc`, 0 violations).
**Still needs the user's physical continuity check against the real part
before fab** — same outstanding step as the GuliKit hallstick footprint.

---

## Spare GPIO
Free on V2: GPIO16, 17, 35, 36, 37, 39, 40, 48. All 12 buttons moved to the TCA9555 expander.
Do not use GPIO45 on XBee DOUT: it must be low at reset or the 3.3 V flash gets 1.8 V (module datasheet Table 4-4).
Pin 34 (GPIO41, default JTAG) — used by bq25185 STAT2.
Pin 35 (GPIO42, default JTAG) — used by XBee SLEEP_RQ (DTR).
Pin 15, 26 (GPIO3/45) — usable, but strapping pins, best for a
static/non-boot-critical signal.
Pin 16 (GPIO46) — strapping pin, used for the ICM-42670-P IMU's INT1 (the
QMA6100P's on V1.1 and earlier — see Accelerometer section above).
