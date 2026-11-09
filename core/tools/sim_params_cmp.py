#!/usr/bin/env python3
"""sim_params_cmp.py params <original sims> <product sims> (w-surfaces) -- the 108 registered parameters of RavenSimulationMotocross (004392f0;
core/game/build_vehicle_params.inc: offset into P = S+0x2f10, kind) of every vehicle of the same spec: only the value words the
kinds define (0: +0x44; 1: +0x50..+0x5c; 2: count +0x44, count knots x +0x48.., y +0x70.., slopes +0x98/+0x9c; 3: +0x44..+0x4c),
paired by (owner, owner vehicle id, slot).
sim_params_cmp.py static <orig early> <orig late> <product early> <product late>: every simulation word that is static on both
sides (equal at both dump times; S-relative pointers normalized, heap pointers left out), original vs product per spec; the
differences classed (both zero / tiny: signed zeros of the parked state; original nonzero vs product 0: the original's reused,
uninitialized heap bytes (P+0x1b0.. the unused record-pointer table, unwritten matrix storage); both nonzero: a real difference)."""
import os, re, struct, sys
INC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "game", "build_vehicle_params.inc")
recs = [(int(m.group(1), 16), int(m.group(2)), m.group(3)) for m in re.finditer(r"\{0x([0-9a-f]+), (\d), 0x[0-9a-f]+, \{[^}]*\}\},\s*/\* (\w+)", open(INC).read())]
def load(p):
    out, cur = {}, None
    for l in open(p):
        t = l.split()
        if t[0] == "slot":
            d = dict(zip(t[0::2], t[1::2])); cur = (int(d["owner"]), int(d["ovid"]), int(d["slot"]))
            out[cur] = {"model": d["model"], "S": int(t[t.index("S") + 1], 16)}
        elif t[0] == "words": out[cur]["w"] = [int(x, 16) if x != "????????" else None for x in t[1:]]
    return out
def f(v): return struct.unpack("<f", struct.pack("<I", v))[0]
def values(w):
    P = 0x2f10; vals = {}
    for off, kind, name in recs:
        r = P + off
        g = lambda o: w[(r + o) // 4]
        if kind == 0: vals[name] = [g(0x44)]
        elif kind == 1: vals[name] = [g(0x50 + 4 * k) for k in range(4)]
        elif kind == 3: vals[name] = [g(0x44 + 4 * k) for k in range(3)]
        else:
            n = g(0x44); n = n if n is not None and n <= 10 else 0
            vals[name] = [g(0x44)] + [g(0x48 + 4 * k) for k in range(n)] + [g(0x70 + 4 * k) for k in range(n)] + [g(0x98), g(0x9c)]
    return vals
def params_mode(fa, fb):
    A, B = load(fa), load(fb)
    print("%d parameter records" % len(recs))
    for ka in sorted(A, key=lambda k: k[2]):
        kb = next((k for k in B if k[:2] == ka[:2] and k[2] == ka[2]), None) or next((k for k in B if k[:2] == ka[:2]), None)
        if kb is None: print("slot %d: no product vehicle of owner %d id %d" % (ka[2], ka[0], ka[1])); continue
        va, vb = values(A[ka]["w"]), values(B[kb]["w"])
        bad = [n for n in va if va[n] != vb[n]]
        print("slot %2d owner %d ovid %3d model %s/%s: %d of %d parameters differ%s" % (ka[2], ka[0], ka[1], A[ka]["model"], B[kb]["model"], len(bad), len(va), (": " + ", ".join(bad[:8])) if bad else ""))
        for n in bad[:6]:
            print("    %s orig %s product %s" % (n, " ".join("%.6g" % f(x) if x is not None else "?" for x in va[n]), " ".join("%.6g" % f(x) for x in vb[n])))

def norm(v, S):
    if v is None: return None
    if S <= v < S + 0x8fc0: return ("S", v - S)
    if 0x10000000 <= v < 0x40000000: return ("heap",)
    return v
def static_mode(f1, f2, f3, f4):
    oa, ob, pa, pb = (load(p) for p in (f1, f2, f3, f4))
    tot = {"zero/tiny": 0, "orig nonzero, product 0": 0, "both nonzero": 0}
    for k in sorted(oa, key=lambda k: k[2]):
        kp = next((x for x in pa if x[:2] == k[:2] and x[2] == k[2]), None)
        if kp is None or k not in ob or kp not in pb: print("slot", k[2], "unpaired"); continue
        So, Sp = oa[k]["S"], pa[kp]["S"]; n_static = 0; cls = {"zero/tiny": 0, "orig nonzero, product 0": 0, "both nonzero": 0}; ex = []
        for i in range(len(oa[k]["w"])):
            o1, o2 = norm(oa[k]["w"][i], So), norm(ob[k]["w"][i], ob[k]["S"])
            p1, p2 = norm(pa[kp]["w"][i], Sp), norm(pb[kp]["w"][i], pb[kp]["S"])
            if o1 is None or o1 != o2 or p1 != p2 or o1 == ("heap",) or p1 == ("heap",): continue
            n_static += 1
            if o1 == p1: continue
            x, y = oa[k]["w"][i], pa[kp]["w"][i]; fx, fy = f(x), f(y)
            if isinstance(o1, tuple) or isinstance(p1, tuple): c = "orig nonzero, product 0" if y == 0 else "both nonzero"
            else: c = "zero/tiny" if abs(fx) < 1e-6 and abs(fy) < 1e-6 else "orig nonzero, product 0" if y == 0 else "both nonzero"
            cls[c] += 1; tot[c] += 1
            if c == "both nonzero": ex.append("S+%04x orig %08x product %08x" % (4 * i, x, y))
        print("slot %2d owner %d ovid %3d model %s: %d static words, differing %s %s" % (k[2], k[0], k[1], pa[kp]["model"], n_static, cls, ex[:6]))
    print("total", tot)
if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "params": params_mode(sys.argv[2], sys.argv[3])
    elif len(sys.argv) == 6 and sys.argv[1] == "static": static_mode(*sys.argv[2:6])
    else: print(__doc__); sys.exit(2)
