"""Raw Fusion STEP -> print-orientation STEP (v2), same orientations as export_print.py.
Needs build123d. Run from this folder: python3 step_orient.py"""
import os
from build123d import import_step, export_step, Pos, Rot, Compound
import export_print_cfg as C
import paths as P

def rot(s, rots):
    for ax, d in rots:
        s = Rot(**{ax.upper(): d}) * s
    return s

def drop(s):
    bb = s.bounding_box()
    return Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * s

os.makedirs(P.OUT_STEP, exist_ok=True)
for n, (rots, note) in C.ORI.items():
    s = drop(rot(import_step(os.path.join(P.SRC_STEP, n + '.step')), rots))
    b = s.bounding_box(); export_step(s, os.path.join(P.OUT_STEP, n + '.step'))
    print('%-13s %.1f x %.1f x %.1f  vol %.1f' % (n, b.size.X, b.size.Y, b.size.Z, s.volume))

caps = import_step(os.path.join(P.SRC_STEP, 'TriggerCaps.step'))
solids = sorted(caps.solids(), key=lambda x: x.center().X)
assert len(solids) == 2, len(solids)
ps = [drop(rot(x, C.CAPS_ROT)) for x in solids]
w = [p.bounding_box().size.X for p in ps]
ps[0] = Pos(-(w[0] + C.CAPS_GAP) / 2, 0, 0) * ps[0]
ps[1] = Pos((w[1] + C.CAPS_GAP) / 2, 0, 0) * ps[1]
out = Compound(ps); b = out.bounding_box()
export_step(out, os.path.join(P.OUT_STEP, 'TriggerCaps.step'))
print('%-13s %.1f x %.1f x %.1f  vol %.2f (two plates)' % ('TriggerCaps', b.size.X, b.size.Y, b.size.Z, out.volume))
