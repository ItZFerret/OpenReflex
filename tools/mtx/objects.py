"""Recover object ownership lost by the Unleashed material-batched render cache.
Reads the user's extracted MTX PRE files; no original assets are checked in.
"""
import gzip, struct, re
import numpy as np
from scenery import Reader

def pre(path):
    b=path.read_bytes()
    if b[:2]==b'\x1f\x8b': b=gzip.decompress(b)
    r=Reader(b);r.take(8); count=r.integer(); out={}
    for _ in range(count):
        size,packed,nl,_=r.unpack('4I');name=r.take(nl).split(b'\0')[0].decode().replace('\\','/').lower();data=r.take((packed or size)+3 & ~3)[:packed or size]
        if packed:
            ring=bytearray(4096);at=4078;p=0;flags=0;dst=bytearray()
            while len(dst)<size:
                flags>>=1
                if not flags&256: flags=data[p]|0xff00;p+=1
                if flags&1: c=data[p];p+=1;dst.append(c);ring[at]=c;at=(at+1)&4095
                else:
                    pos=data[p]|((data[p+1]&240)<<4);n=(data[p+1]&15)+3;p+=2
                    for k in range(n):
                        if len(dst)==size:break
                        c=ring[(pos+k)&4095];dst.append(c);ring[at]=c;at=(at+1)&4095
            data=bytes(dst)
        out[name]=data
    return out

def sectors(b, topology=None):
    r=Reader(b);r.take(12)
    for _ in range(r.integer()):
        r.take(8);np_=r.integer();r.take(4+1+4+1+1+4)
        if r.take(1)[0]:r.take(8)
        if r.unpack('f')[0]>0:r.take(12)
        for _ in range(np_):
            _,flags=r.unpack('II');r.take(1+12+8+8+8+4)
            if flags&1:r.take(32)
            if flags&2:
                for _ in range(r.integer()):n=r.integer();r.take(4+n*8)
            if flags&0x800:n=r.integer();r.take(12+n*8)
            r.take(16)
    out={}
    for _ in range(r.integer()):
        ck,_,flags,nm=r.unpack('4I');r.take(40)
        if flags&0x800000:r.take(40)
        nv=r.integer();r.take(4);v=np.frombuffer(r.take(nv*12),'<f4').reshape(-1,3).copy();v/=12;v[:,2]*=-1
        if flags&4:r.take(nv*12)
        if flags&0x10:r.take(nv*12)
        if flags&1:r.take(r.integer()*nv*8)
        if flags&2:r.take(nv*4)
        if flags&0x800:r.take(nv)
        faces=[]
        for _ in range(nm):
            r.take(48)
            for lod in range(r.integer()):
                strip=np.frombuffer(r.take(r.integer()*2),'<u2')
                if lod==0:
                    faces.extend(tuple(map(int,strip[i:i+3])) for i in range(len(strip)-2) if len(set(strip[i:i+3]))==3)
        if topology is not None:topology[ck]=faces
        out[ck]=v
    return out

def collision_objects(b):
    _,count,nv,_,_,large,small=struct.unpack_from('<7I',b);vb=32+64*count;fb=(vb+large*12+small*6+nv+3)&~3
    out={}
    for i in range(count):
        o=32+64*i;ck=struct.unpack_from('<I',b,o)[0];nv,nf,sf,fixed,first=struct.unpack_from('<HHBBI',b,o+6);lo=np.array(struct.unpack_from('<3f',b,o+16),dtype=np.float32);start=struct.unpack_from('<I',b,o+48)[0]+vb
        v=np.frombuffer(b,'<u2' if fixed else '<f4',nv*3,start).reshape(-1,3).astype(np.float32)
        if fixed:v=v/16+lo
        v/=12;v[:,2]*=-1
        out[ck]=v
    return out

def definitions(folder,level,topology=None):
    files={}
    for suffix in ('Zpre','scnZpre','colZpre'):files.update(pre(folder/(level+suffix)))
    b=files['levels/'+level.lower()+'/'+level.lower()+'.scn.xbx']
    s=sectors(b,topology);c=collision_objects(next(v for k,v in files.items() if k.endswith('.col.xbx')))
    names={}
    for k,b in files.items():
        if k.endswith('.qb'):
            for m in re.finditer(rb'\x2b(.{4})([ -~]{1,64})\x00',b,re.S):names[struct.unpack('<I',m[1])[0]]=m[2].decode().lower()
    return s,c,names

def connected_parts(vertices,faces):
    """Connected cards, welding seam duplicates but never joining separated spectators."""
    parent=list(range(len(vertices)))
    def root(i):
        while parent[i]!=i:parent[i]=parent[parent[i]];i=parent[i]
        return i
    def join(a,b):parent[root(a)]=root(b)
    seen={};used=set()
    for face in faces:
        used.update(face)
        for i in face[1:]:join(face[0],i)
    for i in sorted(used):
        key=tuple(np.rint(vertices[i].astype(float)*1000).astype(np.int64))
        if key in seen:join(i,seen[key])
        seen[key]=i
    parts={}
    for i in sorted(used):parts.setdefault(root(i),[]).append(i)
    return list(parts.values())

def spectator_parts(vertices,faces):
    # Stadium seating meshes also use "crowd" names. Only body-sized connected
    # components receive person pivots; grandstand surfaces must stay attached.
    for ids in connected_parts(vertices,faces):
        size=np.ptp(vertices[ids],axis=0)
        if 2<=size[1]<=16 and max(size[0],size[2])<=min(16,size[1]*2):yield ids

