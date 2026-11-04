"""Helpers for building the Snips v2 shell in Fusion (units: cm in the API, mm in expressions).
Run inside Fusion's script engine; import with sys.path.insert(0, <this folder>)."""
import adsk.core, adsk.fusion, math
P3 = adsk.core.Point3D.create
VI = adsk.core.ValueInput.createByString


def app_design():
    app = adsk.core.Application.get()
    return app, adsk.fusion.Design.cast(app.activeProduct)


def plan(sk, d, y_front='y_front_out', y_rear='y_rear_out', hw='shell_w / 2', rf='r_front_out', rr='r_rear_out'):
    """Parametric rounded-rectangle plan, inset by expression d from the outer outline.
    Front/rear corner radii differ; every dimension is a user-parameter expression."""
    mm = 0.1
    L = sk.sketchCurves.sketchLines
    A = sk.sketchCurves.sketchArcs
    dm = sk.sketchDimensions
    o = sk.originPoint
    # seed geometry at the final size (cm) so the solver never flips
    app_ = adsk.core.Application.get()
    des_ = adsk.fusion.Design.cast(app_.activeProduct)
    ev = lambda e: des_.unitsManager.evaluateExpression(e, 'cm')
    dn = ev(d)
    x0 = ev(hw) - dn
    yf = ev(y_front) - dn
    yr = ev(y_rear) + dn
    rect = L.addTwoPointRectangle(P3(-x0, yr, 0), P3(x0, yf, 0))
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    hs = [l for l in lines if horiz(l)]
    vs = [l for l in lines if not horiz(l)]
    rear = min(hs, key=lambda l: l.startSketchPoint.geometry.y)
    front = max(hs, key=lambda l: l.startSketchPoint.geometry.y)
    left = min(vs, key=lambda l: l.startSketchPoint.geometry.x)
    right = max(vs, key=lambda l: l.startSketchPoint.geometry.x)
    def off(line, expr, tp):
        dim = dm.addOffsetDimension(line, o, tp)
        dim.parameter.expression = expr
        return dim
    off(front, '%s - (%s)' % (y_front, d), P3(x0 + 1, yf / 2, 0))
    off(rear, '-(%s + (%s))' % (y_rear, d), P3(x0 + 1, yr / 2, 0))
    off(left, '%s - (%s)' % (hw, d), P3(-x0 / 2, yr - 1, 0))
    off(right, '%s - (%s)' % (hw, d), P3(x0 / 2, yr - 1, 0))
    def fil(l1, p1, l2, p2, r):
        arc = A.addFillet(l1, p1, l2, p2, ev('%s - (%s)' % (r, d)))
        dim = dm.addRadialDimension(arc, P3(arc.centerSketchPoint.geometry.x + (0.5 if arc.centerSketchPoint.geometry.x > 0 else -0.5), arc.centerSketchPoint.geometry.y + (0.5 if arc.centerSketchPoint.geometry.y > 0 else -0.5), 0))
        dim.parameter.expression = '%s - (%s)' % (r, d)
        return arc
    fil(front, P3(x0 - 0.5, yf, 0), right, P3(x0, yf - 0.5, 0), rf)
    fil(front, P3(-x0 + 0.5, yf, 0), left, P3(-x0, yf - 0.5, 0), rf)
    fil(rear, P3(x0 - 0.5, yr, 0), right, P3(x0, yr + 0.5, 0), rr)
    fil(rear, P3(-x0 + 0.5, yr, 0), left, P3(-x0, yr + 0.5, 0), rr)
    gc = sk.geometricConstraints
    for l in (front, rear):
        try: gc.addHorizontal(l)
        except RuntimeError: pass
    for l in (left, right):
        try: gc.addVertical(l)
        except RuntimeError: pass
    return sk


def main_profile(sk):
    """Largest-area profile of the sketch."""
    return max(sk.profiles, key=lambda p: p.areaProperties().area)


def profile_by_loops(sk, n_loops):
    """Return profiles of the sketch whose profileLoops count equals n_loops."""
    return [p for p in sk.profiles if p.profileLoops.count == n_loops]


FE = adsk.fusion.FeatureOperations


def ext(comp, prof, expr, op, name, neg=False, target=None, start=None, taper=None):
    """Parametric one-sided extrude; expr is a parameter expression."""
    inp = comp.features.extrudeFeatures.createInput(prof, op)
    if taper is None:
        inp.setDistanceExtent(False, VI(('-(%s)' % expr) if neg else expr))
    else:
        inp.setOneSideExtent(adsk.fusion.DistanceExtentDefinition.create(VI(expr)),
                             adsk.fusion.ExtentDirections.NegativeExtentDirection if neg else adsk.fusion.ExtentDirections.PositiveExtentDirection,
                             VI(taper))
    if start is not None:
        inp.startExtent = adsk.fusion.OffsetStartDefinition.create(VI(start))
    if target is not None:
        inp.participantBodies = [target] if not isinstance(target, (list, tuple)) else list(target)
    f = comp.features.extrudeFeatures.add(inp)
    f.name = name
    return f


def new_component(root, name):
    for o in list(root.occurrences):
        if o.component.name == name:
            o.deleteMe()
    occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    occ.component.name = name
    occ.activate()   # build inside the component so cuts resolve their target occurrence
    return occ.component


def planar_face_at(body, z_cm, nz):
    """Planar face at height z whose outward normal points along sign(nz) in z."""
    for fc in body.faces:
        g = fc.geometry
        if g.objectType.endswith('Plane') and abs(fc.pointOnFace.z - z_cm) < 1e-4:
            ok, n = fc.evaluator.getNormalAtPoint(fc.pointOnFace)
            if ok and abs(n.z - nz) < 1e-6:
                return fc
    raise RuntimeError('face not found at z=%s' % z_cm)


def fillet_face_edges(comp, face, radius_expr, name):
    col = adsk.core.ObjectCollection.create()
    for e in face.edges:
        col.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(col, VI(radius_expr), True)
    f = comp.features.filletFeatures.add(fi)
    f.name = name
    return f


def cut_tool(comp, des, target, plan_d, depth_expr, neg, start, face_z_expr, face_nz, fillet_expr, name):
    """Cavity as a filleted tool body combined-cut from target (keeps wall thickness through fillets)."""
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan %s' % name.lower()
    plan(sk, plan_d)
    f = ext(comp, main_profile(sk), depth_expr, FE.NewBodyFeatureOperation, '%s tool' % name, neg=neg, start=start)
    tool = f.bodies.item(0); tool.name = '%s tool' % name
    fillet_face_edges(comp, planar_face_at(tool, des.unitsManager.evaluateExpression(face_z_expr, 'cm'), face_nz), fillet_expr, '%s tool fillet' % name)
    tools = adsk.core.ObjectCollection.create(); tools.add(tool)
    ci = comp.features.combineFeatures.createInput(target, tools)
    ci.operation = FE.CutFeatureOperation
    ci.isKeepToolBodies = False
    cf = comp.features.combineFeatures.add(ci); cf.name = name
    return cf


def build_top_shell(root, des):
    """Outer prism + top edge fillet + seam lip (below seam) + seam groove + cavity."""
    comp = new_component(root, 'TopShell')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan outer'
    plan(sk, '0 mm')
    f = ext(comp, main_profile(sk), 'top_z', FE.NewBodyFeatureOperation, 'Top outer')
    body = f.bodies.item(0); body.name = 'TopShell'
    fillet_face_edges(comp, planar_face_at(body, des.unitsManager.evaluateExpression('top_z', 'cm'), 1), 'edge_r', 'Top edge fillet')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan seam lip ring'
    plan(sk, '0 mm'); plan(sk, 'lip_w')
    ext(comp, profile_by_loops(sk, 2)[0], 'lap_h - seam_clr', FE.JoinFeatureOperation, 'Seam lip', neg=True)
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan seam groove'
    plan(sk, 'lip_w')
    ext(comp, main_profile(sk), 'lap_h', FE.CutFeatureOperation, 'Seam groove', target=body)
    cut_tool(comp, des, body, 'wall', 'top_z - roof_t', False, None, 'top_z - roof_t', 1, 'edge_r - wall', 'Cavity')
    return comp


def build_bottom_shell(root, des):
    """Lower block below the seam ledge, bottom edge fillet, cavity, seam rib."""
    comp = new_component(root, 'BottomShell')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan outer'
    plan(sk, '0 mm')
    f = ext(comp, main_profile(sk), 'bot_d - lap_h', FE.NewBodyFeatureOperation, 'Bottom outer', neg=True, start='-lap_h')
    body = f.bodies.item(0); body.name = 'BottomShell'
    zb = -des.unitsManager.evaluateExpression('bot_d', 'cm')
    fillet_face_edges(comp, planar_face_at(body, zb, -1), 'edge_r', 'Bottom edge fillet')
    cut_tool(comp, des, body, 'wall', 'bot_d - lap_h - floor_t', True, '-lap_h', '-(bot_d - floor_t)', -1, 'edge_r - wall', 'Cavity')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan seam rib ring'
    plan(sk, 'lip_w + seam_clr'); plan(sk, 'wall')
    ext(comp, profile_by_loops(sk, 2)[0], '2*lap_h - seam_clr', FE.JoinFeatureOperation, 'Seam rib', start='-lap_h')
    return comp


# ---------------------------------------------------------------- reference-model helpers
def ev_cm(des, expr):
    return des.unitsManager.evaluateExpression(expr, 'cm')


def rect4(sk, des, left, right, bottom, top):
    """Axis-aligned rectangle from four signed edge-position expressions (none on an axis);
    each edge is dimensioned to the origin by its expression, so it stays parametric."""
    L = sk.sketchCurves.sketchLines
    dm = sk.sketchDimensions
    o = sk.originPoint
    xl, xr, yb, yt = [ev_cm(des, e) for e in (left, right, bottom, top)]
    rect = L.addTwoPointRectangle(P3(xl, yb, 0), P3(xr, yt, 0))
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    hs = [l for l in lines if horiz(l)]; vs = [l for l in lines if not horiz(l)]
    ln = {'bottom': min(hs, key=lambda l: l.startSketchPoint.geometry.y), 'top': max(hs, key=lambda l: l.startSketchPoint.geometry.y),
          'left': min(vs, key=lambda l: l.startSketchPoint.geometry.x), 'right': max(vs, key=lambda l: l.startSketchPoint.geometry.x)}
    gc = sk.geometricConstraints
    gc.addHorizontal(ln['top']); gc.addHorizontal(ln['bottom']); gc.addVertical(ln['left']); gc.addVertical(ln['right'])
    def pos(expr, val):
        return '(%s)' % expr if val > 0 else '-(%s)' % expr
    d = dm.addOffsetDimension(ln['left'], o, P3((xl + xr) / 2, yb - 1, 0)); d.parameter.expression = pos(left, xl)
    d = dm.addOffsetDimension(ln['bottom'], o, P3(xr + 1, (yt + yb) / 2, 0)); d.parameter.expression = pos(bottom, yb)
    d = dm.addOffsetDimension(ln['left'], ln['right'].startSketchPoint, P3((xl + xr) / 2, yt + 1, 0)); d.parameter.expression = '(%s) - (%s)' % (right, left)
    d = dm.addOffsetDimension(ln['bottom'], ln['top'].startSketchPoint, P3(xl - 1, (yt + yb) / 2, 0)); d.parameter.expression = '(%s) - (%s)' % (top, bottom)
    return rect


def circle_at(sk, des, cx, cy, dia):
    """Circle with centre at signed (cx, cy) expressions and diameter dia (expression)."""
    c = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(ev_cm(des, cx), ev_cm(des, cy), 0), ev_cm(des, dia) / 2)
    dm = sk.sketchDimensions; o = sk.originPoint
    vx, vy = ev_cm(des, cx), ev_cm(des, cy)
    d1 = dm.addDistanceDimension(o, c.centerSketchPoint, adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation, P3(vx / 2, -1, 0))
    d1.parameter.expression = '(%s)' % cx if vx > 0 else '-(%s)' % cx
    d2 = dm.addDistanceDimension(o, c.centerSketchPoint, adsk.fusion.DimensionOrientations.VerticalDimensionOrientation, P3(vx + 1, vy / 2, 0))
    d2.parameter.expression = '(%s)' % cy if vy > 0 else '-(%s)' % cy
    d3 = dm.addDiameterDimension(c, P3(vx + 0.5, vy + 0.5, 0))
    d3.parameter.expression = dia
    return c


OLED_PARAMS = [
    ('oled_hole_d', '2 mm', 'OLED mounting hole diameter (drawing: R1.00)'),
    ('oled_hole_inset', '2 mm', 'OLED mounting hole centre inset from each PCB edge'),
    ('oled_notch_w', '14 mm', 'OLED PCB bottom-edge notch width (ESTIMATED from drawing)'),
    ('oled_notch_d', '2 mm', 'OLED PCB bottom-edge notch depth (ESTIMATED from drawing)'),
    ('oled_panel_w', '26.7 mm', 'OLED panel (glass) width'),
    ('oled_panel_h', '19.26 mm', 'OLED panel (glass) height'),
    ('oled_panel_top', '4.27 mm', 'OLED panel top edge, below PCB top edge'),
    ('oled_va_w', '23.744 mm', 'OLED visible area width'),
    ('oled_va_h', '12.864 mm', 'OLED visible area height'),
    ('oled_va_top', '5.37 mm', 'OLED visible area top, below PCB top edge'),
    ('oled_aa_w', '21.744 mm', 'OLED active area width'),
    ('oled_aa_h', '10.864 mm', 'OLED active area height'),
    ('oled_aa_top', '6.37 mm', 'OLED active area top, below PCB top edge'),
    ('oled_pad_pitch', '2.54 mm', 'OLED 4-pin pad pitch'),
    ('oled_pad_d', '1 mm', 'OLED pad hole diameter (ASSUMED)'),
]


def build_oled_ref(root, des):
    """Reference model of the purchased 27.5 x 27.8 mm 0.96in I2C OLED, flat, header edge toward the front."""
    up = des.userParameters
    for n, e, c in OLED_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    comp = new_component(root, 'OLED_Ref')
    z_pcb = 'oled_glass_top_z - oled_glass_t - oled_pcb_t'
    z_gl = 'oled_glass_top_z - oled_glass_t'
    hw = 'oled_pcb_w / 2'
    yt, yb = 'oled_front_y', 'oled_front_y - oled_pcb_l'
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'PCB outline'
    rect4(sk, des, '-(%s)' % hw, hw, yb, yt)
    f = ext(comp, main_profile(sk), 'oled_pcb_t', FE.NewBodyFeatureOperation, 'PCB', start=z_pcb)
    pcb = f.bodies.item(0); pcb.name = 'OLED_PCB'
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'PCB holes and pads'
    gc = sk.geometricConstraints; dm = sk.sketchDimensions
    H = adsk.fusion.DimensionOrientations
    def circ(x, y, d):
        return sk.sketchCurves.sketchCircles.addByCenterRadius(P3(ev_cm(des, x), ev_cm(des, y), 0), ev_cm(des, d) / 2)
    def dist(p1, p2, orient, expr, tp):
        dm.addDistanceDimension(p1, p2, orient, tp).parameter.expression = expr
    xi = 'oled_pcb_w / 2 - oled_hole_inset'
    c1 = circle_at(sk, des, xi, 'oled_front_y - oled_hole_inset', 'oled_hole_d')
    c2 = circ(xi, 'oled_front_y - oled_pcb_l + oled_hole_inset', 'oled_hole_d')
    gc.addVerticalPoints(c1.centerSketchPoint, c2.centerSketchPoint); gc.addEqual(c1, c2)
    dist(c1.centerSketchPoint, c2.centerSketchPoint, H.VerticalDimensionOrientation, 'oled_pcb_l - 2 * oled_hole_inset', P3(1.5, -3, 0))
    c3 = circ('-(%s)' % xi, 'oled_front_y - oled_hole_inset', 'oled_hole_d')
    gc.addHorizontalPoints(c1.centerSketchPoint, c3.centerSketchPoint); gc.addEqual(c1, c3)
    dist(c3.centerSketchPoint, c1.centerSketchPoint, H.HorizontalDimensionOrientation, '2 * (%s)' % xi, P3(0, -1.6, 0))
    c4 = circ('-(%s)' % xi, 'oled_front_y - oled_pcb_l + oled_hole_inset', 'oled_hole_d')
    gc.addVerticalPoints(c3.centerSketchPoint, c4.centerSketchPoint); gc.addHorizontalPoints(c2.centerSketchPoint, c4.centerSketchPoint); gc.addEqual(c1, c4)
    # header pads: four holes at pitch, on the same row as the upper mounting holes
    pads = [circ('%s * oled_pad_pitch' % k, 'oled_front_y - oled_hole_inset', 'oled_pad_d') for k in (-1.5, -0.5, 0.5, 1.5)]
    dm.addDiameterDimension(pads[0], P3(-0.5, -1.6, 0)).parameter.expression = 'oled_pad_d'
    gc.addHorizontalPoints(c1.centerSketchPoint, pads[0].centerSketchPoint)
    dm.addDistanceDimension(sk.originPoint, pads[0].centerSketchPoint, H.HorizontalDimensionOrientation, P3(-0.2, -1.0, 0)).parameter.expression = '-(-1.5 * oled_pad_pitch)'
    for i in (1, 2, 3):
        gc.addHorizontalPoints(pads[0].centerSketchPoint, pads[i].centerSketchPoint); gc.addEqual(pads[0], pads[i])
        dist(pads[i - 1].centerSketchPoint, pads[i].centerSketchPoint, H.HorizontalDimensionOrientation, 'oled_pad_pitch', P3(-0.5 + i * 0.25, -2.4, 0))
    profs = adsk.core.ObjectCollection.create()
    for p in sk.profiles:
        profs.add(p)
    ext(comp, profs, 'oled_pcb_t', FE.CutFeatureOperation, 'PCB holes', start=z_pcb, target=pcb)
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'PCB notch'
    rect4(sk, des, '-(oled_notch_w / 2)', 'oled_notch_w / 2', 'oled_front_y - oled_pcb_l - 1 mm', 'oled_front_y - oled_pcb_l + oled_notch_d')
    ext(comp, main_profile(sk), 'oled_pcb_t', FE.CutFeatureOperation, 'PCB notch', start=z_pcb, target=pcb)
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Panel'
    rect4(sk, des, '-(oled_panel_w / 2)', 'oled_panel_w / 2', 'oled_front_y - oled_panel_top - oled_panel_h', 'oled_front_y - oled_panel_top')
    f = ext(comp, main_profile(sk), 'oled_glass_t', FE.NewBodyFeatureOperation, 'Panel glass', start=z_gl)
    f.bodies.item(0).name = 'OLED_Glass'
    for nm, w, h, tp in (('View area', 'oled_va_w', 'oled_va_h', 'oled_va_top'), ('Active area', 'oled_aa_w', 'oled_aa_h', 'oled_aa_top')):
        sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = nm
        rect4(sk, des, '-(%s / 2)' % w, '%s / 2' % w, 'oled_front_y - %s - %s' % (tp, h), 'oled_front_y - %s' % tp)
        f = ext(comp, main_profile(sk), '0.05 mm', FE.NewBodyFeatureOperation, nm, start='oled_glass_top_z - 0.05 mm')
        f.bodies.item(0).name = 'OLED_' + nm.replace(' ', '')
    return comp


WINDOW_PARAMS = [
    ('oled_win_margin', '0.6 mm', 'Window bezel: margin beyond the active area, each side (at window bottom)'),
    ('oled_win_taper', '-25 deg', 'Window flare (negative = widens toward the top surface)'),
    ('oled_frame_w', '1.6 mm', 'Skirt tube wall around the window, under the roof'),
    ('oled_frame_gap', '0.1 mm', 'Gap between skirt tube bottom and glass top'),
]


def window_edges(extra='0 mm'):
    """Signed edge expressions of the window outline (+extra on every side)."""
    m = 'oled_win_margin + (%s)' % extra
    return ('-(oled_aa_w / 2 + %s)' % m, 'oled_aa_w / 2 + %s' % m,
            'oled_front_y - oled_aa_top - oled_aa_h - (%s)' % m, 'oled_front_y - oled_aa_top + (%s)' % m)


def build_oled_window(top_comp, des):
    """Skirt tube under the roof + tapered window through the roof (TopShell, end of timeline)."""
    up = des.userParameters
    for n, e, c in WINDOW_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'deg' if n.endswith('taper') else 'mm', c)
    top_comp.parentDesign.rootComponent.occurrences  # (keeps linter quiet)
    body = top_comp.bRepBodies.item(0)
    z0 = 'oled_glass_top_z + oled_frame_gap'
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'OLED skirt ring'
    rect4(sk, des, *window_edges('oled_frame_w'))
    rect4(sk, des, *window_edges())
    ext(top_comp, profile_by_loops(sk, 2)[0], 'top_z - roof_t + 0.2 mm - (%s)' % z0, FE.JoinFeatureOperation, 'OLED skirt', start=z0)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'OLED window'
    rect4(sk, des, *window_edges())
    ext(top_comp, main_profile(sk), 'top_z - (%s)' % z0, FE.CutFeatureOperation, 'OLED window', start=z0, target=top_comp.bRepBodies.item(0), taper='oled_win_taper')


MOUNT_PARAMS = [
    ('oled_pad_sz', '4 mm', 'OLED tape pad: width (x) of each corner pad'),
    ('oled_tape_t', '0.15 mm', 'OLED double-sided tape thickness between pad and PCB'),
    ('oled_peg_d', '1.8 mm', 'OLED locating peg diameter (module hole is 2.0)'),
]


def build_oled_mount(top_comp, des):
    """Four taped corner pads + locating pegs hanging from the roof underside (left side built, then mirrored)."""
    up = des.userParameters
    for n, e, c in MOUNT_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    body = top_comp.bRepBodies.item(0)
    z_pad = 'oled_glass_top_z - oled_glass_t + oled_tape_t'      # pad underside = PCB top + tape
    h_up = 'top_z - roof_t + 0.2 mm - (%s)' % z_pad               # run 0.2 mm into the roof
    xl = '-(oled_pcb_w / 2)'; xr = '-(oled_pcb_w / 2) + oled_pad_sz'
    feats = []
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'OLED pad front'
    rect4(sk, des, xl, xr, 'oled_front_y - oled_panel_top + 0.1 mm', 'oled_front_y')
    feats.append(ext(top_comp, main_profile(sk), h_up, FE.JoinFeatureOperation, 'OLED pad front', start=z_pad))
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'OLED pad rear'
    rect4(sk, des, xl, xr, 'oled_front_y - oled_pcb_l', 'oled_front_y - oled_pcb_l + oled_panel_top - 0.1 mm')
    feats.append(ext(top_comp, main_profile(sk), h_up, FE.JoinFeatureOperation, 'OLED pad rear', start=z_pad))
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'OLED pegs'
    gc = sk.geometricConstraints; dm = sk.sketchDimensions
    c1 = circle_at(sk, des, '-(oled_pcb_w / 2 - oled_hole_inset)', 'oled_front_y - oled_hole_inset', 'oled_peg_d')
    c2 = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(ev_cm(des, '-(oled_pcb_w / 2 - oled_hole_inset)'), ev_cm(des, 'oled_front_y - oled_pcb_l + oled_hole_inset'), 0), ev_cm(des, 'oled_peg_d') / 2)
    gc.addVerticalPoints(c1.centerSketchPoint, c2.centerSketchPoint); gc.addEqual(c1, c2)
    dm.addDistanceDimension(c1.centerSketchPoint, c2.centerSketchPoint, adsk.fusion.DimensionOrientations.VerticalDimensionOrientation, P3(-1, -3, 0)).parameter.expression = 'oled_pcb_l - 2 * oled_hole_inset'
    profs = adsk.core.ObjectCollection.create()
    for p in sk.profiles:
        profs.add(p)
    feats.append(ext(top_comp, profs, 'oled_pcb_t + oled_tape_t - 0.1 mm', FE.JoinFeatureOperation, 'OLED pegs', neg=True, start=z_pad))
    col = adsk.core.ObjectCollection.create()
    for f in feats:
        col.add(f)
    mi = top_comp.features.mirrorFeatures.createInput(col, top_comp.yZConstructionPlane)
    mf = top_comp.features.mirrorFeatures.add(mi); mf.name = 'OLED mount mirror'
    return mf


SCREW_PARAMS = [
    ('mh1_x', '-12.5 mm', 'PCB mounting hole 1 x (front left)'), ('mh1_y', '25.4 mm', 'PCB mounting hole 1 y'),
    ('mh2_x', '12.4 mm', 'PCB mounting hole 2 x (front right)'), ('mh2_y', '25.2 mm', 'PCB mounting hole 2 y'),
    ('mh3_x', '-12 mm', 'PCB mounting hole 3 x (rear left)'), ('mh3_y', '-34.5 mm', 'PCB mounting hole 3 y'),
    ('mh4_x', '12 mm', 'PCB mounting hole 4 x (rear right)'), ('mh4_y', '-39 mm', 'PCB mounting hole 4 y'),
    ('pcb_bot_z', '2.2 mm', 'PCB underside height'), ('pcb_top_z', '3.71 mm', 'PCB top face height'),
    ('screw_len', '12 mm', 'M2 self-tapping screw length'), ('post_engage', '4 mm', 'Screw thread engagement in the top post'),
    ('screw_head_seat_z', 'pcb_top_z + post_engage - screw_len', 'Height of the screw head seat (bottom of the head well)'),
    ('post_r', '3.6 mm', 'Post / column radius (merges with the side walls)'),
    ('post_top_z', '10.5 mm', 'Top of the PCB posts (clears the OLED module underside)'),
    ('pilot_d', '1.7 mm', 'Self-tapping pilot hole diameter'), ('screw_clr_d', '2.4 mm', 'Screw shaft clearance bore'),
    ('head_well_d', '4.6 mm', 'Screw head / driver well diameter'),
]
MH = [('mh1_x', 'mh1_y'), ('mh2_x', 'mh2_y'), ('mh3_x', 'mh3_y'), ('mh4_x', 'mh4_y')]


def hole_circles(sk, des, dia_expr):
    """Four circles at the PCB mounting holes: the first is dimensioned from the origin, each next one
    from the previous (avoids Fusion rejecting equal origin dimensions such as |x| = 12)."""
    cs = []
    H = adsk.fusion.DimensionOrientations
    dm = sk.sketchDimensions
    for i, (xe, ye) in enumerate(MH):
        if i == 0:
            cs.append(circle_at(sk, des, xe, ye, dia_expr))
            continue
        pxe, pye = MH[i - 1]
        c = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(ev_cm(des, xe), ev_cm(des, ye), 0), ev_cm(des, dia_expr) / 2)
        p = cs[-1]
        for orient, cur, prev in ((H.HorizontalDimensionOrientation, xe, pxe), (H.VerticalDimensionOrientation, ye, pye)):
            diff = ev_cm(des, cur) - ev_cm(des, prev)
            d = dm.addDistanceDimension(p.centerSketchPoint, c.centerSketchPoint, orient, P3(0.5 * i, 0.5 * i, 0))
            d.parameter.expression = ('(%s) - (%s)' if diff > 0 else '(%s) - (%s)'.replace('(%s) - (%s)', '-((%s) - (%s))')) % (cur, prev)
        sk.geometricConstraints.addEqual(cs[0], c)
        cs.append(c)
    return cs


