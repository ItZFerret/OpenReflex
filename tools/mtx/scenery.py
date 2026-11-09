"""Repack MTX cache scenery as RXSC, omitting the surface replaced by Reflex terrain."""
import io, struct
import numpy as np

class Reader:
    def __init__(self,data): self.data=data; self.p=0
    def take(self,n):
        if n<0 or self.p+n>len(self.data): raise ValueError('Truncated scenery')
        b=self.data[self.p:self.p+n];self.p+=n;return b
    def unpack(self,fmt): return struct.unpack('<'+fmt,self.take(struct.calcsize('<'+fmt)))
    def integer(self): return self.unpack('i')[0]
    def string(self):
        n=0
        for shift in range(0,35,7):
            v=self.take(1)[0];n|=(v&127)<<shift
            if v<128: return self.take(n).decode('utf8')
        raise ValueError('Invalid string')

def convert_world(path,lo,hi,heights,center,surface=None,objects=None):
    r=Reader(path.read_bytes())
    if r.unpack('II')!=(0x57585452,7): raise ValueError('Unsupported world cache')
    textures=[]; surface_textures={}
    for _ in range(r.integer()):
        start=r.p;tid,w,h=r.unpack('Iii')
        if not(0<w<=4096 and 0<h<=4096): raise ValueError('Invalid texture')
        pixels=r.take(w*h*4); textures.append(r.data[start:r.p]); surface_textures[tid]=(w,h,pixels)
    materials=[]; alpha=[]; surface_materials=[]
    for _ in range(r.integer()):
        start=r.p;b=r.take(34); layers=b[33]; layer_data=[r.take(31) for _ in range(layers)]; surface_materials.append((b,layer_data))
        materials.append(r.data[start:r.p]);alpha.append(bool(b[6]) and b[5]>=16)
    groups=[];placed=[];names=[]
    for _ in range(r.integer()):
        start=r.p;name=r.string();r.take(1);has=r.take(1)[0];names.append(name);placed.append(bool(has))
        prefix=r.data[start:r.p]
        if has:
            pos=np.array(r.unpack('3f'));pos[[0,2]]-=center[[0,2]]
            groups.append(prefix+struct.pack('<3f',*pos)+r.take(16))
        else: groups.append(prefix)
    draws=[];removed=0;kept=0;billboards=0;sky_count=0
    h,w=heights.shape
    def sample(pos):
        u=np.clip((pos[:,0]-lo[0])/(hi[0]-lo[0])*(w-1),0,w-1)
        v=np.clip((pos[:,2]-lo[2])/(hi[2]-lo[2])*(h-1),0,h-1)
        ix=np.minimum(u.astype(int),w-2);iz=np.minimum(v.astype(int),h-2);fx=u-ix;fz=v-iz
        return (heights[iz,ix]*(1-fx)+heights[iz,ix+1]*fx)*(1-fz)+(heights[iz+1,ix]*(1-fx)+heights[iz+1,ix+1]*fx)*fz
    for _ in range(r.integer()):
        mi,sky,billboard,group,nv,ni=r.unpack('i??iii');stride=68 if billboard else 48
        if not(0<=mi<len(materials) and 0<nv<=4000000 and ni%3==0): raise ValueError('Invalid draw')
        raw=bytearray(r.take(nv*stride));vertices=np.ndarray((nv,3),'<f4',raw,strides=(stride,4))
        indices=np.frombuffer(r.take(ni*4),'<i4').reshape(-1,3).copy()
        if np.any(indices<0) or np.any(indices>=nv):raise ValueError('Invalid index')
        local=group>=0 and placed[group]
        if not(sky or billboard or alpha[mi] or local):
            tri=vertices[indices];mid=tri.mean(axis=1);normal=np.cross(tri[:,1]-tri[:,0],tri[:,2]-tri[:,0]);length=np.linalg.norm(normal,axis=1)
            near=np.max(np.abs(tri[:,:,1]-sample(tri.reshape(-1,3)).reshape(-1,3)),axis=1)<3
            inside=np.all((tri[:,:,[0,2]]>=lo[[0,2]]) & (tri[:,:,[0,2]]<=hi[[0,2]]),axis=(1,2))
            ground=near & inside & (np.abs(normal[:,1])>length*.25) & (np.abs(mid[:,1]-sample(mid))<3)
            if surface is not None and surface.colour is not None and not surface.accepts(surface_materials[mi]): ground[:]=False
            if surface is not None: surface.draw(surface_materials[mi],raw,indices if surface.colour is not None and surface.accepts(surface_materials[mi]) else indices[ground],surface_textures)
            removed+=int(ground.sum());indices=indices[~ground]
        if not len(indices):continue
        anchors=np.zeros((nv,4),dtype="<f4")
        if objects is not None and not(local or sky):
            counts=objects.matched.copy();owners=objects.objects.copy()
            try:anchors=objects.for_mesh(vertices,indices)
            except ValueError as e:
                if str(e)!='Vertex shared by independent props':raise
                # Cache batching can weld corners of separate original objects. Split
                # draw vertices by triangle before assigning independent prop pivots.
                objects.matched=counts;objects.objects=owners
                packed=np.frombuffer(raw,np.uint8).reshape(nv,stride)
                raw=bytearray(packed[indices.reshape(-1)].tobytes());nv=indices.size
                vertices=np.ndarray((nv,3),'<f4',raw,strides=(stride,4))
                indices=np.arange(nv,dtype=np.int32).reshape(-1,3)
                anchors=objects.for_mesh(vertices,indices)
        if not(local or sky):
            vertices[:,0]-=center[0];vertices[:,2]-=center[2]
            if billboard:
                origins=np.ndarray((nv,3),'<f4',raw,offset=48,strides=(stride,4));origins[:,0]-=center[0];origins[:,2]-=center[2]
        draws.append(struct.pack('<i??iii',mi,sky,billboard,group,nv,indices.size)+raw+indices.astype('<i4').tobytes()+(anchors.tobytes() if objects is not None else b''))
        kept+=len(indices);billboards+=bool(billboard);sky_count+=bool(sky)
    if r.p!=len(r.data): raise ValueError('Unexpected world cache bytes')
    out=io.BytesIO();out.write(struct.pack('<4sI',b'RXSC',3 if objects is not None else 2))
    for collection in (textures,materials,groups,draws):
        out.write(struct.pack('<I',len(collection)))
        for b in collection:out.write(b)
    clips=[]
    for file in sorted((path.parent/'objanims').glob('*.oan')):
        ar=Reader(file.read_bytes())
        if ar.unpack('II')!=(0x4e414f4d,1):raise ValueError('Unsupported object animation')
        duration,count=ar.unpack('fi'); tracks=[]
        if not(0<duration<86400 and 0<=count<=10000):raise ValueError('Invalid animation')
        for _ in range(count):
            name=ar.string();nq=ar.integer();q=np.frombuffer(ar.take(nq*20),'<f4').reshape(nq,5).copy();nt=ar.integer()
            pos=np.frombuffer(ar.take(nt*16),'<f4').reshape(nt,4).copy()
            # Some original clips contain out-of-order timestamps. Sort complete
            # key records together so the runtime can safely interpolate them.
            if not np.isfinite(pos).all() or not np.isfinite(q).all():raise ValueError('Invalid animation keys')
            q=q[np.argsort(q[:,0],kind='stable')];pos=pos[np.argsort(pos[:,0],kind='stable')]
            if (q[:,0]<0).any() or (q[:,0]>duration).any() or (pos[:,0]<0).any() or (pos[:,0]>duration).any():raise ValueError('Animation keys exceed clip duration')
            pos[:,1]-=center[0];pos[:,3]-=center[2]
            if name in names:
                tracks.append(struct.pack('<ii',names.index(name),nq)+q.astype('<f4').tobytes()+struct.pack('<i',nt)+pos.astype('<f4').tobytes())
        if ar.p!=len(ar.data):raise ValueError('Unexpected animation bytes')
        clips.append(struct.pack('<fi',duration,len(tracks))+b''.join(tracks))
    out.write(struct.pack('<I',len(clips)))
    for clip in clips:out.write(clip)
    return out.getvalue(),dict(animations=len(clips),draws=len(draws),triangles=kept,terrainTrianglesRemoved=removed,textures=len(textures),billboards=billboards,skies=sky_count,groups=names)

def convert_collision(data,w,h,center,objects=None):
    r=Reader(data);r.p=68+w*h*4
    chunks=[];triangles=0
    for _ in range(r.integer()):
        nv,nt=r.unpack('ii')
        if not(0<nv<=48000 and 0<nt<=16000):raise ValueError('Invalid collision chunk')
        vertices=np.frombuffer(r.take(nv*16),'<f4').reshape(nv,4)[:,:3]
        indices=np.frombuffer(r.take(nt*3*4),'<i4')
        if np.any(indices<0) or np.any(indices>=nv):raise ValueError('Invalid collision index')
        points=vertices[indices].copy()
        anchors=objects.for_mesh(points,np.arange(len(points)).reshape(-1,3),True) if objects is not None else None
        points[:,0]-=center[0];points[:,2]-=center[2]
        if not np.isfinite(points).all():raise ValueError('Invalid collision vertex')
        chunks.append(struct.pack('<I',len(points))+points.astype('<f4').tobytes()+(anchors.tobytes() if objects is not None else b''));triangles+=nt
    if r.p!=len(data):raise ValueError('Unexpected collision bytes')
    return struct.pack('<4sII',b'RXCO',2 if objects is not None else 1,len(chunks))+b''.join(chunks),triangles
