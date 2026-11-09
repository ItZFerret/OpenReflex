"""sigcheck_vt.py -- the typed-call-through-a-vtable-slot check of sigcheck.py (imported by it, run at the end of every report).

Why. The product calls ported functions through the original's vtables with hand-written C types:
    RX_FN(U32(U32(obj, 0), 0xc0), float (*)(rx_addr))
sigcheck.py checks the positional (integer) call paths; a typed call is not seen there. Found by hand first: the vehicle's vf48 (slot 0xc0)
was called as float (*)(rx_addr) while the four-wheel ports (00486640, 004ac640, 00473570, 004c6fa0) return a struct rx_f80, so every
four-wheel race wrote through a stale struct-return pointer and crashed (fixed in 9d6e44f, engine_startline.c).

What it looks at: every place in core/game and core/api (product side: `#ifdef RX_ORACLE` bodies dropped) that turns an original address into a
typed C function pointer:
    RX_FN(expr, <fn-pointer type or a typedef name>)
    ((<fn-pointer type>)rx_fn(expr))   and   void* f = rx_fn(expr); ... ((<fn-pointer type>)f)(...)      (one cast type only)
    rx_vcall(obj, off, n, args) / rx_tcall(target, obj, n, args)     (the integer path: integer result, n integer arguments)

1. The address expression is expanded only as far as the slot form needs: a local variable is replaced by its assignments before the call, a
   one-line helper (race_vfn, loop_vslot ...) is inlined, a ternary offset gives both slots. An expression that depends on the enclosing
   function's own parameters (loop_tc2_x87(fn, ...), a typed wrapper) is a TEMPLATE: every call of that wrapper (the same file for a static
   one) is expanded with its arguments (up to 4 levels); whether the result is used follows the wrapper's callers. The terminal forms are
   U32(U32(<receiver>, 0), <offset>) -> a vtable slot, U32(<literal vtable>, <offset>) -> that one vtable's slot, a literal address -> that one
   port. Anything else (a table field, a loop variable) is counted as unresolved, not checked (--vt-unresolved lists them).
2. The ports at that slot are the targets of vtables.tsv that are registered (RX_FUNCTION). Only the PRIMARY vtable of a class (its lowest
   address: the vptr at object +0) counts, unless the receiver is an offset into an object (`e + 0x90`: a secondary base). A target that sits at
   3 or more different slots (the empty / `ret N` stubs shared by unrelated classes) says nothing about the slot and is dropped.
3. The candidates are NARROWED to the classes that can reach the call:
   - "own class": the receiver is the `this` (first parameter) of an enclosing function that is itself a vtable target: only the classes whose
     vtable holds that function;
   - "receiver": the words of the receiver expression (`e->vehicle`, `veh`, `beh`, `cam` with a few abbreviations) against the CamelCase words
     of the class names (whole-word match, called functions' names ignored).
   A call whose receiver says nothing (`obj`, `self`, `p`) is UNNARROWED and is skipped (counted; --vt-unnarrowed shows what a check over every
   class at the slot would say: mostly noise, other hierarchies share the slot numbers).
4. The narrowed classes are grouped into hierarchies (classes that share a target at 3 or more slots). If the call's C type fits at least one port of a
   hierarchy, every port of that hierarchy at the slot must fit too (they are overrides of one virtual function). A call that fits none of the narrowed
   ports is a guess by name and only counted ("weak", --vt-weak lists them). A literal address is always checked against its port.
   Compared: return kind (void / integer-pointer / 64-bit / float / double / rx_f80 / other struct) and the arguments (the port takes more
   parameters than the call passes, or the float / double / struct / integer kind differs at a position both have). A void call of a port that
   returns rx_f80 or a struct is a mismatch (hidden result pointer); a call that USES a value from a void port is one (use is read off the
   statement). A parameter the port's body never reads is not a mismatch.
5. A call site that already dispatches on the target (`target == 0x00466a10` in the same function, the engine_startline.c fix) passes when it
   fits all of the compared ports with that address, or all the others.
Findings: vtslot lines. Allow-list (sigcheck_allow.tsv, TAB reason): `vt:<file>:<function>:<slot offset>` for one call site (the function name
keeps it stable across edits), `vtport:<port address>:<slot offset>` for a port that is not a sibling of the receiver (suppresses that port at
that slot everywhere). Stale entries are reported.
"""
import os, re, glob, csv, collections, functools, itertools

INTS = {"rx_addr", "uint32_t", "int32_t", "uint16_t", "int16_t", "uint8_t", "int8_t", "int", "unsigned", "unsigned int", "bool", "_Bool", "size_t",
        "unsigned char", "unsigned short", "char", "short", "long", "unsigned long", "ptrdiff_t", "uintptr_t", "intptr_t", "BOOL", "DWORD", "int16", "uint16"}
I64 = {"uint64_t", "int64_t", "long long", "unsigned long long"}
STOP = {"this", "self", "obj", "object", "ptr", "addr", "target", "slot", "func", "node", "item", "data", "result", "value", "base", "list",
        "entry", "elem", "state", "info", "args", "arg", "out", "src", "dst", "tmp", "temp", "next", "prev", "head", "tail", "first", "last"}


# ---- text helpers -------------------------------------------------------------------------------------------------------------------------
def blank_comments(txt):
    """comments -> spaces (newlines kept); string / char literal contents -> spaces"""
    out, i, n = [], 0, len(txt)
    while i < n:
        c = txt[i]
        if c == "/" and i + 1 < n and txt[i + 1] == "*":
            j = txt.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", txt[i:j]))
            i = j
        elif c == "/" and i + 1 < n and txt[i + 1] == "/":
            j = txt.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and txt[j] != c and txt[j] != "\n":
                j += 2 if txt[j] == "\\" else 1
            out.append(c + " " * max(0, j - i - 1) + (c if j < n and txt[j] == c else ""))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)[:n]


def match_close(s, i):
    """index after the bracket that closes s[i] (one of ( [ {)"""
    pairs = {"(": ")", "[": "]", "{": "}"}
    o = s[i]
    depth = 0
    for k in range(i, len(s)):
        if s[k] in "([{":
            depth += 1
        elif s[k] in ")]}":
            depth -= 1
            if depth == 0:
                return k + 1
    return len(s)


