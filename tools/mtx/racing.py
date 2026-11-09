"""Add source race routes to an RXPK without touching its terrain or scenery."""
import gzip,json,math,pathlib,struct
from catalog import VENUES

def race_payload(source):
    raw=(source/'race.mtx').read_bytes()
    magic,version,flags,n=struct.unpack_from('<4I',raw)
    if (magic,version)!=(0x4358544d,1):raise ValueError('Unsupported MTX course')
    p=16
    lap=[list(struct.unpack_from('<5f',raw,p+20*i)) for i in range(n)];p+=20*n
    m=struct.unpack_from('<I',raw,p)[0];p+=4
    lead=[list(struct.unpack_from('<5f',raw,p+20*i)) for i in range(m)];p+=20*m
    gates=list(struct.unpack_from('<20f',raw,p));p+=80
    if p!=len(raw) or n<10 or m<2:raise ValueError('Missing closed lap / start chute')
    col=(source/'collision.msc').read_bytes();bounds=struct.unpack_from('<6f',col,8)
    cx=(bounds[0]+bounds[3])/2;cz=(bounds[2]+bounds[5])/2
    for points in (lap,lead):
        for q in points:q[0]-=cx;q[2]-=cz
    for i in (0,10):gates[i]-=cx;gates[i+2]-=cz
    # Short, nonzero segments give Reflex's AI and lap tracker a continuous corridor.
    def dense(points):
        out=[]
        for a,b in zip(points,points[1:]):
            d=math.hypot(b[0]-a[0],b[2]-a[2]);steps=max(1,math.ceil(d/10))
            if d<.01:continue
            for j in range(steps):out.append([x+(y-x)*j/steps for x,y in zip(a,b)])
        return out+[points[-1]]
    lap,lead=dense(lap),dense(lead)
    if max(len(lap),len(lead))>4096:raise ValueError('Course too large')
    kind=int(VENUES[source.name][2]=='Supercross')
    return struct.pack('<4s4I20f',b'RXRA',1,kind,len(lap),len(lead),*gates)+b''.join(struct.pack('<5f',*q) for q in lap+lead)

def upgrade(package,source,output):
    data=gzip.decompress(package.read_bytes());magic,v,n=struct.unpack_from('<4sII',data)
    meta=json.loads(data[12:12+n]);body=data[12+n:]
    if meta.get('raceVersion'):raise ValueError('Already has racing')
    payload=race_payload(source);meta['raceVersion']=1
    m=json.dumps(meta,separators=(',',':')).encode()
    if len(m)>65536:raise ValueError('Metadata size')
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(gzip.compress(struct.pack('<4sII',magic,v,len(m))+m+body+struct.pack('<4sI',b'RACE',len(payload))+payload,mtime=0))
    print(output.name,len(payload))

if __name__=='__main__':
    import sys
    root=pathlib.Path(sys.argv[1]);dest=pathlib.Path(sys.argv[2]);cache=pathlib.Path.home()/'Library/Application Support/DefaultCompany/MXvsATV/ImportedTracks/MTX'
    for key,(_,name,_) in VENUES.items():
        stem=''.join(c for c in name if c.isalnum())
        upgrade(root/(stem+'.rxtrack'),cache/key,dest/(stem+'.rxtrack'))
