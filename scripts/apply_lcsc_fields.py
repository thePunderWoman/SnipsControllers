#!/usr/bin/env python3
"""Write matched LCSC part numbers into the schematic as an 'LCSC' field.

Takes the CSV produced by jlc_match.py (one row per part group, comma-
separated References, an LCSC column) and, for every reference with a
matched LCSC code, clones that symbol's existing 'Manufacturer_Part_Number'
property block verbatim -- same indentation, same hide/effects settings --
swapping only the field name and value. That guarantees the new field
matches this project's existing formatting exactly, since it's copied from
a real property already in the file rather than hand-built.

The Fabrication Toolkit KiCad plugin (already used in this repo for
production-file generation) recognizes a field named 'LCSC' as a fallback
match source for its BOM export.

Usage:
    python3 scripts/apply_lcsc_fields.py /tmp/jlc_match.csv
"""
import csv
import glob
import re
import sys
from pathlib import Path

PCB_DIR = Path(__file__).resolve().parent.parent / "PCB"


def find_balanced(text, start):
    """start is the index of an opening '('. Returns index one past its match."""
    depth = 0
    i = start
    in_str = False
    n = len(text)
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
                if depth == 0:
                    return i + 1
        i += 1
    raise ValueError("unbalanced parens")


def load_targets(csv_path):
    targets = {}
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            lcsc = row.get("LCSC", "").strip()
            if not lcsc:
                continue
            for ref in row["Refs"].split(","):
                targets[ref.strip()] = lcsc
    return targets


def find_symbol_blocks(text):
    """Yield (start, end) spans of placed-component '(symbol ...)' blocks.

    Distinguishes them from the graphical sub-unit '(symbol "R_0_1" ...)'
    blocks inside lib_symbols definitions, which start with a quoted name
    instead of '(lib_id ...)'.
    """
    for m in re.finditer(r"\(symbol\s", text):
        start = m.start()
        after = text[m.end():].lstrip()
        if not after.startswith("(lib_id"):
            continue
        end = find_balanced(text, start)
        yield start, end


def apply_to_file(path, targets, applied, missing_mpn):
    text = path.read_text()
    changed = False

    # Process in reverse block order so earlier insertions don't shift the
    # offsets of blocks we haven't processed yet.
    blocks = list(find_symbol_blocks(text))
    for start, end in reversed(blocks):
        block = text[start:end]
        ref_m = re.search(r'\(property "Reference" "([^"]+)"', block)
        if not ref_m or ref_m.group(1) not in targets:
            continue
        ref = ref_m.group(1)
        lcsc = targets[ref]

        mpn_m = re.search(r'\(property "Manufacturer_Part_Number" "((?:[^"\\]|\\.)*)"', block)
        if not mpn_m:
            missing_mpn.append(ref)
            continue

        prop_start = start + mpn_m.start()
        line_start = text.rfind("\n", 0, prop_start) + 1
        indent = text[line_start:prop_start]
        prop_end = find_balanced(text, prop_start)

        original = text[prop_start:prop_end]
        mpn_value = mpn_m.group(1)
        new_block = original.replace(
            f'"Manufacturer_Part_Number" "{mpn_value}"', f'"LCSC" "{lcsc}"', 1
        )
        text = text[:prop_end] + "\n" + indent + new_block + text[prop_end:]
        changed = True
        applied[ref] = lcsc

    if changed:
        path.write_text(text)
    return changed


def main():
    if len(sys.argv) != 2:
        sys.exit(f"Usage: {sys.argv[0]} <jlc_match.csv>")
    targets = load_targets(sys.argv[1])

    applied = {}
    missing_mpn = []
    for sch in sorted(glob.glob(str(PCB_DIR / "*.kicad_sch"))):
        path = Path(sch)
        if apply_to_file(path, targets, applied, missing_mpn):
            print(f"updated {path.name}")

    unmatched = sorted(set(targets) - set(applied))
    print(f"\n{len(applied)}/{len(targets)} references got an LCSC field written.")
    if unmatched:
        print("NOT FOUND in any schematic (check reference spelling):")
        for ref in unmatched:
            print(f"  {ref}")
    if missing_mpn:
        print("Found the symbol but it has no Manufacturer_Part_Number property to clone:")
        for ref in missing_mpn:
            print(f"  {ref}")


if __name__ == "__main__":
    main()
