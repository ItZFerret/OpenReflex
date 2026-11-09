#!/usr/bin/env python3
# simpoke.py <product pair> <product frame> <oracle pair> <oracle frame> [--show] [--kinds S,R,V,...] [--poke-frame F] -- w-parity4w:
# the local slot's gridpair_line.inc pair_simdump records (PROBE_PAIR_SIMDUMP / RX_PAIR_SIMDUMP: S the simulation, R the rider (PAIR_RSIZE),
# V the vehicle, D / A, and with PAIR_SIMDUMP_NODES=1 the node tables M1L/M1W/M1B, M5L/M5W/M5B, RHL/RHW/RHB) compared word by word; the
# words that differ and are neither heap pointers (0x10000000 <= w < 0x30000000 on either side, as rtpoke.py) nor small integers
# (exponent field 0) are printed as PROBE_POKE entries "F:addr:word" at the product's addresses with the oracle's values (default
# F = the product frame). The four-wheel local's EVENT_RTPOKE (pair.sh): its front-end vehicle, rider and node tables at the last
# loading frame (product 8 / oracle 353), the generalisation of rtpoke.py's rider words (the four-wheel sims settle and animate in the
# front end; the MX bike's residue is only rtpoke.py's).
import sys, struct
a = sys.argv[1:]
show = '--show' in a; a = [x for x in a if x != '--show']
kinds = 'S,R,V'; pf = None
if '--kinds' in a: i = a.index('--kinds'); kinds = a[i + 1]; del a[i:i + 2]
if '--poke-frame' in a: i = a.index('--poke-frame'); pf = a[i + 1]; del a[i:i + 2]
def dumps(path, frame):
    d = {}
    for line in open(path):
        r = line.split(' ', 6)
        if len(r) > 5 and r[1] == frame and r[2] == 'slot' and r[4] == 'S':
            w = line.split(); d[w[0]] = (int(w[5], 16), [int(x, 16) for x in w[w.index('words') + 1:]])
    return d
P = dumps(a[0], a[1]); O = dumps(a[2], a[3]); pf = pf or a[1]
isp = lambda w: 0x10000000 <= w < 0x30000000
f32 = lambda w: struct.unpack('<f', struct.pack('<I', w))[0]
out = []
for k in kinds.split(','):
    if k not in P or k not in O: print('# no', k, file=sys.stderr); continue
    (pb, pw), (ob, ow) = P[k], O[k]
    n = cnt = 0
    for i, (x, y) in enumerate(zip(pw, ow)):
        if x == y: continue
        n += 1
        if isp(x) or isp(y) or (x & 0x7f800000) == 0 and x or (y & 0x7f800000) == 0 and y: continue   # pointers, ints (denormal-looking)
        cnt += 1
        out.append('%s:%x:%08x' % (pf, pb + 4 * i, y))
        if show: print('%s +%05x  %08x %08x  %g / %g' % (k, 4 * i, x, y, f32(x), f32(y)), file=sys.stderr)
    print('# %s product %08x oracle %08x: %d words differ, %d not pointers' % (k, pb, ob, n, cnt), file=sys.stderr)
print(','.join(out))
