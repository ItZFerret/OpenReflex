#!/usr/bin/env python3
"""sigcheck.py <repo> [--all] [--raw] [--dump=addr,addr] -- registered core functions whose C signature does not line up POSITIONALLY with
the original's stack.

Why: in the product build a registered function reached through a pointer is called with the original's stack dwords in order:
rx_vcall / rx_tcall (core/game/rx_vcall.h: (obj, stack args...)), the event-proxy thunks (build_vcall_thunks.c, build_event_proxies.c) and
rx_call_original's integer-signature table (src/rx_mem.c, tools/gen_fnsig.py: (ECX if thiscall, stack args...)). C parameter j of a
thiscall function is therefore `this` for j == 0 and stack argument j-1 afterwards. A signature that leaves out a stack argument the
original READS, or puts the arguments in another order, gets the wrong values there. The emulator oracle cannot see it (the RX_PORT
adapter picks m.arg(n) by index). Found first at ActivityAudioLogic::vf27 00582150 (commit 84ea8bd).

What it checks. Only functions that can be reached through a pointer are looked at: a vtable slot (_re/out/vtables.tsv), an address that
is stored somewhere in the image (an immediate or a data word: event delegates, tables) or written as a literal in the core's sources
(act_call_fn(0x..., ...)). A function only ever called directly is called through its owner's prototype and cannot be mis-ordered by a
positional caller. Register-argument (LTCG) functions (EAX / EDX read first, or an adapter that passes ESI / EDI ...) are skipped unless
they are virtual targets: the positional path does not exist for them.
  1. The original's disassembly is walked along its control flow with the stack depth tracked (push / pop / sub / add / calls with a
     known RET n / ebp frames). A read of [esp + k] / [ebp + k] that lands in the argument area is a READ of that stack argument; ECX
     read before it is written is a read of `this`.
  2. needed = (1 for `this`, when the function is a virtual target, reads ECX or its adapter passes ECX first) + (highest stack argument
     read + 1). Unread arguments before a read one still need a (dummy) C parameter: the order is positional.
  3. A function is a FINDING when its C parameter count is below `needed`, or when its RX_PORT adapter (core/oracle/*.cpp) calls it with
     arguments that are not (ECX, arg0, arg1, ...) in order: the reverse case, a register argument before the stack arguments or stack
     arguments swapped.
Reviewed-harmless functions are listed with a reason in core/tools/sigcheck_allow.tsv (address TAB reason) and are not reported (--all
prints them). An allow entry that no longer matches a finding is reported as stale. --raw ignores the allow-list. --dump=a,b prints what
the walk found for those addresses (review aid).
Exit status 1 when there are findings that are not allow-listed.

Typed calls through a vtable slot (core/tools/sigcheck_vt.py, run at the end of every report): RX_FN(U32(U32(obj, 0), off), <C type>) and the
other typed conversions of an address (rx_fn casts, rx_vcall) are compared with the prototype of every registered port that sits at that slot of
a vtable that can reach the call (return kind: void / integer / 64-bit / float / double / rx_f80 / struct; argument count and float-vs-integer
arguments). Its findings are counted with the others. Allow-list lines for it: `vt:<file>:<function>:<slot offset>` (one call site) or
`vtport:<port address>:<slot offset>` (a port that is not a sibling of the call's receiver), TAB reason. --vt-weak prints the typed calls that fit
no narrowed port (not findings); see the module's docstring.

Vtable reach (sigcheck_vt.reach_pass, w-vtunported): every slot call's reachable targets (the constructed classes that can be its receiver) must
be registered: an unregistered one is a stop in the product (rx_call_original). Counted with the findings; core/tools/vtreach_allow.tsv lists the
proven-unreachable ones (and `open:` ones, the work list); core/tools/vtreach_observed.tsv is the runtime evidence for generic receivers.
--vt-reach lists the unregistered and open targets, --vt-reach-sites the call sites.

The ECX pass (w-pose446): a vtable-slot call through world_call_ctx (WORLD_CTX / WORLD_CTX0 / WORLD_CTX_F) whose inline &WCTX(...) names no
.ecx is a finding: the product runs the slot's port only with ECX set, else it stops at the (ported) target. Printed as `ecx` lines.
"""
import os, re, sys, glob, csv, struct, collections
import capstone, pefile
from capstone import x86_const as X

args = [a for a in sys.argv[1:] if not a.startswith("--")]
FLAGS = {a for a in sys.argv[1:] if a.startswith("--")}
R = os.path.abspath(args[0]) if args else os.getcwd()
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

ALLOW = {}
ALLOW_VT = {}
_allow_file = os.path.join(HERE, "sigcheck_allow.tsv")
if os.path.isfile(_allow_file) and "--raw" not in FLAGS:
    for ln in open(_allow_file, encoding="utf-8"):
        ln = ln.rstrip("\n")
        if not ln.strip() or ln.startswith("#"):
            continue
        a, reason = ln.split("\t", 1)
        if a.startswith("vt:") or a.startswith("vtport:"):
            ALLOW_VT[a] = reason          # typed vtable-slot calls: sigcheck_vt.py
        else:
            ALLOW[int(a, 16)] = reason

pe = pefile.PE(os.path.join(R, "game files", "Archive", "MXReflex.exe"), fast_load=True)
img = bytes(pe.get_memory_mapped_image())
B = pe.OPTIONAL_HEADER.ImageBase
size = {}
for r in csv.DictReader(open(os.path.join(R, "_re/out/functions.tsv"), encoding="latin-1"), delimiter="\t"):
    try:
        size[int(r["entry"], 16)] = int(r["size"] or 0)
    except ValueError:
        pass
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True

