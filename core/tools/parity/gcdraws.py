#!/usr/bin/env python3
# gcdraws.py <product pair> <oracle pair> [n] -- w-parity4w: the per-frame game-LCG draw counts (gc deltas) and end seeds of the two
# sides aligned on the event's first frame (as kdiff.py EV=1): the frames whose counts / seeds differ, the first n with both counts.
import sys
import os; sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import gridpair_cmp as g
P, _ = g.parse(sys.argv[1]); O, _ = g.parse(sys.argv[2])
ev = lambda R: next(i + 1 for i in range(len(R) - 1) if R[i]['ms'] == 11 and R[i + 1]['ms'] != 11)
ep, eo = ev(P), ev(O)
n = min(len(P) - ep, len(O) - eo)
bad = [k for k in range(1, n) if P[ep + k]['gc'] - P[ep + k - 1]['gc'] != O[eo + k]['gc'] - O[eo + k - 1]['gc']]
seed = [k for k in range(0, n) if P[ep + k]['gs'] != O[eo + k]['gs']]
print('frames', n, 'count differs in', len(bad), 'first', bad[:8], ' seed differs in', len(seed), 'first', seed[:8])
for k in bad[:int(sys.argv[3]) if len(sys.argv) > 3 else 5]:
    print(k, 'st', P[ep + k]['st'], 'product', P[ep + k]['gc'] - P[ep + k - 1]['gc'], 'oracle', O[eo + k]['gc'] - O[eo + k - 1]['gc'])
