"""Trigger pad and bumper bar: pendulum parts on printed pin hinges.

Each part hangs from two pins (along x) that snap into upward/inward-open slots in fins standing on the shell wall.
The part is installed from inside the open shell before the board goes in; its spring/switch pushes it to a rest stop.
Everything is mesh booleans (manifold3d). Frame: build frame (x across, y along, z up from the board back).
"""
import math
import numpy as np
import trimesh
from shapely.geometry import Polygon
import params as P
import meshops as mo


def M(m):
    return mo.from_trimesh(m)


def box(x0, x1, y0, y1, z0, z1):
    b = trimesh.creation.box(extents=(x1 - x0, y1 - y0, z1 - z0))
    b.apply_translation(((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2))
    return M(b)


def cyl_x(x0, x1, y, z, r, sections=40):
    c = trimesh.creation.cylinder(radius=r, height=x1 - x0, sections=sections)
    c.apply_transform(trimesh.transformations.rotation_matrix(math.pi / 2, (0, 1, 0)))
    c.apply_translation(((x0 + x1) / 2, y, z))
    return M(c)


def cyl_z(x, y, z0, z1, r, sections=40):
    c = trimesh.creation.cylinder(radius=r, height=z1 - z0, sections=sections)
    c.apply_translation((x, y, (z0 + z1) / 2))
    return M(c)


def rot_x(m, pivot, deg):
    """Rotate manifold m about the x axis through pivot (y, z); positive deg takes +y toward +z."""
    y0, z0 = pivot
    return m.translate((0, -y0, -z0)).rotate((deg, 0, 0)).translate((0, y0, z0))


def sweep(m, pivot, deg, n=7):
    """Union of m rotated by 0..deg (the swept volume), deg negative for the pulled direction."""
    return mo.union([rot_x(m, pivot, d) for d in np.linspace(0.0, deg, n)])


def fin(x0, x1, y_open, y_h, z_h, y_far, z_top, z_bot=-30.0, open_dir="+z"):
    """Hinge fin plate x0..x1 with a snap slot for a pin at (y_h, z_h); the slot opens toward `open_dir` (+z or -y)."""
    r = P.HINGE_SLOT / 2
    plate = box(x0, x1, y_open, y_far, z_bot, z_top)
    if open_dir == "+z":
        slot = cyl_x(x0 - 1, x1 + 1, y_h, z_h, r) + box(x0 - 1, x1 + 1, y_h - P.HINGE_MOUTH / 2, y_h + P.HINGE_MOUTH / 2, z_h, z_top + 1)
    else:      # open toward -y
        slot = cyl_x(x0 - 1, x1 + 1, y_h, z_h, r) + box(x0 - 1, x1 + 1, y_open - 1, y_h, z_h - P.HINGE_MOUTH / 2, z_h + P.HINGE_MOUTH / 2)
    return plate - slot


def prism_yz(poly_pts, x0, x1):
    """Polygon in (y, z) extruded across x0..x1."""
    pr = trimesh.creation.extrude_polygon(Polygon(poly_pts), height=x1 - x0)
    v = pr.vertices
    m = trimesh.Trimesh(np.column_stack([v[:, 2] + x0, v[:, 0], v[:, 1]]), pr.faces, process=True)
    if m.volume < 0:
        m.invert()
    return M(m)


def prism_xz(poly, y0, y1):
    """Shapely polygon in (x, z) extruded along y0..y1."""
    pr = trimesh.creation.extrude_polygon(poly, height=y1 - y0)
    v = pr.vertices
    m = trimesh.Trimesh(np.column_stack([v[:, 0], v[:, 2] + y0, v[:, 1]]), pr.faces, process=True)
    if m.volume < 0:
        m.invert()
    return M(m)


def trigger(Ht):
    """PS5-R2 style trigger: a curved hook on the end dome below the bumper, finger dish on its face, flat lower lip on the table plane.
    Two stems carry it through the belly to a hinge inside; tails over the belly carry the magnet and the spring post.
    Returns dict(hook, frame, rigid, sweep, cut, fins, ...). The frame drops in from inside; the hook is glued on from below."""
    x0, x1 = P.TRIG_X
    ZF = P.TRIG_Z_FLAT
    prof = [P.TRIG_PROFILE[0]] + list(P.TRIG_PROFILE[1:]) + [(P.TRIG_BACK_Y, ZF), (P.TRIG_BACK_Y, -14.0)]
    hook = prism_yz(prof, x0, x1)
    from shapely.geometry import box as sbox
    rr = sbox(x0, ZF, x1, -7.6).buffer(-P.TRIG_CORNER_R).buffer(P.TRIG_CORNER_R)
    hook = hook ^ prism_xz(rr, 90.0, 125.0)
    cx = (x0 + x1) / 2
    R, dd, yref = P.TRIG_DISH_R, P.TRIG_DISH_DEPTH, P.TRIG_DISH_Y
    dish = trimesh.creation.cylinder(radius=R, height=40.0, sections=96)
    dish.apply_translation((cx, yref + R - dd, -13.0))
    hook = hook - M(dish)
    # keep clear of the shell: close under the belly, 3.5 mm off the end dome (the pull closes the gap)
    O_belly = mo.outer_offset(Ht, P.TRIG_GAP_BELLY)
    O_dome = mo.outer_offset(Ht, P.TRIG_GAP_DOME)
    hook = hook - (O_belly ^ box(0, 40, 80.0, 103.0, -30, 5)) - (O_dome ^ box(0, 40, 103.0, 125.0, -30, 5))
    yh, zh = P.TRIG_PIVOT
    ys0, ys1 = P.TRIG_STEM_Y
    frame = None
    parts = []
    for sx0, sx1 in P.TRIG_STEMS:
        parts.append(box(sx0, sx1, ys0, ys1, P.TRIG_STEM_Z0, zh + 1.4))
    for tx0, tx1 in P.TRIG_TAILS_X:
        parts.append(box(tx0, tx1, P.TRIG_TAIL_Y0, ys1, P.TRIG_TAIL_Z[0], P.TRIG_TAIL_Z[1]))
    pr = P.HINGE_PIN_D / 2
    (a0, a1), (b0, b1) = P.TRIG_STEMS
    parts.append(cyl_x(a0 - P.HINGE_PIN_LEN, a0 + 0.2, yh, zh, pr))
    parts.append(cyl_x(b1 - 0.2, b1 + P.HINGE_PIN_LEN, yh, zh, pr))
    ztail = P.TRIG_TAIL_Z[1]
    sx, sy = P.SPRING_XY
    parts.append(cyl_z(sx, sy, ztail - 0.5, ztail + P.SPRING_POST_H, P.SPRING_POST_D / 2))
    frame = mo.union(parts)
    mx, my = P.MAG_XY
    frame = frame - cyl_z(mx, my, ztail - P.MAG_T - 0.1, ztail + 1.0, (P.MAG_D + 0.15) / 2)
    # mortises for the stems' tenons in the hook (0.1 mm clearance each side; glue joint)
    for sx0, sx1 in P.TRIG_STEMS:
        hook = hook - box(sx0 - 0.1, sx1 + 0.1, ys0 - 0.1, ys1 + 0.1, P.TRIG_STEM_Z0 - 0.1, P.TRIG_STEM_Z0 + 6.0)
    rigid = hook + frame
    sw = sweep(rigid, P.TRIG_PIVOT, -P.TRIG_THETA_MAX)
    cut = grow(sw, P.FLAP_GAP)
    ft, gap = P.HINGE_FIN_T, P.HINGE_FIN_GAP
    z_top = zh + 2.5
    fins = [fin(a0 - gap - ft, a0 - gap, yh - 4.0, yh, zh, yh + 4.0, z_top),
            fin(b1 + gap, b1 + gap + ft, yh - 4.0, yh, zh, yh + 4.0, z_top)]
    return dict(hook=hook, frame=frame, rigid=rigid, sweep=sw, cut=cut, fins=fins)


def grow(m, d):
    """Minkowski-style growth of a manifold by d via vertex-normal offset (parts here are convex-ish and chunky)."""
    t = mo.to_trimesh(m)
    g = t.copy()
    g.vertices = t.vertices + t.vertex_normals * d
    return mo.from_trimesh(g)


def bumper(Ht):
    """Bar across the screen end, conformal to the end wall (0.5 proud), hinged above on two pins; a prong presses the corner switch.
    Ht: tessellated hollow shell (trimesh). Returns dict(bar, sweep, cut, fins)."""
    x0, x1 = P.BUMP_X
    z0, z1 = P.BUMP_Z
    O_out = mo.outer_offset(Ht, P.BUMP_PROUD)
    IL_in, IL_gap = mo.cavity_offsets(Ht, (-P.BUMP_INSET, -0.3))
    region = box(x0, x1, 90.0, 125.0, z0, z1)
    bar = (region ^ O_out) - IL_in
    yp, zp = P.BUMP_PIVOT
    ear_w = 1.8
    bar = bar + (box(x0, x0 + ear_w, yp - 1.4, yp + 2.6, z1 - 1.0, zp + 1.4) ^ O_out) + (box(x1 - ear_w, x1, yp - 1.4, yp + 2.6, z1 - 1.0, zp + 1.4) ^ O_out)
    pr = P.HINGE_PIN_D / 2
    bar = bar + cyl_x(x0 - P.HINGE_PIN_LEN, x0 + 0.2, yp, zp, pr) + cyl_x(x1 - 0.2, x1 + P.HINGE_PIN_LEN, yp, zp, pr)
    # prong to the switch: a tab on the bar's inner face reaching out to the switch's x, then a pin along -y to the actuator
    sx, sy, sz = P.BUMP_SWITCH
    tab = box(sx - 1.5, x0 + 1.5, 103.0, 125.0, sz - 1.6, sz + 1.6) ^ IL_gap
    bar = bar + tab + cyl_z_y(sx, sz, sy, 106.5, P.BUMP_PRONG_D / 2)
    sw = sweep(bar, P.BUMP_PIVOT, -P.BUMP_THETA_MAX)
    cut = grow(sw, P.FLAP_GAP)
    O_in = mo.outer_offset(Ht, -0.6)
    ft, gap = P.HINGE_FIN_T, P.HINGE_FIN_GAP
    fins = [fin(x0 - gap - ft, x0 - gap, yp - 1.8, yp, zp, yp + 6.0, zp + 3.5, z_bot=zp - 3.5, open_dir="-y"),
            fin(x1 + gap, x1 + gap + ft, yp - 1.8, yp, zp, yp + 6.0, zp + 3.5, z_bot=zp - 3.5, open_dir="-y")]
    return dict(bar=bar, sweep=sw, cut=cut, fins=[f ^ O_in for f in fins])


def cyl_z_y(x, z, y0, y1, r, sections=40):
    """Cylinder along y from y0 to y1 at (x, z)."""
    c = trimesh.creation.cylinder(radius=r, height=y1 - y0, sections=sections)
    c.apply_transform(trimesh.transformations.rotation_matrix(-math.pi / 2, (1, 0, 0)))
    c.apply_translation((x, (y0 + y1) / 2, z))
    return M(c)