# ---- the registered core functions and their C signatures (the macro-expanded scan the product's fnsig table uses) --------------------
import gen_fnsig
cc = gen_fnsig.compiler()
reg = {}      # addr -> (name, ret, params text, file)
gdefs = {}
scans = []
for path in sorted(glob.glob(os.path.join(R, "core/game/*.c"))):
    found, _, _, defs = gen_fnsig.scan(path, cc)
    scans.append((path, found, defs))
    for n, d in defs.items():
        gdefs.setdefault(n, d)
for path, found, defs in scans:
    for addr, name, _ in found:
        d = defs.get(name) or gdefs.get(name)
        if d is None:
            continue
        reg[addr] = (name, d[0], d[1].strip(), os.path.basename(path))

def product_side(txt):
    """the source as the product build sees it: `#ifdef RX_ORACLE` / `#ifdef RX_FN_TRACE` bodies dropped (`#else` kept), `#ifndef RX_ORACLE` bodies kept"""
    out, stack = [], []        # stack of (keeping this branch?, is an RX_ORACLE conditional)
    for ln in txt.split("\n"):
        t = ln.strip()
        if t.startswith("#if"):
            if re.match(r"#\s*ifdef\s+(RX_ORACLE|RX_FN_TRACE)\b", t) or re.match(r"#\s*if\s+defined\(RX_ORACLE\)", t):      # RX_FN_TRACE: the traced build's variants (w-vtlog) are not the product's
                stack.append([False, True])
            elif re.match(r"#\s*ifndef\s+RX_ORACLE\b", t):
                stack.append([True, True])
            else:
                stack.append([True, False])
            out.append("")
            continue
        if t.startswith("#else") and stack:
            if stack[-1][1]:
                stack[-1][0] = not stack[-1][0]
            out.append("")
            continue
        if t.startswith("#endif") and stack:
            stack.pop()
            out.append("")
            continue
        out.append(ln if all(k for k, _ in stack) else "")
    return "\n".join(out)

bodies = {}      # function name -> (parameter names, body text without comments), from the raw sources
def _collect_bodies():
    for path in sorted(glob.glob(os.path.join(R, "core/game/*.c"))):
        txt = open(path, encoding="utf-8", errors="replace").read()
        txt = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), txt, flags=re.S)
        txt = re.sub(r"//[^\n]*", "", txt)
        txt = product_side(txt)
        for m in re.finditer(r"(?m)^[\w \t\*]*?\b(\w+)\s*\(([^;{}()]*)\)\s*\{", txt):
            name = m.group(1)
            if name in bodies:
                continue
            depth, j = 0, m.end() - 1
            while j < len(txt):
                if txt[j] == "{":
                    depth += 1
                elif txt[j] == "}":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            names = []
            for prm in m.group(2).split(","):
                ids = re.findall(r"[A-Za-z_]\w*", re.sub(r"\[[^\]]*\]", "", prm))
                names.append(ids[-1] if ids and len(ids) >= 2 else "")
            bodies[name] = (names, txt[m.end():j])
_collect_bodies()

def used_params(name):
    """indexes of the C parameters the body really uses (a `(void)p;` is not a use)"""
    b = bodies.get(name)
    if not b:
        return None
    names, body = b
    body = re.sub(r"\(void\)\s*\w+\s*;", "", body)
    return {k for k, n in enumerate(names) if n and re.search(r"\b" + re.escape(n) + r"\b", body)}

def c_params(p):
    if p in ("", "void"):
        return []
    return [x.strip() for x in p.split(",") if x.strip()]

# ---- which registered functions can be reached through a pointer ----------------------------------------------------------------------
vtable_target = collections.defaultdict(list)    # addr -> [(class, slot)]
for r in csv.DictReader(open(os.path.join(R, "_re/out/vtables.tsv"), encoding="latin-1"), delimiter="\t"):
    vtable_target[int(r["target"], 16)].append((r["class"], int(r["slot"])))
