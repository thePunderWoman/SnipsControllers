# Snips Controller — ESP32-S3-WROOM-1 GPIO Table

> **⚠ Known kicaddy bug (2026-08-17):** the yaml's `footprint:` field is not
> currently applied by `kicaddy compile` — compiled `.kicad_sch` files have
> blank/wrong Footprint properties for most components right now. The
> `footprint:` values in `snips_controller.yaml` are still accurate as
> documentation of intent; they're just not making it into the compiled
> output yet. **Don't start PCB layout from the current compiled schematic's
> footprints** until this is fixed — assign footprints manually in the
> KiCad GUI in the meantime, or wait for the fix.

> **MCU:** ESP32-S3-WROOM-1 module (pre-certified, integrated flash +
> crystal + WiFi/BT radio + PCB antenna). No external RF design, flash,
> or crystal needed — the module handles all of that internally. Custom
> USB-C for firmware flashing, charging, and future accessory expansion
> (the module's native USB is genuinely exposed, unlike a Pico-family
> module whose USB is hardwired to its own onboard connector).
> Pins referenced by number in the yaml, not name — the module's pins are
> heavily multi-function (e.g. pin 4 is "GPIO4/TOUCH4/ADC1_CH3") and
> numbers are unambiguous. See `MCU_Core` section of `snips_controller.yaml`.

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
| 16 (GPIO46) | Strapping — boot message control |
| 26 (GPIO45) | Strapping — flash voltage select |

**Spare, no caveats** (default JTAG — fully reusable as plain GPIO if you
don't need hardware debugging):
| Pin | Default function |
|---|---|
| 32 (GPIO39) | MTCK |
| 33 (GPIO40) | MTDO |
| 35 (GPIO42) | MTMS |

## XBee Auxiliary Pins
| Pin | Reason |
|---|---|
| 22 (GPIO14) | XBee ON_SLEEP status input |
| 8 (GPIO15) | XBee SPI_ATTN — data-ready interrupt, needed for reliable SPI transfers |

Pulled directly from Amidala's actual AmidalaShield netlist (IPC-2581
export), not guessed: DTR is tied low, ON_SLEEP and SPI_ATTN go to host
GPIO on that board too (ESP32 GPIO15/16 there — same MCU family as this
board now, so the pin roles map over conceptually).

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

## I2C (OLED)
| Function | Pin |
|---|---|
| SDA | 12 (GPIO8) |
| SCL | 17 (GPIO9) |

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
| Vol up | 33 (GPIO40) | Internal pull-up, active low |
| Vol down | 32 (GPIO39) | Internal pull-up, active low |
| Trigger up (mirrored VOL pair, opposite-handed variant) | 9 (GPIO16) | Internal pull-up, active low |
| Trigger down (mirrored VOL pair, opposite-handed variant) | 10 (GPIO17) | Internal pull-up, active low |
| Digital trigger (bumper) | 11 (GPIO18) | Internal pull-up, active low |
| Thumbstick click (KEY) | 23 (GPIO21) | Internal pull-up, active low |
| Macro button 1-6 | 24, 25, 28, 29, 30, 31 (GPIO47, 48, 35, 36, 37, 38) | Internal pull-up, active low — direct-wired, no scan matrix |
| Power button sense | 38 (GPIO2) | Input, monitors for 3-second hold |
| Charge status STAT1 (bq25185) | 39 (GPIO1) | Open-drain input, 10kΩ pull-up to 3V3 |
| Charge status STAT2 (bq25185) | 34 (GPIO41) | Open-drain input, 10kΩ pull-up to 3V3 — with STAT1, fully decodes charging/done/fault |
| Power latch hold | 36 (GPIO44) | Output, driven HIGH on boot to hold soft latch |
| RGB LED (WS2812/SK6812) | 37 (GPIO43) | RMT-driven, data line |

## Macro Buttons — Direct-Wired, No Scan Matrix
6 macro buttons, each wired straight to its own GPIO with an internal
pull-up (same pattern as the other direct buttons). No anti-ghosting
diodes, no row/column scan firmware.

**Why not a 2x3 matrix?** The RP2350A design used a matrix because it
only had 1 spare GPIO — a matrix costs 5 pins for 6 buttons instead of 6,
a savings worth the diodes and scan complexity when pins are that scarce.
ESP32-S3-WROOM-1 has roughly 8 pins to spare even after every other
signal is assigned (see below), so that 1-pin savings isn't worth it
anymore — direct wiring is simpler to wire, debug, and reason about.

---

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
| I2C SDA/SCL | 4.7kΩ | **DNP for prototyping** — Adafruit #938 has onboard pull-ups. Populate only on final PCB with bare OLED panel |

### Series Resistors
| Signal | Value | Notes |
|---|---|---|
| RGB DIN (pin 37) | 300–500Ω | Signal integrity protection, standard NeoPixel/SK6812 best practice |

### Buttons (all direct-wired, no matrix)
No external components needed — ESP32-S3 internal pull-ups enabled in
firmware, wire to GND. Applies to vol up/down, digital trigger, stick
click, and all 6 macro buttons.

### XBee
Reusing Amidala's verified `XB3-24Z8UT-J` symbol (`PCB/libraries/Xbee3.kicad_sym`).
THT socket footprint used for prototyping; SMT module footprint TBD for final PCB.

Beyond the 4-wire SPI bus (SCK/MOSI/MISO/CS) and RESET, three more pins are
wired per Amidala's actual netlist: DTR tied to GND, ON_SLEEP and SPI_ATTN
to pins 22/8. SPI_ATTN in particular is the XBee's data-ready interrupt —
needed for reliable SPI transfers, not just a nice-to-have.

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

## Spare GPIO
Pins 32/33 (GPIO39/40, default JTAG) — used by BTN_VOL_DN/BTN_VOL_UP.
Pin 34 (GPIO41, default JTAG) — used by bq25185 STAT2.
Pin 35 (GPIO42, default JTAG) — clean, no caveats, still free.
Pins 15, 16, 26 (GPIO3/46/45) — usable, but strapping pins, best for a
static/non-boot-critical signal.
