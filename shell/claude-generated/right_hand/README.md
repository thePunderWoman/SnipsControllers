# Snips controller shell: right hand

Everything here is the current, verified design (`verify.py`: 33/33).
The left hand is a mirror of these parts (not generated yet).

## Print list and orientations

Assumptions: FDM, PLA or PETG, 0.4 mm nozzle, 0.2 mm layers. Orientations were chosen by measuring overhang area beyond 45 degrees, bed contact and height for each part (`print_orientations.png` shows them; red = overhang, green = bed contact), then by judgment about strength and which faces you see. I have not sliced these, so check the first layer and support preview in your slicer.

| File | Qty | Load it as | Supports | Why this way |
|---|---|---|---|---|
| `01_top_shell.stl` | 1 | as is (face up, open side down) | tree, build plate only (they grow up inside the cavity); 5 mm brim | The rim is the only flat bed contact (about 320 mm2). The visible face and pod print on top with no supports; the roof, pod underside, posts and pins hang inside and get supported from the plate. Printing face down would put supports on the cosmetic face. |
| `02_bottom_shell.stl` | 1 | as is (belly down, open side up) | tree, build plate only; also inside the battery shroud | The belly is curved, so first-layer contact is thin: use a 5-8 mm brim and slow first layer. Posts, ribs, rail blocks and hinge fins grow up with no support. The battery shroud's arched ceiling spans about 15 mm over the panel opening, so expect supports (or sag) inside it. |
| `03_battery_panel.stl` | 1 | as is (outer face down) | tree, build plate only; brim | Keeps the rail tongues flat in the layer plane (strong). The curved outer face is the back of the controller. If the tongues come out too tight, sand them rather than scaling the part. |
| `04a_trigger_hook.stl` | 1 | as is (flat lip down) | none to light | Biggest flat face on the bed, and the lip is the surface the controller rests on. |
| `04b_trigger_frame.stl` | 1 | use `print/04b_trigger_frame_PRINT.stl` (lying on its back) | none | Hinge pins lie horizontal, so they are not stacked rings (weakest for bending). Overhang is 21 mm2. |
| `05_bumper.stl` | 1 | as is | none to light | Pins lie horizontal; the bar sits on its back. |
| `caps/cap_face_1..6.stl` | 6 | as is (head down) | none | The dome is only 0.6 mm tall, so it first-layers as a small disc; the 0.8 mm flange overhang is short enough to print clean. Print a couple of spares. |
| `caps/cap_side_L1, L2, R1, R2.stl` | 4 | as is (head down) | none | Same. Each is sized to its own wall depth, so keep the names. |
| `caps/cap_power.stl` | 1 | as is (head down) | none | Same; the neck is angled, so check the preview for a small overhang under it. |

Suggested settings: 4 perimeters (the walls are 1.6 mm, so four lines fill them exactly), 15-20% gyroid infill, 0.2 mm layers; PETG for the shells and clip arms if you have it (the snap arms are only about 1% strain, so PLA also works); first layer 100% flow. Print the small fit-test parts first (one face cap, the bumper, the trigger frame and hook, about an hour in total) to check the 0.15 mm hinge play and the 0.05 mm cap flange gap on your printer before committing to the two big shells.

Not printed: GuliKit stick cap (used as is), 14500 cell and holder. No screws anywhere.
Hardware you still need: 3 x 2 mm neodymium disc magnet (3.0 x 2.0), one compression spring (4.4 mm OD, 13 mm free) for the trigger, a dab of glue for the hook to the frame stems, and two stamped battery contacts: a flat (+) plate at the palm end and a spring (-) at the screen end, each about 10 x 9 x 0.5 mm plate with a solder tab and a spring coil up to 5 mm free (generic size; send the part number and I will fit the slots to it).

## Assembly order

1. Snap the trigger frame into the bottom shell from inside (pins click into the fins), drop the magnet in its pocket, put the spring on the post.
2. Glue the trigger hook onto the two stems from below.
3. Snap the bumper into the top shell fins from inside.
4. Drop the PCB into the bottom shell, lower the top shell over it until the clips snap. Move the Hall sensor to x = 25, y = 92 (bend the leads).
5. Slide a contact plate up into each bulkhead slot from below (tab out the back, solder the wires), drop the 14500 in from below (push the spring end in first, then seat the + end), then slide the panel in from the palm end. Press the caps in from outside.

## Reference images

- `battery_bay_views.png` and `battery_bay_sections.png`: the battery bay with the panel out; the cell sits in a continuous channel that is closed on top and open only at the bottom (panel side). Palm end = flat (+) contact, screen end = spring (-). + and - are engraved on the underside of each bulkhead tongue (right side), readable from below with the cell in.
- `print_orientations.png`: each part in its print orientation, bed contact and overhang marked.
- `trigger_bumper_sections.png`: side sections through the trigger, bumper, magnet and spring.
- `trigger_bumper_views.png`: end and oblique views.

## Regenerating

From `../scripts/`: `python -I build.py <step> <out> C`, then `verify.py`, `caps.py`. Numbers live in `params.py`.
