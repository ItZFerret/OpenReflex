"""Reader validation against the generated package and damaged copies."""
import ctypes, gzip, json, pathlib, struct, subprocess, sys, tempfile
root=pathlib.Path(__file__).resolve().parents[2]
package=pathlib.Path(sys.argv[1])
with tempfile.TemporaryDirectory() as temp:
    temp=pathlib.Path(temp)
    library=temp/'reader.dylib'
    subprocess.run(['clang','-dynamiclib',str(root/'core/src/fmt_modtrack.c'),'-lz','-o',str(library)],check=True)
    reader=ctypes.CDLL(str(library)); reader.fmt_modtrack_open.argtypes=[ctypes.c_char_p];reader.fmt_modtrack_height.restype=ctypes.c_float
    assert reader.fmt_modtrack_open(bytes(package))==1
    raw=gzip.decompress(package.read_bytes()); metadata=struct.unpack_from('<I',raw,8)[0]; start=12+metadata
    for i in (0,2048*100+100,2048*1024+1024,2048*2048-1):
        assert reader.fmt_modtrack_height(i)==struct.unpack_from('<f',raw,start+40+4*i)[0]
    version=struct.unpack_from('<I',raw,4)[0]
    collision_offset=None
    if version>=2:
        scenery_offset=start+40+2048*2048*4
        scenery_size=struct.unpack_from('<I',raw,scenery_offset+4)[0]
        collision_offset=scenery_offset+8+scenery_size+8
        assert raw[collision_offset:collision_offset+4]==b'RXCO'
        chunks=struct.unpack_from('<I',raw,collision_offset+8)[0]
        assert reader.fmt_modtrack_collision_count()==chunks and chunks>0
        reader.fmt_modtrack_collision_chunk.argtypes=[ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32)]
        reader.fmt_modtrack_collision_chunk.restype=ctypes.POINTER(ctypes.c_float)
        reader.fmt_modtrack_collision_anchors.argtypes=[ctypes.c_uint32]
        reader.fmt_modtrack_collision_anchors.restype=ctypes.POINTER(ctypes.c_float)
        reader.fmt_modtrack_scale_point.argtypes=[ctypes.POINTER(ctypes.c_float),ctypes.POINTER(ctypes.c_float),ctypes.c_float,ctypes.POINTER(ctypes.c_float)]
        fixed_faces=0
        offset=collision_offset+12
        for chunk in range(chunks):
            count=ctypes.c_uint32(); points=reader.fmt_modtrack_collision_chunk(chunk,ctypes.byref(count))
            expected=struct.unpack_from('<I',raw,offset)[0];offset+=4
            assert count.value==expected
            for i in (0,expected*3-1):assert points[i]==struct.unpack_from('<f',raw,offset+i*4)[0]
            if struct.unpack_from('<I',raw,collision_offset+4)[0]==2:
                anchors=reader.fmt_modtrack_collision_anchors(chunk)
                for v in range(0,expected,3):
                    a=struct.unpack_from('<4f',raw,offset+expected*12+v*16)
                    if a[3]!=1:continue
                    fixed_faces+=1
                    for scale in (.5,1,1.6,1.9,2):
                        result=[]
                        for j in range(3):
                            index=v+j;point=(ctypes.c_float*3)(*points[index*3:index*3+3]);anchor=(ctypes.c_float*4)(*anchors[index*4:index*4+4]);out=(ctypes.c_float*3)()
                            assert tuple(anchor)==a
                            reader.fmt_modtrack_scale_point(point,anchor,scale,out)
                            assert all(abs(out[k]-(point[k]+a[k]*(scale-1)))<.001 for k in range(3))
                            result.append(list(out))
                        # Every edge retains its original size at every supported track scale.
                        assert all(abs((result[1][k]-result[0][k])-(points[(v+1)*3+k]-points[v*3+k]))<.001 for k in range(3))
            offset+=expected*(28 if struct.unpack_from("<I",raw,collision_offset+4)[0]==2 else 12)
        if struct.unpack_from('<I',raw,collision_offset+4)[0]==2:
            assert fixed_faces==json.loads(raw[12:12+metadata])['scenery']['fixedSizeProps']['collisionFaces'], 'Fixed-size collision faces differ from exporter report'
            print('PASS:',fixed_faces,'fixed-size collision faces at 50%, 100%, 160%, 190%, 200%')
        if version>=3:
            surface_offset=offset
            assert raw[offset:offset+4]==b'SURF'
            assert struct.unpack_from('<I',raw,offset+4)[0]==12+2048*2048*4
            assert raw[offset+8:offset+12]==b'RXSM'
            reader.fmt_modtrack_softness.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.POINTER(ctypes.c_float)]
            pixels=raw[offset+20:]
            for i in (0,1024*2048+1024,1234*2048+1150):
                out=(ctypes.c_float*3)();reader.fmt_modtrack_softness(i%2048,i//2048,out)
                grass=pixels[4*i]/255;rock=pixels[4*i+2]/255;dirt=1-grass-rock
                assert abs(out[0]-(.85*dirt+.3*grass+.05*rock))<1e-6
                assert abs(out[1]-(.65*dirt+.25*grass+.05*rock))<1e-6
            offset+=20+2048*2048*4
        if version>=4:
            colour_offset=offset
            assert raw[offset:offset+4]==b'COLR' and raw[offset+8:offset+12]==b'RXCM'
            assert struct.unpack_from('<I',raw,offset+16)[0]==2048
            offset+=20+2048*2048*4
        race_offset=offset
        if offset<len(raw):
            assert raw[offset:offset+4]==b'RACE' and reader.fmt_modtrack_racing()==1
            offset+=8+struct.unpack_from('<I',raw,offset+4)[0]
        assert offset==len(raw)
    reader.fmt_modtrack_recovery_retry.argtypes=[ctypes.c_uint32,ctypes.c_uint32,ctypes.c_float]
    retry=reader.fmt_modtrack_recovery_retry
    assert retry(1,100,20)==0 and retry(1,100,22)==0 and retry(1,100,24)==1
    assert retry(2,100,24)==0  # riders do not share retry state
    assert retry(1,200,26)==1 and retry(1,200,40)==0  # adjacent node / distant crash
    assert retry(1,200,0)==0  # restarting resets the recovery window
    reader.fmt_modtrack_lap_gate.argtypes=[ctypes.c_uint32,ctypes.c_int,ctypes.c_float]
    gate=reader.fmt_modtrack_lap_gate
    assert gate(1,-1,0)==1
    assert gate(1,0,.99)==0 and gate(1,0,.51)==0
    assert gate(1,0,.26)==0 and gate(1,0,.76)==0
    assert gate(1,0,.51)==0 and gate(1,0,.76)==1
    assert gate(1,1,.99)==0   # next lap needs its own ordered sectors
    assert gate(1,-1,0)==1 and gate(1,0,.99)==0  # restart clears progress
    reader.fmt_modtrack_clear()
    assert gate(1,0,.99)==0
    reader.fmt_modtrack_clear()
    broken={}
    b=bytearray(raw);struct.pack_into('<I',b,start+8,0xffffffff);broken['dimensions']=gzip.compress(b)
    b=bytearray(raw);struct.pack_into('<f',b,start+40,float('nan'));broken['nan']=gzip.compress(b)
    if version>=2:
        b=bytearray(raw);struct.pack_into('<I',b,scenery_offset+4,0xffffffff);broken['scenery length']=gzip.compress(b)
        b=bytearray(raw);struct.pack_into('<I',b,collision_offset+12,0xffffffff);broken['collision count']=gzip.compress(b)
        b=bytearray(raw);struct.pack_into('<f',b,collision_offset+16,float('nan'));broken['collision NaN']=gzip.compress(b)
    if version>=2 and struct.unpack_from('<I',raw,collision_offset+4)[0]==2:
        count=struct.unpack_from('<I',raw,collision_offset+12)[0];at=collision_offset+16+count*12
        b=bytearray(raw);struct.pack_into('<f',b,at,float('nan'));broken['anchor NaN']=gzip.compress(b)
        b=bytearray(raw);struct.pack_into('<f',b,at+12,.5);broken['anchor mode']=gzip.compress(b)
    if version>=3:
        b=bytearray(raw);struct.pack_into('<I',b,surface_offset+16,4096);broken['surface size']=gzip.compress(b)
        b=bytearray(raw);b[surface_offset+20:surface_offset+23]=b'\xff\xff\xff';broken['surface weights']=gzip.compress(b)
    if version>=4:
        b=bytearray(raw);struct.pack_into('<I',b,colour_offset+16,4096);broken['colour size']=gzip.compress(b)
    if race_offset<len(raw):
        for name,at,value,fmt in [('race count',race_offset+20,0xffffffff,'I'),('race NaN',race_offset+108,float('nan'),'f'),('race width',race_offset+108+12,-1,'f')]:
            b=bytearray(raw);struct.pack_into('<'+fmt,b,at,value);broken[name]=gzip.compress(b)
    broken['truncated']=gzip.compress(raw[:-4])
    b=bytearray(package.read_bytes());b[-8]^=1;broken['crc']=b
    for name,data in broken.items():
        path=temp/(name+'.rxtrack');path.write_bytes(data)
        assert reader.fmt_modtrack_open(bytes(path))==-1,name
        assert not reader.fmt_modtrack_active(),name
    print('PASS: terrain and collision data agree; malformed lengths, counts, NaN, truncation and CRC rejected')
