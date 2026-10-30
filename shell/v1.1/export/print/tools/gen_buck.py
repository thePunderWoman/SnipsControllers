"""Pre-modelled support bucks for a print-oriented STL: a few big simple blocks in the large voids.
(older notes) Pre-modelled breakaway 'support buck' for a print-oriented STL.
Fills the void under overhangs (surfaces flatter than 45 deg) with a solid block that sits on the
bed (or on upward faces of the part), keeping a gap to the part: XY_GAP sideways, Z_GAP under the
overhang. The block is cut by thin kerfs into chunks so it can be popped out in pieces.
usage: gen_buck.py in_print.stl out_buck.stl [--max-y 50] [--band ya yb] [--conform ya yb ...]
(--conform = roof-following kerfed buck over that y band; plain blocks stay out of it)"""
import numpy as np, sys, struct, trimesh
from scipy import ndimage
from skimage import measure
CX=0.4; CZ=0.2          # voxel size XY / Z (Z = layer height)
XY_GAP=0.4; Z_GAP=0.2   # clearance to the part (one voxel each)
KERF=0.4; CHUNK=14.0    # kerf width and chunk pitch (mm)
MIN_RUN=0.6             # ignore voids shorter than this
def load(f):
    b=open(f,'rb').read(); n=struct.unpack('<I',b[80:84])[0]
    a=np.frombuffer(b[84:84+n*50],dtype=np.dtype([('n','<f4',3),('v','<f4',(3,3)),('a','<u2')]))
    return a['v'].astype(np.float64)
def write(path,tris,name):
    n=len(tris); nr=np.cross(tris[:,1]-tris[:,0],tris[:,2]-tris[:,0]); l=np.linalg.norm(nr,axis=1,keepdims=True); nr=nr/np.maximum(l,1e-12)
    with open(path,'wb') as f:
        f.write(name.encode()[:80].ljust(80,b'\0')); f.write(struct.pack('<I',n))
        a=np.zeros(n,dtype=np.dtype([('n','<f4',3),('v','<f4',(3,3)),('a','<u2')])); a['n']=nr; a['v']=tris; f.write(a.tobytes())
def solid_grid(tris):
    m=trimesh.Trimesh(tris.reshape(-1,3),np.arange(len(tris)*3).reshape(-1,3),process=True)
    lo,hi=m.bounds
    x0,y0=lo[0]-1.0,lo[1]-1.0
    nx=int((hi[0]-x0)/CX)+4; ny=int((hi[1]-y0)/CX)+4; nz=int(hi[2]/CZ)+3
    xs=x0+(np.arange(nx)+.5)*CX; ys=y0+(np.arange(ny)+.5)*CX
    gx,gy=np.meshgrid(xs,ys,indexing='ij'); org=np.c_[gx.ravel(),gy.ravel(),np.full(gx.size,-1.0)]
    loc,ir,_=m.ray.intersects_location(org,np.tile([0,0,1.0],(len(org),1)),multiple_hits=True)
    order=np.lexsort((loc[:,2],ir)); loc=loc[order]; ir=ir[order]
    solid=np.zeros((nx*ny,nz),bool); zc=(np.arange(nz)+.5)*CZ
    # parity fill
    starts=np.r_[0,np.nonzero(np.diff(ir))[0]+1,len(ir)]
    for a,b in zip(starts[:-1],starts[1:]):
        r=ir[a]; z=loc[a:b,2]
        for p in range(0,len(z)-1,2):
            solid[r,int(np.ceil((z[p]-CZ/2)/CZ)):int(np.floor((z[p+1]-CZ/2)/CZ))+1]=True
    return solid.reshape(nx,ny,nz),x0,y0,m
def make_buck(tris):
    solid,x0,y0,m=solid_grid(tris)
    return make_buck_from(solid,x0,y0)
