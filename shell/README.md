# Snips shell

One folder per board/shell revision. Keep old versions; start the next one by copying the folder.

```
shell/<version>/
  fusion/SnipsShell.f3d            the Fusion master (plus lib.py, sc_*.json used by the rebuild scripts)
  export/stl, export/step          raw per-part exports from Fusion (as modelled)
  export/print/stl, print/step     print-ready files (oriented, on Z=0), README.md, tools/
  export/reference/                non-printing reference parts
  export/_export_log.txt
  photos/
PCB/step/<version>/snips_controller.step   the board model for that revision (v1.1, v1.2)
```

## Exporting from Fusion
Write to `shell/<version>/export/` (use the version the Fusion doc belongs to), per component:
`createSTEPExportOptions(.../export/step/<Part>.step, comp)`, `createSTLExportOptions(comp, .../export/stl/<Part>.stl)`
(high refinement, binary), and `createFusionArchiveExportOptions(.../fusion/SnipsShell.f3d)`.
Then regenerate the print files (below).

## Print files
The tools in `export/print/tools/` find their folders from their own location, so run them from that version's
`tools/` folder and they read `export/stl|step` and write `export/print/stl|step`. Override with
`SNIPS_EXPORT_DIR` / `SNIPS_PRINT_DIR` if the files live somewhere else. See `export/print/README.md`.

## v2 variants: standard (14500) and pouch cell
v2 has two bottom shells that share everything else (TopShell, buttons, Bumper, TriggerPost, TriggerCaps, PCB).
Width and length are identical; only the height differs.

| | Standard (14500 cell) | Pouch cell (YDL103035) |
|---|---|---|
| Bottom shell | `BottomShell` | `BottomShell_Pouch` |
| Trigger | `Trigger` | `Trigger_Pouch` (shorter, scooped for the thinner shell) |
| Cell | 14500 (AA size) in the battery bay, `BatteryPanel` + contacts | 10 x 30 x 37 mm LiPo pouch, held by printed stops, JST PHR-2 lead |
| Extra parts | `BatteryPanel`, contact clip, contacts block | none |
| **Width** | 35.0 mm | 35.0 mm |
| **Length** | 80.65 mm | 80.65 mm |
| **Height, flat top surface to flat bottom** | **33.70 mm** | **30.32 mm** |
| Height incl. thumbstick bump | 39.0 mm | 35.62 mm |

Height is measured from the flat shell surface (not the thumbstick bump) to the bottom flat surface, so the pouch
variant is **3.38 mm** thinner. Both triggers rest 8 deg forward of vertical (parameters `trig_rest_deg` and
`pt_rest_deg`) with a 28 deg pull from there, which gives the finger a little more room at the front wall.
Reference cell model: `export/reference/stl|step/Cell_YDL103035_Ref.*`.

### Wrist strap anchor
Both bottom shells have two stacked slots through the rear (USB-end) wall, centred on the shell, below the seam and the
USB port. Push the strap's thin attachment loop in through one slot and back out of the other so it wraps the 3.4 mm
bar of wall between them. Defaults suit a 5 x 1 mm flat loop (`strap_loop_w`, `strap_loop_t`, `strap_clr`):
slots 5.4 x 1.6 mm, bar `strap_bar_h` 3.4 mm, top edge at z -2.4 mm (`strap_top_z`). The bar is the full 2 mm wall thick and
printed with its layers along the load direction, which is comfortably strong for a dangling controller;
if you hang something heavier, raise `strap_bar_h` and re-export. Interior behind the slots is clear in both variants,
so the loop has room to turn.

### Photos
`photos/snips_shell_*.png` show the standard variant, `photos/snips_shell_pouch_*.png` the pouch variant (same angles;
the pouch set has no top/thumbstick close-ups because the top is shared).

| Standard | Pouch |
|---|---|
| ![](v2.x/photos/snips_shell_01_front_left_top.png) | ![](v2.x/photos/snips_shell_pouch_01_front_left_top.png) |
| ![](v2.x/photos/snips_shell_04_bottom.png) | ![](v2.x/photos/snips_shell_pouch_04_bottom.png) |
| ![](v2.x/photos/snips_shell_08_underside_trigger_belly.png) | ![](v2.x/photos/snips_shell_pouch_08_underside_trigger_belly.png) |
| ![](v2.x/photos/snips_shell_11_strap_slots.png) | ![](v2.x/photos/snips_shell_pouch_11_strap_slots.png) |
