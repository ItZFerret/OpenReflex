#!/usr/bin/env python3
"""rng_audit.py [--tsv out.tsv] [--md out.md] [--sites] [--callers 0xADDR] -- RNG parity audit (agent w-rng, 2026-10-27).

Question: where does the ORIGINAL draw from one of the game's random number generators inside code that the PRODUCT (librxcore, no
RX_ORACLE) does not run as the original does? Verify / On mode cannot see such a skip: the oracle runs the original functions, so a
product-only stub or a boundary that leaves out a draw is invisible there, but every later draw of the product (AI, physics variation,
effects ...) then differs from the original's.

1. The generators, found in MXReflex.exe itself (not from a list):
     crt    msvcr90 rand() through the import slot 0x0091624c (`call [slot]`, or `mov reg, [slot]` + `call reg`): the per-thread
            _holdrand (ptd + 0x14), seed * 0x343fd + 0x269ec3; srand() through 0x009162f8.
     lcg    the game's own generator g_GameRandSeed 0x009cb860 (count g_GameRandCount 0x009cb864), always inlined:
            seed = seed * 0x41c64e6d + 0x3039 (`imul r, r, 0x41c64e6d`), (seed >> 16) & 0x7fff.
     other  every other LCG / twister constant the scan looks for (0x343fd, 0x19660d, 69069 + 0x9908b0df (a Mersenne twister object),
            16807 / 48271 (MINSTD), golden ratio): the hits are listed with their function and the reason they are not a shared stream
            (see OTHER below).
   A function containing a draw site is a DRAWER. Draw sites outside Ghidra's function bounds are attributed to the preceding function
   when no int3 padding separates them, else to the entry recovered after the padding (reported as such).
2. The core: every registration RX_FUNCTION(0x..., fn) of core/game/*.c and every C function definition. A C function DRAWS when its
   body (comments removed; for the product view the RX_ORACLE-only regions removed as census_original_calls.py does) calls crt_rand /
   race_crt_rand, touches 0x009cb860 / 0x009cb864 / 0x41c64e6d, or calls a C function that draws (fix point; static helpers resolved
   in their own file first). Private generators (a `holdrand` of the core's own, any other LCG constant) are reported separately.
3. Per drawer: registered? the port's draw in the product view and in the oracle view; else the core sources that name the address and
   the drawer's callers (direct calls, vtable slots, pushed / stored function addresses) with their registration. The VERDICT column is
   the hand-checked classification (VERDICT below), with the evidence; a drawer without a verdict is printed as UNREVIEWED (exit 1).

Verdicts:
  ok          the product runs the port and the port draws as the original (verified per call in the oracle where the worlds reach it)
  fixed       the product skipped the draw; fixed by w-rng / w-sfx (where and how in the evidence)
  host        the original's draw is in a boundary the Unity side implements; the core now makes the draw and exports the result
  no-stream   the draw does not touch the stream the game state uses (another thread's _holdrand, a function-local generator ...)
  not-reached the product never reaches the original's caller chain and neither does the original during an offline race / free ride /
              career event (front end only, online only, development paths); evidence names the chain
  left        a skip that remains (why, and what it would take)
"""
import bisect, collections, csv, glob, os, re, struct, sys

R = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
EXE = os.environ.get("REFLEX_EXE") or os.path.join(R, "game files", "Archive", "MXReflex.exe")
RAND_SLOT, SRAND_SLOT = 0x0091624C, 0x009162F8
SEED, COUNT, LCG_MUL = 0x009CB860, 0x009CB864, 0x41C64E6D

# ------------------------------------------------------------------------------------------------------------------------------------
# the hand-checked classification of every drawer (address -> (verdict, evidence)); kept in this file so the audit is one tool
VERDICT = {}
# loaded from rng_audit_verdicts.tsv next to this file (addr \t verdict \t evidence): the table is long, the tool stays readable
_vf = os.path.join(os.path.dirname(os.path.abspath(__file__)), "rng_audit_verdicts.tsv")
if os.path.exists(_vf):
    for row in csv.reader(open(_vf, encoding="utf-8"), delimiter="\t", quoting=csv.QUOTE_NONE):
        if not row or row[0].startswith("#") or len(row) < 3: continue
        VERDICT[int(row[0], 16)] = (row[1].strip(), row[2].strip())

# ------------------------------------------------------------------------------------------------------------------------------------
def load_image():
    import pefile
    pe = pefile.PE(EXE, fast_load=True)
    return pe.get_memory_mapped_image(), pe.OPTIONAL_HEADER.ImageBase


