#!/usr/bin/env python3
"""census_original_calls.py [out.tsv] [-v] -- the integration sweep's work list: every original function the game code (core/game/*.c, *.h)
still calls through rx_call_original(_ex), outside #ifdef RX_ORACLE blocks (import slots U32(0, slot) excluded).

Two kinds of call sites are found:
  direct   rx_call_original(0x00412345, ...) / rx_call_original_ex(0x..., ...)
  macro    a call of any macro or helper function whose expansion reaches rx_call_original / rx_call_original_ex with one of ITS PARAMETERS
           as the target address (WORLD_CTX, WORLD_ORIG, WORLD_ORIG_EX, B1_ORIG, B1_EX, FX_ORIG, world_call_ctx, crt_unported, ...), with a
           literal address (or a symbol that resolves to one: #define / enum / `const x = 0x...`) in that argument position.
           The wrapper set is derived from the sources (fix point over the definitions), not listed by hand. A wrapper whose target is
           computed (rx_vcall, vtable reads, `fn = U32(...)`) is not a literal call and is not counted.
Columns: addr, name, size, registered (1 = a core port exists: switch the call to the owner's function), files, via, class.
  via   = the macro / helper names the call goes through with the target's argument index ("direct" for a literal rx_call_original).
  class = unported | ported (a port exists: switch) | boundary (documented: a "BOUNDARY (online only)" comment on the call or the 2 lines above it,
          a file whose first 4000 bytes say "ONLINE-SESSION BOUNDARY", or a call through the net_boundary.h rx_net_call* helpers, which
          are fatal in the product by design).
Summary on stderr; -v also lists the wrappers found."""
import re, glob, os, sys, collections
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
F = {}
for l in open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.split("\t"); F[int(c[0], 16)] = (c[1], int(c[3] or 0))
FILES = sorted(glob.glob(os.path.join(R, "core/game/*.c")) + glob.glob(os.path.join(R, "core/game/*.h")))
reg = set()
for f in FILES:
    if f.endswith(".c"):
        reg |= {int(m.group(1), 16) for m in re.finditer(r"RX_FUNCTION\(\s*0x([0-9a-fA-F]+)", open(f, encoding="latin-1").read())}

