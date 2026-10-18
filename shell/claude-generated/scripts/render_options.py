"""Comparison sheet of the shape options. usage: python -I render_options.py <out_dir> <sheet.png> [A B C]"""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import math
import numpy as np, trimesh
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
import params as P
from build import constraint_points, CX

out, sheet = sys.argv[1], sys.argv[2]
keys = sys.argv[3:] or list(P.SHAPES)
env = json.load(open(os.path.join(out, "envelopes.json")))
LIGHT = np.array([-0.35, -0.45, 0.82]); LIGHT /= np.linalg.norm(LIGHT)
BASE = np.array([0.80, 0.76, 0.90])


def shaded(ax, mesh, elev, azim):
    tri = mesh.triangles
    vs = np.clip(mesh.vertex_normals @ LIGHT, 0, 1)          # smooth (per-vertex) shading
    shade = 0.28 + 0.72 * vs[mesh.faces].mean(axis=1)
    cols = BASE[None, :] * shade[:, None]
    ax.add_collection3d(Poly3DCollection(tri, facecolors=cols, edgecolors=cols, linewidths=0.25))
    lo, hi = mesh.bounds
    ax.set_xlim(lo[0], hi[0]); ax.set_ylim(lo[1], hi[1]); ax.set_zlim(lo[2], hi[2])
    ax.set_box_aspect(tuple(hi - lo))
    ax.view_init(elev=elev, azim=azim)
    ax.set_axis_off()


def check(mesh):
    """count constraint points closer to the skin than their required wall"""
    pts, need = [], []
    wall = P.CLEARANCE                      # parts sit in the cavity now: clearance to the INNER wall
    for e in env:
        if e.get("name") == "battery":
            for y in np.linspace(e["y0"], e["y1"], 9):
                for k in range(24):
                    t = 2 * np.pi * k / 24
                    pts.append((CX + P.BATT_D / 2 * np.cos(t), y, P.BATT_AXIS_Z + P.BATT_D / 2 * np.sin(t)))
                    need.append(0.3)                 # battery: slider wall is part of the 2 mm shell
            continue
        if e.get("name") == "oled":
            continue
        if e.get("name") is None:
            if max(e["x1"] - e["x0"], e["y1"] - e["y0"], e["z1"] - e["z0"]) < 2.0:
                continue
            if e["z1"] > 14.0 or e["y0"] < -0.5:
                continue
            cx_, cy_ = (e["x0"] + e["x1"]) / 2, (e["y0"] + e["y1"]) / 2
            if any(abs(cx_ - x) < 3.5 and abs(cy_ - y) < 3.5 for x in P.MACRO_XS for y in P.MACRO_YS):
                continue                              # face-button switches sit under their holes
            if math.hypot(cx_ - P.STICK_XY[0], cy_ - P.STICK_XY[1]) < 10:
                continue                              # stick body sits under its opening
        for y in np.linspace(e["y0"], e["y1"], 5):
            for x in (e["x0"], e["x1"]):
                for z in (e["z0"], e["z1"]):
                    pts.append((x, y, z)); need.append(wall)
    th = np.radians(P.OLED_TILT_DEG)               # true tilted slab corners and edge samples
    u, n = np.array([np.cos(th), np.sin(th)]), np.array([-np.sin(th), np.cos(th)])
    a0 = np.array([P.OLED_Y0, P.OLED_BASE_Z])
    for t in np.linspace(0, 1, 7):
        for off in (0.0, P.OLED_T):
            yz = a0 + u * P.OLED_L * t + n * off
            for x in (CX - P.OLED_W / 2, CX + P.OLED_W / 2):
                pts.append((x, yz[0], yz[1])); need.append(wall)
    d = trimesh.proximity.signed_distance(mesh, np.array(pts))
    short = np.array(need) + d              # d < 0 in the cavity (distance to the inner wall); d > 0 means inside material
    # tilting stick cap must stay clear of the skin by at least STICK_GAP (surface samples, every tilt direction)
    R, pz = P.STICK_CAP_D / 2, P.STICK_PIVOT_Z
    sw = []
    for th in np.radians(np.linspace(0, P.STICK_TILT_DEG, 5)):
        for az in np.radians(np.linspace(0, 360, 12, endpoint=False)):
            for h in (P.STICK_CAP_Z0, P.STICK_CAP_Z1):
                for a in np.radians(np.linspace(0, 360, 16, endpoint=False)):
                    x, y = R * np.cos(a), R * np.sin(a)
                    X = h * np.sin(th) + x * np.cos(th); Z = pz + h * np.cos(th) - x * np.sin(th)
                    sw.append((P.STICK_XY[0] + X * np.cos(az) - y * np.sin(az),
                               P.STICK_XY[1] + X * np.sin(az) + y * np.cos(az), Z))
    sw = np.array(sw)
    ds = np.concatenate([trimesh.proximity.signed_distance(mesh, sw[i:i + 150]) for i in range(0, len(sw), 150)])
    stick_short = float((P.STICK_GAP * 0.9 - (-ds)).max())    # ds > 0 inside the material
    print("   stick sweep: closest approach %.2f mm (need %.2f)" % (-ds.max() if ds.max() > 0 else -ds.max(), P.STICK_GAP * 0.9))
    return int((short > 0.2).sum()) + int(stick_short > 0.0), float(max(short.max(), stick_short))


