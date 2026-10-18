"""Printed button caps for every actuated opening: 6 face caps, 4 side caps, the power cone cap.

    python -I caps.py out/C          # reads features.json, hollow.brep, top.stl, envelopes from out/C, writes out/C/caps/*.stl

Every cap is built in a print frame: +z runs from the head's outer face (z = 0, on the bed) into the shell, so each
prints head-down with only a ~0.8 mm flange overhang and no supports. Built from the same numbers as the cutters.
Mesh booleans only (manifold3d).
"""
import json, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import trimesh
import params as P
import meshops as mo


def M(mesh):
    return mo.from_trimesh(mesh)


def revolve(profile, sections=64):
    """Solid of revolution about +z from (r, z) points, closed through the axis."""
    pts = [(0.0, profile[0][1])] + [tuple(p) for p in profile] + [(0.0, profile[-1][1])]
    return trimesh.creation.revolve(np.array(pts, float), sections=sections)


def cyl(p0, p1, r, sections=48):
    a, b = np.asarray(p0, float), np.asarray(p1, float)
    v = b - a
    c = trimesh.creation.cylinder(radius=r, height=float(np.linalg.norm(v)), sections=sections)
    c.apply_transform(trimesh.geometry.align_vectors([0, 0, 1], v / np.linalg.norm(v)))
    c.apply_translation((a + b) / 2)
    return M(c)


def frame(origin, ex, ez):
    """4x4 local -> world: local x -> ex, local z -> ez, local y = ez x ex."""
    ex, ez = np.asarray(ex, float), np.asarray(ez, float)
    ey = np.cross(ez, ex)
    T = np.eye(4)
    T[:3, 0], T[:3, 1], T[:3, 2], T[:3, 3] = ex, ey, ez, origin
    return T


def face_cap(inner_z, outer_z):
    """Print frame: head apex at z = 0, body and flange above it, plunger last. Returns (mesh, info)."""
    zt = outer_z + P.CAP_PROUD                       # world z of the apex
    zf1 = inner_z - P.CAP_FLANGE_GAP
    zf0 = zf1 - P.CAP_FLANGE_T
    r, rf, rp = P.FACE_CAP_D / 2, P.FACE_FLANGE_D / 2, P.FACE_PLUNGER_D / 2
    R = P.CAP_DOME_R
    dome = [(rho, zt - (R - math.sqrt(R * R - rho * rho))) for rho in np.linspace(r, 0.0, 9)]   # edge -> apex
    prof = [(rp, P.FACE_SWITCH_TOP + 0.02), (rp, zf0), (rf, zf0), (rf, zf1), (r, zf1)] + dome
    m = revolve(prof)
    m.apply_transform(np.diag([1, 1, -1, 1]) @ trimesh.transformations.translation_matrix((0, 0, -zt)))   # flip: head down, apex at z=0
    m.invert() if m.volume < 0 else None
    return m, dict(length=round(zt - P.FACE_SWITCH_TOP, 2), flange_z=(round(zf0, 2), round(zf1, 2)), apex_z=round(zt, 2))


def stadium_prism(w, h, L):
    """Stadium (w along x, h along y, h >= w), extruded along +z from 0 to L."""
    parts = [M(trimesh.creation.box(extents=(w, h - w, L), transform=trimesh.transformations.translation_matrix((0, 0, L / 2))))]
    for sy in (-1, 1):
        c = trimesh.creation.cylinder(radius=w / 2, height=L, sections=48)
        c.apply_translation((0, sy * (h - w) / 2, L / 2))
        parts.append(M(c))
    return mo.union(parts)


def side_cap(f):
    """Print frame: z = distance from the head's outer face toward the switch; local x = world y, local y = world -side*z."""
    side = f["side"]
    front = f["outer_x"] + side * P.SIDE_HEAD_PROUD
    L_head = abs(front - f["floor_x"]) - 0.05          # head back stops a hair short of the pocket floor
    L_tip = abs(front - f["tip_x"])
    L_f0 = abs(front - f["inner_x"]) + P.CAP_FLANGE_GAP
    hy, hz = P.SIDE_HEAD
    head = stadium_prism(hy, hz, L_head)
    neck = cyl((0, 0, L_head - 0.2), (0, 0, L_tip), P.SIDE_NECK_D / 2)
    flange = cyl((0, 0, L_f0), (0, 0, L_f0 + P.CAP_FLANGE_T), P.SIDE_FLANGE_D / 2, 64)
    return mo.to_trimesh(mo.union([head, neck, flange])), dict(length=round(L_tip, 2), head=round(L_head, 2), flange=round(L_f0, 2))


def power_cap(f):
    """Print frame: z = distance from the head's outer face (+y world) into the shell; x = world x; neck leans toward the switch."""
    y_front = f["outer_y"] - P.SIDE_HEAD_PROUD
    L_head = f["floor_y"] - y_front
    hx = f["head_x"]
    T = np.array(f["tip"])
    tip_l = np.array([T[0] - hx, 0.0, T[1] - y_front])
    H = np.array([0.0, 0.0, L_head])
    d = (tip_l - H) / np.linalg.norm(tip_l - H)
    head = cyl((0, 0, 0), (0, 0, L_head), P.PWR_HEAD_D / 2)
    z_tip = T[1] - y_front - 0.05                     # the neck ends in a flat face parallel to the switch's actuator face
    neck = cyl(H - d * 0.2, H + d * ((z_tip - H[2]) / d[2] + 3.0), P.PWR_NECK_D / 2)
    neck = neck ^ M(trimesh.creation.box(extents=(40, 40, z_tip + 5), transform=trimesh.transformations.translation_matrix((0, 0, (z_tip - 5) / 2))))
    Lf0 = (f["inner_y"] - y_front) + P.CAP_FLANGE_GAP
    t = (Lf0 - H[2]) / d[2]
    c = H + d * t
    flange = cyl((c[0], 0, Lf0), (c[0], 0, Lf0 + P.CAP_FLANGE_T), P.PWR_FLANGE_D / 2, 64)
    return mo.to_trimesh(mo.union([head, neck, flange])), dict(length=round(float(np.linalg.norm(tip_l - H)), 2), flange_x=round(float(c[0]), 2))


