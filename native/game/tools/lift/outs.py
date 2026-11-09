#!/usr/bin/env python3
"""outs2.py <list.tsv|addr,...> [out.tsv]: the output registers callers read after a call of each function (NATIVE_AGENTS.md Outputs rule),
as port.h Outputs bits (EAX 1, ECX 2, EDX 4, xmm i 0x100 << i, x87 values 0x10000).

Whole-program register dataflow on the original code (fixpoints, so recursion is exact):
  must_def(F)   registers F writes on every path to its ret (greatest fixpoint); a call to F kills only these (LTCG callers keep values
                in volatile registers across calls to known code); indirect calls and imports kill every volatile register (ABI).
  entry_live(F) registers F reads before writing them (register arguments; least fixpoint).
  live_out(F)   union over F's call sites of the registers live at the return address (backward liveness of the caller, whose ret uses
                live_out(caller)); tail jumps to F count as the jumper's live_out. Address-taken functions (vtable slots, callbacks: by the
                relocations) keep the standard convention, so at most EAX / EDX / st0; for a vtable slot k the indirect calls
                [reg + k] anywhere add their sites (masked to EAX / EDX / st0)."""
import os, sys, struct, pickle
import capstone
from capstone import x86 as X86
import pefile

S = os.path.dirname(os.path.abspath(__file__))
R = "/Users/payton/Documents/Development/REFLEX/_re"
pe = pefile.PE(os.path.join(R, "..", "game files", "Archive", "MXReflex.exe"), fast_load=False)
img = pe.get_memory_mapped_image(); B = pe.OPTIONAL_HEADER.ImageBase
TEXT = (0x401000, 0x401000 + 0x514a5a); RDATA = (0x916000, 0x916000 + 0x8f8fb)
FUNCS = {}
for l in open(os.path.join(R, "out", "functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.split("\t")
    try: FUNCS[int(c[0], 16)] = (c[1], int(c[3]))
    except ValueError: pass
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
def rd32(a): return struct.unpack_from("<I", img, a - B)[0]

EAX, ECX, EDX, ST = 1, 2, 4, 0x10000
XMM = lambda k: 0x100 << k
VOL = EAX | ECX | EDX | 0xff00 | ST
ABI = EAX | EDX | ST
G32 = {"eax": EAX, "ax": EAX, "al": EAX, "ah": EAX, "ecx": ECX, "cx": ECX, "cl": ECX, "ch": ECX, "edx": EDX, "dx": EDX, "dl": EDX, "dh": EDX}
FULL32 = {"eax": EAX, "ecx": ECX, "edx": EDX}
def rbit(name):
    if name in G32: return G32[name]
    if name.startswith("xmm"): return XMM(int(name[3:]))
    return 0
X87_PUSH = {"fld1", "fldz", "fldpi", "fldl2e", "fldl2t", "fldlg2", "fldln2", "fild"}
X87_NONE = {"fnstcw", "fldcw", "fnstsw", "fwait", "wait", "fnclex", "fninit", "fstcw", "fstsw"}
XFULL = {"movaps", "movups", "movapd", "movupd", "movdqa", "movdqu", "movq", "movd", "cvtdq2ps", "cvttps2dq", "cvtps2dq", "cvtps2pd",
         "cvtpd2ps", "pshufd", "sqrtps", "rcpps", "rsqrtps", "cvttpd2dq", "cvtpd2dq", "cvtdq2pd"}
ZERO_IDIOM = {"xor", "sub", "pxor", "xorps", "xorpd", "pcmpeqd"}
JCC = {"jo", "jno", "jb", "jae", "je", "jne", "jbe", "ja", "js", "jns", "jp", "jnp", "jl", "jge", "jle", "jg", "jecxz"}

def use_def(i):
    mn = i.mnemonic; o = i.operands
    try: rr, ww = i.regs_access()
    except capstone.CsError: return VOL, 0
    if mn.startswith("f"):
        if mn in X87_NONE or mn in X87_PUSH: u = 0
        elif mn == "fld": u = ST if (o and o[0].type == X86.X86_OP_REG) else 0
        else: u = ST
        for r in rr: u |= rbit(i.reg_name(r)) & (EAX | ECX | EDX)
        return u, 0
    if len(o) == 2 and mn in ZERO_IDIOM and o[0].type == X86.X86_OP_REG and o[1].type == X86.X86_OP_REG and o[0].reg == o[1].reg:
        nm = i.reg_name(o[0].reg)
        return 0, FULL32.get(nm, 0) | (rbit(nm) if nm.startswith("xmm") else 0)
    u = 0; d = 0
    for r in rr: u |= rbit(i.reg_name(r))
    for r in ww:
        nm = i.reg_name(r)
        if nm in FULL32: d |= FULL32[nm]
        elif nm.startswith("xmm"):
            b = rbit(nm)
            if mn in XFULL or (mn in ("movss", "movsd") and len(o) == 2 and o[1].type == X86.X86_OP_MEM): d |= b
            else: u |= b
    return u, d

def is_import_thunk_bytes(t):
    return img[t - B] == 0xff and img[t - B + 1] == 0x25

# ------------------------------------------------------------------ per-function CFGs as compact tuples
# node: (addr, kind, use, def, target, succs); kind: 0 plain, 1 direct call (target), 2 indirect call / import, 3 ret,
# 4 tail jump to function target, 5 tail jump to import, 6 unknown indirect jump
def build(f):
    nodes = {}; work = [f]; n = FUNCS[f][1]; f1 = f + max(n, 0) + 0x400
    while work:
        a = work.pop()
        while a not in nodes:
            try: i = next(md.disasm(bytes(img[a - B:a - B + 16]), a))
            except StopIteration: nodes[a] = (3, 0, 0, None, ()); break
            mn = i.mnemonic; nxt = a + i.size; o = i.operands
            if mn in ("ret", "int3", "hlt", "ud2"): nodes[a] = (3 if mn == "ret" else 7, 0, 0, None, ()); break
            if mn == "call":
                op = o[0]
                if op.type == X86.X86_OP_IMM and op.imm in FUNCS and not is_import_thunk_bytes(op.imm):
                    nodes[a] = (1, 0, 0, op.imm, (nxt,))
                else:
                    u, _ = use_def(i)
                    nodes[a] = (2, u | ECX | (EDX if op.type == X86.X86_OP_REG else 0), 0, None, (nxt,))
                a = nxt; continue
            if mn == "jmp":
                op = o[0]
                if op.type == X86.X86_OP_IMM:
                    t = op.imm
                    if t != f and t in FUNCS:
                        nodes[a] = ((5 if is_import_thunk_bytes(t) else 4), 0, 0, t, ()); break
                    nodes[a] = (0, 0, 0, None, (t,)); work.append(t); break
                if op.type == X86.X86_OP_MEM and op.mem.base == 0 and op.mem.index != 0 and op.mem.scale == 4:
                    tb = op.mem.disp & 0xffffffff; ents = []
                    for k in range(512):
                        try: v = rd32(tb + 4 * k)
                        except struct.error: break
                        if not (f <= v < f1): break
                        ents.append(v)
                    u, _ = use_def(i)
                    nodes[a] = (0, u, 0, None, tuple(set(ents))); work.extend(set(ents)); break
                if op.type == X86.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0:
                    nodes[a] = (5, 0, 0, None, ()); break   # jmp [IAT]
                nodes[a] = (6, 0, 0, None, ()); break
            if mn in JCC:
                t = o[0].imm
                u, d = use_def(i)
                if t != f and t in FUNCS:   # conditional tail jump
                    nodes[a] = (8, u, 0, t, (nxt,)); a = nxt; continue
                nodes[a] = (0, u, 0, None, (t, nxt)); work.append(t); a = nxt; continue
            u, d = use_def(i)
            nodes[a] = (0, u, d, None, (nxt,)); a = nxt
    return nodes

CACHE = os.path.join(S, "outs2_cfg.pkl")
if os.path.exists(CACHE):
    CFG = pickle.load(open(CACHE, "rb"))
else:
    CFG = {}
    for k, f in enumerate(sorted(FUNCS)):
        if not (TEXT[0] <= f < TEXT[1]) or FUNCS[f][1] <= 0: continue
        if is_import_thunk_bytes(f): continue
        try: CFG[f] = build(f)
        except Exception as e: sys.stderr.write(f"cfg {f:08x}: {e}\n")
        if k % 2000 == 0: sys.stderr.write(f"cfg {k}/{len(FUNCS)}\n")
    pickle.dump(CFG, open(CACHE, "wb"))

def liveness(f, ret_use, entry, must, lout=None):
    nodes = CFG[f]; order = sorted(nodes, reverse=True); LI = dict.fromkeys(nodes, 0)
    changed = True
    while changed:
        changed = False
        for a in order:
            kind, u, d, t, sc = nodes[a]
            lo = 0
            for s in sc: lo |= LI.get(s, 0)
            if kind == 0: v = u | (lo & ~d)
            elif kind == 1: v = entry.get(t, 0) | (lo & ~must.get(t, 0))
            elif kind == 2: v = u
            elif kind == 3: v = ret_use
            elif kind == 4: v = entry.get(t, 0) | (ret_use & ~must.get(t, 0))
            elif kind == 5: v = 0
            elif kind == 6: v = VOL
            elif kind == 8: v = u | lo | entry.get(t, 0) | (ret_use & ~must.get(t, 0))
            else: v = 0
            if v != LI[a]: LI[a] = v; changed = True
    return LI

def must_pass(f, must):
    nodes = CFG[f]; order = sorted(nodes); IN = dict.fromkeys(nodes, VOL); IN[f] = 0; res = None
    changed = True
    while changed:
        changed = False
        for a in order:
            kind, u, d, t, sc = nodes[a]; s = IN[a]
            if kind == 0: out = s | d
            elif kind == 1: out = s | must.get(t, 0)
            elif kind == 2: out = s | VOL
            elif kind == 3: res = s if res is None else res & s; continue
            elif kind == 4: v = s | must.get(t, 0); res = v if res is None else res & v; continue
            elif kind in (5, 6): v = s | VOL; res = v if res is None else res & v; continue
            elif kind == 8:
                v = s | must.get(t, 0); res = v if res is None else res & v; out = s
            else: continue
            for n in sc:
                if n in IN and (IN[n] & out) != IN[n]: IN[n] &= out; changed = True
    return VOL if res is None else res

GLOB = os.path.join(S, "outs2_glob.pkl")
if os.path.exists(GLOB):
    MUST, ENTRY = pickle.load(open(GLOB, "rb"))
else:
    MUST = {f: VOL for f in CFG}
    for it in range(30):
        ch = 0
        for f in CFG:
            v = must_pass(f, MUST)
            if v != MUST[f]: MUST[f] = v; ch += 1
        sys.stderr.write(f"must pass {it}: {ch} changed\n")
        if not ch: break
    ENTRY = {f: 0 for f in CFG}
    for it in range(30):
        ch = 0
        for f in CFG:
            v = liveness(f, 0, ENTRY, MUST).get(f, 0)
            if v != ENTRY[f]: ENTRY[f] = v; ch += 1
        sys.stderr.write(f"entry pass {it}: {ch} changed\n")
        if not ch: break
    pickle.dump((MUST, ENTRY), open(GLOB, "wb"))

# ------------------------------------------------------------------ call sites
direct = {}; vslot = {}
for g, nodes in CFG.items():
    for a, (kind, u, d, t, sc) in nodes.items():
        if kind == 1: direct.setdefault(t, []).append((g, sc[0], "call"))
        elif kind in (4, 8): direct.setdefault(t, []).append((g, None, "tail"))
SITE_SCAN = os.path.join(S, "outs2_vslot.pkl")
if os.path.exists(SITE_SCAN):
    vslot = pickle.load(open(SITE_SCAN, "rb"))
else:
    md2 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md2.detail = True
    for g, nodes in CFG.items():
        for a, (kind, u, d, t, sc) in nodes.items():
            if kind != 2: continue
            i = next(md2.disasm(bytes(img[a - B:a - B + 16]), a)); op = i.operands[0]
            if op.type == X86.X86_OP_MEM and op.mem.base != 0 and op.mem.index == 0:
                vslot.setdefault(op.mem.disp & 0xffffffff, []).append((g, sc[0]))
            elif op.type == X86.X86_OP_REG:
                # mov reg, [base + disp] in the preceding instructions (same block, linear)
                k = a - 1; found = None
                for back in range(2, 40):
                    try: p = next(md2.disasm(bytes(img[a - back - B:a - B]), a - back))
                    except StopIteration: continue
                    if p.address + p.size != a: continue
                    po = p.operands
                    if p.mnemonic == "mov" and len(po) == 2 and po[0].type == X86.X86_OP_REG and po[0].reg == op.reg and po[1].type == X86.X86_OP_MEM \
                            and po[1].mem.base != 0 and po[1].mem.index == 0:
                        found = po[1].mem.disp & 0xffffffff
                    break
                vslot.setdefault(found if found is not None else "unknown", []).append((g, sc[0]))
    pickle.dump(vslot, open(SITE_SCAN, "wb"))

relocs = {}
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_BASERELOC"]])
for blk in getattr(pe, "DIRECTORY_ENTRY_BASERELOC", []):
    for e in blk.entries:
        if e.type == 3: relocs.setdefault(rd32(e.rva + B), []).append(e.rva + B)

def vtable_slot(a):
    s = a
    while True:
        p = rd32(s - 4)
        if TEXT[0] <= p < TEXT[1]: s -= 4; continue
        if RDATA[0] <= p < RDATA[1]: return a - s
        return None

def outputs(targets, depth=10):
    """least fixpoint of live_out over the targets and their direct callers (depth levels); beyond: VOL (ABI if address-taken)"""
    D = set(); frontier = set(targets)
    for lvl in range(depth):
        nxt = set()
        for f in frontier:
            if f in D: continue
            D.add(f)
            for (g, ra, kind) in direct.get(f, []): nxt.add(g)
        frontier = nxt
    abi = {f: (ABI if relocs.get(f) else VOL) for f in D}
    LO = {f: 0 for f in D}
    def lo_of(g): return LO[g] if g in LO else (ABI if relocs.get(g) else VOL)
    vs_cache = {}
    def vslot_part(f):
        res = 0
        for a in relocs.get(f, []):
            if RDATA[0] <= a < RDATA[1]:
                k = vtable_slot(a)
                if k is None: return ABI
                if k in vs_cache: res |= vs_cache[k]; continue
                v = 0
                for (g, ra) in vslot.get(k, []):
                    if g not in CFG: v = ABI; break
                    v |= liveness(g, ABI if g not in LO else LO[g], ENTRY, MUST).get(ra, ABI) & ABI
                    if v == ABI: break
                vs_cache[k] = v; res |= v
            else: return ABI   # address taken in code / data: unknown callers
        return res
    VS = {f: vslot_part(f) for f in D}
    for it in range(50):
        ch = 0; LIC = {}
        for f in D:
            v = VS[f]
            for (g, ra, kind) in direct.get(f, []):
                if g not in CFG: v |= VOL; continue
                if kind == "tail": v |= lo_of(g)
                else:
                    if g not in LIC: LIC[g] = liveness(g, lo_of(g), ENTRY, MUST)
                    v |= LIC[g].get(ra, VOL)
            v &= abi[f]
            if v != LO[f]: LO[f] = v; ch += 1
        sys.stderr.write(f"live_out pass {it}: {ch} changed ({len(D)} functions)\n")
        if not ch: break
    return {f: LO[f] for f in targets}

def fmt(mask):
    parts = []
    if mask & EAX: parts.append("OUT_EAX")
    if mask & ECX: parts.append("OUT_ECX")
    if mask & EDX: parts.append("OUT_EDX")
    x = (mask >> 8) & 0xff
    if x == 0xff: parts.append("OUT_XMM_ALL")
    else:
        for k in range(8):
            if x & (1 << k): parts.append(f"(OUT_XMM0 << {k})" if k else "OUT_XMM0")
    if mask & ST: parts.append("OUT_FPU")
    return " | ".join(parts) if parts else "0"

if __name__ == "__main__":
    arg = sys.argv[1]
    if arg.endswith(".tsv"): addrs = [int(l.split("\t")[0], 16) for l in list(open(arg))[1:]]
    else: addrs = [int(x, 16) for x in arg.split(",")]
    res = outputs(addrs)
    out = open(sys.argv[2], "w") if len(sys.argv) > 2 else sys.stdout
    for a in addrs: out.write(f"{a:08x}\t{res[a]:05x}\t{fmt(res[a])}\n")
