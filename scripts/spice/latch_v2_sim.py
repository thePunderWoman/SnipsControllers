#!/usr/bin/env python3
"""ngspice check of the V2 soft latch across every state it has to survive.

PWR_HOLD is on RXD0, whose pull-up holds the latch through any reset and upload with no firmware help. That
same pull-up comes back as the rail collapses at power-off, and the gate is sitting right at threshold then, so
a plain latch can restart itself. C_LATCH_FB1, from PWR_EN to the gate, gives the latch hysteresis: as PWR_EN
falls it kicks the gate down, so the returning pull-up has real ground to make up before the rail is gone.

  insert    battery plugged in with the board off: the gate stays low
  bounce    the battery contact chatters with the board off: the board stays off
  hotplug   USB raises VSYS with the board off: the gate stays low
  clamped   battery insertion with the pad ESD diodes into the dead rail: the gate stays low, the drain is bounded
  bump      a 20 ms knock on the button: the board stays off
  press     a held button turns PWR_EN on, across supply and FET-threshold corners
  boot      a 1 s press stays on from the pull-up alone: no firmware is running
  upload    the pull-up holds the latch for as long as a reset or upload lasts
  unplug    USB pulled while on: VSYS drops, the kick must not turn the board off
  off       firmware drops PWR_HOLD, then the pull-up returns at a sweep of brownout levels: it stays off
  offheld   the same with the button still held: off comes on release
  ratio     the same as off at both extremes of the two capacitors' ratio

Reuses the models and solver wrapper in latch_sim.py. The netlist mirrors PCB/v2.x/snips/stackup/board.kdl by
hand, so update it when those values or that topology change. See README.md for what is not modeled.

    python3 scripts/spice/latch_v2_sim.py          # exit 1 on any failure
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import latch_sim as ls

R_INV_BASE1 = R_INV_PU1 = "220k"
R_PWR_EN_PD1 = "22k"
C_LATCH_G1 = "1u"
C_LATCH_FB1 = "1u"
C_FB_LOW = "600n"     # estimate: a 50 V X5R 0603 at 3.5 V bias and -10%, the case with the least kick
C_FB_HIGH = "1.2u"    # estimate: +10% and a little over, the case with the most kick
C_RAIL = "48u"        # V2 nominal with the XBee carrier: a heavier rail is the harder case at power-off
C_LATCH_LOW = "0.55u" # estimate: -10% tolerance and about -40% for a 6.3 V 0402 X5R at 2.8 V bias
CORNERS = ((3.0, 1.45, 1.3), (3.7, 1.05, 0.9), (4.2, 0.65, 0.6), (4.5, 0.65, 0.6))
WPU = ("30k", "100k")     # the datasheet gives only 45k typical, so this is a deliberately wide spread
BROWNOUTS = (2.8, 2.4, 2.0)  # rail level at which the chip resets and the pull-up returns

MAX_GATE_OFF = 0.5    # V, well under the AO3400A's 0.65 V minimum threshold
MAX_PRESS_S = 0.6     # button press to PWR_EN on
MAX_OFF_S = 4.0       # PWR_HOLD low to PWR_EN low
MAX_OFF_DRAIN = 60e-6 # A from VSYS with the board off, resistors only: no IC quiescent current
BOOT_PRESS_S = 1.0    # press the board must accept as a normal turn-on
BUMP_S = 0.02         # knock on the button that must not turn the board on


def core(vthn, vthp, vsys_pwl, sw_pwl, c_latch=C_LATCH_G1, c_fb=C_LATCH_FB1):
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
Cfb pwren latchg {c_fb}
Mlatch pwrgate latchg 0 0 NAO3400 W=1 L=1
Cgs1 latchg 0 550p
Cgd1 latchg pwrgate 75p
Rgate vsys pwrgate {ls.R_GATE1}
Mpwr pwren pwrgate vsys vsys PAO3401 W=1 L=1
Cgs2 pwrgate vsys 500p
Rpd pwren 0 {R_PWR_EN_PD1}
Cen pwren 0 20p
"""


def buck(vsys, load_a=0.06, c=C_RAIL):
    v = min(3.32, vsys - 0.05)
    return f"V33 bk 0 {v}\nSbk bk v3v3 pwren 0 SWBK\nCrail v3v3 0 {c}\nRload v3v3 0 {v / load_a}\n"


