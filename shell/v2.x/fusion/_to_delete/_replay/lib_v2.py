"""Helpers for building the Snips v2 shell in Fusion (units: cm in the API, mm in expressions).
Run inside Fusion's script engine; import with sys.path.insert(0, <this folder>)."""
import adsk.core, adsk.fusion
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


def build_bottom_columns(bot_comp, des):
    up = des.userParameters
    body = bot_comp.bRepBodies.item(0)
    z_floor = '-(bot_d - floor_t)'
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'PCB columns'
    hole_circles(sk, des, '2 * post_r')
    ext(bot_comp, all_profiles(sk), 'pcb_bot_z - (%s)' % z_floor, FE.JoinFeatureOperation, 'PCB columns', start=z_floor)
    # trim anything outside the cavity above the rib top (that zone belongs to the top shell wall)
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'Column trim ring'
    plan(sk, '0 mm'); plan(sk, 'wall')
    ext(bot_comp, profile_by_loops(sk, 2)[0], '1 mm', FE.CutFeatureOperation, 'Column trim', start='lap_h - seam_clr', target=bot_comp.bRepBodies.item(0))
    sk = bot_comp.sketches.add(bot_comp.xYConstructionPlane); sk.name = 'Screw head wells'
    hole_circles(sk, des, 'head_well_d')
    ext(bot_comp, all_profiles(sk), 'bot_d + screw_head_seat_z', FE.CutFeatureOperation, 'Screw head wells', start='-bot_d', target=bot_comp.bRepBodies.item(0))
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
          'y_rear_out + wall', 'usb_face_y - usb_stop_gap + 0.5 mm')
    if keep_above:
        ext(comp, main_profile(t), '5 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim', neg=True,
            start='usb_bezel_split_top', target=target)
    else:
        ext(comp, main_profile(t), '10 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim',
            start='usb_bezel_split_bot', target=target)
    # re-cut the bore through the bezel using the existing pocket sketch
    ps = [s for s in comp.sketches if s.name in ('USB plug pocket', 'USB plug notch (bottom)')][0]
    ext(comp, main_profile(ps), 'usb_face_y - usb_stop_gap - (y_rear_out - 1 mm)', FE.CutFeatureOperation,
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
    ('fb_flange_t', '1 mm', 'Button flange thickness'),
    ('fb_switch_top', '6.29 mm', 'PTS810 plunger top z (flange rests on it)'),
    ('fb_free', '0.6 mm', 'Free travel between flange top and guide tube bottom'),
    ('fb_proud', '2 mm', 'Cap top stands this far above the pad'),
    ('fb_tube_od', '7 mm', 'Guide tube outer diameter'),
    ('fb_text_h', '2.8 mm', 'Number height'),
    ('fb_text_depth', '0.4 mm', 'Number deboss depth'),
    ('fb_cap_fillet', '0.4 mm', 'Cap top edge fillet'),
    ('fb_flange_top', 'fb_switch_top + fb_flange_t', 'Flange top z'),
    ('fb_tube_bot', 'fb_flange_top + fb_free', 'Guide tube bottom z'),
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
