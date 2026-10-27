# Snips shell: print-ready files

Every file is already in its print orientation (Z up, lowest point on Z=0, centred on XY). Don't rotate anything.
Designed for a **0.4 mm nozzle**, ~0.2 mm layers.

| Part | Orientation | Supports |
|---|---|---|
| TopShell | seam rim on the bed (as modelled) | slicer tree/organic supports for the roof (interior scarring OK) |
| BottomShell | flipped, rim on the bed, belly up | slicer supports for the belly (interior scarring OK). The alignment lip now stops 1.2 mm above the bed plane, so it needs support under its top edge too (supports everywhere, not just touching build plate) |
| BatteryPanel | flat, belly (outer face) on the bed, cradle up; the snap arms flex in the bed plane | none needed (the front hook tabs are short; add a little support under them if your slicer complains) |
| BumperCap | flat front face on the bed (face down) | slicer supports, small: about 90 mm² of overhang under the rear cross-bar and the rod tie plates (`BumperCap.stl`) |
| Trigger | on its side, blade face down | **built in** (`Trigger_with_supports.stl`) |
| TriggerPost | D flat on the bed, post lying along X | none |
| HallBridge | flat, as modelled | none |
| PowerPlate | flange (back face, with the small switch recess) on the bed, diagonal stem standing up, rounded front end on top | none needed (the stem leans about 29 deg from vertical, under the 45 deg limit) |
| SidePlateL1/L2/R1/R2 | D-shaped caps, flat base on the bed, D stem pointing up | none |
| FaceCap1-6 | stem down, as modelled | none |

## Built-in breakaway supports
- Tapered pillars: 1.0 mm base, 0.6 mm tip (0.7/0.5 mm in tight spots), ending 0.2 mm below the part so they snap off.
- Spaced 2 mm apart, covering every overhang over 45 deg and 0.6 mm tall.
- **Turn slicer supports OFF for the Trigger**, or the slicer will add its own on top.
- The supports are STL only. The STEP files are the bare parts, oriented.
- They are NOT in the Fusion design. `tools/` regenerates them from the STLs
  (`export_print.py`, with `step_orient.py` for the STEP rotation) if a part changes.
  Needs numpy, scipy, trimesh, rtree and build123d.
- If they're too hard to remove, raise `GAP` in `build_sup.py` to 0.25-0.3. If they're too weak, raise `TIP_W` to 0.8.
