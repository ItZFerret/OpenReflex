#!/usr/bin/env python3
"""Convert an installed MTX national or supercross venue into the experimental Reflex mod format."""
import argparse, gzip, io, json, pathlib, struct
import numpy as np
from scenery import convert_world, convert_collision
from surfaces import SurfaceBake
from catalog import VENUES, surfaces
from racing import race_payload

def convert(source, output, name=None, track_id=None, objects_dir=None):
    name = name or (source/"import-report.txt").read_text().splitlines()[0].split(" · ")[0]
    track_id = track_id or ("mtx-mckenna-rock" if source.name == "southwick" else "mtx-" + source.name)
    data = (source / 'collision.msc').read_bytes()
    magic, version, *header = struct.unpack_from('<II12f3I', data)
    if (magic, version) != (0x3143534d, 1):
        raise ValueError('Unsupported MSC collision format')
    lo, hi, spawn, direction = [np.array(header[i:i+3]) for i in (0,3,6,9)]
    w, h, surface = header[12:]
    if not (2 <= w <= 8192 and 2 <= h <= 8192):
        raise ValueError('Invalid source grid')
    heights = np.frombuffer(data, '<f4', w*h, 68).reshape(h,w).copy()
    valid = np.isfinite(heights)
    if not valid.any(): raise ValueError('No source terrain')
    # Propagate the nearest available samples into holes; no missing heights in Reflex's collision grid.
    for _ in range(max(w,h)):
        if valid.all(): break
        old = valid.copy()
        for dz,dx in ((1,0),(-1,0),(0,1),(0,-1)):
            sy=slice(max(0,-dz),min(h,h-dz)); sx=slice(max(0,-dx),min(w,w-dx))
            ty=slice(max(0,dz),min(h,h+dz)); tx=slice(max(0,dx),min(w,w+dx))
            take=~valid[ty,tx] & old[sy,sx]
            heights[ty,tx][take]=heights[sy,sx][take]; valid[ty,tx][take]=True
    n, spacing = 2048, 2.0
    if max((hi-lo)[[0,2]]) > (n-1)*spacing: raise ValueError("Source exceeds the mod terrain extent")
    center=(lo+hi)/2
    x=np.arange(n)*spacing-(n-1)*spacing/2+center[0]
    z=(n-1)*spacing/2-np.arange(n)*spacing+center[2]
    u=np.clip((x-lo[0])/(hi[0]-lo[0])*(w-1),0,w-1)
    v=np.clip((z-lo[2])/(hi[2]-lo[2])*(h-1),0,h-1)
    ix=np.minimum(u.astype(int),w-2); iz=np.minimum(v.astype(int),h-2)
    fx=u-ix; fz=(v-iz)[:,None]
    grid=(heights[iz[:,None],ix]*(1-fx)+heights[iz[:,None],ix+1]*fx)*(1-fz)+(heights[iz[:,None]+1,ix]*(1-fx)+heights[iz[:,None]+1,ix+1]*fx)*fz
    spawn[[0,2]]-=center[[0,2]]
    output.parent.mkdir(parents=True,exist_ok=True)
    f=io.BytesIO()
    f.write(struct.pack('<4sIIf6f',b'RXM1',1,n,spacing,*spawn,*direction))
    f.write(grid.astype('<f4').tobytes())
    manifest=dict(format='reflex-mod-track',version=1,id=track_id,name=f'MTX {name} (Experimental)',source='MTX Mototrax / Unleashed converted cache',kind=VENUES[source.name][2],terrain='terrain.rxmod',materialPreset='NAT_Concord',units='feet',gridOrder='rows from +Z, columns from -X',experimental=True)
    race=(source/'race.mtx').read_bytes()
    if struct.unpack_from('<II',race)!=(0x4358544d,1): raise ValueError('Unsupported course format')
    count=struct.unpack_from('<I',race,12)[0]
    course=np.frombuffer(race,'<f4',count*5,16).reshape(count,5).copy()
    course[:,0]-=center[0]; course[:,2]-=center[2]
    manifest['course']=course.reshape(-1).tolist()
    if source.name in ('denver','saintlouis'):
        # These source free-ride starts face a closed gate/obstacle. Use a clear
        # authored course entry with enough run-up before the first jump.
        start_index=14 if source.name=='denver' else 0
        spawn=course[start_index,:3].copy();direction=course[start_index+1,:3]-course[start_index,:3];direction[1]=0
        direction/=np.linalg.norm(direction)
        gx=np.clip((spawn[0]+2047)/2,0,n-1);gz=np.clip((2047-spawn[2])/2,0,n-1)
        ix=min(int(gx),n-2);iz=min(int(gz),n-2);fx=gx-ix;fz=gz-iz
        spawn[1]=(grid[iz,ix]*(1-fx)+grid[iz,ix+1]*fx)*(1-fz)+(grid[iz+1,ix]*(1-fx)+grid[iz+1,ix+1]*fx)*fz+2
        f.seek(16);f.write(struct.pack('<6f',*spawn,*direction));f.seek(0,2)
        manifest['startPlacement']=dict(courseIndex=start_index,reason='clear run-up; source free-ride start obstructed')

    manifest['terrain']='embedded RXM1'
    surface=SurfaceBake(center) if source.name=='buddscreek' else None
    if source.name not in ('buddscreek','southwick'):surface=SurfaceBake(center,surfaces(source.name))
    objects=None
    if objects_dir is not None:
        from objects import ObjectAnchors
        objects=ObjectAnchors(objects_dir,VENUES[source.name][0],center)
    scenery,report=convert_world(source/'world.mtx',lo,hi,heights,center,surface,objects)
    if surface is not None: manifest['surfaceMap']=dict(size=2048,channels='grass,rough-soil,rock,authored-coverage',faces=surface.faces)
    collision,collision_triangles=convert_collision(data,w,h,center,objects)
    report['collisionTriangles']=collision_triangles
    if objects is not None: report['fixedSizeProps']=dict(objects=len(objects.objects),spectators=len(objects.crowd),renderFaces=objects.matched[0],collisionFaces=objects.matched[1])
    manifest['scenery']=report
    manifest['raceVersion']=1
    racing=race_payload(source)
    metadata=json.dumps(manifest,separators=(',',':')).encode()
    if len(metadata)>65536: raise ValueError('Package metadata exceeds 64 KiB')
    with output.open('xb') as raw, gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as compressed:
        compressed.write(struct.pack('<4sII',b'RXPK',4 if surface is not None and surface.colour is not None else 3 if surface is not None else 2,len(metadata)))
        compressed.write(metadata)
        compressed.write(f.getvalue())
        compressed.write(struct.pack('<4sI',b'SCNE',len(scenery)))
        compressed.write(scenery)
        compressed.write(struct.pack('<4sI',b'COLL',len(collision)))
        compressed.write(collision)
        if surface is not None:
            payload=surface.finish();compressed.write(struct.pack("<4sI",b"SURF",len(payload)));compressed.write(payload)
        if surface is not None and surface.colour is not None:
            payload=struct.pack('<4sII',b'RXCM',1,2048)+surface.colour.tobytes()
            compressed.write(struct.pack('<4sI',b'COLR',len(payload)));compressed.write(payload)
        compressed.write(struct.pack('<4sI',b'RACE',len(racing)));compressed.write(racing)
    print(json.dumps(dict(samples=n*n,min=float(grid.min()),max=float(grid.max()),spawn=spawn.tolist(),output=str(output),scenery=report)))

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('source',type=pathlib.Path); p.add_argument('output',type=pathlib.Path)
    p.add_argument("--name"); p.add_argument("--id",dest="track_id")
    p.add_argument("--objects-dir",type=pathlib.Path,help="Extracted original MTX PRE archives, to preserve prop dimensions")
    a=p.parse_args(); convert(a.source,a.output,a.name,a.track_id,a.objects_dir)
