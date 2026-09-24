#!/usr/bin/env python3
"""Keep every component's Value field unambiguous for the fab.

The exported BOM shows a fab only Designator, Footprint, Quantity and *Value*. It never shows the MF,
Manufacturer_Part_Number or LCSC fields. So the Value must carry everything needed to buy the right
part, and a bare "BAT54W" is not enough (it is a SOT-323 part from most makers, SOD-123 from a few).

Canonical Value, built from the part's own fields:

    <label> | <MPN> | <manufacturer> | <package>

  label         the human value (100kΩ, 10µF, 2.2µH). Dropped when it is just the MPN again.
  MPN           Manufacturer_Part_Number, verbatim.
  manufacturer  MF, with commas/periods removed so the BOM CSV never needs quoting.
  package       explicit, from PACKAGES below (a new footprint must be added there on purpose).

    python3 scripts/bom_values.py --check    # exit 1 if any Value, or the PCB copy, is out of date
    python3 scripts/bom_values.py --apply    # rewrite Value in the schematic sheets and the PCB

Rewrites the .kicad_sch/.kicad_pcb text directly (close them in KiCad first, or reload afterwards).
Parts that are not on the board (power flags) and hand-sourced parts with no MPN (NO_MPN_OK) are
skipped. See CLAUDE.md for the rule this enforces.
"""
import argparse
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
PCB_DIR = os.path.join(ROOT, "PCB")
PCB_FILE = os.path.join(PCB_DIR, "snips_controller.kicad_pcb")
SEP = " | "

# Footprint (as named in the schematic) -> package text a fab can read. Add new footprints here.
PACKAGES = {
    "BQ25185DLHR:IC_BQ25185DLHR": "WSON-10 2.2x2.0mm",
    "Button_Switch_SMD:SW_Push_1P1T-MP_NO_Horizontal_Alps_SKRTLAE010": "SMD tactile horizontal",
    "Button_Switch_SMD:SW_Push_1TS009xxxx-xxxx-xxxx_6x6x5mm": "SMD tactile 6x6x5mm",
    "Button_Switch_SMD:SW_SPST_EVQP2_ShortPushTravel_H2.1mm": "SMD tactile 2.1mm",
    "Capacitor_SMD:C_0402_1005Metric": "0402",
    "Capacitor_SMD:C_0603_1608Metric": "0603",
    "Capacitor_SMD:C_0805_2012Metric": "0805",
    "Resistor_SMD:R_0402_1005Metric": "0402",
    "Connector_JST:JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal": "THT right-angle horizontal 2.00mm",
    "Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal": "SMD horizontal 1.00mm 4-pin",
    "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12": "USB-C receptacle",
    "Diode_SMD:D_SOD-123": "SOD-123",
    "Diode_SMD:D_SOD-323": "SOD-323",
    "Espressif:ESP32-S3-WROOM-1": "SMD module 18x25.5mm",
    "Inductor_SMD:L_Bourns-SRN4018": "4.0x4.0mm SMD",
    "Package_TO_SOT_SMD:SOT-23": "SOT-23",
    "Package_TO_SOT_SMD:SOT-23-5": "SOT-23-5",
    "Package_TO_SOT_SMD:SOT-323_SC-70": "SOT-323 (SC-70)",
    "Package_TO_SOT_THT:TO-92_Inline": "TO-92 THT",
    "SnipsControllers_Custom:GuliKit_HallStick": "custom footprint",
    "SnipsControllers_Custom:QMA6100P": "LGA-12 2x2mm",
    "SnipsControllers_Custom:SK6812MINI-E-012": "SMD 3.2x2.8mm",
    "Xbee3:XB324Z8UTJ": "XBee through-hole",
}

# Labels that carry information the MPN does not (orientation, variant). Used verbatim.
LABEL_OVERRIDES = {
    "BT1": "PH 2-pin battery RIGHT-ANGLE HORIZONTAL",
    "D_RGB1": "SK6812MINI-E-012",
}

# On the board but hand-sourced with no known MPN. Listed on purpose, so a new gap is never silent.
NO_MPN_OK = {"J_STICK1"}


def norm(s):
    return re.sub(r"[^a-z0-9]", "", s.lower())


def block_at(text, start):
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise ValueError("unbalanced parentheses")


def prop(block, name):
    m = re.search(r'\(property "%s" "([^"]*)"' % re.escape(name), block)
    return m.group(1) if m else ""


