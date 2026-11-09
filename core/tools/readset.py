#!/usr/bin/env python3
"""readset.py <run.rmap> <run.dump> [top N] -- what a race reads (construction scope of the OPEN REFLEX core).
<run.rmap>: reflex_boot REFLEX_READSET (1 bit per 16-byte granule of the 4 GB space). <run.dump>: REFLEX_MEMDUMP at the end of the
same run (pages without the stacks). Attributes every read granule to: the executable's image (code / .rdata constants / .data
globals), or a heap object: the nearest vtable pointer at or below it within 0x4000 bytes (class from _re/out/classes.tsv), else
'untyped heap'. Prints per class the objects and bytes read, and the .data globals read."""
import bisect, collections, os, struct, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '_re', 'out')
vt = {}
for l in open(os.path.join(R, 'classes.tsv'), encoding='latin-1').readlines()[1:]:
    c = l.rstrip('\n').split('\t')
    try: vt[int(c[1], 16)] = c[0]
    except (ValueError, IndexError): pass
rmap = open(sys.argv[1], 'rb').read()
mem = {}
d = open(sys.argv[2], 'rb').read()
for i in range(0, len(d), 4100):
    a = struct.unpack_from('<I', d, i)[0]; mem[a] = d[i + 4:i + 4100]
def word(a):
    p = mem.get(a & ~0xfff)
    return struct.unpack_from('<I', p, a & 0xfff)[0] if p else None
gran = []
for byte_i, b in enumerate(rmap):
    if b:
        for k in range(8):
            if b >> k & 1: gran.append((byte_i * 8 + k) * 16)
print(len(gran), 'granules read =', len(gran) * 16, 'bytes')
img = collections.Counter(); objs = collections.defaultdict(set); bytes_ = collections.Counter(); untyped = 0
glob = []
for a in gran:
    if 0x400000 <= a < 0x916000: img['code'] += 16; continue
    if 0x916000 <= a < 0x9a6000: img['rdata'] += 16; continue
    if 0x9a6000 <= a < 0xdd0000: img['data'] += 16; glob.append(a); continue
    if 0x78400000 <= a < 0x78600000: img['crt dll'] += 16; continue
    found = None
    for back in range(0, 0x4000, 4):
        w = word((a - back) & ~3)
        if w in vt: found = ((a - back) & ~3, vt[w]); break
    if found: objs[found[1]].add(found[0]); bytes_[found[1]] += 16
    else: untyped += 16
print('image:', dict(img), 'untyped heap bytes:', untyped)
top = int(sys.argv[3]) if len(sys.argv) > 3 else 60
print('%-60s %6s %8s' % ('class', 'objs', 'bytes'))
for c, n in bytes_.most_common(top): print('%-60s %6d %8d' % (c[:60], len(objs[c]), n))
print(len(bytes_), 'classes;', sum(len(v) for v in objs.values()), 'objects')
