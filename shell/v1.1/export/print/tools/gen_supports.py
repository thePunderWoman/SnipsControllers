import numpy as np, math, os, json, sys
from scipy import ndimage
import orient as O

def heightfield(tris, R, cell, thresh=0.7071):
    t = tris @ R.T
    zmin = float(t[:, :, 2].min()); t[:, :, 2] -= zmin
    e1 = t[:, 1]-t[:, 0]; e2 = t[:, 2]-t[:, 0]
    nrm = np.cross(e1, e2); area = np.linalg.norm(nrm, axis=1)/2
    nz = np.where(area > 0, nrm[:, 2]/(2*np.maximum(area, 1e-12)), 0)
    x0, y0 = float(t[:, :, 0].min()), float(t[:, :, 1].min())
    nx = int((t[:, :, 0].max()-x0)/cell)+3; ny = int((t[:, :, 1].max()-y0)/cell)+3
    cols = {}
    for i in range(len(t)):
        if area[i] < 1e-9 or abs(nrm[i, 2]) < 1e-9: continue
        p = t[i]; xs = (p[:, 0]-x0)/cell; ys = (p[:, 1]-y0)/cell
        ia, ib = int(math.floor(xs.min())), int(math.ceil(xs.max())); ja, jb = int(math.floor(ys.min())), int(math.ceil(ys.max()))
        gx, gy = np.meshgrid(np.arange(ia, ib+1)+0.5, np.arange(ja, jb+1)+0.5, indexing='ij')
        d = (ys[1]-ys[2])*(xs[0]-xs[2])+(xs[2]-xs[1])*(ys[0]-ys[2])
        if abs(d) < 1e-12: continue
        l1 = ((ys[1]-ys[2])*(gx-xs[2])+(xs[2]-xs[1])*(gy-ys[2]))/d; l2 = ((ys[2]-ys[0])*(gx-xs[2])+(xs[0]-xs[2])*(gy-ys[2]))/d; l3 = 1-l1-l2
        m = (l1 >= 0)&(l2 >= 0)&(l3 >= 0)
        if not m.any(): continue
        z = l1*p[0, 2]+l2*p[1, 2]+l3*p[2, 2]
        for a, b in zip(*np.nonzero(m)): cols.setdefault((ia+a, ja+b), []).append((z[a, b], nz[i]))
    ov = np.zeros((nx, ny), bool); zs = np.zeros((nx, ny)); base = np.zeros((nx, ny))
    for (i, j), lst in cols.items():
        lst.sort(); top = 0.0; last = None; best = None
        for z, n in lst:
            if last is not None and abs(z-last) < 1e-3: continue
            last = z
            if n < 0:
                g = z-top
                if g > 0.25 and n < -thresh and best is None: best = (z, top)   # lowest overhang in the column
            else: top = z
        if best: ov[i, j] = True; zs[i, j] = best[0]; base[i, j] = best[1]
    return ov, zs, base, x0, y0, zmin

def pillars(tris, R, cell=0.3, pitch=2.4, edge_margin=0.6):
    ov, zs, base, x0, y0, zmin = heightfield(tris, R, cell)
    lab, n = ndimage.label(ov, structure=np.ones((3, 3)))
    s = int(round(pitch/cell)); out = []
    for k in range(1, n+1):
        idx = np.argwhere(lab == k)
        if len(idx)*cell*cell < 1.5: continue
        # candidate = cells on the pitch lattice, at least edge_margin from region boundary
        er = ndimage.distance_transform_edt(lab == k)*cell
        sel = [(i, j) for i, j in idx if i % s == 0 and j % s == 0 and er[i, j] >= edge_margin]
        if not sel:
            # fall back: deepest interior cell
            q = max(map(tuple, idx), key=lambda c: er[c]); sel = [q]
        for i, j in sel:
            out.append([round(x0+(i+0.5)*cell, 3), round(y0+(j+0.5)*cell, 3), round(float(base[i, j]), 3), round(float(zs[i, j]), 3)])
    return out, zmin

if __name__ == '__main__':
    import paths as P
    d = P.SRC_STL
    jobs = {'BumperCap': '-Z up (flipped)', 'Trigger': '-X up'}
    res = {}
    for n, o in jobs.items():
        tris = O.load(os.path.join(d, n+'.stl'))
        R = O.ORIENTS[o]
        p, zmin = pillars(tris, R)
        res[n] = dict(orientation=o, R=R.tolist(), zmin=zmin, pillars=p)
        hs = [q[3]-q[2] for q in p]
        print(n, o, 'pillars', len(p), 'height min/avg/max %.1f/%.1f/%.1f' % (min(hs), sum(hs)/len(hs), max(hs)))
    json.dump(res, open('pillars.json', 'w'))