def dead_rail(wpu="45k"):
    return f"R3 v3v3 0 300\nC3 v3v3 0 1u\nRw v3v3 hold {wpu}\n"


def clamped_rail(crail):
    """The dead rail with the ESD diodes of IO21, RXD0 and IO10 into it, and the VSYS divider on IO10."""
    return (f"Rv1 vsys io10 200k\nRv2 io10 0 100k\nCv io10 0 100n\nDio10 io10 v3v3 DESD\nDio21 sense v3v3 DESD\n"
            f"Dio44 hold v3v3 DESD\n.model DESD D(IS=1e-14 N=1.05)\nR3 v3v3 0 311k\nC3 v3v3 0 {crail}\nRw v3v3 hold 45k\n")


def shutdown(bod, wpu):
    """Firmware pulls PWR_HOLD low at 10 ms until the rail reaches bod, when the chip resets and its pull-up returns."""
    return (f"Bf 0 flag I = (v(v3v3) < {bod} && time > 10m) ? 1m : 0\nCf flag 0 1u\nRf flag 0 1G\n"
            "Blo lo 0 V = (time >= 10m && v(flag) < 0.5) ? 1 : 0\nSlow hold 0 lo 0 SWDR\n"
            f"Rw v3v3 hold {wpu}\n")


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


def hold_ic(v):
    gate = min(v - 0.7, 2.8)
    return f".ic v(latchg)={gate} v(v3v3)=3.3 v(sense)=3.3 v(pwren)={v} v(pwrgate)=0 v(base)=0.72 v(trig)=0 v(hold)=3.3\n"


def insert():
    out = []
    for (v, n, p), rise in zip(CORNERS, ("1m", "100u", "10u", "10u")):
        _, d = run(core(n, p, f"0 0 {rise} {v} 1 {v}", "0 0 1 0") + dead_rail() + ".tran 5u 400m uic\n.end\n")
        pk = max(d["latchg"])
        out.append((f"insert VSYS={v}V", pk < MAX_GATE_OFF, f"peak gate {pk:.2f} V"))
    return out


def bounce():
    out = []
    for v, n, p in CORNERS:
        steps, t = ["0 0"], 0.0
        for i in range(8):
            t += 0.001
            steps += [f"{t:.4f} {v if i % 2 == 0 else 0}", f"{t + 1e-5:.5f} {0 if i % 2 == 0 else v}"]
        steps.append(f"1 {0 if len(steps) % 2 == 0 else v}")
        _, d = run(core(n, p, " ".join(steps), "0 0 1 0") + dead_rail() + ".tran 5u 300m uic\n.end\n")
        pk = max(d["latchg"])
        out.append((f"bounce VSYS={v}V", pk < MAX_GATE_OFF and max(d["pwren"]) < 0.4, f"peak gate {pk:.2f} V"))
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


def bump():
    out = []
    for v, n, p in CORNERS:
        sw = f"0 0 10m 0 10.01m 1 {0.01 + BUMP_S} 1 {0.0101 + BUMP_S} 0 2 0"
        net = core(n, p, f"0 {v} 2 {v}", sw) + buck(v) + "Rw v3v3 hold 45k\n.tran 100u 1 uic\n.end\n"
        _, d = run(net)
        out.append((f"bump VSYS={v}V VthN={n}V", d["pwren"][-1] < 0.4, f"PWR_EN ends {d['pwren'][-1]:.2f} V"))
    return out


def press():
    out = []
    for v, n, p in CORNERS:
        net = core(n, p, f"0 {v} 4 {v}", "0 0 10m 0 10.01m 1 1.9 1") + buck(v) + "Rw v3v3 hold 100k\n.tran 100u 2 uic\n.end\n"
        t, d = run(net)
        on = next((t[i] - 0.01 for i in range(len(t)) if t[i] > 0.01 and d["pwren"][i] > ls.V_EN_HIGH), None)
        out.append((f"press VSYS={v}V VthN={n}V", on is not None and on <= MAX_PRESS_S,
                    "never" if on is None else f"PWR_EN on after {on * 1e3:.0f} ms"))
    return out


