# SnipsControllers PCB — Design Review (Updated)
**Date:** 2026-08-23 (rev 2) | **Analyzer:** kicad-happy v2.2.0 | **Run:** 2026-08-23_1900 / 2026-08-23_1900-2 | **Prior run:** 2026-08-23_1553

## Delta Since Prior Review

| # | Item | Status |
|---|---|---|
| BUG-001 | R_FB1 100kΩ→255kΩ — buck now outputs **3.322V** | ✅ Fixed |
| HIGH-001 | VBUS TVS added (PESD5V0S1BA, SOD-323, B.Cu) | ✅ Fixed |
| HIGH-003 | Fiducials added — FD-001 cleared on both faces | ✅ Fixed |
| HIGH-004 | Via in pad U_MCU1:4 — VP-001 gone | ✅ Fixed |
| HIGH-005 | SW_BOOT1 moved inboard (0.0mm→0.5mm, now warning) | ✅ Improved |
| MED-003 | SPI_SCK stitching vias added — SPI_SCK gone from GP-001/RP-001 | ✅ Fixed |
| INFO-013 | C_3V3_3 39pF added near MCU for 955MHz PDN gap | ✅ Fixed |
| — | SW_BOOT1 now overlaps XBee courtyard after inboard move | ⚠ New |
| — | SPI_MOSI now appears in GP-001 (91%, 75mm) | ⚠ New |

---

## Overview

| | |
|---|---|
| **Board** | 35.0 × 99.9mm, 4-layer |
| **Stackup** | F.Cu (signal) / In1.Cu (3V3 plane, 83%) / In2.Cu (GND plane, 85%) / B.Cu (signal) |
| **Components** | 71 schematic, 81 PCB footprints (10 mechanical/fiducials) |
| **Routing** | Complete (111 vias, 0 unrouted) |
| **Verification basis** | Internal consistency only — **0 datasheets, 70/71 parts missing MPN** |

---

## Remaining Blockers

### BUG-002 · Missing MPNs on 70 of 71 components (SS-001)

Pre-fab gate: cannot order. Note — D_VBUS_TVS1 was placed with value `PESD5V0S1BA` but the MPN property in the KiCad symbol is not populated; the part counter still shows 1/71 with MPN. Populate the `MPN` field in the schematic symbol to close this.

---

## High

### HIGH-002 · BQ25185 STAT2 pin floating (PU-001)

Unchanged. STAT2 (pin 3) → NO_CONNECT. If not monitoring charging state via STAT2, add a DNP pull-up (4.7kΩ to 3V3) so the open-drain output isn't left floating.

---

## Medium

### MED-001 · XBee courtyard overlaps — now 8 components (PM-001)

One new overlap added this revision: SW_BOOT1 (1.562mm²) now overlaps U_XBEE1's courtyard after being moved inboard. This is the root-cause problem — the XBee courtyard is very large (likely includes the RF keepout zone). **The fix is to correct the footprint courtyard** to the physical module outline only (not the RF keepout), then enforce the keepout separately with a rule area. Once that's done, all 8 overlaps should clear.

Affected: C_XBEE_RC1, C_XBEE_VCC1–4, R_XBEE_RC1, R_XBEE_RST1, SW_BOOT1, REF** (fiducial, 0mm² — effectively touching).

### MED-002 · Edge-mounted switch overhang (PM-002)

Unchanged. SW_PWR1 (0.35mm), SW_THR_DN1 / SW_THR_UP1 / SW_TRIG_DIG1 (0.55mm), SW_VOL_DN1 / SW_VOL_UP1 (0.5mm), U_HALL1 (0.27mm). If intentionally edge-mounted for a controller form factor, suppress these in KiCad's DRC configuration. C_OLED_1 (0.8mm) and D_RGB1 (0.5mm) remain close to edge as warnings.

### MED-003 · SPI_MOSI reference plane coverage 91%, 75mm (GP-001) — new this revision

SPI_SCK is fully resolved. SPI_MOSI is a new (softer) appearance in GP-001 at 91% coverage over 74.9mm. At 91% the gap is small, but given SPI_MOSI runs nearly the full board length, add a stitching via near any F.Cu↔B.Cu layer transition on this net. Priority is lower than SPI_SCK was.

### MED-004 · No test points (TE-001)

Unchanged — 0% coverage. Add test points on: GND, 3V3, VSYS, VBAT, VBUS, USB_DP_MCU/DM_MCU, SPI_SCK/MOSI/MISO, I2C_SDA/SCL, MCU_EN.

---

## Power Tree

