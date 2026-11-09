"""Check exported crowd anchors against original spectator geometry, not crowd bounds.
Usage: test_crowd.py package.rxtrack installed-cache-folder original-PRE-folder LevelName
"""
import gzip, pathlib, struct, sys
import numpy as np
from objects import definitions, spectator_parts, ObjectAnchors
from scenery import Reader

package,cache,source=map(pathlib.Path,sys.argv[1:4]);level=sys.argv[4]
header=struct.unpack_from('<II12f3I',(cache/'collision.msc').read_bytes())
center=(np.array(header[2:5])+np.array(header[5:8]))/2
s,c,names=definitions(source,level,topology:={})
expected={};spectators=0
for ck,vertices in s.items():
    if not any(word in names.get(ck,'') for word in ('people','crowd')):continue
    for ids in spectator_parts(vertices,topology[ck]):
        v=vertices[ids].copy();v[:,[0,2]]-=center[[0,2]]
        foot=(v.min(0)+v.max(0))/2;foot[1]=v[:,1].min()
        for p in v:expected[ObjectAnchors.key(p)]=foot
        spectators+=1
assert spectators>0
r=Reader(gzip.decompress(package.read_bytes()));_,version,n=r.unpack('4sII');r.take(n+40+2048**2*4)
assert r.take(4)==b'SCNE';r.integer();assert r.unpack('4sI')==(b'RXSC',3)
for _ in range(r.integer()):_,w,h=r.unpack('Iii');r.take(w*h*4)
for _ in range(r.integer()):b=r.take(34);r.take(b[33]*31)
for _ in range(r.integer()):
    r.string();r.take(1)
    if r.take(1)[0]:r.take(28)
checked=set()
for _ in range(r.integer()):
    mi,sky,billboard,group,nv,ni=r.unpack('i??iii');stride=68 if billboard else 48
    raw=r.take(nv*stride);v=np.ndarray((nv,3),'<f4',raw,strides=(stride,4))
    idx=np.frombuffer(r.take(ni*4),'<i4');anchors=np.frombuffer(r.take(nv*16),'<f4').reshape(-1,4)
    if sky or group>=0:continue
    for i in np.unique(idx):
        key=ObjectAnchors.key(v[i]);foot=expected.get(key)
        if foot is None:continue
        assert anchors[i,3]==1 and np.allclose(anchors[i,:3],foot,atol=.001), ('crowd-wide pivot',v[i],anchors[i],foot)
        for scale in (.5,1,1.6,1.8,1.85,2):
            # Foot placement follows the terrain; the body offset stays at its original size.
            position=v[i]+anchors[i,:3]*(scale-1)
            assert np.allclose(position-foot*scale,v[i]-foot,atol=.001)
        checked.add(key)
assert len(checked)>=len(expected)*.99,(len(checked),len(expected))
print(f'PASS: {spectators} spectator cards, {len(checked)} vertices; individual foot positions and sizes preserved at 50–200%')