def all_profiles(sk):
    col = adsk.core.ObjectCollection.create()
    for p in sk.profiles:
        col.add(p)
    return col


def build_top_posts(top_comp, des):
    up = des.userParameters
    for n, e, c in SCREW_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    body = top_comp.bRepBodies.item(0)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'PCB posts'
    hole_circles(sk, des, '2 * post_r')
    ext(top_comp, all_profiles(sk), 'post_top_z - pcb_top_z', FE.JoinFeatureOperation, 'PCB posts', start='pcb_top_z')
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'Post pilot holes'
    hole_circles(sk, des, 'pilot_d')
    ext(top_comp, all_profiles(sk), 'post_engage + 1 mm', FE.CutFeatureOperation, 'Post pilot holes', start='pcb_top_z', target=top_comp.bRepBodies.item(0))


def build_bottom_columns(bot_comp, des, d='bot_d'):
    up = des.userParameters
    body = bot_comp.bRepBodies.item(0)
    z_floor = '-(%s - floor_t)' % d
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'PCB columns'
    hole_circles(sk, des, '2 * post_r')
    ext(bot_comp, all_profiles(sk), 'pcb_bot_z - (%s)' % z_floor, FE.JoinFeatureOperation, 'PCB columns', start=z_floor)
    # trim anything outside the cavity above the rib top (that zone belongs to the top shell wall)
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'Column trim ring'
    plan(sk, '0 mm'); plan(sk, 'wall')
    ext(bot_comp, profile_by_loops(sk, 2)[0], '1 mm', FE.CutFeatureOperation, 'Column trim', start='lap_h - seam_clr', target=bot_comp.bRepBodies.item(0))
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'Screw head wells'
    hole_circles(sk, des, 'head_well_d')
    ext(bot_comp, all_profiles(sk), d + ' + screw_head_seat_z', FE.CutFeatureOperation, 'Screw head wells', start='-' + d, target=bot_comp.bRepBodies.item(0))
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'Screw bores'
    hole_circles(sk, des, 'screw_clr_d')
    ext(bot_comp, all_profiles(sk), 'pcb_bot_z - screw_head_seat_z + 0.1 mm', FE.CutFeatureOperation, 'Screw bores', start='screw_head_seat_z', target=bot_comp.bRepBodies.item(0))


def R(x0, x1, y0, y1, c='relief_clr'):
    """Edge expressions of a PCB-component bounding box (mm, Fusion frame) grown by the relief clearance."""
    return ('%s mm - %s' % (x0, c), '%s mm + %s' % (x1, c), '%s mm - %s' % (y0, c), '%s mm + %s' % (y1, c))


def _relief_param(des):
    up = des.userParameters
    if up.itemByName('relief_clr') is None:
        up.add('relief_clr', VI('0.3 mm'), 'mm', 'Clearance added around PCB components in post/column reliefs')


def build_top_reliefs(top_comp, des):
    """Pockets in the top posts for PCB-top components found colliding in interference checks against
    PCB_Reference (bounding boxes from the STEP, Fusion frame, + relief_clr): two 0402 caps near MH3, JST near MH4."""
    _relief_param(des)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'Post component reliefs'
    for box in ((-9.45, -8.95, -37.1, -36.1), (-11.9, -10.9, -37.45, -36.95), (8.25, 14.15, -36.45, -28.85)):
        rect4(sk, des, *R(*box))
    ext(top_comp, all_profiles(sk), 'post_top_z - pcb_top_z', FE.CutFeatureOperation, 'Post component reliefs', start='pcb_top_z', target=top_comp.bRepBodies.item(0))


def build_bottom_reliefs(bot_comp, des):
    """Pockets in the bottom columns for PCB-bottom parts and the XBee carrier."""
    _relief_param(des)
    for nm, box, z_lo in (
        ('XBee carrier relief', (-14.5, 7.5, -32.95, -15.95), '-0.85 mm - relief_clr'),
        ('Cap relief (bottom)', (-11.1, -9.5, -38.1, -37.3), '1.31 mm - relief_clr'),
        ('JST relief (bottom)', (8.25, 14.15, -36.45, -28.85), '-2.74 mm - relief_clr'),
        ('SOT-23 relief (bottom)', (-9.75, -7.25, 21.75, 24.75), '0.91 mm - relief_clr'),
    ):
        sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = nm
        rect4(sk, des, *R(*box))
        ext(bot_comp, main_profile(sk), 'pcb_bot_z - (%s)' % z_lo, FE.CutFeatureOperation, nm, start=z_lo, target=bot_comp.bRepBodies.item(0))




USB_PARAMS = [
    ('usb_x', '2 mm', 'USB-C receptacle centre x (from PCB STEP: -2.47..6.47)'),
    ('usb_z', '5 mm', 'USB-C receptacle / plug centre height (receptacle z 2.94..7.04, centre 4.99)'),
    ('usb_face_y', '-42.8 mm', 'USB-C receptacle front face y'),
    ('usb_stop_gap', '0.06 mm', 'Plug pocket stops this far short of the receptacle face'),
    ('usb_plug_w', '10.65 mm', 'USB-C plug housing width (Jess cable)'),
    ('usb_plug_h', '6.35 mm', 'USB-C plug housing height (Jess cable)'),
    ('usb_pocket_clr', '0.25 mm', 'Plug pocket clearance per side'),
    ('usb_mouth_chamfer', '0.6 mm', 'Chamfer on the plug pocket mouth'),
]


def usb_slot(sk, des, w, h):
    """Stadium (slot) outline, fully constrained, on an XZ sketch (sketch y = -model z), centred (usb_x, usb_z)."""
    cx, cy = ev_cm(des, 'usb_x'), -ev_cm(des, 'usb_z')
    r = ev_cm(des, '(%s) / 2' % h); L = ev_cm(des, '((%s) - (%s)) / 2' % (w, h))
    Ls = sk.sketchCurves.sketchLines; As = sk.sketchCurves.sketchArcs
    top = Ls.addByTwoPoints(P3(cx - L, cy + r, 0), P3(cx + L, cy + r, 0))
    bot = Ls.addByTwoPoints(P3(cx + L, cy - r, 0), P3(cx - L, cy - r, 0))
    ar = As.addByThreePoints(top.endSketchPoint, P3(cx + L + r, cy, 0), bot.startSketchPoint)
    al = As.addByThreePoints(bot.endSketchPoint, P3(cx - L - r, cy, 0), top.startSketchPoint)
    gc = sk.geometricConstraints; dm = sk.sketchDimensions
    gc.addHorizontal(top); gc.addHorizontal(bot)
    gc.addTangent(top, ar); gc.addTangent(bot, ar); gc.addTangent(top, al); gc.addTangent(bot, al)
    dm.addRadialDimension(ar, P3(cx + L + r + 0.3, cy + 0.3, 0)).parameter.expression = '(%s) / 2' % h
    dm.addDistanceDimension(al.centerSketchPoint, ar.centerSketchPoint, adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation, P3(cx, cy - r - 0.5, 0)).parameter.expression = '(%s) - (%s)' % (w, h)
    H = adsk.fusion.DimensionOrientations
    dm.addDistanceDimension(sk.originPoint, ar.centerSketchPoint, H.HorizontalDimensionOrientation, P3(cx, cy + r + 0.5, 0)).parameter.expression = 'usb_x + ((%s) - (%s)) / 2' % (w, h)
    dm.addDistanceDimension(sk.originPoint, ar.centerSketchPoint, H.VerticalDimensionOrientation, P3(cx + L + r + 0.6, cy / 2, 0)).parameter.expression = 'usb_z'
    return sk


def build_usb_pocket(comp, des, name, target):
    up = des.userParameters
    for n, e, c in USB_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = name
    usb_slot(sk, des, 'usb_plug_w + 2 * usb_pocket_clr', 'usb_plug_h + 2 * usb_pocket_clr')
    return ext(comp, main_profile(sk), 'usb_face_y - usb_stop_gap - (y_rear_out - 1 mm)', FE.CutFeatureOperation, name, start='y_rear_out - 1 mm', target=target)


def build_usb_bezel(comp, des, name_prefix, target, keep_above):
    """Closed stadium tube inside the shell around the plug pocket. keep_above=True keeps z >= seam gap (top shell),
    False keeps z <= lap seam (bottom shell). Needs the 'USB plug pocket' / 'USB plug notch (bottom)' sketch in comp."""
    up = des.userParameters
    for n, e, c in [('usb_bezel_t', '1 mm', 'USB bezel tube wall thickness'),
                    ('usb_bezel_split_top', 'lap_h + 0.2 mm', 'Top-shell bezel starts here (z), leaves the seam gap'),
                    ('usb_bezel_split_bot', 'lap_h - seam_clr', 'Bottom-shell bezel ends here (z)')]:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    w = 'usb_plug_w + 2 * usb_pocket_clr + 2 * usb_bezel_t'
    h = 'usb_plug_h + 2 * usb_pocket_clr + 2 * usb_bezel_t'
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = name_prefix + ' bezel outline'
    usb_slot(sk, des, w, h)
    start = 'y_rear_out + wall / 2'
    length = 'usb_face_y - usb_stop_gap - (%s)' % start
    ext(comp, main_profile(sk), length, FE.JoinFeatureOperation,
        name_prefix + ' bezel', start=start, target=target)
    # trim the half that belongs to the other shell, only inside the cavity (never the rear wall)
    t = comp.sketches.add(comp.xYConstructionPlane); t.name = name_prefix + ' bezel trim'
    rect4(t, des, 'usb_x - (usb_plug_w / 2 + usb_pocket_clr + usb_bezel_t) - 1 mm',
          'usb_x + (usb_plug_w / 2 + usb_pocket_clr + usb_bezel_t) + 1 mm',
          'y_rear_out + wall' if keep_above else 'y_rear_out + wall / 2 - 0.1 mm', 'usb_face_y - usb_stop_gap + 0.5 mm')
    if keep_above:
        ext(comp, main_profile(t), '5 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim', neg=True,
            start='usb_bezel_split_top', target=target)
    else:
        ext(comp, main_profile(t), '10 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim',
            start='usb_bezel_split_bot', target=target)
    # re-cut the bore through the bezel using the existing pocket sketch
    ps = [s for s in comp.sketches if s.name in ('USB plug pocket', 'USB plug notch (bottom)')]
    if ps:   # the bottom shell has no pocket sketch (only a sliver of bezel below the seam gap), so no bore re-cut
        ext(comp, main_profile(ps[0]), 'usb_face_y - usb_stop_gap - (y_rear_out - 1 mm)', FE.CutFeatureOperation,
            name_prefix + ' bezel bore', start='y_rear_out - 1 mm', target=target)


PWR_PARAMS = [
    ('pwr_x', '-12 mm', 'Power switch actuator centre x (PCB STEP)'),
    ('pwr_z', '5.55 mm', 'Power switch actuator centre height (z 4.74..6.35)'),
    ('pwr_act_y', '-41.95 mm', 'Power switch actuator tip y'),
    ('pwr_act_gap', '0.2 mm', 'Gap between flange front face and actuator tip at rest'),
    ('pwr_stem', '4 mm', 'Button stem square size (also the cap size)'),
    ('pwr_bore_clr', '0.2 mm', 'Stem-to-bore clearance per side'),
    ('pwr_tube_wall', '1.2 mm', 'Guide tube wall'),
    ('pwr_flange_w', '6.4 mm', 'Button flange width (x)'),
    ('pwr_flange_h', '6 mm', 'Button flange height (z)'),
    ('pwr_flange_t', '1.2 mm', 'Button flange thickness (y)'),
    ('pwr_stop_gap', '0.4 mm', 'Flange free travel back to the guide tube end'),
    ('pwr_floor_y', '-48.95 mm', 'Floor of the button channel in the rear wall'),
    ('pwr_cap_out', '0.6 mm', 'Cap face protrudes this far from the channel floor'),
    ('pwr_chan_w', '5.2 mm', 'Button channel width (x)'),
    ('pwr_chan_h', '6 mm', 'Button channel height (z)'),
    ('pwr_flange_front_y', 'pwr_act_y - pwr_act_gap', 'Flange front face y'),
    ('pwr_flange_rear_y', 'pwr_flange_front_y - pwr_flange_t', 'Flange rear face y'),
    ('pwr_tube_end_y', 'pwr_flange_rear_y - pwr_stop_gap', 'Guide tube front end y'),
    ('pwr_cap_y', 'pwr_floor_y - pwr_cap_out', 'Cap face y'),
]


def add_pwr_params(des):
    up = des.userParameters
    for n, e, c in PWR_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def zrect(sk, des, xc, zc, w, h):
    """Centred rectangle on an XZ-plane sketch (sketch y = -model z)."""
    return rect4(sk, des, '(%s) - (%s) / 2' % (xc, w), '(%s) + (%s) / 2' % (xc, w),
                 '-((%s) + (%s) / 2)' % (zc, h), '-((%s) - (%s) / 2)' % (zc, h))


def build_power_guide(top_comp, des):
    """Top shell: guide tube behind the rear wall, bore through the wall, recessed channel in the rear skin."""
    add_pwr_params(des)
    body = top_comp.bRepBodies.item(0)
    tw = 'pwr_stem + 2 * pwr_bore_clr + 2 * pwr_tube_wall'
    sk = top_comp.sketches.add(top_comp.xZConstructionPlane); sk.name = 'Power guide tube'
    zrect(sk, des, 'pwr_x', 'pwr_z', tw, tw)
    ext(top_comp, main_profile(sk), 'pwr_tube_end_y - (pwr_floor_y + 0.3 mm)', FE.JoinFeatureOperation, 'Power guide tube',
        start='pwr_floor_y + 0.3 mm', target=body)
    sk = top_comp.sketches.add(top_comp.xZConstructionPlane); sk.name = 'Power bore'
    zrect(sk, des, 'pwr_x', 'pwr_z', 'pwr_stem + 2 * pwr_bore_clr', 'pwr_stem + 2 * pwr_bore_clr')
    ext(top_comp, main_profile(sk), 'pwr_tube_end_y - (y_rear_out - 1 mm)', FE.CutFeatureOperation, 'Power bore',
        start='y_rear_out - 1 mm', target=body)
    sk = top_comp.sketches.add(top_comp.xZConstructionPlane); sk.name = 'Power channel'
    zrect(sk, des, 'pwr_x', 'pwr_z', 'pwr_chan_w', 'pwr_chan_h')
    ext(top_comp, main_profile(sk), 'pwr_floor_y - (y_rear_out - 1 mm)', FE.CutFeatureOperation, 'Power channel',
        start='y_rear_out - 1 mm', target=body)


def build_power_button(root, des):
    """Separate printed part: flat flanged base + square stem whose end is the cap."""
    add_pwr_params(des)
    comp = new_component(root, 'PowerButton')
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Button stem'
    zrect(sk, des, 'pwr_x', 'pwr_z', 'pwr_stem', 'pwr_stem')
    ext(comp, main_profile(sk), 'pwr_flange_rear_y - pwr_cap_y', FE.NewBodyFeatureOperation, 'Button stem', start='pwr_cap_y')
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Button flange'
    zrect(sk, des, 'pwr_x', 'pwr_z', 'pwr_flange_w', 'pwr_flange_h')
    ext(comp, main_profile(sk), 'pwr_flange_t', FE.JoinFeatureOperation, 'Button flange', start='pwr_flange_rear_y')
    comp.bRepBodies.item(0).name = 'PowerButton'
    return comp


PIPE_PARAMS = [
    ('led_x0', '-7.98 mm', 'WS2812B body min x (PCB STEP)'),
    ('led_x1', '-4.52 mm', 'WS2812B body max x'),
    ('led_y0', '-40.84 mm', 'WS2812B body min y'),
    ('led_y1', '-37.16 mm', 'WS2812B body max y'),
    ('led_top_z', '5.7 mm', 'WS2812B top z'),
    ('led_gap', '0.2 mm', 'Clearance between LED top and the light pipe seat'),
    ('led_margin', '0.3 mm', 'LED seat pocket margin per side'),
    ('pipe_x', '-6.6 mm', 'Light pipe axis x'),
    ('pipe_start_y', '-39 mm', 'Light pipe start (filament end) y, above the LED'),
    ('pipe_end_y', '-48.3 mm', 'Where the curved part ends and the pipe turns horizontal'),
    ('pipe_end_z', '9 mm', 'Height of the horizontal exit run'),
    ('pipe_r', '18 mm', 'Light pipe bend radius'),
    ('pipe_bore', '2 mm', 'Light pipe bore for 1.75 mm filament'),
    ('pipe_wall', '0.6 mm', 'Light pipe tube wall'),
]


def build_light_pipe(top_comp, des):
    """Curved light-pipe tube from above the status LED to the rear skin (gentle arc, then a horizontal exit run)."""
    up = des.userParameters
    for n, e, c in PIPE_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    body = top_comp.bRepBodies.item(0)
    import math
    R = ev_cm(des, 'pipe_r'); ez = ev_cm(des, 'pipe_end_z'); ey = ev_cm(des, 'pipe_end_y')
    sy = ev_cm(des, 'pipe_start_y'); xy = ev_cm(des, 'y_rear_out - 1 mm'); px = ev_cm(des, 'pipe_x')
    # plane at x = pipe_x ; sketch x = -z, sketch y = y
    pin = top_comp.constructionPlanes.createInput()
    pin.setByOffset(top_comp.yZConstructionPlane, VI('pipe_x'))
    plane = top_comp.constructionPlanes.add(pin); plane.name = 'Light pipe plane'
    sk = top_comp.sketches.add(plane); sk.name = 'Light pipe path'
    zc = ez - R; dy = sy - ey
    sz = zc + math.sqrt(R * R - dy * dy)
    ang = math.atan2(dy, sz - zc)
    mid = ang / 2; mz = zc + R * math.cos(mid); my = ey + R * math.sin(mid)
    As = sk.sketchCurves.sketchArcs; Ls = sk.sketchCurves.sketchLines
    ln = Ls.addByTwoPoints(P3(-ez, ey, 0), P3(-ez, xy, 0))
    arc = As.addByThreePoints(P3(-sz, sy, 0), P3(-mz, my, 0), ln.startSketchPoint)
    gc = sk.geometricConstraints; dm = sk.sketchDimensions; o = sk.originPoint
    H = adsk.fusion.DimensionOrientations
    gc.addVertical(ln)
    gc.addTangent(arc, ln)
    d = dm.addDistanceDimension(o, ln.startSketchPoint, H.HorizontalDimensionOrientation, P3(-ez / 2, 1, 0)); d.parameter.expression = 'pipe_end_z'
    d = dm.addDistanceDimension(o, ln.startSketchPoint, H.VerticalDimensionOrientation, P3(-ez - 1, ey / 2, 0)); d.parameter.expression = '-(pipe_end_y)'
    d = dm.addDistanceDimension(o, ln.endSketchPoint, H.VerticalDimensionOrientation, P3(-ez + 1, xy / 2, 0)); d.parameter.expression = '-(y_rear_out - 1 mm)'
    d = dm.addDistanceDimension(o, arc.startSketchPoint, H.VerticalDimensionOrientation, P3(-sz - 1, sy / 2, 0)); d.parameter.expression = '-(pipe_start_y)'
    d = dm.addRadialDimension(arc, P3(-mz - 0.6, my, 0)); d.parameter.expression = 'pipe_r'
    path = top_comp.features.createPath(arc, True)
    # profile plane at the path start
    p2 = top_comp.constructionPlanes.createInput()
    p2.setByDistanceOnPath(path, VI('0'))
    pp = top_comp.constructionPlanes.add(p2); pp.name = 'Light pipe profile plane'
    def circ(name, dia):
        s = top_comp.sketches.add(pp); s.name = name
        c = s.sketchCurves.sketchCircles.addByCenterRadius(P3(0, 0, 0), ev_cm(des, dia) / 2)
        s.geometricConstraints.addCoincident(c.centerSketchPoint, s.originPoint)
        s.sketchDimensions.addDiameterDimension(c, P3(1.5, 1.5, 0)).parameter.expression = dia
        return s
    s1 = circ('Light pipe tube profile', 'pipe_bore + 2 * pipe_wall')
    si = top_comp.features.sweepFeatures.createInput(s1.profiles.item(0), path, FE.JoinFeatureOperation)
    f1 = top_comp.features.sweepFeatures.add(si); f1.name = 'Light pipe tube'
    # LED seat pocket (trims the tube bottom flat above the LED)
    st = top_comp.sketches.add(top_comp.xYConstructionPlane); st.name = 'LED seat'
    rect4(st, des, 'led_x0 - led_margin', 'led_x1 + led_margin', 'led_y0 - led_margin', 'led_y1 + led_margin')
    ext(top_comp, main_profile(st), '3 mm', FE.CutFeatureOperation, 'LED seat', neg=True, start='led_top_z + led_gap', target=body)
    s2 = circ('Light pipe bore profile', 'pipe_bore')
    si2 = top_comp.features.sweepFeatures.createInput(s2.profiles.item(0), path, FE.CutFeatureOperation)
    si2.participantBodies = [body]
    f2 = top_comp.features.sweepFeatures.add(si2); f2.name = 'Light pipe bore'


def shape_power_cap(root, des):
    """Make the power button cap follow the rear plan corner arc, standing pwr_proud above the shell skin.
    The stem is extended rearward (pwr_cap_out) and trimmed by a cylinder centred on the rear corner arc centre."""
    up = des.userParameters
    if up.itemByName('pwr_proud') is None:
        up.add('pwr_proud', VI('0.5 mm'), 'mm', 'Cap stands this far proud of the rear skin')
    up.itemByName('pwr_cap_out').expression = '2.2 mm'
    occ = [o for o in root.occurrences if o.component.name == 'PowerButton'][0]
    comp = occ.component
    occ.activate()
    body = comp.bRepBodies.item(0)
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Cap curve'
    rect4(sk, des, 'pwr_x - pwr_stem / 2 - 1 mm', 'pwr_x + pwr_stem / 2 + 1 mm', 'y_rear_out - 3 mm', 'y_rear_out + r_rear_out')
    circle_at(sk, des, '-(shell_w / 2 - r_rear_out)', 'y_rear_out + r_rear_out', '2 * (r_rear_out + pwr_proud)')
    prof = min(sk.profiles, key=lambda p: p.boundingBox.minPoint.y)
    ext(comp, prof, '12 mm', FE.CutFeatureOperation, 'Cap curve cut', target=body)
    des.activateRootComponent()


BUMP_PARAMS = [
    ('stick_x', '0 mm', 'Thumbstick axis x'),
    ('stick_y', '9.5 mm', 'Thumbstick axis y (same as v1.1)'),
    ('bump_rim_z', '21.5 mm', 'Stick opening rim top z (v1.1)'),
    ('bump_base_r', '16 mm', 'Bump base radius at the pad (v1.1)'),
    ('bump_rim_r', '10.4 mm', 'Flat rim outer radius (v1.1)'),
    ('bump_hole_r', '9.6 mm', 'Stick opening radius (19.2 mm hole, v1.1)'),
    ('bump_wall', '1.2 mm', 'Bump skin thickness (v1.1)'),
    ('bump_base_z', '12.5 mm', 'Bottom of the bump solid; it grows down onto the curved roof shoulder'),
    ('bump_clip_z', '12 mm', 'Bottom of the cavity-restore tool around the bump'),
    ('bump_clip_r', '16.6 mm', 'Radius of the cavity-restore tool around the bump'),
    ('bump_hole_fillet', '0.6 mm', 'Fillet on the top edge of the stick opening'),
    ('bump_hollow_z', '11 mm', 'Bottom of the bump hollowing tool (inside the cavity)'),
]


def _pt_dim(sk, p, h_expr=None, v_expr=None):
    H = adsk.fusion.DimensionOrientations
    o = sk.originPoint; g = p.geometry
    if h_expr is not None:
        sk.sketchDimensions.addDistanceDimension(o, p, H.HorizontalDimensionOrientation, P3(g.x / 2, g.y + 1.0, 0)).parameter.expression = h_expr
    if v_expr is not None:
        sk.sketchDimensions.addDistanceDimension(o, p, H.VerticalDimensionOrientation, P3(g.x + 1.0, g.y / 2, 0)).parameter.expression = v_expr


def _bump_plane(comp):
    pin = comp.constructionPlanes.createInput()
    pin.setByOffset(comp.xZConstructionPlane, VI('stick_y'))
    pl = comp.constructionPlanes.add(pin); pl.name = 'Stick axis plane'
    return pl


def build_stick_bump_outer(top_comp, des):
    """Revolved solid bump (S-curve from the pad at r=bump_base_r up to a flat rim), joined to the roof.
    Sketch plane x=r, y=-z through the stick axis. The S-curve is a fitted spline with horizontal end tangents."""
    up = des.userParameters
    for n, e, c in BUMP_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    body = top_comp.bRepBodies.item(0)
    for s_ in [x for x in top_comp.sketches if x.name == 'Stick bump profile']: s_.deleteMe()
    for p_ in [x for x in top_comp.constructionPlanes if x.name == 'Stick axis plane']: p_.deleteMe()
    pl = _bump_plane(top_comp)
    sk = top_comp.sketches.add(pl); sk.name = 'Stick bump profile'
    PAD, RIM, RB, RR, Z0 = [ev_cm(des, e) for e in ('top_z', 'bump_rim_z', 'bump_base_r', 'bump_rim_r', 'bump_base_z')]
    axis = sk.project(top_comp.zConstructionAxis).item(0)
    Ls = sk.sketchCurves.sketchLines; gc = sk.geometricConstraints
    axl = Ls.addByTwoPoints(P3(0, -Z0, 0), P3(0, -RIM, 0))
    top = Ls.addByTwoPoints(axl.endSketchPoint, P3(RR, -RIM, 0))
    side = Ls.addByTwoPoints(P3(RB, -PAD, 0), P3(RB, -Z0, 0))
    bot = Ls.addByTwoPoints(side.endSketchPoint, axl.startSketchPoint)
    tail = Ls.addByTwoPoints(side.startSketchPoint, P3(RB + 0.5, -PAD, 0)); tail.isConstruction = True
    pts = adsk.core.ObjectCollection.create()
    pts.add(top.endSketchPoint); pts.add(side.startSketchPoint)  # 2 fit points + horizontal end tangents = fully constrained S-curve
    spl = sk.sketchCurves.sketchFittedSplines.add(pts)
    gc.addCoincident(axl.startSketchPoint, axis); gc.addCoincident(axl.endSketchPoint, axis)
    gc.addVertical(side); gc.addHorizontal(top); gc.addHorizontal(bot); gc.addHorizontal(tail)
    gc.addTangent(spl, top); gc.addTangent(spl, tail)
    _pt_dim(sk, axl.startSketchPoint, None, 'bump_base_z')
    _pt_dim(sk, axl.endSketchPoint, None, 'bump_rim_z')
    _pt_dim(sk, top.endSketchPoint, 'bump_rim_r', None)
    _pt_dim(sk, side.startSketchPoint, 'bump_base_r', 'top_z')
    sk.sketchDimensions.addDistanceDimension(tail.startSketchPoint, tail.endSketchPoint, adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation, P3(RB + 0.25, -PAD + 0.5, 0)).parameter.expression = '0.5 mm'
    prof = main_profile(sk)
    ri = top_comp.features.revolveFeatures.createInput(prof, axis, FE.JoinFeatureOperation)
    ri.setAngleExtent(False, VI('360 deg'))
    ri.participantBodies = [body]
    f = top_comp.features.revolveFeatures.add(ri); f.name = 'Stick bump'
    return sk, f


