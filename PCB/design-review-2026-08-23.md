# SnipsControllers PCB — Design Review
**Date:** 2026-08-23 | **Analyzer:** kicad-happy v2.2.0 | **Run:** 2026-08-23_1553

## Overview

| | |
|---|---|
| **Board** | 35.0 × 99.9mm, 4-layer |
| **Stackup** | F.Cu (signal) / In1.Cu (3V3 plane, 83% fill) / In2.Cu (GND plane, 85% fill) / B.Cu (signal) |
| **Components** | 69 total, 33 unique, 0 DNP |
| **Routing** | Complete (110 vias, 0 unrouted) |
| **Verification basis** | Internal consistency only — **0 datasheets, 68/69 parts missing MPN**. No claim in this review is datasheet-backed. |

---

## Blockers — Must Fix Before Ordering

### BUG-001 · Buck feedback resistors produce 1.668V, not 3.3V (CRITICAL)

**Confidence: high (TLV62569 Vref=0.6V is well-documented)**

The TLV62569 feedback divider is wrong. With Vref = 0.6V, the formula is:

```
Vout = Vref × (1 + R_FB1 / R_FB2)
     = 0.6 × (1 + 100kΩ / 56.2kΩ)
     = 1.668V    ← not 3.3V
```

For 3.3V output: R_FB1 needs to be **≈ 252.9kΩ** (nearest E96: 255kΩ → 3.27V, or 249kΩ → 3.19V).

The inductor (2.2µH SRN4018) and output caps (22µF + four 10µF + ...) are correctly sized for 3.3V at 500kHz — the feedback network is the only mismatch. Everything connected to 3V3 (ESP32-S3, XBee 3, DRV5055A2, OLED) would be starved at 1.67V.

**Fix:** Change R_FB1 from 100kΩ to **255kΩ** (E96, 1%).

---

### BUG-002 · Missing MPNs on 68 of 69 components

SS-001 pre-fab gate. Only `D_OR2` has a populated MPN. Every component must have an MPN before ordering. This also means no datasheet-backed verification is possible anywhere in this review.

---

## High — Fix Before Prototype

### HIGH-001 · No ESD protection on USB VBUS (UC-002)

J_USB1 VBUS is unprotected. A surge on the USB cable goes directly to the BQ25185 IN pin and the board. Add a 5V TVS diode (e.g. PRTR5V0U2X or similar) between VBUS and GND, placed as close as possible to the connector.

### HIGH-002 · BQ25185 STAT2 pin floating (PU-001)

STAT2 (pin 3) is connected to NO_CONNECT. STAT2 is an open-drain output — leaving it floating is fine electrically if you don't need to read it, but the analyzer flags it because unmonitored open-drain outputs can cause undefined leakage. If you don't need STAT2: add a DNP pull-up (4.7kΩ to 3V3) so the net is defined, or add a no-connect marker explicitly in KiCad.

### HIGH-003 · No fiducials on either layer (FD-001)

Neither F.Cu (41 SMD components) nor B.Cu (24 SMD components) has fiducials. The ESP32-S3-WROOM-1 module has 0.30mm fine-pitch pads — pick-and-place alignment needs at least 3 fiducials per populated face. Add fiducials in the corners, outside any component keep-out area.

### HIGH-004 · Via in pad U_MCU1:4, untented (VP-001)

Pad 4 of the ESP32-S3 module (GPIO4/TRIG_ANALOG) has an untented via. During reflow, solder wicks through the via bore, leaving a cold or starved joint on the module pad. Either:
- Tent the via (solder mask both sides), or
- Move the via off the pad (dog-leg route away first)

### HIGH-005 · SW_BOOT1 at 0.0mm from board edge (PM-002)

The boot button courtyard touches the board edge. Depending on the PCB fab's routing tolerance (typically ±0.1mm), the component itself could be partially outside the board. Move SW_BOOT1 inboard by ≥0.3mm.

---

## Medium — Address Before Production