IMG, BASE = load_image()
FN = {}                                   # entry -> (name, size)
for l in open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.rstrip("\n").split("\t")
    FN[int(c[0], 16)] = (c[1], int(c[3] or 0))
ENTRIES = sorted(FN)
TEXT_LO, TEXT_HI = 0x00401000, 0x00916000


def byte(a): return IMG[a - BASE]


def finds(pat):
    out, i = [], 0
    while True:
        i = IMG.find(pat, i)
        if i < 0: return out
        a = BASE + i
        if TEXT_LO <= a < TEXT_HI: out.append(a)
        i += 1


RECOVERED = {}                            # recovered entry -> reason


def owner(a):
    """the function a site belongs to (Ghidra bounds; else the preceding function when no int3 padding lies between; else the entry
    after the padding)"""
    k = bisect.bisect_right(ENTRIES, a) - 1
    if k < 0: return None
    e = ENTRIES[k]; size = max(FN[e][1], 1)
    if a < e + size: return e
    gap = range(e + size, a)
    pads = [x for x in gap if byte(x) == 0xCC]
    if not pads:
        return e
    # entry after the last int3 run before a
    x = a
    while x > e + size and byte(x - 1) != 0xCC: x -= 1
    RECOVERED[x] = "recovered after int3 padding (no Ghidra function)"
    return x


def fname(e):
    if e in FN: return FN[e][0]
    return "FUN_%08x(recovered)" % e if e in RECOVERED else "?%08x" % e

# ---- 1. the draw sites ----------------------------------------------------------------------------------------------------------------
SITES = collections.defaultdict(list)    # drawer -> [(site, kind)]


def scan_slot(slot, kind):
    p = struct.pack("<I", slot)
    hits = [(a - 2, "call") for a in finds(b"\xff\x15" + p)]
    for op in (0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D):
        hits += [(a - 2, "mov") for a in finds(bytes([0x8B, op]) + p)]
    hits += [(a - 1, "mov") for a in finds(b"\xa1" + p)]
    for a, how in hits:
        SITES[owner(a)].append((a, kind if how == "call" else kind + "*"))   # "*": the slot loaded into a register, called through it


scan_slot(RAND_SLOT, "crt")
scan_slot(SRAND_SLOT, "srand")
def lcg_stream(fn_entry, site):
    """the generator an `imul 0x41c64e6d` steps: the game's (g_GameRandSeed) when the seed word 0x9cb860 is read or written near it or
    anywhere in the function (a seed kept in a register over a loop); else the function's own seed word (a read and written global
    next to the site): "lcg@<addr>" """
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    size = max(FN.get(fn_entry, ("", 0))[1], site - fn_entry + 64)
    ins = list(md.disasm(IMG[fn_entry - BASE:fn_entry - BASE + size + 64], fn_entry))
    idx = [i for i, x in enumerate(ins) if x.address == site]
    near = ins[max(0, idx[0] - 12):idx[0] + 12] if idx else []
    mem = lambda xs: {int(m, 16) for x in xs for m in re.findall(r"\[(0x[0-9a-f]+)\]", x.op_str)}
    if SEED in mem(near) or COUNT in mem(near): return "lcg"
    own = sorted(a for a in mem(near) if 0x009c0000 <= a < 0x00e00000 and
                 any(x.mnemonic == "mov" and x.op_str.startswith("dword ptr [0x%x]" % a) for x in ins))
    if own: return "lcg@%08x" % own[0]
    if SEED in mem(ins) or COUNT in mem(ins): return "lcg"
    return "lcg?"


for a in finds(struct.pack("<I", LCG_MUL)):
    e = owner(a)
    SITES[e].append((a - 2, lcg_stream(e, a - 2)))
# seed writes that are not a step (seeding): mov [0x9cb860], reg / imm
for a in finds(struct.pack("<I", SEED)):
    # 89 05/0d/15/1d/2d/35/3d [seed] = mov [seed], reg; a3 [seed] = mov [seed], eax; c7 05 [seed] imm
    if byte(a - 2) == 0x89 or byte(a - 1) == 0xA3 or (byte(a - 2) == 0xC7 and byte(a - 1) == 0x05):
        e = owner(a)
        if not any(k.startswith("lcg") for _, k in SITES.get(e, [])): SITES[e].append((a, "seed"))

OTHER = [  # (constant, what, why it is not one of the shared streams) -- the scan checks the hits still are where the reason says
    (0x10DCD, "69069 seeding of a Mersenne twister object (FUN_008ca5b0, state in the object, seed 4357)", "008ca5b0"),
    (0x9908B0DF, "the twister's matrix constant (FUN_008ca740: the object's ctor)", "008ca740"),
]
MT_DRAW = 0x008CA5F0                      # the twister's draw (state + 0x9c0 index)

