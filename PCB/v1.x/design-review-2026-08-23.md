# SnipsControllers PCB — Design Review
**Date:** 2026-08-23 | **Analyzer:** kicad-happy v2.2.0 | **Run:** 2026-08-23_1949

## Overview

| | |
|---|---|
| **Board** | 35.0 × 99.9mm, 4-layer |
| **Stackup** | F.Cu (signal) / In1.Cu (3V3 plane, 83% fill) / In2.Cu (GND plane, 85% fill) / B.Cu (signal) |
| **Components** | 71 schematic, 81 PCB footprints (includes fiducials and mechanical) |
| **Routing** | Complete — 109 vias, 0 unrouted nets |
| **MPN coverage** | 70/71 (J_STICK1 is a custom part with no standard MPN — expected) |
| **Verification basis** | MPN-level consistency — no datasheets synced yet, so no pin-level or value verification against manufacturer specs |

---

## Schematic

**No errors.** 10 warnings — all either false positives or accepted design decisions (detailed below).

### False positives

**PU-001 — XBee RESET pull-up:** The analyzer flags U_XBEE1 pin RESET for a missing pull-up. This is incorrect — R_XBEE_RST1 pulls XBEE_RESET to 3V3 through XBEE_RST_PU, with R_XBEE_RC1/C_XBEE_RC1 forming a 159Hz debounce RC. Pull-up is present.

**PU-001 — BQ25185 TS/MR:** R_TS1 = 10kΩ to GND is intentional. This disables the NTC thermistor function on the charger. If a battery NTC is added later, replace R_TS1 with a 10kΩ NTC.

**RS-001 — PWR_GATE, PWR_LATCH_G, PWR_BTN_SENSE, PWR_EN, PWR_HOLD, VSYS_SENSE:** These rails are driven by discrete logic (FETs, diodes) or MCU GPIO. The analyzer cannot trace sourcing through passive topology for these nets. Add `PWR_FLAG` symbols if ERC cleanliness is important; no functional issue.

### Accepted open items

**PU-001 — BQ25185 STAT2:** STAT2 (open-drain) → NO_CONNECT. Acceptable if STAT2 status monitoring is not needed. If it causes noise concerns later, add a DNP 4.7kΩ pull-up to 3V3.

**DS-002 — No datasheets synced:** 70/71 MPNs are now populated. Syncing datasheets (via DigiKey/LCSC scripts) would enable pin-level verification and decoupling adequacy checks against manufacturer specs. Not blocking for prototype.

---

## Power Tree

```
VBUS (USB-C J_USB1)
  ├─ D_VBUS_TVS1 (PESD5V0S1BA, SOD-323) → GND   [ESD clamp]
  └─ U_CHG1 (BQ25185DLHR)
       IN=VBUS  →  SYS=VSYS  |  BAT=VBAT ← BT1 (Li-Ion battery)
       R_ISET1=600Ω   (charge current — verify against BQ25185 datasheet)
       R_VSET1=18kΩ   (charge voltage set — verify against datasheet)
       R_TS1=10kΩ→GND (NTC disabled)
       STAT1 → R_STAT1 (10kΩ) → 3V3, read by MCU GPIO1
       STAT2 → NO_CONNECT

VSYS → Power latch
  SW_PWR1 (button) → D_OR1 → PWR_LATCH_G → Q_LATCH1.G (2N7002 NMOS)
  MCU GPIO44 (PWR_HOLD) → D_OR2 → PWR_LATCH_G
  Q_LATCH1.D → PWR_GATE → Q_PWR1.G (DMG2305UX PMOS)
  Q_PWR1: S=VSYS, D=PWR_EN → U_BUCK1.EN
  R_GATE1=100kΩ VSYS→PWR_GATE (pull-up, keeps PMOS off by default)
  R_LATCH_G1=10kΩ PWR_LATCH_G→GND (pull-down, keeps NMOS off by default)

VSYS → U_BUCK1 (TLV62569, switching 500kHz) → 3V3
  R_FB1=255kΩ / R_FB2=56.2kΩ → Vout = 3.322V  (Vref=0.6V)
  L_BUCK1=2.2µH (Bourns SRN4018)
  Cin:  10µF + 10µF + 100nF (VSYS rail)
  Cout: 22µF + 3×10µF + 1µF + 5×100nF + 47pF (3V3 rail)
  Tj=64°C at 25°C ambient, margin 61°C to Tj_max   ✓ thermal OK

3V3 load estimate:
  ESP32-S3-WROOM-1-N8 (U_MCU1)   240mA
  XBee 3 (U_XBEE1)                10mA
  DRV5055A2 (U_HALL1)              5mA
  OLED connector (U_OLED1)        10mA
  ─────────────────────────────────────
  Total                           265mA
```

