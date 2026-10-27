import numpy as np, os, struct, json
import orient as O, build_sup as B
import trimesh
src='/mnt/user-data/uploads/SnipsControllers/shell/export/stl'
out='/mnt/user-data/outputs/print/stl'; os.makedirs(out,exist_ok=True)
ORI={'TriggerPost':'-Z up (flipped)','TopShell':'+Z up (as modelled)','BottomShell':'-Z up (flipped)','BatteryPanel':'+Z up (as modelled)','BumperCap':'+Y up','Trigger':'-X up'}
for n in ['HallBridge','PowerPlate']+['FaceCap%d'%i for i in range(1,7)]: ORI[n]='+Z up (as modelled)'
ORI['PowerPlate']='+Y up'
SUP={'Trigger'}
def write(path,tris,name):
    n=len(tris); nr=np.cross(tris[:,1]-tris[:,0],tris[:,2]-tris[:,0]); l=np.linalg.norm(nr,axis=1,keepdims=True); nr=nr/np.maximum(l,1e-12)
    with open(path,'wb') as f:
        f.write(name.encode()[:80].ljust(80,b'\0')); f.write(struct.pack('<I',n))
        a=np.zeros(n,dtype=np.dtype([('n','<f4',3),('v','<f4',(3,3)),('a','<u2')])); a['n']=nr; a['v']=tris; f.write(a.tobytes())
rep={}
for n,o in ORI.items():
    tris=O.load(os.path.join(src,n+'.stl')); R=O.ORIENTS[o]
    t=tris@R.T; zmin=t[:,:,2].min(); t[:,:,2]-=zmin
    sup=None
    if n in SUP:
        _,_,P,cov=B.layout(n,o)
        m=trimesh.Trimesh(t.reshape(-1,3),np.arange(len(t)*3).reshape(-1,3),process=True)
        from scipy import ndimage
        cell=0.3; ov,zs,base,x0,y0,zm=B.G.heightfield(tris,R,cell)
        lab,k=ndimage.label(ov,structure=np.ones((3,3))); big=np.zeros_like(ov)
        for r in range(1,k+1):
            if (lab==r).sum()*cell*cell>=1.5: big|=(lab==r)
        big&=((zs-base)>=0.6)
        er=ndimage.distance_transform_edt(big)*cell
        def ok(i,j):
            x=x0+(i+.5)*cell; y=y0+(j+.5)*cell; z0=float(base[i,j]); z1=float(zs[i,j])-B.GAP
            if z1-z0<0.4: return None
            zz=np.linspace(z0+0.05,z1-0.05,6)
            for hw,bw,tw in ((0.5,1.0,0.6),(0.35,0.7,0.5)):
                pts=np.array([(x+dx,y+dy,z) for z in zz for dx in (-hw,0,hw) for dy in (-hw,0,hw)])
                if not m.contains(pts).any(): return (x,y,z0,z1,bw,tw)
            return None
        cm=np.zeros_like(ov); keep=[]; bad=0
        for (x,y,z0,z1) in P:
            i=int((x-x0)/cell); j=int((y-y0)/cell)
            if ok(i,j): keep.append(ok(i,j)); cm[i,j]=True
            else: bad+=1
        unreach=np.zeros_like(ov)
        for _ in range(400):
            dist=ndimage.distance_transform_edt(~cm)*cell
            un=big&(dist>1.4)&~unreach
            if not un.any(): break
            ci,cj=min(map(tuple,np.argwhere(un)),key=lambda c:(c[0],c[1]))
            cand=[(i,j) for i,j in np.argwhere(big&(np.hypot(*(np.indices(ov.shape)-np.array([ci,cj])[:,None,None]))*cell<=1.2)) ]
            cand.sort(key=lambda c:-er[c[0],c[1]])
            done=False
            for i,j in cand:
                r=ok(i,j)
                if r: keep.append(r); cm[i,j]=True; done=True; break
            if not done: unreach[ci,cj]=True
        P=keep
        sup=np.concatenate([B.pillar_mesh(*p) for p in P])
        dist=ndimage.distance_transform_edt(~cm)*cell
        cov=float((big&(dist<=1.4)).sum()/big.sum()); far=float(dist[big].max())
        rep[n]=dict(pillars=len(P),removed_for_clipping=bad,coverage_1p4mm=round(cov,3),max_unsupported_distance=round(far,2),watertight=bool(m.is_watertight))
    allt=t if sup is None else np.concatenate([t,sup])
    cx=(t[:,:,0].min()+t[:,:,0].max())/2; cy=(t[:,:,1].min()+t[:,:,1].max())/2
    allt=allt.copy(); allt[:,:,0]-=cx; allt[:,:,1]-=cy
    write(os.path.join(out,n+('_with_supports' if sup is not None else '')+'.stl'),allt,n)
    ext=allt.reshape(-1,3); print(n,o,'bbox %.1f x %.1f x %.1f'%tuple(ext.max(0)-ext.min(0)))
print(json.dumps(rep,indent=1))
ORI.update({'SidePlateL1':'+X up','SidePlateL2':'+X up','SidePlateR1':'-X up','SidePlateR2':'-X up'})
