#!/usr/bin/env python3
"""liveness.py <out.tsv> -- which registers callers can read after each function of MXReflex.exe returns (native/game Verify
compares only those, port.cpp REFLEX_PORT_LIVE / plan/live.tsv).

Rules:
- A function whose address is taken (any dword of the image equal to its entry: vtables, function pointer tables, pushes) or that
  has no direct caller follows the standard ABI: EAX, EDX and the x87 stack (MSVC only changes conventions of functions all of whose
  call sites it knows).
- Otherwise: the union over its direct call sites (call / jmp rel32) of the registers read on some path after the site before being
  written: a ret adds the enclosing function's own live set (pass-through returns); a following call adds that callee's register
  inputs (read before written from its entry) and ends the path; a tail jmp adds the target's inputs and the enclosing function's
  live set. Iterated to a fixed point.
Registers: eax ecx edx, xmm0..7, x87 (as one unit). Callee-saved registers are compared by Verify anyway.
Output: addr, mask (port.h Outputs bits: EAX 1, ECX 2, EDX 4, XMMi 1<<(8+i), FPU 1<<16), names."""
import bisect, os, struct, sys
import capstone, pefile
from capstone import x86 as X

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "_re")
pe = pefile.PE(os.path.join(R, "..", "game files", "Archive", "MXReflex.exe"), fast_load=True)
img = pe.get_memory_mapped_image(); B = pe.OPTIONAL_HEADER.ImageBase
text_lo, text_hi = B + 0x1000, B + 0x1000 + 0x514a5a
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True

