#!/usr/bin/env python3
# kdiff.py <product pair> <oracle pair> [k0] [k1] -- w-parity4w: per aligned frame k (the countdown entry = 0, or with EV=1 the event's first
# frame = 0: the frame after the last one ending in MXGame state 0xb) the gridpair_line.inc L labels / word indices and the R slots / word
# indices that differ, with both values as floats ("<label>[<index>:<product>/<oracle>,...]").
import sys, struct
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import gridpair_cmp as g
P, _ = g.parse(sys.argv[1]); O, _ = g.parse(sys.argv[2])
ep, eo = g.entry(P), g.entry(O)
import os
if os.environ.get('EV'):   # align on the event's first frame (the frame after the last one ending in MXGame state 0xb)
    ev = lambda R: next(i + 1 for i in range(len(R) - 1) if R[i]['ms'] == 11 and R[i + 1]['ms'] != 11)
    ep, eo = ev(P), ev(O)
k0 = int(sys.argv[3]) if len(sys.argv) > 3 else 0
k1 = int(sys.argv[4]) if len(sys.argv) > 4 else k0 + 5
def f(h):
    try: return '%.6g' % struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]
    except Exception: return h
for k in range(k0, k1 + 1):
    if ep + k >= len(P) or eo + k >= len(O): break
    p, o = P[ep + k], O[eo + k]
    out = []
    for lab in p['L']:
        a, b = p['L'].get(lab, []), o['L'].get(lab, [])
        d = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
        if d or len(a) != len(b): out.append('%s[%s]' % (lab, ','.join('%d:%s/%s' % (i, f(a[i]), f(b[i])) for i in d[:4]) + ('..%d' % len(d) if len(d) > 4 else '')))
    for s in sorted(set(p['R']) | set(o['R'])):
        a, b = p['R'].get(s, []), o['R'].get(s, [])
        d = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
        if d or len(a) != len(b): out.append('R%d[%s]' % (s, ','.join('%d:%s/%s' % (i, f(a[i]), f(b[i])) for i in d[:3]) + ('..%d' % len(d) if len(d) > 3 else '')))
    print(k, 'frames', p['frame'], o['frame'], 'gc', p['gc'], o['gc'], ' '.join(out))
