"""Equator split: half-lap seam (top shell overhangs, bottom shell underhangs) plus hidden interior snap clips.

Assembly: the PCB drops into the bottom shell (posts + ribs carry its back face), then the top shell lowers over it
(posts, pins and ribs hang from above). Bottom ribs are bottom-shell features; the part of a rib above the seam is a free
tab that stays TAB_GAP away from the top wall, so the top wall can lower past it.

Clips: each site has a ledge on the top shell's inner face and a snap arm rising from the bottom shell inside the
cavity. The arm's barb hooks over the ledge; the top shell cannot lift off. Nothing shows on the outside.
Local clip frame: t = distance into the cavity from the cavity face (probed at z = 0), v = along the wall, z = height.
"""
import numpy as np
import trimesh
import params as P
import meshops as mo

FACE_Z = 2.0     # reference height for the cavity face: the barb height, where the leaning wall is closest


def seam_rings(probe):
    """Outer and cavity outlines (shapely) of the hollow shell at the seam plane."""
    from shapely.geometry import Polygon
    sec = probe.mesh.section(plane_origin=(0, 0, P.SEAM_Z), plane_normal=(0, 0, 1))
    loops = sorted((Polygon(l[:, :2]) for l in sec.discrete if len(l) > 3), key=lambda g: -g.area)
    return loops[0], loops[1]


def ring_solid(poly, z0, z1):
    """Extrude a shapely polygon (with holes) from z0 to z1 as an OCC solid."""
    from build123d import Face, Wire, Vector, extrude

    def wire(coords):
        return Wire.make_polygon([Vector(x, y, z0) for x, y in list(coords)[:-1]], close=True)
    f = Face(wire(poly.exterior.coords), [wire(h.coords) for h in poly.interiors])
    return extrude(f, z1 - z0, dir=(0, 0, 1))


def _outer_outline(probe, z):
    from shapely.geometry import Polygon
    sec = probe.mesh.section(plane_origin=(0, 0, z), plane_normal=(0, 0, 1))
    return max((Polygon(l[:, :2]) for l in sec.discrete if len(l) > 3), key=lambda g: g.area)


def lap_zones(probe, dz=0.5):
    """Manifolds (top ring, bottom ring, sub ring) that pick the lip, the step and the feature trim out of the hollow
    shell. Built as a stack of dz-thick slabs, each from the shell's own outline at that height, so the skins keep their
    thickness where the dome ends lean (a single vertical ring cut through the underhang there). The shell supplies the
    true outer surface because every ring reaches 3 mm outside it. The lip and the step are nested slab by slab, so they
    never touch."""
    depths = (P.WALL / 2 - P.LAP_GAP / 2, P.WALL / 2 + P.LAP_GAP / 2, P.WALL / 2 + P.LAP_GAP / 2 - 0.05)
    z_lo = P.SEAM_Z - P.LAP_H - P.LAP_FLOOR - 1.0
    z_hi = P.SEAM_Z + 0.5
    slabs = [[] for _ in depths]
    z = z_lo
    while z < z_hi - 1e-9:
        outer = _outer_outline(probe, z + dz / 2)
        for k, d in enumerate(depths):
            ring = outer.buffer(3.0).difference(outer.buffer(-d))
            m = trimesh.creation.extrude_polygon(ring, dz)
            m.apply_translation((0, 0, z))
            slabs[k].append(mo.from_trimesh(m))
        z += dz
    return [mo.union(s) for s in slabs]


def _frame(site, probe):
    """(face coordinate, inward sign, along-axis) for a clip site, probed on the hollow shell at z = 0."""
    wall, v = site
    if wall in ("L", "R"):
        o = (-30.0, v, FACE_Z) if wall == "L" else (70.0, v, FACE_Z)
        d = (1, 0, 0) if wall == "L" else (-1, 0, 0)
        w = probe.walls(o, d)
        face = o[0] + (w[1] if wall == "L" else -w[1])
        return dict(axis="x", s=1 if wall == "L" else -1, face=face, v=v)
    o = (v, 140.0, FACE_Z)
    w = probe.walls(o, (0, -1, 0))
    return dict(axis="y", s=-1, face=o[1] - w[1], v=v)


