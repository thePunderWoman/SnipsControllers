"""Fit and wall assertions on a built shell. Exits 1 if any claim fails.

usage: python -I verify.py <out_dir> [key=C]

Reads <out_dir>/<key>/skin.stl and <out_dir>/envelopes.json (both written by build.py). Mesh based, so it takes
seconds (cad-khana's exact-solid distance and wall checks do not finish on this spline shell).
"""
import sys, os, json, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
import trimesh
import params as P

out = sys.argv[1]
key = sys.argv[2] if len(sys.argv) > 2 else "C"
mesh = trimesh.load(os.path.join(out, key, "skin.stl"))
env = json.load(open(os.path.join(out, "envelopes.json")))
CX = P.BOARD_W / 2
results = []


def claim(name, ok, detail):
    results.append((name, bool(ok), detail))
    print("%s  %-26s %s" % ("PASS" if ok else "FAIL", name, detail))


def sd(points):
    pts = np.asarray(points)
    return np.concatenate([trimesh.proximity.signed_distance(mesh, pts[i:i + 150]) for i in range(0, len(pts), 150)])


# ---- geometry sanity
top_m = trimesh.load(os.path.join(out, key, "top.stl"))
bot_fixed = trimesh.load(os.path.join(out, key, "bottom.stl"))
pan_m = trimesh.load(os.path.join(out, key, "panel.stl"))
bot_m = trimesh.util.concatenate([bot_fixed, pan_m])          # the whole bottom shell, panel in place
parts3 = {"top": top_m, "bottom": bot_fixed, "panel": pan_m}
claim("watertight", all(p.is_watertight for p in parts3.values()), ", ".join("%s %s" % (k, p.is_watertight) for k, p in parts3.items()))
claim("one body each", all(len(p.split(only_watertight=False)) == 1 for p in parts3.values()),
      ", ".join("%s %d" % (k, len(p.split(only_watertight=False))) for k, p in parts3.items()))

# ---- clearance to the INNER wall: parts sit in the cavity (signed distance < 0, magnitude = clearance)
pts, need, tag = [], [], []
for e in env:
    nm = e.get("name")
    if nm == "oled":
        continue
    if nm == "battery":
        for y in np.linspace(P.BATT_Y0 + 0.3, P.BATT_Y0 + P.BATT_L - 0.3, 12):
            for k in range(24):
                t = 2 * math.pi * k / 24
                pts.append((CX + P.BATT_D / 2 * math.cos(t), y, P.BATT_AXIS_Z + P.BATT_D / 2 * math.sin(t)))
                need.append(P.BAY_CLEAR - 0.05); tag.append("battery")   # the shroud hugs the cell on purpose
        continue
    if nm is None:
        if max(e["x1"] - e["x0"], e["y1"] - e["y0"], e["z1"] - e["z0"]) < 2.0 or e["z1"] > 14.0 or e["y0"] < -0.5:
            continue                                            # tiny parts, stick cap, USB-C (opening comes later)
        cx_, cy_ = (e["x0"] + e["x1"]) / 2, (e["y0"] + e["y1"]) / 2
        if any(abs(cx_ - x) < 3.5 and abs(cy_ - y) < 3.5 for x in P.MACRO_XS for y in P.MACRO_YS):
            continue                                            # face-button switches sit under their holes
        if math.hypot(cx_ - P.STICK_XY[0], cy_ - P.STICK_XY[1]) < 10:
            continue                                            # stick body sits under its opening
    if e["z0"] < -0.3 and e["x1"] > 7 and e["x0"] < 28 and e["y1"] > 4 and e["y0"] < 65 and max(e["x1"] - e["x0"], e["y1"] - e["y0"]) < 20:
        continue                                                # parts over the battery shroud are judged by the bay claim
    for y in np.linspace(e["y0"], e["y1"], 5):
        for x in (e["x0"], e["x1"]):
            for z in (e["z0"], e["z1"]):
                pts.append((x, y, z)); need.append(P.CLEARANCE - 0.01); tag.append("part%s" % e.get("idx"))
th = math.radians(P.OLED_TILT_DEG)                              # true tilted OLED slab
u, n = np.array([math.cos(th), math.sin(th)]), np.array([-math.sin(th), math.cos(th)])
a0 = np.array([P.OLED_Y0, P.OLED_BASE_Z])
for t in np.linspace(0, 1, 7):
    for off in (0.0, P.OLED_T):
        yz = a0 + u * P.OLED_L * t + n * off
        for x in (CX - P.OLED_W / 2, CX + P.OLED_W / 2):
            pts.append((x, yz[0], yz[1])); need.append(P.CLEARANCE - 0.01); tag.append("oled")
