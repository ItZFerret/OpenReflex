#!/usr/bin/env python3
"""stop_audit.py [--abort|--indirect] [--tsv] [--vtables] [--summary] [--slot 0xNN] -- the places where the PRODUCT (no RX_ORACLE)
can still stop beyond the literal-address census (census_original_calls.py).

Two kinds of sites, both outside `#ifdef RX_ORACLE` blocks of core/game/*.c, *.h:
  indirect  a rx_call_original(_ex) whose target is NOT a literal address (a variable, a vtable slot, a callback; most are the
            fallback after RX_FN(target) returned NULL: they stop the product only when the target can be an UNPORTED function);
  abort     an explicit stop: act_unreached(...), <prefix>_unreached(...), and the "path not in the reference boot (not ported)"
            messages (build_activity_*, career_*, replay_*, waypoint_*, scene_viewports, build_tables_rdb, build_registry ...).
Comment lines and #define lines are skipped (a macro body is not a call site; the expansions of the FX_ORIG / WORLD_ORIG / B1_ORIG
macros are literal-address calls and belong to the census). The definition of an unreached() helper is not a site.

For each site: file:line, kind, the enclosing function and
  indirect  the target expression; when it is a variable (fn, target, t ...) the expression it was last assigned from in the same
            function (`fn <- U32(U32(obj, 0), 0x58)`), and the vtable slot offset when that has the form U32(U32(x, 0), off);
  abort     the message.
--fatal    adds a third kind: a bare abort() / raise() / exit() of the game code (the "fatal" sites: ABI-frame checks, "Unclosed ...
           dependency" and "rxcore: ... not ported" stops that use fprintf(stderr, ...) + abort() instead of an unreached() helper);
           the nearest fprintf message on the same or the two lines above is the message.
--hidden   the literal targets the census cannot see: addresses passed as a function parameter that ends in rx_call_original(_ex) (resolved
           through the callers, any depth: performance_apply_clip, ik_call_ex, tracked_call, bike_sim_call_stdcall, act_call_fn ...) and as the first argument of the
           wrapper macros (WORLD_CTX / WORLD_ORIG / FX_ORIG / B1_ORIG ...), that are not registered (needs gen_fnsig.py's registrations, run with the preprocessor).
--vtables  also prints, for every vtable slot offset seen at an indirect site, how many class slots of _re/out/vtables.tsv hold an
           UNREGISTERED function (no RX_FUNCTION) -- the candidate unported targets; narrow by the receiver's class by hand.
--slot N   lists those class / function address / size rows for one slot offset (N = byte offset into the vtable).
--summary  counts per file.   --tsv  tab-separated output (file:line, kind, function, target/message, slot).
The classification of every site lives in core/plan/STOP_AUDIT.md; the dynamic half (the product run once per event the shell offers)
is core/tools/stop_audit_run.c + stop_audit_matrix.py. To audit again: run this tool and compare its site list with the table in
that file (a site that is not in the table is new)."""
import re, glob, os, sys, collections
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
G = sorted(glob.glob(os.path.join(R, "core/game/*.c")) + glob.glob(os.path.join(R, "core/game/*.h")))
ABORT = re.compile(r"\b(?:[a-z0-9]+_)*unreached\s*\(|not in the reference boot")
HELPER_DEF = re.compile(r"\b(?:static\s+)?(?:inline\s+)?void\s+\w*unreached\s*\(")
CALL = re.compile(r"rx_call_original(?:_ex)?\s*\(")
FATAL = re.compile(r"\babort\s*\(\s*\)|\braise\s*\(|\bexit\s*\(")


def first_arg(s, i):
    """The text of the first argument of the call whose '(' ends at s[i-1]."""
    depth = 0; j = i
    while j < len(s):
        c = s[j]
        if c in "([{": depth += 1
        elif c in ")]}":
            if depth == 0: break
            depth -= 1
        elif c == "," and depth == 0: break
        j += 1
    return s[i:j].strip()


