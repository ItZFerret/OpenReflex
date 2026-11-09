#!/usr/bin/env python3
"""rng_cmp.py <a.rng> <b.rng> [--from K] [--to K] [--abs] -- the random streams of two runs frame by frame (agent w-rng).

Inputs: the per-frame lines "F <frame> st <state> k <k> rs <CRT _holdrand> gs <g_GameRandSeed> gc <g_GameRandCount> [n <crt> m <lcg>]" of
core/tools/rng_frames.c (the product) or core/oracle/rng_probe.cpp (the original in the emulator; or the core's ports in it, On mode).
The two runs are aligned on k (0 = the frame that enters the countdown). Compared per frame from --from (default 1) on:
  default   the per-frame draw counts n (CRT) and m (game LCG): both runs reseeded at every frame start (RNG_RESEED / RX_RNG_RESEED), so a
            frame's counts depend only on what runs in that frame
  --abs     the states themselves (rs, gs, gc): for two runs of the same world image (the oracle A/B: originals vs the product's
            boundary code), where the streams must stay identical without reseeding
  --by-frame  align on the frame index instead of k (two runs of the same world image; every frame from 0)
Prints the first differing frames (up to 20), the totals, and exits 0 when no compared frame differs."""
import sys

def load(path, by_frame=False):
    out = {}
    for l in open(path):
        p = l.split()
        if not p or p[0] != "F": continue
        d = {"frame": int(p[1])}
        for i in range(2, len(p) - 1, 2): d[p[i]] = p[i + 1]
        k = d["frame"] if by_frame else int(d.get("k", -1))
        if k >= 0: out[k] = d
    return out

def main():
    args = sys.argv[3:]
    byf = "--by-frame" in args
    a, b = load(sys.argv[1], byf), load(sys.argv[2], byf)
    lo = int(args[args.index("--from") + 1]) if "--from" in args else (0 if byf else 1)
    hi = int(args[args.index("--to") + 1]) if "--to" in args else 1 << 30
    keys = ("rs", "gs", "gc") if "--abs" in args else ("n", "m")
    common = sorted(k for k in a if k in b and lo <= k <= hi)
    if not common: print("no common frames (k) to compare"); return 2
    diff, shown = 0, 0
    tot = {x: [0, 0] for x in keys if x in ("n", "m")}
    for k in common:
        da, db = a[k], b[k]
        for x in tot:
            tot[x][0] += int(da.get(x, 0)); tot[x][1] += int(db.get(x, 0))
        bad = [x for x in keys if da.get(x) != db.get(x)]
        if bad:
            diff += 1
            if shown < 20:
                shown += 1
                print("k %d (frames %d / %d, st %s / %s): " % (k, da["frame"], db["frame"], da.get("st"), db.get("st")) +
                      ", ".join("%s %s / %s" % (x, da.get(x), db.get(x)) for x in bad))
    print("compared k %d..%d: %d frames, %d differ%s" % (common[0], common[-1], len(common), diff,
          "".join("; %s total %d / %d" % (x, v[0], v[1]) for x, v in tot.items())))
    return 1 if diff else 0

if __name__ == "__main__":
    sys.exit(main())
