# Half of this circuit is electrical, half is where your thumb goes

*PCB layout reference · soft-latch power switch*

Q_PWR1 carries the entire board's current — every milliamp the ESP32, XBee, OLED, and everything else draws passes through one SOT-23 PMOS. But the layout-driving constraint here isn't electrical at all: SW_PWR1 is a physical button that has to land under a hole in your enclosure.

**Source:** `PCB/power_control.kicad_sch` &nbsp;·&nbsp; **Nets:** VSYS → PWR_EN &nbsp;·&nbsp; **Package:** SOT-23 ×3, SOD-123 ×2, SOT-323 ×1

## What's on this sheet

A press-to-start, software-held latch: the button turns the board on, the MCU has to grab the hold line before the button is released, and either side of a diode-OR can keep power on. A small transistor inverter (R_INV_BASE1 / Q_INV1 / R_INV_PU1) sits between the button and that diode-OR — see callout F for why it can't just tap the button's sense line directly, and callout D for why `R_LATCH_G1` is 1MΩ rather than 10kΩ.

| Ref | Value | Footprint | Role |
|---|---|---|---|
| Q_PWR1 | AO3401A | SOT-23 | Main power switch — VSYS to PWR_EN, carries full system current |
| R_PWR_EN_PD1 | 100kΩ | 0402 | `PWR_EN` pull-down to GND — the buck's EN pin has no internal bias and must not be left floating (callout G) |
| Q_LATCH1 | AO3400A | SOT-23 | Pulls Q_PWR1's gate low to turn it on; low-Vth (≤1.45V) so a press still turns it on at the bottom of the battery range (callout D) |
| D_OR1 / D_OR2 | 1N4148W | SOD-123 | Diode-OR: `PWR_BTN_TRIGGER` (actual button press, via the inverter) or MCU hold, either one latches power on |
| R_GATE1 | 100kΩ | 0402 | Q_PWR1 gate pull-up — off by default |
| R_LATCH_G1 | 1MΩ | 0402 | Q_LATCH1 gate pull-down — off by default; sized to not starve the diode-OR's drive (callout D) |
| C_LATCH_G1 | 100nF | 0402 | Latch-gate filter — keeps the ~2ms battery-insertion transient from lifting `PWR_LATCH_G` (callout I) |
| R_BTN_PU1 | 100kΩ | 0402 | Button sense pull-up to **3V3** (not VSYS) — MCU reads pressed (0V) vs idle (3.3V); callout H |
| R_INV_BASE1 | 100kΩ | 0402 | Dedicated VSYS pull-up for Q_INV1's base — decoupled from `PWR_BTN_SENSE` (callout F) |
| D_INV_ISO1 | BAT54W-7-F | SOT-323 | Schottky isolator: lets a press pull Q_INV1's base low without loading `PWR_BTN_SENSE` |
| Q_INV1 | MMBT3904 | SOT-23 | Inverter: idle (button up) holds `PWR_BTN_TRIGGER` low; pressed lets `R_INV_PU1` pull it high |
| R_INV_PU1 | 100kΩ | 0402 | `PWR_BTN_TRIGGER` pull-up to VSYS — the inverter's collector load |
| C_DBNC1 | 100nF | 0402 | Button debounce, right at the switch |
| SW_PWR1 | Alps SKRTLAE010 | SMD tactile | Physical power button — must reach the enclosure surface |

## Suggested floorplan

Top copper layer. The VSYS→PWR_EN path runs straight across the top as one thick lane; the latch logic hangs underneath it as a compact, low-current block; the inverter sits just below the button's sense line, on the way to the diode-OR; the button sits wherever your enclosure needs it to.

![Top-down PCB floorplan for the soft-latch power switch, showing VSYS entering the PMOS source and exiting the drain as PWR_EN in one straight lane across the top, the NMOS latch and diode-OR logic clustered in a compact low-current block beneath it, a small transistor inverter between the button's sense line and the diode-OR so only an actual press (not the button's idle level) can trigger the latch, and the physical power button placed near the board edge with its debounce capacitor right at its pins.](power_control_layout.svg)

*Top-layer placement for the soft-latch switch. **Gold copper** is the VSYS→PWR_EN power path; **pale blue** is the low-current gate-drive, sense, inverter, and control logic; rings mark vias to the ground plane.*