### Firmware notes

- **PWR_HOLD must be asserted immediately on boot.** GPIO44 defaults to high-impedance. While it floats, D_OR2 does not conduct and the system stays powered only while the button is physically held via D_OR1. Firmware must drive PWR_HOLD high in `app_main()` before yielding to the RTOS scheduler.
- **GPIO44 = U0RXD conflict.** The ESP32-S3 uses GPIO44 as UART0 RX during serial flashing. Configure GPIO44 as a push-pull output only after the UART boot window has closed to avoid conflicting with the programmer's TX line.

---

## Signal Analysis

**USB-C:** CC1/CC2 pull-downs R_CC1=R_CC2=5.1kΩ to GND — correct sink identification. D+/D- connected to MCU GPIO19/20 (native USB PHY) through 27Ω series resistors — appropriate for USB FS.

**SPI (XBee):** MOSI/MISO/SCK/CS on ESP32-S3 hardware SPI pins (GPIO10–13). XBee decoupling: 4×VCC caps (100nF×2, 1µF, 10µF), RC reset filter (159Hz), SPI attention line to MCU GPIO15.

**I2C (OLED):** GPIO8=SDA, GPIO9=SCL. Analyzer did not detect I2C pull-up resistors — verify pull-ups are present (typically 4.7kΩ to 3V3 for 400kHz operation).

**Analog:** TRIG_ANALOG (DRV5055A2 hall sensor) → GPIO4/ADC1_CH3. STICK_X/Y (GuliKit thumbstick) → GPIO5–6/ADC1_CH4–5. VSYS_SENSE (R_VSYS1=200kΩ / R_VSYS2=100kΩ divider, VSYS/3) → GPIO7/ADC1_CH6.

**RGB LED:** D_RGB1 data in from GPIO43 via R_RGB1.

**RC filters:** R_XBEE_RC1/C_XBEE_RC1 at 15.9kHz (SPI line filter), R_XBEE_RST1/C_XBEE_RC1 at 159Hz (reset debounce).

---

## PCB Layout

| Metric | Value | Status |
|---|---|---|
| Board dimensions | 35.0 × 99.9mm | — |
| Copper layers | 4 | — |
| Routing | 100% complete | ✓ |
| DFM tier | Standard | ✓ |
| DRC violations | 0 | ✓ |
| Min track width | 0.2mm | ✓ |
| Min drill | 0.3mm | ✓ |
| Min annular ring | 0.15mm | ✓ |
| Fiducials | ≥1 per face | ✓ |
| Test points | 0% coverage | Open |
| XBee courtyard overlaps | 8 components | Accepted (see below) |
| Edge switch overhangs | 7 instances | Accepted (see below) |

### Accepted placement findings

**XBee courtyard overlaps (PM-001):** C_XBEE_RC1, C_XBEE_VCC1–4, R_XBEE_RC1, R_XBEE_RST1, SW_BOOT1, and REF** (fiducial) all overlap the U_XBEE1 courtyard. Root cause: the XBee footprint courtyard encompasses the RF keepout zone rather than just the physical module outline. The decoupling components are correctly positioned for the module; the courtyard definition is the inaccuracy. If fab-submitted as-is, assembly will not be affected. **Suppress these in DRC** or correct the footprint courtyard to the physical module boundary and enforce the RF keepout separately as a rule area.

**Edge-mounted switches (PM-002):** SW_PWR1 (0.35mm overhang), SW_THR_DN1 / SW_THR_UP1 / SW_TRIG_DIG1 (0.55mm), SW_VOL_DN1 / SW_VOL_UP1 (0.5mm), U_HALL1 (0.27mm from edge), SW_BOOT1 (0.5mm from edge) are all intentionally placed at or near the board edge for a handheld controller form factor. C_OLED_1 (0.8mm) and D_RGB1 (0.5mm) are near-edge warnings. **Suppress these in DRC** with a board-edge clearance exception.

**REF** in PCB not in schematic (XV-001):** Expected — fiducial markers are PCB-only. No action needed.

