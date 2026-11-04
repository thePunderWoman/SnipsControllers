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

## Trigger notes
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