def boot():
    out = []
    for v, n, p in CORNERS:
        sw = f"0 0 10m 0 10.01m 1 {0.01 + BOOT_PRESS_S} 1 {0.0101 + BOOT_PRESS_S} 0 4 0"
        net = core(n, p, f"0 {v} 4 {v}", sw) + buck(v) + "Rw v3v3 hold 100k\n.tran 100u 3 uic\n.end\n"
        _, d = run(net)
        out.append((f"boot VSYS={v}V VthN={n}V", d["pwren"][-1] > ls.V_EN_HIGH, f"PWR_EN ends {d['pwren'][-1]:.2f} V"))
    return out


def upload():
    out = []
    for v, n, p in CORNERS:
        net = core(n, p, f"0 {v} 6 {v}", "0 0 6 0") + buck(v) + "Rw v3v3 hold 100k\n" + hold_ic(v) + ".tran 500u 5 uic\n.end\n"
        _, d = run(net)
        out.append((f"upload VSYS={v}V", d["pwren"][-1] > ls.V_EN_HIGH and min(d["v3v3"]) > 2.8, f"gate ends {d['latchg'][-1]:.2f} V"))
    return out


def unplug():
    out = []
    for b in (3.0, 3.7):
        pwl = f"0 4.5 20m 4.5 20.05m {b} 1 {b}"
        net = core(1.45, 1.3, pwl, "0 0 1 0") + buck(4.5) + "Rw v3v3 hold 100k\n" + hold_ic(4.5) + ".tran 20u 300m uic\n.end\n"
        _, d = run(net)
        out.append((f"unplug 4.5->{b}V", d["pwren"][-1] > ls.V_EN_HIGH, f"gate low point {min(d['latchg']):.2f} V"))
    return out


def stays_off(v, n, p, load, wpu, bod, c_latch=C_LATCH_G1, c_fb=C_LATCH_FB1, sw="0 0 6 0"):
    net = core(n, p, f"0 {v} 6 {v}", sw, c_latch, c_fb) + buck(v, load) + shutdown(bod, wpu) + hold_ic(v) + ".tran 200u 5 uic\n.end\n"
    t, d = run(net)
    fell = next((t[i] for i in range(len(t)) if t[i] > 0.01 and d["pwren"][i] < 0.4), None)
    tail = [g for g, tt in zip(d["latchg"], t) if fell is not None and tt > fell]
    return fell, d["pwren"][-1] < 0.4, max(tail) if tail else None


def power_off():
    out = []
    for v, n, p in CORNERS[:3]:
        worst, slowest, ok = -9.0, 0.0, True
        for load in (0.005, 0.02):
            for wpu in WPU:
                for bod in BROWNOUTS:
                    fell, low, pk = stays_off(v, n, p, load, wpu, bod, c_fb=C_FB_LOW)
                    ok = ok and fell is not None and low and fell - 0.01 <= MAX_OFF_S and pk < n
                    worst, slowest = max(worst, pk if pk is not None else 9), max(slowest, fell or 9)
        out.append((f"off VSYS={v}V VthN={n}V", ok, f"peak gate {worst:.2f} V, off by {slowest:.2f} s"))
    return out


def offheld():
    out = []
    for v, n, p in CORNERS[:3]:
        # The button is held through shutdown, released at 1 s.
        fell, low, pk = stays_off(v, n, p, 0.02, "45k", 2.4, sw="0 1 1 1 1.01 0 6 0")
        ok = fell is not None and low and fell > 1.0 and pk < n
        out.append((f"offheld VSYS={v}V", ok, "never" if fell is None else f"off at {fell:.2f} s, after release"))
    return out


def ratio():
    out = []
    for name, c_latch, c_fb in (("least kick", "1.1u", C_FB_LOW), ("most kick", C_LATCH_LOW, C_FB_HIGH)):
        for v, n, p in CORNERS[:3]:
            fell, low, pk = stays_off(v, n, p, 0.005, "30k", 2.8, c_latch, c_fb)
            ok = fell is not None and low and pk < n
            out.append((f"ratio {name} VSYS={v}V", ok, "never" if fell is None else f"peak gate {pk:.2f} V, off at {fell:.2f} s"))
    return out


def main():
    bad = 0
    for s in (insert, bounce, hotplug, clamped, bump, press, boot, upload, unplug, power_off, offheld, ratio):
        for name, ok, note in s():
            bad += not ok
            print(f"{'PASS' if ok else 'FAIL'}  {name:36} {note}", flush=True)
    print("\nall checks passed" if not bad else f"\n{bad} check(s) failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
