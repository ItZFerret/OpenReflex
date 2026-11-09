#!/usr/bin/env python3
"""gridpair_simdiff.py <product pair trace> <product frame> <oracle pair trace> <oracle frame> [--all] -- the local MX simulation's
words of the two sides' S dumps (gridpair_line.inc pair_simdump: PROBE_PAIR_SIMDUMP / RX_PAIR_SIMDUMP) compared word by word.
Classes: 'self' = a pointer into the side's own S at the same offset (equal), 'ptr' = both sides heap pointers, 'zero/ptr' = 0 on one
side and a heap pointer on the other, 'w' = the w lane (offset % 16 == 12) of a vec4, 'float' = everything else. Prints the counts and
every 'float' difference (all classes with --all)."""
import struct, sys

def f32(w): return struct.unpack('<f', struct.pack('<I', w))[0]

KIND = 'S'
def dump(path, frame):
    for line in open(path):
        if line.startswith('%s %s ' % (KIND, frame)):
            r = line.split(); return int(r[5], 16), [int(x, 16) for x in r[r.index('words') + 1:]]
    raise SystemExit('no dump of frame %s in %s' % (frame, path))

def main():
    global KIND
    a = sys.argv[1:]; every = '--all' in a; a = [x for x in a if x != '--all']
    for x in list(a):
        if x.startswith('--kind='): KIND = x[7:]; a.remove(x)   # S (the simulation), R (the rider), V (the vehicle)
    pS, pw = dump(a[0], a[1]); oS, ow = dump(a[2], a[3])
    heap = lambda x: 0x08000000 <= x < 0x40000000
    counts = {}; rows = []
    for i, (p, o) in enumerate(zip(pw, ow)):
        if p == o: continue
        off = 4 * i
        if p - pS == o - oS and heap(p): c = 'self'
        elif heap(p) and heap(o): c = 'ptr'
        elif (p == 0 and heap(o)) or (o == 0 and heap(p)): c = 'zero/ptr'
        elif off % 16 == 12: c = 'w'
        else: c = 'float'
        counts[c] = counts.get(c, 0) + 1
        if every or c == 'float': rows.append((off, c, p, o))
    print('product S %08x  oracle S %08x  differing words by class: %s' % (pS, oS, counts))
    for off, c, p, o in rows:
        print('  +%04x %-8s %08x %08x  %.9g  %.9g' % (off, c, p, o, f32(p), f32(o)))

if __name__ == '__main__':
    main()