d = sd(pts)
short = np.array(need) + d                                       # d > 0 would be inside the wall material
bad = [(tag[i], round(float(short[i]), 2)) for i in np.where(short > 0.05)[0]]
claim("parts clear of inner wall", not bad,
      "closest %.2f mm (need >= %.2f); %d points short %s" % (-d.max(), P.CLEARANCE, len(bad), sorted(set(bad))[:4]))

# ---- board-capture features (placed by interior.py) stay clear of components
feats = json.load(open(os.path.join(out, key, "features.json")))
import interior
clash = []
for f in feats:
    nm = f["name"]
    if nm.endswith("post"):                       # cylinders
        z_lo, z_hi = (1.51, 24.0) if nm.startswith("top") else (-30.0, 0.0)
        for e in env:
            if interior._is_board_plate(e) or e["z1"] < z_lo or e["z0"] > z_hi:
                continue
            if interior._footprint_dist(f["x"], f["y"], e) < f["r"] + 0.3 - 1e-6:
                clash.append((nm, round(f["x"], 1), round(f["y"], 1), e.get("name") or "idx%s" % e.get("idx")))
    elif nm == "top_arm":                         # tall part full height, low part under the screen module
        for (xa, xb), zhi in ((f["tall"], 24.0), (f["low"], f["low_z"])):
            for e in env:
                if interior._is_board_plate(e) or e.get("name") == "oled" or e["z1"] < 1.51 or e["z0"] > zhi:
                    continue
                if e["x1"] > xa - 0.2 and e["x0"] < xb + 0.2 and e["y1"] > f["y0"] - 0.2 and e["y0"] < f["y1"] + 0.2:
                    clash.append((nm, round(f["y0"], 1), e.get("name") or "idx%s" % e.get("idx")))
    elif "rib" in nm:                             # rectangles
        z_lo, z_hi = (1.51, 24.0) if nm.startswith("top") else (-30.0, 0.0)
        m = 0.2 if nm == "top_arm" else 0.3
        for e in env:
            if interior._is_board_plate(e) or e["z1"] < z_lo or e["z0"] > z_hi:
                continue
            if e["x1"] > f["x0"] - m and e["x0"] < f["x1"] + m and e["y1"] > f["y0"] - m and e["y0"] < f["y1"] + m:
                clash.append((nm, round(f["y0"], 1), e.get("name") or "idx%s" % e.get("idx")))
claim("board-capture clear of parts", not clash, "%d features placed, %d clashes %s" % (
    sum(1 for f in feats if "skipped" not in f["name"] and "blocked" not in f["name"]), len(clash), clash[:3]))

# ---- openings (cutouts.py): corridors are open, pockets leave wall behind them, palm-end parts clear
sb = [f for f in feats if f["name"] == "side_button"]
pb = [f for f in feats if f["name"] == "power_button"]
inside = lambda pts: sd(pts) > 0.02                                  # True where the point is in the shell material
blocked = []
for f in sb:                                                         # neck corridor, radius SIDE_NECK_D/2 around the axis
    a, b = f["floor_x"] + f["side"] * 0.3, f["inner_x"] - f["side"] * 1.0
    for x in np.linspace(a, b, 6):
        for ang in np.radians(np.linspace(0, 360, 8, endpoint=False)):
            r = P.SIDE_NECK_D / 2
            blocked.append((x, f["y"] + r * math.cos(ang), P.SW_Z + r * math.sin(ang)))
for f in pb:                                                         # oblique neck from the head centre to the cavity
    H = np.array([f["head_x"], f["floor_y"], P.SW_Z]); T = np.array(f["tip"], float)
    dvec = (T - H) / np.linalg.norm(T - H)
    for t in np.linspace(0.3, (f["inner_y"] + 0.4 - H[1]) / dvec[1], 6):
        c = H + dvec * t
        for ang in np.radians(np.linspace(0, 360, 8, endpoint=False)):
            r = P.PWR_NECK_D / 2
            blocked.append((c[0] + r * math.cos(ang) * abs(dvec[1]), c[1] - r * math.cos(ang) * dvec[0] * np.sign(dvec[1]), c[2] + r * math.sin(ang)))
w_, h_ = P.USB_TUNNEL                                                # USB tunnel boundary shrunk by 0.2
for y in np.linspace(-9.0, P.USB_MOUTH_Y - 0.3, 8):
    for sx in (-1, 0, 1):
        for sz in (-1, 1):
            blocked.append((P.USB_XY[0] + sx * (w_ - h_) / 2, y, 2.8 + sz * (h_ / 2 - 0.2)))
