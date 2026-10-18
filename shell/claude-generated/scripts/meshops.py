"""Mesh booleans (manifold3d). OpenCascade booleans against the spline shell take minutes and often return invalid
solids; on meshes they take well under a second. The shell is tessellated once (0.02 mm), small features and cutters are
built as OCC solids and tessellated, and every union / cut / clip / split runs here. Output is STL anyway.
"""
import numpy as np
import trimesh
import manifold3d as m3d


def tess(shape, tol=0.02, ang=0.2):
    """OCC shape -> Manifold."""
    v, f = shape.tessellate(tol, ang)
    m = trimesh.Trimesh([(p.X, p.Y, p.Z) for p in v], f, process=False)
    m.merge_vertices()
    return from_trimesh(m)


def from_trimesh(m):
    M = m3d.Manifold(m3d.Mesh(np.asarray(m.vertices, np.float32), np.asarray(m.faces, np.uint32)))
    if M.status() != m3d.Error.NoError:
        raise ValueError("mesh is not a valid manifold: %s" % M.status())
    return M


def to_trimesh(M):
    mesh = M.to_mesh()
    return trimesh.Trimesh(np.asarray(mesh.vert_properties)[:, :3], np.asarray(mesh.tri_verts), process=False)


def union(parts):
    out = parts[0]
    for p in parts[1:]:
        out = out + p
    return out


def above(M, z):
    return M.trim_by_plane((0, 0, 1), z)


def below(M, z):
    return M.trim_by_plane((0, 0, -1), -z)


def _ok32(t):
    """does the mesh survive the float32 STL round trip as a watertight solid?"""
    r = trimesh.Trimesh(np.asarray(t.vertices, np.float32).astype(np.float64), t.faces, process=True)
    return r.is_watertight

def unpinch(t, d=0.006):
    """Manifold allows two surfaces to touch at a point (distinct vertices at one position). Once STL welds by position
    that is a 4-face edge. Pull every vertex of such a coincident group slightly toward the centre of its own faces."""
    v = np.asarray(t.vertices, np.float64).copy()
    f = np.asarray(t.faces)
    key = np.round(v * 1e4).astype(np.int64)
    _, inv, cnt = np.unique(key, axis=0, return_inverse=True, return_counts=True)
    inv = inv.reshape(-1)
    dup = cnt[inv] > 1
    if not dup.any():
        return t
    cen = v[f].mean(axis=1)
    acc = np.zeros_like(v); n = np.zeros(len(v))
    for k in range(3):
        np.add.at(acc, f[:, k], cen - v[f[:, k]]); np.add.at(n, f[:, k], 1)
    dirn = acc / np.maximum(n, 1)[:, None]
    nrm = np.linalg.norm(dirn, axis=1, keepdims=True)
    v[dup] += d * dirn[dup] / np.maximum(nrm[dup], 1e-9)
    return trimesh.Trimesh(v, f, process=False)


def clean(M, min_volume=1.0, tol=0.02):
    """Manifold -> trimesh. Simplify collapses coincident-face slivers (they would vanish in STL float32 and open
    holes), then floating specks are dropped. If the result pinches in float32, other tolerances are tried."""
    best = None
    for tl in (tol, 0.01, 0.04, 0.005, 0.08, 0.0025, 0.12):
        t = to_trimesh(M.simplify(tl))
        parts = [p for p in t.split(only_watertight=False) if abs(p.volume) >= min_volume]
        t = parts[0] if len(parts) == 1 else trimesh.util.concatenate(parts)
        best = best or t
        if _ok32(t):
            return t
    u = unpinch(best)
    return u if _ok32(u) else best


def cavity_offsets(H, distances):
    """The cavity of the hollow shell, grown outward by each distance (vertex-normal offset). H is the tessellated
    hollow shell (trimesh): its inner surface is the shell with negative volume."""
    inner = [p for p in H.split(only_watertight=False) if p.volume < 0]
    IL = inner[0].copy()
    IL.invert()                                                           # cavity as a positive solid
    out = []
    for d in distances:
        g = IL.copy()
        g.vertices = IL.vertices + IL.vertex_normals * d
        out.append(from_trimesh(g))
    return out


def box_mesh(lo=(-60.0, -60.0, -60.0), hi=(80.0, 160.0, 60.0)):
    b = trimesh.creation.box(extents=np.array(hi) - np.array(lo))
    b.apply_translation((np.array(hi) + np.array(lo)) / 2)
    return from_trimesh(b)


def outer_offset(H, d):
    """The outer skin of the hollow shell (positive-volume component of the tessellated shell) moved along its vertex normals by d
    (d < 0 goes inward), as a solid. Used to clip things that must merge into the wall without poking through."""
    outer = [p for p in H.split(only_watertight=False) if p.volume > 0][0]
    g = outer.copy()
    g.vertices = outer.vertices + outer.vertex_normals * d
    return from_trimesh(g)