def split_top(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip() or out:
        out.append(cur.strip())
    return out


def norm_t(t):
    t = re.sub(r"\b(const|volatile|register|static|inline|extern|struct|enum|union|__restrict)\b", " ", t)
    t = re.sub(r"\s*\*\s*", "*", t)
    return " ".join(t.split())


def kind_of(t, structs=()):
    t = norm_t(t)
    if t == "" or t == "void":
        return "V"
    if "*" in t or "[" in t:
        return "I"
    if t in INTS:
        return "I"
    if t in I64:
        return "L"
    if t == "float":
        return "F"
    if t == "double":
        return "D"
    if t in ("rx_f80", "long double"):
        return "X"
    return "S:" + t


def kind_name(k):
    return {"V": "void", "I": "integer", "L": "64-bit integer", "F": "float", "D": "double", "X": "rx_f80"}.get(k, k.replace("S:", "struct "))


def param_kind(p):
    """kind of one parameter declaration text (with or without a name)"""
    p = " ".join(p.split())
    if p in ("", "void"):
        return None
    p = re.sub(r"\[[^\]]*\]$", "*", p)
    if "(*" in p:
        return "I"
    if kind_of(p) != "S:" + norm_t(p) or " " not in norm_t(p).replace("*", "* "):
        # a bare type name (float, rx_addr, uint32_t, rx_v4 ...)
        if not (kind_of(p).startswith("S:") and re.search(r"[\s\*]\w+$", p)):
            return kind_of(p)
    m = re.match(r"^(.*?[\s\*])(\w+)$", p)
    return kind_of(m.group(1)) if m else kind_of(p)


def parse_fnptr(t):
    """'ret (*)(a, b)' -> (ret kind, [arg kinds]) or None"""
    m = re.match(r"^\s*(.*?)\(\s*\*\s*\w*\s*\)\s*\((.*)\)\s*$", t, re.S)
    if not m:
        return None
    args = [param_kind(a) for a in split_top(m.group(2))]
    return kind_of(m.group(1)), [a for a in args if a is not None]


def port_proto(ret, params):
    ps = [param_kind(a) for a in split_top(params)] if params not in ("", "void") else []
    return kind_of(ret), [p for p in ps if p is not None]


def eval_const(s):
    s = re.sub(r"(?<=[0-9a-fA-F])[uUlL]+\b", "", s.strip())
    if not re.fullmatch(r"[0-9a-fA-Fx+\-*/()<>| &]+", s) or not re.search(r"\d", s):
        return None
    try:
        return int(eval(s.replace("/", "//"), {"__builtins__": {}}, {}))
    except Exception:
        return None


@functools.lru_cache(maxsize=None)
def words(name):
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", name)
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", s)
    return [w.lower() for w in re.split(r"[^A-Za-z0-9]+|\s+", s) if w]


@functools.lru_cache(maxsize=None)
def ident_tokens_c(text):
    return tuple(ident_tokens(text))


def ident_tokens(text):
    toks = []
    for idn in re.findall(r"[A-Za-z_]\w*(?!\w|\s*\()", text):
        for w in words(idn):
            if len(w) >= 3 and w not in STOP and not re.fullmatch(r"u32|rx|int|void|const|0x\w+", w):
                toks.append(w)
    return toks


# ---- the sources ----------------------------------------------------------------------------------------------------------------------------
class Func:
    __slots__ = ("name", "params", "pnames", "file", "text", "boff", "line0", "comment", "start", "end", "kind")


KEYWORDS = {"if", "while", "for", "switch", "return", "sizeof", "else", "do", "case", "defined"}


def load_sources(R, product_side):
    files = []
    for d in ("core/game", "core/api"):
        for ext in ("c", "h", "inc"):
            files += sorted(glob.glob(os.path.join(R, d, "*." + ext)))
    srcs = {}      # path -> (stripped text, raw text)
    for p in files:
        raw = open(p, encoding="utf-8", errors="replace").read()
        srcs[p] = (product_side(blank_comments(raw)), raw)
    return srcs


FUNC_RE = re.compile(r"(?m)^(?!\s|#)[^;{}()\n]*?\b(\w+)\s*\(((?:[^;{}\n]|\n[ \t])*)\)\s*(?:__attribute__\(\([^)]*\)\)\s*)?\{")


def collect_functions(srcs):
    funcs = []
    for p, (txt, raw) in srcs.items():
        base = os.path.basename(p)
        for m in FUNC_RE.finditer(txt):
            name = m.group(1)
            if name in KEYWORDS or re.search(r"\n[^\s]", m.group(2)):
                continue          # a keyword, or a registration line (RX_FUNCTION(..)) that ran into the next definition
            b0 = m.end() - 1
            b1 = match_close(txt, b0)
            f = Func()
            f.name, f.file, f.start, f.end, f.boff = name, base, m.start(), b1, b0
            f.text = txt
            f.line0 = txt.count("\n", 0, m.start()) + 1
            f.params = m.group(2)
            f.pnames = []
            for prm in split_top(m.group(2)):
                if prm in ("", "void"):
                    continue
                ids = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", prm))
                f.pnames.append(ids[-1] if len(ids) >= 2 or (ids and "*" in prm) else "")
            # the comment block right above the function (raw text) plus the comments inside it
            rawlines = raw.split("\n")
            lo = max(0, f.line0 - 1 - 6)
            f.comment = "\n".join(rawlines[lo: txt.count("\n", 0, b1) + 1])
            f.kind = None
            funcs.append(f)
    return funcs


def line_of(f, off):
    return f.text.count("\n", 0, off) + 1


TYPEDEF_RE = re.compile(r"typedef\s+([^;(){}]*?)\(\s*\*\s*(\w+)\s*\)\s*\(([^;{}]*)\)\s*;")


def collect_typedefs(srcs):
    td = collections.defaultdict(list)      # name -> [(path, start, kind tuple)]
    for p, (txt, raw) in srcs.items():
        for m in TYPEDEF_RE.finditer(txt):
            args = [param_kind(a) for a in split_top(m.group(3))]
            td[m.group(2)].append((p, m.start(), (kind_of(m.group(1)), [a for a in args if a is not None])))
    return td


# ---- expression expansion -------------------------------------------------------------------------------------------------------------------
class Ctx:
    def __init__(self, funcs, srcs):
        self.funcs = funcs
        self.helpers = {}      # name -> (pnames, expr) for one-line `return expr;` functions
        for f in funcs:
            body = f.text[f.boff + 1: f.end - 1].strip()
            m = re.fullmatch(r"return\s+([^;]*);", body, re.S)
            if m and f.name not in self.helpers:
                self.helpers[f.name] = (f.pnames, m.group(1).strip())

    def defs_of(self, f, name, pos):
        """expressions assigned to local `name` in f before offset pos (declarations with an initialiser count)"""
        out = []
        body = f.text[f.boff:pos]
        for m in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"\s*=(?!=)\s*([^;]*?);", body):
            if re.search(r"\b(rx_addr|uint32_t|int32_t|uint16_t|uint8_t|float|double|int|unsigned|char|void)\s*\**\s*(?:const\s*)?$", body[max(0, m.start() - 24): m.start()]):
                # a declaration: it is out of scope once the block that holds it has closed before pos
                depth = low = 0
                for ch in body[m.end():]:
                    if ch == "{":
                        depth += 1
                    elif ch == "}":
                        depth -= 1
                        low = min(low, depth)
                if low < 0:
                    continue
            parts = split_top(m.group(1))
            rhs = parts[0].strip() if parts else ""
            if rhs:
                out.append(rhs)
        return out


def strip_parens(s):
    s = s.strip()
    while True:
        if s.startswith("(") and match_close(s, 0) == len(s):
            s = s[1:-1].strip()
            continue
        m = re.match(r"^\(\s*(?:const\s+)?(?:rx_addr|uint32_t|int32_t|unsigned|int|void\s*\*)\s*\)\s*(.*)$", s, re.S)
        if m:
            s = m.group(1).strip()
            continue
        return s


def split_ternary(s):
    depth = 0
    q = c = -1
    for i, ch in enumerate(s):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif depth == 0 and ch == "?" and q < 0:
            q = i
        elif depth == 0 and ch == ":" and q >= 0 and c < 0:
            c = i
    if q >= 0 and c > q:
        return [s[q + 1:c].strip(), s[c + 1:].strip()]
    return [s]


def subst_idents(expr, mapping):
    """replace whole identifiers (not after . or ->, not calls) by their mapped text"""
    return re.sub(r"(?<![\w.>])([A-Za-z_]\w*)\b(?!\s*\()", lambda m: mapping.get(m.group(1), m.group(1)), expr)


def parse_call(s):
    m = re.match(r"^(\w+)\s*\(", s)
    if not m:
        return None
    e = match_close(s, m.end() - 1)
    if e != len(s):
        return None
    return m.group(1), split_top(s[m.end(): e - 1])


IDENT = re.compile(r"^[A-Za-z_]\w*$")


def lit_addr(e):
    v = eval_const(e)
    if v is not None and 0x400000 <= v < 0x1000000 and re.match(r"^\(?\s*0x", e):
        return v
    return None


def lazy(ctx, f, expr, pos, depth=0, seen=frozenset()):
    """the address expression `expr` evaluated in f at pos, expanded only as far as the slot form U32(U32(receiver, 0), offset) needs.
    -> list of ('slot', byte offset, receiver text) | ('vt', vtable, byte offset) | ('lit', address) | ('tmpl', text over f's parameters) | ('fail', why)"""
    e = strip_parens(expr)
    if depth > 6 or len(e) > 1200:
        return [("fail", "too deep")]
    alts = split_ternary(e)
    if len(alts) > 1:
        out = []
        for a in alts:
            out += lazy(ctx, f, a, pos, depth, seen)
        return out
    if e in ("NULL", "0"):
        return [("fail", "null")]
    v = lit_addr(e)
    if v is not None:
        return [("lit", v)]
    pc = parse_call(e)
    if pc and pc[0] == "U32" and len(pc[1]) == 2:
        kalts = split_ternary(strip_parens(pc[1][1]))
        if len(kalts) > 1:
            out = []
            for ka in kalts:
                out += lazy(ctx, f, "U32(%s, %s)" % (pc[1][0], ka), pos, depth + 1, seen)
            return out
        k = const_of(ctx, f, pc[1][1], pos)
        vts = vt_of(ctx, f, pc[1][0], pos, depth, seen)
        out = []
        for vt in vts:
            if vt[0] == "fail":
                out.append(vt)
            elif k is None:
                kt = pc[1][1]
                if vt[0] == "recv" and any(re.search(r"(?<![\w.>])" + re.escape(pn) + r"\b", kt + " " + vt[1]) for pn in f.pnames if pn):
                    out.append(("tmpl", "U32(U32(%s, 0), %s)" % (vt[1], kt)))
                else:
                    out.append(("fail", "offset not constant: " + " ".join(kt.split())[:40]))
            elif vt[0] == "recv":
                out.append(("slot", k, vt[1]))
            else:
                out.append(("vt", vt[1], k))
        return out
    if pc and pc[0] in ctx.helpers and len(ctx.helpers[pc[0]][0]) == len(pc[1]):
        pn, body = ctx.helpers[pc[0]]
        return lazy(ctx, f, subst_idents(body, {k_: "(" + a + ")" for k_, a in zip(pn, pc[1])}), pos, depth + 1, seen)
    if IDENT.match(e):
        if e in f.pnames:
            return [("tmpl", e)]
        if e in seen:
            return [("fail", "self-referencing " + e)]
        ds = ctx.defs_of(f, e, pos)
        if not ds:
            return [("fail", "no assignment of " + e)]
        out = []
        for d in ds[-2:]:
            out += lazy(ctx, f, d, pos, depth + 1, seen | {e})
        return out
    return [("fail", "unparsed: " + " ".join(e.split())[:50])]