def circle_on_stick(sk, des, dia_expr):
    """Circle centred on the stick axis (stick_x = 0 on the sketch's vertical axis), diameter dia_expr."""
    cy = ev_cm(des, 'stick_y')
    c = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(0, cy, 0), ev_cm(des, dia_expr) / 2)
    sk.geometricConstraints.addVerticalPoints(sk.originPoint, c.centerSketchPoint)
    sk.sketchDimensions.addDistanceDimension(sk.originPoint, c.centerSketchPoint, adsk.fusion.DimensionOrientations.VerticalDimensionOrientation, P3(-1, cy / 2, 0)).parameter.expression = 'stick_y'
    sk.sketchDimensions.addDiameterDimension(c, P3(cy + 2, cy + 2, 0)).parameter.expression = dia_expr
    return c


def restore_cavity_under_bump(comp, des):
    """The bump solid also filled the cavity under it. Re-cut exactly the original cavity shape, limited to a cylinder
    around the stick, so only the unwanted fill is removed (posts and other cavity features are untouched)."""
    body = comp.bRepBodies.item(0)
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Bump cavity restore plan'
    plan(sk, 'wall')
    f = ext(comp, main_profile(sk), 'top_z - roof_t', FE.NewBodyFeatureOperation, 'Bump cavity restore tool')
    tool = f.bodies.item(0); tool.name = 'Bump cavity restore tool'
    fillet_face_edges(comp, planar_face_at(tool, ev_cm(des, 'top_z - roof_t'), 1), 'edge_r - wall', 'Bump cavity restore fillet')
    sk2 = comp.sketches.add(comp.xYConstructionPlane); sk2.name = 'Bump cavity restore clip'
    circle_on_stick(sk2, des, '2 * bump_clip_r')
    f2 = ext(comp, main_profile(sk2), '4 mm', FE.NewBodyFeatureOperation, 'Bump clip cylinder', start='bump_clip_z')
    clip = f2.bodies.item(0); clip.name = 'Bump clip cylinder'
    col = adsk.core.ObjectCollection.create(); col.add(clip)
    ci = comp.features.combineFeatures.createInput(tool, col)
    ci.operation = FE.IntersectFeatureOperation; ci.isKeepToolBodies = False
    cf = comp.features.combineFeatures.add(ci); cf.name = 'Bump cavity restore tool clip'
    col = adsk.core.ObjectCollection.create(); col.add(tool)
    ci = comp.features.combineFeatures.createInput(body, col)
    ci.operation = FE.CutFeatureOperation; ci.isKeepToolBodies = False
    cf = comp.features.combineFeatures.add(ci); cf.name = 'Bump cavity restore'


def build_stick_bump_hollow(comp, des):
    """Hollow the bump to a constant skin (bump_wall): revolve-cut the region under the inward offset of the S-curve,
    then cut the stick opening and round its top edge."""
    up = des.userParameters
    for n, e, c in BUMP_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    body = comp.bRepBodies.item(0)
    psk = [s for s in comp.sketches if s.name == 'Stick bump profile'][0]
    pl = [p for p in comp.constructionPlanes if p.name == 'Stick axis plane'][0]
    for s_ in [x for x in comp.sketches if x.name == 'Stick bump cavity profile']: s_.deleteMe()
    sk = comp.sketches.add(pl); sk.name = 'Stick bump cavity profile'
    axis = sk.project(comp.zConstructionAxis).item(0)
    spl = psk.sketchCurves.sketchFittedSplines.item(0)
    proj = sk.project(spl)
    for c in proj: c.isConstruction = True
    curves = [c for c in proj]
    gc = sk.geometricConstraints
    oi = gc.createOffsetInput(curves, VI('-bump_wall'))
    off = gc.addOffset2(oi)
    oc = [c for c in sk.sketchCurves if c.objectType.endswith('SketchControlPointSpline') and not c.isConstruction][0]
    a, b = oc.startSketchPoint, oc.endSketchPoint
    base, tip = (a, b) if a.geometry.x > b.geometry.x else (b, a)      # base at r=bump_base_r, tip at the rim
    Ls = sk.sketchCurves.sketchLines
    zh = ev_cm(des, 'bump_hollow_z')
    l1 = Ls.addByTwoPoints(P3(0, -zh, 0), P3(base.geometry.x, -zh, 0))
    l2 = Ls.addByTwoPoints(l1.endSketchPoint, base)
    l3 = Ls.addByTwoPoints(tip, P3(0, tip.geometry.y, 0))
    l4 = Ls.addByTwoPoints(l1.startSketchPoint, l3.endSketchPoint)
    gc.addCoincident(l1.startSketchPoint, axis); gc.addCoincident(l3.endSketchPoint, axis)
    gc.addHorizontal(l1); gc.addHorizontal(l3); gc.addVertical(l2)
    _pt_dim(sk, l1.startSketchPoint, None, 'bump_hollow_z')
    ri = comp.features.revolveFeatures.createInput(main_profile(sk), axis, FE.NewBodyFeatureOperation)
    ri.setAngleExtent(False, VI('360 deg'))
    f = comp.features.revolveFeatures.add(ri); f.name = 'Stick bump hollow tool'
    tool = f.bodies.item(0); tool.name = 'Stick bump hollow tool'
    s1 = comp.sketches.add(comp.xYConstructionPlane); s1.name = 'Stick bump hollow clip'
    circle_on_stick(s1, des, '2 * (bump_base_r - bump_wall)')
    f1 = ext(comp, main_profile(s1), '12 mm', FE.NewBodyFeatureOperation, 'Stick bump hollow clip cylinder', start='bump_hollow_z')
    clip = f1.bodies.item(0); clip.name = 'Stick bump hollow clip'
    col = adsk.core.ObjectCollection.create(); col.add(clip)
    ci = comp.features.combineFeatures.createInput(tool, col)
    ci.operation = FE.IntersectFeatureOperation; ci.isKeepToolBodies = False
    comp.features.combineFeatures.add(ci).name = 'Stick bump hollow tool clip'
    col = adsk.core.ObjectCollection.create(); col.add(tool)
    ci = comp.features.combineFeatures.createInput(body, col)
    ci.operation = FE.CutFeatureOperation; ci.isKeepToolBodies = False
    comp.features.combineFeatures.add(ci).name = 'Stick bump cavity'
    # stick opening
    s2 = comp.sketches.add(comp.xYConstructionPlane); s2.name = 'Stick opening'
    circle_on_stick(s2, des, '2 * bump_hole_r')
    ext(comp, main_profile(s2), '8 mm', FE.CutFeatureOperation, 'Stick opening', start='top_z - roof_t', target=body)
    # round the opening's top edge
    zr = ev_cm(des, 'bump_rim_z'); rr = ev_cm(des, 'bump_hole_r')
    edges = adsk.core.ObjectCollection.create()
    for e in body.edges:
        g = e.geometry
        if isinstance(g, adsk.core.Circle3D) and abs(g.radius - rr) < 1e-4 and abs(g.center.z - zr) < 1e-4:
            edges.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(edges, VI('bump_hole_fillet'), True)
    ff = comp.features.filletFeatures.add(fi); ff.name = 'Stick opening fillet'
    return sk, oc


# ------------------------------------------------------------------ face (macro) buttons
FB_PARAMS = [
    ('mac_px', '9.1 mm', 'Macro button pitch in x'),
    ('mac_py', '7.21 mm', 'Macro button pitch in y (PCB measured 7.21)'),
    ('mac_cy1', '-7.32 mm', 'Front macro row centre y (PCB measured)'),
    ('fb_cap_d', '5 mm', 'Face button cap / stem diameter (D stem is this circle with one flat)'),
    ('fb_flat', '2.1 mm', 'D flat distance from the stem axis (flat toward the rear)'),
    ('fb_clr', '0.2 mm', 'Stem clearance per side in the hole / bore'),
    ('fb_flange_d', '6 mm', 'Button flange diameter'),
    ('fb_flange_t0', '1 mm', 'Original flange thickness (sets the unshortened tube bottom)'),
    ('fb_flange_t', 'fb_tube_bot - fb_free - fb_switch_top', 'Button flange thickness: grows up to fb_free below the shortened tube'),
    ('fb_switch_top', '6.29 mm', 'PTS810 plunger top z (flange rests on it)'),
    ('fb_free', '0.6 mm', 'Free travel between flange top and guide tube bottom'),
    ('fb_proud', '2 mm', 'Cap top stands this far above the pad'),
    ('fb_tube_od', '7 mm', 'Guide tube outer diameter'),
    ('fb_text_h', '2.8 mm', 'Number height'),
    ('fb_text_depth', '0.4 mm', 'Number deboss depth'),
    ('fb_cap_fillet', '0.4 mm', 'Cap top edge fillet'),
    ('fb_flange_top', 'fb_switch_top + fb_flange_t', 'Flange top z'),
    ('fb_tube_full_bot', 'fb_switch_top + fb_flange_t0 + fb_free', 'Unshortened guide tube bottom z'),
    ('fb_tube_frac', '0.33', 'Fraction of the original tube length kept'),
    ('fb_tube_bot', '(top_z - roof_t) - fb_tube_frac * ((top_z - roof_t) - fb_tube_full_bot)', 'Guide tube bottom z (shortened)'),
    ('fb_cap_top', 'top_z + fb_proud', 'Cap top z'),
]


def add_fb_params(des):
    up = des.userParameters
    for n, e, c in FB_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def _fb_circle(sk, cx, cy, dia_cm):
    c = sk.sketchCurves.sketchCircles.addByCenterRadius(P3(cx, cy, 0), dia_cm / 2)
    return c, c.centerSketchPoint


def _fb_d(sk, des, cx, cy, dia_expr, flat_expr):
    """D shape (circle with a flat toward -y), fully constrained except its centre."""
    r = ev_cm(des, dia_expr) / 2; f = ev_cm(des, flat_expr); h = (r * r - f * f) ** 0.5
    ln = sk.sketchCurves.sketchLines.addByTwoPoints(P3(cx - h, cy - f, 0), P3(cx + h, cy - f, 0))
    arc = sk.sketchCurves.sketchArcs.addByThreePoints(ln.endSketchPoint, P3(cx, cy + r, 0), ln.startSketchPoint)
    gc = sk.geometricConstraints; dm = sk.sketchDimensions
    gc.addHorizontal(ln)
    dm.addRadialDimension(arc, P3(cx + r + 0.3, cy + r * 0.3, 0)).parameter.expression = '(%s) / 2' % dia_expr
    dm.addDistanceDimension(arc.centerSketchPoint, ln.startSketchPoint, adsk.fusion.DimensionOrientations.VerticalDimensionOrientation,
                            P3(cx - r - 0.5, cy - f / 2, 0)).parameter.expression = flat_expr
    return arc, arc.centerSketchPoint


def _fb_grid(sk, des, shape_fn):
    """Six shapes on the macro grid; first centre dimensioned to the origin, the rest relative (avoids equal origin dims)."""
    H = adsk.fusion.DimensionOrientations
    px, py, cy1 = ev_cm(des, 'mac_px'), ev_cm(des, 'mac_py'), ev_cm(des, 'mac_cy1')
    pts = {}
    for r in range(2):
        for c in range(3):
            pts[(r, c)] = shape_fn((c - 1) * px, cy1 - r * py)[1]
    gc = sk.geometricConstraints; dm = sk.sketchDimensions; o = sk.originPoint
    gc.addVerticalPoints(o, pts[(0, 1)])
    dm.addDistanceDimension(o, pts[(0, 1)], H.VerticalDimensionOrientation, P3(-px / 2, cy1 / 2, 0)).parameter.expression = '-(mac_cy1)'
    for c in (0, 2):
        gc.addHorizontalPoints(pts[(0, 1)], pts[(0, c)])
        dm.addDistanceDimension(pts[(0, 1)], pts[(0, c)], H.HorizontalDimensionOrientation, P3((c - 1) * px / 2, cy1 + 1.0, 0)).parameter.expression = 'mac_px'
    for c in range(3):
        gc.addVerticalPoints(pts[(0, c)], pts[(1, c)])
        dm.addDistanceDimension(pts[(0, c)], pts[(1, c)], H.VerticalDimensionOrientation, P3((c - 1) * px + 1.5, cy1 - py / 2, 0)).parameter.expression = 'mac_py'


def build_face_guides(top_comp, des):
    """Guide tubes (round OD), D bores below the roof, round holes through the roof and bump skin."""
    add_fb_params(des)
    body = top_comp.bRepBodies.item(0)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'Face tubes'
    _fb_grid(sk, des, lambda cx, cy: _fb_circle(sk, cx, cy, ev_cm(des, 'fb_tube_od')))
    for c in sk.sketchCurves:
        if c.objectType.endswith('SketchCircle'):
            sk.sketchDimensions.addDiameterDimension(c, P3(c.centerSketchPoint.geometry.x + 2, c.centerSketchPoint.geometry.y + 2, 0)).parameter.expression = 'fb_tube_od'
    ext(top_comp, all_profiles(sk), 'top_z - 0.2 mm - fb_tube_bot', FE.JoinFeatureOperation, 'Face button guide tubes', start='fb_tube_bot', target=body)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'Face bores'
    _fb_grid(sk, des, lambda cx, cy: _fb_d(sk, des, cx, cy, 'fb_cap_d + 2 * fb_clr', 'fb_flat + fb_clr'))
    ext(top_comp, all_profiles(sk), 'top_z - roof_t - (fb_tube_bot - 0.5 mm)', FE.CutFeatureOperation, 'Face button D bores', start='fb_tube_bot - 0.5 mm', target=body)
    sk = top_comp.sketches.add(top_comp.xYConstructionPlane); sk.name = 'Face holes'
    _fb_grid(sk, des, lambda cx, cy: _fb_circle(sk, cx, cy, ev_cm(des, 'fb_cap_d + 2 * fb_clr')))
    for c in sk.sketchCurves:
        if c.objectType.endswith('SketchCircle'):
            sk.sketchDimensions.addDiameterDimension(c, P3(c.centerSketchPoint.geometry.x + 2, c.centerSketchPoint.geometry.y + 2, 0)).parameter.expression = 'fb_cap_d + 2 * fb_clr'
    ext(top_comp, all_profiles(sk), '8 mm', FE.CutFeatureOperation, 'Face button round holes', start='top_z - roof_t', target=body)


def build_face_button(root, des, row, col, label):
    """One printed button: D stem whose end is the round cap (with the number debossed), flat flange resting on the switch."""
    add_fb_params(des)
    comp = new_component(root, 'FaceButton%s' % label)
    px, py, cy1 = ev_cm(des, 'mac_px'), ev_cm(des, 'mac_py'), ev_cm(des, 'mac_cy1')
    cx, cy = (col - 1) * px, cy1 - row * py
    H = adsk.fusion.DimensionOrientations
    yexpr = '-(mac_cy1) + %d * mac_py' % row

    def place(sk, pt):
        if col == 1:
            sk.geometricConstraints.addVerticalPoints(sk.originPoint, pt)
        else:
            sk.sketchDimensions.addDistanceDimension(sk.originPoint, pt, H.HorizontalDimensionOrientation, P3(cx / 2, 1.0, 0)).parameter.expression = 'mac_px'
        sk.sketchDimensions.addDistanceDimension(sk.originPoint, pt, H.VerticalDimensionOrientation, P3(cx + 1.0, cy / 2, 0)).parameter.expression = yexpr

    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Button stem'
    arc, pt = _fb_d(sk, des, cx, cy, 'fb_cap_d', 'fb_flat'); place(sk, pt)
    f = ext(comp, main_profile(sk), 'fb_cap_top - (fb_flange_top - 0.2 mm)', FE.NewBodyFeatureOperation, 'Button stem', start='fb_flange_top - 0.2 mm')
    body = f.bodies.item(0); body.name = 'FaceButton%s' % label
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Button flange'
    c, pt = _fb_circle(sk, cx, cy, ev_cm(des, 'fb_flange_d')); place(sk, pt)
    sk.sketchDimensions.addDiameterDimension(c, P3(cx + 2, cy + 2, 0)).parameter.expression = 'fb_flange_d'
    ext(comp, main_profile(sk), 'fb_flange_t', FE.JoinFeatureOperation, 'Button flange', start='fb_switch_top', target=body)
    fillet_face_edges(comp, planar_face_at(body, ev_cm(des, 'fb_cap_top'), 1), 'fb_cap_fillet', 'Cap edge fillet')
    # number deboss on the cap face
    pin = comp.constructionPlanes.createInput(); pin.setByOffset(comp.xYConstructionPlane, VI('fb_cap_top'))
    pl = comp.constructionPlanes.add(pin); pl.name = 'Cap face plane'
    ts = comp.sketches.add(pl); ts.name = 'Button number'
    r = ev_cm(des, 'fb_cap_d') / 2
    ti = ts.sketchTexts.createInput2(str(label), ev_cm(des, 'fb_text_h'))
    ti.setAsMultiLine(P3(cx - r, cy + r, 0), P3(cx + r, cy - r, 0),
                      adsk.core.HorizontalAlignments.CenterHorizontalAlignment, adsk.core.VerticalAlignments.MiddleVerticalAlignment, 0)
    txt = ts.sketchTexts.add(ti)
    ext(comp, txt, 'fb_text_depth', FE.CutFeatureOperation, 'Number deboss', neg=True, target=body)
    return comp


SB_PARAMS = [
    ('sb_y1', '14.5 mm', 'Front side switch actuator centre y (PCB STEP)'),
    ('sb_y2', '4.5 mm', 'Rear side switch actuator centre y (PCB STEP)'),
    ('sb_z', '5.55 mm', 'Side switch actuator centre height'),
    ('sb_tip_x', 'pcb_hw', 'Side switch actuator tip |x|'),
    ('sb_act_gap', '0.1 mm', 'Gap between the flange inner face and the actuator tip at rest'),
    ('sb_stem_w', '7.3 mm', 'D stem / cap width (y), v1.1 size'),
    ('sb_stem_h', '4.6 mm', 'D stem / cap height (z), v1.1 size'),
    ('sb_stem_r', '1.8 mm', 'D stem outer-corner radius'),
    ('sb_hole_clr', '0.2 mm', 'D hole clearance per side'),
    ('sb_fl_w', '8.6 mm', 'Inner flange width (y); must exceed the D hole so it retains'),
    ('sb_fl_h', '5.8 mm', 'Inner flange height (z); must exceed the D hole so it retains'),
    ('sb_fl_t', '0.9 mm', 'Inner flange thickness (x)'),
    ('sb_d_r', '1.8 mm', 'Flange outer-corner radius; the flat side faces the other button'),
    ('sb_pocket_clr', '0.2 mm', 'Flange pocket clearance per side'),
    ('sb_pocket_axial', '0.15 mm', 'Free travel between flange outer face and pocket floor'),
    ('sb_proud', '1 mm', 'Cap face stands this far proud of the side skin'),
    ('sb_cap_fillet', '0.3 mm', 'Cap outer edge fillet'),
    ('sb_wall_in_x', 'shell_w / 2 - wall', 'Side wall inner face |x|'),
    ('sb_wall_out_x', 'shell_w / 2', 'Side wall outer face |x|'),
    ('sb_fl_in_x', 'sb_tip_x + sb_act_gap', 'Flange inner face |x|'),
    ('sb_fl_out_x', 'sb_fl_in_x + sb_fl_t', 'Flange outer face |x|'),
    ('sb_pocket_floor_x', 'sb_fl_out_x + sb_pocket_axial', 'Pocket floor |x|'),
    ('sb_cap_out_x', 'sb_wall_out_x + sb_proud', 'Cap outer face |x|'),
]


def add_sb_params(des):
    up = des.userParameters
    for n, e, c in SB_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def _sb_d(sk, des, yc, w, h, outer_top, r):
    """D profile on a YZ-plane sketch (sketch x = -z, y = y): centred at (yc, sb_z), size w (y) x h (z),
    the two corners on the outer-y side rounded by r, flat side toward the other button."""
    rect = rect4(sk, des, '-(sb_z + (%s) / 2)' % h, '-(sb_z - (%s) / 2)' % h, '(%s) - (%s) / 2' % (yc, w), '(%s) + (%s) / 2' % (yc, w))
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    hs = [l for l in lines if horiz(l)]; vs = [l for l in lines if not horiz(l)]
    outer = max(hs, key=lambda l: l.startSketchPoint.geometry.y) if outer_top else min(hs, key=lambda l: l.startSketchPoint.geometry.y)
    def mid(l):
        a = l.startSketchPoint.geometry; b = l.endSketchPoint.geometry
        return P3((a.x + b.x) / 2, (a.y + b.y) / 2, 0)
    arcs = []
    for v in vs:
        arcs.append(sk.sketchCurves.sketchArcs.addFillet(v, mid(v), outer, mid(outer), ev_cm(des, r)))
    g = arcs[0].centerSketchPoint.geometry
    sk.sketchDimensions.addRadialDimension(arcs[0], P3(g.x, g.y + (0.3 if outer_top else -0.3), 0)).parameter.expression = r
    sk.geometricConstraints.addEqual(arcs[0], arcs[1])
    return rect


def _sb_cap_face(body, s, x_cm):
    for fc in body.faces:
        if fc.geometry.objectType.endswith('Plane'):
            ok, n = fc.evaluator.getNormalAtPoint(fc.pointOnFace)
            if ok and abs(n.x - s) < 1e-6 and abs(abs(fc.pointOnFace.x) - x_cm) < 1e-4:
                return fc
    raise RuntimeError('cap face not found')


SB_LAYOUT = [('L1', -1, 'sb_y1', True), ('L2', -1, 'sb_y2', False), ('R1', 1, 'sb_y1', True), ('R2', 1, 'sb_y2', False)]


def build_side_cuts(top_comp, des, key):
    """Top shell: flange pocket in the wall's inner face + D hole through the remaining outer lip."""
    add_sb_params(des)
    _, s, yc, ot = [t for t in SB_LAYOUT if t[0] == key][0]
    sx = lambda e: ('-(%s)' % e) if s < 0 else '(%s)' % e
    body = top_comp.bRepBodies.item(0)
    sk = top_comp.sketches.add(top_comp.yZConstructionPlane); sk.name = 'Side pocket %s' % key
    _sb_d(sk, des, yc, 'sb_fl_w + 2 * sb_pocket_clr', 'sb_fl_h + 2 * sb_pocket_clr', ot, 'sb_d_r + sb_pocket_clr')
    ext(top_comp, main_profile(sk), 'sb_pocket_floor_x - (sb_wall_in_x - 0.5 mm)', FE.CutFeatureOperation, 'Side pocket %s' % key,
        neg=(s < 0), start=sx('sb_wall_in_x - 0.5 mm'), target=body)
    sk = top_comp.sketches.add(top_comp.yZConstructionPlane); sk.name = 'Side hole %s' % key
    _sb_d(sk, des, yc, 'sb_stem_w + 2 * sb_hole_clr', 'sb_stem_h + 2 * sb_hole_clr', ot, 'sb_stem_r + sb_hole_clr')
    ext(top_comp, main_profile(sk), 'sb_wall_out_x + 1 mm - (sb_pocket_floor_x - 0.2 mm)', FE.CutFeatureOperation, 'Side hole %s' % key,
        neg=(s < 0), start=sx('sb_pocket_floor_x - 0.2 mm'), target=body)


def build_side_button(root, des, key):
    """Printed part: inner D flange in the wall pocket; D stem (its end is the cap, same size) through the lip. No outer flange, installs from inside."""
    add_sb_params(des)
    _, s, yc, ot = [t for t in SB_LAYOUT if t[0] == key][0]
    sx = lambda e: ('-(%s)' % e) if s < 0 else '(%s)' % e
    comp = new_component(root, 'SideButton%s' % key)
    sk = comp.sketches.add(comp.yZConstructionPlane); sk.name = 'Button flange'
    _sb_d(sk, des, yc, 'sb_fl_w', 'sb_fl_h', ot, 'sb_d_r')
    f = ext(comp, main_profile(sk), 'sb_fl_t', FE.NewBodyFeatureOperation, 'Button flange', neg=(s < 0), start=sx('sb_fl_in_x'))
    body = f.bodies.item(0); body.name = 'SideButton%s' % key
    sk = comp.sketches.add(comp.yZConstructionPlane); sk.name = 'Button stem'
    _sb_d(sk, des, yc, 'sb_stem_w', 'sb_stem_h', ot, 'sb_stem_r')
    ext(comp, main_profile(sk), 'sb_cap_out_x - (sb_fl_out_x - 0.2 mm)', FE.JoinFeatureOperation, 'Button stem',
        neg=(s < 0), start=sx('sb_fl_out_x - 0.2 mm'), target=body)
    fillet_face_edges(comp, _sb_cap_face(body, s, ev_cm(des, 'sb_cap_out_x')), 'sb_cap_fillet', 'Cap edge fillet')
    return comp


# ---------------------------------------------------------------- USB port wall
USB_PORT_PARAMS = [
    ('usb_port_w', '8.94 mm', 'Receptacle shell width (PCB STEP x -2.47..6.47)'),
    ('usb_port_h', '3.19 mm', 'Receptacle shell height (PCB STEP z 3.85..7.04, tabs below excluded)'),
    ('usb_port_clr', '0.15 mm', 'Port wall opening clearance per side around the receptacle'),
    ('usb_rwall_t', '0.7 mm', 'Port wall thickness; stops 0.16 mm short of the PCB edge (y -42.0)'),
    ('usb_port_r', '1 mm', 'Receptacle shell corner radius (measured from the STEP)'),
]


def port_opening(sk, des):
    """Rounded rectangle around the receptacle shell (+ clearance), centred (usb_x, usb_z) on an XZ sketch; corner radius
    usb_port_r + usb_port_clr (concentric with the receptacle's own corners). Fully constrained."""
    w = 'usb_port_w + 2 * usb_port_clr'; h = 'usb_port_h + 2 * usb_port_clr'
    rect = rect4(sk, des, 'usb_x - (%s) / 2' % w, 'usb_x + (%s) / 2' % w, '-(usb_z + (%s) / 2)' % h, '-(usb_z - (%s) / 2)' % h)
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    hs = [l for l in lines if horiz(l)]; vs = [l for l in lines if not horiz(l)]
    def mid(l):
        a = l.startSketchPoint.geometry; b = l.endSketchPoint.geometry
        return P3((a.x + b.x) / 2, (a.y + b.y) / 2, 0)
    r = 'usb_port_r + usb_port_clr'
    mids = [(v, mid(v), hh, mid(hh)) for v in vs for hh in hs]
    arcs = [sk.sketchCurves.sketchArcs.addFillet(v, mv, hh, mh, ev_cm(des, r)) for v, mv, hh, mh in mids]
    g = arcs[0].centerSketchPoint.geometry
    sk.sketchDimensions.addRadialDimension(arcs[0], P3(g.x, g.y + 0.3, 0)).parameter.expression = r
    for x in arcs[1:]:
        sk.geometricConstraints.addEqual(arcs[0], x)
    return sk