### MED-001 · Courtyard overlaps around U_XBEE1 (PM-001)

Seven components overlap the XBee 3 module courtyard: C_XBEE_RC1, C_XBEE_VCC1–4, R_XBEE_RC1, R_XBEE_RST1 (1.7–4.3mm² overlap each). Likely cause: the XBee footprint's courtyard includes the RF keepout zone, which is larger than the physical module body.

**Check:** Does the XBee3 (`XB324Z8UTJ`) footprint courtyard match the module's physical outline in the mechanical drawing, or does it include RF antenna clearance? If it's oversized, trim the courtyard to the physical module boundary and run DRC again.

### MED-002 · Edge-mounted switch overhang (PM-002)

SW_PWR1 (0.35mm), SW_THR_DN1 / SW_THR_UP1 / SW_TRIG_DIG1 (0.55mm each), SW_VOL_DN1 / SW_VOL_UP1 (0.5mm), U_HALL1 (0.27mm from edge) are all flagged. If these are intentionally edge-mounted (buttons proud of the PCB edge for a controller form factor), suppress these findings — the overhang is design intent. Otherwise, pull them inboard.

### MED-003 · SPI_SCK reference plane coverage 84%, 80mm of routing (GP-001, RP-001)

SPI_SCK runs 80mm with 16% of that length crossing a reference plane gap. At 10MHz SPI, this creates a loop antenna at ~62.5MHz harmonics. The return current detour between In1.Cu (3V3) and In2.Cu (GND) at via transitions is the likely cause — add stitching vias (one GND via and one 3V3 via) near each layer transition on SPI_SCK to close the return path.

Similarly: XBEE_RESET has only 25% coverage over 2.6mm — the shortest but worst-ratio trace. This is a very short trace so the absolute radiated risk is low; worth fixing while rerouting if easy.

### MED-004 · No test points (TE-001)

0% net coverage. Before production, add test points on at minimum: GND, 3V3, VSYS, VBAT, VBUS, MCU_EN, USB_DP_MCU/USB_DM_MCU, SPI_SCK/MOSI/MISO, and I2C_SDA/SCL.

### MED-005 · BQ25185 TS/MR pin (PU-001 — likely false positive)

The analyzer flags TS/MR (CHG_TS) for a missing pull-up. R_TS1 = 10kΩ from CHG_TS to GND is present. On the BQ25185, pulling TS to GND disables the NTC thermistor function — whether this is intentional depends on your thermal design. If you don't have a battery NTC, this is fine. If you do want temperature protection, replace R_TS1 with a 10kΩ NTC to GND and add the VSET-based bias resistor per the BQ25185 datasheet.

---

## Power Tree

```
VBUS (USB-C J_USB1)
  └─ BQ25185 (U_CHG1) → VSYS
       │   R_ISET1=600Ω → ~1A charge current (formula-estimated, no datasheet)
       │   R_VSET1=18kΩ → charge voltage TBD (no datasheet)
       │   STAT1 → 10kΩ pull-up to 3V3 → MCU GPIO1
       │   STAT2 → NO_CONNECT (floating open-drain)
       └─ VBAT ← BT1 (battery)

VSYS (battery or USB SYS output)
  ├─ Power latch: SW_PWR1 → D_OR1 → PWR_LATCH_G → Q_LATCH1 (2N7002)
  │              MCU GPIO44 (PWR_HOLD) → D_OR2 → PWR_LATCH_G
  │              Q_LATCH1.D → PWR_GATE → Q_PWR1 (DMG2305UX, PMOS)
  │              Q_PWR1 → PWR_EN → U_BUCK1.EN
  │
  └─ U_BUCK1 (TLV62569) → 3V3   ← ⚠ see BUG-001
       R_FB1=100kΩ / R_FB2=56.2kΩ → Vout=1.668V (WRONG; needs 255kΩ for 3.3V)
       L=2.2µH, f=500kHz
       Cout: 22µF + 3×10µF + 5×100nF + 47pF (effective ≈ 51µF derated)
       Load estimate: ESP32-S3 (240mA) + XBee3 (10mA) + DRV5055A2 (5mA) + OLED (10mA) = 265mA
```