# ---- 2. the call graph ---------------------------------------------------------------------------------------------------------------
def build_callers():
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    callers = collections.defaultdict(set)   # callee -> {(caller, how)}
    eset = set(ENTRIES) | set(RECOVERED)
    for e in ENTRIES:
        size = FN[e][1]
        if size <= 0: continue
        code = IMG[e - BASE:e - BASE + size]
        for ins in md.disasm(code, e):
            if ins.mnemonic in ("call", "jmp") and ins.op_str.startswith("0x"):
                t = int(ins.op_str, 16)
                if t in eset and t != e: callers[t].add((e, "call" if ins.mnemonic == "call" else "jmp"))
            elif ins.mnemonic in ("push", "mov") and "0x" in ins.op_str:
                for m in re.finditer(r"0x([0-9a-f]+)", ins.op_str):
                    t = int(m.group(1), 16)
                    if t in eset and t != e and TEXT_LO <= t < TEXT_HI: callers[t].add((e, "ref"))
    for row in csv.DictReader(open(os.path.join(R, "_re/out/vtables.tsv"), encoding="latin-1"), delimiter="\t"):
        t = int(row["target"], 16)
        callers[t].add((row["class"] + "::vf" + row["slot"], "vtable"))
    return callers

# ---- 3. the core ---------------------------------------------------------------------------------------------------------------------
GAME = sorted(glob.glob(os.path.join(R, "core/game/*.c")) + glob.glob(os.path.join(R, "core/game/*.h")))
API = sorted(glob.glob(os.path.join(R, "core/api/*.c")) + glob.glob(os.path.join(R, "core/api/*.inc")))


def blank_comments(t):
    out, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith("/*", i):
            j = t.find("*/", i + 2); j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", t[i:j])); i = j
        elif t.startswith("//", i):
            j = t.find("\n", i); j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif c in "\"'":
            j = i + 1
            while j < n and t[j] != c and t[j] != "\n":
                j += 2 if t[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + re.sub(r"[^\n]", " ", t[i + 1:j - 1]) + (c if j - i >= 2 else "")); i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def view(lines, product):
    """product=True: the RX_ORACLE-only regions blanked; product=False: the product-only regions (#ifndef RX_ORACLE ...) blanked"""
    stack, res = [], []
    for ln in lines:
        s = ln.strip()
        if s.startswith("#"):
            d = re.sub(r"\s+", " ", s[1:].strip())
            if d.startswith("ifdef RX_ORACLE") or re.match(r"if defined\(RX_ORACLE\)\s*$", d) or d == "if RX_ORACLE":
                stack.append([not product, True]); res.append(""); continue
            if d.startswith("ifndef RX_ORACLE") or re.match(r"if !defined\(RX_ORACLE\)\s*$", d):
                stack.append([product, True]); res.append(""); continue
            if d.startswith(("ifdef", "ifndef", "if ")) or d == "if": stack.append([True, False]); res.append(""); continue
            if d.startswith("else") and stack:
                if stack[-1][1]: stack[-1][0] = not stack[-1][0]
                res.append(""); continue
            if d.startswith("elif") and stack: res.append(""); continue
            if d.startswith("endif") and stack: stack.pop(); res.append(""); continue
        res.append(ln if all(e[0] or not e[1] for e in stack) else "")
    return res


DEF_RE = re.compile(r"^[A-Za-z_][\w \t\*]*?\b([A-Za-z_]\w*)\s*\(([^;{}()]*(?:\([^()]*\)[^;{}()]*)*)\)\s*\{", re.M)
PRIM_RE = re.compile(r"\b(?:crt_rand|race_crt_rand)\s*\(|0x0*9cb86[04]\b|0x41c64e6d\b", re.I)
PRIVATE_RE = re.compile(r"holdrand|0x343fd\b|\b214013u?\b|1664525u?\b|0x19660d\b|69069\b", re.I)
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "defined", "RX_FUNCTION"}


def match_close(t, i):
    d = 0
    for j in range(i, len(t)):
        if t[j] == "{": d += 1
        elif t[j] == "}":
            d -= 1
            if d == 0: return j
    return len(t) - 1


class CFn:
    __slots__ = ("name", "file", "line", "body", "static", "prim", "private", "calls", "draws")