def strip_comments(ln):
    ln = re.sub(r"/\*.*?\*/", "", ln)
    return re.sub(r"//.*", "", ln)


def sites():
    out = []
    for f in G:
        lines = open(f, errors="replace").read().split("\n")
        stack = []; fn = "?"; start = 0; in_macro = False
        for i, ln in enumerate(lines):
            s = ln.strip()
            if s.startswith("#if"): stack.append(s.startswith("#ifdef RX_ORACLE") or s.startswith("#if defined(RX_ORACLE)"))
            elif s.startswith("#else") and stack: stack[-1] = False
            elif s.startswith("#endif") and stack: stack.pop()
            m = re.match(r"^(?:static\s+|inline\s+)*[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;]*$", ln) if ln and not ln[0].isspace() and ln[0] not in "#/*}" else None
            if m and not re.match(r"^(if|for|while|switch|return|else)\b", ln): fn = m.group(1); start = i
            if any(stack): continue
            if s.startswith("#define") or in_macro:
                in_macro = s.endswith("\\"); continue
            if s.startswith(("//", "*", "/*", "#")): continue
            body = strip_comments(ln)
            for m in CALL.finditer(body):
                t = first_arg(body, m.end())
                if re.fullmatch(r"0x[0-9a-fA-F]+", t): continue
                via = ""
                if re.fullmatch(r"[A-Za-z_]\w*", t):   # a variable: its last assignment in this function
                    for k in range(i, max(start - 1, i - 80), -1):
                        mm = re.search(r"\b" + re.escape(t) + r"\s*=(?!=)\s*([^;]+);", strip_comments(lines[k]))
                        if mm: via = mm.group(1).strip(); break
                out.append((os.path.relpath(f, R), i + 1, "indirect", fn, t, via))
            if ABORT.search(body) and not HELPER_DEF.search(body) and "%08x %s" not in body:
                mm = re.search(r'"((?:[^"\\]|\\.)*)"', ln)
                out.append((os.path.relpath(f, R), i + 1, "abort", fn, mm.group(1) if mm else s[:90], ""))
            elif "%08x %s" not in body and FATAL.search(body) and not HELPER_DEF.search(body) and not re.search(r"\bvoid\s+\w*unreached\b", body):
                msg = ""
                for k in range(i, max(i - 3, -1), -1):
                    mm = re.search(r'fprintf\(\s*stderr\s*,\s*"((?:[^"\\]|\\.)*)"', lines[k])
                    if mm: msg = mm.group(1)[:100]; break
                out.append((os.path.relpath(f, R), i + 1, "fatal", fn, msg or s[:90], ""))
    return out


def slot_of(t, via):
    for e in (t, via):
        m = re.search(r"U32\(\s*U32\(.*?,\s*0\s*\)\s*,\s*(0x[0-9a-fA-F]+|\d+)\s*\)", e or "")
        if m: return int(m.group(1), 0)
        m = re.search(r"\bvfn\(.*?,\s*(\d+)\s*\)|race_vfn\(.*?,\s*(\d+)\s*\)", e or "")
        if m: return 4 * int(m.group(1) or m.group(2))
    return None


def registered():
    return {int(m.group(1), 16) for f in glob.glob(os.path.join(R, "core/game/*.c")) for m in re.finditer(r"RX_FUNCTION\(\s*0x([0-9a-fA-F]+)", open(f, errors="replace").read())}


def unported_by_slot(slots, reg):
    F = {}
    for l in open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1").readlines()[1:]:
        c = l.split("\t"); F[int(c[0], 16)] = int(c[3] or 0)
    res = collections.defaultdict(list)
    for l in open(os.path.join(R, "_re/out/vtables.tsv"), encoding="latin-1").readlines()[1:]:
        cls, vt, slot, tgt, name = (l.rstrip("\n").split("\t") + [""] * 5)[:5]
        if int(slot) * 4 in slots and int(tgt, 16) not in reg: res[int(slot) * 4].append((cls, int(tgt, 16), F.get(int(tgt, 16), 0)))
    return res


