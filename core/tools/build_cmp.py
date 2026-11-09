#!/usr/bin/env python3
"""build_cmp.py -- rx_canon_cmp.py (same arguments, same output) plus pointers inside LEAF nodes (core/notes/construction.md section 8).

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
src = src.replace(OLD, NEW)
src = src.replace("    print('VERDICT: %s'", "    print('leaf pointer pairs: %d' % LEAFS[0])\n    print('VERDICT: %s'")
LEAF = '--leaf-pointers' in sys.argv
if LEAF: sys.argv.remove('--leaf-pointers')
g = {'__name__': 'build_cmp', '__file__': os.path.join(HERE, 'rx_canon_cmp.py'), 'LEAF': LEAF, 'LEAFS': [0]}
exec(compile(src, 'rx_canon_cmp.py(build_cmp)', 'exec'), g)
sys.exit(g['main']())
