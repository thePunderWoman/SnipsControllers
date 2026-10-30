# Snips shell -> Fusion 360 rebuild helpers.
# SolidCraft coordinates (mm) are shifted to a tidy origin:
#   X: shell centre = 0, Y: shell centre = 0 (front +Y), Z: seam plane = 0 (Z up)
import adsk.core as core
import adsk.fusion as fus
import json, math, traceback

app = core.Application.get()
OX, OY, OZ = 137.5, -104.75, -2.2
DATA = '/Users/jessica/repos/SnipsControllers/shell/v1.1/fusion/'

_sk = None
_ft = None


def design():
    return fus.Design.cast(app.activeProduct)


def root():
    return design().rootComponent


def sketches_data():
    global _sk
    if _sk is None:
        _sk = json.load(open(DATA + 'sc_sketches.json'))
    return _sk


def feats_data():
    global _ft
    if _ft is None:
        _ft = json.load(open(DATA + 'sc_features.json'))
    return _ft


def Pm(x, y, z):
    """SolidCraft mm point -> Fusion (cm) Point3D in tidy frame."""
    return core.Point3D.create((x - OX) / 10.0, (y - OY) / 10.0, (z - OZ) / 10.0)


def mm(v):
    return v / 10.0


def VI(expr):
    return core.ValueInput.createByString(expr)


def VR(v_mm):
    return core.ValueInput.createByReal(v_mm / 10.0)


# ---------- components ----------
def get_comp(name):
    r = root()
    for o in r.occurrences:
        if o.component.name == name:
            return o
    occ = r.occurrences.addNewComponent(core.Matrix3D.create())
    occ.component.name = name
    return occ


# ---------- parameters ----------
def uparam(name, expr, unit='mm', comment=''):
    ps = design().userParameters
    p = ps.itemByName(name)
    if p:
        return p
    return ps.add(name, VI(expr), unit, comment)


# ---------- planes ----------
def s2m(plane, u, v):
    o, x, y = plane['origin'], plane['x_dir'], plane['y_dir']
    return (o[0] + u * x[0] + v * y[0], o[1] + u * x[1] + v * y[1], o[2] + u * x[2] + v * y[2])


def plane_normal(plane):
    x, y = plane['x_dir'], plane['y_dir']
    n = (x[1] * y[2] - x[2] * y[1], x[2] * y[0] - x[0] * y[2], x[0] * y[1] - x[1] * y[0])
    l = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2)
    return (n[0] / l, n[1] / l, n[2] / l)


def _offset_plane(comp, base, d_cm, axis, want_cm, name):
    """offset plane perpendicular to model `axis`; flips sign if Fusion's base normal is opposite"""
    for sign in (1.0, -1.0):
        inp = comp.constructionPlanes.createInput()
        inp.setByOffset(base, core.ValueInput.createByReal(sign * d_cm))
        cp = comp.constructionPlanes.add(inp)
        o = cp.geometry.origin
        val = [o.x, o.y, o.z][axis]
        if abs(val - want_cm) < 1e-6:
            cp.name = name
            return cp
        cp.deleteMe()
    raise RuntimeError('offset plane failed ' + name)


_slope_ref = {}


def slope_plane(comp, plane, name):
    """plane rotated about X, e.g. the OLED window plane (normal in YZ)."""
    for p in comp.constructionPlanes:
        if p.name == name:
            return p
    o = plane['origin']
    n = plane_normal(plane)
    ang = math.atan2(abs(n[1]), abs(n[2]))  # tilt of normal away from Z
    key = (comp.name, round(o[1], 3), round(o[2], 3), round(ang, 5))
    # reference horizontal plane through the axis point
    zp = None
    zname = 'Z_ref_%s' % round(o[2], 3)
    for p in comp.constructionPlanes:
        if p.name == zname:
            zp = p
    if zp is None:
        zp = _offset_plane(comp, comp.xYConstructionPlane, (o[2] - OZ) / 10.0, 2, (o[2] - OZ) / 10.0, zname)
    # axis line sketch
    ak = comp.sketches.add(zp)
    ak.name = 'Axis_' + name
    a = ak.modelToSketchSpace(Pm(o[0] - 10, o[1], o[2]))
    b = ak.modelToSketchSpace(Pm(o[0] + 10, o[1], o[2]))
    ln = ak.sketchCurves.sketchLines.addByTwoPoints(a, b)
    for sign in (1.0, -1.0):
        inp = comp.constructionPlanes.createInput()
        inp.setByAngle(ln, core.ValueInput.createByReal(sign * ang), zp)
        cp = inp and comp.constructionPlanes.add(inp)
        g = cp.geometry
        gn = g.normal
        dot = gn.x * n[0] + gn.y * n[1] + gn.z * n[2]
        if abs(abs(dot) - 1) < 1e-6:
            cp.name = name
            ak.isVisible = False
            return cp
        cp.deleteMe()
    raise RuntimeError('slope plane failed ' + name)