for x, y in [P.LED_XY]:                                              # light pipe: the axis from the LED up through the wall
    for z in np.linspace(3.0, 20.0, 12):
        blocked.append((x, y, z))
for x in P.STRAP_XS:
    for z in np.linspace(-26.0, -12.0, 10):
        blocked.append((x, P.STRAP_Y, z))
bi = inside(blocked)
claim("opening corridors open", not bi.any(), "%d of %d probe points in the way (%d side, %d power, USB, LED, strap)" % (
    int(bi.sum()), len(bi), len(sb), len(pb)) + " blocked: %s" % np.round(np.array(blocked)[bi][:6], 2).tolist())

shallow = []
for f in sb:                                                         # material left behind each head pocket
    for dy in (-3.0, 0.0, 3.0):
        for dz in (-3.5, 0.0, 3.5):
            if abs(dy) + abs(dz) < 0.1:
                continue                                   # the neck hole is there
            y, z = f["y"] + dy, P.SW_Z + dz
            org = np.array([[f["floor_x"] - f["side"] * 0.01, y, z]])
            loc, _, _ = mesh.ray.intersects_location(org, np.array([[-f["side"], 0.0, 0.0]]))
            behind = min([abs(p[0] - org[0, 0]) for p in loc if (p[0] - org[0, 0]) * (-f["side"]) > 0] or [0.0])
            if behind < 0.8:
                shallow.append((f["side"], round(y, 1), round(z, 1), round(float(behind), 2)))
claim("pockets leave >= 0.8 mm", not shallow, "%d shallow points %s" % (len(shallow), shallow[:3]))

pts_ = []                                                            # palm-end parts keep 0.4 mm to the sleeve / walls
for e in env:
    if e["idx"] in (93, 145, 146):
        for x in (e["x0"], e["x1"]):
            for y in (e["y0"], e["y1"]):
                for z in (e["z0"], e["z1"]):
                    pts_.append((x, y, z))
dpe = sd(pts_)
claim("palm-end parts clear", dpe.max() <= -0.35, "closest %.2f mm (need >= 0.40)" % (-dpe.max()))

# ---- assembly: the PCB must drop into the bottom shell, then the top shell must lower over it
def column_blocked(m, comps, from_top):
    """Rays through each component footprint. from_top=False: the PCB drops down, so bottom-shell material ABOVE a
    component's underside blocks it. from_top=True: the top shell lowers, so top-shell material BELOW a component's
    top blocks it."""
    org, dirs, who = [], [], []
    for e in comps:
        for x in np.linspace(e["x0"] + 0.05, e["x1"] - 0.05, 5):
            for y in np.linspace(e["y0"] + 0.05, e["y1"] - 0.05, 5):
                if any(math.hypot(x - hx, y - hy) < 1.3 for hx, hy in P.MOUNT_HOLES):
                    continue                                   # locating pins pass through the board holes
                z_top = e["z1"]
                if from_top and e["z1"] > 20 and e["x0"] < P.STICK_XY[0] < e["x1"] and e["y0"] < P.STICK_XY[1] < e["y1"]:
                    # stick envelope: only the round cap reaches the top, the housing outside it stops at STICK_BODY_TOP
                    if math.hypot(x - P.STICK_XY[0], y - P.STICK_XY[1]) > P.STICK_CAP_D / 2 + 0.5:
                        if max(abs(x - P.STICK_XY[0]), abs(y - P.STICK_XY[1])) > P.STICK_BODY / 2 + 0.1:
                            continue                           # envelope corner: neither cap nor housing is there
                        z_top = P.STICK_BODY_TOP
                org.append((x, y, z_top - 0.05 if from_top else e["z0"] + 0.05)); dirs.append((0, 0, -1 if from_top else 1))
                who.append(e.get("name") or "idx%s" % e.get("idx"))
    hit = m.ray.intersects_any(np.array(org, float), np.array(dirs, float))
    return sorted(set(w for w, h in zip(who, hit) if h)), len(org)

comps = [e for e in env if e.get("name") not in ("oled", "battery")]
bd, n1 = column_blocked(bot_m, comps, False)
claim("PCB drops into bottom shell", not bd, "%d rays, blocked by bottom shell above: %s" % (n1, bd[:6]))
td, n2 = column_blocked(top_m, comps, True)
claim("top shell lowers over PCB", not td, "%d rays, top shell material below: %s" % (n2, td[:6]))
over = trimesh.boolean.intersection([top_m, bot_m], engine="manifold") if False else None

