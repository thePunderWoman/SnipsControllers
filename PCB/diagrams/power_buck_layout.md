# Where every part in the buck stage wants to sit

*PCB layout reference · synchronous buck*

U_BUCK1 (TLV62569DBV) switches at ~2–3 MHz and delivers up to 2A — placement determines whether it's quiet or noisy. This maps the schematic to a suggested top-copper floorplan you can model your PCB after.

**Source:** `PCB/power_buck.kicad_sch` &nbsp;·&nbsp; **Nets:** VSYS → 3V3 &nbsp;·&nbsp; **Package:** SOT-23-5

## What's on this sheet

Six parts, one power stage. Values as drawn in the schematic.

| Ref | Value | Footprint | Role |
|---|---|---|---|
| U_BUCK1 | TLV62569DBV | SOT-23-5 | Synchronous buck IC, 2A, adjustable 0.6–5.5V out |
| C_BUCK_IN1 | 10µF | 0805 | Input bulk/decoupling — absorbs the IC's pulsed input current |
| L_BUCK1 | 2.2µH | Bourns SRN4018 | Power inductor |
| C_BUCK_OUT1 | 22µF | 0805 | Output filter cap |
| R_FB1 | 100kΩ | 0402 | Feedback divider, VOUT side |
| R_FB2 | 56.2kΩ | 0402 | Feedback divider, GND side |

## Suggested floorplan

Top copper layer, drawn roughly to scale. Trace weight signals current, not just wire — the input loop and switch node get thick, short, direct copper; the feedback path gets thin copper routed the long way around to stay quiet.

![Top-down PCB floorplan for the TLV62569 buck stage, showing the input capacitor and IC placed in a tight loop, the inductor placed directly off the switch pin, the output capacitor next to the inductor, and the feedback divider routed on a separate quiet path back to the IC's FB pin, away from the switch-node copper.](power_buck_layout.svg)

*Top-layer placement and routing for U_BUCK1, sized roughly to real footprint proportions. **Gold copper** carries the input, switch, and output power path; **pale blue** carries the low-current FB and EN signals; rings mark vias dropping straight to the ground plane. Board edges shown are the crop of this local zone, not the full board.*

- 🟨 Power copper (VSYS, SW, VOUT)
- 🟦 Feedback / enable signal
- ⚪ Via to GND plane

## Why it's arranged this way

| | |
|---|---|
| **A** | **Input loop stays tiny.** C_BUCK_IN1 sits directly on top of VIN, with its ground pad dropping straight to a via. VIN, the cap, and the return path form the loop the IC's fast input-current pulses actually flow through — keep its area small and this is one of the highest-leverage placements on the board for both efficiency and radiated noise. |
| **B** | **SW node is short and alone.** The trace from pin 3 to L_BUCK1 carries the full switched current at the IC's switching frequency and acts as an antenna if it's long or fat. Keep it the shortest, most direct copper on the board and don't route anything else near or under it. |
| **C** | **Feedback taps the output cap, not the inductor.** R_FB1 picks up VOUT right at C_BUCK_OUT1's pad — the point that's actually regulated — instead of further upstream where switching ripple is still settling. |
| **D** | **Every return pad gets its own via.** C_BUCK_IN1, U_BUCK1, C_BUCK_OUT1, and R_FB2 each drop to the ground plane independently rather than daisy-chaining through each other on the top layer, so switching return current can't inject noise into the quiet feedback ground. |
| **E** | **FB takes the long way home.** Once through the divider, the FB signal is thin and high-impedance — a few millivolts of coupled switching noise here shows up directly as output ripple. It's routed around the left side of the IC, clear of the SW/L1 copper, even though that's a longer path. |

## Routing order

If you're placing copper by hand, do it roughly in this order — later steps have less freedom once earlier ones are locked in.

1. Pour ground on the bottom layer first; treat everything else as carving space out of it.
2. Place U_BUCK1, then butt C_BUCK_IN1 against its VIN pin before anything else claims that space.
3. Route SW to L_BUCK1 — shortest path wins, width sized for 2A.
4. Place C_BUCK_OUT1 off the inductor's far pad, then fan VOUT out to the load.
5. Tap FB at C_BUCK_OUT1, place R_FB1/R_FB2 close together, route the sense line back around the IC last — by now you know exactly what copper it has to avoid.
6. Stitch a via at every ground pad (C_IN, IC, C_OUT, R_FB2).

> **Worth double-checking:** R_FB1/R_FB2 (100kΩ / 56.2kΩ) work out to roughly **1.67V** at the TLV62569's 0.6V typical reference (V<sub>OUT</sub> = 0.6 × (1 + R_FB1⁄R_FB2)), not the 3.3V implied by the "3V3" net label on that rail. Worth confirming which one is intended before you commit to this floorplan — it doesn't change the layout, but it's a five-minute check now versus a respin later.

---
*Generated from `PCB/power_buck.kicad_sch` — a placement reference, not a manufacturing drawing. Model your actual footprints and DRC against your fab's rules. A richer standalone version with the full interactive design lives in [power_buck_layout.html](power_buck_layout.html).*