def cplane(comp, plane, name):
    """Construction plane from a SolidCraft plane dict (axis aligned or tilted about X)."""
    for p in comp.constructionPlanes:
        if p.name == name:
            return p
    o = plane['origin']
    n = plane_normal(plane)
    if abs(abs(n[2]) - 1) < 1e-6:
        return _offset_plane(comp, comp.xYConstructionPlane, (o[2] - OZ) / 10.0, 2, (o[2] - OZ) / 10.0, name)
    if abs(abs(n[0]) - 1) < 1e-6:
        return _offset_plane(comp, comp.yZConstructionPlane, (o[0] - OX) / 10.0, 0, (o[0] - OX) / 10.0, name)
    if abs(abs(n[1]) - 1) < 1e-6:
        return _offset_plane(comp, comp.xZConstructionPlane, (o[1] - OY) / 10.0, 1, (o[1] - OY) / 10.0, name)
    if abs(n[0]) < 1e-6:
        # tilted about X: reference slope plane (OLED window plane) then offset along its normal
        ref = slope_plane(comp, WIN_PLANE, 'OLED window plane')
        nref = plane_normal(WIN_PLANE)
        w = WIN_PLANE['origin']
        d = (o[0] - w[0]) * nref[0] + (o[1] - w[1]) * nref[1] + (o[2] - w[2]) * nref[2]
        if abs(d) < 1e-6:
            if name != 'OLED window plane':
                pass
            return ref
        target = Pm(*o)
        for sign in (1.0, -1.0):
            inp = comp.constructionPlanes.createInput()
            inp.setByOffset(ref, core.ValueInput.createByReal(sign * d / 10.0))
            cp = comp.constructionPlanes.add(inp)
            g = cp.geometry
            v = core.Vector3D.create(target.x - g.origin.x, target.y - g.origin.y, target.z - g.origin.z)
            if abs(v.dotProduct(g.normal)) < 1e-6:
                cp.name = name
                return cp
            cp.deleteMe()
        raise RuntimeError('tilted offset plane failed ' + name)
    raise RuntimeError('unsupported plane for ' + name)


WIN_PLANE = {'origin': [137.5, -65.96, 22.17], 'x_dir': [1.0, 0.0, 0.0], 'y_dir': [0.0, 0.8910013365, 0.454000681]}


