import sys, re
NAMES = set('dt air first c w q flt fade t hist to g l pos v rear front grip in Wr Wf B X'.split())
# stepnew.py a b [n] -- lines that introduce a newly differing (group, index) between two step traces
a = open(sys.argv[1]).read().split('\n'); b = open(sys.argv[2]).read().split('\n')
def groups(line):
    t = line.split(); g = {}; cur = '#'
    for w in t[1:]:
        if w in NAMES: cur = w; g.setdefault(cur, []); continue
        g.setdefault(cur, []).append(w)
    return g
seen = set(); shown = 0; lim = int(sys.argv[3]) if len(sys.argv) > 3 else 10
for i in range(min(len(a), len(b))):
    if not a[i] or not b[i] or a[i] == b[i]: continue
    ga, gb = groups(a[i]), groups(b[i]); new = []
    for k in ga:
        va, vb = ga[k], gb.get(k, [])
        for j in range(max(len(va), len(vb))):
            x = va[j] if j < len(va) else None; y = vb[j] if j < len(vb) else None
            if x != y and (k, j) not in seen: seen.add((k, j)); new.append('%s[%d] %s/%s' % (k, j, x, y))
    if new:
        print('step', i, ' '.join(new)[:900]); shown += 1
        if shown >= lim: break
