# Snips controller shell (right hand)

> **To print, go to [`right_hand/`](right_hand/README.md)**: parts list, print orientations, supports, hardware and assembly order.

A parametric, 3D-printable outer shell for the Snips controller, generated in Python with build123d from the PCB's STEP export.
`scripts/params.py` holds every dimension: change a number, re-run the build, and the parts regenerate. STEP files don't
carry design history, so the scripts are the history.

| Folder | What |
|---|---|
| `scripts/` | Generator, verification and render scripts |
| `right_hand/` | The current printable parts (STL), button caps and reference images |

## The design

- **Shape:** option C, "sculpted flat face", right hand, roughly 48 W x 116 L x 53 T mm. The skin's cross-sections are
  solved to clear every component by 0.8 mm (the battery by 0.4 mm), with a rounded palm-end tip. `build.py` can also
  build the two rejected options (A, B) for comparison.
- **Screen:** an Adafruit 938 (1.3" 128x64 OLED, 4.5 mm stack, no header, Qwiic cable) in a pod tilted 30 degrees
  toward the thumb.
- **Walls:** 2 mm. The cavity is a true parallel offset of the outer profile, so the end domes keep their thickness too.
- **Openings:**
  - Thumbstick: a bowl swept from the tilting cap (17.5 mm cap, +-20 degrees, 1.2 mm gap).
  - Face buttons: six 7.0 mm holes on a 3 x 2 grid.
  - Side buttons (4): a shallow stadium pocket for the cap head plus a 3.4 mm neck hole.
  - Power: an oblique neck hole aimed at the side-actuated switch.
  - USB-C: a plug-sized tunnel (12.4 x 6.6 mm) with a sleeve out to the receptacle, which sits 5 mm behind the wall.
  - LED: a 1.85 mm hole with a guide boss for a filament light pipe.
  - Wrist strap: two cord holes in a thickened pad on the back, near the palm end.
- **Two halves, no screws:** the shell splits at z = -4 mm. The seam is a flush half-lap (the top shell's skin
  overhangs the bottom's inner lip), held by 7 hidden snap clips. The PCB is clamped between the halves by posts with
  locating pins, L-shaped arms at the screen-end holes, and edge ribs. The PCB drops into the bottom shell and the top
  shell lowers over it and snaps shut.
- **Battery:** a single 14500 cell in a closed shroud under the board, between a flat (+) contact bulkhead at the palm
  end and a spring (-) contact at the screen end (54 mm face to face), with +/- engraved on each. A panel in the back
  slides out of the palm end on T-rails with detent bumps.
- **Printed moving parts:**
  - Button caps: 6 face, 4 side, and a cone-necked power cap, each retained by an inner flange.
  - Trigger: a PS5 R2-style curved hook on the end face under the bumper, glued to a two-stem frame that snaps onto a
    pin hinge. A spring returns it, and a magnet in it drives the Hall sensor.
  - Bumper: a bar across the screen end, hinged above, with a prong that presses the corner switch.
  - Both hinges use 2.4 mm printed pins that snap into fins in the shell from inside.
- **PCB change the trigger needs:** move the Hall sensor body to x = 26, y = 94 on the back (bend the leads over from
  the holes at x = 32.7, y = 96.9) so it sits over the magnet.

## Verification

`verify.py` checks 33 claims against a build and exits 1 if any fails:
- every part is a single watertight body;
- component, battery and stick-sweep clearances;
- wall thickness;
- both assembly moves, and the battery panel's slide-out path and retention;
- trigger and bumper swing clearance, snap-in paths, magnet travel and prong press depth.

All 33 pass on the parts in `right_hand/`. It's mesh-based and takes about 20 s.

## Run

    cd scripts
    uv venv venv && uv pip install --python venv/bin/python build123d trimesh numpy scipy shapely matplotlib rtree networkx
    venv/bin/python -I build.py ../../../PCB/snips_controller.step out C    # about 5 min; omit C to also build A and B
    venv/bin/python -I verify.py out C                                       # exits 1 if any claim fails
    venv/bin/python -I caps.py out/C                                         # writes out/C/caps/*.stl
    venv/bin/python -I render_options.py out out/options.png                 # optional: A/B/C comparison sheet

`venv/` and `out/` are gitignored. `REUSE_HOLLOW=1` reuses the cached hollow shell when only interior features changed
(saves about 5 minutes), and `SHELL_STEP=1` also writes STEP files (slow and large).

To update `right_hand/`, copy the parts from `out/C/` with these names:
- `top.stl` -> `01_top_shell.stl`
- `bottom.stl` -> `02_bottom_shell.stl`
- `panel.stl` -> `03_battery_panel.stl`
- `trigger_hook.stl` -> `04a_trigger_hook.stl`
- `trigger_frame.stl` -> `04b_trigger_frame.stl`
- `bumper.stl` -> `05_bumper.stl`
- `caps/<name>.stl` -> `caps/cap_<name>.stl`

`print/04b_trigger_frame_PRINT.stl` is the trigger frame rotated onto its back for printing; re-orient it the same way
if the frame changes.

**The board has changed since these parts were built.** They came from the 2026-09-28 STEP export, and the LED changed
from the reverse-mount SK6812MINI-E to the top-mount SK6812MINI-HS after that (#65). Re-export the STEP and rebuild before
printing the top shell.

## Implementation notes

- Coordinates: X across the board, Y from 0 at the palm/USB end to 99.9 at the screen end, Z 0 at the board's back face
  (+Z toward the user). PCB STEP to local: x - 120, y + 159.9.
- Hollowing: OpenCascade's offset and a loft-minus-loft boolean both fail on this shape (hang or invalid solid).
  `hollow_shell()` in `build.py` joins the outer and reversed inner shells into one solid instead.
- Everything after hollowing uses mesh booleans (`meshops.py`, manifold3d): about 20 s, where the same OpenCascade
  booleans took 10+ minutes and returned invalid solids. Cutters are built from plain boxes rather than from the shell,
  because shared faces left bowtie edges that broke the STL round trip.
- The palm-end tip is an analytic rounded cap with its own sections (`TIP_*` params); a spline loft there overshot into
  "ears".
- cad-khana doesn't finish on this spline shell (each distance query takes 5-15 s), which is why `verify.py` exists.

## Assumptions to check with real parts

- Thumbstick height: the STEP uses an ALPS stand-in (housing top 13.5, cap top 21.25 mm above the back face). Measure
  the real GuliKit stick.
- XBee: an envelope-only placeholder (top 11.23 mm above the back face).
- OLED stack height (4.5 mm).
- Battery contacts: a generic size (about 10 x 9 x 0.5 mm plates). The bulkhead slots get fitted to the real part once
  there's a part number.
- Seam strength: the wall is about 1.5 mm at some curved corners, so the lap lip there is about 0.5 mm. Thicken the seam
  locally if it proves weak.

## Not done yet

- The left hand (a mirror of the right).
- An antenna pocket for the U.FL flex antenna, on the interior wall furthest from the grip.
- Labels (numbers, arrows) on the caps.
