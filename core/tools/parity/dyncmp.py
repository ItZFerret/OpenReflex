import sys
# dyncmp.py product.txt oracle.txt [entryP entryO] -- the O lines (dynamic object records +0x40..+0xd0) by k: the first k / object where any
# word differs, separately for w lanes only (word index % 4 == 3) and for x, y, z lanes
eP = int(sys.argv[3]) if len(sys.argv) > 3 else 861; eO = int(sys.argv[4]) if len(sys.argv) > 4 else 1205
def load(p, e):
    d = {}
    for l in open(p):
        if l[:2] != 'O ': continue
        t = l.split(); d.setdefault(int(t[1]) - e, {})[int(t[2])] = t[3:]
    return d
P = load(sys.argv[1], eP); O = load(sys.argv[2], eO)
names = ['pos', 'rot', 'vel', 'angv', 'r80', 'r90', 'ra0', 'acb0', 'acc0']
fw = fx = None; shown = 0
for k in sorted(set(P) & set(O)):
    if k < int(__import__('os').environ.get('KMIN', '-900')): continue
    for i in sorted(set(P[k]) & set(O[k])):
        a, b = P[k][i], O[k][i]
        for j in range(min(len(a), len(b))):
            if a[j] == b[j]: continue
            lane = j % 4
            tag = '%s.%s' % (names[j // 4], 'xyzw'[lane])
            if lane == 3 and fw is None: fw = (k, i, tag, a[j], b[j]); print('first w-lane difference: k', k, 'object', i, tag, a[j], b[j])
            if lane != 3 and fx is None: fx = (k, i, tag, a[j], b[j]); print('first xyz difference: k', k, 'object', i, tag, a[j], b[j]); print('  P', ' '.join(a)); print('  O', ' '.join(b))
    if fw and fx: break
print('objects per frame', len(P.get(0, {})), len(O.get(0, {})))
