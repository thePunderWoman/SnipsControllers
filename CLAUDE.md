# Snips Controllers — Claude Notes

## Design principles

**Test coverage**
- All new logic should have unit tests in `test/` to prevent regressions.
- Tests run on the native PlatformIO environment (no hardware required) — keep them that way. Do not introduce test dependencies that require Arduino or physical hardware.
- When fixing a bug, add a test that would have caught it.
- CI enforces a minimum line-coverage threshold (via `gcovr`, currently 90%) on `src/`, excluding `SnipsController.ino` (Arduino-only, can't run on native) and a small, explicitly-named list of genuinely hardware-touching adapter files (thin wrappers over I2C/SPI/ADC/RMT/NVS — see the exclusion list in `platformio.ini`'s `[env:native]` and `.github/workflows/ci.yml`'s gcovr `--exclude`). Keep those adapters intentionally thin — no branching logic worth testing — and put all real logic in plain, hardware-agnostic C++ that gets compiled and covered natively for real. Keep new logic covered well enough to not drop below that bar — don't lower the threshold just to unblock a PR, and don't grow the exclusion list to dodge coverage on code that actually has logic in it; both are a deliberate call for a human to make.

**Bug fixes and regression test**
- Any time you fix a bug, that bugfix should be covered by a new regression test.

## Git workflow

**All changes must go through pull requests — never push directly to `main`.**

1. Create a feature branch, make your changes, then open a PR.
2. Push branches and create PRs with:
   ```
   git push thePunderWoman <branch>
   gh pr create ...
   ```
3. Once a PR lands, delete the local feature branch. This repo always squash or rebase merges — `main` never gets a merge commit for the PR, so `git branch -d` (and `--merged` checks) won't recognize the branch as merged even though its content has landed. Confirm via `git log --oneline` (look for the PR's commit/title on `main`) or `gh pr view <branch> --json state`, then use `git branch -D <branch>` to remove it.

## BOM Value fields

The fab's BOM shows only Designator, Footprint, Quantity and **Value** (never MF, MPN or LCSC), so every
component's Value is `<label> | <MPN> | <manufacturer> | <package>` (see `scripts/bom_values.py` for the
exact rules). Every board component needs `MF` and `Manufacturer_Part_Number`.

- After adding or changing a part, run `python3 scripts/bom_values.py --apply`, then `--check`. Close
  the files in KiCad first (or reload after). `--check` also fails if the PCB copy of a part's Value,
  MF, MPN or footprint differs from the schematic, so run Update PCB from Schematic before regenerating
  production files.
- A new footprint must be added to `PACKAGES` in the script on purpose. Parts that are hand-sourced with
  no known MPN go in `NO_MPN_OK`, so a gap is never silent.
- Prefer common parts (several makers, broadly stocked, one package per name). The retired JLCPCB
  Basic-library preference no longer applies.

