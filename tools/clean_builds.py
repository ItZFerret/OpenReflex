#!/usr/bin/env python3
"""clean_builds.py [--apply] -- frees disk in builds/ by removing REGENERABLE bulk only (dry run by default).
Removed: object files (*.o, *.o.d, obj/ eobj/ trees), oracle / test CLI executables (Mach-O > 5 MB, not dylibs), Unity Library
caches, world images / memory dumps / coverage maps (*.bin *.rmap *.dump) that no text file in builds/, HANDOFF/, core/, native/
references. Logs > 20 MB are gzipped, not removed.
Kept always: anything inside *.app, packages/, */runtime/* (accepted evidence: saves, screenshots, receipts), sol_garage/game files,
windows/plugin/downloads, the 13 preserved targets of sol_windows_import_repair/DIAGNOSTIC_CLEANUP_PLAN.json, every .dylib/.dll/.exe/.zip,
screenshots, JSON / MD receipts. Writes builds/CLEANUP_<date>.tsv (every removed path and size)."""
import os, sys, json, re, gzip, shutil, time, stat
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))); B = os.path.join(ROOT, "builds")
APPLY = "--apply" in sys.argv
plan = json.load(open(os.path.join(B, "sol_windows_import_repair/DIAGNOSTIC_CLEANUP_PLAN.json")))
PROTECT = [t["path"] for t in plan["targets"] if t["path"].startswith(B)]
def protected(p):
    parts = p.split(os.sep)
    if any(x.endswith(".app") for x in parts) or "packages" in parts or "runtime" in parts: return True
    if p.startswith(os.path.join(B, "merge_check")): return True   # the current main's own verification build
    if os.sep + "worlds" + os.sep in p + os.sep: return True       # shared world-test inputs (REFLEX_WORLD_TEST dirs)   # the current main's own verification build
    if p.startswith(os.path.join(B, "sol_garage", "game files")) or p.startswith(os.path.join(B, "windows", "plugin", "downloads")): return True
    return any(p == t or p.startswith(t + os.sep) for t in PROTECT)
# references to dump-like files from text (by "parent/basename")
refs = set()
TXT = (".json", ".md", ".sh", ".py", ".txt", ".tsv", ".c", ".h", ".cpp", ".inc")
pat = re.compile(r"([A-Za-z0-9_.\-]+/[A-Za-z0-9_.\-]+\.(?:bin|rmap|dump))")
for top in (B, os.path.join(ROOT, "HANDOFF"), os.path.join(ROOT, "core"), os.path.join(ROOT, "native")):
    for dp, dns, fs in os.walk(top):
        dns[:] = [d for d in dns if d not in ("eobj", "obj", "Library") and not d.endswith(".app")]
        for f in fs:
            if not f.endswith(TXT): continue
            p = os.path.join(dp, f)
            try:
                if os.path.getsize(p) > 50e6: continue
                for m in pat.findall(open(p, errors="ignore").read()): refs.add(m)
            except OSError: pass
def is_macho(p):
    try:
        with open(p, "rb") as h: m = h.read(4)
    except OSError: return False
    return m in (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe", b"\xfe\xed\xfa\xcf")
rm, gz = [], []
for dp, dns, fs in os.walk(B):
    if protected(dp): dns[:] = []; continue
    for d in list(dns):
        full = os.path.join(dp, d)
        if d in ("obj", "eobj") or (d == "Library" and os.path.isdir(os.path.join(full, "ScriptAssemblies")) or d == "Library" and os.path.isdir(os.path.join(full, "Artifacts"))):
            if not protected(full):
                size = sum(os.lstat(os.path.join(a, f)).st_size for a, _, fl in os.walk(full) for f in fl)
                rm.append((full, size, "dir")); dns.remove(d)
    for f in fs:
        p = os.path.join(dp, f)
        if os.path.islink(p) or protected(p): continue
        s = os.lstat(p).st_size
        key = os.path.basename(dp) + "/" + f
        if f.endswith((".o", ".o.d")): rm.append((p, s, "obj"))
        elif f.endswith((".bin", ".rmap", ".dump")) and key not in refs and s > 1e6: rm.append((p, s, "dump"))
        elif s > 5e6 and not f.endswith((".dylib", ".dll", ".exe", ".zip")) and is_macho(p): rm.append((p, s, "exe"))
        elif f.endswith((".log", ".out", ".err", ".txt")) and s > 20e6: gz.append((p, s))
tot = {}
for p, s, k in rm: tot[k] = tot.get(k, 0) + s
print("remove:", {k: f"{v/1e9:.2f} GB" for k, v in tot.items()}, f"total {sum(tot.values())/1e9:.2f} GB, {len(rm)} entries")
print(f"gzip: {len(gz)} logs, {sum(s for _, s in gz)/1e9:.2f} GB before compression")
if not APPLY: print("dry run (pass --apply to execute)"); sys.exit(0)
log = open(os.path.join(B, time.strftime("CLEANUP_%Y%m%d_%H%M%S.tsv")), "w"); log.write("path\tbytes\tkind\n")
for p, s, k in rm:
    if k == "dir": shutil.rmtree(p, ignore_errors=True)
    else: os.remove(p)
    log.write(f"{p}\t{s}\t{k}\n")
for p, s in gz:
    with open(p, "rb") as i, gzip.open(p + ".gz", "wb", 6) as o: shutil.copyfileobj(i, o)
    os.remove(p); log.write(f"{p}\t{s}\tgzipped\n")
log.close(); print("done")