def make_buck_from(solid,x0,y0,kerfs=False,need_all=False,conform=False):
    nx,ny,nz=solid.shape
    BIG=10**6
    # ceiling index (lowest solid at or above k)
    ceil=np.full((nx,ny,nz+1),BIG,np.int64)
    for k in range(nz-1,-1,-1):
        ceil[:,:,k]=np.where(solid[:,:,k],k,ceil[:,:,k+1])
    ceil=ceil[:,:,:nz]
    c=ceil.astype(np.float64)
    gx=(np.roll(c,-1,0)-np.roll(c,1,0))*CZ/(2*CX); gy=(np.roll(c,-1,1)-np.roll(c,1,1))*CZ/(2*CX)
    grad=np.hypot(gx,gy)
    need=(grad<1.0)&(ceil<BIG)&~solid
    if need_all: need=(ceil<BIG)&~solid
    # propagate need of the top void voxel of each run downward
    top=np.zeros_like(need)
    for k in range(nz-1,-1,-1):
        up=np.zeros((nx,ny),bool) if k==nz-1 else top[:,:,k+1]
        above_solid=np.zeros((nx,ny),bool) if k==nz-1 else solid[:,:,k+1]
        top[:,:,k]=np.where(above_solid,need[:,:,k],np.where(solid[:,:,k],False,up))
    fill=top&~solid
    # drop short runs
    run_h=(ceil-np.arange(nz)[None,None,:])*CZ
    fill&=run_h>=MIN_RUN
    # clearance: sideways 1 voxel, and Z_GAP under the part
    forbid=ndimage.binary_dilation(solid,structure=np.ones((3,3,1),bool))
    forbid|=np.concatenate([solid[:,:,1:],np.zeros((nx,ny,1),bool)],axis=2)
    # also forbid the diagonal corners in a 3x3x3 neighbourhood above-side
    fill&=~forbid
    if conform:
        # removable + clear: (1) no part material anywhere BELOW a buck voxel, so every buck column runs from the bed up to just under
        # the roof and the shell lifts straight off it (nothing can sit on a ledge or be trapped under an overhang);
        # (2) 0.8 mm sideways and 0.2-0.4 mm under the part, in all directions around the surface
        below=np.logical_or.accumulate(solid,axis=2)      # True from the first part voxel in a column upward
        fill&=~ndimage.binary_dilation(below,structure=np.ones((3,3,1),bool),iterations=2)   # nothing of the part below or within 0.8 mm beside it
        fill&=~ndimage.binary_dilation(solid,structure=np.ones((3,3,3),bool),iterations=1)
    # kerfs
    xs=x0+(np.arange(nx)+.5)*CX; ys=y0+(np.arange(ny)+.5)*CX
    kx=np.abs(((xs+CHUNK/2)%CHUNK)-CHUNK/2)<KERF/2
    ky=np.abs(((ys+CHUNK/2)%CHUNK)-CHUNK/2)<KERF/2
    if kerfs:
        fill&=~kx[:,None,None]; fill&=~ky[None,:,None]
    # keep only pieces that touch the bed or rest on the part
    lab,n=ndimage.label(fill,structure=np.ones((3,3,3)))
    keep=np.zeros(n+1,bool)
    rest=np.zeros_like(fill); rest[:,:,0]=True
    rest[:,:,1:]|=solid[:,:,:-1]
    ids=np.unique(lab[fill&rest]); keep[ids]=True; keep[0]=False
    sizes=ndimage.sum(fill,lab,index=np.arange(n+1))
    keep&=sizes>=60
    fill=keep[lab]
    return fill,x0,y0
def mesh_from(fill,x0,y0):
    pad=np.pad(fill.astype(np.float32),1)
    v,f,_,_=measure.marching_cubes(pad,0.5,spacing=(CX,CX,CZ))
    v[:,0]+=x0+CX/2-CX; v[:,1]+=y0+CX/2-CX; v[:,2]+=CZ/2-CZ
    v[:,2]=np.maximum(v[:,2],0)
    return v[f[:,::-1]]
# ---- simple box mode: a few big axis-aligned blocks for the large voids only ----
def _max_rect(m):
    """largest all-True rectangle in 2D bool array -> (area,i0,i1,j0,j1) inclusive"""
    nx,ny=m.shape; h=np.zeros(ny,int); best=(0,0,0,0,0)
    for i in range(nx):
        h=np.where(m[i],h+1,0)
        st=[]
        for j in range(ny+1):
            cur=h[j] if j<ny else 0
            start=j
            while st and st[-1][1]>=cur:
                s0,hh=st.pop(); a=hh*(j-s0)
                if a>best[0]: best=(a,i-hh+1,i,s0,j-1)
                start=s0
            st.append((start,cur))
    return best
