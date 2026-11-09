"""Bake terrain material placement from MTX's ground faces and their authored layer blends."""
import struct
import numpy as np

# Paradise Valley texture checksums, inspected against the source cache. Classes are
# grass, rough soil, rock; zero is the primary dark riding dirt. These are surface
# identities, not brightness guesses (the autumn grass is brown/olive).
PARADISE = {
    0xdc9d0810:(1,0,0), 0xf4d87be0:(0,1,0), 0x223592e8:(0,1,0),
    0xcf257168:(0,1,0), 0xeacc5f48:(0,1,0), 0x036caa28:(0,0,1),
    0x6f7ac770:(.6,.4,0), 0x45ff2208:(0,0,0),
}

class SurfaceBake:
    size=2048
    def __init__(self, center, mapping=None):
        self.mapping=PARADISE if mapping is None else mapping
        self.colour=None if mapping is None else np.zeros((self.size,self.size,4),dtype=np.uint8)
        if self.colour is not None:self.colour[:,:,:]=[70,60,45,255]
        self.center=center
        self.pixels=np.zeros((self.size,self.size,4),dtype=np.uint8)
        self.pixels[:,:,0]=255
        self.height=np.full((self.size,self.size),-np.inf,dtype=np.float32)
        self.faces=0

    def accepts(self, material):
        return struct.unpack_from("<I",material[0])[0] in self.mapping

    def draw(self, material, raw, indices, textures):
        b,layers=material;tid=struct.unpack_from('<I',b)[0]
        if tid not in self.mapping:return
        v=np.ndarray((len(raw)//48,3),'<f4',raw,strides=(48,4))
        uv=np.ndarray((len(v),4,2),'<f4',raw,offset=12,strides=(48,8,4))
        alpha=np.ndarray((len(v),),np.uint8,raw,offset=47,strides=(48,))/127.5
        points=np.column_stack(((v[:,0]-self.center[0]+2047)/2,(2047-(v[:,2]-self.center[2]))/2))
        linear_textures={}
        def linear_texture(tid):
            if tid not in linear_textures:
                tw,th,pixels=textures[tid];tex=np.frombuffer(pixels,np.uint8).reshape(th,tw,4)/255.
                tex[:,:,:3]=np.where(tex[:,:,:3]<=.04045,tex[:,:,:3]/12.92,((tex[:,:,:3]+.055)/1.055)**2.4)
                linear_textures[tid]=(tw,th,tex)
            return linear_textures[tid]
        for ids in indices:
            t=points[ids];lo=np.maximum(np.floor(t.min(axis=0)).astype(int),0);hi=np.minimum(np.ceil(t.max(axis=0)).astype(int),2047)
            if np.any(lo>hi):continue
            x,y=np.meshgrid(np.arange(lo[0],hi[0]+1),np.arange(lo[1],hi[1]+1))
            a,c=t[1]-t[0],t[2]-t[0];det=a[0]*c[1]-a[1]*c[0]
            if abs(det)<1e-8:continue
            u=((x-t[0,0])*c[1]-(y-t[0,1])*c[0])/det
            w=(a[0]*(y-t[0,1])-a[1]*(x-t[0,0]))/det
            weights=np.stack((1-u-w,u,w),axis=-1);inside=(weights>=-1e-5).all(axis=-1)
            h=weights@v[ids,1]; region=self.height[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
            inside&=h>=region-.05
            if not inside.any():continue
            rgb=np.broadcast_to(np.array(self.mapping[tid],np.float32),(*x.shape,3)).copy()
            va=np.clip(weights@alpha[ids],0,1)
            def sample(tid, coord, clamp_u, clamp_v):
                tw,th,tex=linear_texture(tid)
                # Bilinear filtering in original UV space (including repeat seams).
                q=coord*np.array([tw,th])-.5; ij=np.floor(q).astype(int);f=q-ij
                def at(dx,dy):
                    ix=ij[:,:,0]+dx;iy=ij[:,:,1]+dy
                    ix=np.clip(ix,0,tw-1) if clamp_u else ix%tw
                    iy=np.clip(iy,0,th-1) if clamp_v else iy%th
                    return tex[iy,ix]
                fx=f[:,:,0,None];fy=f[:,:,1,None]
                return (at(0,0)*(1-fx)+at(1,0)*fx)*(1-fy)+(at(0,1)*(1-fx)+at(1,1)*fx)*fy
            color=None
            if self.colour is not None:
                vc=np.ndarray((len(v),3),np.uint8,raw,offset=44,strides=(48,1))/127.5
                vc=np.clip(weights@vc[ids],.25,1.5)
                color=sample(tid,weights@uv[ids,0],b[10],b[11])[:,:,:3]*vc*np.array(struct.unpack_from('<3f',b,12))

            for l in layers:
                lid=struct.unpack_from('<I',l)[0];mode=l[4]
                if lid not in self.mapping or mode not in (5,6):continue
                tx=textures.get(lid)
                if tx is None:continue
                uvset=max(1,min(3,l[16]));coord=weights@uv[ids,uvset]
                tw,th,pixels=tx;rgba=np.frombuffer(pixels,np.uint8).reshape(th,tw,4)
                ix=np.floor(coord[:,:,0]*tw).astype(int);iy=np.floor(coord[:,:,1]*th).astype(int)
                ix=np.clip(ix,0,tw-1) if l[17] else ix%tw
                iy=np.clip(iy,0,th-1) if l[18] else iy%th
                blend=np.full(x.shape,l[5]/128) if mode==6 else rgba[iy,ix,3]/255*(1 if l[7] else va)
                blend=np.clip(blend,0,1)[...,None]
                rgb=rgb*(1-blend)+np.array(self.mapping[lid])*blend
                if color is not None:
                    layer=sample(lid,coord,l[17],l[18])
                    cb=np.clip(l[5]/128 if mode==6 else layer[:,:,3]*(1 if l[7] else va),0,1)
                    cb=np.broadcast_to(cb,x.shape)[...,None]
                    color=color*(1-cb)+layer[:,:,:3]*vc*np.array(struct.unpack_from('<3f',l,19))*cb
            dest=self.pixels[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
            dest[inside,:3]=np.clip(rgb[inside]*255,0,255).astype(np.uint8);dest[inside,3]=255
            if color is not None:
                destColor=self.colour[lo[1]:hi[1]+1,lo[0]:hi[0]+1]
                # Decode textures before lighting/blending, then store an sRGB map.
                # Encoding a multiply of sRGB bytes instead double-darkens shadows.
                linear=np.maximum(color[inside],0)
                encoded=np.where(linear<=.0031308,linear*12.92,1.055*linear**(1/2.4)-.055)
                destColor[inside,:3]=np.clip(encoded*255,0,255).astype(np.uint8)
            region[inside]=h[inside];self.faces+=1

    def finish(self):
        return struct.pack('<4sII',b'RXSM',1,self.size)+self.pixels.tobytes()
