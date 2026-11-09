#!/usr/bin/env python3
"""What of the original game still runs per frame (native/game progress).
   coverage.py <coverage.bin> [top=60]
The file comes from reflex_boot's world test with REFLEX_COVERAGE=<file> (interpreted from REFLEX_COVERAGE_FROM, with the ports of
REFLEX_PORT installed): executions per exe instruction. Aggregated by function (the Ghidra export's functions.tsv)."""
import bisect, collections, os, struct, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "_re", "out", "functions.tsv")
F = []
for l in open(R, encoding="latin-1").readlines()[1:]:
    c = l.rstrip("\n").split("\t")
    F.append((int(c[0], 16), c[1]))
F.sort(); starts = [a for a, _ in F]
data = open(sys.argv[1], "rb").read()
magic, frames, lo, hi = struct.unpack_from("<4I", data)
counts = memoryview(data)[16:].cast("I")
outside = lo | hi << 32
per = collections.Counter(); total = 0
for i, n in enumerate(counts):
    if not n: continue
    k = bisect.bisect_right(starts, 0x400000 + i) - 1
    per[k] += n; total += n
top = int(sys.argv[2]) if len(sys.argv) > 2 else 60
fr = max(frames, 1)
print(f"{frames} frames: original instructions per frame {total / fr:,.0f} in the exe + {outside / fr:,.0f} in the CRT DLLs, {len(per)} functions")
for k, n in per.most_common(top):
    a, name = F[k]
    print(f"{100 * n / total:6.2f}% {n / fr:12,.0f}/frame  {a:08x} {name}")
