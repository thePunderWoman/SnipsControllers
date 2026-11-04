
cd "$HOME/mnt/SnipsControllers/shell/v2.x/fusion"
mkdir -p _replay && cd _replay
echo "== step 1"
cat >> lib_v2.py <<'EOF'


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
    gc.addEqual(ar, al)
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
EOF
python3 -c "import ast;ast.parse(open('$HOME/mnt/SnipsControllers/shell/v2.x/fusion/lib_v2.py').read())" && echo ok
echo "== step 35"
sed -i '/    gc.addEqual(ar, al)$/d' lib_v2.py && grep -c "addEqual(ar, al)" lib_v2.py
echo "== step 78"
cat >> lib_v2.py <<'EOF'


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
    ext(comp, main_profile(sk), length, FE.JoinOperation if hasattr(FE, 'JoinOperation') else FE.JoinFeatureOperation,
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
        ext(comp, main_profile(t), '5 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim',
            start='usb_bezel_split_bot', target=target)
    # re-cut the bore through the bezel using the existing pocket sketch
    ps = [s for s in comp.sketches if s.name in ('USB plug pocket', 'USB plug notch (bottom)')][0]
    ext(comp, main_profile(ps), 'usb_face_y - usb_stop_gap - (y_rear_out - 1 mm)', FE.CutFeatureOperation,
        name_prefix + ' bezel bore', start='y_rear_out - 1 mm', target=target)
EOF
grep -n "JoinFeatureOperation\|^FE\|^VI\|^P3" lib_v2.py | head
echo "== step 84"
sed -i "s/FE.JoinOperation if hasattr(FE, 'JoinOperation') else FE.JoinFeatureOperation/FE.JoinFeatureOperation/" lib_v2.py && python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 92"
python3 - <<'EOF'
s=open('lib_v2.py').read()
old="""        ext(comp, main_profile(t), '5 mm', FE.CutFeatureOperation, name_prefix + ' bezel trim',
            start='usb_bezel_split_bot', target=target)"""
assert old in s
s=s.replace(old,old.replace("'5 mm'","'10 mm'"))
open('lib_v2.py','w').write(s)
EOF
echo done
echo "== step 158"
cat >> lib_v2.py <<'EOF'


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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 179"
cat >> lib_v2.py <<'EOF'


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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 207"
cat >> lib_v2.py <<'EOF'


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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 232"
cat >> lib_v2.py <<'EOF'


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
    pts.add(P3(RR, -RIM, 0)); pts.add(P3((RR + RB) / 2, -(RIM + PAD) / 2, 0)); pts.add(P3(RB, -PAD, 0))
    spl = sk.sketchCurves.sketchFittedSplines.add(pts)
    gc.addCoincident(spl.startSketchPoint, top.endSketchPoint)
    gc.addCoincident(spl.endSketchPoint, side.startSketchPoint)
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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 240"
python3 - <<'EOF'
s=open('lib_v2.py').read()
s=s.replace("""    pts = adsk.core.ObjectCollection.create()
    pts.add(P3(RR, -RIM, 0)); pts.add(P3((RR + RB) / 2, -(RIM + PAD) / 2, 0)); pts.add(P3(RB, -PAD, 0))
    spl = sk.sketchCurves.sketchFittedSplines.add(pts)
    gc.addCoincident(spl.startSketchPoint, top.endSketchPoint)
    gc.addCoincident(spl.endSketchPoint, side.startSketchPoint)
""","""    pts = adsk.core.ObjectCollection.create()
    pts.add(top.endSketchPoint); pts.add(sk.sketchPoints.add(P3((RR + RB) / 2, -(RIM + PAD) / 2, 0))); pts.add(side.startSketchPoint)
    spl = sk.sketchCurves.sketchFittedSplines.add(pts)
""")
s=s.replace("""    pl = _bump_plane(top_comp)
    sk = top_comp.sketches.add(pl); sk.name = 'Stick bump profile'""","""    for s_ in [x for x in top_comp.sketches if x.name == 'Stick bump profile']: s_.deleteMe()
    for p_ in [x for x in top_comp.constructionPlanes if x.name == 'Stick axis plane']: p_.deleteMe()
    pl = _bump_plane(top_comp)
    sk = top_comp.sketches.add(pl); sk.name = 'Stick bump profile'""")
open('lib_v2.py','w').write(s)
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 258"
python3 - <<'EOF'
s=open('lib_v2.py').read()
a="pts.add(top.endSketchPoint); pts.add(sk.sketchPoints.add(P3((RR + RB) / 2, -(RIM + PAD) / 2, 0))); pts.add(side.startSketchPoint)"
assert a in s
s=s.replace(a,"pts.add(top.endSketchPoint); pts.add(side.startSketchPoint)  # 2 fit points + horizontal end tangents = fully constrained S-curve")
open('lib_v2.py','w').write(s)
EOF
echo ok
echo "== step 261"
cat >> lib_v2.py <<'EOF'


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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 269"
cat >> lib_v2.py <<'EOF'


def build_stick_bump_hollow(comp, des):
    """Hollow the bump to a constant skin (bump_wall): revolve-cut the region under the inward offset of the S-curve,
    then cut the stick opening and round its top edge."""
    body = comp.bRepBodies.item(0)
    psk = [s for s in comp.sketches if s.name == 'Stick bump profile'][0]
    pl = [p for p in comp.constructionPlanes if p.name == 'Stick axis plane'][0]
    for s_ in [x for x in comp.sketches if x.name == 'Stick bump cavity profile']: s_.deleteMe()
    sk = comp.sketches.add(pl); sk.name = 'Stick bump cavity profile'
    axis = sk.project(comp.zConstructionAxis).item(0)
    spl = psk.sketchCurves.sketchFittedSplines.item(0)
    proj = sk.project(spl)
    for c in proj: c.isConstruction = True
    curves = adsk.core.ObjectCollection.create()
    for c in proj: curves.add(c)
    gc = sk.geometricConstraints
    oi = gc.createOffsetInput(curves, VI('-bump_wall'))
    off = gc.addOffset2(oi)
    oc = [c for c in sk.sketchCurves if isinstance(c, adsk.fusion.SketchFittedSpline) and not c.isConstruction][0]
    a, b = oc.startSketchPoint, oc.endSketchPoint
    lo, hi = (a, b) if a.geometry.y < b.geometry.y else (b, a)   # sketch y = -z : lower y = higher z
    hi, lo = lo, hi                                              # hi = top end (small |y|? check below)
    return sk, oc, off


def finish_stick_bump(comp, des, sk, oc):
    pass
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 274"
python3 - <<'EOF'
s=open('lib_v2.py').read()
s=s.replace("""    curves = adsk.core.ObjectCollection.create()
    for c in proj: curves.add(c)
    gc = sk.geometricConstraints
    oi = gc.createOffsetInput(curves, VI('-bump_wall'))""","""    curves = [c for c in proj]
    gc = sk.geometricConstraints
    oi = gc.createOffsetInput(curves, VI('-bump_wall'))""")
open('lib_v2.py','w').write(s)
EOF
echo ok
echo "== step 285"
python3 - <<'EOF'
s=open('lib_v2.py').read()
i=s.index("    oc = [c for c in sk.sketchCurves if isinstance(c, adsk.fusion.SketchFittedSpline)")
s=s[:i]+'''    oc = [c for c in sk.sketchCurves if c.objectType.endswith('SketchControlPointSpline') and not c.isConstruction][0]
    a, b = oc.startSketchPoint, oc.endSketchPoint
    base, tip = (a, b) if a.geometry.x > b.geometry.x else (b, a)      # base at r=bump_base_r, tip at the rim
    Ls = sk.sketchCurves.sketchLines
    l1 = Ls.addByTwoPoints(base, P3(0, base.geometry.y, 0))
    l2 = Ls.addByTwoPoints(tip, P3(0, tip.geometry.y, 0))
    Ls.addByTwoPoints(l1.endSketchPoint, l2.endSketchPoint)
    gc.addCoincident(l1.endSketchPoint, axis); gc.addCoincident(l2.endSketchPoint, axis)
    gc.addHorizontal(l1); gc.addHorizontal(l2)
    ri = comp.features.revolveFeatures.createInput(main_profile(sk), axis, FE.CutFeatureOperation)
    ri.setAngleExtent(False, VI('360 deg'))
    ri.participantBodies = [body]
    f = comp.features.revolveFeatures.add(ri); f.name = 'Stick bump cavity'
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
'''
open('lib_v2.py','w').write(s)
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 295"
python3 - <<'EOF'
s=open('lib_v2.py').read()
i=s.index("    Ls = sk.sketchCurves.sketchLines\n    l1 = Ls.addByTwoPoints(base, P3(0, base.geometry.y, 0))")
j=s.index("    # stick opening")
new='''    Ls = sk.sketchCurves.sketchLines
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
'''
s=s[:i]+new+s[j:]
s=s.replace("    ('bump_hole_fillet', '0.6 mm', 'Fillet on the top edge of the stick opening'),","    ('bump_hole_fillet', '0.6 mm', 'Fillet on the top edge of the stick opening'),\n    ('bump_hollow_z', '11 mm', 'Bottom of the bump hollowing tool (inside the cavity)'),")
open('lib_v2.py','w').write(s)
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 306"
python3 - <<'EOF'
s=open('lib_v2.py').read()
a='''    body = comp.bRepBodies.item(0)
    psk = [s for s in comp.sketches if s.name == 'Stick bump profile'][0]'''
assert a in s
s=s.replace(a,'''    up = des.userParameters
    for n, e, c in BUMP_PARAMS:
        if up.itemByName(n) is None:
            up.add(n, VI(e), 'mm', c)
'''+a)
open('lib_v2.py','w').write(s)
EOF
echo ok
echo "== step 374"
cat >> lib_v2.py <<'EOF'


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
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && grep -n "^def all_profiles" -A6 lib_v2.py
echo "== step 383"
cat >> lib_v2.py <<'EOF'


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
    ts.sketchTexts.add(ti)
    profs = sorted([p for p in ts.profiles], key=lambda p: p.areaProperties().area)
    if str(label) in ('4', '6') and len(profs) > 1:
        profs = profs[1:]          # drop the glyph counter (inner island)
    col_ = adsk.core.ObjectCollection.create()
    for p in profs: col_.add(p)
    ext(comp, col_, 'fb_text_depth', FE.CutFeatureOperation, 'Number deboss', neg=True, target=body)
    return comp
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
echo "== step 388"
python3 - <<'EOF'
s=open('lib_v2.py').read()
i=s.index("    ts.sketchTexts.add(ti)\n    profs = sorted")
j=s.index("    return comp\n", i)
s=s[:i]+"    txt = ts.sketchTexts.add(ti)\n    ext(comp, txt, 'fb_text_depth', FE.CutFeatureOperation, 'Number deboss', neg=True, target=body)\n"+s[j:]
open('lib_v2.py','w').write(s)
EOF
python3 -c "import ast;ast.parse(open('lib_v2.py').read())" && echo ok
