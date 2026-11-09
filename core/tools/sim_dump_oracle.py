#!/usr/bin/env python3
"""sim_dump_oracle.py <memdump> <out> (w-surfaces) -- per MX vehicle slot of an ORIGINAL world's memory dump (reflex_boot with
REFLEX_WORLD_TEST=<world> REFLEX_AFTER=n REFLEX_MEMDUMP=<file>): the vehicle spec's identity (model, owner, owner vehicle id, tuning,
parts; MXGame +0x154d8: +4 VehicleId = P+0x1b0, +8 the model record, +0x98 / +0x9c, +0xa0.. tuning, +0xc parts) and the raw words of
its simulation S (0x8fc0 bytes), in the format of core/tools/bike_parity_probe.c's PROBE_SIMDUMP (the product's side). A world image
holds only the pages the race touched: unreadable words print as ????????, unreadable ids as -999999 (the model record is usually not
resident: map the owner vehicle id through the AIVehicle table). Compare with sim_params_cmp.py."""
import struct, sys
if len(sys.argv) != 3: print(__doc__); sys.exit(2)
d = open(sys.argv[1], "rb").read(); pages = {}
for i in range(0, len(d), 4100): pages[struct.unpack_from("<I", d, i)[0]] = d[i + 4:i + 4100]
def u32(a):
    p = pages.get(a & ~0xfff)
    if p is None: return None
    return struct.unpack_from("<I", p, a & 0xfff)[0]
def i32(a):
    v = u32(a)
    return -999999 if v is None else (v - (1 << 32) if v & 0x80000000 else v)
G = u32(0xa946b8)
out = open(sys.argv[2], "w")
vn, va = u32(G + 0x154dc), u32(G + 0x154d8)
specs = {}
for i in range(vn):
    e = u32(va + 4 * i)
    if e: specs[u32(e + 4)] = e
for k in range(16):
    P = G + 0x7460 + k * 0x12a0
    V = u32(P + 0x1ac)
    if not V: continue
    S = u32(V + 0x234)
    if not S or u32(S) != 0x0093c8a4: continue
    e = specs.get(u32(P + 0x1b0))
    M = u32(e + 8) if e else 0
    tun = [i32(e + o) for o in range(0xa0, 0xc0, 4)] if e else []
    parts = [i32(u32(e + 0xc + 4 * j) + 0x40) if u32(e + 0xc + 4 * j) else -1 for j in range(17)] if e else []
    out.write("slot %d type %d model %d owner %d ovid %d tuning %s parts %s S %08x\n" % (k, i32(P + 0x10), i32(M) if M else -1,
              i32(e + 0x98), i32(e + 0x9c), " ".join(str(t) for t in tun), " ".join(str(p) for p in parts), S))
    words = []
    for o in range(0, 0x8fc0, 4):
        w = u32(S + o)
        words.append("%08x" % w if w is not None else "????????")
    out.write("words " + " ".join(words) + "\n")
