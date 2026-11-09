#!/usr/bin/env python3
"""build_world_cmp.py -- checks of construction units 3 / 5 that rx_canon_cmp.py cannot make (core/notes/construction.md section 10).

  build_world_cmp.py statics <orig world> <orig physworld> <core world> <core physworld> <anchors out>
      The physics worlds' static objects (world + 0x19c0, 0x40 bytes) aligned on (position, quaternion) with difflib: per pair the
      property id, the shape compared deeply (type, flags, the triangles / composite children / box / cylinder / sphere data, its
      reference count), the tail (+0x34..+0x38), the record's index; the static grid's boxes and cells under the index mapping.
      Writes A:<orig entity>=<core entity> anchors (each pair's +0x30) for rx_canon_cmp.py. (The static vector is one leaf node of the
      canonical dump: its pointers are not followed there.)
  build_world_cmp.py deform <core world> <core terrain entity> <orig deform.bin>
      The deformation surface's bits (terrain entity + 0x90 + 0x240, an rx_d3d surface) against the boot's c-deform.bin.
"""
import collections, difflib, os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_map as bm

def loader(path):
    P = bm.image_pages(path)
    def rd(a, n):
        out = bytearray()
        while n:
            pg = a & ~0xfff; o = a & 0xfff; k = min(n, 4096 - o); out += P.get(pg, bytes(4096))[o:o + k]; a += k; n -= k
        return bytes(out)
    def w(a): return struct.unpack('<I', rd(a, 4))[0]
    return rd, w

VT = {0x976608: 'Triangles', 0x9766bc: 'StaticBox', 0x976728: 'StaticCylinder', 0x976674: 'Sphere', 0x9767a0: 'StaticComposite',
      0x9765c0: 'Unknown'}

def shape_sig(rd, w, sh, child=False):
    t = VT.get(w(sh), hex(w(sh)))
    base = (t, w(sh + 4) if child else None, w(sh + 8), w(sh + 0xc), w(sh + 0x10))
    if t == 'Triangles': return base + (w(sh + 0x20), rd(w(sh + 0x24), 0x30 * w(sh + 0x20)), rd(sh + 0x30, 0x10))
    if t == 'StaticComposite':
        def vec(o): b = w(sh + o + 0xc); e = w(sh + o + 0x10); return rd(b, e - b) if e > b else b''
        kids = tuple(shape_sig(rd, w, w(a), True) for a in range(w(sh + 0x2c), w(sh + 0x30), 4))
        return base + (vec(0x38), vec(0x50), vec(0x68), vec(0x80), vec(0x98), kids)
    if t in ('StaticBox', 'StaticCylinder'): return base + (rd(sh + 0x20, 0x50),)
    if t in ('Sphere', 'Unknown'): return base + (rd(sh + 0x20, 0x10),)
    return base

def statics(path, world):
    rd, w = loader(path)
    out = []
    for a in range(w(world + 0x19c0), w(world + 0x19c4), 0x40):
        sh = w(a + 0x28); r = w(a + 0x2c)
        out.append(dict(key=rd(a, 0x20), mat=w(a + 0x20), sig=shape_sig(rd, w, sh), shref=w(sh + 4), tail=rd(a + 0x34, 5),
                        ent=w(a + 0x30), index=w(r + 8) if r else None))
    return out, rd, w, w(w(world + 0x19cc) + 0x30)

def cmd_statics(a):
    o, ord_, ow, og = statics(a[0], int(a[1], 16)); c, crd, cw, cg = statics(a[2], int(a[3], 16))
    print('static objects: original %d, core %d' % (len(o), len(c)))
    sm = difflib.SequenceMatcher(None, [x['key'] for x in o], [x['key'] for x in c], autojunk=False)
    pairs = []
    for op in sm.get_opcodes():
        if op[0] == 'equal': pairs += [(op[1] + k, op[3] + k) for k in range(op[2] - op[1])]
        else:
            ents = ['%08x(vt %08x)' % (o[k]['ent'], ow(o[k]['ent'])) for k in range(op[1], op[2])]
            print('  unpaired: original [%d, %d) core [%d, %d) original entities %s' % (op[1], op[2], op[3], op[4], ' '.join(ents)))
    diffs = collections.Counter(); ex = {}
    for i, j in pairs:
        for f in ('mat', 'sig', 'tail', 'shref'):
            if o[i][f] != c[j][f]:
                diffs[f] += 1
                ex.setdefault(f, (i, j, o[i][f] if f != 'sig' else o[i]['sig'][:5], c[j][f] if f != 'sig' else c[j]['sig'][:5]))
        if o[i]['index'] != i or c[j]['index'] != j: diffs['record index'] += 1
    print('  paired %d (position + quaternion identical); field differences %s' % (len(pairs), dict(diffs) or 'none'))
    for f, v in ex.items(): print('    first %s difference: original #%d %s / core #%d %s' % (f, v[0], v[2], v[1], v[3]))
    box = lambda rd, w, g, i: rd(w(g + 0x41c) + 16 * i, 16) + rd(w(g + 0x420) + 16 * i, 16)
    print('  static grid boxes differing: %d of %d' % (sum(1 for i, j in pairs if box(ord_, ow, og, i) != box(crd, cw, cg, j)), len(pairs)))
    hdr = [(k, ow(og + k), cw(cg + k)) for k in list(range(0x400, 0x41c, 4)) + [0x478]] + \
          [(k, ow(og + k), cw(cg + k)) for k in range(0x430, 0x470, 4)]
    print('  static grid header differing words: %s' % ([('+%x' % k, '%08x/%08x' % (x, y)) for k, x, y in hdr if x != y] or 'none'))
    m = {i: j for i, j in pairs}; per = ow(og + 0x414); dc = 0
    for cell in range(ow(og + 0x410)):
        lo = [ow(ow(og + 0x470) + 4 * (cell * per + k)) for k in range(min(ow(ow(og + 0x474) + 4 * cell), per))]
        lc = [cw(cw(cg + 0x470) + 4 * (cell * per + k)) for k in range(min(cw(cw(cg + 0x474) + 4 * cell), per))]
        if sorted(m[x] for x in lo if x in m) != sorted(lc): dc += 1
    print('  static grid cells differing (indices mapped, unpaired objects left out): %d of %d' % (dc, ow(og + 0x410)))
    seen = set()
    with open(a[4], 'w') as f:
        for i, j in pairs:
            if o[i]['ent'] not in seen: seen.add(o[i]['ent']); f.write('A:%08x=%08x\n' % (o[i]['ent'], c[j]['ent']))
    print('  entity anchors: %d' % len(seen))

def cmd_deform(a):
    rd, w = loader(a[0])
    surf = w(int(a[1], 16) + 0x90 + 0x240); lt = w(surf + 0x24); bits = w(lt); n = w(lt + 8)
    core = rd(bits, n); orig = open(a[2], 'rb').read()
    d = sum(1 for i in range(0, min(n, len(orig)), 4) if core[i:i + 4] != orig[i:i + 4])
    print('deformation surface: %d bytes (original %d), differing words %d' % (n, len(orig), d))

if __name__ == '__main__':
    {'statics': cmd_statics, 'deform': cmd_deform}[sys.argv[1]](sys.argv[2:])