def int_signature_set():
    """addresses of the registered functions whose C signature is integers only: the product's rx_call_original (non-ex) may call those (core/tools/gen_fnsig.py)"""
    import subprocess, tempfile
    try:
        o = os.path.join(tempfile.mkdtemp(), "fnsig.c")
        subprocess.run([sys.executable, os.path.join(R, "core/tools/gen_fnsig.py"), o], capture_output=True, text=True, timeout=600)
        src = open(o).read()
        m = re.search(r"rx_fnsig_int_addr\[\]\s*=\s*\{([^}]*)\}", src, re.S)
        return {int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", m.group(1))} if m else set()
    except Exception:
        return set()
INTSIG = int_signature_set()
BASE = {"rx_call_original": 0, "rx_call_original_ex": 0}       # wrapper name -> index of the target-address argument
# helpers that are fatal in the product by design and say so in their header (net_boundary.h): documented online boundary
BOUNDARY_HELPERS = {"rx_net_call": 0, "rx_net_call_ex": 0}
HEXLIT = r"0[xX][0-9a-fA-F]+"


def blank_comments(t):
    """comments and string / char literal contents replaced by spaces (newlines kept)"""
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


def product_lines(lines):
    """the lines with the RX_ORACLE-only regions blanked (#ifdef RX_ORACLE, #if defined(RX_ORACLE): the then-part; #ifndef RX_ORACLE:
    the else-part). Conditional directives are blanked; other preprocessor lines (#define) stay."""
    stack, res = [], []      # entries: [oracle-only now, is an RX_ORACLE conditional]
    for ln in lines:
        s = ln.strip()
        if s.startswith("#"):
            d = re.sub(r"\s+", " ", s[1:].strip())
            if d.startswith("ifdef RX_ORACLE") or re.match(r"if defined\(RX_ORACLE\)\s*$", d) or d == "if RX_ORACLE": stack.append([True, True]); res.append(""); continue
            if d.startswith("ifndef RX_ORACLE") or re.match(r"if !defined\(RX_ORACLE\)\s*$", d): stack.append([False, True]); res.append(""); continue
            if d.startswith(("ifdef", "ifndef", "if ")) or d == "if": stack.append([False, False]); res.append(""); continue
            if d.startswith("else") and stack:
                if stack[-1][1]: stack[-1][0] = not stack[-1][0]
                res.append(""); continue
            if d.startswith("elif") and stack:
                if stack[-1][1]: stack[-1][0] = False
                res.append(""); continue
            if d.startswith("endif") and stack: stack.pop(); res.append(""); continue
        res.append("" if any(e[0] and e[1] for e in stack) else ln)
    return res


def strip_cast(a):
    """`(fn)`, `(rx_addr)(fn)`, `(uint32_t)fn` -> fn"""
    a = a.strip()
    while True:
        m = re.match(r"^\(\s*(?:const\s+)?(?:rx_addr|uint32_t|unsigned|u32|int)\s*\)\s*(\S.*)$", a, re.S)
        if m: a = m.group(1).strip(); continue
        if a.startswith("(") and a.endswith(")") and match_close(a, 0, "(", ")") == len(a) - 1: a = a[1:-1].strip(); continue
        return a


def split_args(s):
    args, depth, cur = [], 0, []
    for ch in s:
        if ch in "([{": depth += 1
        elif ch in ")]}": depth -= 1
        if ch == "," and depth == 0: args.append("".join(cur).strip()); cur = []
        else: cur.append(ch)
    if "".join(cur).strip() or args: args.append("".join(cur).strip())
    return args


def match_close(t, i, o, c):
    d = 0
    for j in range(i, len(t)):
        if t[j] == o: d += 1
        elif t[j] == c:
            d -= 1
            if d == 0: return j
    return -1


RAW, RLINES, CODE = {}, {}, {}
for f in FILES:
    RAW[f] = open(f, encoding="latin-1").read()
    RLINES[f] = RAW[f].split("\n")
    CODE[f] = blank_comments("\n".join(product_lines(RLINES[f])))

# ---- symbolic constants: #define NAME 0x..., enum { NAME = 0x... }, const T NAME = 0x...; ------------------------------------------------
CONST = collections.defaultdict(dict)    # file -> name -> value ; "" -> from headers (global)
for f, t in CODE.items():
    for m in re.finditer(r"#\s*define\s+(\w+)\s+\(?\s*(?:\(\s*\w+\s*\)\s*)?(" + HEXLIT + r")[uUlL]*\s*\)?\s*$", t, re.M):
        CONST[f][m.group(1)] = int(m.group(2), 16)
    for m in re.finditer(r"\b(?:rx_addr|uint32_t|unsigned|int|u32)\s+(\w+)\s*=\s*(" + HEXLIT + r")[uUlL]*\s*[;,]", t):
        CONST[f].setdefault(m.group(1), int(m.group(2), 16))
    for m in re.finditer(r"[{,]\s*(\w+)\s*=\s*(" + HEXLIT + r")[uUlL]*\s*(?=[,}])", t):
        CONST[f].setdefault(m.group(1), int(m.group(2), 16))
for f in FILES:
    if f.endswith(".h"):
        for k, v in CONST[f].items(): CONST[""].setdefault(k, v)


def resolve(f, arg):
    """a call argument -> an address, when it is a literal or a symbol that is one; None otherwise"""
    a = arg.strip()
    while True:
        m = re.match(r"^\(\s*(?:const\s+)?(?:rx_addr|uint32_t|unsigned|u32|int)\s*\)\s*(.*)$", a, re.S)
        if m: a = m.group(1).strip(); continue
        if a.startswith("(") and a.endswith(")") and match_close(a, 0, "(", ")") == len(a) - 1: a = a[1:-1].strip(); continue
        break
    m = re.fullmatch(r"(" + HEXLIT + r")[uUlL]*", a)
    if m: return int(m.group(1), 16)
    if re.fullmatch(r"[A-Za-z_]\w*", a):
        v = CONST[f].get(a, CONST[""].get(a))
        if v is not None: return v
    return None


# ---- wrapper discovery: macros and functions whose body passes one of their parameters as the target of a wrapper ------------------------
DEFS = []     # (file, name, params, body, kind, scope): scope = "" (visible everywhere: a header, or a non-static function) or the file (static / macro in a .c file)
for f, t in CODE.items():
    for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)\(([^)]*)\)((?:[^\n]*\\\n)*[^\n]*)", t, re.M):
        DEFS.append((f, m.group(1), [p.strip() for p in m.group(2).split(",") if p.strip()], m.group(3), "macro", "" if f.endswith(".h") else f))
    flat = re.sub(r"^[ \t]*#(?:[^\n]*\\\n)*[^\n]*", lambda mm: re.sub(r"[^\n]", " ", mm.group(0)), t, flags=re.M)
    depth, i, n = 0, 0, len(flat)
    pat = re.compile(r"([A-Za-z_]\w*)\s*\(")
    while i < n:
        ch = flat[i]
        if ch == "{": depth += 1; i += 1; continue
        if ch == "}": depth -= 1; i += 1; continue
        if depth == 0 and (ch.isalpha() or ch == "_") and (i == 0 or not (flat[i - 1].isalnum() or flat[i - 1] == "_")):
            m = pat.match(flat, i)
            if m and m.group(1) not in ("if", "while", "for", "switch", "return", "sizeof"):
                p0 = m.end() - 1
                p1 = match_close(flat, p0, "(", ")")
                if p1 > 0:
                    k = p1 + 1
                    while k < n and flat[k] in " \t\r\n": k += 1
                    if k < n and flat[k] == "{":
                        b1 = match_close(flat, k, "{", "}")
                        if b1 > 0:
                            params = [(re.findall(r"[A-Za-z_]\w*", p) or [""])[-1] for p in split_args(flat[p0 + 1:p1])]
                            head = flat[max(flat.rfind(";", 0, i), flat.rfind("}", 0, i)) + 1:i]
                            DEFS.append((f, m.group(1), params, flat[k:b1 + 1], "func", f if (re.search(r"\bstatic\b", head) and f.endswith(".c")) else ""))
                            i = b1 + 1
                            continue
            if m: i = m.end(); continue
            while i < n and (flat[i].isalnum() or flat[i] == "_"): i += 1
            continue
        i += 1