if __name__ == "__main__":
    out = sys.argv[1]
    feats = json.load(open(os.path.join(out, "features.json")))
    from build123d import import_brep
    import cutouts
    pr = cutouts.Probe(import_brep(os.path.join(out, "hollow.brep")))
    os.makedirs(os.path.join(out, "caps"), exist_ok=True)
    caps = {}

    def ring(cx, cy, cz, r, axis, n=12):
        """Inner-wall crossing distances sampled on a circle of radius r around (cx, cy, cz), rays along `axis` (from far outside)."""
        res = []
        for a in np.linspace(0, 2 * math.pi, n, endpoint=False):
            if axis == "z":
                o = (cx + r * math.cos(a), cy + r * math.sin(a), 40.0); d = (0, 0, -1)
            elif axis == "x":
                o = (cx + 30.0 * ring.side, cy + r * math.cos(a), cz + r * math.sin(a)); d = (-ring.side, 0, 0)
            else:
                o = (cx + r * math.cos(a), -30.0, cz + r * math.sin(a)); d = (0, 1, 0)
            w = pr.walls(o, d)
            res.append(o[0] + d[0] * w[1] if axis == "x" else (o[1] + d[1] * w[1] if axis == "y" else o[2] + d[2] * w[1]))
        return res
    k = 0
    for x in P.MACRO_XS:
        for y in P.MACRO_YS:
            w = pr.walls((x, y, 40.0), (0, 0, -1))
            outer = 40.0 - w[0]
            inner = min(ring(x, y, 0, P.FACE_FLANGE_D / 2 * 0.98, "z"))          # lowest inner-wall point under the flange
            m, info = face_cap(inner, outer)
            caps["face_%d" % (k + 1)] = (m, info, (x, y), "face")
            k += 1
    for i, f in enumerate([f for f in feats if f["name"] == "side_button"]):
        ring.side = f["side"]
        xs = ring(f["outer_x"], f["y"], P.SW_Z, P.SIDE_FLANGE_D / 2 * 0.98, "x")
        f = dict(f, inner_x=(max(xs) if f["side"] < 0 else min(xs)))               # the most-intruding inner-wall point
        m, info = side_cap(f)
        caps["side_%s%d" % ("L" if f["side"] < 0 else "R", 1 if f["y"] > 65 else 2)] = (m, info, f, "side")
    pf = [f for f in feats if f["name"] == "power_button"][0]
    ys = ring(pf["head_x"] + 1.0, 0, P.SW_Z, P.PWR_FLANGE_D / 2 * 0.98, "y")
    pf = dict(pf, inner_y=max(ys))
    m, info = power_cap(pf)
    caps["power"] = (m, info, pf, "power")
    for name, (m, info, _, kind) in caps.items():
        m.export(os.path.join(out, "caps", name + ".stl"))
        print("%-9s %-26s watertight=%s vol=%.0f  z %.1f..%.1f" % (name, info, m.is_watertight, m.volume, m.bounds[0][2], m.bounds[1][2]))
    # ---- fit: caps in place vs the shell, the other caps and the components
    top = mo.from_trimesh(trimesh.load(os.path.join(out, "top.stl")))
    env = json.load(open(os.path.join(out, "..", "envelopes.json")))
    placed = {}
    for name, (m, info, ref, kind) in caps.items():
        w = m.copy()
        if kind == "face":
            zt = info["apex_z"]
            w.apply_transform(np.diag([1, 1, -1, 1]) @ np.eye(4)); w.apply_translation((ref[0], ref[1], zt))
        elif kind == "side":
            s = ref["side"]
            w.apply_transform(frame((ref["outer_x"] + s * P.SIDE_HEAD_PROUD, ref["y"], P.SW_Z), (0, 1, 0), (-s, 0, 0)))
        else:
            w.apply_transform(frame((ref["head_x"], ref["outer_y"] - P.SIDE_HEAD_PROUD, P.SW_Z), (1, 0, 0), (0, 1, 0)))
        placed[name] = w
    bad = False
    for name, w in placed.items():
        ov = (mo.from_trimesh(w) ^ top).volume()
        hits = []
        for e in env:
            if e.get("name") in ("oled", "battery") or (e["x1"] - e["x0"] > 30 and e["y1"] - e["y0"] > 90):
                continue
            b = trimesh.creation.box(bounds=[[e["x0"], e["y0"], e["z0"]], [e["x1"], e["y1"], e["z1"]]])
            v = (mo.from_trimesh(w) ^ mo.from_trimesh(b)).volume()
            if v > 0.05:
                hits.append((e.get("name") or "idx%s" % e.get("idx"), round(v, 2), [round(e[k], 1) for k in ("x0", "x1", "y0", "y1", "z0", "z1")]))
        print("%-9s vs top shell %.3f mm3; vs components %s" % (name, ov, hits))
    names = list(placed)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            v = (mo.from_trimesh(placed[names[i]]) ^ mo.from_trimesh(placed[names[j]])).volume()
            if v > 0.01:
                print("cap-cap overlap", names[i], names[j], round(v, 3))