def _prism(fr, pts_tz, v0, v1):
    """Polygon in (t, z) extruded along v from v0 to v1, as a Manifold."""
    from shapely.geometry import Polygon
    m = trimesh.creation.extrude_polygon(Polygon(pts_tz), v1 - v0)        # local x = t, y = z, z = v (0..v1-v0)
    m.apply_translation((0, 0, v0))
    T = np.eye(4)
    if fr["axis"] == "x":                                                 # t -> +-X, z -> Y(height), v -> Y? no: v -> world Y, z -> world Z
        T[:3, :3] = np.array([[fr["s"], 0, 0], [0, 0, 1], [0, 1, 0]], float)   # (t,z,v) -> (X=s*t, Y=v, Z=z)
        T[0, 3] = fr["face"]
    else:
        T[:3, :3] = np.array([[0, 0, 1], [fr["s"], 0, 0], [0, 1, 0]], float)   # (t,z,v) -> (X=v, Y=s*t, Z=z)
        T[1, 3] = fr["face"]
    m.apply_transform(T)
    if m.volume < 0:
        m.invert()
    return mo.from_trimesh(m)


def clip_pair(site, probe):
    """(arm for the bottom shell, ledge for the top shell, info) at one site."""
    fr = _frame(site, probe)
    h = P.CLIP_ARM_W / 2
    v0, v1 = fr["v"] - h, fr["v"] + h
    a0, a1 = P.CLIP_OFF, P.CLIP_OFF + P.CLIP_ARM_T                        # arm spans t in [a0, a1]
    z_ledge_top = P.LEDGE_Z0 + P.LEDGE_H
    zb0 = z_ledge_top + P.CLIP_FIT                                        # barb underside
    zb1 = zb0 + P.BARB_H
    z_tip = zb1 + P.BARB_P + 0.5
    # bottom foot web: wider than the arm, reaching 1.0 into the wall at the foot height (clipped by the inset later)
    foot = _prism(fr, [(-3.0, P.CLIP_Z0), (a1, P.CLIP_Z0), (a1, P.CLIP_Z0 + P.CLIP_FOOT_H), (-3.0, P.CLIP_Z0 + P.CLIP_FOOT_H)], v0, v1)
    arm = _prism(fr, [(a0, P.CLIP_Z0), (a1, P.CLIP_Z0), (a1, z_tip), (a0 + 0.3, z_tip), (a0, zb1 + P.BARB_P)][::1], v0, v1)
    barb = _prism(fr, [(a0 + 0.01, zb0), (a0 - P.BARB_P, zb0), (a0 - P.BARB_P, zb1), (a0, zb1 + P.BARB_P)], v0, v1)
    ledge = _prism(fr, [(-1.2, P.LEDGE_Z0), (0.0, P.LEDGE_Z0), (P.LEDGE_P, P.LEDGE_Z0 + P.LEDGE_P),
                        (P.LEDGE_P, z_ledge_top), (-1.2, z_ledge_top)], v0, v1)
    info = dict(name="clip", wall=site[0], v=site[1], axis=fr["axis"], s=fr["s"], face=round(fr["face"], 2),
                x0=None, y0=None, z0=P.CLIP_Z0, z1=round(z_tip, 2), t1=a1, half_w=h)
    return foot + arm + barb, ledge, info


def clip_sites(feats):
    """Candidate positions (params), greedily picked: no overlap with a bottom rib tab on the same wall, at least
    CLIP_SPACING apart, at most CLIP_PER_WALL per long side."""
    ok = []
    for wall, cands in P.CLIP_CANDIDATES:
        picked = []
        for v in cands:
            if wall in ("L", "R"):
                ribs = [f for f in feats if f["name"] == "bottom_rib_" + wall]
                if any(f["y1"] > v - P.CLIP_ARM_W / 2 - 1.0 and f["y0"] < v + P.CLIP_ARM_W / 2 + 1.0 for f in ribs):
                    continue
                if len(picked) >= P.CLIP_PER_WALL or any(abs(v - u) < P.CLIP_SPACING for u in picked):
                    continue
            picked.append(v)
            ok.append((wall, v))
    return ok