def build_usb_port_wall(comp, des, key, target):
    """Closing wall at the end of the plug channel: the bezel tube's outline continued 0.7 mm, with a stadium opening
    that hugs the receptacle (port shell + clearance), rear face flush with the pocket end. key 'Top' or 'Bottom'."""
    up = des.userParameters
    for n, e, c in USB_PORT_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
    top = (key == 'Top')
    w = 'usb_plug_w + 2 * usb_pocket_clr + 2 * usb_bezel_t'
    h = 'usb_plug_h + 2 * usb_pocket_clr + 2 * usb_bezel_t'
    y0 = 'usb_face_y - usb_stop_gap'
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'USB port wall %s outline' % key.lower()
    usb_slot(sk, des, w, h)
    ext(comp, main_profile(sk), 'usb_rwall_t', FE.JoinFeatureOperation, 'USB port wall %s' % key.lower(), start=y0, target=target)
    t = comp.sketches.add(comp.xYConstructionPlane); t.name = 'USB port wall %s trim' % key.lower()
    rect4(t, des, 'usb_x - (usb_plug_w / 2 + usb_pocket_clr + usb_bezel_t) - 1 mm',
          'usb_x + (usb_plug_w / 2 + usb_pocket_clr + usb_bezel_t) + 1 mm',
          '%s - 0.2 mm' % y0, '%s + usb_rwall_t + 0.2 mm' % y0)
    if top:
        ext(comp, main_profile(t), '5 mm', FE.CutFeatureOperation, 'USB port wall top trim', neg=True,
            start='usb_bezel_split_top', target=target)
        sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'USB port opening'
        port_opening(sk, des)
        ext(comp, main_profile(sk), 'usb_rwall_t + 0.4 mm', FE.CutFeatureOperation, 'USB port opening',
            start='%s - 0.2 mm' % y0, target=target)
    else:
        ext(comp, main_profile(t), '10 mm', FE.CutFeatureOperation, 'USB port wall bottom trim',
            start='usb_bezel_split_bot', target=target)


# ---------------------------------------------------------------- front bumper (straddles the seam)
BUMP2_PARAMS = [
    ('bm_x', '0 mm', 'Bumper switch / bumper centre x'),
    ('bm_z', '0 mm', 'Bumper centre z (on the seam; switch actuator is at z 0.4)'),
    ('bm_act_y', '28.25 mm', 'Bumper switch actuator tip y (PCB STEP)'),
    ('bm_gap', '0.1 mm', 'Gap between the flange inner face and the actuator tip at rest'),
    ('bm_fl_w', '20 mm', 'Inner flange width (x); clears the front screw posts'),
    ('bm_fl_h', '7 mm', 'Inner flange height (z)'),
    ('bm_fl_r', '1 mm', 'Inner flange corner radius'),
    ('bm_fl_t', '0.9 mm', 'Inner flange thickness (y)'),
    ('bm_pk_clr', '0.2 mm', 'Flange pocket clearance per side'),
    ('bm_pk_axial', '0.15 mm', 'Free travel between flange outer face and pocket floor'),
    ('bm_stem_w', '18 mm', 'Stem width (x)'),
    ('bm_stem_h', '4.4 mm', 'Stem height (z)'),
    ('bm_stem_r', '1 mm', 'Stem corner radius'),
    ('bm_hole_clr', '0.2 mm', 'Slot clearance per side'),
    ('bm_cap_w', '24 mm', 'Cap width (x)'),
    ('bm_cap_h', '8 mm', 'Cap height (z)'),
    ('bm_cap_r', '2 mm', 'Cap corner radius'),
    ('bm_proud', '1.5 mm', 'Cap face stands this far proud of the front skin'),
    ('bm_travel', '0.5 mm', 'Gap between the front skin and the cap underside = press travel'),
    ('bm_cap_fillet', '0.4 mm', 'Cap face edge fillet'),
    ('bm_wall_in_y', 'y_front_out - wall', 'Front wall inner face y'),
    ('bm_wall_out_y', 'y_front_out', 'Front wall outer face y'),
    ('bm_fl_in_y', 'bm_act_y + bm_gap', 'Flange inner face y'),
    ('bm_fl_out_y', 'bm_fl_in_y + bm_fl_t', 'Flange outer face y'),
    ('bm_pk_floor_y', 'bm_fl_out_y + bm_pk_axial', 'Pocket floor y'),
    ('bm_cap_in_y', 'bm_wall_out_y + bm_travel', 'Cap underside y'),
    ('bm_cap_out_y', 'bm_wall_out_y + bm_proud', 'Cap face y'),
    ('bm_cap_t', 'bm_cap_out_y - bm_cap_in_y', 'Cap thickness'),
]


def add_bm_params(des):
    up = des.userParameters
    for n, e, c in BUMP2_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def rrect(sk, des, w, h, r):
    """Rounded rectangle centred (bm_x, bm_z) on an XZ-plane sketch (sketch y = -z); fully constrained."""
    rect = rect4(sk, des, 'bm_x - (%s) / 2' % w, 'bm_x + (%s) / 2' % w, '-(bm_z + (%s) / 2)' % h, '-(bm_z - (%s) / 2)' % h)
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    hs = [l for l in lines if horiz(l)]; vs = [l for l in lines if not horiz(l)]
    def mid(l):
        a = l.startSketchPoint.geometry; b = l.endSketchPoint.geometry
        return P3((a.x + b.x) / 2, (a.y + b.y) / 2, 0)
    arcs = [sk.sketchCurves.sketchArcs.addFillet(v, mid(v), hh, mid(hh), ev_cm(des, r)) for v in vs for hh in hs]
    g = arcs[0].centerSketchPoint.geometry
    sk.sketchDimensions.addRadialDimension(arcs[0], P3(g.x, g.y + 0.3, 0)).parameter.expression = r
    for x in arcs[1:]:
        sk.geometricConstraints.addEqual(arcs[0], x)
    return sk


def build_bumper_cuts(comp, des, key, target):
    """Front wall of one shell half: flange pocket in the inner face + stem slot through the remaining lip. key 'Top'/'Bottom'."""
    add_bm_params(des)
    k = key.lower()
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Bumper pocket %s' % k
    rrect(sk, des, 'bm_fl_w + 2 * bm_pk_clr', 'bm_fl_h + 2 * bm_pk_clr', 'bm_fl_r + bm_pk_clr')
    ext(comp, main_profile(sk), 'bm_pk_floor_y - (bm_fl_in_y - 0.05 mm)', FE.CutFeatureOperation, 'Bumper pocket %s' % k,
        start='bm_fl_in_y - 0.05 mm', target=target)
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Bumper slot %s' % k
    rrect(sk, des, 'bm_stem_w + 2 * bm_hole_clr', 'bm_stem_h + 2 * bm_hole_clr', 'bm_stem_r + bm_hole_clr')
    ext(comp, main_profile(sk), 'bm_wall_out_y + 0.5 mm - (bm_pk_floor_y - 0.1 mm)', FE.CutFeatureOperation, 'Bumper slot %s' % k,
        start='bm_pk_floor_y - 0.1 mm', target=target)


def _face_y(body, y_cm, ny):
    for fc in body.faces:
        if fc.geometry.objectType.endswith('Plane'):
            ok, n = fc.evaluator.getNormalAtPoint(fc.pointOnFace)
            if ok and abs(n.y - ny) < 1e-6 and abs(fc.pointOnFace.y - y_cm) < 1e-4:
                return fc
    raise RuntimeError('face not found at y=%s' % y_cm)


def build_bumper(root, des):
    """Printed bumper: inner flange, stem through the slot, wide cap outside the front wall, straddling the seam."""
    add_bm_params(des)
    comp = new_component(root, 'Bumper')
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Bumper flange'
    rrect(sk, des, 'bm_fl_w', 'bm_fl_h', 'bm_fl_r')
    f = ext(comp, main_profile(sk), 'bm_fl_t', FE.NewBodyFeatureOperation, 'Bumper flange', start='bm_fl_in_y')
    body = f.bodies.item(0); body.name = 'Bumper'
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Bumper stem'
    rrect(sk, des, 'bm_stem_w', 'bm_stem_h', 'bm_stem_r')
    ext(comp, main_profile(sk), 'bm_cap_in_y + 0.2 mm - (bm_fl_out_y - 0.2 mm)', FE.JoinFeatureOperation, 'Bumper stem',
        start='bm_fl_out_y - 0.2 mm', target=body)
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Bumper cap'
    rrect(sk, des, 'bm_cap_w', 'bm_cap_h', 'bm_cap_r')
    ext(comp, main_profile(sk), 'bm_cap_t', FE.JoinFeatureOperation, 'Bumper cap', start='bm_cap_in_y', target=body)
    fillet_face_edges(comp, _face_y(body, ev_cm(des, 'bm_cap_out_y'), 1), 'bm_cap_fillet', 'Cap edge fillet')
    return comp


# ================================================================ battery bay (v1.1 contacts, flat tabbed access panel)
BAY_PARAMS = [
    ('bay_axis_z', '-8.4 mm', 'cell axis height (carrier underside parts reach z -0.82)'),
    ('bay_cell_d', '14.5 mm', '14500 cell with wrap'),
    ('bay_cell_len', '50.5 mm', '14500 cell incl. nub'),
    ('bay_trench_clr', '0.25 mm', 'radial clearance of the cell trench'),
    ('bay_floor_z', '-(bot_d - floor_t)', 'cavity floor'),
    ('bay_clip_y', 'y_rear_out + wall + 0.1 mm', 'rear contact clip back face'),
    ('bay_clip_t', '1.45 mm', 'rear clip thickness'),
    ('bay_front_y', 'bay_clip_y + 55.9 mm', 'front contact block plate front face (v1.1 spacing)'),
    ('bay_blk_t', '1.45 mm', 'front block plate thickness'),
    ('bay_pin_tip_y', 'bay_front_y - 2.29 mm', 'front contact pin tip'),
    ('bay_cell_y0', 'bay_clip_y + bay_clip_t + 0.8 mm', 'cell rear end'),
    ('bay_hold_clr', '0.1 mm', 'contact holder clearance'),
    ('bay_hold_w', '1.2 mm', 'contact holder cheek thickness'),
    ('bay_hold_top_z', '-1.5 mm', 'contact holder top'),
    ('bay_clip_w', '11.2 mm', 'rear clip width'),
    ('bay_blk_w', '10.8 mm', 'front block plate width'),
    ('bay_pan_w', '21.6 mm', 'panel width'),
    ('bay_pan_clr', '0.15 mm', 'panel to shell gap'),
    ('bay_pan_y0', 'bay_clip_y + bay_clip_t + 0.2 mm + bay_pan_clr', 'panel rear edge: the whole cell must drop through the opening'),
    ('bay_pan_y1', '-1.45 mm', 'panel front edge'),
    ('bay_pan_t', '4.7 mm', 'panel thickness'),
    ('bay_pan_nw', '15.2 mm', 'panel tail width (rear part, clear of the PCB screw columns)'),
    ('bay_pan_step_y', 'mh3_y + post_r + 0.5 mm', 'where the panel widens (just past the rear screw column)'),
    ('bay_rim_w', '1.5 mm', 'rim wall beside the opening'),
    ('bay_rim_top_z', '-12.3 mm', 'rim wall top'),
    ('bay_hook_x0', '5.5 mm', 'hook tab inner x'),
    ('bay_hook_x1', '10.5 mm', 'hook tab outer x'),
    ('bay_hook_len', '2.5 mm', 'hook tab length past the panel front'),
    ('bay_hook_z0', '-15.2 mm', 'hook tab bottom'),
    ('bay_hook_z1', '-13.8 mm', 'hook tab top'),
    ('bay_slot_clr', '0.2 mm', 'hook slot clearance'),
    ('bay_bulk_t', '3.5 mm', 'front hook bulkhead thickness'),
    ('bay_bulk_top_z', '-12.0 mm', 'front hook bulkhead top'),
]


def add_bay_params(des):
    up = des.userParameters
    for n, e, c in BAY_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def _move_occ(root, occ, rows):
    """Sticky placement by a MoveFeature; rows = 3x4 row-major rotation+translation in cm."""
    m = adsk.core.Matrix3D.create()
    m.setWithArray([rows[0][0], rows[0][1], rows[0][2], rows[0][3],
                    rows[1][0], rows[1][1], rows[1][2], rows[1][3],
                    rows[2][0], rows[2][1], rows[2][2], rows[2][3], 0, 0, 0, 1])
    col = adsk.core.ObjectCollection.create()
    for b in occ.bRepBodies:
        col.add(b)
    inp = root.features.moveFeatures.createInput2(col)
    inp.defineAsFreeMove(m)
    return root.features.moveFeatures.add(inp)


def place_contacts(root, des):
    """Move the imported v1.1 contact parts onto the v2 cell axis (v1.1 transforms, new y/z)."""
    add_bay_params(des)
    ax = ev_cm(des, 'bay_axis_z'); cy = ev_cm(des, 'bay_clip_y'); fy = ev_cm(des, 'bay_front_y')
    clip = root.occurrences.itemByName('ContactClip_Revolve:1')
    blk = root.occurrences.itemByName('Contacts_Block:1')
    # clip: x'=x, y'=z+cy, z'=-y+(ax+0.5)
    f1 = _move_occ(root, clip, [[1, 0, 0, 0], [0, 0, 1, cy], [0, -1, 0, ax + 0.5]])
    f1.name = 'Place rear clip'
    # block: rotate 180 about z, plate front face at fy, axis at ax
    f2 = _move_occ(root, blk, [[-1, 0, 0, 0], [0, -1, 0, fy], [0, 0, 1, ax]])
    f2.name = 'Place front contact block'


def build_cell_ref(root, des):
    add_bay_params(des)
    comp = new_component(root, 'Cell14500_Ref')
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Cell'
    circle_at(sk, des, '0 mm', '-bay_axis_z', 'bay_cell_d')
    f = ext(comp, main_profile(sk), 'bay_cell_len', FE.NewBodyFeatureOperation, 'Cell', start='bay_cell_y0')
    f.bodies.item(0).name = 'Cell14500'
    return comp


def _xy_rects(comp, des, name, rects):
    """XY-plane sketch with several fully dimensioned rectangles; rects = [(left, right, bottom, top), ...]."""
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = name
    for r in rects:
        rect4(sk, des, *r)
    return sk


def build_bay_shell(bot_comp, des, body):
    """BottomShell side of the battery bay: contact holders, hook bulkhead, rim walls, cell trench,
    panel opening, hook slots and thumb pocket (z positions from bay_* parameters)."""
    add_bay_params(des)
    c = bot_comp
    hx = 'bay_clip_w / 2 + bay_hold_clr'
    zs = 'bay_floor_z - 0.3 mm'
    # rear clip cheeks
    sk = _xy_rects(c, des, 'Bay rear cheeks', [
        ('-(%s + bay_hold_w)' % hx, '-(%s)' % hx, 'bay_clip_y - bay_hold_clr', 'bay_clip_y + bay_clip_t + 0.2 mm'),
        ('%s' % hx, '%s + bay_hold_w' % hx, 'bay_clip_y - bay_hold_clr', 'bay_clip_y + bay_clip_t + 0.2 mm')])
    ext(c, all_profiles(sk), 'bay_hold_top_z - (%s)' % zs, FE.JoinFeatureOperation, 'Bay rear cheeks', start=zs, target=body)
    # front block holder (backing + cheeks) and plate pocket
    fx = 'bay_blk_w / 2 + bay_hold_clr'
    sk = _xy_rects(c, des, 'Bay front holder', [
        ('-(%s + bay_hold_w)' % fx, '%s + bay_hold_w' % fx, 'bay_front_y - bay_blk_t - bay_hold_clr', 'bay_front_y + bay_hold_clr + 1.5 mm')])
    ext(c, main_profile(sk), 'bay_hold_top_z - (%s)' % zs, FE.JoinFeatureOperation, 'Bay front holder', start=zs, target=body)
    sk = _xy_rects(c, des, 'Bay front plate pocket', [
        ('-(%s)' % fx, '%s' % fx, 'bay_front_y - bay_blk_t - bay_hold_clr', 'bay_front_y + bay_hold_clr')])
    ext(c, main_profile(sk), 'bay_hold_top_z + 1 mm - bay_floor_z', FE.CutFeatureOperation, 'Bay front plate pocket', start='bay_floor_z', target=body)
    # hook bulkhead and rim walls
    sk = _xy_rects(c, des, 'Bay hook bulkhead', [
        ('-(shell_w / 2 - wall + 0.2 mm)', 'shell_w / 2 - wall + 0.2 mm', 'bay_pan_y1 + bay_pan_clr', 'bay_pan_y1 + bay_pan_clr + bay_bulk_t')])
    ext(c, main_profile(sk), 'bay_bulk_top_z - (bay_floor_z - 0.3 mm)', FE.JoinFeatureOperation, 'Bay hook bulkhead', start='bay_floor_z - 0.3 mm', target=body)
    px = 'bay_pan_w / 2 + bay_pan_clr'
    sk = _xy_rects(c, des, 'Bay rim walls', [
        ('-(%s + bay_rim_w)' % px, '-(%s)' % px, 'bay_pan_step_y - bay_pan_clr', 'bay_pan_y1 + bay_pan_clr + 0.5 mm'),
        ('%s' % px, '%s + bay_rim_w' % px, 'bay_pan_step_y - bay_pan_clr', 'bay_pan_y1 + bay_pan_clr + 0.5 mm')])
    ext(c, all_profiles(sk), 'bay_rim_top_z - (bay_floor_z - 0.3 mm)', FE.JoinFeatureOperation, 'Bay rim walls', start='bay_floor_z - 0.3 mm', target=body)
    # cell trench (cylinder along y)
    sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Bay cell trench'
    circle_at(sk, des, '0 mm', '-bay_axis_z', 'bay_cell_d + 2 * bay_trench_clr')
    ext(c, main_profile(sk), 'bay_pin_tip_y - (bay_clip_y + bay_clip_t + 0.2 mm)', FE.CutFeatureOperation, 'Bay cell trench',
        start='bay_clip_y + bay_clip_t + 0.2 mm', target=body)
    # panel opening through the floor
    nx = 'bay_pan_nw / 2 + bay_pan_clr'
    sk = _xy_rects(c, des, 'Bay panel opening', [
        ('-(%s)' % nx, '%s' % nx, 'bay_pan_y0 - bay_pan_clr', 'bay_pan_y1 + bay_pan_clr')])
    ext(c, main_profile(sk), 'bay_pan_t + 1 mm + 0.3 mm', FE.CutFeatureOperation, 'Bay panel opening', start='-bot_d - 1 mm', target=body)
    sk = _xy_rects(c, des, 'Bay wide opening', [
        ('-(%s)' % px, '%s' % px, 'bay_pan_step_y - bay_pan_clr', 'bay_pan_y1 + bay_pan_clr')])
    ext(c, main_profile(sk), 'bay_pan_t + 1 mm + 0.3 mm', FE.CutFeatureOperation, 'Bay wide opening', start='-bot_d - 1 mm', target=body)
    # hook slots in the bulkhead
    sk = _xy_rects(c, des, 'Bay hook slots', [
        ('-(bay_hook_x1 + bay_slot_clr)', '-(bay_hook_x0 - bay_slot_clr)', 'bay_pan_y1 + bay_pan_clr - 0.2 mm', 'bay_pan_y1 + bay_hook_len + bay_slot_clr + 0.3 mm'),
        ('bay_hook_x0 - bay_slot_clr', 'bay_hook_x1 + bay_slot_clr', 'bay_pan_y1 + bay_pan_clr - 0.2 mm', 'bay_pan_y1 + bay_hook_len + bay_slot_clr + 0.3 mm')])
    ext(c, all_profiles(sk), 'bay_hook_z1 + bay_slot_clr - bay_floor_z', FE.CutFeatureOperation, 'Bay hook slots', start='bay_floor_z', target=body)
    # thumb pocket in the belly behind the panel
    sk = _xy_rects(c, des, 'Bay thumb pocket', [
        ('-4.5 mm', '4.5 mm', 'bay_pan_y0 - bay_pan_clr - 3.5 mm', 'bay_pan_y0 - bay_pan_clr')])
    ext(c, main_profile(sk), '0.6 mm', FE.CutFeatureOperation, 'Bay thumb pocket', start='-bot_d', target=body)


BAY_PANEL_PARAMS = [
    ('bay_arm_t', '1.2 mm', 'snap arm strip thickness'),
    ('bay_slit_w', '0.6 mm', 'snap arm slit width'),
    ('bay_arm_len', '14 mm', 'snap arm length from the panel step'),
    ('bay_bump_h', '0.45 mm', 'detent bump height'),
    ('bay_bump_len', '4 mm', 'detent bump length (y)'),
    ('bay_bump_off', '2 mm', 'detent bump distance from the panel step'),
    ('bay_bump_z0', '-15.5 mm', 'detent bump bottom'),
    ('bay_bump_z1', '-13.3 mm', 'detent bump top'),
    ('bay_groove_clr', '0.15 mm', 'detent groove clearance'),
]


def add_bay_panel_params(des):
    add_bay_params(des)
    up = des.userParameters
    for n, e, c in BAY_PANEL_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def poly_pts(sk, des, pts):
    """Closed polygon through signed (x, y) expression pairs; every vertex dimensioned to the origin (nonzero coords only)."""
    L = sk.sketchCurves.sketchLines; dm = sk.sketchDimensions; o = sk.originPoint
    vals = [(ev_cm(des, x), ev_cm(des, y)) for x, y in pts]
    lines = []
    first = L.addByTwoPoints(P3(vals[0][0], vals[0][1], 0), P3(vals[1][0], vals[1][1], 0))
    lines.append(first)
    for i in range(2, len(vals)):
        lines.append(L.addByTwoPoints(lines[-1].endSketchPoint, P3(vals[i][0], vals[i][1], 0)))
    lines.append(L.addByTwoPoints(lines[-1].endSketchPoint, first.startSketchPoint))
    gc = sk.geometricConstraints
    for l in lines:
        a = l.startSketchPoint.geometry; b = l.endSketchPoint.geometry
        if abs(a.y - b.y) < 1e-9: gc.addHorizontal(l)
        elif abs(a.x - b.x) < 1e-9: gc.addVertical(l)
    spts = [first.startSketchPoint] + [l.endSketchPoint for l in lines[:-1]]
    HO = adsk.fusion.DimensionOrientations
    for sp, (vx, vy), (ex, ey) in zip(spts, vals, pts):
        for orient, v, e, tp in ((HO.HorizontalDimensionOrientation, vx, ex, P3(vx / 2, -1, 0)),
                                 (HO.VerticalDimensionOrientation, vy, ey, P3(vx + 1, vy / 2, 0))):
            try:
                d = dm.addDistanceDimension(o, sp, orient, tp)
            except RuntimeError:
                continue   # already fixed by an earlier dimension / constraint
            d.parameter.expression = '(%s)' % e if v > 0 else '-(%s)' % e
    return lines


def _bump_tri(sk, des, sgn, grow, ramp_extra=False):
    """Detent wedge in an XZ sketch (sketch y = -z): flat bottom, ramp on the top. sgn +1/-1 = side; grow = extra clearance."""
    px = '%sbay_pan_w / 2' % ('' if sgn > 0 else '-')
    def X(h): return ('bay_pan_w / 2 + %s' % h) if sgn > 0 else ('-(bay_pan_w / 2 + %s)' % h)
    zb = '-(bay_bump_z0 - %s)' % grow      # sketch y of the bottom edge
    zt = '-(bay_bump_z1 + %s%s)' % (grow, ' + 1 mm' if ramp_extra else '')
    poly_pts(sk, des, [(X('0 mm'), zb), (X('bay_bump_h + %s%s' % (grow, ' + 0.2 mm' if ramp_extra else '')), zb), (X('0 mm'), zt)])


def build_bay_panel(root, des):
    """Flat access panel: plate flush with the belly, cell scoop, front hook tabs, side snap arms with detent wedges."""
    add_bay_panel_params(des)
    comp = new_component(root, 'BatteryPanel')
    # envelope: the shell's own rear belly curve (YZ sketch: x = -z, y = y), extruded symmetric about x = 0
    sk = comp.sketches.add(comp.yZConstructionPlane); sk.name = 'Panel envelope'
    rect = rect4(sk, des, 'bot_d - 7 mm', 'bot_d', 'y_rear_out', 'bay_pan_y1')
    lines = [rect.item(i) for i in range(4)]
    def horiz(l): return abs(l.startSketchPoint.geometry.y - l.endSketchPoint.geometry.y) < 1e-9
    bottom = min([l for l in lines if horiz(l)], key=lambda l: l.startSketchPoint.geometry.y)
    right = max([l for l in lines if not horiz(l)], key=lambda l: l.startSketchPoint.geometry.x)
    def mid(l):
        a = l.startSketchPoint.geometry; b = l.endSketchPoint.geometry
        return P3((a.x + b.x) / 2, (a.y + b.y) / 2, 0)
    arc = sk.sketchCurves.sketchArcs.addFillet(right, mid(right), bottom, mid(bottom), ev_cm(des, 'edge_r'))
    g = arc.centerSketchPoint.geometry
    sk.sketchDimensions.addRadialDimension(arc, P3(g.x - 0.3, g.y + 0.3, 0)).parameter.expression = 'edge_r'
    inp = comp.features.extrudeFeatures.createInput(main_profile(sk), FE.NewBodyFeatureOperation)
    inp.setSymmetricExtent(VI('bay_pan_w'), True)
    f = comp.features.extrudeFeatures.add(inp); f.name = 'Panel envelope'
    body = f.bodies.item(0); body.name = 'BatteryPanel'
    sk = _xy_rects(comp, des, 'Panel rear trim', [('-(bay_pan_w / 2 + 1 mm)', 'bay_pan_w / 2 + 1 mm', 'y_rear_out - 1 mm', 'bay_pan_y0')])
    ext(comp, main_profile(sk), '10 mm', FE.CutFeatureOperation, 'Panel rear trim', start='-bot_d - 1 mm', target=body)
    sk = _xy_rects(comp, des, 'Panel tail trim', [
        ('-(bay_pan_w / 2 + 1 mm)', '-(bay_pan_nw / 2)', 'y_rear_out - 1 mm', 'bay_pan_step_y'),
        ('bay_pan_nw / 2', 'bay_pan_w / 2 + 1 mm', 'y_rear_out - 1 mm', 'bay_pan_step_y')])
    ext(comp, all_profiles(sk), '10 mm', FE.CutFeatureOperation, 'Panel tail trim', start='-bot_d - 1 mm', target=body)
    sk = _xy_rects(comp, des, 'Panel top trim', [('-(bay_pan_w / 2 + 1 mm)', 'bay_pan_w / 2 + 1 mm', 'y_rear_out - 1 mm', 'bay_pan_y1 + 1 mm')])
    ext(comp, main_profile(sk), '5 mm', FE.CutFeatureOperation, 'Panel top trim', start='bay_pan_t - bot_d', neg=False, target=body)
    sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Panel scoop'
    circle_at(sk, des, '0 mm', '-bay_axis_z', 'bay_cell_d + 2 * bay_trench_clr')
    ext(comp, main_profile(sk), 'bay_pan_y1 - bay_pan_y0 + 2 mm', FE.CutFeatureOperation, 'Panel scoop', start='bay_pan_y0 - 1 mm', target=body)
    sk = _xy_rects(comp, des, 'Panel hook tabs', [
        ('-bay_hook_x1', '-bay_hook_x0', 'bay_pan_y1 - 0.5 mm', 'bay_pan_y1 + bay_hook_len'),
        ('bay_hook_x0', 'bay_hook_x1', 'bay_pan_y1 - 0.5 mm', 'bay_pan_y1 + bay_hook_len')])
    ext(comp, all_profiles(sk), 'bay_hook_z1 - bay_hook_z0', FE.JoinFeatureOperation, 'Panel hook tabs', start='bay_hook_z0', target=body)
    ax = 'bay_pan_w / 2 - bay_arm_t'
    sk = _xy_rects(comp, des, 'Panel arm slits', [
        ('-(%s)' % ax, '-(%s - bay_slit_w)' % ax, 'bay_pan_step_y - 1 mm', 'bay_pan_step_y + bay_arm_len'),
        ('%s - bay_slit_w' % ax, '%s' % ax, 'bay_pan_step_y - 1 mm', 'bay_pan_step_y + bay_arm_len')])
    ext(comp, all_profiles(sk), 'bay_pan_t + 2 mm', FE.CutFeatureOperation, 'Panel arm slits', start='-bot_d - 1 mm', target=body)
    for sgn, nm in ((1, 'R'), (-1, 'L')):
        sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = 'Panel detent %s' % nm
        _bump_tri(sk, des, sgn, '0 mm')
        ext(comp, main_profile(sk), 'bay_bump_len', FE.JoinFeatureOperation, 'Panel detent %s' % nm, start='bay_pan_step_y + bay_bump_off', target=body)
    return comp