def schematic_parts():
    """Yield (path, ref, block) for every placed symbol instance (not lib_symbols cache entries)."""
    for path in sorted(glob.glob(os.path.join(PCB_DIR, "*.kicad_sch"))):
        text = open(path).read()
        for m in re.finditer(r'\n\t\(symbol\n\t\t\(lib_id "[^"]+"\)', text):
            block = block_at(text, m.start() + 1)
            yield path, prop(block, "Reference"), block


def canonical(ref, block):
    """Return (value, problem). value is None when the part is skipped or cannot be built."""
    if "(on_board no)" in block:
        return None, None
    mpn, mf, fp = prop(block, "Manufacturer_Part_Number"), prop(block, "MF"), prop(block, "Footprint")
    if not mpn:
        return None, None if ref in NO_MPN_OK else "no Manufacturer_Part_Number"
    if not mf:
        return None, "no MF (manufacturer)"
    if fp not in PACKAGES:
        return None, f"footprint {fp!r} not in PACKAGES"
    parts = prop(block, "Value").split(SEP)
    label = parts[0] if len(parts) == 4 else ("" if len(parts) == 3 else parts[0])
    if ref in LABEL_OVERRIDES:
        label = LABEL_OVERRIDES[ref]
    elif norm(label) in norm(mpn) or norm(mpn) in norm(label):
        label = ""
    fields = ([label] if label else []) + [mpn, re.sub(r"[,.]", "", mf).strip(), PACKAGES[fp]]
    return SEP.join(fields), None


def set_value(text, start, value):
    """Rewrite the Value property inside the block that starts at `start`."""
    block = block_at(text, start)
    new, n = re.subn(r'(\(property "Value" ")[^"]*(")', lambda m: m.group(1) + value + m.group(2), block, count=1)
    assert n == 1
    return text[:start] + new + text[start + len(block):]


def pcb_footprints(text):
    for m in re.finditer(r'\n\t\(footprint "([^"]+)"', text):
        start = m.start() + 1
        block = block_at(text, start)
        yield m.group(1), start, block


def run(apply):
    problems, updates = [], 0
    wanted = {}
    for path, ref, block in schematic_parts():
        value, problem = canonical(ref, block)
        if problem:
            problems.append(f"{ref}: {problem}")
        if value is None:
            continue
        wanted[ref] = (value, prop(block, "Footprint"), prop(block, "MF"), prop(block, "Manufacturer_Part_Number"))
        if prop(block, "Value") != value:
            updates += 1
            problems.append(f"{ref}: schematic Value is {prop(block, 'Value')!r}, want {value!r}")
            if apply:
                text = open(path).read()
                m = re.search(r'\(property "Reference" "%s"' % re.escape(ref), text)
                start = text.rfind("\n\t(symbol\n", 0, m.start()) + 1
                open(path, "w").write(set_value(text, start, value))

    pcb = open(PCB_FILE).read()
    seen = set()
    for fpname, start, block in list(pcb_footprints(pcb))[::-1]:  # last-to-first keeps offsets valid
        ref = prop(block, "Reference")
        if ref not in wanted:
            continue
        seen.add(ref)
        value, fp, mf, mpn = wanted[ref]
        if fpname != fp.split(":", 1)[-1] and fpname != fp:
            problems.append(f"{ref}: PCB footprint {fpname!r} differs from schematic {fp!r} (update PCB from schematic)")
        if prop(block, "Manufacturer_Part_Number") != mpn or prop(block, "MF") != mf:
            problems.append(f"{ref}: PCB MF/MPN differ from schematic (update PCB from schematic)")
        if prop(block, "Value") != value:
            updates += 1
            problems.append(f"{ref}: PCB Value is {prop(block, 'Value')!r}, want {value!r}")
            if apply:
                pcb = set_value(pcb, start, value)
    for ref in sorted(set(wanted) - seen):
        problems.append(f"{ref}: in the schematic but not on the PCB (update PCB from schematic)")
    if apply:
        open(PCB_FILE, "w").write(pcb)
    return problems, updates, len(wanted)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--check", action="store_true")
    g.add_argument("--apply", action="store_true")
    args = p.parse_args()
    problems, updates, n = run(args.apply)
    if args.apply:
        print(f"{n} parts checked, {updates} Value fields rewritten")
        problems = [x for x in problems if "Value is" not in x]
    for line in problems:
        print(line)
    if problems:
        print(f"\n{len(problems)} problem(s)")
        return 1
    print(f"{n} parts, all Values and PCB fields in sync")
    return 0


if __name__ == "__main__":
    sys.exit(main())