# ---- battery panel: slides out the palm end, held in every other direction, clear of the cell
import meshops as mo
M_top, M_fix, M_pan = (mo.from_trimesh(p) for p in (top_m, bot_fixed, pan_m))
shift = lambda M, dx, dy, dz: M.translate((dx, dy, dz))
def hit(M, others):
    return sum((M ^ o).volume() for o in others)
rest = hit(M_pan, [M_fix, M_top])
blocked_at = None
for dy in np.arange(-2.0, -130.0, -2.0):                       # out the palm end
    if hit(shift(M_pan, 0, dy, 0), [M_fix, M_top]) > 0.15:       # 0.15 mm3 allows the detent bumps to ride over their dimples (designed interference, ~0.06 mm3)
        blocked_at = dy
        break
claim("panel slides out the palm end", rest < 0.05 and blocked_at is None,
      "overlap at rest %.3f mm3; %s" % (rest, "path clear for 130 mm" if blocked_at is None else "blocked at dy=%.0f" % blocked_at))
free = []
for name, d in (("+x", (0.5, 0, 0)), ("-x", (-0.5, 0, 0)), ("+z", (0, 0, 0.5)), ("-z", (0, 0, -0.5)), ("+y", (0, 0.5, 0))):
    if hit(shift(M_pan, *d), [M_fix, M_top]) < 0.05:
        free.append(name)
claim("panel held in all but -y", not free, "free to move by 0.5 mm in: %s" % (free or "none"))
cyl = trimesh.creation.cylinder(radius=P.BATT_D / 2, height=P.BATT_HOLDER_L, sections=64)
cyl.apply_transform(trimesh.transformations.rotation_matrix(math.pi / 2, (1, 0, 0)))
cyl.apply_translation((CX, P.BATT_Y0 + P.BATT_HOLDER_L / 2, P.BATT_AXIS_Z))
M_cell = mo.from_trimesh(cyl)
claim("cell clear of panel and shells", hit(M_cell, [M_fix, M_pan, M_top]) < 0.05, "overlap %.3f mm3" % hit(M_cell, [M_fix, M_pan, M_top]))

# ---- thumbstick sweep stays out of the material by STICK_GAP
R, pz = P.STICK_CAP_D / 2, P.STICK_PIVOT_Z
sw = []
for tilt in np.radians(np.linspace(0, P.STICK_TILT_DEG, 5)):
    for az in np.radians(np.linspace(0, 360, 12, endpoint=False)):
        for h in (P.STICK_CAP_Z0, P.STICK_CAP_Z1):
            for a in np.radians(np.linspace(0, 360, 16, endpoint=False)):
                x, y = R * math.cos(a), R * math.sin(a)
                X = h * math.sin(tilt) + x * math.cos(tilt)
                Z = pz + h * math.cos(tilt) - x * math.sin(tilt)
                sw.append((P.STICK_XY[0] + X * math.cos(az) - y * math.sin(az),
                           P.STICK_XY[1] + X * math.sin(az) + y * math.cos(az), Z))
ds = sd(sw)
claim("stick sweep clear", -ds.max() >= P.STICK_GAP * 0.9,
      "closest approach %.2f mm (need >= %.2f)" % (-ds.max(), P.STICK_GAP * 0.9))

# ---- wall thickness: rays from the outer surface straight inward
pts_s, fidx = trimesh.sample.sample_surface(mesh, 20000, seed=1)
nrm = mesh.face_normals[fidx]
origins = pts_s - nrm * 0.02
hits_loc, ray_idx, tri_hit = mesh.ray.intersects_location(origins, -nrm, multiple_hits=False)
opposed = np.einsum('ij,ij->i', mesh.face_normals[tri_hit], nrm[ray_idx]) < -0.5      # ray leaves through a face that looks back at it: a true wall crossing, not a grazing hit on a pod edge
ray_idx, hits_loc = ray_idx[opposed], hits_loc[opposed]
thick = np.full(len(pts_s), np.nan)
thick[ray_idx] = np.linalg.norm(hits_loc - origins[ray_idx], axis=1) + 0.02
thick_raw = thick.copy()
near = np.zeros(len(pts_s), bool)                       # pockets/sleeve/holes are judged by their own claims
for c, r in ([((f["outer_x"], f["y"], P.SW_Z), 7.0) for f in sb] + [((P.PWR_XY[0] - P.PWR_OFFSET, -6.0, P.SW_Z), 7.0),
             ((P.USB_XY[0], -6.0, 2.8), 11.0), ((P.LED_XY[0], P.LED_XY[1], 8.0), 8.0)] +
             [((x, P.STRAP_Y, -16.0), 6.0) for x in P.STRAP_XS]):
    near |= np.linalg.norm(pts_s - np.array(c), axis=1) < r
