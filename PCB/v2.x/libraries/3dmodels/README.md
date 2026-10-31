# 3D models

Models for parts whose stock KiCad model is missing, plus simplified placeholders for parts that have
no vendor model. The footprints reference them as `${KIPRJMOD}/libraries/3dmodels/...`, so they work on
any machine that has the repo. `scripts/set_3d_models.py` writes the model entries (paths, offsets,
rotations). `scripts/set_3d_models.py --check` fails if any footprint on the board has no model, or has
a model that does not resolve.

| File | Part | Source | Alignment notes |
|---|---|---|---|
| `USB_C_Receptacle_HRO_TYPE-C-31-M-12.step` | J_USB1, HRO TYPE-C-31-M-12 | EasyEDA/LCSC C165948, fetched with `easyeda2kicad` | Rotated 180° and offset -1.05mm in Y so the shell legs, pegs and SMD tails land on the KiCad footprint's pads |
| `SW_Alps_SKRTLAE010.step` | SW_PWR1, SW_VOL_*, SW_THR_*, SW_TRIG_DIG1 (side buttons) | EasyEDA/LCSC C110293 | Pegs, terminals and plunger already match the KiCad footprint. No offset |
| `Thumbstick_ALPS_RKJXV1224005.step` | J_STICK1 (DNP) | EasyEDA/LCSC C146170 (ALPS RKJXV1224005) | **Stand-in**: the GuliKit hall stick is a drop-in for this ALPS footprint family, but its body may differ. Z offset +11.4mm puts the housing standoffs on the board |
| `BT840X_outline.step` | U_MCU1 (BT840XE) | Fanstel `bt832x_1211.stp`, same 15 x 28 mm body (license not stated) | **Stand-in**: one solid with BT832X markings and no u.FL. Rotate -90 about X, offset (0, -0.3, 0.3) |

Models taken from KiCad's own library (`${KICAD10_3DMODEL_DIR}`) with no local copy:

- SW_MACRO1-6, SW_BOOT1: `SW_SPST_PTS810.step`, a same-size stand-in for the Alps SKRP.
- D_RGB1: `LED_SMD.3dshapes/LED_WS2812B-Mini_PLCC4_3.5x3.5mm.step`, a 3535 PLCC4 body standing in for the
  SK6812MINI-HS, rotated 180° about Z so its pin-1 corner sits on pad 1 (GND, lower right).

U_CHG1 uses the SnapEDA model already in `../BQ25185DLHR/`, rotated -90° about X because that file is Y-up.

## Why the stock models were missing

KiCad 10's `SW_Push_1P1T-MP_NO_Horizontal_Alps_SKRTLAE010` and `USB_C_Receptacle_HRO_TYPE-C-31-M-12`
footprints point at model files that KiCad 10 does not ship, and that are not in the upstream
kicad-packages3D repository either.
The custom footprints (QMA6100P, GuliKit_HallStick, BQ25185DLHR) had no model at all.

## Caveats

- EasyEDA models are community/vendor-provided and are good for enclosure design, not guaranteed
  exact. Check critical dimensions (USB-C opening, button plunger, stick height) against the datasheet
  or a real part before finalising a shell.