```
VBUS (USB-C J_USB1)
  ├─ D_VBUS_TVS1 (PESD5V0S1BA) → GND  ✅ new ESD clamp
  └─ BQ25185 (U_CHG1) → VSYS
       R_ISET1=600Ω  R_VSET1=18kΩ  R_TS1=10kΩ→GND (NTC disabled)
       STAT1 → 10kΩ pull-up → MCU GPIO1  |  STAT2 → NC (open-drain floating)
       VBAT ← BT1 (battery)

VSYS → U_BUCK1 (TLV62569) → 3V3  ✅ corrected
  R_FB1=255kΩ / R_FB2=56.2kΩ → Vout=3.322V  (Vref=0.6V, heuristic)
  L=2.2µH SRN4018, f=500kHz
  Tj=64°C, margin 61°C to Tj_max  ✅ thermal OK
  Estimated load: ESP32-S3 (240mA) + XBee3 (10mA) + DRV5055A2 (5mA) + OLED (10mA) = 265mA
```

---

## PCB Layout

| Metric | Prior | Now | Status |
|---|---|---|---|
| Routing | Complete | Complete | OK |
| Vias | 110 | 111 | OK (+1 SPI_SCK stitch) |
| Fiducials F.Cu | 0 | ≥1 | ✅ |
| Fiducials B.Cu | 0 | ≥1 | ✅ |
| Via in pad U_MCU1:4 | Untented | Resolved | ✅ |
| SW_BOOT1 edge clearance | 0.0mm (error) | 0.5mm (warning) | Improved |
| XBee courtyard overlaps | 7 | 8 | ⚠ +SW_BOOT1 |
| Test points | 0% | 0% | Open |

---

## EMC Assessment

| Signal | Coverage | Length | Severity | Notes |
|---|---|---|---|---|
| XBEE_RESET | 25% | 2.6mm | Error | Short; low absolute risk |
| BTN_THR_UP | 68% | 24mm | Error | Button line; low speed |
| I2C_SCL | 73% | 19mm | Error | Low speed; tolerable |
| BTN_MACRO1 | 75% | 38mm | Error | Button line |
| TRIG_ANALOG | 77% | 27mm | Error | ADC; low frequency |
| SPI_SCK | — | — | ✅ Resolved | Stitching vias added |
| SPI_MOSI | 91% | 75mm | Warning | New; add stitch via |
| SU-001 stackup | — | — | False positive | Inner layers are 3V3+GND planes |
| U_BUCK1 harmonics 30–88MHz | — | — | Info | Inherent; add ferrite if needed |

**Stackup note:** SU-001 "adjacent signal layers" errors are false positives. In1.Cu is a 3V3 copper pour (83% fill), In2.Cu is a GND copper pour (85% fill). Standard 4-layer stackup: signal / power / ground / signal.

---

## Thermal

U_BUCK1 (TLV62569): Tj = **64°C** at 25°C ambient, 61°C margin to Tj_max. No other components flagged. Thermal is clean.

---

## Schematic Quality

| Finding | Status |
|---|---|
| PWR_FLAG missing on VBAT, VBUS, PWR_* rails (RS-001) | Open |
| STAT2 floating (HIGH-002 above) | Open |
| D_VBUS_TVS1 MPN field not populated | Open |
| XBee RESET pull-up PU-001 | False positive (R_XBEE_RST1 to 3V3 provides pull-up) |
| BQ25185 TS/MR PU-001 | False positive (R_TS1=10kΩ to GND intentionally disables NTC) |

---

## Firmware Notes (unchanged)

- **PWR_HOLD must be asserted early:** During boot, GPIO44 (PWR_HOLD) defaults to high-impedance. The board stays powered only while the button is held via D_OR1. Assert PWR_HOLD high early in `app_main()` before the RTOS scheduler yields.
- **GPIO44 = U0RXD conflict:** During serial flashing, ESP32-S3 uses GPIO44 as UART0 RX. Configure PWR_HOLD as output only after the UART boot window closes.

---

## Priority Action List

| # | Action | Severity |
|---|---|---|
| 1 | Add MPNs to all 70 remaining components | BLOCKER |
| 2 | Populate `MPN` field on D_VBUS_TVS1 (PESD5V0S1BA) | BLOCKER |
| 3 | Fix XBee footprint courtyard → physical outline only; add RF keepout as rule area | MEDIUM |
| 4 | Add stitching via near SPI_MOSI layer transition | MEDIUM |
| 5 | Decide edge switches: intentional → suppress DRC; else move inboard | MEDIUM |
| 6 | Add test points on power rails, USB, SPI, I2C | MEDIUM |
| 7 | Add STAT2 DNP pull-up 4.7kΩ → 3V3 | LOW |
| 8 | Add PWR_FLAG to VBAT, VBUS, PWR_* rails | LOW |

---

## Skipped Analyses

| Analysis | Reason |
|---|---|
| Datasheet verification | No MPNs / no datasheets on disk |
| SPICE simulation | No ngspice/LTspice/Xyce installed |
| Gerber verification | No gerber files in project |
| Lifecycle audit | No MPNs to query |