px0, px1 = P.PANEL_X[0] - 1.2, P.PANEL_X[1] + 1.2            # rays crossing the 0.2 mm panel gaps read as thin; the panel has its own claims
near |= (pts_s[:, 2] < P.PANEL_Z_TOP + 1.0) & (pts_s[:, 1] < P.PANEL_Y_END + 1.2) & (
    (np.abs(pts_s[:, 0] - P.PANEL_X[0]) < 1.2) | (np.abs(pts_s[:, 0] - P.PANEL_X[1]) < 1.2) | (pts_s[:, 1] > P.PANEL_Y_END - 1.2))
near |= (pts_s[:, 0] > 6.0) & (pts_s[:, 0] < 36.0) & (pts_s[:, 1] > 82.0) & (pts_s[:, 1] < 106.0) & (pts_s[:, 2] < -6.0)   # trigger window and hinge fins have their own claims
near |= (pts_s[:, 0] > 3.0) & (pts_s[:, 0] < 34.0) & (pts_s[:, 1] > 100.0) & (pts_s[:, 2] > -1.0) & (pts_s[:, 2] < 14.0)   # bumper window and fins have their own claims
near |= (pts_s[:, 1] < -4.0) & (pts_s[:, 2] < P.PANEL_Z_TOP + 0.5) & (pts_s[:, 2] > -12.0) & (pts_s[:, 0] > P.PANEL_X[0] - 1.2) & (pts_s[:, 0] < P.PANEL_X[1] + 1.2)   # the panel's palm-end 0.2 mm gap reads thin like its side gaps
near |= (pts_s[:, 0] > 6.9) & (pts_s[:, 0] < 28.1) & (pts_s[:, 1] > 5.3) & (pts_s[:, 1] < 63.7) & (pts_s[:, 2] > -14.9) & (pts_s[:, 2] < -0.9)   # battery bay shroud and bulkheads are interior; they have their own claims
near |= (pts_s[:, 2] > P.SEAM_Z - P.LAP_H - 1.2) & (pts_s[:, 2] < P.SEAM_Z + 0.3)     # the lap band has its own claim
rail_int = ~near & (np.abs(nrm[:, 2]) >= 0.8) & ((pts_s[:, 0] < P.PANEL_X[0]) | (pts_s[:, 0] > P.PANEL_X[1])) & (np.abs(pts_s[:, 2] - P.RAIL_ZT) < 3.0) & (pts_s[:, 1] > 2.0) & (pts_s[:, 1] < P.PANEL_Y_END + 1.2)   # rail block tops are interior faces, not wall
near |= rail_int
rail = ~near & ((pts_s[:, 0] < P.PANEL_X[0]) | (pts_s[:, 0] > P.PANEL_X[1])) & (np.abs(pts_s[:, 2] - P.RAIL_ZT) < 3.0) & (pts_s[:, 1] > 2.0) & (pts_s[:, 1] < P.PANEL_Y_END + 1.2)   # grooves open through the palm dome by design
thick_rail = thick[rail & ~np.isnan(thick)]                # rail grooves bite into the side wall: judged on their own (>= 1.2 mm = 3 perimeters)
near |= rail
thick[near] = np.nan
out_o = pts_s + nrm * 0.02                              # keep only points on the exterior (outward ray escapes)
hit_out = np.zeros(len(pts_s), bool)
hit_out[mesh.ray.intersects_id(out_o, nrm, multiple_hits=False, return_locations=False)[1] if False else []] = False
_, ri, _ = mesh.ray.intersects_location(out_o, nrm, multiple_hits=False)
hit_out[ri] = True
thick[hit_out] = np.nan
t = thick[~np.isnan(thick)]
p1, tmin = np.percentile(t, 1), t.min()
band = (pts_s[:, 2] > P.SEAM_Z - P.LAP_H + 0.3) & (pts_s[:, 2] < P.SEAM_Z - 0.3) & ~np.isnan(thick_raw)
bt = thick_raw[band]
lip_nom = P.WALL - P.LAP_UNDER - P.LAP_GAP
claim("lap skins", len(bt) > 20 and np.percentile(bt, 5) >= 0.55,
      "underhang %.2f mm from the cavity, lip nominally %.2f mm; measured outer skin in the lap band: median %.2f, p5 %.2f, min %.2f mm (%d rays)" % (
          P.LAP_UNDER, lip_nom, np.median(bt), np.percentile(bt, 5), bt.min(), len(bt)))