def const_of(ctx, f, k, pos):
    v = eval_const(strip_parens(k))
    if v is not None:
        return v
    k = strip_parens(k)
    if IDENT.match(k) and k not in f.pnames:
        ds = ctx.defs_of(f, k, pos)
        vals = {eval_const(strip_parens(d)) for d in ds[-2:]}
        if len(vals) == 1 and None not in vals:
            return vals.pop()
    # 4u * slot, helper-inlined offsets
    pc = parse_call(k)
    if pc and pc[0] in ctx.helpers and len(ctx.helpers[pc[0]][0]) == len(pc[1]):
        pn, body = ctx.helpers[pc[0]]
        return const_of(ctx, f, subst_idents(body, {k_: "(" + a + ")" for k_, a in zip(pn, pc[1])}), pos)
    m = re.fullmatch(r"(.*?)\s*\*\s*([A-Za-z_]\w*)\s*", k)
    if m and m.group(2) not in f.pnames:
        a, b = eval_const(m.group(1)), const_of(ctx, f, m.group(2), pos)
        if a is not None and b is not None:
            return a * b
    return None


def vt_of(ctx, f, a, pos, depth, seen):
    """what the object word `a` (the first argument of U32(a, off)) is: [('recv', X)] for U32(X, 0) | [('lit', vtable)]"""
    a = strip_parens(a)
    pc = parse_call(a)
    if pc and pc[0] == "U32" and len(pc[1]) == 2 and eval_const(pc[1][1]) == 0:
        return [("recv", pc[1][0])]
    v = lit_addr(a)
    if v is not None:
        return [("lit", v)]
    if pc and pc[0] in ctx.helpers and len(ctx.helpers[pc[0]][0]) == len(pc[1]):
        pn, body = ctx.helpers[pc[0]]
        return vt_of(ctx, f, subst_idents(body, {k_: "(" + x + ")" for k_, x in zip(pn, pc[1])}), pos, depth + 1, seen)
    if IDENT.match(a) and a not in seen and depth < 6:
        ds = ctx.defs_of(f, a, pos)
        out = []
        for d in ds[-2:]:
            out += vt_of(ctx, f, d, pos, depth + 1, seen | {a})
        if out:
            return out
    return [("fail", "vtable word not U32(x, 0): " + " ".join(a.split())[:40])]


class Site:
    __slots__ = ("file", "func", "line", "term", "ctype", "used", "chain", "ef", "recv", "dispatch", "kind", "text", "orig", "slotoff", "conv")


def stmt_value_used(txt, call_start):
    """is the value of the call that starts at call_start used? (what precedes it)"""
    k = call_start - 1
    while k >= 0 and txt[k] in " \t\n":
        k -= 1
    prev = txt[k] if k >= 0 else ";"
    if prev in ";{})":
        return False                  # a statement, `if (f) f(x);`, `for (...) f(x)`
    if prev == ",":
        # macro-wrapped statement `NAME(n, f(x))`
        return not re.search(r"\b[A-Z][A-Z0-9_]+\(\s*[^,()]*,\s*$", txt[max(0, k - 70): k + 1])
    if re.search(r"\b(else|do)$", txt[max(0, k - 6): k + 1]):
        return False
    return True


def bound_var(txt, off):
    """the variable a typed pointer is bound to by `T (*v)(..) = RX_FN(` / `v = RX_FN(` just before off"""
    pre = txt[max(0, off - 400): off]
    m = re.search(r"\(\s*\*\s*(\w+)\s*\)\s*\([^;]*?\)\s*=\s*(?:[^;=]*\?[^;:]*:\s*)?$", pre) or \
        re.search(r"(?<![\w.>])(\w+)\s*=\s*(?:[^;=]*\?[^;:]*:\s*)?$", pre)
    return m.group(1) if m else None


def value_used_for(f, off, end):
    var = bound_var(f.text, off)
    if var:
        calls = [m.start() for m in re.finditer(r"(?<![\w.>])" + re.escape(var) + r"\s*\(", f.text[end: f.end])]
        if calls:
            return any(stmt_value_used(f.text, end + c) for c in calls)
        return False
    return stmt_value_used(f.text, off)


PURECALL = 0x008f9fd6      # the abstract-slot thunk (jmp [_purecall]): a pure virtual is never called on a constructed object


def load_reach_allow(R):
    """core/tools/vtreach_allow.tsv: unregistered vtable targets that are proven unreachable, TAB reason. Keys: `<target hex>` (that function),
    `class:<regex>` (every target of the classes whose name matches, re.search), `site:<file>:<function>:<slot>` (one call site's reach)."""
    path = os.path.join(R, "core/tools/vtreach_allow.tsv")
    out = collections.OrderedDict()
    if os.path.exists(path):
        for ln in open(path, encoding="utf-8"):
            if not ln.strip() or ln.startswith("#"):
                continue
            k, _, why = ln.rstrip("\n").partition("\t")
            out[k.strip()] = why.strip()
    return out