- 🟨 Power copper (VSYS, PWR_EN)
- 🟦 Gate-drive / sense / inverter / control
- ⚪ Via to GND plane

## Why it's arranged this way

| | |
|---|---|
| **A** | **Q_PWR1 is the one part on this sheet that isn't low-current.** Every downstream milliamp — MCU, XBee, OLED, everything — flows through its source-drain path. AO3401A's RDS(on) (<60mΩ at 4.5V gate drive, <85mΩ at 2.5V) keeps drop and heat trivial at this board's current budget, but a SOT-23 has no separate thermal pad — the source/drain copper itself is the heatsink, so give those pads real copper, not just a skinny trace. |
| **B** | **SW_PWR1's position isn't yours to optimize electrically.** It has to land under a hole in whatever enclosure this board sits in — mechanical placement drives this part, and the rest of the latch logic has to route to wherever that ends up, not the other way around. |
| **C** | **C_DBNC1 sits right on the switch's own pins.** Same principle as the charger's pushbutton guidance in its own datasheet: the debounce cap does its job by being close to the contact bounce it's filtering, not by being close to anything else. |
| **D** | **The diode-OR and its pull-down are one compact decision node — and R_LATCH_G1's value is load-bearing, not incidental.** D_OR1, D_OR2, R_LATCH_G1, and Q_LATCH1's gate all meet at PWR_LATCH_G. A press only drives this node through `R_INV_PU1` (100kΩ) and a diode drop — with the original 10kΩ pull-down, that divider left barely 0.3V at Q_LATCH1's gate, nowhere near enough to turn it on. Raised to 1MΩ, the same divider delivers ~2.4V at the bottom of the battery range (VSYS≈3.0V, modelling a real ~0.35V diode drop at µA currents) up to ~3.5V at full charge, without adding any idle current (that's set by `R_INV_PU1`, untouched). A 2N7002's worst-case threshold is 2.5V, which left no margin at the bottom, so `Q_LATCH1` is an AO3400A (Vgs(th) ≤1.45V): at least ~1V of overdrive everywhere. Keeping this cluster tight matters less for noise than for just keeping the logic legible on the board. |
| **E** | **PWR_HOLD arriving late is a hard failure, not a glitch.** Per this project's own GPIO notes, pin 36 (PWR_HOLD) must go high as the MCU's very first instruction — release the button before that happens and R_LATCH_G1's pull-down turns everything back off. Not a layout fix, but it's the reason D_OR2's path exists at all. |
| **F** | **D_OR1 can't read `PWR_BTN_SENSE` directly — found the hard way, on real hardware.** The original design had `R_BTN_PU1` (to VSYS) and a since-removed `R_PWR_SENSE1` (to GND) both on that node, dividing it to roughly half VSYS at idle — comfortably above a diode's ~0.6V forward threshold, so `D_OR1` was forward-biased (and the board powered on) the instant a battery was plugged in, button or no button. The inverter fixes this properly instead of just retuning the divider: idle, `Q_INV1` is on and holds `PWR_BTN_TRIGGER` at GND; pressed, it turns off and `R_INV_PU1` pulls `PWR_BTN_TRIGGER` to VSYS. Only a real press can forward-bias `D_OR1` now. Keep this block close to `D_OR1` — it's parts serving one signal, not worth spreading out.<br><br>**Second-order bug, found the same way:** the inverter's own base resistor originally tapped `PWR_BTN_SENSE` directly, in series with `R_BTN_PU1` — stealing enough idle base current to sag the MCU's own button-read node to ~1V instead of near-VSYS. `R_INV_BASE1` is now its own dedicated 100kΩ pull-up straight to VSYS (not sharing current with `R_BTN_PU1`), and `D_INV_ISO1` (a Schottky, chosen specifically for its lower forward drop than Q_INV1's own base-emitter junction) lets a press still pull the base low without ever loading `PWR_BTN_SENSE`. |
| **G** | **`PWR_EN` had no defined rest state — TI's own datasheet says not to do this.** `Q_PWR1`'s drain feeds the buck's EN pin directly, with nothing else on that node. TLV62569's EN pin is a bare comparator input with no internal pull ("Do not leave floating" per its datasheet) — with `Q_PWR1` off, EN was held by MOSFET/input leakage alone, no guaranteed logic level. `R_PWR_EN_PD1` (100kΩ to GND) fixes it: negligible loading when `Q_PWR1`'s tens-of-mΩ RDS(on) is driving EN high, solid GND the instant it's off. |
| **H** | **`PWR_BTN_SENSE` must never sit above the MCU's rail.** It lands directly on GPIO2 (pin 38), whose ceiling is VDD + 0.3V (~3.6V), but `R_BTN_PU1` used to pull it to VSYS — up to ~4.5V on USB, and it back-fed the unpowered 3V3 rail through the pin's clamp diode whenever the MCU was off. Since `R_INV_BASE1` gave the inverter its own VSYS pull-up (callout F), `R_BTN_PU1` no longer has to supply the inverter, so it now pulls to 3V3: idle reads 3.3V, and with the MCU off the node is simply unpowered instead of leaking into the rail. |
| **I** | **Battery insertion used to blip `PWR_EN` for ~2ms — found in simulation, since no board existed to bench-test.** When VSYS first appears, `Q_INV1` isn't conducting yet: its base current is diverted through `D_INV_ISO1` into the discharged `C_DBNC1` until that charges to ~0.4V. For that ~1–2ms `PWR_BTN_TRIGGER` follows VSYS, `PWR_LATCH_G` rises to ~VSYS−0.6V, and `Q_PWR1` briefly enables the buck. `C_LATCH_G1` (100nF against the 100kΩ pull-up) makes the gate rise slowly enough that the transient peaks at ~0.3V — well under the AO3400A's minimum threshold — at the cost of 4–8ms of press latency, and it gives release a little extra hold time. Keep it right at `PWR_LATCH_G`. |

