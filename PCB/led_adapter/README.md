# D_RGB1 LED adapter (rev-1 boards only)

The 20 rev-1 Snips boards (10 + Alex's 10) were fabbed with the old SK6812MINI-E-012 land at
D_RGB1. That land is the datasheet's bottom view, so a lens-up LED is mirrored on it and no
rotation fixes it (see PR #65). This small adapter fixes those boards without rework. Its bottom
pads solder onto the old land, and its top carries the SK6812MINI-HS-012 land, the same footprint
the main board uses now. Boards made after PR #65 don't need it.

| Old land pad (bottom) | Net | LED pin (top) |
|---|---|---|
| 1, lower-left | GND | 1 GND |
| 2, upper-left | DIN | 2 DIN |
| 3, upper-right | 3V3 | 3 VDD |
| 4, lower-right | not connected on the main board | none (LED pin 4 DOUT is unused) |

Adapter: 6.8 x 5.6 mm, 2 layers, three 0.3 mm vias. Its outline stays clear of J_USB1, MH4,
R_RGB1/C_RGB1/R_CC2 and the board edge around D_RGB1.

## Files

- `generate_led_adapter.py` is the source. It writes `led_adapter.kicad_pcb` (one adapter) and
  `led_adapter_panel.kicad_pcb` (4 x 5 panel). Don't edit the boards by hand; change the script
  and rerun it with KiCad's Python:
  `/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 PCB/led_adapter/generate_led_adapter.py`
- `production/led_adapter_panel_gerbers.zip` is the upload for the fab (Gerbers, PTH and NPTH
  Excellon drill files).

## Ordering

| Setting | Value |
|---|---|
| Layers | 2 |
| Size | 47.2 x 50.0 mm (one panel of 20 adapters) |
| Delivery format | Panel by customer, 1 design, 4 x 5, mouse-bite tabs already in the files |
| Thickness | 0.6 mm (keeps the LED low; 0.8 mm also works) |
| Surface finish | ENIG preferred (flat pads for stacking); lead-free HASL works |
| Min track / hole | 0.25 mm / 0.3 mm via, 0.5 mm NPTH (standard) |
| Quantity | The minimum (usually 5 panels = 100 adapters) |

## Assembly

1. Snap the adapters out of the panel and sand the tab nubs flush on both short sides. Those
   sides sit about 0.3 mm from the USB-C connector and the mounting hole.
2. Solder the LED to the adapter top ("TOP"). The LED's chamfered corner (pin 1, GND) goes to the
   corner with the ▲ mark, on the side away from "USB".
3. Tin the old D_RGB1 pads and the adapter's bottom pads lightly. Place the adapter with the
   arrow pointing at the USB-C connector. The pad marked "NC" on the bottom goes over the
   main-board pad with no trace running to it. Reflow with hot air or a hotplate.
4. Before powering the board, check that 3V3 is not shorted to GND. Then check that the LED lights.
