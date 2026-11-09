#!/usr/bin/env python3
"""gap_functions.py <out.tsv> <coverage.bin>... -- the functions any of the coverage runs executed, INCLUDING code Ghidra has no
function for (coverage_union.py gives such instructions to the function before them: Database::vf5, 33 bytes, got 6.8 M
instructions of the undefined code after it).

An instruction counts for a functions.tsv function only inside [start, start + size). Executed code outside every function
("gap" code) is split into functions at candidate entries: direct call targets (linear disassembly of the known functions and of
the gaps), code pointers from relocations outside .text (vtables, callbacks), and the first byte after int3 padding (16-aligned,
or at the gap's start). Executed code before a gap's first entry (the tail of a function Ghidra cut short) stays gap_<its pc>.
Gap functions are named gap_<addr>; their size runs to the next candidate or known function.

Columns: addr, name, size, instructions (sum of per-frame means over the runs), runs, kind (fn / gap)."""
import bisect, collections, os, struct, sys
import capstone, pefile
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
pe = pefile.PE(os.environ.get("REFLEX_EXE") or os.path.join(R, "game files", "Archive", "MXReflex.exe"))
img = pe.get_memory_mapped_image(); B = pe.OPTIONAL_HEADER.ImageBase
text = next(s for s in pe.sections if s.Name.startswith(b".text"))
T0, T1 = B + text.VirtualAddress, B + text.VirtualAddress + text.Misc_VirtualSize
F = {}
for l in open(os.path.join(R, "_re", "out", "functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.rstrip("\n").split("\t"); F[int(c[0], 16)] = (c[1], int(c[3] or 0))
known = sorted(F); known_end = [a + F[a][1] for a in known]

def covering(a):   # the known function containing a, else None
    i = bisect.bisect_right(known, a) - 1
    return known[i] if i >= 0 and a < known_end[i] else None

# gaps: [end of a known function, next known start) that contain executed code -- computed after reading the coverage
cost = collections.Counter(); hits = collections.defaultdict(set); gap_pcs = collections.Counter()
for run, p in enumerate(sys.argv[2:]):
    d = open(p, "rb").read(); frames = max(struct.unpack_from("<4I", d)[1], 1); cnt = memoryview(d)[16:].cast("I")
    for i, n in enumerate(cnt):
        if not n: continue
        a = 0x400000 + i; k = covering(a)
        if k is not None: cost[k] += n / frames; hits[k].add(run)
        else: gap_pcs[a] += n / frames; hits[("g", a)].add(run)

# candidate entries
cand = set()
for e in pe.DIRECTORY_ENTRY_BASERELOC if hasattr(pe, "DIRECTORY_ENTRY_BASERELOC") else []:
    for r in e.entries:
        if r.type != 3: continue
        at = r.rva + B
        if T0 <= at < T1: continue        # inside .text: jump tables (mid-function targets)
        v = struct.unpack_from("<I", img, r.rva)[0]
        if T0 <= v < T1: cand.add(v)
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
def scan(lo, hi):
    for ins in md.disasm(img[lo - B:hi - B], lo):
        if ins.mnemonic == "push" and ins.op_str.startswith("0x"):   # pushed code pointers (callbacks, factory tables)
            try:
                v = int(ins.op_str, 16)
                if T0 <= v < T1: cand.add(v)
            except ValueError: pass
        if ins.mnemonic == "call" and ins.op_str.startswith("0x"):
            try: cand.add(int(ins.op_str, 16))
            except ValueError: pass
for a in known: scan(a, a + F[a][1])
gaps = []
for i, a in enumerate(known):
    lo, hi = known_end[i], known[i + 1] if i + 1 < len(known) else T1
    if hi > lo: gaps.append((lo, hi))
gap_starts = [g[0] for g in gaps]
used = {gaps[bisect.bisect_right(gap_starts, a) - 1] for a in gap_pcs}
for lo, hi in used:
    scan(lo, hi)
    for a in range(lo + 1, hi):            # first byte after padding
        if img[a - 1 - B] == 0xCC and img[a - B] != 0xCC and a % 16 == 0: cand.add(a)
    a = lo                                 # the gap's first byte after int3 padding (not after 0x90: nops sit inside instructions)
    while a < hi and img[a - B] == 0xCC: a += 1
    if a > lo: cand.add(a)
cs = sorted(c for c in cand if covering(c) is None)
gcost = collections.Counter(); gruns = collections.defaultdict(set)
tail = {}                                  # per gap: its first executed pc (code before the gap's first entry)
for a in sorted(gap_pcs):
    lo = gaps[bisect.bisect_right(gap_starts, a) - 1][0]
    tail.setdefault(lo, a)
for a, n in gap_pcs.items():
    lo = gaps[bisect.bisect_right(gap_starts, a) - 1][0]
    i = bisect.bisect_right(cs, a) - 1
    s = cs[i] if i >= 0 and cs[i] >= lo else tail[lo]   # only an entry inside the same gap (2026-10-18 fix: never an earlier gap's)
    gcost[s] += n; gruns[s] |= hits[("g", a)]
rows = [(a, F[a][0], F[a][1], c, len(hits[a]), "fn") for a, c in cost.items()]
for s, c in gcost.items():
    i = bisect.bisect_right(cs, s); nxt = min(cs[i] if i < len(cs) else T1, known[bisect.bisect_right(known, s)] if bisect.bisect_right(known, s) < len(known) else T1)
    rows.append((s, f"gap_{s:08x}", nxt - s, c, len(gruns[s]), "gap"))
rows.sort(key=lambda r: -r[3])
with open(sys.argv[1], "w") as o:
    o.write("addr\tname\tsize\tinstructions\truns\tkind\n")
    for a, n, sz, c, h, k in rows: o.write(f"{a:08x}\t{n}\t{sz}\t{c:.0f}\t{h}\t{k}\n")
g = [r for r in rows if r[5] == "gap"]
print(len(rows), "functions:", len(rows) - len(g), "known", f"({sum(r[3] for r in rows if r[5] == 'fn'):,.0f} instructions),", len(g), "gap", f"({sum(r[3] for r in g):,.0f})")
