# The charger's whole story is thermal, not noise

*PCB layout reference · linear Li-ion charger*

U_CHG1 (BQ25185) is a linear charger — no inductor, no switch node. The layout problem isn't EMI, it's getting heat out of a 2×3mm WSON-10 package while keeping three separate power nets short. Values below are verified against TI's own datasheet, not estimated.

**Source:** `PCB/power_charger.kicad_sch` &nbsp;·&nbsp; **Nets:** VBUS → VSYS / VBAT &nbsp;·&nbsp; **Package:** WSON-10

## What's on this sheet

Eleven parts. Every setting resistor's effect is confirmed against the BQ25185 datasheet (SLUSF65A), not assumed.

| Ref | Value | Footprint | Role |
|---|---|---|---|
| U_CHG1 | BQ25185DLHR | WSON-10 | Linear Li-ion charger + power path, 1A max |
| C_VIN_1 / C_VIN_2 | 10µF / 100nF | 0805 / 0402 | IN decoupling — right at the VBUS pin |
| C_SYS_1 / C_SYS_2 | 10µF / 100nF | 0805 / 0402 | SYS decoupling — this feeds the whole board |
| C_BAT1 | 10µF | 0805 | BAT decoupling, right at the battery pin |
| R_ISET1 | 600Ω | 0402 | Sets ICHG ≈ 500mA (TI-tested value, table 6.5) |
| R_VSET1 | 18kΩ | 0402 | Sets 500mA input limit + 4.2V VBATREG (table 7-1) |
| R_TS1 | 10kΩ | 0402 | Disables TS/battery-NTC monitoring, per datasheet §8.3.9 |
| R_STAT1 | 10kΩ | 0402 | Open-drain STAT1 pull-up to 3V3 |
| R_STAT2 | 10kΩ | 0402 | Open-drain STAT2 pull-up to 3V3 — with STAT1, fully decodes charging/done/fault |

## Suggested floorplan

IN and the SYS/BAT pair sit on opposite edges of the package — a genuine gift from the pinout. Route VBUS straight through from one side to SYS/VBAT on the other and nothing has to double back.

![Top-down PCB floorplan for the BQ25185 linear charger, showing VBUS entering the IN pin on the right, VSYS and VBAT exiting the SYS/BAT pins on the left, decoupling capacitors placed directly at each of those three pins, the ISET/VSET/TS setting resistors clustered tightly on the analog side, STAT1 and STAT2 pull-ups routed out to the MCU on opposite sides of the package, and a via array under the center thermal pad tying it to the ground plane.](power_charger_layout.svg)

*Top-layer placement for U_CHG1. **Gold copper** carries VBUS/VSYS/VBAT power; **pale blue** carries the low-current setting and status signals; rings mark vias to the ground plane. IN sits on the right, SYS/BAT on the left — route the power path straight through rather than doubling back.*

- 🟨 Power copper (VBUS, VSYS, VBAT)
- 🟦 Setting / status signal
- ⚪ Via to GND plane

## Why it's arranged this way

| | |
|---|---|
| **A** | **The thermal pad is the real story here.** This is a linear charger — every volt it drops between IN/SYS/BAT and whatever it's not dropping across the battery becomes heat inside a 2×3mm package. TI's WSON layout guidance calls for 4–6 vias under the pad at ≤0.3mm diameter, ~1mm spacing; the stock footprint in this library only places 2. Add the rest before fab. |
| **B** | **IN, SYS, and BAT each get their own decoupling, right at their own pin.** TI's datasheet layout guidelines (§8.4.1) say it plainly: place the IN, SYS, and BAT capacitors as close as possible to the device — these are three separate short loops, not one shared input loop like a switcher would have. |
| **C** | **VBUS in on the right, VSYS out on the left — no doubling back.** The package puts IN on one edge and SYS/BAT on the opposite edge. That's not a layout choice, it's a gift from the pinout: route the power path straight through and every trace is as short as it can be. |
| **D** | **ISET especially wants a short, direct trace.** R_ISET1 sets the actual charge current (ICHG = K<sub>ISET</sub>/R<sub>ISET</sub>, K≈300AΩ) — at 600Ω that's ≈500mA, matching TI's own tested value for this exact resistance. Stray resistance or noise pickup on this trace shows up directly as charge-current error. |
| **E** | **GND pin and thermal pad share one solid pour.** TI's guideline is explicit: "a solid ground plane tied to the GND pin and thermal pad should be used" — not two separate ground fills stitched together, one plane doing both jobs. |
| — | **STAT1 and STAT2 exit on opposite sides of the package, because that's where their pins are.** STAT1 (pin 9) is right next to IN on the right; STAT2 (pin 3) is on the left, next to SYS/BAT. Individually they're each still a short, direct pull-up-to-pin trace — the split just reflects the pinout, not a routing choice. Together they let firmware fully decode charging vs. done vs. fault instead of just charging-vs-not. |

## Routing priority

Do it roughly in this order — the thermal pad and its via array need the most freedom, so give them first pick of the copper underneath the part.

1. Place U_CHG1, then lay out the thermal-pad via array before anything else claims that space (aim for 4–6 vias, not the footprint's stock 2).
2. Butt C_VIN_1/C_VIN_2 against the IN pin, C_SYS_1/C_SYS_2 against SYS, and C_BAT1 against BAT — three independent short loops.
3. Route VBUS in from one edge and VSYS/VBAT out the opposite edge, straight through.
4. Cluster R_ISET1, R_VSET1, R_TS1, and R_STAT2 tight against their pins on the analog side — keep them clear of the power copper.
5. Pour ground last, tying the GND pin, the thermal pad array, and every decoupling cap's return into one plane.

> **Verified against TI datasheet SLUSF65A:** R_ISET1 (600Ω) → ICHG ≈ 500mA is one of TI's own named test conditions, not an estimate. R_VSET1 (18kΩ) maps to Table 7-1 exactly: 500mA input current limit, 4.2V battery regulation, 3.0V precharge threshold — standard single-cell Li-ion settings. R_TS1 (10kΩ) matches TI's own instruction verbatim: "if the TS function is not required, connect a 10kΩ resistor from the TS/MR pin to GND." Nothing on this sheet needed correcting.

---
*Generated from `PCB/power_charger.kicad_sch` — a placement reference, not a manufacturing drawing. Model your actual footprints and DRC against your fab's rules. A richer standalone version with the full interactive design lives in [power_charger_layout.html](power_charger_layout.html).*
