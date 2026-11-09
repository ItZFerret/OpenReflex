import sys
# dstream.py product oracle -- streaming: the first D/d line (seq ignored) that differs; NOW=1 ignores the pA / pB w lanes
import os
now = os.environ.get('NOW')
def lines(p):
    for l in open(p):
        if l[:2] in ('D ', 'd '):
            t = l.split()
            if t[0] == 'D' and len(t) < 32: continue
            t = t[:1] + t[2:]
            if now and t[0] == 'D':
                for k in ('pA', 'pB'):
                    i = t.index(k); t[i + 4] = 'w'
            yield ' '.join(t)
def from_sync(g):
    key = os.environ.get('SYNC', 'tB 32768')
    for x in g:
        if x.startswith('D') and key in x: yield x; break
    yield from g
n = 0; prev = None
for a, b in zip(from_sync(lines(sys.argv[1])), from_sync(lines(sys.argv[2]))):
    if a != b:
        print('line', n); print(' prev', prev[:300] if prev else None); print(' P', a[:600]); print(' O', b[:600]); break
    prev = a; n += 1
else: print('identical over', n)