### Open item

**Test points (TE-001):** 0% net coverage. Recommend adding test points before production on: GND, 3V3, VSYS, VBAT, VBUS, USB_DP_MCU / USB_DM_MCU, SPI_SCK / MOSI / MISO, I2C_SDA / SCL, MCU_EN.

---

## EMC Assessment

This is a handheld consumer device with no FCC/CE certification requirement indicated. EMC findings are noted for awareness; none are blocking for prototype.

**Stackup (SU-001 — false positive):** The analyzer flags F.Cu, In1.Cu, In2.Cu, and B.Cu as all "signal" type and reports adjacent signal layer crosstalk. In reality In1.Cu is a 3V3 copper pour (83% fill) and In2.Cu is a GND copper pour (85% fill). The stackup is correct: signal / power plane / ground plane / signal. KiCad stores copper-pour planes as "signal" type, which confuses the EMC analyzer. These three SU-001 findings can be disregarded.

**Ground plane coverage (GP-001):** Several signals cross areas where the In1.Cu (3V3) and In2.Cu (GND) planes don't fully overlap, creating return current detours. The affected signals are all low-speed (button GPIO, I2C at 400kHz, analog ADC inputs) — radiated risk is low in practice.

| Signal | Coverage | Length | Risk |
|---|---|---|---|
| XBEE_RESET | 25% | 2.6mm | Low (very short trace) |
| BTN_THR_UP | 68% | 24mm | Low (slow GPIO) |
| I2C_SCL | 73% | 19mm | Low (400kHz) |
| BTN_MACRO1 | 75% | 38mm | Low (slow GPIO) |
| TRIG_ANALOG | 77% | 27mm | Low (ADC, slow) |
| STICK_X, BTN_STICK | 80–81% | 20–24mm | Low |
| BTN_THR_DN through BTN_MACRO6 | 82–93% | 33–65mm | Low |
| XBEE_SPI_ATTN, XBee_CS, STAT1 | 86–89% | 81–103mm | Low–Medium |
| STICK_Y, XBEE_ON_SLEEP | 91–93% | 42–93mm | Low |

**Return path stitching (RP-001):** 20+ nets have F.Cu↔B.Cu via transitions without adjacent ground stitching vias. Same root cause as GP-001 — affects the same slow signals. For a non-certified handheld, these can be accepted. If EMC pre-compliance becomes a concern, add a GND stitching via near each layer-transition via on the signals with the lowest coverage.

**SPI_SCK (CK-001, CK-003):** SPI_SCK is fully routed on outer layers (microstrip) and passes within 8.5–9mm of J_STICK1 and J_USB1. At typical SPI frequencies (≤20MHz) this is unlikely to cause coupling issues, but if signal integrity becomes a concern, route SPI_SCK on an inner layer.

**Buck converter harmonics (SW-001):** U_BUCK1 switching at 500kHz produces 117 harmonics in the 30–88MHz band. Inherent to any switching regulator. Add a ferrite bead on the VSYS input to U_BUCK1 if conducted emissions become a concern.

---

## Thermal

| Component | Package | Tj (25°C ambient) | Margin to Tj_max |
|---|---|---|---|
| U_BUCK1 (TLV62569) | SOT-23-5 | 64°C | 61°C |

All other components below thermal threshold. No action needed.

---

## Next Steps Before Ordering

| Priority | Action |
|---|---|
| Recommended | Sync datasheets (`sync_datasheets_digikey.py` or `lcsc`) to enable pin-level verification against manufacturer specs |
| Recommended | Verify I2C pull-up resistors are present on I2C_SDA / I2C_SCL |
| Recommended | Suppress accepted DRC findings (XBee courtyard, edge switches) in KiCad DRC config |
| Optional | Add J_STICK1 MPN (custom part — add an internal part number if one exists) |
| Optional | Add test points before production run |
| Optional | Add PWR_FLAG symbols to clean up ERC |
| Optional | Add DNP pull-up on STAT2 (4.7kΩ to 3V3) |

---

## Skipped Analyses

| Analysis | Reason |
|---|---|
| Pin-level datasheet verification | No datasheets synced (MPNs now available — run sync to enable) |
| SPICE simulation | No ngspice / LTspice / Xyce installed |
| Gerber verification | No gerber files in project directory |
| Component lifecycle audit | Network access or API keys needed; run `analyze_schematic.py --lifecycle` |
