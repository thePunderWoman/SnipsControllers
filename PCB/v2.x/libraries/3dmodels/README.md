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
| `ESP32-S3-WROOM-1U.step` | U_MCU1 | Espressif official [kicad-libraries](https://github.com/espressif/kicad-libraries) (`ESP32-S3-WROOM-1U.STEP`, license not stated) | Origin at the module corner, so offset (-9, -9.6, 0) |
| `XB3-24Z8UM.step` | U_XBEE1 | Digi `XB3-UFL-MMT.STEP`, from "XBee 3 Micro 3D STEP Files MMT" on hub.digi.com (license not stated) | Y-up and far from origin, so rotate -90 about X and offset (76.96, 42.73, 3.53) |

Models taken from KiCad's own library (`${KICAD10_3DMODEL_DIR}`) with no local copy:

- SW_MACRO1-6, SW_BOOT1: `SW_SPST_PTS810.step`, a same-size stand-in for the Alps SKRP.
- D_RGB1: `LED_SMD.3dshapes/LED_WS2812B-Mini_PLCC4_3.5x3.5mm.step`, a 3535 PLCC4 body standing in for the
  SK6812MINI-HS, rotated 180° about Z so its pin-1 corner sits on pad 1 (GND, lower right).

U_CHG1 uses the SnapEDA model already in `../BQ25185DLHR/`, rotated -90° about X because that file is Y-up.

## Why the stock models were missing

KiCad 10's `SW_Push_1P1T-MP_NO_Horizontal_Alps_SKRTLAE010` and `USB_C_Receptacle_HRO_TYPE-C-31-M-12`
footprints point at model files that KiCad 10 does not ship, and that are not in the upstream
kicad-packages3D repository either. The ESP32 footprint pointed at an old `${KICAD8_3RD_PARTY}` path.
The custom footprints (QMA6100P, GuliKit_HallStick, BQ25185DLHR) had no model at all.

## Caveats

- EasyEDA models are community/vendor-provided and are good for enclosure design, not guaranteed
  exact. Check critical dimensions (USB-C opening, button plunger, stick height) against the datasheet
  or a real part before finalising a shell.