## Routing priority

The button's position is fixed by the enclosure before you start — everything else routes around it.

1. Place SW_PWR1 wherever the enclosure cutout requires; that position is a constraint, not a choice.
2. Put C_DBNC1 directly on SW_PWR1's sense pin before routing anything else nearby.
3. Route the VSYS→Q_PWR1→PWR_EN lane as one straight, generously-wide path across the board.
4. Cluster Q_LATCH1, R_GATE1, R_LATCH_G1, D_OR1, and D_OR2 into one compact low-current block under the main lane.
5. Keep R_INV_BASE1, D_INV_ISO1, Q_INV1, and R_INV_PU1 together as their own small block, close to D_OR1 — they're a single-purpose inverter, not independent parts.
6. Bring PWR_BTN_SENSE and PWR_HOLD in from wherever the MCU sheet actually sits — these are off-sheet nets, not local components.
7. Drop `R_PWR_EN_PD1` right at Q_PWR1's drain, on the way to `PWR_EN` leaving the sheet — it's a bias resistor for that pin, not an independent part.
8. Put `C_LATCH_G1` on the `PWR_LATCH_G` node next to `R_LATCH_G1` and `Q_LATCH1`'s gate, with a short GND return.

> **Checked, not just assumed:** AO3401A is rated for 4A continuous with <60mΩ RDS(on) at 4.5V gate drive in this SOT-23 package — comfortably oversized for a handheld controller's sub-2A system budget, so this isn't a thermal risk the way the charger's WSON is. MMBT3904 is a general-purpose small-signal part switching microamps here (just enough to bias a diode) — nowhere near its ratings, no thermal consideration needed. BAT54W-7-F's job is purely to isolate two nodes at sub-1V forward drop, also nowhere near its ratings. Connectivity alone (ERC, netlist, DRC) doesn't catch bad DC bias points — the `R_LATCH_G1`/`PWR_BTN_SENSE` loading bugs in callouts D and F, and the floating `PWR_EN` in callout G, all passed every connectivity check and were only found by actually working through the bias/divider math or reading the driven IC's own datasheet, so don't treat a clean ERC/DRC as proof a gate-drive, sense, or enable network will behave.

---
*Generated from `PCB/power_control.kicad_sch` — a placement reference, not a manufacturing drawing. Model your actual footprints and DRC against your fab's rules. A richer standalone version with the full interactive design lives in [power_control_layout.html](power_control_layout.html).*
