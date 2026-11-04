import numpy as np, os, json, math
import orient as O, gen_supports as G
from scipy import ndimage
import paths as P
d=P.SRC_STL
GAP=0.2; BASE_W=1.0; TIP_W=0.6
def pillar_mesh(x,y,z0,z1,bw=None,tw=None):
    h0=(bw or BASE_W)/2; h1=(tw or TIP_W)/2
    b=[(x-h0,y-h0,z0),(x+h0,y-h0,z0),(x+h0,y+h0,z0),(x-h0,y+h0,z0)]
    t=[(x-h1,y-h1,z1),(x+h1,y-h1,z1),(x+h1,y+h1,z1),(x-h1,y+h1,z1)]
    v=np.array(b+t); f=[(0,2,1),(0,3,2),(4,5,6),(4,6,7)]
    for i in range(4):
        j=(i+1)%4; f+= [(i,j,j+4),(i,j+4,i+4)]
    return v[np.array(f)]
def layout(n,o,pitch=2.0,margin=0.3,cell=0.3):
    tris=O.load(os.path.join(d,n+'.stl')); R=O.ORIENTS[o]
    ov,zs,base,x0,y0,zmin=G.heightfield(tris,R,cell)
    lab,k=ndimage.label(ov,structure=np.ones((3,3)))
    s=int(round(pitch/cell)); pts=[]
    for r in range(1,k+1):
        m=lab==r
        if m.sum()*cell*cell<1.5: continue
        er=ndimage.distance_transform_edt(m)*cell
        idx=np.argwhere(m)
        for i,j in idx:
            if i%s==0 and j%s==0 and er[i,j]>=margin: pts.append((i,j))
        # guarantee every connected piece of the region has a pillar within reach
        cov=np.zeros_like(m)
        for i,j in pts: cov[i,j]=True
        dist=ndimage.distance_transform_edt(~cov)*cell
        while True:
            un=m&(dist>1.4)
            if not un.any(): break
            i,j=max(map(tuple,np.argwhere(un)),key=lambda c:(er[c]))
            pts.append((i,j)); cov[i,j]=True; dist=ndimage.distance_transform_edt(~cov)*cell
    P=[(x0+(i+.5)*cell,y0+(j+.5)*cell,float(base[i,j]),float(zs[i,j])-GAP) for i,j in pts]
    P=[p for p in P if p[3]-p[2]>0.4]
    covered=(ndimage.distance_transform_edt(~np.isin(np.indices(ov.shape)[0]*0,[1]))) # placeholder
    cm=np.zeros_like(ov)
    for i,j in pts: cm[i,j]=True
    dist=ndimage.distance_transform_edt(~cm)*cell
    return tris,R,P,float((ov&(dist<=1.4)).sum()/max(ov.sum(),1))
if __name__=='__main__':
    for n,o in {'BumperCap':'-Z up (flipped)','Trigger':'-X up'}.items():
        tris,R,P,c=layout(n,o)
        hs=[p[3]-p[2] for p in P]
        print(n,len(P),'pillars  h %.1f..%.1f'%(min(hs),max(hs)),'coverage within 1.4mm: %.0f%%'%(100*c))
