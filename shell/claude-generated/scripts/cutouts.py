"""Openings cut into the outer skin: thumbstick bowl and face-button holes.

All numbers live in params.py. Cuts are plain vertical features for now (no cavity yet).
"""
import math
import numpy as np
import params as P
from build123d import Cylinder, Pos, Rot, Plane, Polygon, revolve, Axis
import interior


def stick_sweep_profile(z_lo=12.5, z_hi=32.0, step=0.5, gap=None):
    """r(z): how far from the stick axis the tilting cap reaches at each height, plus the gap."""
    gap = P.STICK_GAP if gap is None else gap
    R = P.STICK_CAP_D / 2
    pz = P.STICK_PIVOT_Z
    pts = []                                    # (rho, z) samples of the swept cap surface
    for th in np.radians(np.linspace(0, P.STICK_TILT_DEG, 41)):
        for h in np.linspace(P.STICK_CAP_Z0, P.STICK_CAP_Z1, 15):
            for a in np.radians(np.linspace(0, 180, 73)):        # azimuth around the cap; tilt towards +x
                x, y = R * math.cos(a), R * math.sin(a)
                # tilt about y by th: axis direction (sin th, 0, cos th)
                X = h * math.sin(th) + x * math.cos(th)
                Z = pz + h * math.cos(th) - x * math.sin(th)
                pts.append((math.hypot(X, y), Z))
    pts = np.array(pts)
    zs = np.arange(z_lo, z_hi + 1e-6, step)
    out = []
    for z in zs:
        m = (pts[:, 1] >= z - step) & (pts[:, 1] <= z + step)
        out.append(pts[m, 0].max() + gap if m.any() else None)
    # below the lowest cap point keep the narrowest opening; above it use the swept radius
    known = [(z, r) for z, r in zip(zs, out) if r is not None]
    res = []
    for z, r in zip(zs, out):
        if r is None:
            r = known[0][1] if z < known[0][0] else known[-1][1]
        res.append((z, r))
    return res


def stick_opening(gap=None):
    prof = stick_sweep_profile(gap=gap)
    pts = [(0, prof[0][0])] + [(r, z) for z, r in prof] + [(0, prof[-1][0])]
    cut = revolve(Plane.XZ * Polygon(*pts, align=None), Axis.Z)
    return Pos(P.STICK_XY[0], P.STICK_XY[1], 0) * cut


def face_button_holes():
    holes = None
    for x in P.MACRO_XS:
        for y in P.MACRO_YS:
            c = Pos(x, y, 5.0) * Cylinder(P.MACRO_HOLE_D / 2, 30.0, align=None)
            holes = c if holes is None else holes + c
    return holes


class Probe:
    """Ray queries against the hollow shell (before features are added): where are the outer and inner walls?"""

    def __init__(self, shape):
        import trimesh
        v, f = shape.tessellate(0.05, 0.3)
        self.mesh = trimesh.Trimesh(vertices=[(p.X, p.Y, p.Z) for p in v], faces=f, process=False)

    def walls(self, origin, direction):
        """Distances along `direction` from `origin` to every wall crossing, sorted."""
        import numpy as np
        d = np.asarray(direction, float)
        loc, _, _ = self.mesh.ray.intersects_location(np.array([origin], float), np.array([d / np.linalg.norm(d)]))
        return sorted(float(np.dot(p - np.asarray(origin, float), d / np.linalg.norm(d))) for p in loc)


def cyl_between(p0, p1, r):
    """Cylinder of radius r from point p0 to p1 (any direction)."""
    import numpy as np
    a, b = np.asarray(p0, float), np.asarray(p1, float)
    v = b - a
    L = float(np.linalg.norm(v))
    ang_z = math.degrees(math.atan2(v[1], v[0]))
    ang_y = math.degrees(math.acos(v[2] / L))
    return Pos(*((a + b) / 2)) * Rot(Z=ang_z) * Rot(Y=ang_y) * Cylinder(r, L)