# ---------- sketches ----------
def draw_sketch(comp, sid, name=None, plane_name=None, only=None, skip=None):
    """Create sketch from SolidCraft sketch id; returns Fusion sketch.
    only/skip: sets of curve ids to include/exclude."""
    d = sketches_data()[str(sid)]
    plane = d['plane']
    cp = cplane(comp, plane, plane_name or ('P_' + (name or d['name'])))
    sk = comp.sketches.add(cp)
    sk.name = name or d['name']
    sk.isComputeDeferred = True
    sk.arePointsShown = False
    for c in d['curves']:
        if only is not None and c['id'] not in only:
            continue
        if skip is not None and c['id'] in skip:
            continue
        if c['k']:
            continue
        if c['t'] == 'l':
            a = sk.modelToSketchSpace(Pm(*s2m(plane, *c['a'])))
            b = sk.modelToSketchSpace(Pm(*s2m(plane, *c['b'])))
            sk.sketchCurves.sketchLines.addByTwoPoints(a, b)
        elif c['t'] == 'c':
            m = sk.modelToSketchSpace(Pm(*s2m(plane, *c['c'])))
            sk.sketchCurves.sketchCircles.addByCenterRadius(m, c['r'] / 10.0)
        elif c['t'] == 'q':
            pts = []
            for key in ('a', 'c', 'b'):
                pts.append(sk.modelToSketchSpace(Pm(*s2m(plane, *c[key]))))
            sk.sketchCurves.sketchConicCurves.add(pts[0], pts[2], pts[1], c.get('rho') or 0.5)
        elif c['t'] == 'a':
            cx, cy = c['c']
            a0 = math.atan2(c['a'][1] - cy, c['a'][0] - cx)
            a1 = math.atan2(c['b'][1] - cy, c['b'][0] - cx)
            while a1 <= a0:
                a1 += 2 * math.pi
            am = (a0 + a1) / 2
            r = c['r']
            mid = (cx + r * math.cos(am), cy + r * math.sin(am))
            pa = sk.modelToSketchSpace(Pm(*s2m(plane, *c['a'])))
            pm_ = sk.modelToSketchSpace(Pm(*s2m(plane, *mid)))
            pb = sk.modelToSketchSpace(Pm(*s2m(plane, *c['b'])))
            sk.sketchCurves.sketchArcs.addByThreePoints(pa, pm_, pb)
    sk.isComputeDeferred = False
    return sk


def profile_centroid_sc(sk, prof):
    """Centroid of a profile in SolidCraft mm coordinates."""
    c = prof.areaProperties(fus.CalculationAccuracy.MediumCalculationAccuracy).centroid
    c = sk.sketchToModelSpace(c)  # centroid is reported in sketch space
    return (c.x * 10 + OX, c.y * 10 + OY, c.z * 10 + OZ)


def pick_profiles(sk, sid=None, indices=None):
    """Return ObjectCollection of profiles. indices refer to SolidCraft profile
    indices (matched by centroid); None -> all profiles."""
    col = core.ObjectCollection.create()
    if indices is None:
        for p in sk.profiles:
            col.add(p)
        return col
    d = sketches_data()[str(sid)]
    plane = d['plane']
    want = []
    for pr in d['profiles']:
        if pr['i'] in indices:
            want.append(s2m(plane, *pr['c']))
    for p in sk.profiles:
        c = profile_centroid_sc(sk, p)
        for w in want:
            if math.dist(c, w) < 0.05:
                col.add(p)
                break
    return col


# ---------- features ----------
def sketch_normal(sk):
    xd, yd = sk.xDirection, sk.yDirection
    return xd.crossProduct(yd)


def extrude(comp, sk, sid, profiles, dist_expr, op, name, direction='positive', targets=None,
            start_offset=None, symmetric=False, new_body_name=None):
    """SolidCraft-style extrude: direction is relative to the SolidCraft sketch normal."""
    ops = {'new': fus.FeatureOperations.NewBodyFeatureOperation,
           'join': fus.FeatureOperations.JoinFeatureOperation,
           'cut': fus.FeatureOperations.CutFeatureOperation,
           'intersect': fus.FeatureOperations.IntersectFeatureOperation}
    pl = sid if isinstance(sid, dict) else sketches_data()[str(sid)]['plane']
    nsc = plane_normal(pl)
    nf = sketch_normal(sk)
    same = (nf.x * nsc[0] + nf.y * nsc[1] + nf.z * nsc[2]) > 0
    inp = comp.features.extrudeFeatures.createInput(profiles, ops[op])
    if targets:
        inp.participantBodies = targets
    dist = VI(dist_expr)
    if symmetric:
        inp.setSymmetricExtent(dist, True)
        sgn = 1.0
    else:
        want_pos_sc = (direction == 'positive')
        fusion_pos = want_pos_sc if same else (not want_pos_sc)
        ext = fus.DistanceExtentDefinition.create(dist)
        inp.setOneSideExtent(ext, fus.ExtentDirections.PositiveExtentDirection if fusion_pos
                             else fus.ExtentDirections.NegativeExtentDirection)
        sgn = 1.0 if fusion_pos else -1.0
    if start_offset is not None:
        # SolidCraft offset is measured along the extrude direction; Fusion's along sketch normal
        inp.startExtent = fus.OffsetStartDefinition.create(VI('(%s)*%d' % (start_offset, int(sgn))))
    f = comp.features.extrudeFeatures.add(inp)
    f.name = name
    if new_body_name and f.bodies.count:
        f.bodies.item(0).name = new_body_name
    return f


