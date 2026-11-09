#!/usr/bin/env python3
"""build_vehicle_cmp.py -- rx_canon_cmp.py (same arguments and output) against a SNAPSHOT of the original's memory taken right after
the construction (core/notes/construction.md section 15: the vehicles run race / intro frames before the world capture, so the
captured world is not the constructed state).

  --snap <image>   an RXWI image of the original's memory at the moment of interest (oracle/build_vehicle_trace.cpp
                   REFLEX_VTRACE_SNAP); the original's node boundaries still come from its canonical dump + world image (--orig),
                   the words from the snapshot.
Pointers are derived from addresses on BOTH sides (a word inside a heap node of that side is a pointer to it at that offset),
so leaf nodes and words written after the capture need no special rules. Everything else is rx_canon_cmp.
"""
import os, sys, struct
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build_map as bm
src = open(os.path.join(HERE, 'rx_canon_cmp.py')).read()
OLD = """def words(side, n):
    cb = side['cb']
    w = list(struct.unpack_from('<%dI' % n['nw'], cb, n['words_at']))
    p = {wi: (kind, target, toff) for wi, kind, target, toff in n['ptrs']}
    return w, p"""
NEW = """def words(side, n):
    cb = side['cb']
    w = list(struct.unpack_from('<%dI' % n['nw'], cb, n['words_at']))
    src = side.get('snap') if side.get('snap') is not None else side.get('pages')
    if src is not None:   # the words from the image (the canonical dump stores pointer words as references, not values)
        for k in range(len(w)):
            a = n['addr'] + 4 * k; pg = src.get(a & ~0xfff)
            w[k] = struct.unpack_from('<I', pg, a & 0xfff)[0] if pg else 0
    p = {}
    for k, v in enumerate(w):
        if v < 0x01000000: continue
        t, off = node_at(side, v)
        if t is not None: p[k] = (1, t['name'], off)
    return w, p

def word_img(side, a):
    pages = side.get('snap') if side.get('snap') is not None else side['pages']
    p = pages.get(a & ~0xfff) if pages else None
    return None if p is None else struct.unpack_from('<I', p, a & 0xfff)[0]"""
assert OLD in src, 'rx_canon_cmp.py changed: update build_vehicle_cmp.py'
src = src.replace(OLD, NEW)
src = src.replace("""def word_img(side, a):
    p = side['pages'].get(a & ~0xfff) if side['pages'] else None
    return None if p is None else struct.unpack_from('<I', p, a & 0xfff)[0]
""", "")
src = src.replace("    O = load(*orig); C = load(*core)", "    O = load(*orig); C = load(*core); O['snap'] = SNAP")
SNAP = None
if '--snap' in sys.argv:
    i = sys.argv.index('--snap'); SNAP = bm.image_pages(sys.argv[i + 1]); del sys.argv[i:i + 2]
g = {'__name__': 'build_vehicle_cmp', '__file__': os.path.join(HERE, 'rx_canon_cmp.py'), 'SNAP': SNAP}
exec(compile(src, 'rx_canon_cmp.py(build_vehicle_cmp)', 'exec'), g)
sys.exit(g['main']())
