#!/usr/bin/env python3
"""ngspice check of the V2 soft latch, which differs from V1 in three ways.

PWR_HOLD is on IO42, which has no reset pull, so the pin floats during a reset. C_LATCH_G1 is 1 uF so
the latch rides through a short reset, and R_INV_BASE1 and R_INV_PU1 are 220k to cut the off current.

  insert    battery plugged in with the board off: the gate stays low
  hotplug   USB raises VSYS with the board off: the gate stays low
  press     a held button turns PWR_EN on, across supply and FET-threshold corners
  off       firmware drops PWR_HOLD: PWR_EN falls and stays down, and no pull-up can re-latch it
  ride      the hold pin floats for 500 ms (a reset): the board stays on, and for 300 ms at a derated C_LATCH_G1
  clamped   battery insertion with the pad ESD diodes into the dead rail: the gate stays low, and the off drain is bounded
  boot      a 500 ms press survives until firmware drives PWR_HOLD 350 ms after the rail comes up

Reuses the models and solver wrapper in latch_sim.py. The netlist mirrors PCB/v2.x/stackup/board.kdl by
hand, so update it when those values or that topology change. See README.md for what is not modeled.

    python3 scripts/spice/latch_v2_sim.py          # exit 1 on any failure
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import latch_sim as ls

R_INV_BASE1 = R_INV_PU1 = "220k"
C_LATCH_G1 = "1u"
C_LATCH_DERATED = "0.55u"  # estimate: -10% tolerance and about -40% for a 6.3 V 0402 X5R at 2.8 V bias
CORNERS = ((3.0, 1.45, 1.3), (3.7, 1.05, 0.9), (4.2, 0.65, 0.6), (4.5, 0.65, 0.6))

MAX_GATE_OFF = 0.5    # V, well under the AO3400A's 0.65 V minimum threshold
MAX_PRESS_S = 0.25    # button press to PWR_EN on
MAX_OFF_S = 2.5       # PWR_HOLD low to PWR_EN low
RIDE_S = 0.5          # reset the latch must ride through
RIDE_DERATED_S = 0.3  # the same, at C_LATCH_DERATED
MAX_OFF_DRAIN = 60e-6 # A from VSYS with the board off, resistors only: no IC quiescent current
PRESS_S = 0.5         # press the board must accept as a normal turn-on
FW_DELAY_S = 0.35     # rail up to firmware driving PWR_HOLD, an estimate until measured


def core(vthn, vthp, vsys_pwl, sw_pwl, c_latch=C_LATCH_G1):
    return f"""* v2 latch