---

## Subcircuits & Signal Analysis

**Buck converter** (detect: deterministic, Vref heuristic): correctly identified TLV62569 topology, SW pin, FB divider. Vout estimate of 1.668V is correct given the resistor values — this is a bug, not an analyzer error.

**VSYS battery sense divider**: R_VSYS1=200kΩ / R_VSYS2=100kΩ → VSYS_SENSE = VSYS/3, read by GPIO7/ADC1_CH6. Correct topology for ADC battery monitoring.

**XBee reset circuit**: R_XBEE_RST1 from 3V3 → XBEE_RST_PU, R_XBEE_RC1 from XBEE_RST_PU → XBEE_RESET (XBee RESET pin). RC time constant ≈ 1/159Hz = 6.3ms. PU-001 pull-up warning is a **false positive** — the pull-up is present through R_XBEE_RST1.

**RC filters**: R_XBEE_RC1/C_XBEE_RC1 at 15.9kHz (XBee SPI bus filter), R_XBEE_RST1/C_XBEE_RC1 at 159Hz (reset debounce). Both look intentional.

**USB-C CC**: R_CC1=5.1kΩ, R_CC2=5.1kΩ both to GND — correct for a USB-C sink device.

**USB data path**: J_USB1 D± → 27Ω series resistors (R_USB_DP1/DM1) → MCU GPIO19/20 (native USB PHY). Correct for USB FS.

**Power latch circuit**: D_OR1 (PWR_BTN_SENSE → PWR_LATCH_G) and D_OR2 (PWR_HOLD → PWR_LATCH_G) OR the gate of Q_LATCH1 (N-MOSFET), which pulls the gate of Q_PWR1 (P-MOSFET) low, enabling VSYS → PWR_EN → buck EN. Circuit is logical and clean.

**⚠ Firmware note:** During boot, GPIO44 (PWR_HOLD) defaults to a high-impedance input. The system stays powered only while the button is held (D_OR1 conducting). The firmware must assert PWR_HOLD high early in `app_main()` — before the RTOS scheduler yields — or the board will power off when the user releases the button.

**⚠ GPIO44 = U0RXD conflict:** During serial flashing the ESP32-S3 uses GPIO44 as UART0 RX. If PWR_HOLD drives this pin high-output during normal operation, it will conflict with the programmer's TX line. Handle with care in GPIO init order (configure as output only after the UART boot window closes).

**Hall trigger**: DRV5055A2 (±16mT), powered from 3V3, output to TRIG_ANALOG → MCU GPIO4/ADC1_CH3. Looks correct.

**Thumbstick**: GuliKit_HallStick J_STICK1 — X_LO/X_HI and Y_LO/Y_HI powered from GND and 3V3 respectively. X_OUT and Y_OUT to ADC (GPIO5, GPIO6). Button to GPIO21. Looks correct.

---

## PCB Layout

| Metric | Value | Status |
|---|---|---|
| Board | 35 × 99.9mm, 4-layer | OK |
| DFM tier | Standard (min track 0.2mm, min drill 0.3mm) | OK |
| DRC violations | 0 | OK |
| Routing | 100% complete | OK |
| Fiducials F.Cu | 0 | **FAIL** |
| Fiducials B.Cu | 0 | **FAIL** |
| Test points | 0% coverage | Fail |
| Via in pad | U_MCU1:4 untented | Fix |
| Edge clearance | 7 components at/past edge | Review |
| XBee courtyard overlaps | 7 components | Investigate |

**Stackup note:** The SU-001 EMC findings ("adjacent signal layers F.Cu/In1.Cu, In1.Cu/In2.Cu, In2.Cu/B.Cu") are **false positives**. In1.Cu is a 3V3 copper pour (83% fill) and In2.Cu is a GND copper pour (85% fill). The stackup is standard 4-layer: signal / power / ground / signal. KiCad reports both inner layers as "signal" type, which confuses the EMC analyzer.