def build_bay_grooves(bot_comp, des, body):
    """Detent grooves in the rim walls (wedge + clearance)."""
    add_bay_panel_params(des)
    for sgn, nm in ((1, 'R'), (-1, 'L')):
        sk = bot_comp.sketches.add(bot_comp.xZConstructionPlane); sk.name = 'Bay groove %s' % nm
        _bump_tri(sk, des, sgn, 'bay_groove_clr', True)
        ext(bot_comp, main_profile(sk), 'bay_bump_len + 2 * bay_groove_clr', FE.CutFeatureOperation, 'Bay groove %s' % nm,
            start='bay_pan_step_y + bay_bump_off - bay_groove_clr', target=body)


CRADLE_PARAMS = [
    ('bay_cradle_t', '1.6 mm', 'cradle arc wall thickness (v1.1 housing wall)'),
    ('bay_cradle_deg', '45 deg', 'cradle arc top angle above the cell equator'),
    ('bay_cradle_y0', 'bay_clip_y + bay_clip_t', 'cradle arcs start at the rear clip front face (clip plate is wider than the arc gap)'),
    ('bay_cradle_y1', 'bay_pan_y1 + bay_pan_clr + 0.5 mm', 'cradle arcs end 0.5 mm past the bulkhead rear face (the cell tilts about it on removal)'),
    ('bay_cradle_ay1', 'bay_pan_y1 + bay_pan_clr + bay_bulk_t', 'cradle anchor blocks end at the front of the hook bulkhead'),
]


def add_cradle_params(des):
    add_bay_params(des)
    up = des.userParameters
    for n, e, c in CRADLE_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'deg' if 'deg' in e else 'mm', c)
        elif n.startswith('bay_cradle'):
            pr.expression = e


def build_bay_cradle(c, des, body):
    """Pair of arc walls (concentric with the cell, inner radius = trench radius) above the cell equator,
    anchored to the rear wall and to front blocks on the hook bulkhead. Sector sketch on the XZ plane (y = -z)."""
    add_cradle_params(des)
    rin_e = 'bay_cell_d / 2 + bay_trench_clr'
    rout_e = '(%s) + bay_cradle_t' % rin_e
    sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Bay cradle arcs'
    c1 = circle_at(sk, des, '0 mm', '-bay_axis_z', '2 * (%s)' % rin_e)
    cen = c1.centerSketchPoint.geometry
    c2 = sk.sketchCurves.sketchCircles.addByCenterRadius(c1.centerSketchPoint, ev_cm(des, rout_e))
    sk.sketchDimensions.addDiameterDimension(c2, P3(cen.x + 0.6, cen.y + 0.6, 0)).parameter.expression = '2 * (%s)' % rout_e
    L = sk.sketchCurves.sketchLines; gc = sk.geometricConstraints; dm = sk.sketchDimensions
    ln = ev_cm(des, rout_e) + 0.2
    len_e = '(%s) + 2 mm' % rout_e
    def spoke(dx, dy):
        l = L.addByTwoPoints(c1.centerSketchPoint, P3(cen.x + dx * ln, cen.y + dy * ln, 0))
        return l
    h_r = spoke(1, 0); h_l = spoke(-1, 0)
    k = math.sqrt(0.5)
    d_r = spoke(k, -k); d_l = spoke(-k, -k)       # sketch y = -z: up in z is -y
    gc.addHorizontal(h_r); gc.addHorizontal(h_l)
    for l in (h_r, h_l, d_r, d_l):
        d = dm.addDistanceDimension(l.startSketchPoint, l.endSketchPoint, adsk.fusion.DimensionOrientations.AlignedDimensionOrientation,
                                    P3(l.endSketchPoint.geometry.x, l.endSketchPoint.geometry.y + 0.3, 0))
        d.parameter.expression = len_e
    for a, b in ((h_r, d_r), (h_l, d_l)):
        ad = dm.addAngularDimension(a, b, P3(cen.x, cen.y - 0.5, 0))
        ad.parameter.expression = 'bay_cradle_deg'
    # pick the two sector profiles by their centroids (model space, cm)
    rin = ev_cm(des, rin_e); rout = ev_cm(des, rout_e)
    picked = adsk.core.ObjectCollection.create()
    axz = ev_cm(des, 'bay_axis_z')
    for p in sk.profiles:
        cp = p.areaProperties().centroid
        dx = cp.x; dz = (-axz) - cp.y      # centroid is in sketch space; sketch y = -z, so up = (-axz) - y
        r = math.hypot(dx, dz)
        ang = math.degrees(math.atan2(dz, abs(dx)))
        if rin < r < rout and 5 < ang < 40:
            picked.add(p)
    if picked.count != 2:
        info = []
        for p in sk.profiles:
            cp = p.areaProperties().centroid
            info.append((round(cp.x, 3), round(cp.y, 3), round(cp.z, 3), round(p.areaProperties().area, 4)))
        raise RuntimeError('cradle sector profiles: %d rin=%.3f rout=%.3f axz=%.3f %s' % (picked.count, rin, rout, axz, info))
    ext(c, picked, 'bay_cradle_y1 - bay_cradle_y0', FE.JoinFeatureOperation, 'Bay cradle arcs', start='bay_cradle_y0', target=body)
    # front anchor blocks under the arc ends, standing on the hook bulkhead
    sk = _xy_rects(c, des, 'Bay cradle anchors', [
        ('-(%s)' % rout_e, '-(%s)' % rin_e, 'bay_pan_y1 + bay_pan_clr', 'bay_cradle_ay1'),
        ('%s' % rin_e, '%s' % rout_e, 'bay_pan_y1 + bay_pan_clr', 'bay_cradle_ay1')])
    ext(c, all_profiles(sk), 'bay_axis_z + 0.3 mm - (bay_bulk_top_z - 0.3 mm)', FE.JoinFeatureOperation, 'Bay cradle anchors',
        start='bay_bulk_top_z - 0.3 mm', target=body)


SEAL_PARAMS = [
    ('bay_seal_xn0', 'bay_pan_nw / 2 + bay_pan_clr', 'narrow-zone wall inner face = narrow opening edge'),
    ('bay_seal_xw0', 'bay_pan_w / 2 + bay_pan_clr', 'wide-zone wall inner face = wide opening edge'),
    ('bay_seal_xw1', 'bay_seal_xw0 + bay_rim_w', 'wide-zone wall outer face = rim wall outer face'),
    ('bay_seal_y0', 'y_rear_out + wall - 0.1 mm', 'seal walls start 0.1 mm inside the rear wall'),
    ('bay_seal_ys', 'bay_pan_step_y - bay_pan_clr', 'step closure face, one panel clearance behind the panel step'),
    ('bay_seal_zlow', 'bay_floor_z - 0.3 mm', 'seal walls sink 0.3 mm into the floor'),
    ('bay_seal_zrim', 'bay_rim_top_z - 0.2 mm', 'rim-wall extension starts 0.2 mm below the rim top'),
    ('bay_seal_ztop', 'bay_axis_z + bay_cradle_t', 'seal wall / web top, 1.6 mm above the cell equator'),
    ('bay_seal_hold_y0', 'bay_front_y - bay_blk_t', 'front holder cheek start (y)'),
    ('bay_seal_hold_y1', 'bay_front_y + 1.55 mm', 'front holder rear face (y)'),
    ('bay_hold_x_in', 'bay_blk_w / 2 + bay_hold_clr + 0.5 mm', 'front closure starts inside the holder cheek'),
]


def add_seal_params(des):
    add_cradle_params(des)
    up = des.userParameters
    for n, e, c in SEAL_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'mm', c)
        else:
            pr.expression = e


def build_bay_seal(c, des, body):
    """Close the cell compartment: vertical walls under the cradle arcs (narrow zone), a step closure where the
    panel widens, rim-wall extensions + a web joining them to the arcs (wide zone), and front closures to the
    holder. Walls stay outside the cell radius and the panel opening (clearance bay_pan_clr)."""
    add_seal_params(des)
    t = 'bay_cradle_t'
    def mir(x0, x1, y0, y1):
        return [('%s' % x0, '%s' % x1, y0, y1), ('-(%s)' % x1, '-(%s)' % x0, y0, y1)]
    # 1 narrow-zone walls under the arcs, floor up to just above the equator (joins the arc bottoms)
    r = mir('bay_seal_xn0', 'bay_seal_xn0 + %s' % t, 'bay_seal_y0', 'bay_seal_ys - %s' % t)
    sk = _xy_rects(c, des, 'Bay seal narrow', r)
    ext(c, all_profiles(sk), 'bay_axis_z + 0.3 mm - bay_seal_zlow', FE.JoinFeatureOperation, 'Bay seal narrow',
        start='bay_seal_zlow', target=body)
    # 2 step closure + front side walls + holder closure (floor to wall top)
    r = (mir('bay_seal_xn0', 'bay_seal_xw1', 'bay_seal_ys - %s' % t, 'bay_seal_ys')
         + mir('bay_seal_xw0', 'bay_seal_xw1', 'bay_cradle_ay1', 'bay_seal_hold_y0')
         + mir('bay_hold_x_in', 'bay_seal_xw1', 'bay_seal_hold_y0', 'bay_seal_hold_y1'))
    sk = _xy_rects(c, des, 'Bay seal step front', r)
    ext(c, all_profiles(sk), 'bay_seal_ztop - bay_seal_zlow', FE.JoinFeatureOperation, 'Bay seal step front',
        start='bay_seal_zlow', target=body)
    # 3 rim-wall extensions beside the wide opening
    sk = _xy_rects(c, des, 'Bay seal rim', mir('bay_seal_xw0', 'bay_seal_xw1', 'bay_seal_ys', 'bay_cradle_ay1'))
    ext(c, all_profiles(sk), 'bay_seal_ztop - bay_seal_zrim', FE.JoinFeatureOperation, 'Bay seal rim',
        start='bay_seal_zrim', target=body)
    # 4 web from the arc outer faces out to the rim-wall extensions
    sk = _xy_rects(c, des, 'Bay seal web', mir('bay_cell_d / 2 + bay_trench_clr + 1 mm', 'bay_seal_xw0', 'bay_seal_ys', 'bay_cradle_ay1'))
    ext(c, all_profiles(sk), 'bay_seal_ztop - bay_axis_z', FE.JoinFeatureOperation, 'Bay seal web',
        start='bay_axis_z', target=body)


TRIG_PARAMS = [
    ('trig_sc_y0', '9.1 mm', 'scoop curve fit point 0 (y) - Jess drawn curve, at the belly'),
    ('trig_sc_z0', '-bot_d', 'scoop curve fit point 0 (z) = belly'),
    ('trig_sc_y1', '16.16 mm', 'scoop curve fit point 1 (y)'),
    ('trig_sc_z1', '-12.99 mm', 'scoop curve fit point 1 (z)'),
    ('trig_sc_y2', '23.77 mm', 'scoop curve fit point 2 (y)'),
    ('trig_sc_z2', '-7.76 mm', 'scoop curve fit point 2 (z)'),
    ('trig_sc_y3', '30.3 mm', 'scoop curve fit point 3 (y) - ends in the front wall'),
    ('trig_sc_z3', '-5.31 mm', 'scoop curve fit point 3 (z)'),
    ('trig_floor_t', '2.2 mm', 'ramp floor thickness, normal to the scoop curve'),
    ('trig_ramp_y', 'y_front_out - wall', 'ramp floor stops at the front wall inner face'),
    ('trig_cut_y1', 'y_front_out + 1.1 mm', 'scoop cut closes past the front face'),
    ('trig_cut_zlow', '-bot_d - 1.5 mm', 'scoop cut bottom, below the belly'),
    ('trig_cut_x', 'shell_w / 2 + 1 mm', 'scoop cut half-width, past the side walls'),
]


def add_trig_params(des):
    up = des.userParameters
    for n, e, c in TRIG_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'mm', c)
        else:
            pr.expression = e


def build_trigger_scoop(c, des, body):
    """Scoop the belly in front of the battery bay for the trigger: spline cut across the full width below the
    curve, plus a ramp floor strip (offset of the curve) up to the front wall. YZ sketch: (x_s, y_s) = (-z, y)."""
    add_trig_params(des)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger scoop'
    O = sk.originPoint; dm = sk.sketchDimensions; gc = sk.geometricConstraints
    Hz = adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation
    Vt = adsk.fusion.DimensionOrientations.VerticalDimensionOrientation
    ev = lambda e: ev_cm(des, e)
    def S(y_e, z_e): return sk.modelToSketchSpace(P3(0, ev(y_e), ev(z_e)))
    names = [('trig_sc_y%d' % i, 'trig_sc_z%d' % i) for i in range(4)]
    oc = adsk.core.ObjectCollection.create()
    for yn, zn in names: oc.add(S(yn, zn))
    sp = sk.sketchCurves.sketchFittedSplines.add(oc)
    for i, fp in enumerate(sp.fitPoints):
        yn, zn = names[i]; g = fp.geometry
        dm.addDistanceDimension(O, fp, Hz, P3(g.x / 2, g.y + 0.3, 0)).parameter.expression = '-(%s)' % zn
        dm.addDistanceDimension(O, fp, Vt, P3(g.x + 0.3, g.y / 2, 0)).parameter.expression = yn
    # ramp floor curve: offset of the scoop curve, toward +z (negative distance on this side)
    inp = gc.createOffsetInput([sp], VI('-(trig_floor_t)'))
    oco = gc.addOffset2(inp)
    osp = list(oco.childCurves)[0]
    # cut closure: P3 -> Q1 -> Q2 -> Q3 -> P0
    L = sk.sketchCurves.sketchLines
    p0 = sp.startSketchPoint; p3 = sp.endSketchPoint
    q1g = S('trig_cut_y1', 'trig_sc_z3'); q2g = S('trig_cut_y1', 'trig_cut_zlow'); q3g = S('trig_sc_y0', 'trig_cut_zlow')
    l1 = L.addByTwoPoints(p3, q1g)
    l2 = L.addByTwoPoints(l1.endSketchPoint, q2g)
    l3 = L.addByTwoPoints(l2.endSketchPoint, q3g)
    l4 = L.addByTwoPoints(l3.endSketchPoint, p0)
    gc.addVertical(l1); gc.addHorizontal(l2); gc.addVertical(l3); gc.addHorizontal(l4)
    q1 = l1.endSketchPoint; q2 = l2.endSketchPoint
    dm.addDistanceDimension(O, q1, Vt, P3(q1.geometry.x + 0.3, q1.geometry.y / 2, 0)).parameter.expression = 'trig_cut_y1'
    dm.addDistanceDimension(O, q2, Hz, P3(q2.geometry.x / 2, q2.geometry.y + 0.3, 0)).parameter.expression = '-(trig_cut_zlow)'
    # start cap of the ramp floor strip
    cap = L.addByTwoPoints(p0, osp.startSketchPoint)
    # front stop line at the front wall inner face (constant y => horizontal in this sketch), bottom on l3, top on the offset curve
    vg0 = S('trig_ramp_y', 'trig_cut_zlow'); vg1 = S('trig_ramp_y', '-3 mm')
    v = L.addByTwoPoints(vg0, vg1)
    gc.addHorizontal(v)
    gc.addCoincident(v.startSketchPoint, l3)
    gc.addCoincident(v.endSketchPoint, osp)
    dm.addOffsetDimension(v, O, P3(v.startSketchPoint.geometry.x, v.startSketchPoint.geometry.y / 2, 0)).parameter.expression = 'trig_ramp_y'
    # classify profiles against the scoop curve
    import bisect
    evl = sp.geometry.evaluator; ok, s0, s1 = evl.getParameterExtents()
    pts = []
    for i in range(201):
        ok, pt = evl.getPointAtParameter(s0 + (s1 - s0) * i / 200.0); pts.append((pt.y, pt.x))
    pts.sort(); ys = [p[0] for p in pts]
    def xs_at(y):
        j = min(max(bisect.bisect_left(ys, y), 1), len(ys) - 1)
        (ya, xa), (yb, xb) = pts[j - 1], pts[j]
        return xa + (xb - xa) * (y - ya) / (yb - ya) if yb != ya else xa
    ramp_y = ev('trig_ramp_y')
    cut = adsk.core.ObjectCollection.create(); floor = adsk.core.ObjectCollection.create(); info = []
    for p in sk.profiles:
        cp = p.areaProperties().centroid; info.append((round(cp.x, 3), round(cp.y, 3), round(p.areaProperties().area, 4)))
        if cp.x > xs_at(cp.y): cut.add(p)         # larger x_s = lower z = below the curve
        elif cp.y < ramp_y: floor.add(p)
    if cut.count != 2 or floor.count != 1:
        raise RuntimeError('scoop profiles cut=%d floor=%d %s' % (cut.count, floor.count, info))
    f = ext(c, cut, '2 * trig_cut_x', FE.CutFeatureOperation, 'Trigger scoop cut', start='-trig_cut_x', target=body)
    ext(c, floor, 'shell_w - wall', FE.JoinFeatureOperation, 'Trigger ramp floor', start='-(shell_w - wall) / 2', target=body)
    return sk, sp, osp


TRIG_SLOT_PARAMS = [
    ('trig_x0', '-9.45 mm', 'trigger body left edge (x)'),
    ('trig_x1', '8.7 mm', 'trigger body right edge (x)'),
    ('trig_clr', '0.4 mm', 'trigger side clearance in the slot'),
    ('trig_slot_y0', '11.65 mm', 'trigger slot rear edge (full-pull lobe tip 12.4 mm less clearance)'),
    ('trig_slot_ztop', '2.5 mm', 'slot cut top, past the shell top so no ceiling lip is left'),
]


def build_trigger_slot(c, des, body):
    """Centre slot through the ramp floor for the trigger (also relieves the front screw columns), then
    re-cut the front screw head wells through the new ramp floor."""
    add_trig_params(des)
    up = des.userParameters
    for n, e, cm in TRIG_SLOT_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', cm)
        else: pr.expression = e
    sk = _xy_rects(c, des, 'Trigger slot', [('trig_x0 - trig_clr', 'trig_x1 + trig_clr', 'trig_slot_y0', 'trig_ramp_y')])
    ext(c, all_profiles(sk), 'trig_slot_ztop - trig_cut_zlow', FE.CutFeatureOperation, 'Trigger slot', start='trig_cut_zlow', target=body)
    sk = c.sketches.add(c.xYConstructionPlane); sk.name = 'Screw head wells (ramp)'
    circle_at(sk, des, 'mh1_x', 'mh1_y', 'head_well_d')
    circle_at(sk, des, 'mh2_x', 'mh2_y', 'head_well_d')
    ext(c, all_profiles(sk), 'bot_d + screw_head_seat_z', FE.CutFeatureOperation, 'Screw head wells (ramp)', start='-bot_d', target=body)
    # the left screw column leaves only ~0.3 mm between the head well and the slot: open the well into the slot (keyhole)
    sk = _xy_rects(c, des, 'Left well keyhole', [('mh1_x', 'trig_x0 - trig_clr', 'mh1_y - head_well_d / 2', 'mh1_y + head_well_d / 2')])
    ext(c, all_profiles(sk), 'bot_d + screw_head_seat_z', FE.CutFeatureOperation, 'Left well keyhole', start='-bot_d', target=body)


# ---------------------------------------------------------------- pivot trigger (v2) ----
# v1.1 blade side profile (y, z mm) shifted to the v2 pivot (26.3, -3.0): rest pose, before the nose / tip trims
TRIG_PROFILE = [(21.6, -17.06), (23.19, -18.4), (24.4, -19.12), (25.54, -19.36), (25.91, -19.24), (26.15, -18.95), (26.19, -18.29),
                (25.68, -15.85), (25.51, -14.25), (25.52, -12.55), (25.84, -10.27), (26.46, -8.14), (27.27, -6.38), (27.7, -6.3),
                (28.9, -4.6), (28.9, -4.4), (29.3, -4.4), (29.3, -3.4), (28.9, -3.4), (28.9, -3.0), (28.1, -1.1), (26.3, -0.4),
                (24.5, -1.1), (23.7, -3.0), (23.85, -6.04), (18.5, -7.73), (18.5, -7.5), (16.8, -7.5), (16.8, -8.28), (15.33, -8.73),
                (14.45, -9.22), (14.16, -9.55), (14.22, -9.84), (14.49, -10.13), (15.43, -10.4), (16.15, -10.92), (18.26, -13.56)]

TRIG2_PARAMS = [
    ('trig_piv_y', '25.3 mm', 'trigger pivot axis y (axis runs along x); 1.0 mm behind the v1.1-derived 26.3 so the hub rim (r 2.6) clears the front wall / bumper at every pull angle'),
    ('trig_piv_z', '-3.0 mm', 'trigger pivot axis z'),
    ('trig_rest_deg', '8 deg', 'rest pose is rotated this far forward of the original rest pose (more finger stroke); the pull end stays where it was'),
    ('trig_pull_deg', '28 deg + trig_rest_deg', 'total trigger rotation from rest to full pull (28 deg plus the forward rest offset)'),
    ('trig_x0', '-9.45 mm', 'trigger body left edge (x); slot / spring envelope is trig_x0..trig_x1'),
    ('trig_bx1', '7.6 mm', 'trigger blade right edge (x); coil sits between here and the slot wall'),
    ('trig_nose_y', 'trig_ramp_y - 0.4 mm', 'trigger front trim: 0.4 mm behind the front wall inner face'),
    ('trig_tip_z', '-bot_d + 0.3 mm', 'trigger tip trim: 0.3 mm above the belly'),
    ('trig_post_d', '2.9 mm', 'pivot post diameter (D shape, as v1.1)'),
    ('trig_post_flat', '0.55 mm', 'pivot post flat depth, on the +z side'),
    ('trig_bore_clr', '0.15 mm', 'hub bore clearance per side around the post'),
    ('trig_sock_l', '-10.35 mm', 'left post socket end (x), 0.5 mm into the left column wall'),
    ('trig_sock_r', '9.8 mm', 'right post socket end (x), 0.7 mm into the right column wall'),
    ('trig_sock_clr', '0.15 mm', 'socket clearance per side around the post'),
    ('trig_post_x0', 'trig_sock_l + 0.15 mm', 'post left end'),
    ('trig_post_x1', 'trig_sock_r - 0.15 mm', 'post right end'),
    ('trig_hall_y', '23.25 mm', 'hall sensor y (U_HALL1)'),
    ('trig_hall_z', '0.91 mm', 'hall package bottom face z'),
    ('trig_hall_gap', '1.1 mm', 'magnet top face to hall package gap at full pull'),
    ('trig_mag_x', '-8.0 mm', 'magnet centre x (hall is at -8.5; nudged so the pocket wall clears the slot)'),
    ('trig_mag_d', '2.0 mm', 'magnet diameter (2 x 2 mm N52 cylinder)'),
    ('trig_mag_t', '2.0 mm', 'magnet thickness'),
    ('trig_mag_clr', '0.1 mm', 'magnet pocket clearance (diameter)'),
    ('trig_mag_wall', '0.4 mm', 'pocket wall'),
    ('trig_mag_floor', '1.0 mm', 'pocket floor thickness'),
    ('trig_mag_ty', 'trig_piv_y + (trig_hall_y - trig_piv_y) * cos(trig_pull_deg) - (trig_hall_z - trig_hall_gap - trig_piv_z) * sin(trig_pull_deg)',
     'magnet top face y in the rest pose (hall point rotated back by the pull angle about the pivot)'),
    ('trig_mag_tz', 'trig_piv_z + (trig_hall_y - trig_piv_y) * sin(trig_pull_deg) + (trig_hall_z - trig_hall_gap - trig_piv_z) * cos(trig_pull_deg)',
     'magnet top face z in the rest pose'),
    ('trig_sw_x', '2.7 mm', 'hub notch half-width for the bumper switch (switch body +-2.3)'),
    ('trig_sw_floor_z', 'trig_piv_z', 'hub notch floor z: everything above the pivot level is removed under the switch (it sweeps up with the pull)'),
]


def add_trig2_params(des):
    add_trig_params(des)
    up = des.userParameters
    for n, e, c in TRIG2_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'deg' if n in ('trig_rest_deg', 'trig_pull_deg') else 'mm', c)
        else:
            pr.expression = e
    for n, e in (('trig_x0', '-9.45 mm'),):
        pass


def _clip(pts, keep):
    """Sutherland-Hodgman against one half-plane; keep(p) -> signed distance (>= 0 kept)."""
    out = []
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        dp, dq = keep(p), keep(q)
        if dp >= 0: out.append(p)
        if (dp >= 0) != (dq >= 0):
            t = dp / (dp - dq)
            out.append((p[0] + (q[0] - p[0]) * t, p[1] + (q[1] - p[1]) * t))
    return out


def _d_profile(c, des, name, dia_e, flat_e):
    """YZ sketch: circle of diameter dia_e about the pivot with a flat chord on the +z side, flat_e from the centre.
    Returns (sketch, D profile)."""
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = name
    circ = circle_at(sk, des, '-(trig_piv_z)', 'trig_piv_y', dia_e)
    cx = circ.centerSketchPoint.geometry.x
    L = sk.sketchCurves.sketchLines; dm = sk.sketchDimensions; O = sk.originPoint
    xc = cx - ev_cm(des, flat_e)
    ln = L.addByTwoPoints(P3(xc, ev_cm(des, 'trig_piv_y - 3 mm'), 0), P3(xc, ev_cm(des, 'trig_piv_y + 3 mm'), 0))
    sk.geometricConstraints.addVertical(ln)
    HO = adsk.fusion.DimensionOrientations
    dm.addDistanceDimension(circ.centerSketchPoint, ln.startSketchPoint, HO.HorizontalDimensionOrientation, P3(xc, ev_cm(des, 'trig_piv_y') - 0.5, 0)).parameter.expression = flat_e
    dm.addDistanceDimension(O, ln.startSketchPoint, HO.VerticalDimensionOrientation, P3(xc + 1, ev_cm(des, 'trig_piv_y - 3 mm') / 2, 0)).parameter.expression = 'trig_piv_y - 3 mm'
    dm.addDistanceDimension(O, ln.endSketchPoint, HO.VerticalDimensionOrientation, P3(xc + 1, ev_cm(des, 'trig_piv_y + 3 mm') / 2, 0)).parameter.expression = 'trig_piv_y + 3 mm'
    return sk, max(sk.profiles, key=lambda p: p.areaProperties().area)