class ObjectAnchors:
    def __init__(self,folder,level,center):
        topology={}
        sectors,coll,names=definitions(folder,level,topology);self.center=center;self.maps=[];self.anchors={};families={}
        # A people sector is a crowd distributed across a hillside, not one prop.
        # Split by actual mesh connectivity so each spectator keeps their own foot position.
        self.crowd=[];crowd_keys=set()
        for ck in list(sectors):
            if not any(word in names.get(ck,'') for word in ('people','crowd')):continue
            vertices=sectors.pop(ck);collision=coll.pop(ck,None);name=names.pop(ck)
            person_ids={tuple(ids) for ids in spectator_parts(vertices,topology[ck])}
            for part,ids in enumerate(connected_parts(vertices,topology[ck])):
                key=(ck,part);v=vertices[ids];sectors[key]=v;names[key]=name+' spectator '+str(part)
                if collision is not None:
                    select=np.array([np.min(np.max(np.abs(v-p),axis=1))<.001 for p in collision],dtype=bool)
                    coll[key]=collision[select]
                if tuple(ids) in person_ids:self.crowd.append(v.copy());crowd_keys.add(key)
        # Structures tied to the course (fences, bridge/decks, terrain, backdrops)
        # retain track scaling. Free-standing props use their original physical size.
        prop=re.compile(r'(^bale_|^cone_|tent|snackbar|entranceshack|watertank|watertower|pickuptruck|(?:^|_)rv(?:_|$)|(?:^|_)speaker|(?:^|_)people|(?:^|_)crowd|(?:^|_)podium|^winnerspodium|tree_trunk|tree_leaves|boa_treetrunk|boa_treebranch|^oak_|^pinetree_|^sagebrush_|color_tree|tree_lp_|(?:^|_)tower|grandstand)')
        for ck,v in sectors.items():
            name=names.get(ck,'')
            if isinstance(ck,tuple) and ck not in crowd_keys:continue
            if (ck not in crowd_keys and not prop.search(name)) or 'shadow' in name or 'concretepad' in name:continue
            family=re.sub(r'tree_(trunk|leaves)', 'tree',name)
            family=re.sub(r'boa_tree(trunk|branch)','boa_tree',family)
            family=re.sub(r'^oak_(trunk|leaves)','oak',family)
            family=re.sub(r'(tower[0-9]+)b$',lambda m:m[1],family)
            if 'grandstand' in name:family='grandstand'
            if 'snackbar' in name:family='snackbar'
            if 'watertank' in name or 'watertower' in name:family='watertank'
            families.setdefault(family,[]).append(ck)
        # Signs are separate sectors even when bolted to a booth. Keep contained
        # attachments with the structure so shrinking the booth cannot strand its signs.
        for family,keys in families.items():
            if not re.search(r'tower|snackbar|entranceshack|grandstand|tent',family):continue
            vertices=np.concatenate([sectors[k] for k in keys]);lo=vertices.min(0)-1;hi=vertices.max(0)+1
            for ck,v in sectors.items():
                if ck in keys or not re.search(r'banner|adbord|sign|window',names.get(ck,'')):continue
                if np.all(v.min(0)>=lo) and np.all(v.max(0)<=hi):keys.append(ck)
        for keys in families.values():
            vertices=np.concatenate([sectors[k] for k in keys]);lo=vertices.min(axis=0);hi=vertices.max(axis=0);anchor=(lo+hi)/2;anchor[1]=lo[1];anchor[[0,2]]-=center[[0,2]]
            for k in keys:self.anchors[k]=np.array([*anchor,1],dtype='<f4')
        for source in (sectors,coll):
            lookup={}
            for ck,vs in source.items():
                if ck not in self.anchors:continue
                for v in vs:lookup.setdefault(self.key(v),[]).append((ck,v.copy()))
            self.maps.append(lookup)
        self.matched=[0,0];self.objects=set()
    @staticmethod
    def key(v):return tuple(np.rint(np.asarray(v,dtype=float)*1000).astype(np.int64))
    def for_mesh(self,vertices,indices,collision=False):
        lookup=self.maps[int(collision)];out=np.zeros((len(vertices),4),dtype='<f4')
        owners=[]
        from itertools import product
        neighbors=list(product((-1,0,1),repeat=3))
        for v in vertices:
            key=self.key(v);found=set()
            for delta in neighbors:
                for ck,p in lookup.get(tuple(key[k]+delta[k] for k in range(3)),()):
                    if np.max(np.abs(v-p))<.001:found.add(ck)
            owners.append(found)
        for tri in indices:
            common=owners[tri[0]]&owners[tri[1]]&owners[tri[2]]
            if not common:continue
            ck=min(common,key=str);anchor=self.anchors[ck]
            for i in tri:
                if out[i,3] and not np.array_equal(out[i],anchor):raise ValueError('Vertex shared by independent props')
                out[i]=anchor
            self.matched[int(collision)]+=1;self.objects.add(ck)
        return out

if __name__=='__main__':
    import pathlib,sys
    s,c,names=definitions(pathlib.Path(sys.argv[1]),sys.argv[2])
    for k,v in s.items():print(f'{k:08x}',names.get(k,'?'),np.round(np.ptp(v,axis=0),1).tolist())