.param VTHN={vthn} VTHP={vthp}
{ls.MODELS}
.options method=gear reltol=2e-3 abstol=1n vntol=50u gmin=1e-10 itl4=200
.model SWDR SW(RON=30 ROFF=1G VT=0.5 VH=0.05)
.model SWBK SW(RON=0.3 ROFF=10Meg VT=0.8 VH=0.2)
Vsys vsys 0 PWL({vsys_pwl})
Rbtn v3v3 sense {ls.R_BTN_PU1}
Cdb sense 0 {ls.C_DBNC1}
Ssw sense 0 swc 0 SWM
Vsw swc 0 PWL({sw_pwl})
Ribase vsys base {R_INV_BASE1}
Diso base sense DBAT54
Q1 trig base 0 QMMBT
Ripu vsys trig {R_INV_PU1}
Dor1 trig latchg D4148
Dor2 hold latchg D4148
Rlg latchg 0 {ls.R_LATCH_G1}
Clatch latchg 0 {c_latch}
Mlatch pwrgate latchg 0 0 NAO3400 W=1 L=1
Cgs1 latchg 0 550p
Cgd1 latchg pwrgate 75p
Rgate vsys pwrgate {ls.R_GATE1}
Mpwr pwren pwrgate vsys vsys PAO3401 W=1 L=1
Cgs2 pwrgate vsys 500p
Rpd pwren 0 {ls.R_PWR_EN_PD1}
Cen pwren 0 20p
"""


def buck(vsys, load_a=0.06, c="37u"):
    v = min(3.32, vsys - 0.05)
    return f"V33 bk 0 {v}\nSbk bk v3v3 pwren 0 SWBK\nCrail v3v3 0 {c}\nRload v3v3 0 {v / load_a}\n"


def dead_rail():
    return "R3 v3v3 0 300\nC3 v3v3 0 1u\nRh hold 0 100Meg\n"


def clamped_rail(crail):
    """The dead rail with the ESD diodes of IO21, IO42 and IO10 into it, and the VSYS divider on IO10."""
    return (f"Rv1 vsys io10 200k\nRv2 io10 0 100k\nCv io10 0 100n\nDio10 io10 v3v3 DESD\nDio21 sense v3v3 DESD\n"
            f"Dio42 hold v3v3 DESD\n.model DESD D(IS=1e-14 N=1.05)\nR3 v3v3 0 311k\nC3 v3v3 0 {crail}\nRh hold 0 100Meg\n")


def firmware(delay):
    """Drives PWR_HOLD from the rail once the rail has been up for delay seconds."""
    return (f"Bup 0 up I = v(v3v3) > 2.8 ? {1e-6 / delay} : 0\nCup up 0 1u\nRup up 0 1G\n"
            "Bhi hi 0 V = v(up) > 1 ? 1 : 0\nSdrv v3v3 hold hi 0 SWDR\nRh hold 0 100Meg\n")


def run(net, nodes=("v(latchg)", "v(pwren)", "v(v3v3)")):
    ng = ls._load()
    ng.ngSpice_Command(b"destroy all")
    lines = [l.encode() for l in net.strip().split("\n")] + [None]
    ng.ngSpice_Circ((ls.ctypes.c_char_p * len(lines))(*lines))
    ng.ngSpice_Command(b"run")
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "o.txt")
        ng.ngSpice_Command(f"wrdata {out} {' '.join(nodes)}".encode())
        rows = [[float(x) for x in l.split()] for l in open(out)]
    names = [n[2:-1] if n.startswith("v(") else n for n in nodes]
    return [r[0] for r in rows], {n: [r[2 * i + 1] for r in rows] for i, n in enumerate(names)}


def insert():
    out = []
    for (v, n, p), rise in zip(CORNERS, ("1m", "100u", "10u", "10u")):
        _, d = run(core(n, p, f"0 0 {rise} {v} 1 {v}", "0 0 1 0") + dead_rail() + ".tran 5u 400m uic\n.end\n")
        pk = max(d["latchg"])
        out.append((f"insert VSYS={v}V", pk < MAX_GATE_OFF, f"peak gate {pk:.2f} V"))
    return out


def hotplug():
    out = []
    for a in (3.0, 3.7, 4.2):
        pwl = f"0 {a} 10m {a} 10.001m 4.5 1 4.5"
        net = core(0.65, 0.6, pwl, "0 0 1 0") + dead_rail() + ".ic v(base)=0.7 v(sense)=0.3\n.tran 2u 300m uic\n.end\n"
        t, d = run(net)
        pk = max(x for x, tt in zip(d["latchg"], t) if tt > 0.009)
        out.append((f"hotplug {a}->4.5V", pk < MAX_GATE_OFF and d["pwren"][-1] < 0.4, f"peak gate {pk:.2f} V"))
    return out


def press():
    out = []
    for v, n, p in CORNERS:
        net = core(n, p, f"0 {v} 4 {v}", "0 0 10m 0 10.01m 1 1.9 1") + buck(v) + "Rh hold 0 100Meg\n.tran 100u 2 uic\n.end\n"
        t, d = run(net)
        on = next((t[i] - 0.01 for i in range(len(t)) if t[i] > 0.01 and d["pwren"][i] > ls.V_EN_HIGH), None)
        out.append((f"press VSYS={v}V VthN={n}V", on is not None and on <= MAX_PRESS_S,
                    "never" if on is None else f"PWR_EN on after {on * 1e3:.0f} ms"))
    return out


def hold_ic(v):
    gate = min(v - 0.7, 2.8)
    return f".ic v(latchg)={gate} v(v3v3)=3.3 v(sense)=3.3 v(pwren)={v} v(pwrgate)=0 v(base)=0.72 v(trig)=0 v(hold)=3.3\n"


def power_off():
    out = []
    for v, n, p in CORNERS:
        net = (core(n, p, f"0 {v} 6 {v}", "0 0 6 0") + buck(v, 0.02) + "Bhi hi 0 V = time < 10m ? 1 : 0\n"
               "Sdrv v3v3 hold hi 0 SWDR\n" + hold_ic(v) + ".tran 200u 5 uic\n.end\n")
        t, d = run(net)
        off = next((t[i] - 0.01 for i in range(len(t)) if t[i] > 0.01 and d["pwren"][i] < 0.4), None)
        ok = off is not None and off <= MAX_OFF_S and d["pwren"][-1] < 0.4
        out.append((f"off VSYS={v}V VthN={n}V", ok, "never" if off is None else f"PWR_EN low after {off:.2f} s"))
    return out


def ride():
    out = []
    for c, float_s in ((C_LATCH_G1, RIDE_S), (C_LATCH_DERATED, RIDE_DERATED_S)):
        for v, n, p in CORNERS:
            net = (core(n, p, f"0 {v} 6 {v}", "0 0 6 0", c) + buck(v) + f"Bhi hi 0 V = (time < 10m || time > {0.01 + float_s}) ? 1 : 0\n"
                   "Sdrv v3v3 hold hi 0 SWDR\n" + hold_ic(v) + ".tran 200u 2 uic\n.end\n")
            _, d = run(net)
            out.append((f"ride {float_s * 1e3:.0f} ms C={c} VSYS={v}V", d["pwren"][-1] > ls.V_EN_HIGH, f"PWR_EN ends {d['pwren'][-1]:.2f} V"))
    return out


def clamped():
    out = []
    for v in (3.0, 4.2, 4.5):
        for crail in ("10u", "37u"):
            net = core(0.65, 0.6, f"0 0 10u {v} 1 {v}", "0 0 1 0") + clamped_rail(crail) + ".tran 20u 600m uic\n.end\n"
            _, d = run(net, ("v(latchg)", "v(pwren)", "i(Vsys)"))
            pk, drain = max(d["latchg"]), abs(d["i(Vsys)"][-1])
            ok = pk < MAX_GATE_OFF and max(d["pwren"]) < 0.4 and drain < MAX_OFF_DRAIN
            out.append((f"clamped VSYS={v}V rail {crail}", ok, f"peak gate {pk:.2f} V, drain {drain * 1e6:.0f} uA"))
    return out


def boot():
    out = []
    for v, n, p in CORNERS:
        sw = f"0 0 10m 0 10.01m 1 {0.01 + PRESS_S} 1 {0.0101 + PRESS_S} 0 4 0"
        net = core(n, p, f"0 {v} 4 {v}", sw) + buck(v) + firmware(FW_DELAY_S) + ".tran 100u 2.5 uic\n.end\n"
        _, d = run(net)
        out.append((f"boot VSYS={v}V VthN={n}V", d["pwren"][-1] > ls.V_EN_HIGH, f"PWR_EN ends {d['pwren'][-1]:.2f} V"))
    return out


def main():
    bad = 0
    for s in (insert, hotplug, press, power_off, ride, clamped, boot):
        for name, ok, note in s():
            bad += not ok
            print(f"{'PASS' if ok else 'FAIL'}  {name:30} {note}")
    print("\nall checks passed" if not bad else f"\n{bad} check(s) failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