PORTED_NAMES = set()      # names of ported functions (RX_FUNCTION(0x..., name)): a port passing a parameter on is not a literal-address wrapper
for f in FILES:
    for m in re.finditer(r"RX_FUNCTION\(\s*0x[0-9a-fA-F]+\s*,\s*(\w+)", CODE[f]): PORTED_NAMES.add(m.group(1))

WR = {(n, ""): (k,) for n, k in BASE.items()}     # (name, scope) -> indices of the target-address arguments
WRAP_SRC = {}         # (name, scope) -> (file, kind)
ENDS_EX = {("rx_call_original", ""): False, ("rx_call_original_ex", ""): True}   # (name, scope) -> the chain ends in rx_call_original_ex (always fatal in the product)
HANDLED = collections.defaultdict(set)      # (name, scope) -> addresses a `switch (target)` of the wrapper chain handles natively (case labels)
GUARDED = {}                                # (name, scope) -> True when the chain tests rx_fn / RX_FN of the target first (a registered target runs the core function)
changed = True
while changed:
    changed = False
    for (f, name, params, body, kind, scope) in DEFS:
        if (name, scope) in WR or name in BOUNDARY_HELPERS or name in PORTED_NAMES: continue
        idxs = []; inner = None; ex_here = False
        for (w, wscope), ks in list(WR.items()):
            if wscope not in ("", f): continue           # a file-local wrapper is seen only inside its own file
            for m in re.finditer(r"\b" + w + r"\s*\(", body):
                p1 = match_close(body, m.end() - 1, "(", ")")
                if p1 < 0: continue
                args = split_args(body[m.end():p1])
                for k in ks:
                    if k < len(args):
                        a = strip_cast(args[k])
                        if a in params and params.index(a) not in idxs:
                            idxs.append(params.index(a)); inner = inner or (w, wscope)
                            ex_here = ENDS_EX.get((w, wscope), False)
        if idxs:
            WR[(name, scope)] = tuple(sorted(idxs)); WRAP_SRC[(name, scope)] = (os.path.basename(f), kind); changed = True
            ENDS_EX[(name, scope)] = ex_here or bool(re.search(r"\brx_call_original_ex\b", body))
            targets = [params[i] for i in idxs]
            h = set()
            for (w, wscope) in WR:                          # every wrapper this one forwards to
                if wscope in ("", f) and re.search(r"\b" + re.escape(w) + r"\s*\(", body): h |= HANDLED.get((w, wscope), set())
            for target in targets:
                if re.search(r"switch\s*\(\s*" + re.escape(target) + r"\s*\)", body):
                    h |= {int(c, 16) for c in re.findall(r"\bcase\s+(" + HEXLIT + r")\s*:", body)}
                # `if (target == 0x...)` / `target != 0x...`: the wrapper tells that address apart itself
                h |= {int(c, 16) for c in re.findall(re.escape(target) + r"\s*[!=]=\s*(" + HEXLIT + r")", body)}
            # an address the wrapper looks up in the registry itself: PL_CORE(0x..., fn) / rx_fn(0x...) / RX_FN(0x..., type)
            h |= {int(c, 16) for c in re.findall(r"(?:rx_fn|RX_FN|\w*CORE)\s*\(\s*(" + HEXLIT + r")", body)}
            HANDLED[(name, scope)] = h
            GUARDED[(name, scope)] = all(bool(re.search(r"\b(?:rx_fn|RX_FN)\s*\(\s*" + re.escape(t) + r"\b", body)) for t in targets) or GUARDED.get(inner, False)

