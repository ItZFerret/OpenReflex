# grpfirst_xyz.py <product pair> <oracle pair> -- w-parity2: grpfirst.py (the first differing k of each rider's generic dynamic
# objects, PAIR_DYNOBJ lines) counting only the x / y / z lanes of the records' vec4s (the w lanes are dead words: stale stack /
# copies of other w lanes no code reads as data). KMIN (default 151), EP / EO the k origins (861 / 1205). Prints [(rider, k, part)].
import sys, os
EP, EO, KMIN = int(os.environ.get("EP", "861")), int(os.environ.get("EO", "1205")), int(os.environ.get("KMIN", "151"))
def load(p, e):
    d = {}
    for l in open(p):
        if l[:2] != 'O ': continue
        t = l.split(); k = int(t[1]) - e
        if k >= KMIN: d.setdefault(k, {})[int(t[2])] = t[3:]
    return d
P = load(sys.argv[1], EP); O = load(sys.argv[2], EO)
first = {}
for k in sorted(P):
    if k not in O: continue
    for i, a in P[k].items():
        b = O[k].get(i)
        if b is None or i // 17 in first: continue
        if any(a[j] != b[j] for j in range(min(len(a), len(b))) if j % 4 != 3): first[i // 17] = (k, i % 17)
print(sorted(((r, k, part) for r, (k, part) in first.items()), key=lambda x: x[1]))
print("last aligned k:", max(k for k in P if k in O))