def parse_core(product):
    fns, byname = [], collections.defaultdict(list)
    for f in GAME + API:
        raw = open(f, encoding="latin-1").read()
        code = blank_comments("\n".join(view(raw.split("\n"), product)))
        for m in DEF_RE.finditer(code):
            name = m.group(1)
            if name in KEYWORDS: continue
            ob = code.index("{", m.end() - 1)
            body = code[ob:match_close(code, ob) + 1]
            fn = CFn(); fn.name = name; fn.file = f; fn.line = code.count("\n", 0, m.start()) + 1; fn.body = body
            fn.static = "static" in m.group(0).split(name)[0]
            fn.prim = bool(PRIM_RE.search(body)); fn.private = bool(PRIVATE_RE.search(body))
            fn.calls = {c for c in CALL_RE.findall(body) if c not in KEYWORDS and c != name}
            fn.draws = fn.prim and name != "crt_rand"
            fns.append(fn); byname[name].append(fn)
    # macros that draw (static inline helpers are functions above; #define helpers here)
    for f in GAME:
        code = blank_comments("\n".join(view(open(f, encoding="latin-1").read().split("\n"), product)))
        for m in re.finditer(r"#\s*define\s+(\w+)\s*\([^)]*\)(.*(?:\\\n.*)*)", code):
            if PRIM_RE.search(m.group(2)):
                fn = CFn(); fn.name = m.group(1); fn.file = f; fn.line = code.count("\n", 0, m.start()) + 1; fn.body = m.group(2)
                fn.static = False; fn.prim = True; fn.private = False; fn.calls = set(); fn.draws = True
                fns.append(fn); byname[fn.name].append(fn)

    def resolve(name, f):
        c = byname.get(name)
        if not c: return []
        same = [x for x in c if x.file == f]
        return same if same else [x for x in c if not x.static] or c
    changed = True
    while changed:
        changed = False
        for fn in fns:
            if fn.draws: continue
            for c in fn.calls:
                if any(x.draws for x in resolve(c, fn.file)):
                    fn.draws = True; changed = True; break
    return fns, byname, resolve


def registrations():
    reg = {}
    for f in GAME:
        t = blank_comments(open(f, encoding="latin-1").read())
        for m in re.finditer(r"RX_FUNCTION\(\s*0x([0-9a-fA-F]+)\s*,\s*(\w+)\s*\)", t):
            reg.setdefault(int(m.group(1), 16), (m.group(2), f, t.count("\n", 0, m.start()) + 1))
    return reg


def mentions():
    """address -> [file:line] of the core sources (game, api, oracle) whose text names it (comments included)"""
    out = collections.defaultdict(list)
    for f in GAME + API + [x for x in sorted(glob.glob(os.path.join(R, "core/oracle/*.cpp"))) if not os.path.basename(x).startswith("rng_")]:
        for i, ln in enumerate(open(f, encoding="latin-1"), 1):
            for m in re.finditer(r"\b(?:0x)?(00[4-8][0-9a-fA-F]{5})\b", ln):
                out[int(m.group(1), 16)].append("%s:%d" % (os.path.relpath(f, R), i))
    return out