MACROS = "FX_ORIG FX_ORIG0 B1_ORIG B1_ORIG0 B1_EX B1_EX0 WORLD_ORIG WORLD_ORIG0 WORLD_ORIG_EX WORLD_ORIG_EX0 WORLD_CTX WORLD_CTX0 WORLD_CTX_F".split()


def all_registered():
    """every registered address: gen_fnsig.py --report (macro families included); falls back to the literal RX_FUNCTION scan."""
    import subprocess, tempfile
    g = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gen_fnsig.py")
    try:
        o = os.path.join(tempfile.mkdtemp(), "fnsig.c")
        r = subprocess.run([sys.executable, g, o, "--report"], capture_output=True, text=True, timeout=600)
        src = open(o).read()
        s = {int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{8})", src)}
        for l in r.stderr.splitlines():
            m = re.match(r"gen_fnsig:\s+non-integer\s+([0-9a-fA-F]{8})", l)
            if m: s.add(int(m.group(1), 16))
        if len(s) > 1000: return s
    except Exception: pass
    return registered()


def split_args(s, i):
    args = []; depth = 0; cur = ""
    while i < len(s):
        c = s[i]
        if c in "([{": depth += 1; cur += c
        elif c in ")]}":
            if depth == 0: args.append(cur.strip()); return args
            depth -= 1; cur += c
        elif c == "," and depth == 0: args.append(cur.strip()); cur = ""
        else: cur += c
        i += 1
    return args


def oracle_lines(text):
    st = []; res = set()
    for i, ln in enumerate(text.split("\n"), 1):
        s = ln.strip()
        if s.startswith("#if"): st.append(s.startswith("#ifdef RX_ORACLE") or s.startswith("#if defined(RX_ORACLE)"))
        elif s.startswith("#else") and st: st[-1] = False
        elif s.startswith("#endif") and st: st.pop()
        if any(st): res.add(i)
    return res


def hidden_targets():
    """[(address, file:line, via)] of the unregistered literals reaching rx_call_original(_ex) through helper parameters and wrapper macros."""
    reg = all_registered()
    texts = {f: open(f, errors="replace").read() for f in G}
    orc = {f: oracle_lines(t) for f, t in texts.items()}
    lit = re.compile(r"\(?\s*0x0*([4-8][0-9a-fA-F]{5})u?\s*\)?")
    found = collections.defaultdict(set)
    # macros
    mp = re.compile(r"\b(" + "|".join(MACROS) + r")\s*\(")
    for f, t in texts.items():
        for m in mp.finditer(t):
            ln = t.count("\n", 0, m.start()) + 1
            ls = t.rfind("\n", 0, m.start()) + 1
            if ln in orc[f] or t[ls:m.start()].lstrip().startswith("#define"): continue
            a = split_args(t, m.end())
            if a and lit.fullmatch(a[0]): found[int(lit.fullmatch(a[0]).group(1), 16)].add((os.path.relpath(f, R) + ":%d" % ln, m.group(1)))
    # helper parameters, breadth first through the callers
    def param_index(fname, var):
        for t in texts.values():
            m = re.search(r"\b" + re.escape(fname) + r"\s*\(([^)]*)\)\s*\{", t)
            if m:
                for i, p in enumerate(x.strip() for x in m.group(1).split(",")):
                    if re.search(r"\b" + re.escape(var) + r"\s*$", p): return i
        return None
    def callers(fname, idx):
        res = []
        pat = re.compile(r"\b" + re.escape(fname) + r"\s*\(")
        for f, t in texts.items():
            for m in pat.finditer(t):
                ls = t.rfind("\n", 0, m.start()) + 1
                pre = t[ls:m.start()]
                if re.search(r"\b(void|int|uint32_t|rx_addr|float|static|inline|int32_t|double|rx_regs|uint8_t|uint64_t)\b", pre) and not pre.strip().startswith(("return", "else")): continue
                ln = t.count("\n", 0, m.start()) + 1
                if ln in orc[f]: continue
                a = split_args(t, m.end())
                if idx < len(a): res.append((f, ln, a[idx], t))
        return res
    queue = []; seen = set()
    for f, l, k, fn, tg, via in sites():
        if k == "indirect" and not via and re.fullmatch(r"[A-Za-z_]\w*", tg):
            i = param_index(fn, tg)
            if i is not None: queue.append((fn, i))
    while queue:
        fn, idx = queue.pop()
        if (fn, idx) in seen: continue
        seen.add((fn, idx))
        for f, ln, a, t in callers(fn, idx):
            m = lit.fullmatch(a)
            if m: found[int(m.group(1), 16)].add((os.path.relpath(f, R) + ":%d" % ln, fn)); continue
            if not re.fullmatch(r"[A-Za-z_]\w*", a): continue
            lines = t.split("\n")
            for k in range(ln - 1, max(0, ln - 500), -1):
                m2 = re.match(r"^(?:static\s+|inline\s+)*[A-Za-z_][\w\s\*]*?\b(\w+)\s*\(([^;{]*)\)\s*\{?\s*$", lines[k])
                if m2 and not lines[k].startswith((" ", "\t", "#")) and m2.group(1) not in ("if", "for", "while", "switch"):
                    for i, p in enumerate(x.strip() for x in m2.group(2).split(",")):
                        if re.search(r"\b" + re.escape(a) + r"\s*$", p): queue.append((m2.group(1), i))
                    break
    return [(a, sorted(found[a])) for a in sorted(found) if a not in reg and 0x401000 <= a < 0x8f0000]


