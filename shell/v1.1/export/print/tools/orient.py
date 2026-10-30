import numpy as np, struct, sys, math, os, json

def load(f):
    b = open(f, 'rb').read()
    n = struct.unpack('<I', b[80:84])[0]
    a = np.frombuffer(b[84:84 + n * 50], dtype=np.dtype([('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')]))
    return a['v'].astype(np.float64)  # (n,3,3)

def rot(axis, deg):
    c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    if axis == 'x': return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])
    if axis == 'y': return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.eye(3)

ORIENTS = {
    '+Z up (as modelled)': np.eye(3),
    '-Z up (flipped)': rot('x', 180),
    '+Y up': rot('x', -90),
    '-Y up': rot('x', 90),
    '+X up': rot('y', 90),
    '-X up': rot('y', -90),
}
for ax in ('x', 'y'):
    for d in (30, 45, -30, -45):
        ORIENTS['tilt %s %+d' % (ax, d)] = rot(ax, d)
        ORIENTS['flip+tilt %s %+d' % (ax, d)] = rot(ax, d) @ rot('x', 180)

def analyse(tris, R, cell=0.6, thresh=0.7071):
    t = tris @ R.T
    t[:, :, 2] -= t[:, :, 2].min()
    e1 = t[:, 1] - t[:, 0]; e2 = t[:, 2] - t[:, 0]
    nrm = np.cross(e1, e2); area = np.linalg.norm(nrm, axis=1) / 2
    nz = np.where(area > 0, nrm[:, 2] / (2 * np.maximum(area, 1e-12)), 0)
    H = t[:, :, 2].max()
    # bed contact: downward faces with all z < 0.15
    bed = area[(nz < -0.99) & (t[:, :, 2].max(axis=1) < 0.15)].sum()
    x0, y0 = t[:, :, 0].min(), t[:, :, 1].min()
    nx = int((t[:, :, 0].max() - x0) / cell) + 2; ny = int((t[:, :, 1].max() - y0) / cell) + 2
    cols = {}
    for i in range(len(t)):
        if area[i] < 1e-9 or abs(nrm[i, 2]) < 1e-9: continue
        p = t[i]
        xs = (p[:, 0] - x0) / cell; ys = (p[:, 1] - y0) / cell
        ia, ib = int(math.floor(xs.min())), int(math.ceil(xs.max()))
        ja, jb = int(math.floor(ys.min())), int(math.ceil(ys.max()))
        gx, gy = np.meshgrid(np.arange(ia, ib + 1) + 0.5, np.arange(ja, jb + 1) + 0.5, indexing='ij')
        # barycentric in xy (cell units)
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-12: continue
        l1 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        l2 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        l3 = 1 - l1 - l2
        m = (l1 >= 0) & (l2 >= 0) & (l3 >= 0)
        if not m.any(): continue
        z = l1 * p[0, 2] + l2 * p[1, 2] + l3 * p[2, 2]
        for a, b in zip(*np.nonzero(m)):
            cols.setdefault((ia + a, ja + b), []).append((z[a, b], nz[i]))
    sup_vol = 0.0; sup_area = 0.0; floating = 0.0
    c2 = cell * cell
    for k, lst in cols.items():
        lst.sort()
        # dedupe near-equal crossings
        ded = []
        for z, n in lst:
            if ded and abs(z - ded[-1][0]) < 1e-3: continue
            ded.append((z, n))
        top = 0.0
        for z, n in ded:
            if n < 0:
                gap = z - top
                if gap > 0.25 and n < -thresh:
                    sup_vol += gap * c2; sup_area += c2
            else:
                top = z
    vol = float(np.einsum('ij,ij->i', t[:, 0], np.cross(t[:, 1], t[:, 2])).sum() / 6)
    return dict(H=H, bed=bed, sup_area=sup_area, sup_vol=sup_vol, vol=abs(vol))

if __name__ == '__main__':
    out = {}
    d = sys.argv[1]
    only = sys.argv[2:]
    for f in sorted(os.listdir(d)):
        if not f.endswith('.stl'): continue
        n = f[:-4]
        if only and n not in only: continue
        tris = load(os.path.join(d, f))
        cell = 0.6 if len(tris) > 5000 else 0.3
        rows = []
        for name, R in ORIENTS.items():
            r = analyse(tris, R, cell)
            rows.append((name, r))
        rows.sort(key=lambda x: x[1]['sup_vol'])
        print('== %s  (%d tris, vol %.0f mm3)' % (n, len(tris), rows[0][1]['vol']))
        for name, r in rows[:6]:
            print('  %-24s H %5.1f  bed %6.1f mm2  support %7.0f mm3 (%4.0f%% of part)  overhang area %6.0f mm2' % (
                name, r['H'], r['bed'], r['sup_vol'], 100 * r['sup_vol'] / r['vol'], r['sup_area']))
        base = [r for nm, r in rows if nm.startswith('+Z')][0]
        print('  [as modelled: support %.0f mm3, bed %.0f mm2, H %.1f]' % (base['sup_vol'], base['bed'], base['H']))
        sys.stdout.flush()