def build_trigger(root, des):
    """Blade body (v1.1 profile, trimmed), D pivot bore, magnet tab + pocket, bumper-switch notch. Rest pose."""
    add_trig2_params(des)
    c = new_component(root, 'Trigger')
    ev = lambda e: ev_cm(des, e) * 10.0
    py, pz = ev('trig_piv_y'), ev('trig_piv_z')
    nose, tip = ev('trig_nose_y'), ev('trig_tip_z')
    pts = [(py + (y - 26.3), pz + (z + 3.0)) for y, z in TRIG_PROFILE]      # profile was extracted about the (26.3, -3.0) pivot
    rest = des.userParameters.itemByName('trig_rest_deg').value       # radians
    pts = [(py + (p[0] - py) * math.cos(rest) - (p[1] - pz) * math.sin(rest), pz + (p[0] - py) * math.sin(rest) + (p[1] - pz) * math.cos(rest)) for p in pts]
    pts = _clip(pts, lambda p: nose - p[0])
    pts = _clip(pts, lambda p: p[1] - tip)
    rows = []
    for y, z in pts:
        ye = 'trig_nose_y' if abs(y - nose) < 1e-6 else '(trig_piv_y + (%.3f mm))' % (y - py)
        ze = 'trig_tip_z' if abs(z - tip) < 1e-6 else '(trig_piv_z + (%.3f mm))' % (z - pz)
        rows.append(('-(%s)' % ze, ye))        # sketch (x_s, y_s) = (-z, y)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger blade profile'
    poly_pts(sk, des, rows)
    ext(c, main_profile(sk), 'trig_bx1 - trig_x0', FE.NewBodyFeatureOperation, 'Trigger blade', start='trig_x0')
    body = c.bRepBodies.item(0); body.name = 'Trigger'
    # magnet tab behind the hub (rest pose), carries the pocket under the hall point
    tab_top = 'trig_mag_tz'
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger magnet tab'
    rect4(sk, des, '-(trig_mag_tz)', '-(trig_mag_tz - trig_mag_t - trig_mag_floor - 0.1 mm)',
          'trig_mag_ty - trig_mag_d / 2 - trig_mag_clr / 2 - trig_mag_wall', 'trig_piv_y - 2 mm')
    ext(c, all_profiles(sk), 'trig_mag_x + trig_mag_d / 2 + trig_mag_clr / 2 + trig_mag_wall - trig_x0', FE.JoinFeatureOperation,
        'Trigger magnet tab', start='trig_x0', target=body)
    sk = c.sketches.add(c.xYConstructionPlane); sk.name = 'Trigger magnet pocket'
    circle_at(sk, des, 'trig_mag_x', 'trig_mag_ty', 'trig_mag_d + trig_mag_clr')
    ext(c, all_profiles(sk), 'trig_mag_t + 0.1 mm', FE.CutFeatureOperation, 'Trigger magnet pocket', neg=True, start='trig_mag_tz', target=body)
    # D bore for the pivot post
    sk, prof = _d_profile(c, des, 'Trigger D bore', 'trig_post_d + 2 * trig_bore_clr', 'trig_post_d / 2 - trig_post_flat + trig_bore_clr')
    ext(c, prof, 'trig_bx1 - trig_x0 + 2 mm', FE.CutFeatureOperation, 'Trigger D bore', start='trig_x0 - 1 mm', target=body)
    # hub notch under the bumper switch
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger switch notch'
    rect4(sk, des, '-(1 mm)', '-(trig_sw_floor_z)', 'trig_piv_y - 3.6 mm', 'trig_piv_y + 3.6 mm')
    ext(c, all_profiles(sk), '2 * trig_sw_x', FE.CutFeatureOperation, 'Trigger switch notch', start='-trig_sw_x', target=body)
    return c


def build_trigger_post(root, des):
    add_trig2_params(des)
    c = new_component(root, 'TriggerPost')
    sk, prof = _d_profile(c, des, 'Trigger post D', 'trig_post_d', 'trig_post_d / 2 - trig_post_flat')
    ext(c, prof, 'trig_post_x1 - trig_post_x0', FE.NewBodyFeatureOperation, 'Trigger post', start='trig_post_x0')
    c.bRepBodies.item(0).name = 'TriggerPost'
    return c


def build_trigger_sockets(c, des, body):
    """U-slots (open to the top) in the left / right column walls for the pivot post ends."""
    add_trig2_params(des)
    for side, x0, x1, nm in (('L', 'trig_sock_l', '-9 mm', 'left'), ('R', '9 mm', 'trig_sock_r', 'right')):
        sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger socket ' + nm
        circle_at(sk, des, '-(trig_piv_z)', 'trig_piv_y', 'trig_post_d + 2 * trig_sock_clr')
        rect4(sk, des, '-(trig_slot_ztop)', '-(trig_piv_z)', 'trig_piv_y - trig_post_d / 2 - trig_sock_clr', 'trig_piv_y + trig_post_d / 2 + trig_sock_clr')
        ext(c, all_profiles(sk), '(%s) - (%s)' % (x1, x0), FE.CutFeatureOperation, 'Trigger socket ' + nm, start=x0, target=body)


# ---------------------------------------------------------------- torsion spring mount (v2) ----
# spring leg geometry relative to the pivot (y, z mm), from the v1.1 model (trigger-torsion-spring.md)
SPR_S = (0.85, -1.81)      # moving leg leaves the coil here
SPR_T = (-5.04, -4.55)     # moving leg tip
SPR_H = (-5.67, -3.19)     # moving leg hook end
SPR_SHIFT = (19.45, -15.9, -1.4)   # v1.1 -> v2 translation of SpringRef (x, y, z mm): coil centre x -11.1 -> 8.35

SPRING_PARAMS = [
    ('trig_ear_x1', '8.3 mm', 'spring ear outer face (x)'),
    ('trig_ear_w', '1.2 mm', 'spring ear half-width around the moving leg'),
    ('trig_leg_slot', '0.3 mm', 'moving leg slot half-width (wire 0.35 + 0.25 clearance)'),
    ('trig_fix_x', '8.7 mm', 'fixed leg x'),
    ('trig_fix_z', 'trig_piv_z + 2 mm', 'fixed leg z (top of the coil)'),
    ('trig_fix_y1', 'trig_piv_y - 6.5 mm', 'fixed leg tip y'),
    ('trig_rib_x0', '8.1 mm', 'fixed leg rib: left edge'),
    ('trig_rib_x1', '11.5 mm', 'fixed leg rib: right edge (anchors into the right screw column)'),
    ('trig_rib_y0', 'trig_fix_y1 + 0.2 mm', 'fixed leg rib: rear edge'),
    ('trig_rib_y1', '23.3 mm', 'fixed leg rib: front edge'),
    ('trig_rib_z0', 'trig_fix_z + 0.15 mm', 'fixed leg rib: underside, bears on the wire top'),
    ('trig_rib_top', 'pcb_bot_z', 'fixed leg rib: top (PCB underside)'),
]


def add_spring_params(des):
    add_trig2_params(des)
    up = des.userParameters
    for n, e, c in SPRING_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', c)
        else: pr.expression = e


def place_spring(root):
    """Move the v1.1 spring reference to the v2 pivot (MoveFeature on its body, sticky in the timeline)."""
    occ = root.occurrences.itemByName('SpringRef_v1.1:1')
    m = [[1, 0, 0, SPR_SHIFT[0] / 10.0], [0, 1, 0, SPR_SHIFT[1] / 10.0], [0, 0, 1, SPR_SHIFT[2] / 10.0]]
    body_occ = occ.childOccurrences.item(0) if occ.childOccurrences.count else occ
    f = _move_occ(root, body_occ, m)
    f.name = 'Place spring (v2 pivot)'
    return occ


def _seg_strip(a, b, w, l0=0.0, l1=0.0):
    """Rectangle around segment a->b (extended by l0 / l1 at the ends), half-width w. Points (dy, dz) rel pivot."""
    dx, dy = b[0] - a[0], b[1] - a[1]; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    nx, ny = -dy, dx
    p0 = (a[0] - dx * l0, a[1] - dy * l0); p1 = (b[0] + dx * l1, b[1] + dy * l1)
    return [(p0[0] + nx * w, p0[1] + ny * w), (p1[0] + nx * w, p1[1] + ny * w), (p1[0] - nx * w, p1[1] - ny * w), (p0[0] - nx * w, p0[1] - ny * w)]


def _rel_rows(pts):
    return [('-(trig_piv_z + (%.3f mm))' % z, 'trig_piv_y + (%.3f mm)' % y) for y, z in pts]


def build_trigger_ear(c, des, body):
    """Ear on the blade's right face carrying the spring's moving leg: through-slot along the leg + hook pocket."""
    add_spring_params(des)
    rest = des.userParameters.itemByName('trig_rest_deg').value
    _r = lambda p: (p[0] * math.cos(rest) - p[1] * math.sin(rest), p[0] * math.sin(rest) + p[1] * math.cos(rest))
    R_S, R_T, R_H = _r(SPR_S), _r(SPR_T), _r(SPR_H)
    dx, dy = R_T[0] - R_S[0], R_T[1] - R_S[1]; n = math.hypot(dx, dy); d = (dx / n, dy / n)
    pa = (R_S[0] + d[0] * 2.6, R_S[1] + d[1] * 2.6)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger spring ear'
    poly_pts(sk, des, _rel_rows(_seg_strip(pa, R_T, 1.2, 0, 0.7)))
    poly_pts(sk, des, _rel_rows(_seg_strip(R_T, R_H, 1.0, 0.9, 0.5)))
    ext(c, all_profiles(sk), 'trig_ear_x1 - trig_bx1', FE.JoinFeatureOperation, 'Trigger spring ear', start='trig_bx1', target=body)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger leg slot'
    pb = (R_S[0] + d[0] * 2.0, R_S[1] + d[1] * 2.0)
    poly_pts(sk, des, _rel_rows(_seg_strip(pb, R_T, 0.3, 0, 0.3)))
    poly_pts(sk, des, _rel_rows(_seg_strip(R_T, R_H, 0.3, 0.3, 0.3)))
    ext(c, all_profiles(sk), 'trig_ear_x1 - trig_bx1 + 0.4 mm', FE.CutFeatureOperation, 'Trigger leg slot', start='trig_bx1 - 0.2 mm', target=body)


def build_spring_rib(c, des, body):
    """Overhang under the PCB that holds the spring's fixed leg (wire bears up into a 0.6 mm groove)."""
    add_spring_params(des)
    sk = _xy_rects(c, des, 'Spring leg rib', [('trig_rib_x0', 'trig_rib_x1', 'trig_rib_y0', 'trig_rib_y1')])
    ext(c, all_profiles(sk), 'trig_rib_top - trig_rib_z0', FE.JoinFeatureOperation, 'Spring leg rib', start='trig_rib_z0', target=body)
    sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Spring leg groove'
    rect4(sk, des, 'trig_fix_x - 0.3 mm', 'trig_fix_x + 0.3 mm', '-(trig_rib_z0 + 0.45 mm)', '-(trig_rib_z0 - 0.1 mm)')
    ext(c, all_profiles(sk), 'trig_rib_y1 - trig_rib_y0 + 0.2 mm', FE.CutFeatureOperation, 'Spring leg groove', start='trig_rib_y0 - 0.1 mm', target=body)


def build_ramp_trim(comp, body):
    """Trim the ramp-floor strip's lateral ends to the true outer envelope (plan outline + edge_r bottom fillet).
    Envelope tool -> cut out of an oversized box -> cut the remainder from the shell."""
    des = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    FEo = adsk.fusion.FeatureOperations
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan outer envelope'
    plan(sk, '0 mm')
    f = ext(comp, main_profile(sk), 'bot_d - lap_h', FEo.NewBodyFeatureOperation, 'Outer envelope', neg=True, start='-lap_h')
    env = f.bodies.item(0); env.name = 'Outer envelope'
    zb = -des.unitsManager.evaluateExpression('bot_d', 'cm')
    fillet_face_edges(comp, planar_face_at(env, zb, -1), 'edge_r', 'Outer envelope fillet')
    sk2 = comp.sketches.add(comp.xYConstructionPlane); sk2.name = 'Plan trim box'
    plan(sk2, '-2 mm')
    f2 = ext(comp, main_profile(sk2), 'edge_r + 2 mm', FEo.NewBodyFeatureOperation, 'Trim box', neg=True, start='-(bot_d - edge_r)')
    box = f2.bodies.item(0); box.name = 'Trim box'
    for tgt, tool, nm in ((box, env, 'Trim tool (box minus envelope)'), (body, box, 'Trim ramp strip to shell fillet')):
        t = adsk.core.ObjectCollection.create(); t.add(tool)
        ci = comp.features.combineFeatures.createInput(tgt, t)
        ci.operation = FEo.CutFeatureOperation; ci.isKeepToolBodies = False
        cf = comp.features.combineFeatures.add(ci); cf.name = nm
    return body


def build_trigger_rim_fillets(comp, des):
    """Round the trigger scoop rim: trig_rim_r (3 mm) on the rear lip + side rims where the finger rolls over,
    trig_lip_r (0.8 mm) on the front lip. Edges are found by geometry (NURBS scoop face vs outer face)."""
    up = des.userParameters
    for n, v, c in (('trig_rim_r', '3 mm', 'Trigger scoop rim fillet (rear lip + side rims)'),
                    ('trig_lip_r', '0.8 mm', 'Trigger scoop front lip fillet')):
        if not up.itemByName(n):
            up.add(n, adsk.core.ValueInput.createByString(v), 'mm', c)
    body = comp.bRepBodies.item(0)
    ev = lambda e: des.unitsManager.evaluateExpression(e, 'mm') * 10   # evaluateExpression returns cm
    ymin, yfr = ev('trig_sc_y0'), ev('trig_sc_y3')
    def bb(e):
        b = e.boundingBox
        return [v * 10 for v in (b.minPoint.x, b.maxPoint.x, b.minPoint.y, b.maxPoint.y, b.minPoint.z, b.maxPoint.z)]
    big = adsk.core.ObjectCollection.create()
    for e in body.edges:
        x0, x1, y0, y1, z0, z1 = bb(e)
        st = sorted(f.geometry.surfaceType for f in e.faces)
        if 7 in st and y0 >= ymin - 0.05 and y1 <= 27.25 and z1 < -11.4 + 5.0 and e.length * 10 > 3 and st != [7, 7] and (
                (st == [0, 7] and abs(y0 - ymin) < 0.05 and abs(y1 - ymin) < 0.05) or st == [1, 7] and y1 < 27.25 or
                (st == [0, 7] and abs(abs(x0) - 17.5) < 0.05 and abs(x1 - x0) < 0.05)):
            big.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(big, adsk.core.ValueInput.createByString('trig_rim_r'), False)
    f1 = comp.features.filletFeatures.add(fi); f1.name = 'Trigger rim fillet (roll-over)'
    body = comp.bRepBodies.item(0)
    lip = adsk.core.ObjectCollection.create()
    for e in body.edges:
        x0, x1, y0, y1, z0, z1 = bb(e)
        if sorted(f.geometry.surfaceType for f in e.faces) == [0, 7] and abs(y1 - yfr) < 0.06 and abs(z1 - ev('trig_sc_z3')) < 0.06 and (x1 - x0) > 2:
            lip.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(lip, adsk.core.ValueInput.createByString('trig_lip_r'), True)
    f2 = comp.features.filletFeatures.add(fi); f2.name = 'Trigger front lip fillet'
    return f1, f2
# NOTE: the rim-edge selection above is a best-effort reconstruction of what was applied interactively
# (edges: rear lip y=trig_sc_y0, belly-corner arcs, side-wall rims x=+-17.5 up to y 27.2); verify on a rebuild.


# ------------------------------------------------- slot widening, rib pillar, post caps (2026-11-03) ----
SLOT_CAP_PARAMS = [
    ('trig_leg_slot', '0.4 mm', 'moving leg slot half-width (0.8 mm = 2 perimeters at a 0.4 nozzle)'),
    ('trig_groove_w', '0.4 mm', 'fixed leg groove half-width under the rib (0.8 mm total)'),
    ('trig_rib_x0', '7.9 mm', 'fixed leg rib: left edge (leaves a 0.4 mm wall beside the 0.8 groove)'),
    ('trig_pil_x0', '9.3 mm', 'rib pillar: left edge (0.2 mm clear of the groove / wire)'),
    ('trig_pil_x1', 'trig_rib_x1', 'rib pillar: right edge'),
    ('trig_pil_y1', '22.7 mm', 'rib pillar: front edge (keeps 0.3 mm clear of the right screw head well)'),
    ('trig_pil_zb0', '-9.6 mm', 'rib pillar bottom z at y = trig_rib_y0 (buried 1.3 mm in the ramp floor)'),
    ('trig_pil_zb1', '-7.1 mm', 'rib pillar bottom z at y = trig_pil_y1 (buried 1.3 mm in the ramp floor)'),
    ('trig_cap_clr', '0.05 mm', 'post cap: clearance to the socket end'),
    ('trig_cap_t_l', '0.45 mm', 'post cap thickness, left socket'),
    ('trig_cap_t_r', '0.6 mm', 'post cap thickness, right socket'),
    ('trig_cap_w', '3.0 mm', 'post cap width (socket is trig_post_d + 2 * trig_sock_clr = 3.2)'),
    ('trig_cap_nub', '0.1 mm', 'post cap: friction nub protrusion each side'),
    ('trig_cap_gap', '0.1 mm', 'post cap: clearance above the post and below the PCB'),
]


def add_slot_cap_params(des):
    add_spring_params(des)
    up = des.userParameters
    for n, e, c in SLOT_CAP_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', c)
        else: pr.expression = e


def _seg_strip_w(a, b, wexp, l0=0.0, l1=0.0):
    """Like _seg_strip but the half-width is the parameter expression wexp. Returns (z_expr, y_expr) rows."""
    dx, dy = b[0] - a[0], b[1] - a[1]; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    nx, ny = -dy, dx
    p0 = (a[0] - dx * l0, a[1] - dy * l0); p1 = (b[0] + dx * l1, b[1] + dy * l1)
    rows = []
    for (py_, pz_), s in ((p0, 1), (p1, 1), (p1, -1), (p0, -1)):
        rows.append(('-(trig_piv_z + (%.3f mm) + (%.4f) * %s)' % (pz_, s * ny, wexp),
                     'trig_piv_y + (%.3f mm) + (%.4f) * %s' % (py_, s * nx, wexp)))
    return rows


def build_trigger_leg_slot(c, des, body):
    """Through-slot in the ear for the moving leg, half-width = trig_leg_slot (parametric)."""
    add_slot_cap_params(des)
    rest = des.userParameters.itemByName('trig_rest_deg').value
    _r = lambda p: (p[0] * math.cos(rest) - p[1] * math.sin(rest), p[0] * math.sin(rest) + p[1] * math.cos(rest))
    R_S, R_T, R_H = _r(SPR_S), _r(SPR_T), _r(SPR_H)
    dx, dy = R_T[0] - R_S[0], R_T[1] - R_S[1]; n = math.hypot(dx, dy); d = (dx / n, dy / n)
    pb = (R_S[0] + d[0] * 2.0, R_S[1] + d[1] * 2.0)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger leg slot'
    poly_pts(sk, des, _seg_strip_w(pb, R_T, 'trig_leg_slot', 0, 0.3))
    poly_pts(sk, des, _seg_strip_w(R_T, R_H, 'trig_leg_slot', 0.3, 0.3))
    ext(c, all_profiles(sk), 'trig_ear_x1 - trig_bx1 + 0.4 mm', FE.CutFeatureOperation, 'Trigger leg slot', start='trig_bx1 - 0.2 mm', target=body)


def build_spring_groove(c, des, body):
    """Fixed-leg groove under the rib, half-width trig_groove_w (parametric)."""
    add_slot_cap_params(des)
    sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Spring leg groove'
    rect4(sk, des, 'trig_fix_x - trig_groove_w', 'trig_fix_x + trig_groove_w', '-(trig_rib_z0 + 0.45 mm)', '-(trig_rib_z0 - 0.1 mm)')
    ext(c, all_profiles(sk), 'trig_rib_y1 - trig_rib_y0 + 0.2 mm', FE.CutFeatureOperation, 'Spring leg groove', start='trig_rib_y0 - 0.1 mm', target=body)


def build_rib_pillar(c, des, body):
    """Solid pillar under the spring-leg rib so a belly-down print needs no support there. Side profile (YZ) is
    buried 1.3 mm in the ramp floor; footprint stays clear of the spring (x 9.3+) and the right screw head well."""
    add_slot_cap_params(des)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Rib pillar'
    poly_pts(sk, des, [('-(trig_rib_z0 - 0.2 mm)', 'trig_rib_y0'), ('-(trig_rib_z0 - 0.2 mm)', 'trig_pil_y1'),
                       ('-(trig_pil_zb1)', 'trig_pil_y1'), ('-(trig_pil_zb0)', 'trig_rib_y0')])
    ext(c, all_profiles(sk), 'trig_pil_x1 - trig_pil_x0', FE.JoinFeatureOperation, 'Rib pillar', start='trig_pil_x0', target=body)


def build_post_caps(root, des):
    """Component 'TriggerCaps': a thin cap above each post end, filling the gap between post and PCB (0.1 mm each way).
    Friction nubs on the sides hold it in the socket. Printed flat, loose parts."""
    add_slot_cap_params(des)
    c = new_component(root, 'TriggerCaps')
    zt = '-(pcb_bot_z - trig_cap_gap)'                                   # sketch x_s = -z
    zb = '-(trig_piv_z + trig_post_d / 2 + trig_cap_gap)'
    y0, y1 = 'trig_piv_y - trig_cap_w / 2', 'trig_piv_y + trig_cap_w / 2'
    for nm, start, t in (('left', 'trig_sock_l + trig_cap_clr', 'trig_cap_t_l'),
                         ('right', 'trig_sock_r - trig_cap_t_r - trig_cap_clr', 'trig_cap_t_r')):
        sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Post cap %s' % nm
        rect4(sk, des, zt, zb, y0, y1)
        f = ext(c, all_profiles(sk), t, FE.NewBodyFeatureOperation, 'Post cap %s' % nm, start=start)
        body = f.bodies.item(0); body.name = 'PostCap_%s' % nm
        # friction nub: a short bar across the plate, protruding trig_cap_nub each side (axis along y)
        sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Post cap nub %s' % nm
        xa = '(%s) + 0.1 mm' % start; xb = '(%s) + (%s) - 0.1 mm' % (start, t)
        rect4(sk, des, xa, xb, '-(trig_piv_z + 2.4 mm)', '-(trig_piv_z + 1.8 mm)')      # XZ sketch (x, y_s = -z)
        ext(c, all_profiles(sk), 'trig_cap_w + 2 * trig_cap_nub', FE.JoinFeatureOperation, 'Post cap nub %s' % nm,
            start='trig_piv_y - trig_cap_w / 2 - trig_cap_nub', target=body)
    return c


# ---------------------------------------------------------------- pouch-cell variant (BottomShell_Pouch)
# 1100 mAh YDL 103035 pouch cell (10 x 30 x 37 mm incl. PCM), permanent: no battery panel, plain floor.
POUCH_PARAMS = [
    ('pouch_cell_t', '10 mm', 'YDL103035 thickness'),
    ('pouch_cell_w', '30 mm', 'YDL103035 width'),
    ('pouch_cell_l', '37 mm', 'YDL103035 length incl. protection board'),
    ('pouch_swell', '0.5 mm', 'thickness allowance for cell swelling'),
    ('pouch_clr', '0.3 mm', 'clearance between cell and PCB parts / floor'),
    ('pouch_top_z', '-0.82 mm - pouch_clr', 'cell top: lowest carrier part (z -0.82) minus clearance'),
    ('pouch_cav_r', '2 mm', 'cavity floor corner radius: small enough that the cell corners clear it, big enough to keep the belly corner wall above about 1.3 mm'),
    ('pouch_bot_d', '-pouch_top_z + pouch_cell_t + pouch_swell + pouch_clr + floor_t', 'pouch variant belly depth'),
    ('pouch_plug_gap', '4 mm', 'room in front of the PCB battery JST (front face y -28.85, opening faces +y) for the mating PHR-2 plug: 4 mm because the plug is mated with the shells apart and the cell wire has slack; ASSUMED plug ~6 x 4.5 x 6 mm, verify with the real plug'),
    ('pouch_y0', '-28.85 mm + pouch_plug_gap', 'cell rear face y: JST battery connector front face + plug room'),
    ('pouch_y1', 'pouch_y0 + pouch_cell_l', 'cell front face y'),
    ('pouch_stop_t', '1.2 mm', 'retention stop rib thickness'),
    ('pouch_stop_h', '5 mm', 'front stop rib height above the floor'),
    ('pouch_rear_h', '3 mm', 'rear stop height: low, so the cell wires (exit at mid-height) clear it'),
    ('pouch_stop_x', 'shell_w / 2 - wall + 0.2 mm', 'stop rib half length (0.2 mm into the side walls to fuse)'),
    ('pouch_wire_x0', '6 mm', 'rear stop ends here (+x side left open as the wire path to the JST connector at x 8.2..14.1)'),
]


def add_pouch_params(des):
    up = des.userParameters
    for n, e, c in POUCH_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def build_pouch_shell(root, des):
    """BottomShell_Pouch: like build_bottom_shell with depth pouch_bot_d and a tight cavity corner radius,
    then PCB columns, screw wells/bores and column reliefs. No battery bay."""
    add_pouch_params(des)
    _relief_param(des)
    d = 'pouch_bot_d'
    comp = new_component(root, 'BottomShell_Pouch')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan outer'
    plan(sk, '0 mm')
    f = ext(comp, main_profile(sk), d + ' - lap_h', FE.NewBodyFeatureOperation, 'Bottom outer', neg=True, start='-lap_h')
    body = f.bodies.item(0); body.name = 'BottomShell_Pouch'
    zb = -des.unitsManager.evaluateExpression(d, 'cm')
    fillet_face_edges(comp, planar_face_at(body, zb, -1), 'edge_r', 'Bottom edge fillet')
    cut_tool(comp, des, body, 'wall', d + ' - lap_h - floor_t', True, '-lap_h', '-(%s - floor_t)' % d, -1, 'pouch_cav_r', 'Cavity')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan seam rib ring'
    plan(sk, 'lip_w + seam_clr'); plan(sk, 'wall')
    ext(comp, profile_by_loops(sk, 2)[0], '2*lap_h - seam_clr', FE.JoinFeatureOperation, 'Seam rib', start='-lap_h')
    build_bottom_columns(comp, des, d)
    build_bottom_reliefs(comp, des)
    return comp