def make_boxes(tris,extra=2,min_cm3=3.0,max_boxes=3,band=None,exclude=None):
    solid,x0,y0,m=solid_grid(tris)
    fill,_,_=make_buck_from(solid,x0,y0,need_all=True)
    # downsample 2x in XY and Z, erode by `extra` coarse voxels so the blocks stay well clear
    f=fill[:fill.shape[0]//2*2,:fill.shape[1]//2*2,:fill.shape[2]//2*2]
    f=f.reshape(f.shape[0]//2,2,f.shape[1]//2,2,f.shape[2]//2,2).all(axis=(1,3,5))
    f=ndimage.binary_erosion(f,structure=np.ones((3,3,1)),iterations=extra,border_value=1)
    cx,cz=CX*2,CZ*2; boxes=[]
    if band:   # only keep free space whose y lies inside the band
        ys=y0+np.arange(f.shape[1])*cx; f[:,(ys<band[0])|(ys+cx>band[1]),:]=False
    for ea,eb in (exclude or []):   # y-bands that are filled by conforming bucks instead
        ys=y0+np.arange(f.shape[1])*cx; f[:,(ys+cx>ea-0.8)&(ys<eb+0.8),:]=False
    for _ in range(max_boxes):
        best=None; acc=np.ones(f.shape[:2],bool)
        for k in range(f.shape[2]):
            acc&=f[:,:,k]
            if not acc.any(): break
            a,i0,i1,j0,j1=_max_rect(acc)
            v=a*(k+1)
            if best is None or v>best[0]: best=(v,i0,i1,j0,j1,k)
        if best is None or best[0]*cx*cx*cz<min_cm3*1000: break
        v,i0,i1,j0,j1,k=best
        boxes.append((x0+i0*cx,x0+(i1+1)*cx,y0+j0*cx,y0+(j1+1)*cx,0.0,(k+1)*cz))
        f[max(i0-1,0):i1+2,max(j0-1,0):j1+2,:k+1]=False
    return boxes
def make_conform(tris,ya,yb,min_vox=400):
    """roof-following buck (0.2 mm under the roof, 0.4 mm from walls), kerfed every CHUNK mm so it lifts out in pieces,
    only for y in [ya,yb] (print frame)"""
    solid,x0,y0,m=solid_grid(tris)
    fill,_,_=make_buck_from(solid,x0,y0,kerfs=True,need_all=True,conform=True)
    ys=y0+(np.arange(fill.shape[1])+.5)*CX
    fill[:,(ys<ya)|(ys>yb),:]=False
    # mesh-level safety pass: the voxel model can miss thin skins, so trim every buck voxel on a boundary that is
    # closer than 0.4 mm to the real part mesh, drop slivers (< 3 voxels wide) and small leftovers; repeat once
    from trimesh.proximity import closest_point
    for _ in range(2):
        fill=ndimage.binary_opening(fill,structure=np.ones((3,3,1),bool))
        bnd=fill&~ndimage.binary_erosion(fill,structure=np.ones((3,3,3),bool),border_value=1)
        ii=np.argwhere(bnd)
        if len(ii):
            c=np.c_[x0+(ii[:,0]+.5)*CX,y0+(ii[:,1]+.5)*CX,(ii[:,2]+.5)*CZ]
            d=np.concatenate([closest_point(m,c[a:a+4000])[1] for a in range(0,len(c),4000)])
            bad=ii[d<0.4]; fill[bad[:,0],bad[:,1],bad[:,2]]=False
    lab,n=ndimage.label(fill,structure=np.ones((3,3,3)))
    sz=ndimage.sum(fill,lab,index=np.arange(n+1)); keep=sz>=min_vox; keep[0]=False
    fill=keep[lab]
    mt=mesh_from(fill,x0,y0)
    try:   # marching cubes is very dense: decimate 90% (checked: stays watertight, gap to the part within ~0.02 mm)
        import fast_simplification as fs
        v=mt.reshape(-1,3); m=trimesh.Trimesh(v,np.arange(len(v)).reshape(-1,3),process=True)
        pv,pf=fs.simplify(np.asarray(m.vertices,float),np.asarray(m.faces,np.int32),target_reduction=0.85)
        sm=trimesh.Trimesh(pv,pf,process=False)
        if sm.is_watertight: mt=np.asarray(sm.vertices)[np.asarray(sm.faces)]
    except Exception as e: print('decimation skipped:',e)
    return mt,int(keep.sum())
def box_tris(b):
    x0,x1,y0,y1,z0,z1=b
    v=np.array([[x0,y0,z0],[x1,y0,z0],[x1,y1,z0],[x0,y1,z0],[x0,y0,z1],[x1,y0,z1],[x1,y1,z1],[x0,y1,z1]])
    f=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
    return v[np.array(f)]

def raise_top(tris,b,gap=Z_GAP,side=0.3):
    """lift the flat top of a block to `gap` under the lowest roof point above its footprint, as long as every
    face of the block stays at least `side` from the part"""
    m=trimesh.Trimesh(tris.reshape(-1,3),np.arange(len(tris)*3).reshape(-1,3),process=True)
    x0,x1,y0,y1,z0,z1=b
    xs=np.linspace(x0+.2,x1-.2,30); ys=np.linspace(y0+.2,y1-.2,30)
    gx,gy=np.meshgrid(xs,ys); o=np.c_[gx.ravel(),gy.ravel(),np.full(gx.size,z1)]
    loc,ir,_=m.ray.intersects_location(o,np.tile([0,0,1.],(len(o),1)),multiple_hits=False)
    if len(loc)==0: return b
    top=float(loc[:,2].min())-gap
    from trimesh.proximity import closest_point
    rng=np.random.default_rng(0)
    for zt in np.arange(top,z1,-0.2)[:60]:
        nb=(x0,x1,y0,y1,z0,zt)
        pts=np.concatenate([np.c_[rng.uniform(x0,x1,4000),rng.uniform(y0,y1,4000),np.full(4000,zt)],
            np.c_[np.full(2000,x0),rng.uniform(y0,y1,2000),rng.uniform(z1,zt,2000)],np.c_[np.full(2000,x1),rng.uniform(y0,y1,2000),rng.uniform(z1,zt,2000)],
            np.c_[rng.uniform(x0,x1,2000),np.full(2000,y0),rng.uniform(z1,zt,2000)],np.c_[rng.uniform(x0,x1,2000),np.full(2000,y1),rng.uniform(z1,zt,2000)]])
        if zt<=z1: return nb if zt>z1-1e-9 else b
        _,d,_=closest_point(m,pts)
        if d.min()>=side-1e-6 and not m.contains(pts[::7]).any(): return nb
    return b
if __name__=='__main__':
    # usage: gen_buck.py in_print_plain.stl out_with_buck.stl   (boxes are added as extra lumps in place)
    tris=load(sys.argv[1])
    conf=[(float(sys.argv[i+1]),float(sys.argv[i+2])) for i,a in enumerate(sys.argv) if a=='--conform']
    boxes=make_boxes(tris,extra=1,min_cm3=2.5,max_boxes=4,exclude=conf)
    if '--band' in sys.argv:   # extra small blocks in a y band, e.g. around the face buttons / thumbstick: --band -20 22
        i=sys.argv.index('--band'); bb=(float(sys.argv[i+1]),float(sys.argv[i+2]))
        boxes+=make_boxes(tris,extra=1,min_cm3=0.3,max_boxes=8,band=bb,exclude=conf)
    if '--max-y' in sys.argv:   # drop blocks out past this print-frame y (e.g. outside the shell, under the OLED housing)
        my=float(sys.argv[sys.argv.index('--max-y')+1]); boxes=[b for b in boxes if b[3]<=my]
    boxes=[raise_top(tris,b) for b in boxes]
    extra=[]
    for ya,yb in conf:
        mt,nl=make_conform(tris,ya,yb); extra.append(mt); print('conforming buck y %.1f..%.1f: %d pieces, %d triangles'%(ya,yb,nl,len(mt)))
    allt=np.concatenate([tris]+[box_tris(b) for b in boxes]+extra) if (boxes or extra) else tris
    write(sys.argv[2],allt,'part+buck')
    for b in boxes: print('box x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f  (%.1f cm3)'%(b[0],b[1],b[2],b[3],b[4],b[5],(b[1]-b[0])*(b[3]-b[2])*(b[5]-b[4])/1000))
