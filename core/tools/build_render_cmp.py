#!/usr/bin/env python3
"""build_render_cmp.py -- build_load_cmp.py (same arguments, same output) plus the D3D stand-in objects of core/src/rx_d3d.h.

The original's boot makes its D3D objects (textures, vertex / index buffers, declarations, shaders, constant tables) as small heap nodes
(a 32-byte block, a 16-byte one for an ID3DXConstantTable: word 0 a pointer to a vtable block of the emulator's device stand-ins, the rest 0:
their state lives host-side), the core's are 0x40-byte arena blocks whose word 0 is a vtable at 0x00e40000..0x00e47fff and whose other words
hold the object's state. They can never be equal word for word. A pair whose core node starts with an rx_d3d vtable and whose original is a
16 / 32-byte node with a pointer at word 0 is therefore taken as one D3D object (counted, not compared); the original's vtable block must have
the slot count of the core's interface (22 texture / cube texture, 17 surface, 14 buffer, 5 declaration / shader, 27 constant table, 119 device). A core
object paired with anything else is a difference.

build_load_cmp.py -- build_cmp.py plus pointer offsets compared relative to the payload of 128-aligned blocks (asset loaders, section 9).

The node set blocks of ModelAssets are _aligned_malloc(size, 0x80): the payload starts at an address-dependent pad after the raw malloc
block's start, so the offsets of pointers into them differ between two heaps. A pointer's offset is compared relative to the target block's
payload start (the first word of the block holding the block's own start address is the aligned allocation's back pointer: payload = that
word's offset + 4).

Original docstring of build_cmp.py follows.

build_cmp.py -- rx_canon_cmp.py (same arguments, same output) plus pointers inside LEAF nodes (core/notes/construction.md section 8).

The canonical dump stores data blocks (>= 64 KiB with pointer hits mostly not at block starts: big database records, the anim key
blocks of the new layout) as leaves: their pointer words are plain words, so rx_canon_cmp reports every pointer in them as a
difference (the addresses differ between the worlds). This wrapper runs rx_canon_cmp unchanged except for that case: two plain words
that differ, where the original's word is an address inside an original node and the core's word an address inside a core node at the
same offset of a node of the same size, are a pointer pair: the two targets are paired (queued for the walk like any pointer; a
target already paired with another node is a pairing conflict, i.e. a difference). The count of such pairs is printed. Without
--leaf-pointers it is exactly rx_canon_cmp.
"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(HERE, 'rx_canon_cmp.py')).read()
OLD = """            if po is None and pc is None:
                if (ow[k] ^ cw[k]) & dmask: bad.append('+%04x %08x / core %08x' % (4 * k, ow[k], cw[k]))"""
NEW = """            if po is None and pc is None:
                if (ow[k] ^ cw[k]) & dmask:
                    tn, to = node_at(O, ow[k]); un, uo = node_at(C, cw[k])
                    if LEAF and tn is not None and un is not None and to == uo and tn['size'] == un['size']:
                        LEAFS[0] += 1
                        if seen_o.get(tn['name'], un['name']) != un['name'] or seen_c.get(un['name'], tn['name']) != tn['name']:
                            bad.append('+%04x leaf pointer to another node' % (4 * k))
                        elif tn['name'] not in seen_o: queue.append((tn['name'], un['name'], '%s+%x(leaf)' % (label(O, on), 4 * k), 0))
                    else: bad.append('+%04x %08x / core %08x' % (4 * k, ow[k], cw[k]))"""
assert OLD in src, 'rx_canon_cmp.py changed: update build_cmp.py'
NEW = NEW.replace("to == uo and tn['size'] == un['size']:", "to == uo and (tn['size'] == un['size'] or (tn['size'] in (0x10, 0x20) and un['size'] == 0x40 and 0xe40000 <= words(C, un)[0][0] < 0xe48000)):")
src = src.replace(OLD, NEW)
OLD3 = "                if po[0] != pc[0] or po[2] != pc[2]: bad.append("
NEW3 = "                if po[0] != pc[0] or (po[2] - padof(O, po[1]) != pc[2] - padof(C, pc[1]) and not (po[2] == 0 and pc[2] == 0)) if po[0] == 1 else (po[0] != pc[0] or po[2] != pc[2]): bad.append("
assert OLD3 in src, 'rx_canon_cmp.py changed: update build_load_cmp.py'
src = src.replace(OLD3, NEW3)
PADDEF = '''
_PAD = {}
def padof(side, nm):
    key = (id(side), nm)
    if key in _PAD: return _PAD[key]
    n = side['nodes'][nm]; w, p = words(side, n); a = n['addr'] or 0; pad = 0x10
    for k in range(min(len(w), 0x24)):
        if (k in p and p[k][1] == nm and p[k][2] == 0) or (k not in p and w[k] == a and a): pad = 4 * k + 4   # the LAST one (word 3 is the heap header)
    _PAD[key] = pad; return pad
'''
src = src.replace('def main():', PADDEF + 'def main():', 1)
D3DPATCH = True
OLDW = "        ow, op = words(O, on); cw, cp = words(C, cn)\n"
assert OLDW in src
src = src.replace(OLDW, OLDW + "        sO = (padof(O, onm) - 0x10) // 4; sC = (padof(C, cnm) - 0x10) // 4\n"
    "        if sO or sC:\n"
    "            ow = ow[sO:]; cw = cw[sC:]; op = {a_ - sO: b_ for a_, b_ in op.items() if a_ >= sO}; cp = {a_ - sC: b_ for a_, b_ in cp.items() if a_ >= sC}\n")

D3DS = {2: 22, 3: 17, 4: 14, 5: 14, 6: 5, 7: 5, 8: 5, 9: 27, 10: 22, 1: 119}
OLDD = "        if sO or sC:\n"
OLDP = "            # data that looks like a pointer on one side only: the raw words decide (the original's from its image)\n"
D3DPTR = (
"            if po is None and pc is not None and pc[0] == 1 and 0xe40000 <= words(C, C['nodes'][pc[1]])[0][0] < 0xe48000: D3D[0] += 1; continue   # a D3D object inside a leaf node of the original\n")
src = src.replace(OLDP, D3DPTR + OLDP, 1)
D3DCODE = (
"        if len(cw) > 2 and 0 not in cp and 0xe40000 <= cw[0] < 0xe48000:\n"
"            D3D[0] += 1\n"
"            vt = op.get(0)\n"
"            vn = O['nodes'][vt[1]] if vt and vt[0] == 1 else None\n"
"            if vn is not None and on['size'] in (0x10, 0x20) and vn['size'] == 4 * D3DS.get(cw[2], -1): same += 1\n"
"            else: differ += 1; diffs.append(('%s@%08x/core %08x <- %s' % (label(O, on), on['addr'] or 0, cn['addr'] or 0, via), 'not the stand-in of a D3D object of kind %d' % cw[2]))\n"
"            continue\n")
src = src.replace(OLDD, D3DCODE + OLDD, 1)
for old, new in (("word_img(O, on['addr'] + 4 * k)", "word_img(O, on['addr'] + 4 * (k + sO))"), ("word_img(C, cn['addr'] + 4 * k)", "word_img(C, cn['addr'] + 4 * (k + sC))"),
                 ("ca_ = cn['addr'] + 4 * k", "ca_ = cn['addr'] + 4 * (k + sC)")):
    assert old in src, old
    src = src.replace(old, new)
src = src.replace("    print('VERDICT: %s'", "    print('leaf pointer pairs: %d; D3D stand-in objects: %d' % (LEAFS[0], D3D[0]))\n    print('VERDICT: %s'")
LEAF = '--leaf-pointers' in sys.argv
if LEAF: sys.argv.remove('--leaf-pointers')
g = {'__name__': 'build_cmp', '__file__': os.path.join(HERE, 'rx_canon_cmp.py'), 'LEAF': LEAF, 'LEAFS': [0], 'D3D': [0], 'D3DS': D3DS}
exec(compile(src, 'rx_canon_cmp.py(build_cmp)', 'exec'), g)
sys.exit(g['main']())
