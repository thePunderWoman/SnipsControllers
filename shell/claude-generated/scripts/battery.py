"""Battery bay: cradle ribs and two contact bulkheads that stand on the rail blocks of the fixed bottom shell.
The cell drops in from below once the panel is out. Flat (+) contact at the palm end, spring (-) at the screen end.
Each bulkhead has a round window through a thin front lip, a slot behind it for a stamped contact plate (slid in from below)
and a solder-tab slot out the back. + and - are engraved on the underside of the tongues, so they read from below with the cell in.
"""
import numpy as np, trimesh
from shapely.geometry import Point, box as sbox
import params as P
import meshops as mo
import flaps
from flaps import M, box, cyl_z

CX = 17.5


def _cyl_y(x, z, y0, y1, r):
    c = trimesh.creation.cylinder(radius=r, height=y1 - y0, sections=48)
    c.apply_transform(trimesh.transformations.rotation_matrix(np.pi / 2, (1, 0, 0)))
    c.apply_translation((x, 0.5 * (y0 + y1), z))
    return M(c)


def _profile(full):
    az = P.BATT_AXIS_Z
    rin = P.BATT_D / 2 + P.BAY_CLEAR
    rout = rin + P.BAY_RIB_T
    zb, zt = P.BAY_ZB, az + rin + P.BAY_ROOF_T
    outer = Point(CX, az).buffer(rout, 64).union(sbox(CX - rout, zb, CX + rout, az))
    poly = outer
    if not full:
        inner = Point(CX, az).buffer(rin, 64).union(sbox(CX - rin, zb - 1, CX + rin, az))
        poly = outer.difference(inner)
    poly = poly.intersection(sbox(0, zb, 40, zt))
    stubs = sbox(7.0, zb, 9.0, az + 0.2).union(sbox(2 * CX - 9.0, zb, 2 * CX - 7.0, az + 0.2))
    return poly.union(stubs)


def _prism(poly, y0, y1):
    gs = list(poly.geoms) if hasattr(poly, 'geoms') else [poly]
    return mo.union([flaps.prism_xz(g, y0, y1) for g in gs])


def bay(pockets=()):
    """Returns dict(solid, plates, cell, bulk) in the shell frame."""
    az = P.BATT_AXIS_Z
    y_p, y_s = P.BATT_Y0, P.BATT_Y0 + P.BATT_HOLDER_L
    parts = []
    parts.append(_prism(_profile(False), y_p - 0.2, y_s + 0.2))      # one continuous shroud, bulkhead to bulkhead
    full = _profile(True)
    tx0, tx1, tzb = P.BAY_TONGUE
    tongue = sbox(tx0, tzb, tx1, P.BAY_ZB + 0.3)
    full = full.union(tongue)
    T, lip, slot = P.BAY_BULK_T, P.BAY_LIP_T, P.BAY_SLOT_T
    pw, ph, pt = P.BAY_PLATE
    zpl0, zpl1 = az - ph / 2, az + ph / 2
    slot_top = zpl1 + 0.3
    bulks, plates = [], []
    for name, face, sgn in (("palm", y_p, -1), ("screen", y_s, +1)):
        ya, yb = (face - T, face) if sgn < 0 else (face, face + T)
        b = _prism(full, ya, yb)
        # lip nearest the face, then the plate slot, then the back wall
        if sgn < 0:
            s0, s1 = face - lip - slot, face - lip
            w0, w1 = face - lip - 0.05, face + 0.1
            t0, t1 = ya - 0.1, s0 + 0.05
        else:
            s0, s1 = face + lip, face + lip + slot
            w0, w1 = face - 0.1, face + lip + 0.05
            t0, t1 = s1 - 0.05, yb + 0.1
        b = b - box(CX - (pw + 0.3) / 2, CX + (pw + 0.3) / 2, s0, s1, tzb - 0.1, slot_top)
        b = b - _cyl_y(CX, az, w0, w1, P.BAY_WINDOW_D / 2) - box(CX - P.BAY_WINDOW_D / 2, CX + P.BAY_WINDOW_D / 2, w0, w1, tzb - 0.1, az)   # keyhole: open at the bottom so a spring coil slides up with its plate
        b = b - box(CX - P.BAY_TAB_W / 2, CX + P.BAY_TAB_W / 2, t0, t1, tzb - 0.1, zpl0 + 1.2)
        # engraved polarity mark on the underside of the tongue, outside the slot opening (x > CX + 5.15 + 0.4)
        gx, gy = 0.5 * (CX + (pw + 0.3) / 2 + 0.4 + tx1), 0.5 * (ya + yb)
        zc0, zc1 = tzb - 0.1, tzb + 0.4
        if name == "palm":      # +
            b = b - box(gx - 0.9, gx + 0.9, gy - 0.3, gy + 0.3, zc0, zc1) - box(gx - 0.3, gx + 0.3, gy - 0.9, gy + 0.9, zc0, zc1)
        else:                   # -
            b = b - box(gx - 0.9, gx + 0.9, gy - 0.3, gy + 0.3, zc0, zc1)
        bulks.append(b)
        # the contact plate sits in the slot; the spring (screen end) or the cell nub (palm end) reaches through the window
        py0, py1 = (s0 + 0.075, s0 + 0.075 + pt) if sgn > 0 else (s1 - 0.075 - pt, s1 - 0.075)
        pl = box(CX - pw / 2, CX + pw / 2, py0, py1, zpl0, zpl1)
        if sgn > 0:             # spring coil projecting into the bay
            pl = pl + _cyl_y(CX, az, face - P.SPRING_PROJ, py0 + 0.01, 3.0)
        plates.append(pl)
    # cell
    cell = _cyl_y(CX, az, y_p, y_p + P.BATT_L, P.BATT_D / 2)
    solid = mo.union(parts + bulks)
    for e in pockets:       # board parts that dip into the roof get a pocket (0.25 mm clear all round)
        g = P.BAY_POCKET_GAP
        solid = solid - box(e['x0'] - g, e['x1'] + g, e['y0'] - g, e['y1'] + g, e['z0'] - g, e['z1'] + 1.0)
    return dict(solid=solid, plates=plates, cell=cell, bulks=bulks)


def pocket_boxes(env):
    """Board parts that reach into the roof zone (small parts only: not the stick, USB or screen boxes)."""
    return [e for e in env if e.get("name") is None and e["z0"] < -0.3 and e["x1"] > 7 and e["x0"] < 28
            and e["y1"] > 5 and e["y0"] < 64 and max(e["x1"] - e["x0"], e["y1"] - e["y0"]) < 20]