# ---- call sites -----------------------------------------------------------------------------------------------------------------------
# a site: (address, via, file, boundary category or None, guarded)
SITES = []
unresolved = collections.Counter()
allw_global = {n: ks for (n, sc), ks in WR.items() if sc == ""}
allw_global.update({n: (k,) for n, k in BOUNDARY_HELPERS.items()})
def boundary_name_category(name):
    """macros / helpers named *BOUNDARY* are documented boundaries (world_util.h AUDIO_BOUNDARY_*, net_boundary.h rx_net_call*)"""
    if name in BOUNDARY_HELPERS: return "online"
    if "BOUNDARY" in name: return name.split("BOUNDARY")[0].strip("_").lower().replace("_", "-") or "boundary"   # AUDIO_BOUNDARY_CTX -> audio, UI_BOUNDARY_REGS -> ui
    return None
for f in FILES:
    t = CODE[f]; rlines = RLINES[f]; base = os.path.basename(f)
    allw = dict(allw_global)
    for (n, sc), ks in WR.items():
        if sc == f: allw[n] = ks
    wre = re.compile(r"\b(" + "|".join(sorted(map(re.escape, allw), key=len, reverse=True)) + r")\s*\(")
    whole_file_boundary = "ONLINE-SESSION BOUNDARY" in RAW[f][:4000]
    starts = [0] + [m.end() for m in re.finditer("\n", t)]
    def line_of(pos):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= pos: lo = mid
            else: hi = mid - 1
        return lo
    for m in wre.finditer(t):
        name = m.group(1)
        p0 = m.end() - 1
        p1 = match_close(t, p0, "(", ")")
        if p1 < 0: continue
        pre = t[max(0, m.start() - 40):m.start()]
        post = t[p1 + 1:p1 + 40].lstrip()
        if post.startswith("{") or re.search(r"#\s*define\s+$", pre): continue          # the wrapper's own definition
        args = split_args(t[p0 + 1:p1])
        ln = line_of(m.start())
        for k in allw[name]:
            if k >= len(args): continue
            a = resolve(f, args[k])
            if a is None:
                if not re.match(r"^U32\(\s*0\s*,", args[k]): unresolved[name] += 1       # (import slots: not game code)
                continue
            if a not in F: continue                                                          # not a function address
            scope_key = (name, "") if (name, "") in WR else (name, f)
            if a in HANDLED.get(scope_key, ()): continue                                     # a native `case` of the wrapper chain's switch
            near = " ".join(rlines[max(0, ln - 2):ln + 1])
            via = "direct" if name in BASE else f"{name}[{k}]"
            cat = boundary_name_category(name)
            mb = re.search(r"BOUNDARY \(([^)]*)\)", near)
            if cat is None and mb: cat = "online" if mb.group(1).startswith("online") else re.sub(r"\s+", "-", mb.group(1).strip())
            if cat is None and whole_file_boundary: cat = "online"
            # guarded: the wrapper tests rx_fn first, or the same literal is looked up with rx_fn / RX_FN / *_CORE within the 14 lines above
            guarded = GUARDED.get(scope_key, False)
            if not guarded:
                ctx = " ".join(rlines[max(0, ln - 14):ln + 1])
                if re.search(r"(?:rx_fn|RX_FN|\w*CORE)\s*\(\s*0x0*" + ("%x" % a) + r"\b", ctx, re.I): guarded = True
            if name in ("WORLD_CTX", "WORLD_CTX0", "world_call_ctx", "WORLD_CTX_F") and not re.search(r"\.ecx\s*=\s*[^0]", " ".join(args[1:2])): guarded = False
            SITES.append((a, via, base, cat, guarded, ENDS_EX.get(scope_key, False)))

