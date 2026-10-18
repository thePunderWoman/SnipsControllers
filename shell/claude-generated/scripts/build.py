"""Outer skin: superellipse sections, solver-grown to clear the electronics, lofted.

usage: python -I build.py <pcb.step> <out_dir> [A B C]
"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from scipy.interpolate import PchipInterpolator
from scipy.ndimage import gaussian_filter1d, maximum_filter1d
from shapely.geometry import Polygon, Point
import params as P
from build123d import (loft, import_step, export_stl, export_step, export_brep, import_brep, Location,
                       Edge, Wire, Face)

CX = P.BOARD_W / 2


# ---------------------------------------------------------------- envelopes
def oled_boxes(n=8):
    th = math.radians(P.OLED_TILT_DEG)
    y_len = P.OLED_L * math.cos(th)
    boxes = []
    for k in range(n):
        ya = P.OLED_Y0 + y_len * k / n
        yb = P.OLED_Y0 + y_len * (k + 1) / n
        zl = lambda y: P.OLED_BASE_Z + (y - P.OLED_Y0) * math.tan(th)
        boxes.append(dict(idx=-2, name="oled", x0=(P.BOARD_W - P.OLED_W) / 2,
                          x1=(P.BOARD_W + P.OLED_W) / 2, y0=ya, y1=yb,
                          z0=zl(ya), z1=zl(yb) + P.OLED_T / math.cos(th)))
    return boxes


def pcb_envelopes(step_path):
    shape = import_step(step_path).moved(Location(P.STEP_SHIFT))
    out = []
    for i, s in enumerate(shape.solids()):
        b = s.bounding_box()
        out.append(dict(idx=i, x0=b.min.X, y0=b.min.Y, z0=b.min.Z,
                        x1=b.max.X, y1=b.max.Y, z1=b.max.Z))
    # the Hall sensor moves under the trigger magnet: its body sits at P.MAG_XY with the leads bent over from the board holes
    hx, hy = P.MAG_XY
    for e in list(out):
        if e.get("idx", -9) >= 0 and abs((e["x0"] + e["x1"]) / 2 - P.HALL_XY[0]) < 3.0 and abs((e["y0"] + e["y1"]) / 2 - P.HALL_XY[1]) < 3.0 and e["z0"] < -6.0:
            out.remove(e)
            out.append(dict(idx=e["idx"], x0=hx - 2.3, x1=hx + 2.3, y0=hy - 1.95, y1=hy + 1.95, z0=e["z0"], z1=e["z0"] + 4.6, moved_hall=True))
            out.append(dict(idx=e["idx"] + 1000, x0=hx, x1=P.HALL_XY[0] + 1.3, y0=92.6, y1=P.HALL_XY[1] + 3.0, z0=-2.8, z1=-0.1, moved_hall=True))
    out.append(dict(idx=-1, name="battery", x0=CX - P.BATT_D / 2, x1=CX + P.BATT_D / 2,
                    y0=P.BATT_Y0 - 1.5, y1=P.BATT_Y0 + P.BATT_HOLDER_L + 0.5,
                    z0=P.BATT_AXIS_Z - P.BATT_D / 2, z1=P.BATT_AXIS_Z + P.BATT_D / 2))
    out += oled_boxes()
    return out


def make_pod(z_floor=3.0, grow=None):
    """Screen pod: side profile of the tilted module grown by wall + clearance, filled down to the skin,
    extruded across the module width and rounded."""
    from build123d import Plane, Polygon, Pos, extrude, fillet
    from shapely.geometry import Polygon as SPoly, MultiPoint
    th = math.radians(P.OLED_TILT_DEG)
    g = P.WALL + P.CLEARANCE if grow is None else grow
    u, n = (math.cos(th), math.sin(th)), (-math.sin(th), math.cos(th))
    a = (P.OLED_Y0, P.OLED_BASE_Z)
    c = [a, (a[0] + P.OLED_L * u[0], a[1] + P.OLED_L * u[1])]
    c += [(p[0] + P.OLED_T * n[0], p[1] + P.OLED_T * n[1]) for p in c[::-1]]
    ring = SPoly(c).buffer(g, join_style=1)
    xs, zs = zip(*ring.exterior.coords)
    hull = MultiPoint(list(ring.exterior.coords) + [(min(xs), z_floor), (max(xs), z_floor)]).convex_hull
    pts = list(hull.exterior.coords)[:-1]
    width = P.OLED_W + 2 * g
    solid = extrude(Plane.YZ * Polygon(*pts, align=None), amount=width)
    solid = Pos(CX + width / 2, 0, 0) * solid
    for r in (P.OLED_POD_R, 4.0, 3.0, 2.0):
        try:
            return fillet(solid.edges(), r)
        except ValueError:
            continue
    return solid


def constraint_points(env, y):
    """(x_rel, z, required wall) for everything that lives near station y."""
    pts = []
    need = P.WALL + P.CLEARANCE + P.SOLVER_MARGIN
    lo, hi = y - P.SOLVER_WINDOW, y + P.SOLVER_WINDOW
    for e in env:
        if e["y1"] < lo or e["y0"] > hi:
            continue
        if e.get("name") == "oled":                # the pod carries the screen, not the skin
            continue
        if e.get("name") == "battery":
            for k in range(24):
                t = 2 * math.pi * k / 24
                pts.append((P.BATT_D / 2 * math.cos(t), P.BATT_AXIS_Z + P.BATT_D / 2 * math.sin(t),
                            P.WALL + P.BATT_CLEAR + P.SOLVER_MARGIN))
            continue
        if e.get("name") is None:
            if max(e["x1"] - e["x0"], e["y1"] - e["y0"], e["z1"] - e["z0"]) < 2.0:
                continue
            if e["z1"] > 14.0 or e["y0"] < -0.5:     # stick cap / USB-C exit through the skin
                continue
        for x in (e["x0"], e["x1"]):
            for z in (e["z0"], e["z1"]):
                pts.append((x - CX, z, need))
        if e.get("name") is None and e["z0"] == 0.0 and e["x1"] - e["x0"] > 30:
            for x in (e["x0"], e["x1"]):             # the top shell's lap lip runs down past the board edge: keep the body wide enough at the lip
                for z in (P.SEAM_Z - 1.0, P.SEAM_Z - P.LAP_H):
                    pts.append((x - CX, z, need - P.LAP_UNDER))
    return pts


# ---------------------------------------------------------------- sections
def se_points(w, zf, zb, nf, nb, eq, n=None):
    n = n or P.SECTION_POINTS
    zc = zb + eq * (zf - zb)
    bf, bb, a = zf - zc, zc - zb, w / 2
    pts = []
    for k in range(n):
        t = 2 * math.pi * k / n
        c, s = math.cos(t), math.sin(t)
        e, b = (nf, bf) if s >= 0 else (nb, bb)
        pts.append((a * math.copysign(abs(c) ** (2 / e), c),
                    zc + b * math.copysign(abs(s) ** (2 / e), s)))
    return pts


def worst_violation(w, zf, zb, nf, nb, eq, cps):
    """largest distance (mm) by which any constraint point pokes into its required wall"""
    poly = Polygon(se_points(w, zf, zb, nf, nb, eq))
    inner, worst = {}, 0.0
    for x, z, need in cps:
        if need not in inner:
            inner[need] = poly.buffer(-need)
        g = inner[need]
        p = Point(x, z)
        if g.is_empty:
            return 1e3
        if not g.buffer(1e-6).contains(p):
            worst = max(worst, g.distance(p))
    return worst


def solve_station(y, w, zf, zb, nf, nb, eq, env):
    cps = constraint_points(env, y)
    for _ in range(200):
        v = worst_violation(w, zf, zb, nf, nb, eq, cps)
        if v == 0.0:
            break
        moves = [(w + 1.0, zf, zb), (w, zf + 0.5, zb), (w, zf, zb - 0.5)]
        scored = [(worst_violation(*m, nf, nb, eq, cps), i) for i, m in enumerate(moves)]
        best_v, best_i = min(scored)
        if best_v >= v:                      # nothing helps on its own; widen and retry
            best_i = 0
        w, zf, zb = moves[best_i]
    return w, zf, zb


def stations(shp, env):
    """rows of [y, w, zf, zb, nf, nb]"""
    t = np.array([list(r) + [shp["nf"], shp["nb"]][len(r) - 4:] for r in shp["table"]], dtype=float)
    fs = [PchipInterpolator(t[:, 0], t[:, k]) for k in range(1, 6)]
    ys = np.unique(np.concatenate([np.arange(t[0, 0], t[-1, 0], P.SECTION_STEP), [t[-1, 0]]]))
    rows = [[float(y)] + [float(f(y)) for f in fs] for y in ys]
    eq = shp["eq"]

    def solve(rows):
        return [[r[0], *solve_station(r[0], r[1], r[2], r[3], r[4], r[5], eq, env), r[4], r[5]]
                for r in rows]

    rows = solve(rows)                       # 1. what the electronics require, station by station
    a = np.array(rows)
    for k, sgn in ((1, 1), (2, 1), (3, -1)):  # 2. dilate then blur -> smooth, never below need (re-solved below)
        col = sgn * a[:, k]
        sm = gaussian_filter1d(maximum_filter1d(col, size=2 * P.SMOOTH_MAX_R + 1, mode="nearest"),
                               P.SMOOTH_SIGMA, mode="nearest")
        col[2:-2] = sm[2:-2]                  # keep the dome ends as authored
        a[:, k] = sgn * col
    return solve(a.tolist())                 # 3. guarantee clearance


def section_face(y, w, zf, zb, nf, nb, eq):
    pts = [(CX + x, y, z) for x, z in se_points(w, zf, zb, nf, nb, eq)]
    return Face(Wire([Edge.make_spline(pts, periodic=True)]))



def tip_fix(rows, fs):
    """palm end = one smooth rounded cap: for y<0 the section follows a power-law blend from the tip section to the y=0 section"""
    b0 = [float(f(0.0)) for f in fs]
    out = []
    for r in rows:
        y = r[0]
        if y < 0:
            u = (y - P.TIP_Y) / (0 - P.TIP_Y)
            k = 1 - (1 - u) ** P.TIP_POW
            w = P.TIP_W + (b0[0] - P.TIP_W) * k
            zf = b0[1] * (P.TIP_ZF + (1 - P.TIP_ZF) * k)
            zb = b0[2] * (P.TIP_ZB + (1 - P.TIP_ZB) * k)
            r = [y, w, zf, zb, b0[3], b0[4]]
        out.append(r)
    return out

def dense(rows):
    """shape-preserving (no overshoot) resample of the solved stations for lofting"""
    a = np.array(rows)
    fs = [PchipInterpolator(a[:, 0], a[:, k]) for k in range(1, 6)]
    y0, y1 = a[0, 0], a[-1, 0]
    ys = np.concatenate([np.arange(y0, y1, P.LOFT_STEP), [y1],
                         np.arange(y0, y0 + P.END_ZONE, P.END_STEP),          # fine sampling in the end domes
                         np.arange(y0, 0.0, P.TIP_STEP),
                         np.arange(y1 - P.END_ZONE, y1, P.END_STEP)])
    ys = np.unique(np.round(ys, 3))
    ys = np.unique(np.round(np.concatenate([np.arange(P.TIP_Y, 0.0, P.TIP_STEP), [0.0], ys[ys > 0.0]]), 3))     # the cap uses its own evenly spaced sections only
    return tip_fix([[float(y)] + [float(f(y)) for f in fs] for y in ys], fs)


def inner_rows(rows, wall=None):
    """Sections of the cavity. Each profile curve (half-width, front height, back height versus y) is offset
    inward by `wall` as a true parallel curve (shapely buffer of the profile region), so the wall stays `wall`
    thick even where the end domes are steep. Sections where the cavity would be too small are dropped."""
    from shapely.geometry import LineString
    wall = P.WALL if wall is None else wall
    a = np.array(rows)
    y = a[:, 0]

    def region(lo, hi):                          # polygon between two curves of (y, value)
        return Polygon(list(zip(y, lo)) + list(zip(y[::-1], hi[::-1]))).buffer(-wall, join_style=1)

    big = 200.0
    reg_w = region(-a[:, 1] / 2, a[:, 1] / 2)
    reg_f = region(np.full_like(y, -big), a[:, 2])
    reg_b = region(a[:, 3], np.full_like(y, big))

    def span(reg, yy):
        seg = reg.intersection(LineString([(yy, -big), (yy, big)]))
        if seg.is_empty:
            return None
        b_ = seg.bounds
        return b_[1], b_[3]

    out = []
    for i, r in enumerate(a):
        sw, sf, sb = span(reg_w, y[i]), span(reg_f, y[i]), span(reg_b, y[i])
        if sw is None or sf is None or sb is None:
            continue
        w = sw[1] - sw[0]
        zf, zb = sf[1], sb[0]
        if w < 8.0 or zf - zb < 6.0:
            continue
        out.append([r[0], w, zf, zb, r[4], r[5]])
    return out


def hollow_shell(shp, rows):
    """Thin shell without lofted-vs-lofted booleans (those produce invalid solids).

    1. tube   = outer loft + reversed inner loft joined as one solid with a cavity
    2. A      = tube minus the pod cavity (opens the top wall under the screen pod)
    3. cap    = (pod minus pod cavity) minus the skin (the wedge walls that stand proud of the body)
    4. result = A + cap
    Uses the coarse section set (3 mm, 1 mm in the end domes), enough for smooth surfaces.
    """
    from build123d import Solid
    from OCP.BRepBuilderAPI import BRepBuilderAPI_MakeSolid
    from OCP.TopoDS import TopoDS
    d = coarse(rows)
    skin = loft([section_face(*r, shp["eq"]) for r in d])
    inner = loft([section_face(*r, shp["eq"]) for r in inner_rows(d)])
    ms = BRepBuilderAPI_MakeSolid()
    ms.Add(TopoDS.Shell(skin.shell().wrapped))
    ms.Add(TopoDS.Shell(inner.shell().wrapped.Reversed()))
    tube = Solid(ms.Solid())
    pod, ipod = make_pod(), make_pod(grow=P.CLEARANCE)
    return (tube - ipod) + ((pod - ipod) - skin)


def coarse(rows, step=3.0, end_zone=9.0, end_step=1.0):
    a = np.array(rows)
    fs = [PchipInterpolator(a[:, 0], a[:, k]) for k in range(1, 6)]
    y0, y1 = a[0, 0], a[-1, 0]
    ys = np.concatenate([np.arange(y0, y1, step), [y1], np.arange(y0, y0 + end_zone, end_step),
                         np.arange(y0, 0.0, 0.4),
                         np.arange(y1 - end_zone, y1, end_step)])
    ys = np.unique(np.round(ys, 3))
    ys = np.unique(np.round(np.concatenate([np.arange(P.TIP_Y, 0.0, 0.4), [0.0], ys[ys > 0.0]]), 3))     # the cap uses its own evenly spaced sections only
    return tip_fix([[float(y)] + [float(f(y)) for f in fs] for y in ys], fs)


def build_skin(shp, env):
    rows = stations(shp, env)
    return loft([section_face(*r, shp["eq"]) for r in dense(rows)]), rows


if __name__ == "__main__":
    step_path, out_dir = sys.argv[1], sys.argv[2]
    keys = sys.argv[3:] or list(P.SHAPES)
    env = pcb_envelopes(step_path)
    os.makedirs(out_dir, exist_ok=True)
    json.dump(env, open(os.path.join(out_dir, "envelopes.json"), "w"))
    for k in keys:
        shp = P.SHAPES[k]
        d = os.path.join(out_dir, k)
        os.makedirs(d, exist_ok=True)
        rows = stations(shp, env)                    # solved sections; the solid comes from hollow_shell()
        cache = os.path.join(d, "hollow.brep")        # REUSE_HOLLOW=1 skips the 5-minute hollowing when only the interior changed
        if os.environ.get("REUSE_HOLLOW") and os.path.exists(cache):
            skin = import_brep(cache)
        else:
            skin = hollow_shell(shp, rows)
            export_brep(skin, cache)
        print("hollow shell vol %.0f valid %s" % (skin.volume, skin.is_valid), flush=True)
        import interior, cutouts, split, meshops as mo
        import time
        t0 = time.time()
        lap = lambda msg: print("%6.1fs %s" % (time.time() - t0, msg), flush=True)
        probe = cutouts.Probe(skin)                  # wall positions for the openings, measured on the hollow shell
        top, bot, info = interior.features(env)
        otop, obot = interior.opening_features()
        top, bot = top + otop, bot + obot
        H = mo.tess(skin); lap("hollow shell meshed")
        lofts = lambda w: mo.tess(loft([section_face(*r, shp["eq"]) for r in inner_rows(coarse(rows), wall=w)]))
        inset, inset2 = lofts(P.EMBED), lofts(1.2)        # top features embed 0.8 into the wall; bottom ones start 1.2 in (below the lap)
        cavity = lofts(P.WALL + P.TAB_GAP)
        import trimesh
        a = P.LAP_UNDER
        il_under, il_lip, il_sub = mo.cavity_offsets(mo.to_trimesh(H), (a, a + P.LAP_GAP, a - 0.05))
        lap("insets + cavity + lap lofts meshed")
        top_f = mo.tess(interior.fuse(top)) ^ inset                      # ^ is intersection in manifold3d
        fb = mo.tess(interior.fuse(bot))
        low_f, tabs = mo.below(fb ^ inset2, P.SEAM_Z), mo.above(fb ^ cavity, P.SEAM_Z)   # ribs above the seam: free tabs in the cavity
        import flaps as _fl
        far = lambda y0: _fl.box(-30, 70, y0, 200, -40, 40)                                 # everything beyond the palm-end butt zone
        low_f = low_f - ((mo.above(mo.below(mo.box_mesh(), P.SEAM_Z + 0.5), P.SEAM_Z - P.LAP_H - P.LAP_FLOOR - 1.0) - il_sub) ^ far(P.PALM_BUTT_Y))                                        # nothing of the bottom shell in the step zone
        # half-lap: the top shell's outer skin reaches LAP_H below the seam; the bottom keeps only its inner skin there
        lip = (mo.above(mo.below(H, P.SEAM_Z), P.SEAM_Z - P.LAP_H) - il_lip) ^ far(P.PALM_BUTT_Y + 0.05)
        step = (mo.above(mo.below(mo.box_mesh(), P.SEAM_Z + 0.01), P.SEAM_Z - P.LAP_H - P.LAP_FLOOR) - il_under) ^ far(P.PALM_BUTT_Y)      # a plain box minus the cavity offset: shares no faces with H
        top_m = mo.above(H, P.SEAM_Z) + lip + top_f
        bot_m = (mo.below(H, P.SEAM_Z) - step) + low_f + tabs
        # hidden snap clips: ledge on the top wall's inner face, arm rising from the bottom shell
        clip_info = []
        for site in split.clip_sites(info):
            arm, ledge, ci = split.clip_pair(site, probe)
            bot_m = bot_m + (arm ^ inset2)
            top_m = top_m + (ledge ^ inset)
            clip_info.append(ci)
        lap("seam, tabs and %d clips added" % len(clip_info))
        import panel as pnl
        for cut in cutouts.bottom_cutters():
            bot_m = bot_m - mo.tess(cut)
        lap('pre-panel bottom watertight=%s' % mo.clean(bot_m).is_watertight)
        import flaps
        trig = flaps.trigger(mo.to_trimesh(H))
        O_in = mo.outer_offset(mo.to_trimesh(H), -0.6)
        bot_m = (bot_m - trig["cut"]) + mo.union([f ^ O_in for f in trig["fins"]])     # window first, then the hinge fins (they merge into the wall)
        top_m = top_m - trig["cut"]
        bump = flaps.bumper(mo.to_trimesh(H))
        top_m = (top_m - bump["cut"]) + mo.union(bump["fins"])
        mo.clean(bump['bar']).export(os.path.join(d, 'bumper.stl'))
        bot_m, panel_m = pnl.split_panel(bot_m, inset2)
        import battery
        _pk = battery.pocket_boxes(env)
        bay = battery.bay(_pk)
        bot_m = bot_m if os.environ.get('SKIP_BAY') else bot_m + bay['solid']
        for nm, key_ in (('bay_cell', 'cell'),):
            mo.clean(bay[key_]).export(os.path.join(d, nm + '.stl'))
        mo.clean(mo.union(bay['plates'])).export(os.path.join(d, 'bay_contacts.stl'))
        cuts, info_t = cutouts.top_cutters(probe)
        for cut in cuts:
            top_m = top_m - mo.tess(cut)
        lap("shells assembled: top vol %.0f  bottom vol %.0f  overlap %.2f" % (top_m.volume(), bot_m.volume(), (top_m ^ bot_m).volume() + (bot_m ^ panel_m).volume() + (top_m ^ panel_m).volume()))
        info_t = info_t + clip_info
        json.dump(info + info_t, open(os.path.join(d, "features.json"), "w"))
        import trimesh
        mo.clean(trig['rigid']).export(os.path.join(d, 'trigger.stl')); mo.clean(trig['hook']).export(os.path.join(d, 'trigger_hook.stl')); mo.clean(trig['frame']).export(os.path.join(d, 'trigger_frame.stl'))
        top_m = top_m ^ _fl.box(-30, 70, P.TIP_Y + 0.1, 200, -40, 40)      # nothing pokes out past the rounded palm tip (USB sleeve was built for the old tip)
        tm, bm, pm = mo.clean(top_m, tol=float(os.environ.get('TTOL', 0.02))), mo.clean(bot_m, tol=float(os.environ.get('BTOL', 0.02))), mo.clean(panel_m)
        raw = mo.to_trimesh(bot_m); lap('raw bottom watertight=%s' % raw.is_watertight)
        tm.export(os.path.join(d, "top.stl")); bm.export(os.path.join(d, "bottom.stl")); pm.export(os.path.join(d, "panel.stl"))
        trimesh.util.concatenate([tm, bm, pm]).export(os.path.join(d, "skin.stl"))     # both shells, assembled
        print("%s %-18s top watertight=%s bottom watertight=%s  Z %.1f..%.1f  W %.1f  L %.1f" % (
            k, shp["name"], tm.is_watertight, bm.is_watertight, bm.bounds[0][2], tm.bounds[1][2],
            max(tm.extents[0], bm.extents[0]), max(tm.extents[1], bm.extents[1])), flush=True)
        json.dump(rows, open(os.path.join(d, "stations.json"), "w"))
        continue
        bb = skin.bounding_box()
        print("%s %-18s valid=%s vol=%.0f  W %.1f  L %.1f  T %.1f  (Z %.1f..%.1f)" %
              (k, shp["name"], skin.is_valid, skin.volume, bb.size.X, bb.size.Y, bb.size.Z,
               bb.min.Z, bb.max.Z))
        export_stl(skin, os.path.join(d, "skin.stl"), tolerance=0.1, angular_tolerance=0.15)   # both shells, assembled
        if os.environ.get("SHELL_STEP"):             # STEP is slow and large; opt in with SHELL_STEP=1
            export_step(skin, os.path.join(d, "skin.step"))
        json.dump(rows, open(os.path.join(d, "stations.json"), "w"))