def build_pouch_stops(comp, des, body):
    """Cell retention: rear stop stub (open on +x for the wires to the battery JST) and a full-width front stop."""
    add_pouch_params(des)
    z0 = '-(pouch_bot_d - floor_t)'
    for nm, l, r, yb, yt, h in (
        ('Pouch rear stop', '-pouch_stop_x', 'pouch_wire_x0', 'pouch_y0 - pouch_clr - pouch_stop_t', 'pouch_y0 - pouch_clr', 'pouch_rear_h'),
        ('Pouch front stop', '-pouch_stop_x', 'pouch_stop_x', 'pouch_y1 + pouch_clr', 'pouch_y1 + pouch_clr + pouch_stop_t', 'pouch_stop_h'),
    ):
        sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = nm
        rect4(sk, des, l, r, yb, yt)
        ext(comp, main_profile(sk), h, FE.JoinFeatureOperation, nm, start=z0, target=body)


def build_pouch_cell_ref(root, des):
    add_pouch_params(des)
    comp = new_component(root, 'Cell_YDL103035_Ref')
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Cell'
    rect4(sk, des, '-pouch_cell_w / 2', 'pouch_cell_w / 2', 'pouch_y0', 'pouch_y1')
    f = ext(comp, main_profile(sk), 'pouch_cell_t', FE.NewBodyFeatureOperation, 'Cell', start='pouch_top_z - pouch_cell_t')
    f.bodies.item(0).name = 'Cell_YDL103035'
    return comp


def build_pouch_usb(comp, des, body):
    """Bottom half of the USB plug channel: bezel sliver below the seam gap (trimmed all the way back to the bezel start, so
    nothing is left above the seam inside the rear wall) and the closing port wall."""
    build_usb_bezel(comp, des, 'USB bottom', body, False)
    build_usb_port_wall(comp, des, 'Bottom', body)


def build_pouch_bumper(comp, des, body):
    """Bottom half of the bumper: flange pocket and stem slot in the front wall."""
    build_bumper_cuts(comp, des, 'Bottom', body)


# ---------------------------------------------------------------- pouch variant trigger (pt_*) ----
# Static pt_ copies of the standard trigger / scoop builders for BottomShell_Pouch (standard parts stay untouched).
# Pouch differences: scoop starts at y 14.4 on the shallower belly, side clearance 0.3, slot rear = cell stop front face,
# rear lobe trimmed (pt_lobe_ymin) so its sweep clears the stop.
PT_PARAMS = [
    ('pt_sc_y0', '14.55 mm', 'scoop curve fit point 0 (y): where the standard scoop curve crosses the shallower pouch belly'),
    ('pt_sc_z0', '-pouch_bot_d', 'scoop curve fit point 0 (z) = pouch belly'),
    ('pt_sc_y1', '15.37 mm', 'scoop curve fit point 1 (y): on the standard curve, keeps the pouch curve identical to it'),
    ('pt_sc_z1', '-13.555 mm', 'scoop curve fit point 1 (z)'),
    ('pt_sc_y2', '16.16 mm', 'scoop curve fit point 2 (y) (standard point 1)'),
    ('pt_sc_z2', '-12.99 mm', 'scoop curve fit point 2 (z)'),
    ('pt_sc_y3', '23.77 mm', 'scoop curve fit point 3 (y) (standard point 2)'),
    ('pt_sc_z3', '-7.76 mm', 'scoop curve fit point 3 (z)'),
    ('pt_sc_y4', '30.3 mm', 'scoop curve fit point 4 (y) - ends in the front wall (standard point 3)'),
    ('pt_sc_z4', '-5.31 mm', 'scoop curve fit point 4 (z)'),
    ('pt_floor_t', '2.2 mm', 'ramp floor thickness, normal to the scoop curve'),
    ('pt_ramp_y', 'y_front_out - wall', 'ramp floor stops at the front wall inner face'),
    ('pt_cut_y1', 'y_front_out + 1.1 mm', 'scoop cut closes past the front face'),
    ('pt_cut_zlow', '-pouch_bot_d - 1.5 mm', 'scoop cut bottom, below the belly'),
    ('pt_cut_x', 'shell_w / 2 + 1 mm', 'scoop cut half-width, past the side walls'),
]


def add_pt_params(des):
    up = des.userParameters
    for n, e, c in PT_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'mm', c)
        else:
            pr.expression = e


def build_pt_scoop(c, des, body):
    """Scoop the belly in front of the battery bay for the trigger: spline cut across the full width below the
    curve, plus a ramp floor strip (offset of the curve) up to the front wall. YZ sketch: (x_s, y_s) = (-z, y)."""
    add_pt_params(des)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger scoop'
    O = sk.originPoint; dm = sk.sketchDimensions; gc = sk.geometricConstraints
    Hz = adsk.fusion.DimensionOrientations.HorizontalDimensionOrientation
    Vt = adsk.fusion.DimensionOrientations.VerticalDimensionOrientation
    ev = lambda e: ev_cm(des, e)
    def S(y_e, z_e): return sk.modelToSketchSpace(P3(0, ev(y_e), ev(z_e)))
    names = [('pt_sc_y%d' % i, 'pt_sc_z%d' % i) for i in range(5)]
    oc = adsk.core.ObjectCollection.create()
    for yn, zn in names: oc.add(S(yn, zn))
    sp = sk.sketchCurves.sketchFittedSplines.add(oc)
    for i, fp in enumerate(sp.fitPoints):
        yn, zn = names[i]; g = fp.geometry
        dm.addDistanceDimension(O, fp, Hz, P3(g.x / 2, g.y + 0.3, 0)).parameter.expression = '-(%s)' % zn
        dm.addDistanceDimension(O, fp, Vt, P3(g.x + 0.3, g.y / 2, 0)).parameter.expression = yn
    # ramp floor curve: offset of the scoop curve, toward +z (negative distance on this side)
    inp = gc.createOffsetInput([sp], VI('-(pt_floor_t)'))
    oco = gc.addOffset2(inp)
    osp = list(oco.childCurves)[0]
    # cut closure: P3 -> Q1 -> Q2 -> Q3 -> P0
    L = sk.sketchCurves.sketchLines
    p0 = sp.startSketchPoint; p3 = sp.endSketchPoint
    q1g = S('pt_cut_y1', 'pt_sc_z4'); q2g = S('pt_cut_y1', 'pt_cut_zlow'); q3g = S('pt_sc_y0', 'pt_cut_zlow')
    l1 = L.addByTwoPoints(p3, q1g)
    l2 = L.addByTwoPoints(l1.endSketchPoint, q2g)
    l3 = L.addByTwoPoints(l2.endSketchPoint, q3g)
    l4 = L.addByTwoPoints(l3.endSketchPoint, p0)
    gc.addVertical(l1); gc.addHorizontal(l2); gc.addVertical(l3); gc.addHorizontal(l4)
    q1 = l1.endSketchPoint; q2 = l2.endSketchPoint
    dm.addDistanceDimension(O, q1, Vt, P3(q1.geometry.x + 0.3, q1.geometry.y / 2, 0)).parameter.expression = 'pt_cut_y1'
    dm.addDistanceDimension(O, q2, Hz, P3(q2.geometry.x / 2, q2.geometry.y + 0.3, 0)).parameter.expression = '-(pt_cut_zlow)'
    # start cap of the ramp floor strip
    cap = L.addByTwoPoints(p0, osp.startSketchPoint)
    # front stop line at the front wall inner face (constant y => horizontal in this sketch), bottom on l3, top on the offset curve
    vg0 = S('pt_ramp_y', 'pt_cut_zlow'); vg1 = S('pt_ramp_y', '-3 mm')
    v = L.addByTwoPoints(vg0, vg1)
    gc.addHorizontal(v)
    gc.addCoincident(v.startSketchPoint, l3)
    gc.addCoincident(v.endSketchPoint, osp)
    dm.addOffsetDimension(v, O, P3(v.startSketchPoint.geometry.x, v.startSketchPoint.geometry.y / 2, 0)).parameter.expression = 'pt_ramp_y'
    # classify profiles against the scoop curve
    import bisect
    evl = sp.geometry.evaluator; ok, s0, s1 = evl.getParameterExtents()
    pts = []
    for i in range(201):
        ok, pt = evl.getPointAtParameter(s0 + (s1 - s0) * i / 200.0); pts.append((pt.y, pt.x))
    pts.sort(); ys = [p[0] for p in pts]
    def xs_at(y):
        j = min(max(bisect.bisect_left(ys, y), 1), len(ys) - 1)
        (ya, xa), (yb, xb) = pts[j - 1], pts[j]
        return xa + (xb - xa) * (y - ya) / (yb - ya) if yb != ya else xa
    ramp_y = ev('pt_ramp_y')
    cut = adsk.core.ObjectCollection.create(); floor = adsk.core.ObjectCollection.create(); info = []
    for p in sk.profiles:
        cp = p.areaProperties().centroid; info.append((round(cp.x, 3), round(cp.y, 3), round(p.areaProperties().area, 4)))
        if cp.x > xs_at(cp.y): cut.add(p)         # larger x_s = lower z = below the curve
        elif cp.y < ramp_y: floor.add(p)
    if cut.count != 2 or floor.count != 1:
        raise RuntimeError('scoop profiles cut=%d floor=%d %s' % (cut.count, floor.count, info))
    f = ext(c, cut, '2 * pt_cut_x', FE.CutFeatureOperation, 'Trigger scoop cut', start='-pt_cut_x', target=body)
    ext(c, floor, 'shell_w - wall', FE.JoinFeatureOperation, 'Trigger ramp floor', start='-(shell_w - wall) / 2', target=body)
    return sk, sp, osp


PT_SLOT_PARAMS = [
    ('pt_x0', '-9.45 mm', 'trigger body left edge (x)'),
    ('pt_x1', '8.7 mm', 'trigger body right edge (x)'),
    ('pt_clr', '0.3 mm', 'trigger side clearance in the slot'),
    ('pt_slot_y0', 'pouch_y1 + pouch_clr + pouch_stop_t', 'trigger slot rear edge: front face of the cell front stop'),
    ('pt_lobe_ymin', 'pt_slot_y0 + 0.3 mm', 'rear lobe is trimmed so its swept path stays in front of this y at every pull angle'),
    ('pt_slot_ztop', '2.5 mm', 'slot cut top, past the shell top so no ceiling lip is left'),
]


def build_pt_slot(c, des, body):
    """Centre slot through the ramp floor for the trigger (also relieves the front screw columns), then
    re-cut the front screw head wells through the new ramp floor."""
    add_pt_params(des)
    up = des.userParameters
    for n, e, cm in PT_SLOT_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', cm)
        else: pr.expression = e
    sk = _xy_rects(c, des, 'Trigger slot', [('pt_x0 - pt_clr', 'pt_x1 + pt_clr', 'pt_slot_y0', 'pt_ramp_y')])
    ext(c, all_profiles(sk), 'pt_slot_ztop - pt_cut_zlow', FE.CutFeatureOperation, 'Trigger slot', start='pt_cut_zlow', target=body)
    sk = c.sketches.add(c.xYConstructionPlane); sk.name = 'Screw head wells (ramp)'
    circle_at(sk, des, 'mh1_x', 'mh1_y', 'head_well_d')
    circle_at(sk, des, 'mh2_x', 'mh2_y', 'head_well_d')
    ext(c, all_profiles(sk), 'pouch_bot_d + screw_head_seat_z', FE.CutFeatureOperation, 'Screw head wells (ramp)', start='-pouch_bot_d', target=body)
    # the left screw column leaves only ~0.3 mm between the head well and the slot: open the well into the slot (keyhole)
    sk = _xy_rects(c, des, 'Left well keyhole', [('mh1_x', 'pt_x0 - pt_clr', 'mh1_y - head_well_d / 2', 'mh1_y + head_well_d / 2')])
    ext(c, all_profiles(sk), 'pouch_bot_d + screw_head_seat_z', FE.CutFeatureOperation, 'Left well keyhole', start='-pouch_bot_d', target=body)


# ---------------------------------------------------------------- pivot trigger (v2) ----
# v1.1 blade side profile (y, z mm) shifted to the v2 pivot (26.3, -3.0): rest pose, before the nose / tip trims
PT_PROFILE = [(21.6, -17.06), (23.19, -18.4), (24.4, -19.12), (25.54, -19.36), (25.91, -19.24), (26.15, -18.95), (26.19, -18.29),
                (25.68, -15.85), (25.51, -14.25), (25.52, -12.55), (25.84, -10.27), (26.46, -8.14), (27.27, -6.38), (27.7, -6.3),
                (28.9, -4.6), (28.9, -4.4), (29.3, -4.4), (29.3, -3.4), (28.9, -3.4), (28.9, -3.0), (28.1, -1.1), (26.3, -0.4),
                (24.5, -1.1), (23.7, -3.0), (23.85, -6.04), (18.5, -7.73), (18.5, -7.5), (16.8, -7.5), (16.8, -8.28), (15.33, -8.73),
                (14.45, -9.22), (14.16, -9.55), (14.22, -9.84), (14.49, -10.13), (15.43, -10.4), (16.15, -10.92), (18.26, -13.56)]

PT2_PARAMS = [
    ('pt_piv_y', '25.3 mm', 'trigger pivot axis y (axis runs along x); 1.0 mm behind the v1.1-derived 26.3 so the hub rim (r 2.6) clears the front wall / bumper at every pull angle'),
    ('pt_piv_z', '-3.0 mm', 'trigger pivot axis z'),
    ('pt_rest_deg', '8 deg', 'rest pose is rotated this far forward of the standard rest pose (more finger stroke); the pull end stays where it was'),
    ('pt_pull_deg', '28 deg + pt_rest_deg', 'total trigger rotation from rest to full pull (28 deg as the standard trigger, plus the forward rest offset)'),
    ('pt_x0', '-9.45 mm', 'trigger body left edge (x); slot / spring envelope is pt_x0..pt_x1'),
    ('pt_bx1', '7.6 mm', 'trigger blade right edge (x); coil sits between here and the slot wall'),
    ('pt_nose_y', 'pt_ramp_y - 0.4 mm', 'trigger front trim: 0.4 mm behind the front wall inner face'),
    ('pt_tip_z', '-pouch_bot_d + 0.3 mm', 'trigger tip trim: 0.3 mm above the belly'),
    ('pt_flat_len', '3.2 mm', 'length (y) of the flat bottom face at pt_tip_z (was 6.3 mm before the chamfer); the blade underside behind it is chamfered up to the lobe'),
    ('pt_post_d', '2.9 mm', 'pivot post diameter (D shape, as v1.1)'),
    ('pt_post_flat', '0.55 mm', 'pivot post flat depth, on the +z side'),
    ('pt_bore_clr', '0.15 mm', 'hub bore clearance per side around the post'),
    ('pt_sock_l', '-10.35 mm', 'left post socket end (x), 0.5 mm into the left column wall'),
    ('pt_sock_r', '9.8 mm', 'right post socket end (x), 0.7 mm into the right column wall'),
    ('pt_sock_clr', '0.15 mm', 'socket clearance per side around the post'),
    ('pt_post_x0', 'pt_sock_l + 0.15 mm', 'post left end'),
    ('pt_post_x1', 'pt_sock_r - 0.15 mm', 'post right end'),
    ('pt_hall_y', '23.25 mm', 'hall sensor y (U_HALL1)'),
    ('pt_hall_z', '0.91 mm', 'hall package bottom face z'),
    ('pt_hall_gap', '1.1 mm', 'magnet top face to hall package gap at full pull'),
    ('pt_mag_x', '-8.0 mm', 'magnet centre x (hall is at -8.5; nudged so the pocket wall clears the slot)'),
    ('pt_mag_d', '2.0 mm', 'magnet diameter (2 x 2 mm N52 cylinder)'),
    ('pt_mag_t', '2.0 mm', 'magnet thickness'),
    ('pt_mag_clr', '0.1 mm', 'magnet pocket clearance (diameter)'),
    ('pt_mag_wall', '0.4 mm', 'pocket wall'),
    ('pt_mag_floor', '1.0 mm', 'pocket floor thickness'),
    ('pt_mag_ty', 'pt_piv_y + (pt_hall_y - pt_piv_y) * cos(pt_pull_deg) - (pt_hall_z - pt_hall_gap - pt_piv_z) * sin(pt_pull_deg)',
     'magnet top face y in the rest pose (hall point rotated back by the pull angle about the pivot)'),
    ('pt_mag_tz', 'pt_piv_z + (pt_hall_y - pt_piv_y) * sin(pt_pull_deg) + (pt_hall_z - pt_hall_gap - pt_piv_z) * cos(pt_pull_deg)',
     'magnet top face z in the rest pose'),
    ('pt_sw_x', '2.7 mm', 'hub notch half-width for the bumper switch (switch body +-2.3)'),
    ('pt_sw_floor_z', 'pt_piv_z', 'hub notch floor z: everything above the pivot level is removed under the switch (it sweeps up with the pull)'),
]


def add_pt2_params(des):
    add_pt_params(des)
    up = des.userParameters
    for n, e, c in PT2_PARAMS:
        pr = up.itemByName(n)
        if pr is None:
            up.add(n, VI(e), 'deg' if n in ('pt_rest_deg', 'pt_pull_deg') else 'mm', c)
        else:
            pr.expression = e
    for n, e in (('pt_x0', '-9.45 mm'),):
        pass


def _pt_d_profile(c, des, name, dia_e, flat_e):
    """YZ sketch: circle of diameter dia_e about the pivot with a flat chord on the +z side, flat_e from the centre.
    Returns (sketch, D profile)."""
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = name
    circ = circle_at(sk, des, '-(pt_piv_z)', 'pt_piv_y', dia_e)
    cx = circ.centerSketchPoint.geometry.x
    L = sk.sketchCurves.sketchLines; dm = sk.sketchDimensions; O = sk.originPoint
    xc = cx - ev_cm(des, flat_e)
    ln = L.addByTwoPoints(P3(xc, ev_cm(des, 'pt_piv_y - 3 mm'), 0), P3(xc, ev_cm(des, 'pt_piv_y + 3 mm'), 0))
    sk.geometricConstraints.addVertical(ln)
    HO = adsk.fusion.DimensionOrientations
    dm.addDistanceDimension(circ.centerSketchPoint, ln.startSketchPoint, HO.HorizontalDimensionOrientation, P3(xc, ev_cm(des, 'pt_piv_y') - 0.5, 0)).parameter.expression = flat_e
    dm.addDistanceDimension(O, ln.startSketchPoint, HO.VerticalDimensionOrientation, P3(xc + 1, ev_cm(des, 'pt_piv_y - 3 mm') / 2, 0)).parameter.expression = 'pt_piv_y - 3 mm'
    dm.addDistanceDimension(O, ln.endSketchPoint, HO.VerticalDimensionOrientation, P3(xc + 1, ev_cm(des, 'pt_piv_y + 3 mm') / 2, 0)).parameter.expression = 'pt_piv_y + 3 mm'
    return sk, max(sk.profiles, key=lambda p: p.areaProperties().area)


def build_pt_trigger(root, des):
    """Blade body (v1.1 profile, trimmed), D pivot bore, magnet tab + pocket, bumper-switch notch. Rest pose."""
    add_pt2_params(des)
    c = new_component(root, 'Trigger_Pouch')
    ev = lambda e: ev_cm(des, e) * 10.0
    py, pz = ev('pt_piv_y'), ev('pt_piv_z')
    nose, tip = ev('pt_nose_y'), ev('pt_tip_z')
    pts = [(py + (y - 26.3), pz + (z + 3.0)) for y, z in PT_PROFILE]      # profile was extracted about the (26.3, -3.0) pivot
    rest = des.userParameters.itemByName('pt_rest_deg').value       # radians
    pull = int(math.ceil(math.degrees(des.userParameters.itemByName('pt_pull_deg').value)))
    def _fwd(p):    # rotate (y, z) forward about the pivot by the rest offset
        dy, dz = p[0] - py, p[1] - pz
        return (py + dy * math.cos(rest) - dz * math.sin(rest), pz + dy * math.sin(rest) + dz * math.cos(rest))
    pts = [_fwd(p) for p in pts]
    pts = _clip(pts, lambda p: nose - p[0])
    pts = _clip(pts, lambda p: p[1] - tip)
    ymin = ev('pt_lobe_ymin')
    for a in range(0, pull + 1):
        th = -math.radians(a)
        pts = _clip(pts, lambda p, th=th: (py + (p[0] - py) * math.cos(th) - (p[1] - pz) * math.sin(th)) - ymin)
    # shorten the flat belly-side face: 45-degree-ish chamfer from pt_flat_len behind the pad front up to the lobe's lower point
    yf = max(p[0] for p in pts if abs(p[1] - tip) < 1e-6)
    A = (yf - ev('pt_flat_len'), tip); B = _fwd((py + (16.8 - 26.3), pz + (-8.28 + 3.0)))
    cr = lambda p: (B[0] - A[0]) * (p[1] - A[1]) - (B[1] - A[1]) * (p[0] - A[0])
    sg = 1 if cr((py, pz)) >= 0 else -1
    pts = _clip(pts, lambda p: sg * cr(p))
    rows = []
    for y, z in pts:
        ye = 'pt_nose_y' if abs(y - nose) < 1e-6 else '(pt_piv_y + (%.3f mm))' % (y - py)
        ze = 'pt_tip_z' if abs(z - tip) < 1e-6 else '(pt_piv_z + (%.3f mm))' % (z - pz)
        rows.append(('-(%s)' % ze, ye))        # sketch (x_s, y_s) = (-z, y)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger blade profile'
    poly_pts(sk, des, rows)
    ext(c, main_profile(sk), 'pt_bx1 - pt_x0', FE.NewBodyFeatureOperation, 'Trigger blade', start='pt_x0')
    body = c.bRepBodies.item(0); body.name = 'Trigger_Pouch'
    # magnet tab behind the hub (rest pose), carries the pocket under the hall point
    tab_top = 'pt_mag_tz'
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger magnet tab'
    rect4(sk, des, '-(pt_mag_tz)', '-(pt_mag_tz - pt_mag_t - pt_mag_floor - 0.1 mm)',
          'pt_mag_ty - pt_mag_d / 2 - pt_mag_clr / 2 - pt_mag_wall', 'pt_piv_y - 2 mm')
    ext(c, all_profiles(sk), 'pt_mag_x + pt_mag_d / 2 + pt_mag_clr / 2 + pt_mag_wall - pt_x0', FE.JoinFeatureOperation,
        'Trigger magnet tab', start='pt_x0', target=body)
    sk = c.sketches.add(c.xYConstructionPlane); sk.name = 'Trigger magnet pocket'
    circle_at(sk, des, 'pt_mag_x', 'pt_mag_ty', 'pt_mag_d + pt_mag_clr')
    ext(c, all_profiles(sk), 'pt_mag_t + 0.1 mm', FE.CutFeatureOperation, 'Trigger magnet pocket', neg=True, start='pt_mag_tz', target=body)
    # D bore for the pivot post
    sk, prof = _pt_d_profile(c, des, 'Trigger D bore', 'pt_post_d + 2 * pt_bore_clr', 'pt_post_d / 2 - pt_post_flat + pt_bore_clr')
    ext(c, prof, 'pt_bx1 - pt_x0 + 2 mm', FE.CutFeatureOperation, 'Trigger D bore', start='pt_x0 - 1 mm', target=body)
    # hub notch under the bumper switch
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger switch notch'
    rect4(sk, des, '-(1 mm)', '-(pt_sw_floor_z)', 'pt_piv_y - 3.6 mm', 'pt_piv_y + 3.6 mm')
    ext(c, all_profiles(sk), '2 * pt_sw_x', FE.CutFeatureOperation, 'Trigger switch notch', start='-pt_sw_x', target=body)
    return c


def build_pt_post(root, des):
    add_pt2_params(des)
    c = new_component(root, 'TriggerPost_Pouch')
    sk, prof = _pt_d_profile(c, des, 'Trigger post D', 'pt_post_d', 'pt_post_d / 2 - pt_post_flat')
    ext(c, prof, 'pt_post_x1 - pt_post_x0', FE.NewBodyFeatureOperation, 'Trigger post', start='pt_post_x0')
    c.bRepBodies.item(0).name = 'TriggerPost_Pouch'
    return c


def build_pt_sockets(c, des, body):
    """U-slots (open to the top) in the left / right column walls for the pivot post ends."""
    add_pt2_params(des)
    for side, x0, x1, nm in (('L', 'pt_sock_l', '-9 mm', 'left'), ('R', '9 mm', 'pt_sock_r', 'right')):
        sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger socket ' + nm
        circle_at(sk, des, '-(pt_piv_z)', 'pt_piv_y', 'pt_post_d + 2 * pt_sock_clr')
        rect4(sk, des, '-(pt_slot_ztop)', '-(pt_piv_z)', 'pt_piv_y - pt_post_d / 2 - pt_sock_clr', 'pt_piv_y + pt_post_d / 2 + pt_sock_clr')
        ext(c, all_profiles(sk), '(%s) - (%s)' % (x1, x0), FE.CutFeatureOperation, 'Trigger socket ' + nm, start=x0, target=body)


# ---------------------------------------------------------------- torsion spring mount (v2) ----
# spring leg geometry relative to the pivot (y, z mm), from the v1.1 model (trigger-torsion-spring.md)
PT_SPR_S = (0.85, -1.81)      # moving leg leaves the coil here
PT_SPR_T = (-5.04, -4.55)     # moving leg tip
PT_SPR_H = (-5.67, -3.19)     # moving leg hook end
PT_SPR_SHIFT = (19.45, -15.9, -1.4)   # v1.1 -> v2 translation of SpringRef (x, y, z mm): coil centre x -11.1 -> 8.35

PT_SPRING_PARAMS = [
    ('pt_ear_x1', '8.3 mm', 'spring ear outer face (x)'),
    ('pt_ear_w', '1.2 mm', 'spring ear half-width around the moving leg'),
    ('pt_leg_slot', '0.3 mm', 'moving leg slot half-width (wire 0.35 + 0.25 clearance)'),
    ('pt_fix_x', '8.7 mm', 'fixed leg x'),
    ('pt_fix_z', 'pt_piv_z + 2 mm', 'fixed leg z (top of the coil)'),
    ('pt_fix_y1', 'pt_piv_y - 6.5 mm', 'fixed leg tip y'),
    ('pt_rib_x0', '8.1 mm', 'fixed leg rib: left edge'),
    ('pt_rib_x1', '11.5 mm', 'fixed leg rib: right edge (anchors into the right screw column)'),
    ('pt_rib_y0', 'pt_fix_y1 + 0.2 mm', 'fixed leg rib: rear edge'),
    ('pt_rib_y1', '23.3 mm', 'fixed leg rib: front edge'),
    ('pt_rib_z0', 'pt_fix_z + 0.15 mm', 'fixed leg rib: underside, bears on the wire top'),
    ('pt_rib_top', 'pcb_bot_z', 'fixed leg rib: top (PCB underside)'),
]


def add_pt_spring_params(des):
    add_pt2_params(des)
    up = des.userParameters
    for n, e, c in PT_SPRING_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', c)
        else: pr.expression = e


def place_pt_spring_UNUSED(root):
    """Move the v1.1 spring reference to the v2 pivot (MoveFeature on its body, sticky in the timeline)."""
    occ = root.occurrences.itemByName('SpringRef_v1.1:1')
    m = [[1, 0, 0, PT_SPR_SHIFT[0] / 10.0], [0, 1, 0, PT_SPR_SHIFT[1] / 10.0], [0, 0, 1, PT_SPR_SHIFT[2] / 10.0]]
    body_occ = occ.childOccurrences.item(0) if occ.childOccurrences.count else occ
    f = _move_occ(root, body_occ, m)
    f.name = 'Place spring (v2 pivot)'
    return occ


