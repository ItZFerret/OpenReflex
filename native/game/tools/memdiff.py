#!/usr/bin/env python3
"""memdiff.py <a.dump> <b.dump> [max=60] -- where two runs' memories differ (reflex_boot with REFLEX_MEMDUMP=<file>, e.g. without
and with REFLEX_PORT). Per differing dword: address, both values, and the owner: the nearest vtable pointer at or below it within
0x4000 bytes (RTTI class from _re/out/classes.tsv) and the offset into that object. Stacks are not in the dumps."""
import os, struct, sys, bisect
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "_re", "out", "classes.tsv")
vt = {}
for l in open(R, encoding="latin-1").readlines()[1:]:
    c = l.split("\t")
    try: vt[int(c[1], 16)] = c[0]
    except ValueError: pass
def load(p):
    d = open(p, "rb").read(); pages = {}
    for i in range(0, len(d), 4100): pages[struct.unpack_from("<I", d, i)[0]] = d[i + 4:i + 4100]
    return pages
A, B = load(sys.argv[1]), load(sys.argv[2]); mx = int(sys.argv[3]) if len(sys.argv) > 3 else 60
def u32(P, a):
    p = P.get(a & ~0xfff); return None if p is None else struct.unpack_from("<I", p, a & 0xfff)[0]
def owner(a):
    for x in range(a & ~3, max(a - 0x4000, 0), -4):
        v = u32(A, x)
        if v in vt: return f"{vt[v]}@{x:08x}+{a - x:#x}"
    return "?"
n = 0
only = set(A) ^ set(B)
if only: print(f"pages only in one run: {len(only)} (e.g. {sorted(only)[:5]})")
for pg in sorted(set(A) & set(B)):
    a, b = A[pg], B[pg]
    if a == b: continue
    for i in range(0, 4096, 4):
        if a[i:i + 4] != b[i:i + 4]:
            n += 1
            if n <= mx: print(f"{pg + i:08x} {struct.unpack_from('<I', a, i)[0]:08x} {struct.unpack_from('<I', b, i)[0]:08x}  {owner(pg + i)}")
print(f"{n} differing dwords")
