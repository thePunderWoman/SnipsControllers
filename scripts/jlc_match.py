#!/usr/bin/env python3
"""Match SnipsControllers BOM parts to JLCPCB/LCSC part numbers.

Exports a grouped BOM from the schematic (by Manufacturer + MPN, which every
part in this project already carries as the 'MF' and 'Manufacturer_Part_Number'
fields) and looks each one up against JLCPCB's public component-search
endpoint -- the same one jlcpcb.com's own website search box uses. No API key
needed; it's the unauthenticated site-search API, not the registered OpenAPI.

This only produces a report. It does not touch the schematic -- review the
output, then a separate step writes confirmed matches back in as an 'LCSC'
field for the Fabrication Toolkit plugin to pick up.

Usage:
    python3 scripts/jlc_match.py
    python3 scripts/jlc_match.py --out /tmp/jlc_match.csv
"""
import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

KICAD_CLI_FALLBACK = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"
JLC_SEARCH_URL = (
    "https://jlcpcb.com/api/overseas-pcb-order/v1/"
    "shoppingCart/smtGood/selectSmtComponentList/v2"
)
FIELDNAMES = ["Refs", "Value", "Footprint", "MF", "MPN", "LCSC", "LibraryType", "Status"]


def find_kicad_cli():
    found = shutil.which("kicad-cli")
    if found:
        return found
    if Path(KICAD_CLI_FALLBACK).exists():
        return KICAD_CLI_FALLBACK
    sys.exit("kicad-cli not found on PATH or at the default macOS install location")


def export_bom(kicad_cli, sch_path):
    with tempfile.NamedTemporaryFile(suffix=".csv", delete=False) as f:
        out_path = f.name
    try:
        subprocess.run(
            [
                kicad_cli, "sch", "export", "bom",
                "--fields", "Reference,Value,Footprint,MF,Manufacturer_Part_Number,DNP",
                "--labels", "Refs,Value,Footprint,MF,MPN,DNP",
                "--group-by", "MF,Manufacturer_Part_Number,Value,Footprint",
                "--ref-range-delimiter", "",
                "-o", out_path,
                str(sch_path),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        with open(out_path, newline="") as f:
            return list(csv.DictReader(f))
    finally:
        Path(out_path).unlink(missing_ok=True)


def normalize(s):
    return re.sub(r"[^A-Z0-9]", "", (s or "").upper())


def search_jlc(keyword):
    payload = {
        "currentPage": 1,
        "pageSize": 25,
        "keyword": keyword,
        "searchSource": "search",
        "searchType": 2,
        "componentBrandList": [],
        "componentSpecificationList": [],
        "componentAttributeList": [],
        "paramList": [],
    }
    req = urllib.request.Request(
        JLC_SEARCH_URL,
        data=json.dumps(payload).encode(),
        headers={
            "Content-Type": "application/json;charset=UTF-8",
            "Accept": "application/json, text/plain, */*",
            "Referer": "https://jlcpcb.com/parts",
            "User-Agent": "Mozilla/5.0 (compatible; SnipsControllers-jlc-match/1.0)",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        data = json.load(resp)
    if data.get("code") != 200:
        return []
    return (data.get("data") or {}).get("componentPageInfo", {}).get("list", []) or []


def best_match(mf, candidates):
    """Pick the right candidate among exact-MPN hits.

    The search is keyword-based, so every candidate already has a matching
    MPN by construction -- what's left is picking the right manufacturer
    when more than one brand sells a part under the same part number.

    JLC's own "JLCPCB Assembly" brand shows up as a zero-stock placeholder
    SKU alongside the real listing for some parts (seen on the USB-C
    connector) -- it's not a real manufacturer, so it's discarded before
    disambiguating on brand.
    """
    real = [c for c in candidates if normalize(c.get("componentBrandEn")) != "JLCPCBASSEMBLY"]
    pool = real or candidates

    mf_n = normalize(mf)
    exact = [c for c in pool if normalize(c.get("componentBrandEn")) == mf_n]
    if len(exact) == 1:
        return exact[0], "matched"

    # JLC sometimes prefixes/suffixes the brand (e.g. "Korean Hroparts Elec"
    # for our "Hroparts Elec") -- fall back to substring containment.
    contains = [c for c in pool if mf_n in normalize(c.get("componentBrandEn")) or
                normalize(c.get("componentBrandEn")) in mf_n]
    if len(contains) == 1:
        return contains[0], "matched (brand name is a variant of schematic's MF field)"

    if len(pool) == 1:
        return pool[0], "matched (brand name differs from schematic's MF field)"
    if exact:
        return exact[0], "matched (multiple brands sell this MPN, picked ours)"
    if contains:
        return contains[0], "matched (multiple brands sell this MPN, picked closest brand match)"
    return pool[0], "AMBIGUOUS: multiple brands sell this MPN, none match our MF field"


def match_row(mf, mpn):
    exact_mpn = normalize(mpn)
    candidates = [c for c in search_jlc(mpn) if normalize(c.get("componentModelEn")) == exact_mpn]
    if not candidates:
        return None, "no LCSC listing found for this MPN"
    return best_match(mf, candidates)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--sch", default="PCB/snips_controller.kicad_sch")
    parser.add_argument("--out", default=None, help="Write results CSV here")
    parser.add_argument("--delay", type=float, default=0.4, help="Seconds between API requests")
    args = parser.parse_args()

    kicad_cli = find_kicad_cli()
    rows = export_bom(kicad_cli, Path(args.sch))

    results = []
    for row in rows:
        mf, mpn, dnp = row["MF"].strip(), row["MPN"].strip(), row.get("DNP", "").strip()
        if dnp:
            results.append({**row, "LCSC": "", "LibraryType": "", "Status": "DNP, skipped"})
            continue
        if not mf or not mpn:
            results.append({**row, "LCSC": "", "LibraryType": "", "Status": "missing MF/MPN"})
            continue
        try:
            match, status = match_row(mf, mpn)
        except (urllib.error.URLError, TimeoutError) as e:
            results.append({**row, "LCSC": "", "LibraryType": "", "Status": f"lookup failed: {e}"})
            continue
        results.append({
            **row,
            "LCSC": (match or {}).get("componentCode", ""),
            "LibraryType": (match or {}).get("componentLibraryType", ""),
            "Status": status,
        })
        time.sleep(args.delay)

    width = {k: max(len(k), *(len(r.get(k, "")) for r in results)) for k in FIELDNAMES}
    header = "  ".join(k.ljust(width[k]) for k in FIELDNAMES)
    print(header)
    print("-" * len(header))
    for r in results:
        print("  ".join(r.get(k, "").ljust(width[k]) for k in FIELDNAMES))

    if args.out:
        with open(args.out, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=FIELDNAMES)
            w.writeheader()
            for r in results:
                w.writerow({k: r.get(k, "") for k in FIELDNAMES})
        print(f"\nWrote {args.out}", file=sys.stderr)

    matched = sum(1 for r in results if r["LCSC"])
    print(f"\n{matched}/{len(results)} parts matched to an LCSC part number.", file=sys.stderr)


if __name__ == "__main__":
    main()