entries = set(reg)
taken = collections.defaultdict(int)             # addr -> references found in the image (code immediates, data words)
pe.parse_data_directories([pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG"]])
lc = getattr(pe, "DIRECTORY_ENTRY_LOAD_CONFIG", None)
seh_lo = seh_hi = 0                              # the safe-SEH handler table holds RVAs, which collide with small addresses
if lc is not None and getattr(lc.struct, "SEHandlerTable", 0):
    seh_lo = lc.struct.SEHandlerTable
    seh_hi = seh_lo + 4 * lc.struct.SEHandlerCount
sect = [(B + s_.VirtualAddress, B + s_.VirtualAddress + s_.Misc_VirtualSize, bool(s_.Characteristics & 0x20000000)) for s_ in pe.sections]
def _section(va):
    for lo, hi, ex in sect:
        if lo <= va < hi:
            return ex
    return None
for off in range(4):
    chunk = img[off: off + (len(img) - off) // 4 * 4]
    for k, (v,) in enumerate(struct.iter_unpack("<I", chunk)):
        if v not in entries:
            continue
        i = off + 4 * k                          # image offset of the dword
        ex = _section(B + i)
        if ex is None or seh_lo <= B + i < seh_hi:
            continue
        if ex:
            # an absolute address in code: push imm32, mov reg, imm32, mov [mem], imm32 (the prefix bytes before it)
            pb = img[i - 1] if i else 0
            if not (pb == 0x68 or 0xB8 <= pb <= 0xBF or any(i - j >= 0 and img[i - j] == 0xC7 for j in range(2, 8))):
                continue
        elif i % 4:
            continue                             # a table word is aligned
        taken[v] += 1
# addresses written as literals in the core's own sources (act_call_fn(0x0075f890, ...), tables, bound handlers): also reached by address
# in the product. RX_FUNCTION / RX_PORT lines are excluded.
core_literal = collections.defaultdict(set)
for path in sorted(glob.glob(os.path.join(R, "core/game/*.[ch]")) + glob.glob(os.path.join(R, "core/game/*.inc"))):
    txt = re.sub(r"/\*.*?\*/", " ", open(path, encoding="utf-8", errors="replace").read(), flags=re.S)
    txt = re.sub(r"//[^\n]*", "", txt)
    for ln in txt.split("\n"):
        if "RX_FUNCTION(" in ln or "PORT" in ln:
            continue
        for m in re.finditer(r"\b0x0*([0-9a-fA-F]{6,8})\b", ln):
            v = int(m.group(1), 16)
            if v in entries:
                core_literal[v].add(os.path.basename(path))

# call sites that name a registered function by literal address and pass the convention: wrapper(0xADDR, RX_conv, ecx, n, args). In the product
# rx_call_original / act_call_fn call the registered function positionally with exactly that convention (ECX first for RX_THISCALL).
literal_calls = []     # (addr, conv, n, file, line)
for path in sorted(glob.glob(os.path.join(R, "core/game/*.[ch]"))):
    txt = product_side(re.sub(r"//[^\n]*", "", re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), open(path, encoding="utf-8", errors="replace").read(), flags=re.S)))
    for m in re.finditer(r"\b(?!RX_FUNCTION)(\w+)\(\s*(0x[0-9a-fA-F]{6,8})\s*,\s*RX_(THISCALL|STDCALL|CDECL)\s*,\s*([^,]+?)\s*,\s*(\d+)\s*,", txt):
        a = int(m.group(2), 16)
        if a in reg:
            literal_calls.append((a, m.group(3), int(m.group(5)), os.path.basename(path), txt.count("\n", 0, m.start()) + 1, m.group(1)))

# ---- the stack-argument reads of an original -----------------------------------------------------------------------------------------------
_info_cache = {}
_ins_cache = {}

def decode(va):
    if va in _ins_cache:
        return _ins_cache[va]
    n = size.get(va, 0)
    out = None
    if n > 0:
        code = img[va - B: va - B + n]
        out = {i.address: i for i in md.disasm(code, va)}
    _ins_cache[va] = out
    return out

def ret_counts(va):
    ins = decode(va)
    if not ins:
        return None
    return {int(i.op_str, 16) if i.op_str else 0 for i in ins.values() if i.mnemonic == "ret"}

REGSUB = {"ecx": {"ecx", "cx", "cl", "ch"}, "eax": {"eax", "ax", "al", "ah"}, "edx": {"edx", "dx", "dl", "dh"},
          "esi": {"esi", "si"}, "edi": {"edi", "di"}, "ebx": {"ebx", "bx", "bl", "bh"}}

def _solve(equations, bound):
    """equations: [(constant, pos, neg)] meaning sum(u[pos]) - sum(u[neg]) == constant, with 0 <= u[site] <= bound[site] and u a multiple of 4.
    Returns {site: bytes} for every site when the system has exactly one solution, else {} (the reads that depend on it stay unresolved)."""
    sites = sorted({x for _, ps, ns in equations for x in tuple(ps) + tuple(ns)})
    dom = {x: list(range(0, bound.get(x, 0) + 1, 4)) for x in sites}
    sols = []
    def holds(cur, eq):
        c, ps, ns = eq
        return sum(cur[x] for x in ps) - sum(cur[x] for x in ns) == c
    def rec(k, cur):
        if len(sols) > 1:
            return
        if k == len(sites):
            if all(holds(cur, eq) for eq in equations):
                sols.append(dict(cur))
            return
        x = sites[k]
        for v in dom[x]:
            cur[x] = v
            if all(holds(cur, eq) for eq in equations if x in eq[1] + eq[2] and all(y in cur for y in eq[1] + eq[2])):
                rec(k + 1, cur)
            del cur[x]
    if len(sites) <= 14:
        rec(0, {})
    return sols[0] if len(sols) == 1 else {}

def analyze(va):
    """dict(reads, addr_taken, ecx, regs, tail, rets, unknown, lost, maxarg) for the function at va, or None.
    The stack depth `d` below the entry stack pointer is (constant, call sites): a call to a callee that pops an unknown amount (an indirect
    call, a callee with several RETs) adds that site's unknown pop u[site] >= 0; every ret must be at depth 0, which gives equations that fix
    the unknowns; the reads that were recorded against a symbolic depth are resolved afterwards."""
    if va in _info_cache:
        return _info_cache[va]
    _info_cache[va] = None     # recursion guard
    ins = decode(va)
    if not ins:
        return None
    order = sorted(ins)
    end = order[-1] + ins[order[-1]].size
    reads, addr_taken, rets, regs_read = set(), set(), set(), set()
    pending = []            # (disp, d, is_lea) recorded against a symbolic depth
    equations = []          # (constant, positive sites, negative sites) per ret and per join of unequal depths
    first_depth = {}
    ecx_read = tail = unknown = lost = False
    seen = set()
    work = [(va, (0, ()), None, frozenset(), 0)]     # (address, depth, depth at `mov ebp, esp`, registers written, pushes since the last call or stack adjustment)
    bound = {}              # unknown-pop site -> most dwords it can pop (the pushes pending at the call)

    def dadd(d, n):
        return None if d is None else (d[0] + n, d[1])

    while work:
        a, d, eb, wr, pp = work.pop()
        if a in ins and d is not None:
            f0 = first_depth.setdefault(a, d)
            if f0 != d:
                equations.append((f0[0] - d[0], tuple(sorted(set(d[1]) - set(f0[1]))) + (), tuple(sorted(set(f0[1]) - set(d[1])))))
                # f0 = c0 - sum(u0), d = c1 - sum(u1): c0 - c1 == sum(u0) - sum(u1)  ->  pos = sites only in u0, neg = sites only in u1
                equations[-1] = (f0[0] - d[0], tuple(sorted(set(f0[1]) - set(d[1]))), tuple(sorted(set(d[1]) - set(f0[1]))))
        if a not in ins or (a, wr) in seen:
            continue
        seen.add((a, wr))
        if len(seen) > 40000:
            unknown = True
            break
        i = ins[a]
        mn = i.mnemonic
        nxt = a + i.size
        ops = i.operands
        # registers read before they are written: ECX = this, EAX / EDX / ESI / EDI / EBX = register arguments
        try:
            rr, rw = i.regs_access()
        except capstone.CsError:
            rr, rw = [], []
        rn = {i.reg_name(x) for x in rr}
        wn = {i.reg_name(x) for x in rw}
        clearing = mn in ("xor", "sub", "sbb") and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG \
            and ops[0].reg == ops[1].reg
        nop = (mn == "lea" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_MEM and ops[1].mem.base == ops[0].reg
               and ops[1].mem.index == 0 and ops[1].mem.disp == 0) or (mn in ("mov", "xchg") and len(ops) == 2 and ops[0].type == X.X86_OP_REG
               and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg)
        const_write = mn in ("or", "and", "mov") and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_IMM \
            and (mn == "mov" or (mn == "or" and ops[1].imm in (-1, 0xffffffff)) or (mn == "and" and ops[1].imm == 0))
        if clearing or nop or const_write or (mn == "push" and i.op_str in ("ecx", "eax", "edx", "esi", "edi", "ebx", "ebp")):
            rn = set()          # `push esi` saving a register, `lea ecx, [ecx]`, or an idiom setting a register to a constant: not a use
        for r_, sub in REGSUB.items():
            if (rn & sub) and r_ not in wr:
                if r_ == "ecx":
                    ecx_read = True
                else:
                    regs_read.add(r_)
        wr2 = set(wr)
        for r_, sub in REGSUB.items():
            if wn & sub:
                wr2.add(r_)
        # memory operands in the argument area
        for op in ops:
            if op.type != X.X86_OP_MEM or op.mem.index != 0 or not op.mem.base:
                continue
            base = i.reg_name(op.mem.base)
            if base == "esp":
                if d is None:
                    if eb is None:
                        lost = True                      # an esp-relative access with the depth unknown: reads may be missed
                    continue
                if d[1]:
                    pending.append((op.mem.disp, d, mn == "lea", bool(op.access & capstone.CS_AC_READ)))
                    continue
                rel = op.mem.disp - d[0]                 # distance above the entry stack pointer (the return address is at 0)
            elif base == "ebp" and eb is not None:
                rel = op.mem.disp - eb
            else:
                continue
            if rel >= 4 and (rel - 4) // 4 < 32:
                k = (rel - 4) // 4
                if mn == "lea":
                    addr_taken.add(k)
                elif op.access & capstone.CS_AC_READ:
                    reads.add(k)
        # stack depth
        nd, neb = d, eb
        if "ebp" in wn and not (mn == "mov" and len(ops) == 2 and ops[1].type == X.X86_OP_REG and ops[1].reg == X.X86_REG_ESP):
            neb = None          # ebp reused as a general register: it no longer addresses the argument area
        if mn == "ret":
            rets.add(int(i.op_str, 16) if i.op_str else 0)
            if d is not None:
                equations.append((d[0], tuple(sorted(d[1])), ()))
            continue
        npp = 0 if (mn in ("call", "pop", "popfd", "popal", "leave") or (ops and ops[0].type == X.X86_OP_REG and ops[0].reg == X.X86_REG_ESP)) else pp
        if mn in ("push", "pushfd"):
            nd = dadd(d, 4)
            npp = pp + 1
        elif mn in ("pop", "popfd"):
            nd = dadd(d, -4)
        elif mn == "pushal":
            nd = dadd(d, 32)
        elif mn == "popal":
            nd = dadd(d, -32)
        elif mn in ("sub", "add") and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[0].reg == X.X86_REG_ESP:
            nd = dadd(d, ops[1].imm if mn == "sub" else -ops[1].imm) if ops[1].type == X.X86_OP_IMM else None
        elif mn == "mov" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG \
                and ops[0].reg == X.X86_REG_EBP and ops[1].reg == X.X86_REG_ESP:
            neb = d[0] if (d is not None and not d[1]) else None
        elif mn == "mov" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG \
                and ops[0].reg == X.X86_REG_ESP and ops[1].reg == X.X86_REG_EBP:
            nd = (eb, ()) if eb is not None else None
        elif mn == "lea" and len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[0].reg == X.X86_REG_ESP:
            m = ops[1].mem
            if m.base == X.X86_REG_EBP and m.index == 0 and eb is not None:
                nd = (eb - m.disp, ())
            elif m.base == X.X86_REG_ESP and m.index == 0 and d is not None:
                nd = dadd(d, -m.disp)
            else:
                nd = None
        elif mn == "leave":
            nd = (eb - 4, ()) if eb is not None else None
        elif ops and ops[0].type == X.X86_OP_REG and ops[0].reg == X.X86_REG_ESP and mn not in ("cmp", "test"):
            nd = None           # and esp, -16 / mov esp, [..] / anything else: the depth is lost (an ebp frame still resolves reads)
        if mn == "call":
            popped = None
            if ops and ops[0].type == X.X86_OP_IMM:
                rc = ret_counts(ops[0].imm)
                if rc and len(rc) == 1:
                    popped = next(iter(rc))
            if popped is None:
                nx = ins.get(nxt)
                if nx is not None and nx.mnemonic == "add" and nx.operands and nx.operands[0].type == X.X86_OP_REG \
                        and nx.operands[0].reg == X.X86_REG_ESP:
                    popped = 0      # the caller cleans (cdecl)
            if nd is not None:
                if popped is not None:
                    nd = dadd(nd, -popped)
                else:
                    nd = (nd[0], tuple(sorted(set(nd[1]) | {a})))      # an unknown pop at this call site: u[a] >= 0 bytes, solved at the rets
                    bound[a] = 4 * pp
                    # the site's u is subtracted from the depth: depth = constant - sum(u)
            wr2 |= {"eax", "ecx", "edx"}
            work.append((nxt, nd, neb, frozenset(wr2), 0))
            continue
        if mn == "jmp":
            if ops and ops[0].type == X.X86_OP_IMM:
                tgt = ops[0].imm
                if va <= tgt < end:
                    work.append((tgt, nd, neb, frozenset(wr2), npp))
                else:
                    tail = True
                    sub = analyze(tgt)
                    if sub is not None and d is not None and not d[1]:
                        for k in sub["reads"]:
                            if k - d[0] // 4 >= 0:
                                reads.add(k - d[0] // 4)
                        if sub["ecx"] and "ecx" not in wr2:
                            ecx_read = True
                    else:
                        unknown = True
            else:
                tail = True
                unknown = True
            continue
        if mn.startswith("j") and ops and ops[0].type == X.X86_OP_IMM:
            work.append((ops[0].imm, nd, neb, frozenset(wr2), npp))
            work.append((nxt, nd, neb, frozenset(wr2), npp))
            continue
        if mn in ("int3", "ud2", "hlt"):
            continue
        work.append((nxt, nd, neb, frozenset(wr2), npp))
    # resolve the unknown pops from the rets (depth 0 there) and the joins (equal depths)
    if pending or any(ps or ns for _, ps, ns in equations):
        sol = _solve(equations, bound)
        sites_all = {x for _, ps, ns in equations for x in tuple(ps) + tuple(ns)}
        if sites_all and not sol:
            lost = True
        for disp, d, is_lea, is_read in pending:
            if any(x not in sol for x in d[1]):
                lost = True
                continue
            rel = disp - d[0] + sum(sol[x] for x in d[1])
            if rel >= 4 and (rel - 4) // 4 < 32:
                k = (rel - 4) // 4
                if is_lea:
                    addr_taken.add(k)
                elif is_read:
                    reads.add(k)
    elif any(c != 0 for c, ps, ns in equations):
        lost = True         # the depth drifted: a ret not at depth 0, or a join of unequal depths
    res = dict(reads=reads, addr_taken=addr_taken, ecx=ecx_read, regs=regs_read, tail=tail, rets=rets, unknown=unknown, lost=lost,
               maxarg=max(reads | addr_taken) if (reads | addr_taken) else -1)
    _info_cache[va] = res
    return res

# ---- the RX_PORT adapters: how each registered function is called -----------------------------------------------------------------------
def split_args(s):
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
    if cur.strip():
        out.append(cur.strip())
    return out

def balanced(text, start):
    depth = 0
    for k in range(start, len(text)):
        if text[k] == "(":
            depth += 1
        elif text[k] == ")":
            depth -= 1
            if depth == 0:
                return k + 1
    return len(text)

def strip_c(s):
    s = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)

def classify(a):
    """one adapter call argument: 'E' (ECX = m.r[1]), ('S', n) for m.arg(n), 'R<k>' another register, '?' anything else"""
    m = re.search(r"m\.arg\(\s*(\d+)\s*\)", a)
    r = re.search(r"m\.r\[\s*(\d+)\s*\]", a)
    if m and not r:
        return ("S", int(m.group(1)))
    if r:
        return "E" if r.group(1) == "1" else "R" + r.group(1)
    return "?"

adapters = {}     # addr -> [classified call arguments] of the registered function inside its RX_PORT body
for f in sorted(glob.glob(os.path.join(R, "core/oracle/*.cpp"))):
    text = strip_c(open(f, encoding="utf-8", errors="replace").read())
    for m in re.finditer(r"\b(?:RX_PORT_RAW|RX_PORT|ANIM_PORT|FX_PORT|RX_CRT_PORT|FINISH_PORT)\(\s*0x([0-9a-fA-F]+)\s*,", text):
        addr = int(m.group(1), 16)
        if addr not in reg:
            continue
        k = text.find("{", m.end())
        depth, j = 0, k
        while j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        body = text[k:j]
        c = re.search(r"\b" + re.escape(reg[addr][0]) + r"\s*\(", body)
        if not c:
            continue
        s = c.end() - 1
        adapters[addr] = [classify(a) for a in split_args(body[s + 1: balanced(body, s) - 1])]

def adapter_order_problem(addr, thiscall):
    a = adapters.get(addr)
    if a is None:
        return None
    expect = (["E"] if thiscall else []) + [("S", n) for n in range(len(a))]
    for idx, (g, e) in enumerate(zip(a, expect)):
        if g != "?" and g != e:
            return "param %d is %s, positional order wants %s" % (idx, g, e)
    return None

def adapter_uses_other_registers(addr):
    return any(isinstance(x, str) and x.startswith("R") for x in (adapters.get(addr) or []))

# ---- --dump ---------------------------------------------------------------------------------------------------------------------------------
dumps = [x for x in sys.argv[1:] if x.startswith("--dump=")]
for a in dumps:
    for tok in a[7:].split(","):
        va = int(tok, 16)
        info = analyze(va)
        nm = reg.get(va)
        print("%08x %s C(%s) vtable=%s taken=%d literal=%s rets=%s ecx=%s regs=%s reads=%s addr_taken=%s tail=%s unknown=%s adapter=%s" % (
            va, nm[0] if nm else "-", nm[2] if nm else "-", [x[0] + ":" + str(x[1]) for x in vtable_target.get(va, [])][:3] or "-",
            taken.get(va, 0), sorted(core_literal.get(va, [])), sorted(info["rets"]) if info else None, info and info["ecx"],
            info and sorted(info["regs"]), info and sorted(info["reads"]), info and sorted(info["addr_taken"]), info and info["tail"],
            info and info["unknown"], adapters.get(va)))
if dumps:
    sys.exit(0)

# ---- --coverage: how many of the pointer-reachable functions that take stack arguments have an RX_PORT adapter whose order was checked ----
if "--coverage" in FLAGS:
    n = ck = 0
    for va in sorted(reg):
        if not (va in vtable_target or taken.get(va, 0) or va in core_literal):
            continue
        info = analyze(va)
        if not info or info["maxarg"] < 0 or not gen_fnsig.int_only(reg[va][1], reg[va][2], "")[0]:
            continue
        n += 1
        if va in adapters:
            ck += 1
        elif "--missing" in FLAGS:
            print("%08x %s %s" % (va, reg[va][0], reg[va][3]))
    print("%d pointer-reachable integer-signature functions read stack arguments; %d have a parsed adapter" % (n, ck), file=sys.stderr)
    sys.exit(0)

# ---- --thunks: registered vtable targets with a float / pointer signature in a slot an event-proxy thunk (build_vcall_thunks.c) can reach ----
if "--thunks" in FLAGS:
    offs = [int(x, 16) for x in re.findall(r"VCALL_THUNK_BODY\(0x([0-9a-f]+)\)", open(os.path.join(R, "core/game/build_vcall_thunks.c")).read())]
    slots = {o // 4 for o in offs}
    for va in sorted(reg):
        if va not in vtable_target or gen_fnsig.int_only(reg[va][1], reg[va][2], "")[0]:
            continue
        hit = sorted({"%s:%d" % (c, sl) for c, sl in vtable_target[va] if sl in slots})
        if hit:
            print("%08x\t%s\t%s\t(%s)\t%s" % (va, reg[va][0], reg[va][3], reg[va][2], ", ".join(hit[:3])))
    sys.exit(0)

# ---- --table: every registered function the old RET-count heuristic would list (C parameters < RET n / 4 + ECX), with what the walk found ----
if "--table" in FLAGS:
    print("addr\tname\tfile\tC params\tRET/4\tECX read\treach\tstack args read\tverdict")
    for va in sorted(reg):
        name, ret, params, f = reg[va]
        info = analyze(va)
        if not info or len(info["rets"]) != 1:
            continue
        st = next(iter(info["rets"])) // 4
        pc = len(c_params(params))
        if pc >= st + (1 if info["ecx"] else 0):
            continue
        reach = "vtable" if va in vtable_target else ("pointer" if (taken.get(va, 0) or va in core_literal) else "direct only")
        used = sorted(info["reads"] | info["addr_taken"])
        if reach == "direct only":
            verdict = "direct callers only (the owner's prototype)"
        elif info["regs"] and reach != "vtable":
            verdict = "register-argument function"
        elif not used:
            verdict = "harmless: no stack argument is read"
        else:
            verdict = "stack args %s read; C takes %d" % (used, pc) + (" -> OK" if pc >= (1 if info["ecx"] else 0) + max(used) + 1 else " -> CHECK")
        print("%08x\t%s\t%s\t%d\t%d\t%s\t%s\t%s\t%s" % (va, name, f, pc, st, "yes" if info["ecx"] else "no", reach, used or "-", verdict))
    sys.exit(0)

# ---- the report -------------------------------------------------------------------------------------------------------------------------------
rows = []
typed_skipped = []
for va in sorted(reg):
    name, ret, params, f = reg[va]
    virt = va in vtable_target
    if not (virt or taken.get(va, 0) > 0 or va in core_literal):
        continue
    info = analyze(va)
    if info is None:
        continue
    if not virt and (info["regs"] or adapter_uses_other_registers(va)):
        continue            # an LTCG register-argument function: not on the positional path
    if not gen_fnsig.int_only(ret, params, "")[0]:
        typed_skipped.append(va)
        if "--typed" not in FLAGS:
            continue            # a float / pointer parameter or result: rx_vcall and the integer table cannot call it (typed call sites only)
    ad = adapters.get(va) or []
    thiscall = virt or info["ecx"] or (bool(ad) and ad[0] == "E")
    maxarg = info["maxarg"]
    unresolved = ""
    if info["lost"] and len(info["rets"]) == 1:
        # the stack depth was lost on some path (a callee that pops an unknown amount, and no ebp frame): a later read may be missed, so
        # assume every stack argument the RET pops may be read
        if next(iter(info["rets"])) // 4 - 1 > maxarg:
            maxarg = next(iter(info["rets"])) // 4 - 1
            unresolved = " [stack depth lost: all RET arguments assumed read]"
    # a virtual target that neither reads this nor any stack argument takes whatever the caller passes
    needed = 0 if (maxarg < 0 and not info["ecx"]) else (1 if thiscall else 0) + maxarg + 1
    pc = len(c_params(params))
    problems = []
    if pc < needed:
        problems.append("C takes %d, positional needs %d (%sstack arg 0..%d read)%s" % (pc, needed, "this + " if thiscall else "", maxarg, unresolved))
    if len(info["rets"]) == 1 and not info["tail"]:
        supplied = (1 if thiscall else 0) + max(next(iter(info["rets"])) // 4, maxarg + 1)   # RET 0 is cdecl: the reads show the count
        if pc > supplied:
            problems.append("C takes %d but a positional caller supplies %d (%d extra parameter%s with no source)" %
                            (pc, supplied, pc - supplied, "" if pc - supplied == 1 else "s"))
    # the C body uses a parameter whose positional source (stack argument j - 1) the original never reads: the caller's value for it is a different argument
    up = used_params(name)
    if up is not None and not info["lost"] and not info["tail"] and not info["unknown"]:
        used_src = set(info["reads"] | info["addr_taken"])
        top = max(next(iter(info["rets"])) // 4 if info["rets"] else 0, maxarg + 1) + 1
        for k in info["addr_taken"]:
            used_src |= set(range(k, top))      # an address of argument k can reach the dwords after it (a tag, a vector, a struct by value)
        for j in sorted(up):
            src = j - (1 if thiscall else 0)
            if src >= 0 and src not in used_src and (not info["rets"] or src < max(next(iter(info["rets"])) // 4, maxarg + 1)):
                problems.append("C uses parameter %d (stack argument %d) but the original never reads that argument" % (j, src))
    order = adapter_order_problem(va, thiscall)
    if order:
        problems.append("adapter: " + order)
    if problems:
        rows.append((va, name, f, pc, next(iter(info["rets"])) if len(info["rets"]) == 1 else None, maxarg, info["ecx"],
                     "vtable" if virt else "ptr", info, problems))

# literal call sites: the convention and argument count the product's positional call uses must fit the original
for a, conv, n, fl, ln, wrapper in literal_calls:
    info = analyze(a)
    if info is None:
        continue
    name, ret, params, f = reg[a]
    ad = adapters.get(a) or []
    this_first = (ad[0] == "E") if ad else info["ecx"]
    if info["regs"] and not ad:
        continue
    bad = []
    if (conv == "THISCALL") != this_first and not (conv == "THISCALL" and not info["ecx"] and not ad):
        bad.append("passes %s but %s" % (conv, "the original is a thiscall (ECX is the first C parameter)" if this_first else "the original is not a thiscall (C parameter 0 is stack argument 0)"))
    if info["rets"] and len(info["rets"]) == 1 and conv != "CDECL" and n != next(iter(info["rets"])) // 4:
        bad.append("passes %d argument(s) but the original pops %d" % (n, next(iter(info["rets"])) // 4))
    if bad:
        rows.append((a, name, f, len(c_params(params)), None, info["maxarg"], info["ecx"], "call@%s:%d" % (fl, ln), info, ["%s(%08x, RX_%s, .., %d, ..) %s" % (wrapper, a, conv, n, "; ".join(bad))]))

import sigcheck_vt
_vt_rows = list(csv.DictReader(open(os.path.join(R, "_re/out/vtables.tsv"), encoding="latin-1"), delimiter="\t"))
vt_findings, vt_allowed, vt_stale, vt_sum, vt_unres = sigcheck_vt.run(R, reg, _vt_rows, product_side, ALLOW_VT, FLAGS)

# The ECX pass (w-pose446): a virtual call through world_call_ctx (WORLD_CTX / WORLD_CTX0 / WORLD_CTX_F, game/world_util.h) runs the
# registered port only when the context's ECX is set (the receiver); with an inline &WCTX(...) that names no .ecx the product takes the
# original-only path and stops ("unported game function <the slot's target>") although the target is ported. First hit: anim_rider_play.c
# at 00620d78 (simulation vf33 00446cd0 called with `.eax = sim` and `sim` pushed), the FMX Free Ride abort of 2026-11-01. Findings.
_ecx_bad = []
_slot_read = re.compile(r"^(U32\(U32\(.+,0u?\),(0x[0-9a-fA-F]+|\d+)u?\)|race_vfn\(.+\)|loop_vslot\(.+\))$")
for _p in sorted(glob.glob(os.path.join(R, "core/game/*.[ch]")) + glob.glob(os.path.join(R, "core/api/*.[ch]")) + glob.glob(os.path.join(R, "core/api/*.inc"))):
    _t = sigcheck_vt.blank_comments(open(_p, encoding="utf-8", errors="replace").read())
    for _m in re.finditer(r"\bWORLD_CTX(0|_F)?\s*\(", _t):
        if _t[_t.rfind("\n", 0, _m.start()) + 1:_m.start()].lstrip().startswith("#"):
            continue                                       # the macros' own #define lines
        _a = sigcheck_vt.split_top(_t[_m.end():sigcheck_vt.match_close(_t, _m.end() - 1) - 1])
        if len(_a) < 2:
            continue
        _fn, _ctx = re.sub(r"\s+", "", _a[0]), re.sub(r"\s+", "", _a[1])
        if _slot_read.match(_fn) and _ctx.startswith("&WCTX(") and ".ecx=" not in _ctx:
            _ecx_bad.append("%s:%d\t%s\t%s" % (os.path.relpath(_p, R), _t.count("\n", 0, _m.start()) + 1, _a[0].strip(), _a[1].strip()))
for _e in _ecx_bad:
    print("ecx\t%s\ta virtual call through world_call_ctx without .ecx: the product stops at the slot's target even when it is ported" % _e)
print("ECX pass: %d virtual calls through world_call_ctx (WORLD_CTX*) with an inline context that names no .ecx" % len(_ecx_bad), file=sys.stderr)

real = [r for r in rows if r[0] not in ALLOW]
allowed = [r for r in rows if r[0] in ALLOW]
stale = sorted(a for a in ALLOW if a not in {r[0] for r in rows})

def show(r, note=""):
    va, name, f, pc, ret, maxarg, ecx, cls, info, problems = r
    print("%08x\t%s\t%s\t%s\tC params %d\tRET %s%s\tmax stack arg read %d%s%s\t%s%s" % (
        va, name, f, cls, pc, "?" if ret is None else ret, " + ECX" if ecx else "", maxarg,
        " (address taken: %s)" % sorted(info["addr_taken"]) if info["addr_taken"] else "", " tail-jmp" if info["tail"] else "",
        "; ".join(problems), note))

for r in real:
    show(r)
for it in vt_findings:
    print(sigcheck_vt.show_item(it, reg))
if "--all" in FLAGS:
    for r in allowed:
        show(r, "\tALLOW: " + ALLOW[r[0]])
    for it in vt_allowed:
        print(sigcheck_vt.show_item(it, reg, "\tALLOW: " + ALLOW_VT[it[9]]))
if "--vt-weak" in FLAGS:
    for w in vt_sum["weak"]:
        print("vtweak\t%s:%d\t%s\tslot %s\t(%s)\t%s" % (w[0], w[1], w[2], w[3], w[4], "; ".join("%s [%s]" % (m, ",".join(l)) for m, l in w[5].items())))
if "--vt-hetero" in FLAGS:
    for h in vt_sum["hetero"]:
        print(sigcheck_vt.show_hetero(h, reg))
_dump = [a.split("=", 1)[1] for a in FLAGS if a.startswith("--vt-dump=")]
if _dump:      # --vt-dump=<file.json>: the static data core/tools/vtrace_check.py compares the RX_FN_TRACE runtime log against
    import json
    with open(_dump[0], "w") as _jf:
        json.dump(vt_sum["export"], _jf)
if "--vt-unresolved" in FLAGS:
    for u in vt_unres:
        print("vtunresolved\t%s:%d\t%s" % u)
_rc = vt_sum["reach"]
if "--vt-reach" in FLAGS:
    for _t, _e in _rc["found"].items():
        print("vtreach\t%08x\t%s\t%s\t%s\t%s\t%d sites: %s" % (_t, _e["name"], ",".join(sorted(_e["classes"]))[:120], "abort" if _e["abort"] else "silent",
              "/".join(sorted(_e["how"])), len(_e["sites"]), " | ".join(sorted(_e["sites"])[:4])))
if "--vt-reach" in FLAGS:
    for _t in sorted(_rc["open"]):
        print("vtreachopen\t%08x\t%s\t%s" % (_t, ",".join(sorted(_rc["open"][_t])), _rc["open_why"].get("%08x" % _t, "")))
if "--vt-reach-sites" in FLAGS:
    for _k, _e in _rc["bysite"].items():
        print("vtreachsite\t%s\t%s\trecv=%s\t%s\t%d: %s" % (_k, _e["how"], _e["recv"][:50], "abort" if _e["abort"] else "silent", len(_e["targets"]),
              " ".join("%08x" % t for t in sorted(_e["targets"]))))
ptr_total = sum(1 for va in reg if va in vtable_target or taken.get(va, 0) > 0 or va in core_literal)
print("%d registered functions reachable through a pointer (%d vtable targets; %d with a float / pointer signature are typed-call only): "
      "%d literal call sites of registered functions checked: %d findings, %d allow-listed" %
      (ptr_total, sum(1 for va in reg if va in vtable_target), len(typed_skipped), len(literal_calls), len(real) + len(vt_findings),
       len(allowed) + len(vt_allowed)), file=sys.stderr)
_ck = vt_sum["checked"]
print("%d typed address conversions (RX_FN / rx_fn casts / rx_vcall) in core/game + core/api, %d call sites after the wrapper expansion: %d vtable-slot "
      "calls resolved to a slot, %d of them compared with the ports of the narrowed classes (%d skipped: no class narrowed; %d of the compared fit none "
      "of the narrowed ports: weak, --vt-weak), "
      "%d literal-address calls, %d not resolved: %d findings, %d allow-listed (included in the count above)" %
      (vt_sum["conversions"], vt_sum["expansions"], _ck["checked (vtable slot)"], _ck["checked (vtable slot)"] - _ck["unnarrowed"], _ck["unnarrowed"], _ck["weak: no narrowed port fits"],
       _ck["checked (literal address)"], vt_sum["unresolved"], len(vt_findings), len(vt_allowed)), file=sys.stderr)
print("of the %d generic-receiver slot calls: %d homogeneous (the call fits every registered port at the slot: safe whatever the receiver is), %d checked "
      "by a receiver role (core/tools/sigcheck_vt_roles.tsv, %d rules), %d heterogeneous and not narrowed (--vt-hetero lists them; reviewed by hand, see "
      "core/plan/QUEUE.md w-sigvt2)" % (_ck["unnarrowed"] + _ck["checked by role"], _ck["homogeneous"], _ck["checked by role"], len(vt_sum["roles"]),
                                         len(vt_sum["hetero"])), file=sys.stderr)
_rc_stop = [_t for _t, _e in _rc["found"].items() if _e["abort"]]
print("vtable reach (sigcheck_vt.reach_pass): %d constructed classes' vtables, %d slot / literal call sites: %d UNREGISTERED reachable targets behind a "
      "fatal fallback (rx_call_original: the product stops), %d more behind a silent one (if (f) f(...): skipped, not a stop); --vt-reach lists them; "
      "%d allow-listed (core/tools/vtreach_allow.tsv, %d entries)" % (
          _rc["constructed"], _rc["sites"], len(_rc_stop), len(_rc["found"]) - len(_rc_stop), len(_rc["allowed"]), _rc["allow_n"]), file=sys.stderr)
if _rc["open"]:
    print("vtable reach: %d OPEN targets (reachable or not disproven, not ported yet; vtreach_allow.tsv `open:` lines, --vt-reach lists them)" % len(_rc["open"]), file=sys.stderr)
for a in _rc["stale"]:
    print("vtreach allow-list entry %s matches nothing any more (remove it)" % a, file=sys.stderr)
for r_ in vt_sum["roles"]:
    if not r_["used"]:
        print("role rule never used (remove it?): %s %s %s %s %s" % (r_["file"].pattern, r_["func"].pattern, r_["recv"].pattern, r_["slot"].pattern, r_["mode"]), file=sys.stderr)
for a in vt_stale:
    print("allow-list entry %s does not match a finding any more (remove it)" % a, file=sys.stderr)
for a in stale:
    print("allow-list entry %08x does not match a finding any more (remove it)" % a, file=sys.stderr)
sys.exit(1 if (real or vt_findings or _rc_stop or _ecx_bad) else 0)
