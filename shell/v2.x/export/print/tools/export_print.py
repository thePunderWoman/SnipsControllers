"""Raw Fusion STL -> print-orientation STL (v2). Run from this folder: python3 export_print.py
Reads export/stl, writes export/print/stl. No supports are generated (see ../README.md)."""
import os, struct
import numpy as np
import trimesh
import export_print_cfg as C
import paths as P

def rotm(ax, deg):
    a = np.radians(deg); c, s = np.cos(a), np.sin(a)
    return {'x': np.array([[1, 0, 0], [0, c, -s], [0, s, c]]),
            'y': np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]]),
            'z': np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])}[ax]

def orient(tris, rots):
    t = tris.copy()
    for ax, d in rots:
        t = t @ rotm(ax, d).T
    t[:, :, 2] -= t[:, :, 2].min()
    return t

def write(path, tris, name):
    n = len(tris); nr = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
    nr = nr / np.maximum(np.linalg.norm(nr, axis=1, keepdims=True), 1e-12)
    arr = np.zeros(n, dtype=np.dtype([('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')]))
    arr['n'] = nr; arr['v'] = tris
    with open(path, 'wb') as f:
        f.write(name.encode()[:80].ljust(80, b'\0')); f.write(struct.pack('<I', n)); f.write(arr.tobytes())

def centre_xy(t):
    t = t.copy()
    t[:, :, 0] -= (t[:, :, 0].min() + t[:, :, 0].max()) / 2
    t[:, :, 1] -= (t[:, :, 1].min() + t[:, :, 1].max()) / 2
    return t

os.makedirs(P.OUT_STL, exist_ok=True)
for n, (rots, note) in C.ORI.items():
    m = trimesh.load(os.path.join(P.SRC_STL, n + '.stl'))
    t = centre_xy(orient(m.triangles, rots))
    write(os.path.join(P.OUT_STL, n + '.stl'), t, n)
    sz = t.reshape(-1, 3).max(0) - t.reshape(-1, 3).min(0)
    print('%-13s %.1f x %.1f x %.1f  %s' % (n, sz[0], sz[1], sz[2], note))

# TriggerCaps: split the two lumps, lay each flat, place side by side
m = trimesh.load(os.path.join(P.SRC_STL, 'TriggerCaps.stl'))
lumps = sorted(m.split(only_watertight=False), key=lambda b: b.centroid[0])
assert len(lumps) == 2, len(lumps)
parts = [orient(b.triangles, C.CAPS_ROT) for b in lumps]
parts = [centre_xy(p) for p in parts]
w = [p[:, :, 0].max() - p[:, :, 0].min() for p in parts]
parts[0][:, :, 0] -= (w[0] + C.CAPS_GAP) / 2
parts[1][:, :, 0] += (w[1] + C.CAPS_GAP) / 2
allt = np.concatenate(parts)
write(os.path.join(P.OUT_STL, 'TriggerCaps.stl'), allt, 'TriggerCaps')
sz = allt.reshape(-1, 3).max(0) - allt.reshape(-1, 3).min(0)
print('%-13s %.1f x %.1f x %.1f  two plates flat on the bed' % ('TriggerCaps', sz[0], sz[1], sz[2]))