def side_button_cuts(probe):
    """Left/right side buttons: stadium head pocket (CAP_POCKET deep) and a neck hole through the rest of the wall."""
    cuts, info = None, []
    for side, tip in zip((-1, 1), P.SIDE_TIP_X):
        for y in P.SIDE_SW_Y:
            # ray from far outside towards the board: first crossing = outer wall, second = inner wall
            ox = tip + side * 30.0
            w = probe.walls((ox, y, P.SW_Z), (-side, 0, 0))
            outer, inner = ox - side * w[0], ox - side * w[1]
            floor = outer - side * P.CAP_POCKET
            hy, hz = P.SIDE_HEAD
            c = P.CAP_CLEAR
            x_out = outer + side * 4.0
            pocket = interior.stadium_x(y, P.SW_Z, hy + 2 * c, hz + 2 * c, min(x_out, floor), max(x_out, floor))
            xa, xb = floor + side * 0.5, inner - side * 1.5
            neck = Pos((xa + xb) / 2, y, P.SW_Z) * (Rot(Y=90) * Cylinder(P.SIDE_NECK_D / 2 + c, abs(xb - xa)))
            cuts = pocket + neck if cuts is None else cuts + pocket + neck
            info.append(dict(name="side_button", side=side, y=y, outer_x=round(outer, 2), floor_x=round(floor, 2),
                             inner_x=round(inner, 2), tip_x=tip))
    return cuts, info


def power_button_cuts(probe):
    """Oblique cone cap: head sits PWR_OFFSET to the left of the switch, the cone points at the actuator."""
    tx, ty, z = P.PWR_XY[0], P.PWR_TIP_Y, P.SW_Z
    hx = tx - P.PWR_OFFSET
    w = probe.walls((hx, -30.0, z), (0, 1, 0))
    outer_y, inner_y = -30.0 + w[0], -30.0 + w[1]
    floor_y = outer_y + P.CAP_POCKET
    c = P.CAP_CLEAR
    pocket = Pos(hx, (outer_y - 4.0 + floor_y) / 2, z) * (Rot(X=90) * Cylinder(P.PWR_HEAD_D / 2 + c, floor_y - outer_y + 4.0))
    import numpy as np
    H, T = np.array([hx, floor_y, z]), np.array([tx, ty, z])
    d = (T - H) / np.linalg.norm(T - H)
    t_end = (ty - 0.05 - H[1]) / d[1]                  # all the way to the actuator face (a channel through the USB sleeve's outer wall)
    neck = cyl_between(H - d * 0.5, H + d * t_end, P.PWR_NECK_D / 2 + c)
    info = [dict(name="power_button", head_x=hx, outer_y=round(outer_y, 2), floor_y=round(floor_y, 2),
                 inner_y=round(inner_y, 2), tip=[tx, ty, z])]
    return pocket + neck, info


def usb_cut():
    w, h = P.USB_TUNNEL
    return interior.stadium_y(P.USB_XY[0], 2.8, w, h, -30.0, P.USB_MOUTH_Y - 0.1)


def led_cut():
    x, y = P.LED_XY
    return Pos(x, y, (2.6 + 30.0) / 2) * Cylinder((P.LIGHT_PIPE_D + P.LIGHT_PIPE_CLEAR) / 2, 30.0 - 2.6)


def strap_cut():
    out = None
    for x in P.STRAP_XS:
        c = Pos(x, P.STRAP_Y, -25.0) * Cylinder((P.STRAP_D + 2 * P.CAP_CLEAR) / 2, 30.0)
        out = c if out is None else out + c
    return out


def top_cutters(probe):
    """Cutters for the top shell (everything but the strap holes sits above the seam)."""
    side, info_s = side_button_cuts(probe)
    pwr, info_p = power_button_cuts(probe)
    return [stick_opening(), face_button_holes(), side, pwr, usb_cut(), led_cut()], info_s + info_p


def bottom_cutters():
    return [strap_cut()]