uses = collections.defaultdict(lambda: collections.defaultdict(set))       # addr -> via -> files
boundary = collections.defaultdict(lambda: collections.defaultdict(set))
bcat = collections.defaultdict(set)
guard_all = collections.defaultdict(lambda: True)
for a, via, base, cat, guarded, ex in SITES:
    if cat:
        boundary[a][via].add(base); bcat[a].add(cat)
    else:
        uses[a][via].add(base)
        # a registered target is fine in the product when the call is guarded by a registry test, or is a plain rx_call_original of an
        # integer-signature function (src/rx_mem.c); an rx_call_original_ex form, a non-integer signature or no port is fatal
        fine = guarded or (not ex and a in INTSIG)
        guard_all[a] = guard_all[a] and fine

rows = sorted(uses, key=lambda a: (a in reg, guard_all[a], -F.get(a, ("", 0))[1], a))
o = open(sys.argv[1], "w") if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else sys.stdout
o.write("addr\tname\tsize\tregistered\tfiles\tvia\tclass\n")
def row(a, d, cls):
    n, sz = F.get(a, ("?", 0))
    o.write(f"{a:08x}\t{n}\t{sz}\t{int(a in reg)}\t{','.join(sorted(set().union(*d[a].values())))}\t{','.join(sorted(d[a]))}\t{cls}\n")
for a in rows: row(a, uses, ("ported (callable)" if guard_all[a] else "ported (switch)") if a in reg else "unported")
for a in sorted(boundary, key=lambda a: (-F.get(a, ("", 0))[1], a)):
    if a not in uses: row(a, boundary, "boundary:" + "+".join(sorted(bcat[a])))
miss = [a for a in uses if a not in reg]
switch_rows = [a for a in uses if a in reg and not guard_all[a]]
guarded_rows = [a for a in uses if a in reg and guard_all[a]]
only_boundary = sorted(set(boundary) - set(uses))
macro_miss = [a for a in miss if any(v != "direct" for v in uses[a])]
if "-v" in sys.argv:
    for w in sorted(WRAP_SRC): print(f"wrapper {w[0]}{list(WR[w])}  ({WRAP_SRC[w][0]}, {WRAP_SRC[w][1]}{', file-local' if w[1] else ''})", file=sys.stderr)
print(f"{len(WRAP_SRC)} macros / helpers lead to rx_call_original(_ex) with a parameter as the target (-v lists them); "
      f"calls with a computed target (vtable reads, variables) are not literal and not counted", file=sys.stderr)
bycat = collections.Counter(); bbytes = collections.Counter()
for a in only_boundary:
    c = "+".join(sorted(bcat[a])); bycat[c] += 1; bbytes[c] += F.get(a, ("", 0))[1]
print("documented boundary calls: %d functions (%d called only there; %s)" % (len(boundary), len(only_boundary),
      ", ".join("%s %d fn / %s bytes" % (c, bycat[c], format(bbytes[c], ",")) for c in sorted(bycat)) or "none"), file=sys.stderr)
print(f"{len(uses)} original functions still called outside boundaries: {len(miss)} UNPORTED ({sum(F.get(a, ('', 0))[1] for a in miss):,} bytes; "
      f"{len(macro_miss)} of them reached through a macro / helper), {len(switch_rows)} ported but the call is fatal in the product "
      f"(switch it to the port), {len(guarded_rows)} ported and callable in the product (fine)", file=sys.stderr)
