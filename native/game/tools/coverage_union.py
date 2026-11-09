#!/usr/bin/env python3
"""coverage_union.py <out.tsv> <coverage.bin>... -- the functions any of the coverage runs executed (native/game work lists):
addr, name, size, instructions per frame (sum of the runs' per-frame means), runs that hit it. Sorted by cost."""
import bisect, collections, os, struct, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "_re", "out", "functions.tsv")
F = []
for l in open(R, encoding="latin-1").readlines()[1:]:
    c = l.rstrip("\n").split("\t"); F.append((int(c[0], 16), c[1], int(c[3] or 0)))
F.sort(); starts = [a for a, _, _ in F]
cost = collections.Counter(); hits = collections.Counter()
for p in sys.argv[2:]:
    d = open(p, "rb").read(); frames = struct.unpack_from("<4I", d)[1]; cnt = memoryview(d)[16:].cast("I")
    per = collections.Counter()
    for i, n in enumerate(cnt):
        if n: per[bisect.bisect_right(starts, 0x400000 + i) - 1] += n
    for k, n in per.items(): cost[k] += n / max(frames, 1); hits[k] += 1
with open(sys.argv[1], "w") as o:
    o.write("addr\tname\tsize\tinstr_per_frame\truns\n")
    for k, c in cost.most_common():
        a, name, size = F[k]; o.write(f"{a:08x}\t{name}\t{size}\t{c:.0f}\t{hits[k]}\n")
print(len(cost), "functions,", f"{sum(cost.values()):,.0f}", "instructions per frame (sum over runs)")