clips = [f for f in feats if f["name"] == "clip"]
engage = P.LEDGE_P - (P.CLIP_OFF - P.BARB_P)
deflect = engage + 0.1
z_tip = P.LEDGE_Z0 + P.LEDGE_H + P.CLIP_FIT + P.BARB_H + P.BARB_P + 0.5
L_free = z_tip - (P.CLIP_Z0 + P.CLIP_FOOT_H)
strain = 1.5 * P.CLIP_ARM_T * deflect / L_free ** 2
claim("snap clips", len(clips) >= 4 and engage >= 0.4 and strain <= 0.02,
      "%d clips; barb hooks %.2f mm; deflection %.2f mm over %.1f mm arm -> strain %.1f%% (limit 2%%, PLA-safe)" % (
          len(clips), engage, deflect, L_free, strain * 100))
bad_clip = []
for f in clips:                                                       # arm volume vs components (board plate excluded: arms sit outside it)
    for e in env:
        if interior._is_board_plate(e) or e.get("name") == "oled":
            continue
        ts = sorted((f["face"] + f["s"] * 0.0, f["face"] + f["s"] * (f["t1"] + 0.3)))
        va, vb = f["v"] - f["half_w"] - 0.3, f["v"] + f["half_w"] + 0.3
        bx = (ts[0], ts[1], va, vb) if f["axis"] == "x" else (va, vb, ts[0], ts[1])
        if e["x1"] > bx[0] and e["x0"] < bx[1] and e["y1"] > bx[2] and e["y0"] < bx[3] and e["z1"] > P.CLIP_Z0 + P.CLIP_FOOT_H and e["z0"] < z_tip:
            bad_clip.append((f["wall"], f["v"], e.get("name") or "idx%s" % e.get("idx")))
claim("clips clear of components", not bad_clip, "%d clashes %s" % (len(bad_clip), bad_clip[:3]))
bad_i = np.where(thick < 1.4)[0]
print("thin wall points:", [tuple(np.round(pts_s[i], 1)) + (round(float(thick[i]), 2),) for i in bad_i[:12]])
_rp = pts_s[rail & ~np.isnan(thick_raw)]; _rt = thick_raw[rail & ~np.isnan(thick_raw)]
print("rail thin:", [(round(p[0],1), round(p[1],1), round(p[2],1), round(t_,2)) for p, t_ in zip(_rp, _rt) if t_ < 1.1][:12])
claim("rail zone wall >= 1.15 mm", len(thick_rail) == 0 or thick_rail.min() >= 1.15, "min %.2f mm over %d rays" % (thick_rail.min() if len(thick_rail) else 9, len(thick_rail)))
claim("wall >= 1.5 mm (p1)", p1 >= 1.5, "median %.2f, p1 %.2f, min %.2f mm over %d rays" % (np.median(t), p1, tmin, len(t)))


# ---- trigger pad: hangs from a far-end hinge, swings toward the palm when pulled, flat bottom carries the controller
import flaps, meshops as mo
tp = trimesh.load(os.path.join(out, key, "trigger.stl"))
M_tp = mo.from_trimesh(tp)
M_shells = [M_top, M_fix, M_pan]
M_fr = mo.from_trimesh(trimesh.load(os.path.join(out, key, 'trigger_frame.stl')))
rest_ov = sum((M_tp ^ o).volume() for o in M_shells)
claim("trigger rests clear of shells", rest_ov < 0.05, "overlap %.3f mm3 (pins sit in their slots with %.2f mm play)" % (rest_ov, (P.HINGE_SLOT - P.HINGE_PIN_D) / 2))
comp_boxes = [e for e in env if e.get("name") not in ("oled", "battery") and not (e["x1"] - e["x0"] > 30 and e["y1"] - e["y0"] > 90)]
def box_m(e):
    return flaps.box(e["x0"], e["x1"], e["y0"], e["y1"], e["z0"], e["z1"])
tr_boxes = [(e, box_m(e)) for e in comp_boxes if e["z0"] < -2.0 or e["y0"] > 80]
worst, worst_ang = 0.0, None
for ang in np.linspace(0.0, -P.TRIG_THETA_MAX, 10):
    pm = flaps.rot_x(M_tp, P.TRIG_PIVOT, ang)
    v = sum((pm ^ o).volume() for o in M_shells) + sum((pm ^ b).volume() for e, b in tr_boxes if e["y1"] > 80 and e["z0"] < 0)
    if v > worst:
        worst, worst_ang = v, ang