def _pt_seg_strip(a, b, w, l0=0.0, l1=0.0):
    """Rectangle around segment a->b (extended by l0 / l1 at the ends), half-width w. Points (dy, dz) rel pivot."""
    dx, dy = b[0] - a[0], b[1] - a[1]; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    nx, ny = -dy, dx
    p0 = (a[0] - dx * l0, a[1] - dy * l0); p1 = (b[0] + dx * l1, b[1] + dy * l1)
    return [(p0[0] + nx * w, p0[1] + ny * w), (p1[0] + nx * w, p1[1] + ny * w), (p1[0] - nx * w, p1[1] - ny * w), (p0[0] - nx * w, p0[1] - ny * w)]


def _pt_rel_rows(pts):
    return [('-(pt_piv_z + (%.3f mm))' % z, 'pt_piv_y + (%.3f mm)' % y) for y, z in pts]


def build_pt_ear(c, des, body):
    """Ear on the blade's right face carrying the spring's moving leg: through-slot along the leg + hook pocket."""
    add_pt_spring_params(des)
    rest = des.userParameters.itemByName('pt_rest_deg').value
    _r = lambda p: (p[0] * math.cos(rest) - p[1] * math.sin(rest), p[0] * math.sin(rest) + p[1] * math.cos(rest))
    SPR_S, SPR_T, SPR_H = _r(PT_SPR_S), _r(PT_SPR_T), _r(PT_SPR_H)
    dx, dy = SPR_T[0] - SPR_S[0], SPR_T[1] - SPR_S[1]; n = math.hypot(dx, dy); d = (dx / n, dy / n)
    pa = (SPR_S[0] + d[0] * 2.6, SPR_S[1] + d[1] * 2.6)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger spring ear'
    poly_pts(sk, des, _pt_rel_rows(_pt_seg_strip(pa, SPR_T, 1.2, 0, 0.7)))
    poly_pts(sk, des, _pt_rel_rows(_pt_seg_strip(SPR_T, SPR_H, 1.0, 0.9, 0.5)))
    ext(c, all_profiles(sk), 'pt_ear_x1 - pt_bx1', FE.JoinFeatureOperation, 'Trigger spring ear', start='pt_bx1', target=body)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger leg slot'
    pb = (SPR_S[0] + d[0] * 2.0, SPR_S[1] + d[1] * 2.0)
    poly_pts(sk, des, _pt_rel_rows(_pt_seg_strip(pb, SPR_T, 0.3, 0, 0.3)))
    poly_pts(sk, des, _pt_rel_rows(_pt_seg_strip(SPR_T, SPR_H, 0.3, 0.3, 0.3)))
    ext(c, all_profiles(sk), 'pt_ear_x1 - pt_bx1 + 0.4 mm', FE.CutFeatureOperation, 'Trigger leg slot', start='pt_bx1 - 0.2 mm', target=body)


def build_pt_spring_rib(c, des, body):
    """Overhang under the PCB that holds the spring's fixed leg (wire bears up into a 0.6 mm groove)."""
    add_pt_spring_params(des)
    sk = _xy_rects(c, des, 'Spring leg rib', [('pt_rib_x0', 'pt_rib_x1', 'pt_rib_y0', 'pt_rib_y1')])
    ext(c, all_profiles(sk), 'pt_rib_top - pt_rib_z0', FE.JoinFeatureOperation, 'Spring leg rib', start='pt_rib_z0', target=body)


def build_pt_ramp_trim(comp, body):
    """Trim the ramp-floor strip's lateral ends to the true outer envelope (plan outline + edge_r bottom fillet).
    Envelope tool -> cut out of an oversized box -> cut the remainder from the shell."""
    des = adsk.fusion.Design.cast(adsk.core.Application.get().activeProduct)
    FEo = adsk.fusion.FeatureOperations
    sk = comp.sketches.add(comp.xYConstructionPlane); sk.name = 'Plan outer envelope'
    plan(sk, '0 mm')
    f = ext(comp, main_profile(sk), 'pouch_bot_d - lap_h', FEo.NewBodyFeatureOperation, 'Outer envelope', neg=True, start='-lap_h')
    env = f.bodies.item(0); env.name = 'Outer envelope'
    zb = -des.unitsManager.evaluateExpression('pouch_bot_d', 'cm')
    fillet_face_edges(comp, planar_face_at(env, zb, -1), 'edge_r', 'Outer envelope fillet')
    sk2 = comp.sketches.add(comp.xYConstructionPlane); sk2.name = 'Plan trim box'
    plan(sk2, '-2 mm')
    f2 = ext(comp, main_profile(sk2), 'edge_r + 2 mm', FEo.NewBodyFeatureOperation, 'Trim box', neg=True, start='-(pouch_bot_d - edge_r)')
    box = f2.bodies.item(0); box.name = 'Trim box'
    for tgt, tool, nm in ((box, env, 'Trim tool (box minus envelope)'), (body, box, 'Trim ramp strip to shell fillet')):
        t = adsk.core.ObjectCollection.create(); t.add(tool)
        ci = comp.features.combineFeatures.createInput(tgt, t)
        ci.operation = FEo.CutFeatureOperation; ci.isKeepToolBodies = False
        cf = comp.features.combineFeatures.add(ci); cf.name = nm
    return body


def build_pt_rim_fillets(comp, des):
    """Round the trigger scoop rim: pt_rim_r (3 mm) on the rear lip + side rims where the finger rolls over,
    pt_lip_r (0.8 mm) on the front lip. Edges are found by geometry (NURBS scoop face vs outer face)."""
    up = des.userParameters
    for n, v, c in (('pt_rim_r', '2 mm', 'Trigger scoop rim fillet (rear lip + side rims); 3 mm (standard) does not compute on the shallow pouch belly, 2.2 is the limit'),
                    ('pt_lip_r', '0.8 mm', 'Trigger scoop front lip fillet')):
        if not up.itemByName(n):
            up.add(n, adsk.core.ValueInput.createByString(v), 'mm', c)
    body = comp.bRepBodies.item(0)
    ev = lambda e: des.unitsManager.evaluateExpression(e, 'mm') * 10   # evaluateExpression returns cm
    ymin, yfr = ev('pt_sc_y0'), ev('pt_sc_y4')
    def bb(e):
        b = e.boundingBox
        return [v * 10 for v in (b.minPoint.x, b.maxPoint.x, b.minPoint.y, b.maxPoint.y, b.minPoint.z, b.maxPoint.z)]
    big = adsk.core.ObjectCollection.create()
    for e in body.edges:
        x0, x1, y0, y1, z0, z1 = bb(e)
        st = sorted(f.geometry.surfaceType for f in e.faces)
        if 7 in st and y0 >= ymin - 0.05 and y1 <= 27.25 and z1 < -5.0 and e.length * 10 > 3 and st != [7, 7] and (
                (st == [0, 7] and abs(y0 - ymin) < 0.05 and abs(y1 - ymin) < 0.05) or st == [1, 7] and y1 < 27.25 or
                (st == [0, 7] and abs(abs(x0) - 17.5) < 0.05 and abs(x1 - x0) < 0.05)):
            big.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(big, adsk.core.ValueInput.createByString('pt_rim_r'), False)
    f1 = comp.features.filletFeatures.add(fi); f1.name = 'Trigger rim fillet (roll-over)'
    body = comp.bRepBodies.item(0)
    lip = adsk.core.ObjectCollection.create()
    for e in body.edges:
        x0, x1, y0, y1, z0, z1 = bb(e)
        if sorted(f.geometry.surfaceType for f in e.faces) == [0, 7] and abs(y1 - yfr) < 0.06 and abs(z1 - ev('pt_sc_z4')) < 0.06 and (x1 - x0) > 2:
            lip.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(lip, adsk.core.ValueInput.createByString('pt_lip_r'), True)
    f2 = comp.features.filletFeatures.add(fi); f2.name = 'Trigger front lip fillet'
    return f1, f2
# NOTE: the rim-edge selection above is a best-effort reconstruction of what was applied interactively
# (edges: rear lip y=pt_sc_y0, belly-corner arcs, side-wall rims x=+-17.5 up to y 27.2); verify on a rebuild.


# ------------------------------------------------- slot widening, rib pillar, post caps (2026-11-03) ----
PT_SLOT_CAP_PARAMS = [
    ('pt_leg_slot', '0.4 mm', 'moving leg slot half-width (0.8 mm = 2 perimeters at a 0.4 nozzle)'),
    ('pt_groove_w', '0.4 mm', 'fixed leg groove half-width under the rib (0.8 mm total)'),
    ('pt_rib_x0', '7.9 mm', 'fixed leg rib: left edge (leaves a 0.4 mm wall beside the 0.8 groove)'),
    ('pt_pil_x0', '9.3 mm', 'rib pillar: left edge (0.2 mm clear of the groove / wire)'),
    ('pt_pil_x1', 'pt_rib_x1', 'rib pillar: right edge'),
    ('pt_pil_y1', '22.7 mm', 'rib pillar: front edge (keeps 0.3 mm clear of the right screw head well)'),
    ('pt_pil_zb0', '-9.6 mm', 'rib pillar bottom z at y = pt_rib_y0 (buried 1.3 mm in the ramp floor)'),
    ('pt_pil_zb1', '-7.1 mm', 'rib pillar bottom z at y = pt_pil_y1 (buried 1.3 mm in the ramp floor)'),
    ('pt_cap_clr', '0.05 mm', 'post cap: clearance to the socket end'),
    ('pt_cap_t_l', '0.45 mm', 'post cap thickness, left socket'),
    ('pt_cap_t_r', '0.6 mm', 'post cap thickness, right socket'),
    ('pt_cap_w', '3.0 mm', 'post cap width (socket is pt_post_d + 2 * pt_sock_clr = 3.2)'),
    ('pt_cap_nub', '0.1 mm', 'post cap: friction nub protrusion each side'),
    ('pt_cap_gap', '0.1 mm', 'post cap: clearance above the post and below the PCB'),
]


def add_pt_slot_cap_params(des):
    add_pt_spring_params(des)
    up = des.userParameters
    for n, e, c in PT_SLOT_CAP_PARAMS:
        pr = up.itemByName(n)
        if pr is None: up.add(n, VI(e), 'mm', c)
        else: pr.expression = e


def _pt_seg_strip_w(a, b, wexp, l0=0.0, l1=0.0):
    """Like _seg_strip but the half-width is the parameter expression wexp. Returns (z_expr, y_expr) rows."""
    dx, dy = b[0] - a[0], b[1] - a[1]; n = math.hypot(dx, dy); dx, dy = dx / n, dy / n
    nx, ny = -dy, dx
    p0 = (a[0] - dx * l0, a[1] - dy * l0); p1 = (b[0] + dx * l1, b[1] + dy * l1)
    rows = []
    for (py_, pz_), s in ((p0, 1), (p1, 1), (p1, -1), (p0, -1)):
        rows.append(('-(pt_piv_z + (%.3f mm) + (%.4f) * %s)' % (pz_, s * ny, wexp),
                     'pt_piv_y + (%.3f mm) + (%.4f) * %s' % (py_, s * nx, wexp)))
    return rows


def build_pt_leg_slot(c, des, body):
    """Through-slot in the ear for the moving leg, half-width = pt_leg_slot (parametric)."""
    add_pt_slot_cap_params(des)
    rest = des.userParameters.itemByName('pt_rest_deg').value
    _r = lambda p: (p[0] * math.cos(rest) - p[1] * math.sin(rest), p[0] * math.sin(rest) + p[1] * math.cos(rest))
    SPR_S, SPR_T, SPR_H = _r(PT_SPR_S), _r(PT_SPR_T), _r(PT_SPR_H)
    dx, dy = SPR_T[0] - SPR_S[0], SPR_T[1] - SPR_S[1]; n = math.hypot(dx, dy); d = (dx / n, dy / n)
    pb = (SPR_S[0] + d[0] * 2.0, SPR_S[1] + d[1] * 2.0)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Trigger leg slot'
    poly_pts(sk, des, _pt_seg_strip_w(pb, SPR_T, 'pt_leg_slot', 0, 0.3))
    poly_pts(sk, des, _pt_seg_strip_w(SPR_T, SPR_H, 'pt_leg_slot', 0.3, 0.3))
    ext(c, all_profiles(sk), 'pt_ear_x1 - pt_bx1 + 0.4 mm', FE.CutFeatureOperation, 'Trigger leg slot', start='pt_bx1 - 0.2 mm', target=body)


def build_pt_spring_groove(c, des, body):
    """Fixed-leg groove under the rib, half-width pt_groove_w (parametric)."""
    add_pt_slot_cap_params(des)
    sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Spring leg groove'
    rect4(sk, des, 'pt_fix_x - pt_groove_w', 'pt_fix_x + pt_groove_w', '-(pt_rib_z0 + 0.45 mm)', '-(pt_rib_z0 - 0.1 mm)')
    ext(c, all_profiles(sk), 'pt_rib_y1 - pt_rib_y0 + 0.2 mm', FE.CutFeatureOperation, 'Spring leg groove', start='pt_rib_y0 - 0.1 mm', target=body)


def build_pt_rib_pillar(c, des, body):
    """Solid pillar under the spring-leg rib so a belly-down print needs no support there. Side profile (YZ) is
    buried 1.3 mm in the ramp floor; footprint stays clear of the spring (x 9.3+) and the right screw head well."""
    add_pt_slot_cap_params(des)
    sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Rib pillar'
    poly_pts(sk, des, [('-(pt_rib_z0 - 0.2 mm)', 'pt_rib_y0'), ('-(pt_rib_z0 - 0.2 mm)', 'pt_pil_y1'),
                       ('-(pt_pil_zb1)', 'pt_pil_y1'), ('-(pt_pil_zb0)', 'pt_rib_y0')])
    ext(c, all_profiles(sk), 'pt_pil_x1 - pt_pil_x0', FE.JoinFeatureOperation, 'Rib pillar', start='pt_pil_x0', target=body)


def build_pt_post_caps(root, des):
    """Component 'TriggerCaps': a thin cap above each post end, filling the gap between post and PCB (0.1 mm each way).
    Friction nubs on the sides hold it in the socket. Printed flat, loose parts."""
    add_pt_slot_cap_params(des)
    c = new_component(root, 'TriggerCaps_Pouch')
    zt = '-(pcb_bot_z - pt_cap_gap)'                                   # sketch x_s = -z
    zb = '-(pt_piv_z + pt_post_d / 2 + pt_cap_gap)'
    y0, y1 = 'pt_piv_y - pt_cap_w / 2', 'pt_piv_y + pt_cap_w / 2'
    for nm, start, t in (('left', 'pt_sock_l + pt_cap_clr', 'pt_cap_t_l'),
                         ('right', 'pt_sock_r - pt_cap_t_r - pt_cap_clr', 'pt_cap_t_r')):
        sk = c.sketches.add(c.yZConstructionPlane); sk.name = 'Post cap %s' % nm
        rect4(sk, des, zt, zb, y0, y1)
        f = ext(c, all_profiles(sk), t, FE.NewBodyFeatureOperation, 'Post cap %s' % nm, start=start)
        body = f.bodies.item(0); body.name = 'PostCap_%s' % nm
        # friction nub: a short bar across the plate, protruding pt_cap_nub each side (axis along y)
        sk = c.sketches.add(c.xZConstructionPlane); sk.name = 'Post cap nub %s' % nm
        xa = '(%s) + 0.1 mm' % start; xb = '(%s) + (%s) - 0.1 mm' % (start, t)
        rect4(sk, des, xa, xb, '-(pt_piv_z + 2.4 mm)', '-(pt_piv_z + 1.8 mm)')      # XZ sketch (x, y_s = -z)
        ext(c, all_profiles(sk), 'pt_cap_w + 2 * pt_cap_nub', FE.JoinFeatureOperation, 'Post cap nub %s' % nm,
            start='pt_piv_y - pt_cap_w / 2 - pt_cap_nub', target=body)
    return c





def build_pt_front_rounding(comp, des):
    """pt_front_r (1.5 mm) round on the outer front lower edge of the scoop's front end (flat front edge + the two corner arcs),
    on top of the lip fillet. The standard shell carries it on a longer chain; the pouch scoop's flat is only the 0.1 mm sliver."""
    up = des.userParameters
    if not up.itemByName('pt_front_r'):
        up.add('pt_front_r', adsk.core.ValueInput.createByString('1.5 mm'), 'mm', 'Trigger scoop front rounding fillet (on top of pt_lip_r)')
    ev = lambda e: des.unitsManager.evaluateExpression(e, 'mm') * 10
    z4, y4 = ev('pt_sc_z4'), ev('pt_sc_y4'); es = adsk.core.ObjectCollection.create()
    for e in comp.bRepBodies.item(0).edges:
        b = e.boundingBox; st = sorted(g.geometry.surfaceType for g in e.faces)
        z0, z1, y0 = b.minPoint.z * 10, b.maxPoint.z * 10, b.minPoint.y * 10
        if st in ([0, 0], [0, 1]) and abs(z0 - z4) < 0.02 and abs(z1 - z4) < 0.02 and y0 > y4 - 0.05:
            es.add(e)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(es, adsk.core.ValueInput.createByString('pt_front_r'), False)
    f = comp.features.filletFeatures.add(fi); f.name = 'Trigger front rounding fillet'
    return f


# ---------------------------------------------------------------- wrist strap anchor (both bottom shells)
# Two vertical slots side by side in the left rear corner (-x), a bar of wall between them. The loop goes in one slot and out
# of the other, wrapping the bar. Placed in the free corner pocket: the centre of the rear wall is taken by the contact clip /
# 14500 bay (standard shell). The slots run through the corner mass (cut to strap_cut_y) to open into the interior.
STRAP_PARAMS = [
    ('strap_loop_w', '3 mm', 'Wrist strap attachment loop width (flat webbing), runs along z through the slot'),
    ('strap_loop_t', '1 mm', 'Wrist strap attachment loop thickness'),
    ('strap_clr', '0.4 mm', 'Clearance added to the loop width in the slot'),
    ('strap_slot_h', 'strap_loop_w + strap_clr', 'Strap slot height (z)'),
    ('strap_slot_w', 'strap_loop_t + 0.5 mm', 'Strap slot width (x)'),
    ('strap_bar_w', '2.2 mm', 'Bar of wall between the two slots (x), the load-bearing bar the loop wraps'),
    ('strap_x', '-12.4 mm', 'x of the centre of the slot pair (left rear corner pocket)'),
    ('strap_z', '-6 mm', 'z of the slot centres (below the thin lap section, above the belly fillet)'),
    ('strap_cut_y', '-45.5 mm', 'y the slot cut runs to (through the corner mass into the interior)'),
]


def add_strap_params(des):
    up = des.userParameters
    for n, e, c in STRAP_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def _strap_slot(sk, des, xc_expr):
    """Vertical stadium slot strap_slot_w x strap_slot_h centred (xc_expr, strap_z) on an XZ sketch (sketch y = -model z).
    xc_expr is negative (left side). Fully constrained."""
    H = adsk.fusion.DimensionOrientations
    cx = ev_cm(des, xc_expr); cy = -ev_cm(des, 'strap_z')
    w = ev_cm(des, 'strap_slot_w'); h = ev_cm(des, 'strap_slot_h')
    r = w / 2; L = (h - w) / 2
    Ls = sk.sketchCurves.sketchLines; As = sk.sketchCurves.sketchArcs
    lft = Ls.addByTwoPoints(P3(cx - r, cy - L, 0), P3(cx - r, cy + L, 0))
    rgt = Ls.addByTwoPoints(P3(cx + r, cy + L, 0), P3(cx + r, cy - L, 0))
    atop = As.addByThreePoints(lft.endSketchPoint, P3(cx, cy + L + r, 0), rgt.startSketchPoint)
    abot = As.addByThreePoints(rgt.endSketchPoint, P3(cx, cy - L - r, 0), lft.startSketchPoint)
    gc = sk.geometricConstraints; dm = sk.sketchDimensions
    gc.addVertical(lft); gc.addVertical(rgt)
    gc.addTangent(lft, atop); gc.addTangent(rgt, atop); gc.addTangent(lft, abot); gc.addTangent(rgt, abot)
    dm.addRadialDimension(atop, P3(cx + r + 0.3, cy + L + 0.3, 0)).parameter.expression = 'strap_slot_w / 2'
    dm.addDistanceDimension(abot.centerSketchPoint, atop.centerSketchPoint, H.VerticalDimensionOrientation, P3(cx + r + 0.5, cy, 0)).parameter.expression = 'strap_slot_h - strap_slot_w'
    dm.addDistanceDimension(sk.originPoint, atop.centerSketchPoint, H.HorizontalDimensionOrientation, P3(cx / 2, cy + L + r + 0.3, 0)).parameter.expression = '-(%s)' % xc_expr
    dm.addDistanceDimension(sk.originPoint, atop.centerSketchPoint, H.VerticalDimensionOrientation, P3(cx - r - 0.6, cy / 2, 0)).parameter.expression = '-strap_z + (strap_slot_h - strap_slot_w) / 2'
    return sk


def build_strap_slots(comp, des, target):
    """Two vertical slots through the rear-left corner of a bottom shell with a bar of wall between them. One sketch per slot."""
    add_strap_params(des)
    profs = adsk.core.ObjectCollection.create()
    for nm, xc in (('Strap slot outer', 'strap_x - (strap_slot_w + strap_bar_w) / 2'),
                   ('Strap slot inner', 'strap_x + (strap_slot_w + strap_bar_w) / 2')):
        sk = comp.sketches.add(comp.xZConstructionPlane); sk.name = nm
        _strap_slot(sk, des, xc)
        profs.add(sk.profiles.item(0))
    return ext(comp, profs, 'strap_cut_y - (y_rear_out - 1 mm)', FE.CutFeatureOperation, 'Strap slots', start='y_rear_out - 1 mm', target=target)


# ---------------------------------------------------------------- trigger finger-edge fillet (both triggers)
# Rounds the long edges across the finger blade (nose, bottom-flat corners, rear corner) so the trigger feels soft under the
# finger. Each edge ends on a flat side face, so it computes at the full radius. The outline of the two side faces is NOT filleted:
# it carries sub-0.5 mm sliver edges at the ramp teeth and the nose polyline, and any fillet chain over them fails (ASM_BL_CAP_COMPLEX
# at every radius down to 0.3 mm). Cleaning those slivers out of the profile would unlock it.
TRIG_FILLET_PARAMS = [
    ('trig_edge_r', '1.2 mm', 'Fillet on the long finger-blade edges of both triggers'),
    ('trig_edge_zmax', '-9.5 mm', 'Only edges below this z (the finger blade) are filleted; the ledge, hub, ear and pocket stay sharp'),
]


def add_trig_fillet_params(des):
    up = des.userParameters
    for n, e, c in TRIG_FILLET_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def finger_edges(comp, des):
    """Long (>10 mm) blade-wide edges below trig_edge_zmax whose faces meet at more than 20 deg."""
    import math
    zmax = ev_cm(des, 'trig_edge_zmax'); out = adsk.core.ObjectCollection.create()
    for e in comp.bRepBodies[0].edges:
        if e.faces.count != 2 or e.length < ev_cm(des, '10 mm'):
            continue
        ev = e.evaluator
        ok, t0, t1 = ev.getParameterExtents(); ok, pm = ev.getPointAtParameter((t0 + t1) / 2)
        if pm.z >= zmax:
            continue
        ok, n0 = e.faces[0].evaluator.getNormalAtPoint(pm); ok, n1 = e.faces[1].evaluator.getNormalAtPoint(pm)
        if math.degrees(math.acos(max(-1.0, min(1.0, n0.dotProduct(n1))))) > 20.0:
            out.add(e)
    return out


def build_trigger_finger_fillet(comp, des, name='Trigger finger edge fillet (long edges)'):
    add_trig_fillet_params(des)
    fi = comp.features.filletFeatures.createInput()
    fi.addConstantRadiusEdgeSet(finger_edges(comp, des), VI('trig_edge_r'), False)
    f = comp.features.filletFeatures.add(fi); f.name = name
    return f


# ---------------------------------------------------------------- trigger side-edge fillet (both triggers)
# Rounds the outline of the two flat side faces along the finger blade (rear face up to the hub, round the bottom, along the
# nose diagonal) so a finger resting on either side of the trigger doesn't meet a sharp edge. The outline is one closed loop that
# also carries the ledge, ramp teeth and hub pockets with sliver edges (0.07..0.4 mm); a chain over those fails (ASM_BL_CAP_COMPLEX),
# so only a contiguous run is filleted and the search takes the longest run that computes. 1.2 mm does not compute on the run, 1.0 does.
TRIG_SIDE_PARAMS = [('trig_side_r', '1.0 mm', 'Fillet on the side-face outline along the finger blade of both triggers (1.2 mm does not compute)')]


def add_trig_side_params(des):
    up = des.userParameters
    for n, e, c in TRIG_SIDE_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)


def _edge_mid_mm(e):
    ok, t0, t1 = e.evaluator.getParameterExtents(); ok, p = e.evaluator.getPointAtParameter((t0 + t1) / 2)
    return p.y * 10, p.z * 10


def trigger_side_faces(comp):
    out = []
    for f in comp.bRepBodies[0].faces:
        if f.geometry.surfaceType != adsk.core.SurfaceTypes.PlaneSurfaceType:
            continue
        ok, nn = f.evaluator.getNormalAtPoint(f.pointOnFace)
        if abs(nn.x) > 0.99 and f.area * 100 > 30:
            out.append((f.pointOnFace.x, f))
    return sorted(out, key=lambda t: t[0])


def build_trigger_side_fillets(comp, des):
    """One fillet feature per side face. Chain = rear-lower edge, then up toward the hub (u edges) and around the bottom/nose
    (d edges); longest (u, d) that computes wins."""
    add_trig_side_params(des)
    feats = []
    for xi in range(2):
        x, face = trigger_side_faces(comp)[xi]
        eds = list([l for l in face.loops if l.isOuter][0].edges); n = len(eds); ms = [_edge_mid_mm(e) for e in eds]
        i0 = min(range(n), key=lambda i: (ms[i][0] - 25.9) ** 2 + (ms[i][1] + 11.4) ** 2)
        du = 1 if ms[(i0 + 1) % n][1] > ms[i0][1] else -1
        done = None
        for u, d in sorted([(u, d) for u in range(1, 6) for d in range(3, 9)], key=lambda t: -(t[0] + t[1])):
            idx = [(i0 + du * t) % n for t in range(0, u + 1)] + [(i0 - du * t) % n for t in range(1, d + 1)]
            if len(set(idx)) != len(idx):
                continue
            es = adsk.core.ObjectCollection.create()
            for i in idx:
                es.add(eds[i])
            try:
                fi = comp.features.filletFeatures.createInput()
                fi.addConstantRadiusEdgeSet(es, VI('trig_side_r'), False)
                f = comp.features.filletFeatures.add(fi); f.name = 'Trigger side edge fillet (x=%.1f)' % (x * 10)
                done = f; break
            except RuntimeError:
                pass
        if done is None:
            raise RuntimeError('no side-edge fillet run computes on the face at x=%.1f' % (x * 10))
        feats.append(done)
    return feats
