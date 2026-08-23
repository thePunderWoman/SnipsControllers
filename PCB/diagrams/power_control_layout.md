# Half of this circuit is electrical, half is where your thumb goes

*PCB layout reference · soft-latch power switch*

Q_PWR1 carries the entire board's current — every milliamp the ESP32, XBee, OLED, and everything else draws passes through one SOT-23 PMOS. But the layout-driving constraint here isn't electrical at all: SW_PWR1 is a physical button that has to land under a hole in your enclosure.

**Source:** `PCB/power_control.kicad_sch` &nbsp;·&nbsp; **Nets:** VSYS → PWR_EN &nbsp;·&nbsp; **Package:** SOT-23 ×2, SOD-123 ×2

## What's on this sheet

A press-to-start, software-held latch: the button turns the board on, the MCU has to grab the hold line before the button is released, and either side of a diode-OR can keep power on.

| Ref | Value | Footprint | Role |
|---|---|---|---|
| Q_PWR1 | DMG2305UX | SOT-23 | Main power switch — VSYS to PWR_EN, carries full system current |
| Q_LATCH1 | 2N7002 | SOT-23 | Pulls Q_PWR1's gate low to turn it on |
| D_OR1 / D_OR2 | 1N4148W | SOD-123 | Diode-OR: button press or MCU hold, either one latches power on |
| R_GATE1 | 100kΩ | 0402 | Q_PWR1 gate pull-up — off by default |
| R_LATCH_G1 | 10kΩ | 0402 | Q_LATCH1 gate pull-down — off by default |
| R_BTN_PU1 / R_PWR_SENSE1 | 100kΩ / 100kΩ | 0402 | Button sense divider — MCU reads pressed (0V) vs idle (½VSYS) |
| C_DBNC1 | 100nF | 0402 | Button debounce, right at the switch |
| SW_PWR1 | Alps SKRTLAE010 | SMD tactile | Physical power button — must reach the enclosure surface |

## Suggested floorplan

Top copper layer. The VSYS→PWR_EN path runs straight across the top as one thick lane; the latch logic hangs underneath it as a compact, low-current block; the button sits wherever your enclosure needs it to.

![Top-down PCB floorplan for the soft-latch power switch, showing VSYS entering the PMOS source and exiting the drain as PWR_EN in one straight lane across the top, the NMOS latch and diode-OR logic clustered in a compact low-current block beneath it, and the physical power button placed near the board edge with its debounce capacitor right at its pins.](power_control_layout.svg)

*Top-layer placement for the soft-latch switch. **Gold copper** is the VSYS→PWR_EN power path; **pale blue** is the low-current gate-drive, sense, and control logic; rings mark vias to the ground plane.*

- 🟨 Power copper (VSYS, PWR_EN)
- 🟦 Gate-drive / sense / control
- ⚪ Via to GND plane

## Why it's arranged this way

| | |
|---|---|
| **A** | **Q_PWR1 is the one part on this sheet that isn't low-current.** Every downstream milliamp — MCU, XBee, OLED, everything — flows through its source-drain path. DMG2305UX's RDS(on) (~35–50mΩ) keeps drop and heat trivial at this board's current budget, but a SOT-23 has no separate thermal pad — the source/drain copper itself is the heatsink, so give those pads real copper, not just a skinny trace. |
| **B** | **SW_PWR1's position isn't yours to optimize electrically.** It has to land under a hole in whatever enclosure this board sits in — mechanical placement drives this part, and the rest of the latch logic has to route to wherever that ends up, not the other way around. |
| **C** | **C_DBNC1 sits right on the switch's own pins.** Same principle as the charger's pushbutton guidance in its own datasheet: the debounce cap does its job by being close to the contact bounce it's filtering, not by being close to anything else. |
| **D** | **The diode-OR and its pull-down are one compact decision node.** D_OR1, D_OR2, R_LATCH_G1, and Q_LATCH1's gate all meet at PWR_LATCH_G — keeping that cluster tight matters less for noise than for just keeping the logic legible on the board. |
| **E** | **PWR_HOLD arriving late is a hard failure, not a glitch.** Per this project's own GPIO notes, pin 36 (PWR_HOLD) must go high as the MCU's very first instruction — release the button before that happens and R_LATCH_G1's pull-down turns everything back off. Not a layout fix, but it's the reason D_OR2's path exists at all. |

## Routing priority

The button's position is fixed by the enclosure before you start — everything else routes around it.

1. Place SW_PWR1 wherever the enclosure cutout requires; that position is a constraint, not a choice.
2. Put C_DBNC1 directly on SW_PWR1's sense pin before routing anything else nearby.
3. Route the VSYS→Q_PWR1→PWR_EN lane as one straight, generously-wide path across the board.
4. Cluster Q_LATCH1, R_GATE1, R_LATCH_G1, D_OR1, and D_OR2 into one compact low-current block under the main lane.
5. Bring PWR_BTN_SENSE and PWR_HOLD in from wherever the MCU sheet actually sits — these are off-sheet nets, not local components.

> **Checked, not just assumed:** DMG2305UX is rated for ~4A continuous with 35–50mΩ RDS(on) in this SOT-23 package — comfortably oversized for a handheld controller's sub-2A system budget, so this isn't a thermal risk the way the charger's WSON is. Nothing on this sheet needed correcting.

---
*Generated from `PCB/power_control.kicad_sch` — a placement reference, not a manufacturing drawing. Model your actual footprints and DRC against your fab's rules. A richer standalone version with the full interactive design lives in [power_control_layout.html](power_control_layout.html).*