def body_by_name(comp, name):
    for b in comp.bRepBodies:
        if b.name == name:
            return b
    return None


def edge_near(body, p_sc, tol=0.02):
    """Find edge of body passing through SolidCraft point p_sc (mm)."""
    pt = Pm(*p_sc)
    best, bd = None, 1e9
    for e in body.edges:
        ev = e.evaluator
        ok, pp = ev.getParameterAtPoint(pt)
        if not ok:
            continue
        ok, pc = ev.getPointAtParameter(pp)
        if not ok:
            continue
        dd = pc.distanceTo(pt)
        if dd < bd:
            bd, best = dd, e
    return best if bd < tol else None


def fillet(comp, body, pts_sc, radius_expr, name, tangent=True):
    es = core.ObjectCollection.create()
    miss = []
    for p in pts_sc:
        e = edge_near(body, p)
        if e is None:
            miss.append(p)
        else:
            es.add(e)
    if miss:
        raise RuntimeError('fillet %s: edges not found at %s' % (name, miss))
    inp = comp.features.filletFeatures.createInput()
    inp.addConstantRadiusEdgeSet(es, VI(radius_expr), tangent)
    f = comp.features.filletFeatures.add(inp)
    f.name = name
    return f


def props(body):
    p = body.physicalProperties
    bb = body.boundingBox
    return {'name': body.name, 'vol_mm3': round(p.volume * 1000, 2),
            'min': [round(bb.minPoint.x * 10 + OX, 3), round(bb.minPoint.y * 10 + OY, 3), round(bb.minPoint.z * 10 + OZ, 3)],
            'max': [round(bb.maxPoint.x * 10 + OX, 3), round(bb.maxPoint.y * 10 + OY, 3), round(bb.maxPoint.z * 10 + OZ, 3)],
            'shells': body.shells.count, 'faces': body.faces.count}


def draw_custom(comp, name, plane, circles=(), polys=(), rects=()):
    """plane: SolidCraft plane dict. circles: [(u,v,r)], polys: [[(u,v),...] closed], in plane 2D coords (mm)."""
    cp = cplane(comp, plane, 'P_' + name)
    sk = comp.sketches.add(cp)
    sk.name = name
    sk.isComputeDeferred = True
    for (u, v, r) in circles:
        m = sk.modelToSketchSpace(Pm(*s2m(plane, u, v)))
        sk.sketchCurves.sketchCircles.addByCenterRadius(m, r / 10.0)
    for pts in polys:
        n = len(pts)
        for i in range(n):
            a = sk.modelToSketchSpace(Pm(*s2m(plane, *pts[i])))
            b = sk.modelToSketchSpace(Pm(*s2m(plane, *pts[(i + 1) % n])))
            sk.sketchCurves.sketchLines.addByTwoPoints(a, b)
    sk.isComputeDeferred = False
    return sk


def zplane(z, ox=137.5, oy=-104.75):
    return {'origin': [ox, oy, z], 'x_dir': [1, 0, 0], 'y_dir': [0, 1, 0]}


# ---------- data-driven SolidCraft feature replay ----------
BODY_MAP = {'TopShell2': 'TopShell', 'BottomShell2': 'BottomShell'}
LOG = []


def _num(expr):
    try:
        return float(expr)
    except Exception:
        return None