def reach_pass(R, srcs, funcs, sites, reg, vtable_rows, by_vt, primary, fam_of, reg_by_name, vt_classes, roles, tok_ok):
    """The unregistered targets a vtable-slot call of the product can reach (w-vtunported). A slot call whose target has no port falls back to
    the original (loop_tc1 / rx_vcall / `if (!f) rx_call_original(...)`): fatal in the product ("unported game function", the 0055fd50 save-spot
    stop). For every resolved site of run() (slot calls, literal vtables, literal addresses):
      1. CONSTRUCTED classes: a vtable whose address the product's code names (core/game; core/api only exports), except type tests (`== VT`,
         `case VT:`, arrays only compared against), destructors' stores and a base constructor whose every caller goes on to store a derived
         class's vtable (AnimationEntity under RiderAnimation): the original's code does not run, so no other object can exist.
      2. the receiver's classes, first match wins: an embedded member (`owner + off` where a ported constructor stored that vtable at +off), a
         secondary base at that offset; the function's own class (`this` of a vtable target); a role of sigcheck_vt_roles.tsv (`only`, a regex or
         `<Base` for Base and its primary-chain subclasses); the receiver's words; else GENERIC: the classes whose INTRODUCER at slot k (the
         root-most class of their chain with more than k slots: the class that declares that virtual) is one the RX_FN_TRACE runs saw this
         function's slot-k calls reach (core/tools/vtreach_observed.tsv), or, never observed, every introducer with a port at slot k.
         A narrowed site the runs reached is cut to the observed introducers too. Primary vtables, except `owner + off` receivers.
      3. a target is reported when it is not registered, not the _purecall thunk and not allow-listed (vtreach_allow.tsv: proven unreachable by
         target, class or site, each with its reason; `open:` = reachable or not disproven and not ported: the work list, listed apart).
    `abort`: the site's fallback is fatal (rx_vcall / rx_tcall, or a fatal call within 600 characters after the typed conversion); else silent.
    RX_VTREACH_PRETEND_UNPORTED=<addr,...> drops ports for a sanity check (0055fd50 must report the save-spot site)."""
    allow = load_reach_allow(R)
    pretend = {int(x, 16) for x in os.environ.get("RX_VTREACH_PRETEND_UNPORTED", "").replace(",", " ").split()}
    if pretend:      # sanity check: treat these ports as missing (RX_VTREACH_PRETEND_UNPORTED=0055fd50 must report the save-spot call)
        reg = {a: v for a, v in reg.items() if a not in pretend}
    allow_cls = [(re.compile(k[6:]), k) for k in allow if k.startswith("class:")]
    used_allow = set()
    lits = collections.defaultdict(set)              # vtable literal -> the functions naming it (None: file scope, a table)
    fpos = collections.defaultdict(list)
    for f in funcs:
        fpos[f.file].append(f)
    for p, (txt, raw) in srcs.items():
        if os.sep + "api" + os.sep in p:
            continue              # core/api exports and type-tests game objects (rx_session.c's ambient / prop tables); it constructs none
        tests = []                # the initialisers of arrays that are only compared against (`vt == k_character_renderable_vt[k]`): type tests
        for m in re.finditer(r"\b(\w+)\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)?=\s*\{", txt):
            nm = m.group(1)
            if re.search(r"(==|!=)\s*" + re.escape(nm) + r"\s*\[|\b" + re.escape(nm) + r"\s*\[[^\]]*\](?:\s*\[[^\]]*\])?(?:\.\w+)?\s*(==|!=)", txt):
                tests.append((m.end() - 1, match_close(txt, m.end() - 1)))
        for m in re.finditer(r"\b0[xX]([0-9a-fA-F]{6,8})\b", txt):
            if any(a <= m.start() < b for a, b in tests):
                continue
            pre = txt[max(0, m.start() - 12): m.start()]
            post = txt[m.end(): m.end() + 12]
            if re.search(r"(==|!=)\s*(\(\s*u?int32_t\s*\)|\(rx_addr\))?\s*$", pre) or re.match(r"\s*u?\s*(==|!=)", post) or re.search(r"\bcase\s*$", pre):
                continue          # a type test (`U32(e, 0) == 0x0095fc24`, `case 0x...:`) names a class without making one
            fn_ = next((x.name for x in fpos[os.path.basename(p)] if x.start <= m.start() < x.end), None)
            lits[int(m.group(1), 16)].add(fn_)
    vt_cls, vname = {}, {}
    for r in vtable_rows:
        vt_cls[int(r["vtable"], 16)] = r["class"]
        vname[int(r["target"], 16)] = r["name"]
    cons0 = {v for v in by_vt if v in lits}
    # classes.tsv: each vtable's offset in its object (col_offset: 0 = the primary vtable, 0x90 = the IRenderable base at +0x90 ...)
    col_off, nvf, bases_of = {}, {}, {}
    cpath = os.path.join(R, "_re/out/classes.tsv")
    if os.path.exists(cpath):
        for r in csv.DictReader(open(cpath, encoding="latin-1"), delimiter="\t"):
            try:
                v_ = int(r["vtable"], 16)
                col_off[v_] = int(r["col_offset"] or "0", 16)
            except ValueError:
                continue
            if col_off[v_] == 0:
                nvf[r["class"]] = int(r["nvfuncs"] or 0)
                bases_of[r["class"]] = [b for b in (r["bases"] or "").split(",") if b]

    def introducer(v, k):
        """the class that declares virtual k of vtable v: the root-most class of its base chain (that sub-object's chain for a secondary
        vtable) that already has more than k slots. Two classes with one introducer at k share that virtual function."""
        c = vt_cls[v]
        o = col_off.get(v, 0)
        if o:
            chain = [b.split("@")[0] for b in bases_of.get(c, []) if b.endswith("@%x" % o)]
        else:
            chain = [c] + [b for b in bases_of.get(c, []) if "@" not in b]
        intro = c if not o else (chain[-1] if chain else c)
        for b in chain:
            if nvf.get(b, 0) > k:
                intro = b
        return intro
    # embedded members: a literal vtable stored at `X + off` (SU32(ent, 0, VT) with `ent = A + 0x1143e0`, or SU32(A, 0x1143e0, VT)) says that a
    # member object of that class lives at +off of some owner: a receiver `owner + off` is one of those classes
    funcs_by_file = collections.defaultdict(list)
    for f in funcs:
        funcs_by_file[f.file].append(f)

    def local_defs(f, name, upto):
        body = f.text[f.boff: upto]
        return [m.group(1).strip() for m in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"\s*=(?!=)\s*([^;]+);", body)]

    def member_off(f, expr, upto, depth=0):
        """the literal member offset of an address expression `X + 0x...` (locals expanded), else None"""
        e = strip_parens(" ".join(expr.split()))
        m = re.fullmatch(r"(.+?)\s*\+\s*(0x[0-9a-fA-F]+|\d+)", e)
        if m and "(" not in m.group(2):
            return int(m.group(2), 0)
        if depth < 3 and re.fullmatch(r"[A-Za-z_]\w*", e) and f is not None:
            ds = {member_off(f, d, upto, depth + 1) for d in local_defs(f, e, upto)}
            if len(ds) == 1:
                return next(iter(ds))
        return None
    # A base class's vtable is stored by its constructor and then overwritten by the derived constructor that called it (AnimationEntity
    # 0082d040 under RiderAnimation, AnimEntityProximity 006b8d90 under LandAnimalProximity), and a destructor stores the base vtables on its
    # way down: neither makes an object of that class. A vtable counts as CONSTRUCTED when one function naming it is not a destructor and
    # either has no caller in the product (a factory / table entry) or has a caller that does not go on to store a derived class's vtable.
    callers = collections.defaultdict(set)
    for f in funcs:
        for c_ in set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", f.text[f.boff: f.end])):
            callers[c_].add(f.name)
    names_in = collections.defaultdict(set)          # function -> vtable literals it names
    for v, fs in lits.items():
        for fn_ in fs:
            names_in[fn_].add(v)

    def derived_of(v):
        c = vt_cls[v]
        return {w for w in cons0 if c in [b.split("@")[0] for b in bases_of.get(vt_cls[w], [])]}
    DTOR = re.compile(r"(sdtor|dtor|destroy|destruct|release|teardown|_free\b)", re.I)
    cons = set()
    for v in cons0:
        der = derived_of(v)
        for fn_ in lits[v]:
            if fn_ is None:
                cons.add(v)
                break
            if DTOR.search(fn_):
                continue
            cs = callers.get(fn_, set())
            if not cs or any(not (names_in.get(c_, set()) & der) for c_ in cs):
                cons.add(v)
                break
    intro_reg = collections.defaultdict(set)        # slot index -> introducers whose virtual at that slot has a port in a constructed class
    for v in cons:
        for k, t in by_vt[v].items():
            if t in reg and t != PURECALL:
                intro_reg[k].add(introducer(v, k))
    at_slot = collections.defaultdict(list)          # (target, slot index) -> vtables holding it there
    for v, d in by_vt.items():
        for k, t in d.items():
            at_slot[(t, k)].append(v)
    observed = collections.defaultdict(set)          # (file, function) -> targets reached below it in RX_FN_TRACE runs (vtreach_observed.tsv)
    opath = os.environ.get("RX_VTREACH_OBSERVED") or os.path.join(R, "core/tools/vtreach_observed.tsv")
    if os.path.exists(opath):
        for ln in open(opath, encoding="utf-8"):
            if ln.startswith("#") or not ln.strip():
                continue
            f_ = ln.rstrip("\n").split("\t")
            if len(f_) >= 3:
                observed[(f_[0], f_[1])] |= {int(x, 16) for x in f_[2].split(",") if x}
    members = collections.defaultdict(set)          # member offset -> vtables stored there
    for p, (txt, raw) in srcs.items():
        base = os.path.basename(p)
        for m in re.finditer(r"\bSU32\(\s*([^,;]{1,60}?)\s*,\s*(0x[0-9a-fA-F]+|0)\s*,\s*0x([0-9a-fA-F]{6,8})\s*\)", txt):
            v = int(m.group(3), 16)
            if v not in by_vt:
                continue
            f = next((x for x in funcs_by_file[base] if x.start <= m.start() < x.end), None)
            o = int(m.group(2), 0) - col_off.get(v, 0)
            mo = member_off(f, m.group(1), m.start())
            if mo is not None:
                o += mo
            elif re.fullmatch(r"[A-Za-z_]\w*", m.group(1).strip()) and o == 0:
                continue                             # SU32(this, 0, VT): the object itself, not a member at a known offset
            if o > 0:
                members[o].add(v)
    # functions / macros of the product whose body reaches rx_call_original(_ex / _regs): the fatal fallback
    abort = {"rx_call_original", "rx_call_original_ex", "rx_call_original_regs", "crt_unported"}
    bodies = {}
    for f in funcs:
        bodies.setdefault(f.name, "")
        bodies[f.name] += f.text[f.boff: f.end]
    for p, (txt, raw) in srcs.items():
        for m in re.finditer(r"(?m)^[ \t]*#[ \t]*define[ \t]+(\w+)((?:[^\n]*\\\n)*[^\n]*)", txt):
            bodies.setdefault(m.group(1), "")
            bodies[m.group(1)] += m.group(2)
    calls_in = {n: set(re.findall(r"\b([A-Za-z_]\w*)\s*\(", b)) for n, b in bodies.items()}
    changed = True
    while changed:
        changed = False
        for n, cs in calls_in.items():
            if n not in abort and cs & abort:
                abort.add(n)
                changed = True
    func_at_conv = {}
    for f in funcs:
        func_at_conv.setdefault(f.file, []).append(f)

    abort_call = re.compile(r"\b(%s|abort)\s*\(" % "|".join(sorted(re.escape(a) for a in abort)))
    line_starts = {}

    def conv_aborts(s):
        """does the conversion's null-target path end in the product's fatal fallback? rx_vcall / rx_tcall do (rx_call_original); a typed
        conversion does when a fatal call (rx_call_original*, crt_unported, a helper or macro that reaches one, abort) follows it within the
        next 600 characters of its function (the `if (!f) return rx_call_original(...)` idiom); `if (f) f(...)` alone is silent"""
        if s.kind in ("vcall", "tcall"):
            return True
        cf, cl = s.conv
        for f in func_at_conv.get(cf, []):
            if f.line0 <= cl <= f.text.count("\n", 0, f.end) + 1:
                if f.file not in line_starts:
                    line_starts[f.file] = [0] + [m.end() for m in re.finditer(r"\n", f.text)]
                ls = line_starts[f.file]
                off = ls[cl - 1] if cl - 1 < len(ls) else f.boff
                return abort_call.search(f.text[off: min(f.end, off + 600)]) is not None
        return True

    def allowed(t, cls, s, slot_txt):
        for k in ("%08x" % t, "site:%s:%s:%s" % (s.file, s.func, slot_txt), "open:%08x" % t):
            if k in allow:
                used_allow.add(k)
                if k.startswith("open:"):
                    open_hits[t].add(cls)
                    return "OPEN " + allow[k]
                return allow[k]
        for rx, k in allow_cls:
            if rx.search(cls):
                used_allow.add(k)
                return allow[k]
        return None

    found = collections.OrderedDict()        # target -> dict(classes, sites, how, abort)
    allowed_hits = collections.defaultdict(set)
    open_hits = collections.defaultdict(set)       # `open:<target>` entries: reachable or not disproven, not ported yet (tracked in core/plan/QUEUE.md)
    bysite = collections.OrderedDict()
    stat = collections.Counter()
    for s in sites:
        t = s.term
        hit = []                              # (target, class)
        if t[0] == "lit":
            if t[1] not in reg:
                hit = [(t[1], "(literal address)")]
            how, slot_txt = "literal address", "%08x" % t[1]
        elif t[0] == "vt":
            tg = by_vt.get(t[1], {}).get(t[2] // 4)
            if tg is not None and tg not in reg:
                hit = [(tg, vt_cls.get(t[1], "?"))]
            how, slot_txt = "literal vtable", "0x%x" % t[2]
        else:
            if t[1] % 4:
                continue
            k = t[1] // 4
            slot_txt = "0x%x" % t[1]
            sub = re.search(r"[+-]\s*(?:0x[0-9a-fA-F]+|\d+)\b", s.recv) is not None
            cand = [v for v in cons if k in by_vt[v] and (sub or primary.get(vt_cls[v]) == v)]
            r_ = strip_parens(s.recv)
            sel, how = [], ""
            mo = member_off(s.ef, s.recv, s.ef.end)
            if mo is not None:
                # `owner + off`: an embedded member stored there, or the secondary base at that offset of the object; neither known: an
                # embedded member of some class (its primary vtable)
                mem = [v for v in cons if k in by_vt[v] and (v in members.get(mo, ()) or (col_off.get(v) == mo and mo not in members))]
                if mem:
                    sel, how = mem, "member +0x%x" % mo
                else:
                    cand = [v for v in cons if k in by_vt[v] and primary.get(vt_cls[v]) == v]
            if not sel and s.ef.pnames and s.ef.pnames[0] and r_ == s.ef.pnames[0] and not s.chain:
                own = {a_ for (n_, f_), a_ in reg_by_name.items() if n_ == s.ef.name and f_ == s.ef.file}
                cls_ = set()
                for a_ in own:
                    cls_ |= vt_classes.get(a_, set())
                sel = [v for v in cand if vt_cls[v] in cls_]
                how = "own class" if sel else ""
            rn = re.sub(r"[()\s]", "", s.recv)
            mine = [x for x in roles if x["file"].fullmatch(s.file) and x["func"].fullmatch(s.func) and x["recv"].fullmatch(rn) and x["slot"].fullmatch(slot_txt)]
            only = [x for x in mine if x["mode"] == "only"]
            if not sel and only:
                sel = [v for v in cand if only[0]["cls"].search(vt_cls[v])]
                how = "role"
                if not sel:
                    stat["sites"] += 1
                    continue              # the role's classes have no constructed vtable with that slot: nothing to reach
            if not sel:
                toks = ident_tokens_c(s.recv)
                sel = [v for v in cand if any(tok_ok(tk, vt_cls[v]) for tk in toks)]
                how = "receiver" if sel else ""
            if not sel:
                # a generic receiver: the `not` roles drop classes; then the observed introducers, else every hierarchy with a port at slot k
                nots = [x for x in mine if x["mode"] == "not"]
                cand = [v for v in cand if not any(x["cls"].search(vt_cls[v]) for x in nots)]
                obs = {introducer(v, k) for t_ in observed.get((s.file, s.func), ()) for v in at_slot.get((t_, k), ())}
                if obs:
                    # the runs saw this function's slot-k calls reach classes of these introducers: the receiver's static type is one of them
                    sel = [v for v in cand if introducer(v, k) in obs]
                    how = "observed"
                else:
                    sel = [v for v in cand if introducer(v, k) in intro_reg[k]]
                    how = "generic"
            if how not in ("observed", "generic"):
                # a narrowed site the runs also reached: its receiver's static type declares slot k in one of the observed introducers
                obs = {introducer(v, k) for t_ in observed.get((s.file, s.func), ()) for v in at_slot.get((t_, k), ())}
                both = [v for v in sel if introducer(v, k) in obs]
                if both:
                    sel, how = both, how + "+observed"
            hit = [(by_vt[v][k], vt_cls[v]) for v in sel if by_vt[v][k] not in reg]
        stat["sites"] += 1
        skey = "%s:%d %s %s" % (s.file, s.line, s.func, slot_txt) + (" via " + ">".join(s.chain) if s.chain else "")
        for tg, cls in hit:
            if tg == PURECALL:
                continue
            why = allowed(tg, cls, s, slot_txt)
            if why is not None:
                if not why.startswith("OPEN "):
                    allowed_hits[tg].add(cls)
                continue
            e = found.setdefault(tg, dict(classes=set(), sites=set(), how=set(), abort=False, name=vname.get(tg, "")))
            e["classes"].add(cls)
            e["sites"].add(skey)
            e["how"].add(how)
            e["abort"] = e["abort"] or conv_aborts(s)
            bysite.setdefault(skey, dict(how=how, targets=set(), recv=" ".join(s.recv.split()), abort=conv_aborts(s)))["targets"].add(tg)
    stale = [k for k in allow if k not in used_allow]
    return dict(found=found, allowed=allowed_hits, open=open_hits, open_why={k[5:]: v for k, v in allow.items() if k.startswith("open:")}, stale=stale, constructed=len(cons), sites=stat["sites"], allow_n=len(allow), bysite=bysite,
                members=len(members))


def load_roles(R):
    """core/tools/sigcheck_vt_roles.tsv: what the receiver of a generic call IS, read off the code by hand.
    Columns (TAB): file regex, function regex, receiver regex (the receiver text without spaces and parentheses), slot regex ("0x44"),
    mode, class regex (or `<Base`: Base and the classes derived from it, classes.tsv), reason. All regexes are full matches. mode `only`: the receiver is one of the classes whose name matches
    (re.search) the class regex; the call must fit EVERY registered port of those classes at the slot. mode `not`: the classes whose name
    matches cannot reach this call (the rest of the generic check, homogeneous / heterogeneous, runs on the others)."""
    path = os.path.join(R, "core/tools/sigcheck_vt_roles.tsv")
    out = []
    if not os.path.exists(path):
        return out
    bases = {}
    cpath = os.path.join(R, "_re/out/classes.tsv")
    if os.path.exists(cpath):
        for r in csv.DictReader(open(cpath, encoding="latin-1"), delimiter="\t"):
            bases.setdefault(r["class"], {b for b in (r["bases"] or "").split(",") if b and "@" not in b})   # the primary chain (a base at +off is a sub-object)

    class Derived:
        """class regex `<Base`: the class Base and every class with Base in its primary base chain (classes.tsv; `Entity@f0` does not count)"""
        def __init__(self, base):
            self.base, self.pattern = base, "<" + base

        def search(self, c):
            return c == self.base or self.base in bases.get(c, ())
    for ln in open(path, encoding="utf-8"):
        if not ln.strip() or ln.startswith("#"):
            continue
        f = ln.rstrip("\n").split("\t")
        if len(f) < 7:
            continue
        out.append(dict(file=re.compile(f[0]), func=re.compile(f[1]), recv=re.compile(f[2]), slot=re.compile(f[3]), mode=f[4],
                        cls=Derived(f[5][1:]) if f[5].startswith("<") else re.compile(f[5]), reason=f[6], used=0))
    return out


def run(R, reg, vtable_rows, product_side, allow_vt, flags=()):
    roles = load_roles(R)
    import bisect
    srcs = load_sources(R, product_side)
    funcs = collect_functions(srcs)
    ctx = Ctx(funcs, srcs)
    typedefs = collect_typedefs(srcs)
    path_of = {os.path.basename(p): p for p in srcs}
    funcs_by_path = collections.defaultdict(list)
    for f in funcs:
        funcs_by_path[path_of[f.file]].append(f)
    starts = {}

    def func_at(path, off):
        fl = funcs_by_path[path]
        if path not in starts:
            starts[path] = [x.start for x in fl]
        i = bisect.bisect_right(starts[path], off) - 1
        return fl[i] if i >= 0 and fl[i].start <= off < fl[i].end else None

    def typedef_kind(name, path, off):
        c = typedefs.get(name)
        if not c:
            return None
        same = [x for x in c if x[0] == path]
        if same:
            before = [x for x in same if x[1] <= off]
            return (before[-1] if before else same[0])[2]
        hdr = [x for x in c if x[0].endswith(".h")]
        return (hdr or c)[0][2]

    def type_kind(ttext, path, off):
        t = " ".join(ttext.split())
        k = parse_fnptr(t)
        if k:
            return k
        if IDENT.match(t):
            return typedef_kind(t, path, off)
        return None

    by_slot = collections.defaultdict(lambda: collections.defaultdict(list))      # slot index -> target -> [classes], every vtable
    by_slot_prim = collections.defaultdict(lambda: collections.defaultdict(list))  # the same, primary vtables only (the one at object +0)
    by_vt = {}                                                                     # vtable address -> {slot: target}
    primary = {}
    for r in vtable_rows:
        v_ = int(r["vtable"], 16)
        if r["class"] not in primary or v_ < primary[r["class"]]:
            primary[r["class"]] = v_       # the primary vtable is the class's first (lowest) one: the others belong to the secondary bases
    for r in vtable_rows:
        t = int(r["target"], 16)
        s_ = int(r["slot"])
        by_vt.setdefault(int(r["vtable"], 16), {})[s_] = t
        if t in reg:
            by_slot[s_][t].append(r["class"])
            if int(r["vtable"], 16) == primary[r["class"]]:
                by_slot_prim[s_][t].append(r["class"])
    stub_slots = collections.defaultdict(set)
    for r in vtable_rows:
        stub_slots[int(r["target"], 16)].add(int(r["slot"]))
    for tbl in (by_slot, by_slot_prim):
        for s_ in list(tbl):                      # a shared stub (007e3be0 empty, 00679000 ret 4 ...) sits at many slots of unrelated classes and
            for t in list(tbl[s_]):               # takes whatever the caller passes: it says nothing about the slot's signature
                if len(stub_slots[t]) >= 3:
                    del tbl[s_][t]
    proto = {a: port_proto(v[1], v[2]) for a, v in reg.items()}
    reg_by_name = {(v[0], v[3]): a for a, v in reg.items()}
    vt_classes = collections.defaultdict(set)
    for r in vtable_rows:
        vt_classes[int(r["target"], 16)].add(r["class"])

    stats = collections.Counter()
    unresolved = []
    unres_conv = []      # the conversion (file, line) of each `unresolved` entry (None: the type was not understood)
    raw_calls = []        # (path, function, offset, address expression, (ret kind, [arg kinds]), end of the conversion, kind)
    for p, (txt, raw) in srcs.items():
        for m in re.finditer(r"\bRX_FN\s*\(", txt):
            if txt[max(0, m.start() - 8): m.start()].endswith("define "):
                continue
            e = match_close(txt, m.end() - 1)
            a = split_top(txt[m.end(): e - 1])
            f = func_at(p, m.start())
            if len(a) != 2 or f is None:
                continue
            ck = type_kind(a[1], p, m.start())
            if ck is None:
                if "__typeof__" in a[1]:
                    stats["typeof: literal address of a named function"] += 1
                else:
                    stats["type not understood"] += 1
                    unresolved.append((os.path.basename(p), line_of(f, m.start()), "type " + a[1]))
                    unres_conv.append((os.path.basename(p), line_of(f, m.start())))
                continue
            raw_calls.append((p, f, m.start(), a[0], ck, e, "RX_FN"))
        for m in re.finditer(r"\brx_fn\s*\(", txt):
            f = func_at(p, m.start())
            if f is None or f.name in ("rx_fn", "rx_tcall"):
                continue
            e = match_close(txt, m.end() - 1)
            arg = txt[m.end(): e - 1]
            k = m.start() - 1
            while k >= 0 and txt[k] in " \t\n":
                k -= 1
            ck = None
            if k >= 0 and txt[k] == ")":
                depth, j = 0, k
                while j >= 0:
                    if txt[j] == ")":
                        depth += 1
                    elif txt[j] == "(":
                        depth -= 1
                        if depth == 0:
                            break
                    j -= 1
                ck = type_kind(txt[j + 1: k], p, m.start()) if j >= 0 else None
            else:
                mv = re.search(r"(\w+)\s*=\s*$", txt[max(0, m.start() - 40): m.start()])
                if mv:
                    casts = {" ".join(x.split()) for x in re.findall(r"\(\s*([^()]*\(\s*\*\s*\)\s*\([^()]*\))\s*\)\s*" + re.escape(mv.group(1)) + r"\b", txt[m.start(): f.end])}
                    if len(casts) > 1:
                        stats["generic dispatcher (several casts of one rx_fn result)"] += 1
                        continue
                    if casts:
                        ck = type_kind(next(iter(casts)), p, m.start())
            if ck is None:
                stats["rx_fn without a typed cast (presence test)"] += 1
                continue
            raw_calls.append((p, f, m.start(), arg, ck, e, "rx_fn"))
        for m in re.finditer(r"\brx_(vcall|tcall)\s*\(", txt):
            f = func_at(p, m.start())
            if f is None or f.name in ("rx_vcall", "rx_tcall"):
                continue
            e = match_close(txt, m.end() - 1)
            a = split_top(txt[m.end(): e - 1])
            if len(a) != 4:
                continue
            n = eval_const(a[2])
            if n is None:
                stats["rx_vcall/rx_tcall with a variable count"] += 1
                continue
            expr = "U32(U32(%s, 0), %s)" % (a[0], a[1]) if m.group(1) == "vcall" else a[0]
            raw_calls.append((p, f, m.start(), expr, ("I", ["I"] * (n + 1)), e, m.group(1)))

    call_index = {}

    def calls_of(fname, fdefile):
        key = (fname, fdefile if not fdefile.endswith(".h") else "")
        if key in call_index:
            return call_index[key]
        out = []
        pat = re.compile(r"(?<![\w.>])" + re.escape(fname) + r"\s*\(")
        for p, (txt, raw) in srcs.items():
            if key[1] and os.path.basename(p) != key[1]:
                continue         # a function defined in a .c file is static: only its own file calls it
            for m in pat.finditer(txt):
                f = func_at(p, m.start())
                if f is None or f.name == fname:
                    continue
                e = match_close(txt, m.end() - 1)
                out.append((p, f, m.start(), split_top(txt[m.end(): e - 1])))
        call_index[key] = out
        return out

    def resolve(f, expr, pos, depth, chain, used=True):
        """-> [(leaf, function holding the final expression, wrapper chain, is the result used)]"""
        res = []
        for lf in lazy(ctx, f, expr, pos):
            if lf[0] == "tmpl":
                if depth >= 4:
                    res.append((("fail", "wrapper chain too long"), f, chain, used, pos))
                    continue
                cs = [c for c in calls_of(f.name, f.file) if len(c[3]) == len(f.pnames)]
                if not cs:
                    res.append((("fail", "wrapper %s has no caller" % f.name), f, chain, used, pos))
                for (cp, cf, coff, args) in cs:
                    sub = subst_idents(lf[1], {pn: "(" + a + ")" for pn, a in zip(f.pnames, args) if pn})
                    # the wrapper hands the typed call's value on to its caller: the value is used when the wrapper uses it and its caller does
                    res += resolve(cf, sub, coff, depth + 1, chain + [f.name], used and stmt_value_used(cf.text, coff))
            else:
                res.append((lf, f, chain, used, pos))
        return res

    sites = []
    for (p, f, off, expr, ck, e, how) in raw_calls:
        used = value_used_for(f, off, e) if how == "RX_FN" else (True if how == "rx_fn" else stmt_value_used(f.text, off))
        ids = set(re.findall(r"(?<![\w.>])([A-Za-z_]\w*)\b(?!\s*\()", expr))
        body = f.text[f.boff: f.end]
        disp = set()
        for v in ids:
            for mm in re.finditer(r"(?<![\w.>])" + re.escape(v) + r"\s*[!=]=\s*(0x[0-9a-fA-F]+)|(0x[0-9a-fA-F]+)\s*[!=]=\s*" + re.escape(v) + r"\b", body):
                disp.add(int(mm.group(1) or mm.group(2), 16))
        for (lf, ef, chain, used_, epos) in resolve(f, expr, off, 0, [], used):
            s = Site()
            s.file, s.func, s.ctype, s.used, s.kind = ef.file, ef.name, ck, used_, how
            s.line = line_of(ef, epos)
            s.chain, s.ef, s.dispatch, s.orig = chain, ef, disp, " ".join(expr.split())
            s.conv = (f.file, line_of(f, off))      # where the typed conversion itself is (a wrapper's body for a wrapper chain): the runtime trace (RX_FN_TRACE) records this file:line
            if lf[0] == "fail":
                stats["unresolved"] += 1
                unresolved.append((f.file, s.line, "%s: %s -> %s" % (how, s.orig[:50], lf[1])))
                unres_conv.append(s.conv)
                continue
            s.term = lf
            s.recv = lf[2] if lf[0] == "slot" else ""
            s.slotoff = lf[1] if lf[0] == "slot" else None
            sites.append(s)

    ABBR = {"veh": "vehicle", "beh": "behavior", "sim": "simulation", "cam": "camera", "act": "activity", "drv": "driver", "ent": "entity",
            "mod": "modifier", "bike": "motocross", "moto": "motocross", "atv": "atv", "utv": "utv"}

    @functools.lru_cache(maxsize=None)
    def tok_ok(tok, cls):
        t = ABBR.get(tok, tok)
        t = t[:-1] if t.endswith("s") and len(t) > 4 else t
        return t in words(cls)

    def narrow(slot, s):
        # a call through U32(U32(x, 0), off) reads the primary vtable; a receiver that is an offset into an object (`e + 0x90`) is a
        # sub-object whose vptr is one of the secondary vtables
        sub = re.search(r"[+-]\s*(?:0x[0-9a-fA-F]+|\d+)\b", s.recv) is not None
        cands = (by_slot if sub else by_slot_prim).get(slot, {})
        if not cands:
            return {}, "no registered port"
        # the receiver is the function's own `this` (its first parameter) and the function is itself a vtable target: it only runs on the
        # classes whose vtable holds it
        r_ = strip_parens(s.recv)
        if s.ef.pnames and s.ef.pnames[0] and r_ == s.ef.pnames[0] and not s.chain:
            own = {a_ for (n_, f_), a_ in reg_by_name.items() if n_ == s.ef.name and f_ == s.ef.file}
            cls_ = set()
            for a_ in own:
                cls_ |= vt_classes.get(a_, set())
            if cls_:
                hit = {a: [c for c in cl if c in cls_] for a, cl in cands.items() if any(c in cls_ for c in cl)}
                if hit:
                    return hit, "own class (`%s` is the `this` of %s, a vtable target of %d classes)" % (r_, s.ef.name, len(cls_))
        rn = re.sub(r"[()\s]", "", s.recv)
        sl_ = "0x%x" % (slot * 4)
        mine = [r_ for r_ in roles if r_["file"].fullmatch(s.file) and r_["func"].fullmatch(s.func) and r_["recv"].fullmatch(rn) and r_["slot"].fullmatch(sl_)]
        for r_ in mine:
            if r_["mode"] == "only":      # a role is read off the code: it wins over the receiver's words (`map` is not a KeyMap)
                r_["used"] += 1
                hit = {a: [c for c in cl if r_["cls"].search(c)] for a, cl in cands.items() if any(r_["cls"].search(c) for c in cl)}
                return hit, "role: " + r_["reason"][:60]
        toks = ident_tokens_c(s.recv)
        hit = {a: c for a, c in cands.items() if any(tok_ok(t, cl) for t in toks for cl in c)}
        if hit:
            return hit, "receiver `%s`" % " ".join(s.recv.split())[:40]
        nots = [r_ for r_ in mine if r_["mode"] == "not"]
        if nots:
            hit = {}
            for a, cl in cands.items():
                keep = [c for c in cl if not any(r_["cls"].search(c) for r_ in nots)]
                if keep:
                    hit[a] = keep
            for r_ in nots:
                r_["used"] += 1
            return hit, "unnarrowed"
        return cands, "unnarrowed"

    port_used = set()
    func_by = {}
    for f_ in funcs:
        func_by.setdefault((f_.name, f_.file), f_)

    def param_used(a, i):
        """does the port's C body read its parameter i? (an unread parameter cannot be mis-passed)"""
        name, _, _, file = reg[a]
        f_ = func_by.get((name, file))
        if f_ is None or i >= len(f_.pnames) or not f_.pnames[i]:
            return True
        body = re.sub(r"\(void\)\s*\w+\s*;", "", f_.text[f_.boff:f_.end])
        return re.search(r"\b" + re.escape(f_.pnames[i]) + r"\b", body) is not None

    def compare(s, targets):
        out = collections.defaultdict(list)
        cr, cargs = s.ctype
        for a in targets:
            if ("vtport:%08x:0x%x" % (a, s.slotoff)) in allow_vt if s.slotoff is not None else False:
                port_used.add("vtport:%08x:0x%x" % (a, s.slotoff))
                continue
            pr, pargs = proto[a]
            msg = []
            if cr != pr:
                if cr == "I" and pr == "L":
                    pass
                elif cr == "V" and pr in ("I", "L", "F", "D"):
                    pass
                elif cr == "V":
                    msg.append("the call ignores the result but the port returns %s (a hidden result pointer the call does not pass)" % kind_name(pr))
                elif pr == "V":
                    if s.used:
                        msg.append("the call uses a %s result but the port returns void" % kind_name(cr))
                else:
                    msg.append("the call returns %s, the port returns %s" % (kind_name(cr), kind_name(pr)))
            if len(pargs) > len(cargs) and any(param_used(a, i) for i in range(len(cargs), len(pargs))):
                msg.append("the port takes %d parameters, the call passes %d" % (len(pargs), len(cargs)))
            for i in range(min(len(pargs), len(cargs))):
                if pargs[i] != cargs[i] and param_used(a, i):
                    msg.append("argument %d is %s in the call, %s in the port" % (i, kind_name(cargs[i]), kind_name(pargs[i])))
            for mtxt in msg:
                out[mtxt].append(a)
        return out

    # ---- class families: classes that share a target at 3 or more slots (stubs that sit at 3+ different slots, like _purecall, and targets
    # shared by more than 40 classes do not count) are one hierarchy; the ports at one slot of one hierarchy implement one virtual function
    slots_of = collections.defaultdict(set)
    for r in vtable_rows:
        slots_of[int(r["target"], 16)].add(int(r["slot"]))
    shared = collections.defaultdict(set)
    for r in vtable_rows:
        t_ = int(r["target"], 16)
        if len(slots_of[t_]) < 3:
            shared[(int(r["slot"]), t_)].add(r["class"])
    pair_n = collections.Counter()
    for cl in shared.values():
        if 1 < len(cl) <= 40:
            for x, y in itertools.combinations(sorted(cl), 2):
                pair_n[(x, y)] += 1
    parent = {}

    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for (x, y), n in pair_n.items():
        if n >= 3:
            parent[find(x)] = find(y)
    fam_of = {r["class"]: find(r["class"]) for r in vtable_rows}
    fam_ports = collections.defaultdict(lambda: collections.defaultdict(dict))        # slot -> family -> {port: [classes]}
    for sl, d in by_slot.items():
        for a_, cls in d.items():
            for c_ in cls:
                fam_ports[sl][fam_of[c_]].setdefault(a_, []).append(c_)

    findings, allowed, weak = [], [], []
    hetero = []
    stat_n = collections.Counter()
    dedup = set()
    for s in sites:
        t = s.term
        fam_note = ""
        if t[0] == "lit":
            groups = [("literal", {t[1]: ["literal"]})] if t[1] in reg else []
            how_narrow, keyslot = "literal address", "lit%08x" % t[1]
        elif t[0] == "vt":
            tg = by_vt.get(t[1], {}).get(t[2] // 4)
            groups = [("vtable", {tg: ["vtable %08x" % t[1]]})] if tg in reg else []
            how_narrow, keyslot = "literal vtable", "0x%x" % t[2]
        else:
            if t[1] % 4:
                continue
            targets, how_narrow = narrow(t[1] // 4, s)
            keyslot = "0x%x" % t[1]
            fams = collections.OrderedDict()
            for a_, cls in targets.items():
                for c_ in cls:
                    fams.setdefault(fam_of[c_], {}).setdefault(a_, []).append(c_)
            groups = list(fams.items())          # the narrowed classes only, grouped by hierarchy
        if not groups:
            stat_n["no registered port at the target"] += 1
            continue
        stat_n["checked (%s)" % ("literal address" if t[0] == "lit" else "vtable slot")] += 1
        unn = how_narrow == "unnarrowed"
        if unn:
            stat_n["unnarrowed"] += 1
        if unn:
            # nothing says which class reaches the call: it is safe whatever the receiver is only if the call fits EVERY port at the slot
            allp = {}
            for f_, g in groups:
                allp.update(g)
            probs = compare(s, list(allp))
            if not probs:
                stat_n["homogeneous"] += 1
                continue
            fam_fit, fam_bad = [], []
            for f_, g in groups:
                (fam_bad if compare(s, list(g)) else fam_fit).append((f_, g))
            if s.dispatch:
                dset = [a_ for a_ in allp if a_ in s.dispatch]
                rest = [a_ for a_ in allp if a_ not in s.dispatch]
                if dset and not compare(s, dset):
                    stat_n["dispatch on the target recognised"] += 1      # the call site tells the ports apart by address
                    continue
            hetero.append((s, keyslot, probs, allp, fam_fit, fam_bad))
            continue
        if how_narrow.startswith("role:"):
            # the receiver's class was read off the code (sigcheck_vt_roles.tsv): the call must fit every port of those classes at the slot
            merged = {}
            for f_, g in groups:
                merged.update(g)
            probs = compare(s, list(merged))
            if not probs:
                stat_n["checked by role"] += 1
                continue
            if s.dispatch:
                dset = [a_ for a_ in merged if a_ in s.dispatch]
                rest = [a_ for a_ in merged if a_ not in s.dispatch]
                if (dset and not compare(s, dset)) or (rest and not compare(s, rest)):
                    stat_n["dispatch on the target recognised"] += 1
                    continue
            key = (s.file, s.func, keyslot, s.line)
            if key in dedup:
                continue
            dedup.add(key)
            allow_key = "vt:%s:%s:%s" % (s.file, s.func, keyslot)
            item = (s.file, s.line, s.func, keyslot, s.kind, s.chain, how_narrow, probs, merged, allow_key, s.orig)
            (allowed if allow_key in allow_vt else findings).append(item)
            continue
        fitting = [(f_, g) for f_, g in groups if any(not compare(s, [a_]) for a_ in g)]
        if fitting or t[0] in ("lit", "vt"):
            pick = fitting or groups                 # the call fits a class of that hierarchy: its siblings must fit too (a literal address: the port itself)
        else:
            # no class the narrowing found fits the call: the narrowing is a guess (words of a name), so this is only counted
            stat_n["weak: no narrowed port fits"] += 1
            allp = {}
            for f_, g in groups:
                allp.update(g)
            weak.append((s.file, s.line, s.func, keyslot, how_narrow, {m: [("%08x" % a_) for a_ in l] for m, l in compare(s, list(allp)).items()}))
            continue
        merged = {}
        for f_, g in pick:
            merged.update(g)
        probs = compare(s, list(merged))
        if not probs:
            continue
        if s.dispatch:
            dset = [a_ for a_ in merged if a_ in s.dispatch]
            rest = [a_ for a_ in merged if a_ not in s.dispatch]
            if (dset and not compare(s, dset)) or (rest and not compare(s, rest)):
                stat_n["dispatch on the target recognised"] += 1
                continue
        key = (s.file, s.func, keyslot, s.line)
        if key in dedup:
            continue
        dedup.add(key)
        allow_key = "vt:%s:%s:%s" % (s.file, s.func, keyslot)
        item = (s.file, s.line, s.func, keyslot, s.kind, s.chain, how_narrow + fam_note, probs, merged, allow_key, s.orig)
        (allowed if allow_key in allow_vt else findings).append(item)
    reach_res = reach_pass(R, srcs, funcs, sites, reg, vtable_rows, by_vt, primary, fam_of, reg_by_name, vt_classes, roles, tok_ok)
    summary_roles = [(r_["reason"], r_["used"]) for r_ in roles]
    stale = sorted(k for k in allow_vt if k not in {a[9] for a in findings + allowed} and k not in port_used)
    # ---- the data vtrace_check.py needs (sigcheck.py --vt-dump): the registered ports' prototypes, the typedef kinds, every typed conversion
    # (file:line, kind, C type kinds, whether its value is used) and every resolved call site / unresolved conversion with its class
    hetero_ids = {id(h[0]) for h in hetero}
    export = dict(
        ports={"%08x" % a: dict(name=reg[a][0], file=reg[a][3], ret=proto[a][0], args=proto[a][1], used=[param_used(a, i) for i in range(len(proto[a][1]))],
                                text="%s %s(%s)" % (reg[a][1].strip(), reg[a][0], " ".join(reg[a][2].split())))
               for a in reg},
        typedefs={n: [(os.path.basename(x[0]), srcs[x[0]][0].count("\n", 0, x[1]) + 1, x[2][0], x[2][1]) for x in l] for n, l in typedefs.items()},      # (file, line, ret, args)
        conversions=[dict(file=os.path.basename(p), line=line_of(f, off), how=how, ret=ck[0], args=ck[1], used=bool(value_used_for(f, off, e) if how == "RX_FN" else (True if how == "rx_fn" else stmt_value_used(f.text, off))))
                     for (p, f, off, expr, ck, e, how) in raw_calls],
        sites=[dict(file=s.file, func=s.func, line=s.line, conv=list(s.conv), chain=s.chain, how=s.kind, term=s.term[0],
                    slot=("0x%x" % s.slotoff) if s.slotoff is not None else (("0x%x" % s.term[2]) if s.term[0] == "vt" else ("%08x" % s.term[1])),
                    vtable=("%08x" % s.term[1]) if s.term[0] == "vt" else None, recv=" ".join(s.recv.split()), used=bool(s.used), hetero=id(s) in hetero_ids,
                    ret=s.ctype[0], args=s.ctype[1])
               for s in sites],
        unresolved=[dict(file=u[0], line=u[1], text=u[2], conv=list(c)) for u, c in zip(unresolved, unres_conv)])
    summary = dict(conversions=len(raw_calls), expansions=len(sites), stats=stats, checked=stat_n, unresolved=len(unresolved), weak=weak, hetero=hetero, roles=roles, export=export,
                   reach=reach_res)
    return findings, allowed, stale, summary, unresolved


def show_item(item, reg, note=""):
    (file, line, func, keyslot, kind, chain, how_narrow, probs, targets, allow_key, text) = item
    parts = []
    for msg, tl in probs.items():
        ps = ", ".join("%08x %s" % (a, reg[a][0]) for a in tl[:3]) + (" (+%d)" % (len(tl) - 3) if len(tl) > 3 else "")
        parts.append("%s [ports: %s]" % (msg, ps))
    return "vtslot\t%s:%d\t%s\tslot %s\t%s%s\t(narrowed by %s)\t%s%s" % (
        file, line, func, keyslot, kind, " via " + ">".join(chain) if chain else "", how_narrow, " | ".join(parts), note)


def show_hetero(h, reg):
    """one line per unnarrowed call site whose C type does not fit every port at the slot (--vt-hetero)"""
    s, keyslot, probs, ports, fam_fit, fam_bad = h
    parts = []
    for msg, tl in probs.items():
        ps = ", ".join("%08x %s" % (a, reg[a][0]) for a in tl[:4]) + (" (+%d)" % (len(tl) - 4) if len(tl) > 4 else "")
        parts.append("%s [%s]" % (msg, ps))
    return "vthetero\t%s:%d\t%s\tslot %s\t%s%s\trecv=%s\t%s\t%s" % (
        s.file, s.line, s.func, keyslot, s.kind, " via " + ">".join(s.chain) if s.chain else "", " ".join(s.recv.split())[:40],
        "used" if s.used else "unused", " | ".join(parts)) + "\n\tFIT %s\n\tBAD %s" % (
        "; ".join("%s(%d)" % (f_, sum(len(c) for c in g.values())) for f_, g in fam_fit), "; ".join("%s(%d)" % (f_, sum(len(c) for c in g.values())) for f_, g in fam_bad))
