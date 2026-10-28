# Snips shell: print-ready files

Every file is already in its print orientation (Z up, lowest point on Z=0, centred on XY). Don't rotate anything.
Designed for a **0.4 mm nozzle**, ~0.2 mm layers.

| Part | Orientation | Supports |
|---|---|---|
| TopShell | seam rim on the bed (as modelled), one piece (no split) | slicer supports for the roof, or use `TopShell_with_buck.stl` (see Support blocks) |
| BottomShell | flipped, rim on the bed, belly up | slicer supports for the belly (interior scarring OK). The alignment lip now stops 1.2 mm above the bed plane, so it needs support under its top edge too (supports everywhere, not just touching build plate) |
| BatteryPanel | flat, belly (outer face) on the bed, cradle up; the snap arms flex in the bed plane | none needed (the front hook tabs are short; add a little support under them if your slicer complains) |
| BumperCap | flat front face on the bed (face down) | slicer supports, small: under the side flange ties (about 2.4 mm wide overhang each side). New simple design: a flat button (34 x 6.7 mm) that goes through the window from INSIDE before the PCB, with a flange on each side behind the front wall and a 4.5 mm post on each flange |
| Trigger | on its side, the +X arm face down, spring ear (the little block with the wire channel) up | none needed (`Trigger.stl`). The ear has a 45 deg gusset, and the only overhangs left are tiny bridges inside the 0.6 mm wire channel (about 6 mm2) |
| TriggerPost | D flat on the bed, post lying along X | none |
| HallBridge | flat, as modelled | none |
| PowerPlate | flange (back face, with the small switch recess) on the bed, diagonal stem standing up, rounded front end on top | none needed (the stem leans about 29 deg from vertical, under the 45 deg limit) |
| SidePlateL1/L2/R1/R2 | D-shaped caps, flat base on the bed, D stem pointing up | none |
| FaceCap1-6 | stem down, as modelled. The stem is now the full flange width (6 x 6 mm) down to the bed, so there is no flange overhang | none |

## Support blocks (TopShell, BottomShell)
`TopShell_with_buck.stl` and `BottomShell_with_buck.stl` include a few plain rectangular blocks in the big open voids, already in the right place (separate lumps in the same STL, sitting on the bed). `TopShell.stl` / `BottomShell.stl` are the bare shells (default).
- TopShell: 2 blocks (about 13.5 and 3.6 cm3; the one out front under the OLED housing was dropped, `--max-y 50`). BottomShell: 2 side blocks (about 4 cm3 each).
- Each block keeps 0.3 mm or more from the part on every side, and its flat top is lifted to about 0.2 mm under the lowest roof point above it. The sloped parts of the roof, and everything else, still use normal slicer supports.
- In OrcaSlicer load the STL and answer yes to "split into parts". Give the block parts 1 wall and about 5% infill, no supports.
- Regenerate with `tools/gen_buck.py <bare print stl> <out stl>` (needs numpy, scipy, scikit-image, trimesh, rtree).

## Built-in breakaway supports
- Tapered pillars: 1.0 mm base, 0.6 mm tip (0.7/0.5 mm in tight spots), ending 0.2 mm below the part so they snap off.
- Spaced 2 mm apart, covering every overhang over 45 deg and 0.6 mm tall.
- The Trigger no longer needs supports (it did before the 45 deg ear gusset). Leave slicer supports off for it, or limit them to the wire channel.
- The supports are STL only. The STEP files are the bare parts, oriented.
- They are NOT in the Fusion design. `tools/` regenerates them from the STLs
  (`export_print.py`, with `step_orient.py` for the STEP rotation) if a part changes.
  Needs numpy, scipy, trimesh, rtree and build123d.
- If they're too hard to remove, raise `GAP` in `build_sup.py` to 0.25-0.3. If they're too weak, raise `TIP_W` to 0.8.

## Trigger torsion spring (not printed)
The trigger return spring is a small steel torsion spring: 0.35 mm wire, coil ID 3.65 mm / OD 4.35 mm, 2 turns, legs 6.5 mm straight plus a 1.5 mm 90 degree hook on each end, about 180 degrees between the legs at rest. It sits on the printed D-post at x = -11.1 mm (right-handed coil; the moving leg is on the -x side).
- **Moving leg** goes in the channel and hook pocket in the Trigger. **Fixed leg** lies in the groove under the HallBridge. The BottomShell has a ring relief for the coil and clearance for the legs.
- Channels are 0.6 mm wide (0.25 mm clearance on the wire). If they print too tight for the wire or the hook, drill or ream them slightly instead of changing the model.
- The pull rotates the moving leg toward the fixed leg and winds the coil tighter. Expect roughly 0.5 N at a 30 mm lever at rest (an estimate; tune on the bench).
- `../reference/` has a non-printing `SpringRef` body (coil envelope, legs and hooks) in the same coordinates as the other parts, for checking fit in a viewer.
- Details and checks: `claude/trigger-torsion-spring.md` in the Snips project.

## PCB / shell screws (M2 x 12)
Four M2 self-tapping screws, all the same length: **M2 x 12 mm** (pan or button head, head under 4.4 mm). They go up from the belly through the PCB holes into the top-shell posts, about 4 mm of thread. Each hole has a 4.6 mm head well in a 7 mm tube; the head seat is at z -4.3 (front wells about 8.5 mm deep, rear about 12.4 mm). Use a driver with at least 13 mm of reach. The 2.4 mm clearance hole continues up through the seat and the standoff.
