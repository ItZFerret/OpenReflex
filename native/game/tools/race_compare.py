#!/usr/bin/env python3
"""race_compare.py <a-race.txt> <b-race.txt> -- compares two races without addresses (native/game notes/loading.md, "Acceptance").

The files come from reflex_boot's world test with REFLEX_RACE_STATE=<file> (boot_main.cpp; tools/loading_accept.sh runs it): every
REFLEX_RACE_STATE_EVERY frames, per rider slot the vehicle's render bodies (position, orientation), gear, rpm, speed and hashes of its
simulation and rider objects with the address-like words left out, then the race state and every driver's place, laps and times.
Prints IDENTICAL when every sample matches bit for bit; else the first frame that differs, what differs there, and how far the bodies
are apart over the run (largest position distance per slot). Exit status 0 when identical."""
import math, sys

def parse(path):
    rows = {}
    for line in open(path):
        f = line.split()
        if len(f) < 3 or f[0] != 'f': continue
        frame = int(f[1])
        key = (frame, f[2] + (f[3] if f[2] == 'slot' else ''))
        rows[key] = f[2:]
    return rows

def bodies(fields):
    out = {}
    for i, x in enumerate(fields):
        if x.startswith('b') and x[1:].isdigit() and i + 7 < len(fields) + 1:
            out[int(x[1:])] = [float(v) for v in fields[i + 1:i + 8]]
    return out

def main():
    A, B = parse(sys.argv[1]), parse(sys.argv[2])
    keys = sorted(set(A) | set(B))
    first = None; worst = {}; same = 0
    for k in keys:
        a, b = A.get(k), B.get(k)
        if a == b: same += 1; continue
        if first is None: first = (k, a, b)
        if a and b and k[1].startswith('slot'):
            ba, bb = bodies(a), bodies(b)
            for i in set(ba) & set(bb):
                d = math.dist(ba[i][:3], bb[i][:3])
                if d > worst.get((k[1], i), (0, 0))[0]: worst[(k[1], i)] = (d, k[0])
    print('%d samples, %d identical' % (len(keys), same))
    if first is None:
        print('VERDICT: IDENTICAL'); return 0
    (frame, what), a, b = first
    print('VERDICT: DIFFERENT, first at frame %d (%s)' % (frame, what))
    if a and b:
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y: print('  field %d (%s): %s vs %s' % (i, a[i - 1] if i else '', x, y))
    else: print('  only in %s' % ('A' if a else 'B'))
    for (slot, i), (d, fr) in sorted(worst.items()):
        print('  %s body %d: largest position distance %.4f at frame %d' % (slot, i, d, fr))
    return 1

if __name__ == '__main__':
    sys.exit(main())