def main():
    args = sys.argv[1:]
    want_sites = "--sites" in args
    callers = build_callers()
    if "--callers" in args:
        a = int(args[args.index("--callers") + 1], 16)
        for c, how in sorted(callers.get(a, []), key=str): print(how, c if isinstance(c, str) else "%08x %s" % (c, fname(c)))
        return 0
    reg = registrations()
    pfns, pby, presolve = parse_core(True)
    ofns, oby, oresolve = parse_core(False)
    ment = mentions()

    def core_draw(cname, f, by, resolve):
        c = resolve(cname, f)
        return any(x.draws for x in c) if c else None
    rows = []
    for d in sorted(SITES, key=lambda x: x or 0):
        sites = SITES[d]
        kinds = collections.Counter(k.rstrip("*") for _, k in sites)
        r = reg.get(d)
        if r:
            cname, f, line = r
            pd, od = core_draw(cname, f, pby, presolve), core_draw(cname, f, oby, oresolve)
            core = "%s (%s:%d) product %s oracle %s" % (cname, os.path.relpath(f, R), line,
                                                         "draws" if pd else "NO DRAW", "draws" if od else "NO DRAW")
        else:
            m = sorted(set(x.split(":")[0] for x in ment.get(d, [])))
            core = "unregistered" + ((" (named in " + ", ".join(m[:4]) + (" ..." if len(m) > 4 else "") + ")") if m else "")
        cl = callers.get(d, set())
        cdesc = []
        for c, how in sorted(cl, key=str):
            if isinstance(c, str): cdesc.append("%s[vt]" % c)
            else: cdesc.append("%08x%s" % (c, "+" if c in reg else "-"))
        v = VERDICT.get(d, ("UNREVIEWED", ""))
        rows.append(dict(addr="%08x" % d, name=fname(d), kinds=" ".join("%s:%d" % kv for kv in sorted(kinds.items())),
                         sites=" ".join("%x" % a for a, _ in sites), core=core, callers=" ".join(cdesc[:12]) + (" ..." if len(cdesc) > 12 else ""),
                         verdict=v[0], evidence=v[1]))
    # private generators in the core
    privs = [x for x in pfns if x.private]
    # report
    cols = ["addr", "name", "kinds", "verdict", "core", "callers", "evidence"] + (["sites"] if want_sites else [])
    out_tsv = args[args.index("--tsv") + 1] if "--tsv" in args else None
    out_md = args[args.index("--md") + 1] if "--md" in args else None
    if out_tsv:
        with open(out_tsv, "w") as fo:
            fo.write("\t".join(cols) + "\n")
            for r in rows: fo.write("\t".join(r[c] for c in cols) + "\n")
    cnt = collections.Counter(r["verdict"] for r in rows)
    if out_md:
        write_md(out_md, rows, privs, cnt)
    if not out_tsv and not out_md:
        for r in rows: print("\t".join(r[c] for c in cols))
    print("drawers %d (crt %d, lcg %d, srand / seed %d); verdicts %s; recovered entries %s; core private generators: %s" % (
        len(rows), sum(1 for r in rows if "crt" in r["kinds"]), sum(1 for r in rows if "lcg" in r["kinds"]),
        sum(1 for r in rows if "srand" in r["kinds"] or "seed" in r["kinds"]), dict(sorted(cnt.items())),
        " ".join("%08x" % e for e in sorted(RECOVERED)) or "none",
        " ".join("%s(%s:%d)" % (x.name, os.path.basename(x.file), x.line) for x in privs) or "none"), file=sys.stderr)
    return 1 if cnt.get("UNREVIEWED") else 0


def write_md(path, rows, privs, cnt):
    L = []
    L.append("# RNG_AUDIT: the original's random draws the product must make too (generated by core/tools/rng_audit.py; verdicts in core/tools/rng_audit_verdicts.tsv)\n")
    L.append("Regenerate: `_re/.venv/bin/python core/tools/rng_audit.py --md core/plan/RNG_AUDIT.md --tsv core/plan/rng_audit.tsv`. "
             "The tool's docstring has the method; QUEUE.md \"w-rng\" the fixes and the per-frame product-vs-oracle RNG comparison.\n")
    L.append("Generators: msvcr90 `rand()` (import 0x0091624c, the per-thread `_holdrand` ptd + 0x14; `srand` 0x009162f8) and the game's "
             "LCG g_GameRandSeed 0x009cb860 / g_GameRandCount 0x009cb864 (inlined `imul 0x41c64e6d`). Other constants found: a Mersenne "
             "twister object (008ca5b0 / 008ca5f0 / 008ca740, state in the object) and the MINSTD multipliers of the hash maps' bucket "
             "hash (00820df0 / 00842630 ...: no state): not shared streams.\n")
    L.append("## Counts\n")
    L.append("| verdict | drawers |\n|---|---|")
    for k, v in sorted(cnt.items()): L.append("| %s | %d |" % (k, v))
    L.append("")
    L.append("## Drawers\n")
    L.append("Columns: the original function (draw kinds: crt / lcg / srand / seed and their site count), the verdict, the core "
             "(registration; whether the port's body draws in the product view and in the oracle view, helpers followed), the original "
             "callers (`+` registered in the core, `-` not; `[vt]` vtable slots), the evidence.\n")
    L.append("| addr | function | draws | verdict | core | callers | evidence |\n|---|---|---|---|---|---|---|")
    esc = lambda s: s.replace("|", "\\|")
    for r in rows:
        L.append("| %s | %s | %s | **%s** | %s | %s | %s |" % (r["addr"], esc(r["name"]), r["kinds"], r["verdict"], esc(r["core"]),
                                                             esc(r["callers"]), esc(r["evidence"])))
    L.append("")
    L.append("## Private generators in the core (not the game's streams)\n")
    for x in privs: L.append("- `%s` %s:%d" % (x.name, os.path.relpath(x.file, R), x.line))
    L.append("")
    open(path, "w").write("\n".join(L))


if __name__ == "__main__":
    sys.exit(main())