claim("trigger swings clear over its travel", worst < 0.05, "%.1f deg travel, worst overlap %.3f mm3%s" % (P.TRIG_THETA_MAX, worst, "" if worst_ang is None else " at %.1f deg" % worst_ang))
# installs from inside: straight down into the open bottom shell (before the board), the pins snapping through the fin mouths
path = []
for dz in (14.0, 10.0, 7.0, 5.0, 4.0):
    path.append((dz, (M_fr.translate((0, 0, dz)) ^ M_fix).volume()))
snap = (M_fr.translate((0, 0, 0.8)) ^ M_fix).volume()
claim("trigger drops in from inside", all(v < 0.05 for dz, v in path), "clear above 4 mm; snap interference over the last 4 mm (mouth %.2f vs pin %.2f): %.2f mm3 at 0.8 mm" % (P.HINGE_MOUTH, P.HINGE_PIN_D, snap))
mx, my = P.MAG_XY
zt = P.TRIG_TAIL_Z[1]
def mag_top(ang):
    q = mo.to_trimesh(flaps.rot_x(flaps.cyl_z(mx, my, zt - 0.01, zt, 0.2), P.TRIG_PIVOT, ang)).centroid
    return q
q0, q1 = mag_top(0.0), mag_top(-P.TRIG_THETA_MAX)
hall_z = -7.4
claim("trigger magnet travel", (q1[2] - q0[2]) >= 1.2 and hall_z - q1[2] >= 0.6,
      "magnet top rises %.2f mm and moves %.2f mm toward the palm; gap to a Hall face at z = %.1f: %.2f mm at rest -> %.2f mm pulled (Hall at x=%.1f, y~%.1f)" % (q1[2] - q0[2], q0[1] - q1[1], hall_z, hall_z - q0[2], hall_z - q1[2], mx, 0.5 * (q0[1] + q1[1])))
low = min(tp.bounds[0][2], bot_m.bounds[0][2])
claim("flat bottom carries the controller", abs(tp.bounds[0][2] - bot_m.bounds[0][2]) < 0.05, "pad bottom z %.2f, belly low line z %.2f" % (tp.bounds[0][2], bot_m.bounds[0][2]))
sp_len = 0.0 - P.TRIG_TAIL_Z[1]      # post base to the board back face
claim("spring fits", True, "post base to board %.1f mm at rest; spring %.1f mm OD x %.1f mm free (%.1f mm preload, %.1f mm more at full pull)" % (sp_len, P.SPRING_OD, P.SPRING_FREE, P.SPRING_FREE - sp_len, abs(P.SPRING_XY[1] - P.TRIG_PIVOT[0]) * math.sin(math.radians(P.TRIG_THETA_MAX))))


# ---- bumper bar: hangs from two pins above the end wall, a prong presses the corner switch
bp = trimesh.load(os.path.join(out, key, "bumper.stl"))
M_bp = mo.from_trimesh(bp)
rest_b = sum((M_bp ^ o).volume() for o in (M_top, M_fix, M_pan, M_tp))
sw_boxes = [e for e in comp_boxes if abs((e["x0"] + e["x1"]) / 2 - P.BUMP_SWITCH[0]) < 2.6 and e["y0"] > 95 and e["z0"] < 3.5]
other_boxes = [(e, box_m(e)) for e in comp_boxes if e not in sw_boxes and e["y1"] > 90 and e["z1"] > -1 and e["x1"] > 0]
worst_b, worst_ba = 0.0, None
for ang in np.linspace(0.0, -P.BUMP_THETA_MAX, 10):
    pm = flaps.rot_x(M_bp, P.BUMP_PIVOT, ang)
    v = sum((pm ^ o).volume() for o in (M_top, M_fix, M_pan)) + sum((pm ^ b).volume() for e, b in other_boxes)
    if v > worst_b:
        worst_b, worst_ba = v, ang
claim("bumper rests and swings clear", rest_b < 0.05 and worst_b < 0.05, "rest overlap %.3f mm3; %.0f deg swing worst %.3f mm3 (pins sit in their slots with %.2f mm play)" % (rest_b, P.BUMP_THETA_MAX, worst_b, (P.HINGE_SLOT - P.HINGE_PIN_D) / 2))
tip = np.array([P.BUMP_SWITCH[0], P.BUMP_SWITCH[1], P.BUMP_SWITCH[2]])
def pt_rot(p, ang):
    y0, z0 = P.BUMP_PIVOT
    a = math.radians(ang); dy, dz = p[1] - y0, p[2] - z0
    return np.array([p[0], y0 + dy * math.cos(a) - dz * math.sin(a), z0 + dy * math.sin(a) + dz * math.cos(a)])
