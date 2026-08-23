# PCB Layout Diagrams

Suggested top-copper floorplans for the sheets in this project with real
layout stakes — tight switching loops, thermal pads, or current-carrying
parts — derived from the schematic plus the actual component datasheets,
not guessed. Model your KiCad layout after these rather than placing
parts from scratch.

## Diagrams

| Diagram | Sheet | Why it needed one |
|---|---|---|
| [Buck converter](power_buck_layout.md) | `power_buck.kicad_sch` | Switching regulator — tight input loop, short switch node, feedback routed clear of noise |
| [Charger](power_charger_layout.md) | `power_charger.kicad_sch` | Linear charger in a WSON-10 — thermal pad via array is the whole story |
| [Power latch](power_control_layout.md) | `power_control.kicad_sch` | One transistor carries the entire board's current; the power button's position is a mechanical constraint, not an electrical one |
