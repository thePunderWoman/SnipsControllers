# Snips shell v2: print-ready files

Every file is already in its print orientation (lowest point on Z=0, centred on XY). Don't rotate anything.
Designed for a **0.4 mm nozzle**, ~0.2 mm layers. No breakaway supports or support bucks are generated for v2:
use slicer supports where noted. Source of truth: `../../fusion/SnipsShell_v2.f3d` (rebuild library `lib_v2.py`).

| Part | Orientation | Supports |
|---|---|---|
| TopShell | seam rim on the bed (as modelled) | slicer supports for the roof (face buttons, thumbstick bump) |
| BottomShell | belly down, open side up (as modelled) | **yes, under the trigger scoop**: the scoop slopes 33/35/21 deg above horizontal (57/55/69 deg overhang), so supports are needed there. Add a **brim**: the bottom ~1.8 mm of the 6 mm belly fillet is steeper than 45 deg. Everything else (rib pillar, spring groove, post sockets) prints without supports |
| BottomShell_Pouch | belly down, open side up (as modelled) | same as BottomShell: slicer supports under the trigger scoop plus a brim for the belly fillet (expected, not yet test-printed). 30.3 mm tall instead of 33.7 mm. Uses `TriggerPost`/`TriggerCaps`; no BatteryPanel |
| BatteryPanel | outer face on the bed | none (short hook tabs; add a little support if your slicer complains) |
| Bumper | front face on the bed | none expected |
| PowerButton | flange on the bed, stem up | none |
| FaceButton1-6 | flange on the bed, stem and cap up | none |
| SideButtonL1/L2, R1/R2 | flat base on the bed, D stem up | none |
| Trigger | big flat blade face on the bed, spring ear up | none expected |
| Trigger_Pouch | big flat blade face on the bed, spring ear up | none expected. Pouch variant only (use `Trigger` with `BottomShell`) |
| TriggerPost | D flat on the bed, post lying along X | none |
| TriggerCaps | two thin plates (0.45 mm left, 0.6 mm right) laid flat side by side, 4 mm apart, in one STL/STEP | none. They are loose parts: slide them into the sockets after the post and PCB are in; the 0.1 mm nubs give a friction fit |

## Variants
Standard build: `BottomShell` + `Trigger` + `BatteryPanel` (14500 cell). Pouch build: `BottomShell_Pouch` + `Trigger_Pouch` (YDL103035
pouch cell, no BatteryPanel). TopShell, buttons, Bumper, TriggerPost and TriggerCaps are shared. Overall size: 35.0 x 80.65 mm,
33.70 mm tall (standard) vs 30.32 mm (pouch), measured flat top surface to flat bottom.

## Wrist strap anchor
`BottomShell` and `BottomShell_Pouch` each have two vertical 1.5 x 3.4 mm slots through the rear-left corner (2.2 mm bar between
them) for a 3 x 1 mm strap loop. Slots are narrow and tall with rounded ends, so they print without supports. Parameters:
`strap_loop_w`, `strap_loop_t`, `strap_clr`, `strap_bar_w`, `strap_x`, `strap_z`, `strap_cut_y`.

## 14500 bay ceiling and markings (BottomShell only)
- 0.7 mm printed ceiling over the cell at the rear (positive) end and the front (negative) end of the bay, open under the XBee module,
  radial clearance to the cell unchanged (`bay_ceil_*` parameters). Printed belly down the new part is a domed roof over the cell trench: the surface is steeper than 45 deg up to the arc tips, then the last ~10.6 mm (between the arc tips) curves over to horizontal and has to bridge. Expect it to print with bridge settings; check the preview and add slicer supports only if the dome sags (not yet test-printed).
- Engraved + / - at the two ends of both ledges and "Li-ion 14500 only" on the right ledge (`pol_*` parameters, 0.5 mm deep).
  The ledge is a bridge when printed belly down, so the letters may print a little rough.
- The negative end is a closed square tube to the back wall (`bay_tube_*`): 1.1 mm walls, 0.7 mm roof that bridges ~17 mm across when printed belly down (not yet test-printed; use bridge settings and check the preview).

## Trigger notes
- Both triggers are filleted where a finger touches them: the long edges across the blade (nose, bottom-flat corners, rear corner) at 1.2 mm (`trig_edge_r`, only below `trig_edge_zmax`) and the outline of both side faces along the finger blade, from the rear face near the hub round the bottom and along the nose diagonal, at 1.0 mm (`trig_side_r`). 1.2 mm does not compute on the side outline, because of sliver edges at the ramp teeth and hub; the rest of the outline (ledge, ramp, hub pockets) stays sharp.
- Both triggers rest 8 deg forward of vertical (`trig_rest_deg`, `pt_rest_deg`); the pull is 28 deg beyond that.
  `Trigger` was regenerated for this, so reprint it if you printed an earlier one.
- The trigger spring slots (ear slot and fixed-leg groove) are 0.8 mm wide, parametric (`trig_leg_slot`, `trig_groove_w`). Print a short test coupon first if the 0.35 mm wire does not slide in, and ream rather than change the model.
- The rib pillar (`trig_pil_*`) supports the rib so the BottomShell needs no support there.
- Spring, post and screw details are unchanged from v1.1 (see `shell/v1.1/export/print/README.md` and `claude/trigger-torsion-spring.md` in the Snips project): 0.35 mm wire torsion spring, M2 x 12 self-tapping screws.
- `../reference/` holds non-printing parts (XBee carrier, thumbstick knob, spring, OLED, 14500 cell, contact clip, contacts block) in the same coordinates for checking fit.

## Regenerating
Run from `tools/` (they find `export/stl|step` and `print/` from their own location; override with `SNIPS_EXPORT_DIR` / `SNIPS_PRINT_DIR`):
`python export_print.py` (STL) and `python step_orient.py` (STEP). Orientations live in `export_print_cfg.py`.
Needs numpy, scipy, trimesh, rtree and build123d. (`build_sup.py`, `gen_buck.py`, `gen_supports.py` are carried over from v1.1 and not used for v2.)