if __name__ == "__main__":
    a = sys.argv[1:]; S = sites()
    if "--abort" in a: S = [s for s in S if s[2] == "abort"]
    if "--indirect" in a: S = [s for s in S if s[2] == "indirect"]
    if "--fatal" not in a: S = [s for s in S if s[2] != "fatal"]
    if "--hidden" in a:
        sz = {}
        try:
            for l in open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1").readlines()[1:]:
                c = l.split("\t"); sz[int(c[0], 16)] = int(c[3] or 0)
        except OSError: pass
        for ad, via in hidden_targets(): print("%08x\t%s\t%s" % (ad, sz.get(ad, "?"), "; ".join("%s (%s)" % v for v in via[:3])))
        sys.exit(0)
    if "--slot" in a:
        sl = int(a[a.index("--slot") + 1], 0)
        for cls, tgt, sz in sorted(unported_by_slot({sl}, registered())[sl]): print(f"{cls}\t{tgt:08x}\t{sz}")
        sys.exit(0)
    if "--summary" in a:
        for k, v in sorted(collections.Counter((s[0], s[2]) for s in S).items()): print(f"{v:4}  {k[1]:8} {k[0]}")
    else:
        for f, l, k, fn, t, via in S:
            sl = slot_of(t, via) if k == "indirect" else None
            tx = t + (f"  <- {via}" if via else "")
            if "--tsv" in a: print(f"{f}:{l}\t{k}\t{fn}\t{tx}\t{('0x%x' % sl) if sl is not None else ''}")
            else: print(f"{f}:{l}  {k:8} {fn}  {tx[:130]}" + (f"  slot=0x{sl:x}" if sl is not None else ""))
    c = collections.Counter(s[2] for s in S)
    print(f"# {c['indirect']} indirect rx_call_original sites, {c['abort']} abort sites" + (f", {c['fatal']} fatal (bare abort) sites" if c['fatal'] else ""), file=sys.stderr)
    if "--vtables" in a:
        slots = {slot_of(t, v) for _, _, k, _, t, v in S if k == "indirect" and slot_of(t, v) is not None}
        for sl, v in sorted(unported_by_slot(slots, registered()).items()):
            print(f"slot 0x{sl:x}: {len(v)} class slots with an unregistered function")
