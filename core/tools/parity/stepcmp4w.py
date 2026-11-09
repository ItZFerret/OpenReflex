#!/usr/bin/env python3
# stepcmp4w.py <product steps> <oracle steps> [n] [min step] -- w-parity4w: the four-wheel per-step lines (bike_parity_probe.c PROBE_STEP_TRACE
# of a gridpair_stepbuild.sh probe / the oracle's rx_WhipStepTrace with RX_WHIP_TRACE=1 RX_WHIP_TRACE_SLOT) aligned on the first identical
# bodies (B) after <min step> (default 100: the front ends differ in length), then the first step with a difference in in / B / Q (NaN
# signs ignored: the ports' w-lane NaNs are 0x7fc00000, x86's 0xffc00000) / W0..3 (the wheel records' uninitialised words +4 +8 +0xc
# +0xc8 +0xcc +0x10c +0x118 +0x11c ignored) / spin / X / off / T, and the differing words of it and the next n - 1 steps.
import sys, struct
LABS = ('dt', 'fw', 'in', 'B', 'Q', 'W0', 'W1', 'W2', 'W3', 'spin', 'X', 'off', 'T')
SKIPW = {1, 2, 3, 50, 51, 67, 70, 71}
def load(p):
    out = []
    for l in open(p):
        t = l.split(); d, lab = {}, None
        for x in t[1:]:
            if x in LABS: lab = x; d[lab] = []
            else: d[lab].append(x)
        out.append(d)
    return out
import os
WLANES = os.environ.get('STEPCMP_WLANES')   # w-parity5: STEPCMP_WLANES=1 ignores the w lanes (word index % 4 == 3) of B / X / W0..3:
                                            # the original leaves NaNs there where the product's ports store values (no reader)
def norm(lab, n, x):
    if lab[0] == 'W' and n in SKIPW: return '-'
    if WLANES and lab[0] in 'BXW' and n % 4 == 3: return '-'
    if x in ('7fc00000', 'ffc00000'): return 'nan'
    return x
def same(a, b):
    return all([norm(l, n, x) for n, x in enumerate(a.get(l, []))] == [norm(l, n, x) for n, x in enumerate(b.get(l, []))] for l in LABS)
f32 = lambda h: struct.unpack('<f', struct.pack('<I', int(h, 16)))[0]
P, O = load(sys.argv[1]), load(sys.argv[2])
idx = {}
for j, o in enumerate(O): idx.setdefault(' '.join(o['B']), j)
i = j = None
for ii, p in enumerate(P):
    jj = idx.get(' '.join(p['B']))
    if jj is not None and ii > (int(sys.argv[4]) if len(sys.argv) > 4 else 100): i, j = ii, jj; break
print('aligned: product step', i, 'oracle step', j)
while i < len(P) and j < len(O) and same(P[i], O[j]): i += 1; j += 1
print('first differing: product step', i, 'oracle step', j, 'of', len(P), len(O))
for k in range(int(sys.argv[3]) if len(sys.argv) > 3 else 2):
    if i + k >= len(P) or j + k >= len(O): break
    a, b = P[i + k], O[j + k]
    out = []
    for lab in LABS:
        x, y = a.get(lab, []), b.get(lab, [])
        d = [n for n, (u, v) in enumerate(zip(x, y)) if norm(lab, n, u) != norm(lab, n, v)]
        if d: out.append('%s[%d: %s]' % (lab, len(d), ' '.join('%d:%s/%s(%.6g/%.6g)' % (n, x[n], y[n], f32(x[n]), f32(y[n])) for n in d[:8])))
    print('step +%d:' % k, '\n   '.join(out))