funcs = []
for l in open(os.path.join(R, "out", "functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.split("\t")
    a = int(c[0], 16)
    if text_lo <= a < text_hi: funcs.append(a)
funcs.sort(); fset = set(funcs)
def owner(a):
    i = bisect.bisect_right(funcs, a) - 1
    return funcs[i] if i >= 0 else None

cache = {}
def insn(a):
    if a not in cache:
        if not (text_lo <= a < text_hi): cache[a] = None
        else:
            l = list(md.disasm(img[a - B:a - B + 16], a, 1)); cache[a] = l[0] if l else None
    return cache[a]

GPR = {"eax": 1, "ecx": 2, "edx": 4}
SUB = {"al": "eax", "ah": "eax", "ax": "eax", "cl": "ecx", "ch": "ecx", "cx": "ecx", "dl": "edx", "dh": "edx", "dx": "edx"}
def bit(name):
    name = SUB.get(name, name)
    if name in GPR: return GPR[name]
    if name.startswith("xmm"): return 1 << (8 + int(name[3:]))
    return 0
FPU = 1 << 16
ABI = 1 | 4 | FPU
ALL = 1 | 2 | 4 | (0xff << 8) | FPU
VOL = 1 | 2 | 4 | (0xff << 8)                       # caller-clobbered by the cdecl/stdcall ABI (EAX/ECX/EDX, xmm)
# Opt-in refinement (NATIVE_AGENTS.md: ui_tools/outs2.py's must-define analysis), off by default so the committed
# plan/live.tsv / live.inc are not silently changed. When on, a call to a known function kills only the registers that
# callee writes on every path (must_def): LTCG callers keep values in volatile registers across such calls, so a register
# read after the call is live-out of the caller. This only ever ADDS live bits (strictly more conservative for Verify);
# regenerate live.inc with it only together with a full verify.sh re-run.
MUST_DEFINE = "--must-define" in sys.argv

# reads / writes of one instruction (masks), with the usual full-write idioms
FULL_WRITE = {"movaps", "movups", "movdqa", "movdqu", "cvtsi2ss", "cvtsi2sd", "movd", "movq", "cvtdq2ps", "cvttps2dq", "cvtps2dq",
              "sqrtps", "rcpps", "rsqrtps", "movapd", "movupd", "pshufd", "cvtps2pd", "cvtpd2ps", "cvttss2si", "cvtss2si"}
FPU_NOREAD = {"fld", "fild", "fldz", "fld1", "fldpi", "fldl2e", "fldln2", "fldlg2", "fldl2t", "fnstcw", "fldcw", "fninit", "fwait",
              "fnstsw", "fstsw", "fnclex", "fnstenv", "fldenv", "frstor", "fnsave", "ffree", "fincstp", "fdecstp", "emms"}
def rw(i):
    try: rr, ww = i.regs_access()
    except capstone.CsError: return ALL, 0
    r = 0; w = 0
    for x in rr: r |= bit(i.reg_name(x))
    for x in ww: w |= bit(i.reg_name(x))
    m = i.mnemonic; ops = i.operands
    if len(ops) == 2 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg and m in ("xor", "sub", "xorps", "pxor", "xorpd"):
        r &= ~bit(i.reg_name(ops[0].reg))
    if ops and ops[0].type == X.X86_OP_REG and (m in FULL_WRITE or m in ("mov", "lea", "movzx", "movsx", "pop") or (m in ("movss", "movsd") and ops[1].type == X.X86_OP_MEM)):
        dn = i.reg_name(ops[0].reg); d = bit(dn)
        full = dn in ("eax", "ecx", "edx") or dn.startswith("xmm")          # al / ah / ax keep the rest of the register
        src_uses = any(o.type == X.X86_OP_REG and bit(i.reg_name(o.reg)) & d for o in ops[1:]) or \
                   any(o.type == X.X86_OP_MEM and (bit(i.reg_name(o.mem.base)) & d if o.mem.base else 0 or bit(i.reg_name(o.mem.index)) & d if o.mem.index else 0) for o in ops[1:])
        if full and not src_uses: r &= ~d
        if full: w |= d
    if m.startswith("f") and m not in FPU_NOREAD: r |= FPU
    if m in ("fld", "fild", "fldz", "fld1") : w |= 0   # a push: the old st(0) stays live below
    return r, w

def must_def(f, memo={}):
    """A sound lower bound on the registers a known callee writes on every path: the straight-line prefix from its entry up
    to the first control transfer (jcc / jmp / call / ret).  A lower bound is safe here — fewer registers killed by a call
    means more survive it, i.e. strictly more live-out (more conservative)."""
    if not MUST_DEFINE: return VOL           # default: a call clobbers every volatile (the pre-refinement behaviour)
    if f in memo: return memo[f]
    memo[f] = 0
    w_all = 0; a = f; steps = 0
    while steps < 200:
        i = insn(a)
        if i is None: break
        m = i.mnemonic
        if m in ("call", "ret", "retn", "jmp") or m.startswith("j") or m.startswith("loop"): break
        _, w = rw(i); w_all |= w
        a += i.size; steps += 1
    memo[f] = w_all & VOL
    return memo[f]

def inputs_of(f, memo={}):
    """registers read before written from f's entry (paths up to 300 instructions; a call ends a path, adding its inputs: no deeper)"""
    if f in memo: return memo[f]
    memo[f] = 0
    reads = 0; seen = set(); stack = [(f, 0, 0)]
    while stack:
        a, written, n = stack.pop()
        if (a, written) in seen or n > 300: continue
        seen.add((a, written)); i = insn(a)
        if i is None: continue
        r, w = rw(i); m = i.mnemonic
        reads |= r & ~written
        if m == "call":
            # a callee's register inputs that f has not written (one level: a callee's own memo may still be incomplete)
            t = i.operands[0]
            if t.type == X.X86_OP_IMM and t.imm in fset and t.imm != f: reads |= inputs_of(t.imm) & ~written & (ALL & ~FPU)
            continue
        if m in ("ret", "retn"): continue
        written |= w
        if m == "jmp":
            t = i.operands[0]
            if t.type == X.X86_OP_IMM:
                if t.imm in fset and t.imm != f: reads |= inputs_of(t.imm) & ~written & (ALL & ~FPU)
                else: stack.append((t.imm, written, n + 1))
            continue
        if m.startswith("j") or m.startswith("loop"):
            t = i.operands[0]
            if t.type == X.X86_OP_IMM: stack.append((t.imm, written, n + 1))
        stack.append((a + i.size, written, n + 1))
    memo[f] = reads
    return reads

# direct call / tail-jmp sites and address-taken functions
print("scanning ...", file=sys.stderr)
sites = {}          # callee -> [(site, kind, enclosing)]
for idx, f in enumerate(funcs):
    end = funcs[idx + 1] if idx + 1 < len(funcs) else text_hi
    end = min(end, f + 0x8000)
    for i in md.disasm(img[f - B:end - B], f):
        if i.mnemonic in ("call", "jmp") and i.operands and i.operands[0].type == X.X86_OP_IMM:
            t = i.operands[0].imm
            if t in fset and not (i.mnemonic == "jmp" and t == f):
                if i.mnemonic == "jmp" and owner(t) == f: continue
                sites.setdefault(t, []).append((i.address, i.mnemonic, f))
taken = set()
for sec in pe.sections:
    lo = sec.VirtualAddress; data = img[lo:lo + sec.Misc_VirtualSize]
    for k in range(0, len(data) - 3):
        v = struct.unpack_from("<I", data, k)[0]
        if v in fset and (sec.Name.startswith(b".text") is False or True):
            if sec.Name.startswith(b".text"):
                # in code: only as an immediate operand (push offset / mov reg, offset), not as a rel32 target
                if img[lo + k - 1] in (0x68, 0xb8, 0xb9, 0xba, 0xbb, 0xbe, 0xbf) or img[lo + k - 2:lo + k] in (b"\xc7\x05", b"\xc7\x00", b"\xc7\x01", b"\xc7\x06"):
                    taken.add(v)
            else: taken.add(v)
print(f"{len(funcs)} functions, {sum(len(v) for v in sites.values())} direct sites, {len(taken)} address-taken", file=sys.stderr)

def after(site_end, enclosing, live):
    """registers read after a call site's return before written"""
    reads = 0; seen = set(); stack = [(site_end, 0, 0)]
    while stack:
        a, written, n = stack.pop()
        if (a, written) in seen or n > 400: continue
        seen.add((a, written)); i = insn(a)
        if i is None: continue
        r, w = rw(i); m = i.mnemonic
        reads |= r & ~written
        if m == "call":
            t = i.operands[0]
            if t.type == X.X86_OP_IMM and t.imm in fset:
                reads |= inputs_of(t.imm) & ~written & (ALL & ~FPU)
                if MUST_DEFINE:                                             # continue past the call; it kills only must_def
                    written |= must_def(t.imm); stack.append((a + i.size, written, n + 1)); continue
            elif t.type != X.X86_OP_IMM: reads |= (2) & ~written            # indirect call: thiscall ECX at most
            if MUST_DEFINE:                                                 # unknown / indirect call: the ABI clobbers volatiles
                written |= VOL; stack.append((a + i.size, written, n + 1))
            continue
        if m in ("ret", "retn"):
            reads |= live.get(enclosing, ABI) & ~written
            continue
        written |= w
        if m == "jmp":
            t = i.operands[0]
            if t.type == X.X86_OP_IMM:
                if t.imm in fset and owner(t.imm) != enclosing:
                    reads |= (inputs_of(t.imm) | live.get(enclosing, ABI)) & ~written
                else: stack.append((t.imm, written, n + 1))
            else: reads |= live.get(enclosing, ABI) & ~written
            continue
        if m.startswith("j") or m.startswith("loop"):
            t = i.operands[0]
            if t.type == X.X86_OP_IMM: stack.append((t.imm, written, n + 1))
        stack.append((a + i.size, written, n + 1))
    return reads

live = {f: ABI for f in funcs}
for it in range(40):
    changed = 0
    for f in funcs:
        if f in taken or f not in sites: v = ABI
        else:
            v = 0
            for site, kind, enc in sites[f]:
                if kind == "jmp": v |= live.get(enc, ABI)
                else: v |= after(site + insn(site).size, enc, live)
            if f in taken: v |= ABI
        if v != live[f]: live[f] = v; changed += 1
    print(f"pass {it}: {changed} changed", file=sys.stderr)
    if not changed: break

def names(v):
    out = [n for n, b in GPR.items() if v & b] + [f"xmm{k}" for k in range(8) if v & (1 << (8 + k))] + (["x87"] if v & FPU else [])
    return ",".join(out)
with open(sys.argv[1], "w") as o:
    o.write("addr\tmask\tregs\tcallers\n")
    for f in funcs: o.write(f"{f:08x}\t{live[f]:x}\t{names(live[f])}\t{'taken' if f in taken else len(sites.get(f, []))}\n")