---

## EMC Assessment

With a 4-layer stackup where both inner layers are reference planes, the board is reasonable for a handheld consumer device. Key risks:

| Finding | Severity | Notes |
|---|---|---|
| SPI_SCK 84% plane coverage, 80mm routing | Medium | Add stitching vias near layer transitions |
| XBEE_RESET 25% plane coverage, 2.6mm | Medium | Short trace; tolerable if signals are slow |
| SPI_SCK on outer layer, near J_USB1 | Low | Add keepaway or route away from connector |
| U_BUCK1 harmonics in 30–88MHz | Low | Inherent to 500kHz switching; add input ferrite if needed |
| No EMC filtering on J_USB1 | Low | No common-mode choke; acceptable for a non-CE/FCC device |
| 3V3 PDN anti-resonance at 955MHz | Info | Add one 39pF MLCC near U_MCU1 VCC pins |

SPICE simulation was skipped — no simulator installed (ngspice/LTspice/Xyce).

---

## Thermal

Analyzer found **0 thermal findings**. No component is estimated to exceed safe junction temperature. No thermal vias are flagged as insufficient.

---

## Schematic Quality

| Finding | Action |
|---|---|
| RS-001: VBAT, VBUS, PWR_* have no declared source | Add `PWR_FLAG` symbols to each of these rails so ERC/analyzer can verify sourcing |
| BQ25185 STAT2 → NO_CONNECT | Add DNP pull-up 4.7kΩ to 3V3 for cleanliness |
| TS/MR at GND via R_TS1=10kΩ | Intentional (NTC disabled); document in schematic |

---

## Positive Findings

- Power latch topology is clean and correct (two-diode OR, N+P MOSFET pair)
- CC1/CC2 pull-downs (5.1kΩ) correctly identify device as USB-C sink
- USB series resistors 27Ω appropriate for ESP32-S3 FS USB
- VSYS/3 ADC divider for battery monitoring is correct
- XBee SPI interface properly using hardware SPI pins (GPIO10–13)
- I2C for OLED on GPIO8/9 with proper bus labeling
- Buck input/output capacitance well-provisioned (22µF output, 10µF input)
- DFM: no spacing violations, standard tier compatible

---

## Skipped Analyses

| Analysis | Reason |
|---|---|
| Datasheet verification | No MPNs, no datasheets on disk |
| SPICE simulation | No ngspice/LTspice/Xyce installed |
| Gerber verification | No gerber files in project directory |
| Lifecycle audit | No MPNs to query |
| Prior review delta | First review of this project |

---

## Priority Action List

| # | Action | Severity |
|---|---|---|
| 1 | **Change R_FB1 from 100kΩ to 255kΩ** (3.3V output) | CRITICAL |
| 2 | Add MPNs to all 68 remaining components | BLOCKER |
| 3 | Add TVS/ESD on VBUS | HIGH |
| 4 | Tent via in U_MCU1 pad 4, or move via off-pad | HIGH |
| 5 | Add 3 fiducials per SMD face | HIGH |
| 6 | Move SW_BOOT1 inboard by ≥0.3mm | HIGH |
| 7 | Investigate XBee courtyard size; resolve overlaps | MEDIUM |
| 8 | Decide on edge switches (intentional overhang → suppress; else move) | MEDIUM |
| 9 | Add stitching vias near SPI_SCK layer transitions | MEDIUM |
| 10 | Add test points on power rails, USB, SPI, I2C | MEDIUM |
| 11 | Assert PWR_HOLD high early in firmware `app_main()` | FIRMWARE |
| 12 | Add PWR_FLAG symbols to VBAT, VBUS, PWR_* rails | LOW |
| 13 | Add 39pF MLCC near MCU VCC pins for 955MHz PDN gap | INFO |
