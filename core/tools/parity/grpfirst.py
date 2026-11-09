import sys
def load(p, e, kmin=int(__import__("os").environ.get("KMIN","151")), kmax=1300):
    d = {}
    for l in open(p):
        if l[:2] != 'O ': continue
        t = l.split(); k=int(t[1]) - e
        if kmin<=k<=kmax: d.setdefault(k, {})[int(t[2])] = t[3:]
    return d
P = load(sys.argv[1], 861); O = load(sys.argv[2], 1205)
first={}
for k in sorted(P):
    for i in P[k]:
        if k in O and P[k][i]!=O[k].get(i) and i//17 not in first: first[i//17]=k
print(sorted(first.items(), key=lambda x:x[1]))