fig = plt.figure(figsize=(21, 5.2 * len(keys)))
for r, k in enumerate(keys):
    shp = P.SHAPES[k]
    mesh = trimesh.load(os.path.join(out, k, "skin.stl"))
    bad, worst = check(mesh)
    dims = mesh.extents
    print("%s %-18s %.0f x %.0f x %.0f mm  fit-check: %d points short, worst %.2f mm" %
          (k, shp["name"], dims[0], dims[1], dims[2], bad, worst))
    views = [(28, -50), (90, -90), (0, 0)]
    for c, (el, az) in enumerate(views):
        ax = fig.add_subplot(len(keys), 5, r * 5 + c + 1, projection="3d")
        shaded(ax, mesh, el, az)
        if c == 0:
            ax.set_title("%s: %s   (%.0f W x %.0f L x %.0f T mm)" % (k, shp["name"], dims[0], dims[1], dims[2]),
                         loc="left", fontsize=13)
    for c, (yy, name) in enumerate(((22.0, "XBee zone, y=22"), (P.STICK_XY[1], "stick zone, y=%.0f" % P.STICK_XY[1]))):
        ax = fig.add_subplot(len(keys), 5, r * 5 + 4 + c)
        sec = mesh.section(plane_origin=(0, yy, 0), plane_normal=(0, 1, 0))
        for poly in sec.discrete:
            ax.plot(poly[:, 0], poly[:, 2], "k", lw=1.8)
        ax.add_patch(Rectangle((0, 0), P.BOARD_W, P.BOARD_T, fc="#2d6a4f"))
        for e in env:
            if e["y0"] <= yy <= e["y1"] and e.get("name") != "battery" and \
               max(e["x1"]-e["x0"], e["y1"]-e["y0"], e["z1"]-e["z0"]) >= 1.5:
                ax.add_patch(Rectangle((e["x0"], e["z0"]), e["x1"]-e["x0"], e["z1"]-e["z0"],
                                       fc="#f4a261", ec="k", lw=0.3, alpha=0.6))
        ax.add_patch(Circle((CX, P.BATT_AXIS_Z), P.BATT_D / 2, fc="#8ecae6", ec="b", alpha=0.45))
        ax.set_title(name); ax.set_aspect("equal"); ax.grid(alpha=.3)
        ax.set_xlim(-12, 47); ax.set_ylim(-28, 27)
fig.tight_layout()
fig.savefig(sheet, dpi=55)
print("saved", sheet)
