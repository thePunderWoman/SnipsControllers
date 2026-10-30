"""Where the print tools read and write. Everything is derived from this file's location, so the tools in
shell/<version>/export/print/tools/ always work on that version's files.

  EXPORT  shell/<version>/export          (raw Fusion exports: stl/, step/)
  PRINT   shell/<version>/export/print    (print-ready output: stl/, step/)

Override for a scratch checkout or a container with SNIPS_EXPORT_DIR / SNIPS_PRINT_DIR."""
import os
HERE = os.path.dirname(os.path.abspath(__file__))
EXPORT = os.environ.get('SNIPS_EXPORT_DIR') or os.path.normpath(os.path.join(HERE, '..', '..'))
PRINT = os.environ.get('SNIPS_PRINT_DIR') or os.path.normpath(os.path.join(HERE, '..'))
SRC_STL = os.path.join(EXPORT, 'stl')
SRC_STEP = os.path.join(EXPORT, 'step')
OUT_STL = os.path.join(PRINT, 'stl')
OUT_STEP = os.path.join(PRINT, 'step')
