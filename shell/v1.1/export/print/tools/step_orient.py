import os
from build123d import import_step, export_step, Pos, Rot
import export_print_cfg as C
import paths as P
src=P.SRC_STEP
out=P.OUT_STEP; os.makedirs(out,exist_ok=True)
RR={'+Z up (as modelled)':Rot(0,0,0),'-Z up (flipped)':Rot(X=180),'-Y up':Rot(X=90),'+Y up':Rot(X=-90),'-X up':Rot(Y=-90),'+X up':Rot(Y=90)}
for n,o in C.ORI.items():
    s=RR[o]*import_step(os.path.join(src,n+'.step'))
    bb=s.bounding_box()
    s=Pos(-(bb.min.X+bb.max.X)/2,-(bb.min.Y+bb.max.Y)/2,-bb.min.Z)*s
    b=s.bounding_box(); export_step(s,os.path.join(out,n+'.step'))
    print(n,'%.1f x %.1f x %.1f'%(b.size.X,b.size.Y,b.size.Z),'vol %.0f'%s.volume)