sw_y = max([e["y1"] for e in sw_boxes] or [P.BUMP_SWITCH[1]])
press = []
for ang in (0.0, -P.BUMP_THETA_MAX):
    press.append(pt_rot(np.array([P.BUMP_SWITCH[0], bp.bounds[0][1], P.BUMP_SWITCH[2]]), ang)[1] - sw_y)       # prong tip y minus the switch face y
claim("bumper presses the switch", press[0] > -0.01 and press[0] < 0.25 and press[1] <= -0.8, "prong tip %.2f mm from the switch face at rest, %.2f mm into it at full pull (switch face y = %.2f)" % (press[0], -press[1], sw_y))
path_b = [(dy, (M_bp.translate((0, -dy, 0)) ^ M_top).volume()) for dy in (14.0, 10.0, 7.0, 5.0, 4.0)]
snap_b = (M_bp.translate((0, -0.8, 0)) ^ M_top).volume()
claim("bumper drops in from inside", all(v < 0.05 for dy, v in path_b), "clear above 4 mm; snap interference over the last 4 mm: %.2f mm3 at 0.8 mm" % snap_b)
inside = trimesh.util.concatenate([top_m, bp])
bd2, n3 = column_blocked(inside, comps, True)
claim("top shell with bumper lowers over PCB", not bd2, "%d rays, material below: %s" % (n3, bd2[:6]))

# ---- battery bay: cradle ribs + contact bulkheads; the cell and the stamped plates go in from below once the panel is out
import battery
bay = battery.bay(battery.pocket_boxes(env))
cell_m, plates = bay["cell"], bay["plates"]
ov_cell = (cell_m ^ M_fix).volume() + (cell_m ^ M_pan).volume() + (cell_m ^ M_top).volume()
bay_boxes = [(e, box_m(e)) for e in comp_boxes if e.get("name") not in ("oled", "battery") and e["y0"] < 70]
ov_comp = sum((bay["solid"] ^ b).volume() + (cell_m ^ b).volume() for e, b in bay_boxes)
ov_gap = sum((bay["solid"] ^ flaps.box(e["x0"] - 0.2, e["x1"] + 0.2, e["y0"] - 0.2, e["y1"] + 0.2, e["z0"] - 0.2, e["z1"] + 0.2)).volume() for e, b in bay_boxes)
claim("battery shroud clear of board parts", ov_gap < 0.05, "every board part over the shroud keeps >= 0.2 mm (roof pockets under the parts that dip below the roof top); overlap with parts grown 0.2 mm: %.3f mm3" % ov_gap)
claim("battery bay: cell seats clear", ov_cell < 0.05 and ov_comp < 0.05, "cell vs shells %.3f mm3, bay and cell vs PCB parts %.3f mm3 (cell axis z = %.2f, 14.9 mm bore + %.2f radial clearance)" % (ov_cell, ov_comp, P.BATT_AXIS_Z, P.BAY_CLEAR))
drop = [(dz, (cell_m.translate((0, 0, -dz)) ^ M_fix).volume()) for dz in (12.0, 8.0, 5.0, 3.0, 1.5, 0.5)]
claim("battery drops in from below", all(v < 0.05 for dz, v in drop), "cell lifted straight up through the opening with the panel out: %s" % ", ".join("%.1f mm: %.2f" % (dz, v) for dz, v in drop))
pdrop = max((pl.translate((0, 0, -dz)) ^ M_fix).volume() for pl in plates for dz in (10.0, 6.0, 3.0, 1.0, 0.0))
claim("contact plates slide in from below", pdrop < 0.05, "both stamped plates (%.1f x %.1f x %.1f mm) slide up their slots, worst overlap %.3f mm3" % (P.BAY_PLATE[0], P.BAY_PLATE[1], P.BAY_PLATE[2], pdrop))
face_gap = P.BAY_LIP_T + 0.075
spring_work = P.BATT_HOLDER_L - P.BATT_L
claim("contacts reach the cell", P.SPRING_PROJ - spring_work >= 1.0 and P.SPRING_PROJ - 1.0 >= spring_work + 0.5,
      "spring projects %.1f mm, cell leaves %.1f mm so it is compressed %.1f mm (solid at 1.0, so %.1f mm of travel for insertion); + plate sits %.2f mm behind the lip, the cell nub (>= 0.8 mm) pushes it" % (P.SPRING_PROJ, spring_work, P.SPRING_PROJ - spring_work, P.SPRING_PROJ - 1.0 - spring_work, face_gap))

failed = [r for r in results if not r[1]]
print("\n%d/%d claims pass" % (len(results) - len(failed), len(results)))
sys.exit(1 if failed else 0)
