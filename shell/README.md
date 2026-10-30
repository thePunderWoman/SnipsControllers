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
