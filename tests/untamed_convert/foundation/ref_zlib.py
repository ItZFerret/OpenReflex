# tests/untamed_convert/foundation/ref_zlib.py <out file> <Reflex install> <extracted Untamed disc folder>
# Writes the md5 of zlib.compress(body) (level 6) for every BXML body of the stock SX_FortDodge / SX_Kingston scenes, databases and level
# files, of the 12 Untamed scenes / databases / level files, and of a few synthetic inputs; prints how many recompress to the stored bytes.
#   $UX_PYTHON -I -B ref_zlib.py <scratch>/ref/zlib.json <install> <disc folder>
import sys, json, hashlib, struct, zlib
sys.dont_write_bytecode = True
G = sys.argv[2].replace(chr(92), '/').rstrip('/') + '/Database/'
U = sys.argv[3].replace(chr(92), '/').rstrip('/') + '/Database/'
LEVELS = ['Norton','Capital_City','Columbus','Denton','HarrisPark','Foothills','WestStone','Jefferson','Wilford','BridgePort','Fairview','Lincoln']
out = []
def body(d, p=0):
    z = struct.unpack_from('<I', d, p + 32)[0]
    return zlib.decompress(d[p + 36:p + 36 + z]), d[p + 36:p + 36 + z]
def scene(path, big):
    d = open(path, 'rb').read(); e = '>' if big else '<'
    n = struct.unpack_from(e + 'I', d, 4)[0]; res = []
    for i in range(n):
        o = 8 + i * 264; off = struct.unpack_from(e + 'I', d, o + 260)[0]
        nch = struct.unpack_from(e + 'I', d, off)[0]; q = off + 8
        for _ in range(nch):
            b, z = body(d, q); c = zlib.compress(b)
            res.append([hashlib.md5(c).hexdigest(), int(c == z)]); q += 36 + len(z)
    return res
def doc(path):
    d = open(path, 'rb').read(); b, z = body(d); c = zlib.compress(b)
    return [[hashlib.md5(c).hexdigest(), int(c == z)]]
for lv in ('SX_FortDodge', 'SX_Kingston'):
    out.append(dict(file=lv + '.dx9.scene', kind='pcscene', md5=scene(G + lv + '.dx9.scene', False)))
    out.append(dict(file=lv + '.dx9.database', kind='pcdoc', md5=doc(G + lv + '.dx9.database')))
    out.append(dict(file=lv + '.dx9.level', kind='pcdoc', md5=doc(G + lv + '.dx9.level')))
for lv in LEVELS:
    out.append(dict(file=lv + '_STADIUM.xenon.scene', kind='xscene', md5=scene(U + lv + '_STADIUM.xenon.scene', True)))
    out.append(dict(file=lv + '_STADIUM.xenon.database', kind='xdoc', md5=doc(U + lv + '_STADIUM.xenon.database')))
    out.append(dict(file=lv + '_STADIUM.xenon.level', kind='xdoc', md5=doc(U + lv + '_STADIUM.xenon.level')))
# synthetic: sizes around the block / window limits; the generator is a 32-bit LCG both sides can run
def lcg(n, seed, mask):
    x = seed; o = bytearray(n)
    for i in range(n):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        o[i] = (x >> 24) & mask
    return bytes(o)
syn = []
for n, seed, mask in ((0, 1, 255), (1, 2, 255), (5, 3, 3), (300, 4, 1), (65535, 5, 7), (65536, 6, 255), (70000, 7, 15), (300000, 8, 3), (1000000, 9, 31), (40000, 10, 0)):
    syn.append([n, seed, mask, hashlib.md5(zlib.compress(lcg(n, seed, mask))).hexdigest()])
json.dump(dict(zlib=zlib.ZLIB_RUNTIME_VERSION, files=out, synthetic=syn), open(sys.argv[1], 'w'))
print(sum(len(o['md5']) for o in out), 'documents;', sum(m[1] for o in out for m in o['md5']), 'recompress to the stored bytes')
for o in out:
    if o['kind'] in ('pcscene', 'xscene'): print(o['file'], len(o['md5']), sum(m[1] for m in o['md5']))