def do_extrude(comp, fid, name=None, targets=None, only=None, indices=None, new_body_name=None,
               sk_name=None, profile_filter=None, sketch_override=None):
    """Replay SolidCraft ExtrudeFeature `fid` in Fusion `comp`."""
    fd = feats_data()[str(fid)]
    sid = sketch_override or fd['sketch']
    sd = sketches_data()[str(sid)]
    sk = draw_sketch(comp, sid, name=sk_name or sd['name'], only=only)
    if profile_filter is not None:
        profs = core.ObjectCollection.create()
        for p in sk.profiles:
            a = p.areaProperties(fus.CalculationAccuracy.MediumCalculationAccuracy).area * 100
            if profile_filter(a):
                profs.add(p)
    else:
        profs = pick_profiles(sk, sid, indices)
    ex = fd['extent']
    direction = ex['direction']
    dist = ex['distance']
    n = _num(dist)
    if n is not None:
        if n < 0:
            direction = 'negative' if direction == 'positive' else 'positive'
            n = -n
        dist = '%s mm' % repr(n) if abs(n - round(n)) > 1e-9 else '%d mm' % round(n)
    so = ex.get('start_offset')
    if so is not None and _num(so) is not None:
        so = '%s mm' % so
    op = {'new_body': 'new', 'join': 'join', 'cut': 'cut'}[fd['operation']]
    tg = None
    if op != 'new':
        tg = []
        for t in (targets or [BODY_MAP[x] for x in fd.get('targets', [])]):
            b = body_by_name(comp, t)
            if b is None:
                raise RuntimeError('target body %s not found in %s' % (t, comp.name))
            tg.append(b)
    f = extrude(comp, sk, sid, profs, dist, op, name or fd.get('name') or ('Extrude %s' % fid),
                direction=('positive' if direction == 'symmetric' else direction), targets=tg,
                start_offset=so, symmetric=(direction == 'symmetric'), new_body_name=new_body_name)
    return f


def step(comp, label, fn):
    """run fn(), log result volume of bodies in comp"""
    n0 = comp.sketches.count
    try:
        fn()
        vols = ', '.join('%s=%.1f' % (b.name, b.physicalProperties.volume * 1000) for b in comp.bRepBodies if b.name != 'Cavity')
        LOG.append('OK  %s | %s' % (label, vols))
    except Exception:
        LOG.append('ERR %s | %s' % (label, traceback.format_exc().splitlines()[-1]))
        for i in range(comp.sketches.count - 1, n0 - 1, -1):
            try:
                comp.sketches.item(i).deleteMe()
            except Exception:
                pass


def ids_in_region(sid, center, radius):
    """curve ids of sketch `sid` whose reference point lies within radius of center (sketch uv)."""
    out = set()
    for c in sketches_data()[str(sid)]['curves']:
        if c['t'] == 'l':
            p = ((c['a'][0] + c['b'][0]) / 2, (c['a'][1] + c['b'][1]) / 2)
        elif c['t'] == 'c':
            p = tuple(c['c'])
        elif c['t'] == 'q':
            p = ((c['a'][0] + c['b'][0]) / 2, (c['a'][1] + c['b'][1]) / 2)
        else:
            p = tuple(c['c'])
        if math.hypot(p[0] - center[0], p[1] - center[1]) <= radius:
            out.add(c['id'])
    return out


def move_body(comp, body, dx, dy, dz, name):
    col = core.ObjectCollection.create()
    col.add(body)
    t = core.Matrix3D.create()
    t.translation = core.Vector3D.create(dx / 10.0, dy / 10.0, dz / 10.0)
    inp = comp.features.moveFeatures.createInput2(col)
    inp.defineAsFreeMove(t)
    f = comp.features.moveFeatures.add(inp)
    f.name = name
    return f


def profiles_no_counters(sk):
    """All profiles except those that merely fill another profile's hole (glyph counters)."""
    inner_tokens = set()
    profs = [p for p in sk.profiles]
    for p in profs:
        for lp in p.profileLoops:
            if not lp.isOuter:
                for pc in lp.profileCurves:
                    inner_tokens.add(pc.sketchEntity.entityToken)
    col = core.ObjectCollection.create()
    for p in profs:
        outer = [lp for lp in p.profileLoops if lp.isOuter]
        toks = set()
        for lp in outer:
            for pc in lp.profileCurves:
                toks.add(pc.sketchEntity.entityToken)
        if toks and toks.issubset(inner_tokens):
            continue
        col.add(p)
    return col
