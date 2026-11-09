#!/usr/bin/env python3
"""Instruction-level transliteration of an MXReflex.exe function into a native port (C++ for native/game/ports, engine_util.h).

lift.py <addr> <name> [description] > file.cpp
Same operations in the same order on guest memory; registers in locals; flags only where read (resolved statically to the
defining instruction); calls through LIFT_CALL; x87 on the Machine's stack (reflex_f80)."""
import sys, struct, capstone, pefile
from capstone import x86 as X

EXE = "/Users/payton/Documents/Development/REFLEX/game files/Archive/MXReflex.exe"
pe = pefile.PE(EXE, fast_load=True); img = pe.get_memory_mapped_image(); B = pe.OPTIONAL_HEADER.ImageBase
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
ENTS = {}
for l in open("/Users/payton/Documents/Development/REFLEX/_re/out/functions.tsv", encoding="latin-1").readlines()[1:]:
    c = l.split("\t"); ENTS[int(c[0], 16)] = c[1]

GROUP = "engine"   # set by gen.py before lift(); the REFLEX_PORT group string emitted
class LiftError(Exception): pass
# Worker thread procedures: a wait that blocks (the world runtime's WaitForSingleObject stand-in returns without popping its
# arguments) ends the thread's run (LIFT_CALLB).
BLOCKING = {0x881900, 0x84f320, 0x7df370, 0x7a5a70, 0x662ec0}   # + render-sync worker, ModelGroup worker (ld_render / ld_models), forest cull thread (ld_entities)
# Addresses inside functions where the world runtime may install a native (a patch of the original): the port runs it there as
# the interpreter would, and resumes at the pc it returns (one of the listed ones).
HOOKS = {0x7e1082: [0x7e1101]}   # runtime.cpp set_ambient_animation: the render-sync worker phase of 007e0fc0
# Functions whose entry reads the caller's flags: the call is a flags consumer, and the materialized EFLAGS word goes to the
# callee in fl (m.flags). __RTC_CheckEsp (/RTCs code: `cmp esi, esp; call __RTC_CheckEsp`, jne at its entry).
FLAGS_IN = {0x8fa3bd}
# Callees that move their return-address slot into the caller's frame and store there (__SEH_prolog4: the slot becomes the EH4
# record's encoded scope table, [ebp-8]): the call must not write the return address back afterwards.
NO_RA = {0x8fa554}

def insn(a):
    return next(md.disasm(img[a - B:a - B + 16], a))
def u32img(a): return struct.unpack("<I", img[a - B:a - B + 4])[0]
def u64img(a): return struct.unpack("<Q", img[a - B:a - B + 8])[0]
def rdata(a): return 0x916000 <= a < 0x9a6000

R32 = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"]
R16 = {"ax": "eax", "cx": "ecx", "dx": "edx", "bx": "ebx", "sp": "esp", "bp": "ebp", "si": "esi", "di": "edi"}
R8L = {"al": "eax", "cl": "ecx", "dl": "edx", "bl": "ebx"}
R8H = {"ah": "eax", "ch": "ecx", "dh": "edx", "bh": "ebx"}
CCS = ["o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g"]
FLAG_NEUTRAL = {"mov", "movzx", "movsx", "lea", "push", "pop", "movaps", "movups", "movapd", "movdqa", "movss", "movsd", "shufps",
                "unpcklps", "unpckhps", "mulps", "addps", "subps", "divps", "minps", "maxps", "sqrtps", "andps", "andnps", "orps",
                "xorps", "pxor", "mulss", "addss", "subss", "divss", "minss", "maxss", "sqrtss", "mulsd", "addsd", "subsd", "divsd",
                "cvtss2sd", "cvtsd2ss", "cvtps2pd", "cvtpd2ps", "cvtsi2ss", "cvttss2si", "cvtsi2sd", "fld", "fstp", "fst", "fld1",
                "fldz", "fadd", "fmul", "fdiv", "fsub", "fdivr", "fsubr", "faddp", "fmulp", "fdivp", "fsubp", "fdivrp", "fsubrp",
                "fxch", "fchs", "fstsw", "fnstsw", "fabs", "fsqrt", "cdq", "nop", "wait", "lahf", "movlhps", "movhlps", "unpcklpd", "cvtdq2ps", "cvttps2dq", "movd",
                "cmpltps", "cmpleps", "cmpneqps", "cmpeqps", "cmpnltps", "cmpnleps", "cmpunordps", "cmpordps", "not", "fild", "fistp",
                "movlps", "movhps", "movq", "pshufd", "sqrtsd", "movmskps", "cmpltss", "cmpless", "cmpeqss", "cmpneqss", "cmpnless", "cmpnltss"}
for cc in CCS: FLAG_NEUTRAL |= {"j" + cc, "set" + cc, "cmov" + cc}
FLAG_NEUTRAL |= {"jmp", "jz", "jnz", "sete", "setne"}
# instructions that do not touch the integer EFLAGS (so the flag reaching-def scan passes through them): the SSE / x87 /
# MMX forms the fx and anim groups added, plus the string-move forms.
FLAG_NEUTRAL |= {"fnstcw", "fldcw", "emms", "movq", "cvtpi2ps", "rcpss", "rsqrtss", "rcpps", "rsqrtps", "cvtps2dq",
                 "cvtsd2si", "cvttsd2si", "unpckhpd", "shufpd", "andpd", "xorpd", "orpd", "andnpd", "pand", "por", "pandn",
                 "movdqu", "movupd", "rep movsb", "rep stosb", "rep movsd", "rep stosd", "int3",
                 "prefetcht0", "prefetcht1", "prefetcht2", "prefetchnta",
                 "rep stosw", "rep movsw", "movsb", "movsw"}
# x87 arithmetic with an integer memory operand (m32int / m16int) and cwde (ld_ui, wave 4)
FLAG_NEUTRAL |= {"fiadd", "fimul", "fisub", "fisubr", "fidiv", "fidivr", "cwde"}
# x87 constants and transcendentals through lift_rt.h's x87_* helpers (vm.cpp execute_fp's reflex_f80::transcendental) (ld5_mid, wave 5)
X87_CONST = {"fldpi": "pi", "fldl2e": "l2e", "fldln2": "ln2", "fldl2t": "l2t", "fldlg2": "lg2"}
X87_TRANS = {"fpatan", "fyl2x", "fyl2xp1", "f2xm1", "fsin", "fcos", "fptan"}
FLAG_NEUTRAL |= set(X87_CONST) | X87_TRANS
CMP_PRED = {"eq": 0, "lt": 1, "le": 2, "unord": 3, "neq": 4, "nlt": 5, "nle": 6, "ord": 7}

def hx(v): return "0x%xu" % (v & 0xffffffff)

_CLEANUP = {}
def ret_cleanup(t):
    """the bytes a direct callee pops on return (its single `ret N`), else None (several / none / unknown)."""
    if t in _CLEANUP: return _CLEANUP[t]
    _CLEANUP[t] = None
    seen, todo, rets = set(), [t], set()
    try:
        while todo and len(seen) < 20000:
            a = todo.pop()
            while a not in seen:
                seen.add(a); i = insn(a); m = i.mnemonic
                if m.startswith("j") and i.operands[0].type == X.X86_OP_IMM:
                    if not (m == "jmp" and i.operands[0].imm in ENTS and i.operands[0].imm != t): todo.append(i.operands[0].imm)
                if m == "jmp": break
                if m == "ret": rets.add(i.operands[0].imm if i.operands else 0); break
                if m in ("int3", "hlt", "ud2"): break
                a += i.size
    except Exception:
        return None
    _CLEANUP[t] = rets.pop() if len(rets) == 1 else None
    return _CLEANUP[t]

def stack_step(i, st):
    """stack state after instruction i: (d = esp - entry esp or None, e = ebp - entry esp or None, frame = ebp holds this
    function's frame pointer (set by mov ebp, esp; cleared by any other write to ebp))."""
    d, e, fr = st; m = i.mnemonic; ops = i.operands
    def reg(k): return ops[k].type == X.X86_OP_REG and i.reg_name(ops[k].reg)
    def imm(k): return ops[k].imm if ops[k].type == X.X86_OP_IMM else None
    try: _, wr = i.regs_access()
    except Exception: wr = []
    wnames = {i.reg_name(r) for r in wr}
    nd = None; done = False
    if m == "push": nd = None if d is None else d - 4; done = True
    elif m == "pop":
        nd = None if d is None else d + 4; done = True
        if reg(0) == "ebp": e, fr = None, False
    elif m in ("sub", "add") and reg(0) == "esp" and len(ops) == 2 and imm(1) is not None:
        nd = None if d is None else (d - imm(1) if m == "sub" else d + imm(1)); done = True
    elif m == "mov" and reg(0) == "ebp" and reg(1) == "esp": nd = d; e, fr = d, True; done = True
    elif m == "mov" and reg(0) == "esp" and reg(1) == "ebp": nd = e; done = True
    elif m == "leave": nd = None if e is None else e + 4; e, fr = None, False; done = True
    elif m == "lea" and reg(0) == "esp" and ops[1].type == X.X86_OP_MEM and i.reg_name(ops[1].mem.base) == "ebp" and not ops[1].mem.index:
        nd = None if e is None else e + ops[1].mem.disp; done = True
    elif m == "call":
        c = ret_cleanup(ops[0].imm) if ops[0].type == X.X86_OP_IMM else None
        nd = None if (d is None or c is None) else d + c; done = True
    if not done: nd = None if "esp" in wnames else d
    if "ebp" in wnames and m not in ("mov", "leave", "pop") : e, fr = None, False
    elif m == "mov" and reg(0) == "ebp" and reg(1) != "esp": e, fr = None, False
    return (nd, e, fr)

class Lifter:
    def __init__(self, entry):
        self.entry = entry
        self.ins = {}            # addr -> insn
        self.targets = set()     # label addresses
        self.tables = {}         # jmp addr -> (index reg, [targets])
        self.walk()

    # ------------------------------------------------------------ discovery
    def walk(self):
        # Stack state along the walk (stack_step): a `jmp` to another Ghidra entry is a tail call only when the stack is back at
        # the entry depth. Ghidra also starts "functions" at labels inside a function (e.g. the continuation after an SEH catch
        # funclet, 004f498c inside 004f48e0): a jmp there while this function's frame is still on the stack is an internal jump
        # (followed and lifted inline: always exact), never LIFT_TAILJMP (which assumes a return address at [esp]).
        self.internal_jmps = set()
        todo = [(self.entry, (0, None, False))]
        while todo:
            a, st = todo.pop()
            while a not in self.ins:
                i = insn(a); self.ins[a] = i; m = i.mnemonic
                if m.startswith("j"):
                    op = i.operands[0]
                    if op.type == X.X86_OP_IMM:
                        t = op.imm
                        if m == "jmp" and t in ENTS and t != self.entry and not (st[2] or (st[0] is not None and st[0] != 0)):
                            pass   # tail call
                        else:
                            if m == "jmp" and t in ENTS and t != self.entry: self.internal_jmps.add(i.address)
                            todo.append((t, st)); self.targets.add(t)
                    elif m == "jmp":
                        n0 = len(todo); self.jump_table(i, todo)
                        todo[n0:] = [(t, st) for t in todo[n0:]]
                    if m == "jmp": break
                if m in ("ret", "int3", "hlt", "ud2"): break
                st = stack_step(i, st)
                a += i.size

    def jump_table(self, i, todo):
        op = i.operands[0]
        if op.type == X.X86_OP_REG or (op.type == X.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0):
            self.tailptr = getattr(self, "tailptr", set()); self.tailptr.add(i.address); return   # jmp reg / jmp [IAT]: a tail jump
        if op.type == X.X86_OP_MEM and op.mem.base != 0 and op.mem.index == 0:
            self.tailptr = getattr(self, "tailptr", set()); self.tailptr.add(i.address); return   # jmp [reg + disp] (vtable slot): a tail jump
        if op.type != X.X86_OP_MEM or op.mem.base != 0 or op.mem.scale != 4:
            raise LiftError("indirect jmp %x %s" % (i.address, i.op_str))
        idx = i.reg_name(op.mem.index); base = op.mem.disp
        # bound: the nearest preceding cmp idx, N / ja
        n = None
        a = i.address
        for back in range(1, 12):
            # scan backwards: decode from earlier addresses known in self.ins
            prevs = [p for p in self.ins if p < a and p + self.ins[p].size == a]
            if not prevs: break
            a = prevs[0]; p = self.ins[a]
            if p.mnemonic == "cmp" and p.reg_name(p.operands[0].reg) == idx and p.operands[1].type == X.X86_OP_IMM:
                n = p.operands[1].imm + 1; break
        prevs = [p for p in self.ins if p + self.ins[p].size == i.address]
        # two-level table behind a biased bound (ld7_b, 00754430): `lea R, [idx - c]; cmp R, N; ja; movzx idx, byte [idx + T8];
        # jmp [idx * 4 + T32]` (five consecutive instructions): idx is in [c, c + N], so the byte table is read at T8 + c .. T8 + c + N.
        lo = 0
        seq = [i]
        while len(seq) < 5:
            pv = [p for p in self.ins if p + self.ins[p].size == seq[-1].address]
            if not pv: break
            seq.append(self.ins[pv[0]])
        if len(seq) == 5:
            mz, ja, cp, le = seq[1], seq[2], seq[3], seq[4]
            if (mz.mnemonic == "movzx" and mz.operands[1].type == X.X86_OP_MEM and mz.operands[1].size == 1
                    and mz.reg_name(mz.operands[0].reg) == idx and mz.operands[1].mem.base != 0
                    and mz.reg_name(mz.operands[1].mem.base) == idx and mz.operands[1].mem.index == 0
                    and ja.mnemonic == "ja" and cp.mnemonic == "cmp" and cp.operands[0].type == X.X86_OP_REG
                    and cp.operands[1].type == X.X86_OP_IMM and le.mnemonic == "lea" and le.operands[1].mem.base != 0
                    and le.reg_name(le.operands[1].mem.base) == idx and le.operands[1].mem.index == 0
                    and le.reg_name(le.operands[0].reg) == cp.reg_name(cp.operands[0].reg) != idx):
                d = le.operands[1].mem.disp
                d = d - (1 << 32) if d >= 0x80000000 else d
                lo = -d; n = cp.operands[1].imm + 1
                if lo < 0: raise LiftError("jump table bias %x" % i.address)
                bt = (mz.operands[1].mem.disp & 0xffffffff) + lo
                n = max(img[bt - B + k] for k in range(n)) + 1
                prevs = []   # the bound and the byte table are resolved
        if n is not None and prevs:   # two-level table: movzx idx, byte [idx + bytes] then jmp [idx * 4 + dwords]
            p = self.ins[prevs[0]]
            if p.mnemonic == "movzx" and p.operands[1].type == X.X86_OP_MEM and p.operands[1].size == 1 and p.reg_name(p.operands[0].reg) == idx:
                bt = p.operands[1].mem.disp & 0xffffffff
                n = max(img[bt - B + k] for k in range(n)) + 1
        if n is None:   # no bound check (the compiler knew the range): the entries up to the first that is not code near the function
            n = 0
            while n < 256 and self.entry <= u32img(base + 4 * n) < self.entry + 0x4000: n += 1
            if n == 0: raise LiftError("jump table bound %x" % i.address)
        ts = [u32img(base + 4 * k) for k in range(n)]
        for t in ts: todo.append(t); self.targets.add(t)
        self.tables[i.address] = (idx, ts)

    # ------------------------------------------------------------ operands
    def addr(self, i, op):
        mm = op.mem
        parts = []
        if mm.segment != 0:
            if i.reg_name(mm.segment) != "fs": raise LiftError("segment %x" % i.address)
            parts.append("FS_BASE")   # the TEB (lift_rt.h FS_BASE = 0x7ffd0000, vm.cpp: the same address on every thread)
        if mm.base: parts.append(i.reg_name(mm.base))
        if mm.index: parts.append("%s * %d" % (i.reg_name(mm.index), mm.scale) if mm.scale != 1 else i.reg_name(mm.index))
        d = mm.disp & 0xffffffff
        if not parts: return hx(d)
        s = " + ".join(parts)
        if d:
            if d >= 0x80000000 and mm.base: s += " - " + hx(-d & 0xffffffff)
            else: s += " + " + hx(d)
        return s

    def regkind(self, name):
        if name in R32: return ("r32", name)
        if name in R16: return ("r16", R16[name])
        if name in R8L: return ("r8l", R8L[name])
        if name in R8H: return ("r8h", R8H[name])
        if name.startswith("xmm"): return ("x", "x" + name[3:])
        if name.startswith("st("): return ("st", int(name[3]))
        raise LiftError("register " + name)

    def note(self, a, size):
        if rdata(a) and size == 4:
            f = struct.unpack("<f", img[a - B:a - B + 4])[0]
            return " /* %r */" % f
        if rdata(a) and size == 8:
            f = struct.unpack("<d", img[a - B:a - B + 8])[0]
            return " /* %r */" % f
        return ""

    def rv(self, i, op, size=None):
        """value of an operand: uint32 (size<=4), uint64 (8), X (16)"""
        size = size or op.size
        if op.type == X.X86_OP_IMM: return hx(op.imm & ((1 << (8 * min(size, 4))) - 1))
        if op.type == X.X86_OP_REG:
            k, r = self.regkind(i.reg_name(op.reg))
            if k == "r32": return r
            if k == "r16": return "(%s & 0xffffu)" % r
            if k == "r8l": return "(%s & 0xffu)" % r
            if k == "r8h": return "((%s >> 8) & 0xffu)" % r
            if k == "x":
                return {4: "%s.l(0)" % r, 8: "%s.lo" % r, 16: r}[size]
            raise LiftError("rv st")
        a = self.addr(i, op)
        if op.mem.base == 0 and op.mem.index == 0: nt = self.note(op.mem.disp & 0xffffffff, size)
        else: nt = ""
        return {1: "rd8(m, %s)", 2: "rd16(m, %s)", 4: "rd32(m, %s)", 8: "rd64(m, %s)", 16: "ldx(m, %s)"}[size] % a + nt

    def wv(self, i, op, val, size=None):
        size = size or op.size
        if op.type == X.X86_OP_REG:
            k, r = self.regkind(i.reg_name(op.reg))
            if k == "r32": return "%s = %s;" % (r, val)
            if k == "r16": return "%s = (%s & 0xffff0000u) | ((%s) & 0xffffu);" % (r, r, val)
            if k == "r8l": return "%s = (%s & 0xffffff00u) | ((%s) & 0xffu);" % (r, r, val)
            if k == "r8h": return "%s = (%s & 0xffff00ffu) | (((%s) & 0xffu) << 8);" % (r, r, val)
            if k == "x": return "%s = %s;" % (r, val)
            raise LiftError("wv st")
        a = self.addr(i, op)
        return {1: "wr8(m, %s, %s);", 2: "wr16(m, %s, %s);", 4: "wr32(m, %s, %s);", 8: "wr64(m, %s, %s);", 16: "stx(m, %s, %s);"}[size] % (a, val)

    def xreg(self, i, op):
        k, r = self.regkind(i.reg_name(op.reg))
        if k != "x": raise LiftError("xmm expected %x" % i.address)
        return r

    # ------------------------------------------------------------ flags
    def reaching_defs(self, a, need):
        """flag-defining instructions reaching the consumer at a (backwards over all predecessors)"""
        defs = set(); seen = set(); todo = list(self.preds.get(a, []))
        while todo:
            p = todo.pop()
            if p in seen: continue
            seen.add(p); pi = self.ins[p]; m = pi.mnemonic
            if m in ("inc", "dec") and "c" in need: todo += self.preds.get(p, []); continue
            if m in FLAG_NEUTRAL or (self.cc_of(m) is not None): todo += self.preds.get(p, []); continue
            if m in ("ret", "call"): raise LiftError("flags for %x cross a %s at %x" % (a, m, p))
            defs.add(p)
        if not defs: raise LiftError("flag def not found for %x" % a)
        return defs

    def prepare_flags(self):
        addrs = sorted(self.ins)
        self.preds = {}
        for a in addrs:
            i = self.ins[a]; m = i.mnemonic
            succ = []
            if m not in ("jmp", "ret"): succ.append(a + i.size)
            if m.startswith("j") and i.operands[0].type == X.X86_OP_IMM: succ.append(i.operands[0].imm)
            if a in self.tables: succ += self.tables[a][1]
            for t in succ:
                if t in self.ins: self.preds.setdefault(t, []).append(a)
        self.consumers = {}   # consumer addr -> def addr, or a set of defs (materialized flags)
        self.defs = {}        # def addr -> set of consumer kinds
        self.multi = set()    # defs that materialize FL
        for a in addrs:
            i = self.ins[a]; m = i.mnemonic
            cc = self.cc_of(m)
            need = None
            if cc is not None: need = "c" if cc in ("b", "ae", "be", "a") else "x"
            elif m in ("sbb", "adc"): need = "c"
            elif m == "lahf": need = "c"
            elif m == "call" and i.operands[0].type == X.X86_OP_IMM and i.operands[0].imm in FLAGS_IN: need = "x"
            if need is None: continue
            ds = self.reaching_defs(a, need)
            if len(ds) == 1 and m != "call":
                d = ds.pop(); self.consumers[a] = d; self.defs.setdefault(d, set()).add(m)
            else:
                self.consumers[a] = frozenset(ds)
                for d in ds: self.defs.setdefault(d, set()).add(m); self.multi.add(d)

    def cc_of(self, m):
        for pre in ("cmov", "set", "j"):
            if m.startswith(pre) and m != "jmp":
                cc = m[len(pre):]
                cc = {"z": "e", "nz": "ne", "c": "b", "nc": "ae", "nae": "b", "nb": "ae", "na": "be", "nbe": "a", "nge": "l", "nl": "ge",
                      "ng": "le", "nle": "g", "pe": "p", "po": "np"}.get(cc, cc)
                if cc in CCS: return cc
        return None

    def cond(self, d, cc):
        """C++ condition for cc after the def at d"""
        if isinstance(d, frozenset): return "fl_cc(%d, FL)" % CCS.index(cc)
        kind, n = self.defkind[d]
        A, Bv, Rr = "fA_%x" % d, "fB_%x" % d, "fR_%x" % d
        mask = {4: "", 2: " & 0xffffu", 1: " & 0xffu"}[n]
        if kind == "fcmp":
            c = "fC_%x" % d
            return {"b": "fc_b(%s)" % c, "ae": "!fc_b(%s)" % c, "e": "fc_z(%s)" % c, "ne": "!fc_z(%s)" % c,
                    "be": "(fc_b(%s) || fc_z(%s))" % (c, c), "a": "!(fc_b(%s) || fc_z(%s))" % (c, c),
                    "p": "%s.un" % c, "np": "!%s.un" % c, "l": "false", "ge": "true", "le": "fc_z(%s)" % c, "g": "!fc_z(%s)" % c,
                    "s": "false", "ns": "true", "o": "false", "no": "true"}[cc]
        if kind == "sub":
            sa, sb = "int32_t(sext(%s, %d))" % (A, n), "int32_t(sext(%s, %d))" % (Bv, n)
            res = "((%s - %s)%s)" % (A, Bv, mask)
            return {"e": "%s == %s" % (A, Bv), "ne": "%s != %s" % (A, Bv), "b": "%s < %s" % (A, Bv), "ae": "%s >= %s" % (A, Bv),
                    "be": "%s <= %s" % (A, Bv), "a": "%s > %s" % (A, Bv), "l": "%s < %s" % (sa, sb), "ge": "%s >= %s" % (sa, sb),
                    "le": "%s <= %s" % (sa, sb), "g": "%s > %s" % (sa, sb), "s": "sgn(%s, %d)" % (res, n), "ns": "!sgn(%s, %d)" % (res, n),
                    "o": "sub_of(%s, %s, %d)" % (A, Bv, n), "no": "!sub_of(%s, %s, %d)" % (A, Bv, n),
                    "p": "parity8(%s)" % res, "np": "!parity8(%s)" % res}[cc]
        if kind == "add":
            res = "((%s + %s)%s)" % (A, Bv, mask)
            zf, sf, of, cf = "(%s == 0)" % res, "sgn(%s, %d)" % (res, n), "add_of(%s, %s, %d)" % (A, Bv, n), "add_cf(%s, %s, %d)" % (A, Bv, n)
            return self.generic(cc, zf, sf, of, cf, "parity8(%s)" % res)
        if kind == "logic":
            return self.generic(cc, "(%s == 0)" % Rr, "sgn(%s, %d)" % (Rr, n), "false", "false", "parity8(%s)" % Rr)
        if kind in ("inc", "dec"):
            op = "add" if kind == "inc" else "sub"
            return self.generic(cc, "(%s == 0)" % Rr, "sgn(%s, %d)" % (Rr, n), "%s_of(%s, 1u, %d)" % (op, A, n), None, "parity8(%s)" % Rr)
        if kind == "neg":
            return self.generic(cc, "(%s == 0)" % Rr, "sgn(%s, %d)" % (Rr, n), "sub_of(0u, %s, %d)" % (A, n), "(%s != 0)" % A, "parity8(%s)" % Rr)
        if kind == "shift":
            cf = of = None
            if d in self.shiftinfo:   # 32-bit shift by an immediate count 1..31 (vm.cpp: CF = the last bit out; OF only for count 1)
                sm, sc = self.shiftinfo[d]
                if sm in ("shl", "sal"): cf = "(((%s >> %d) & 1u) != 0)" % (A, 32 - sc)
                elif sm == "shr": cf = "(((%s >> %d) & 1u) != 0)" % (A, sc - 1)
                else: cf = "(((uint32_t(int32_t(%s) >> %d)) & 1u) != 0)" % (A, sc - 1)
                if sc != 1 or sm == "sar": of = "false"
                elif sm == "shr": of = "sgn(%s, 4)" % A
                else: of = "(sgn(%s, 4) != %s)" % (Rr, cf)
            return self.generic(cc, "(%s == 0)" % Rr, "sgn(%s, %d)" % (Rr, n), of, cf, "parity8(%s)" % Rr)
        if kind == "mul":
            hi = "(%s != 0)" % Rr
            if cc not in ("o", "no", "b", "ae"): raise LiftError("mul flag %s" % cc)
            return hi if cc in ("o", "b") else "!" + hi
        if kind in ("sbb", "adc"):
            K = "fK_%x" % d
            if kind == "sbb":
                res = "((%s - %s - %s)%s)" % (A, Bv, K, mask)
                cf = "(uint64_t(%s) < uint64_t(%s) + uint64_t(%s))" % (A, Bv, K)
                of = "sbb_of(%s, %s, %s)" % (A, Bv, K)
            else:
                res = "((%s + %s + %s)%s)" % (A, Bv, K, mask)
                cf = "(uint64_t(%s) + uint64_t(%s) + uint64_t(%s) > 0xffffffffull)" % (A, Bv, K)
                of = None
            return self.generic(cc, "(%s == 0)" % res, "sgn(%s, %d)" % (res, n), of, cf, "parity8(%s)" % res)
        raise LiftError("cond kind " + kind)

    def generic(self, cc, zf, sf, of, cf, pf):
        need = {"e": [zf], "ne": [zf], "s": [sf], "ns": [sf], "o": [of], "no": [of], "b": [cf], "ae": [cf], "be": [cf, zf], "a": [cf, zf],
                "l": [sf, of], "ge": [sf, of], "le": [zf, sf, of], "g": [zf, sf, of], "p": [pf], "np": [pf]}[cc]
        if any(x is None for x in need): raise LiftError("flag %s unavailable" % cc)
        zf, sf, of, cf, pf = [x if x is not None else "UNAVAILABLE" for x in (zf, sf, of, cf, pf)]
        return {"e": zf, "ne": "!" + zf, "s": sf, "ns": "!" + sf, "o": of, "no": "!" + of, "b": cf, "ae": "!" + cf,
                "be": "(%s || %s)" % (cf, zf), "a": "!(%s || %s)" % (cf, zf), "l": "(%s != %s)" % (sf, of), "ge": "(%s == %s)" % (sf, of),
                "le": "(%s || %s != %s)" % (zf, sf, of), "g": "(!%s && %s == %s)" % (zf, sf, of), "p": pf, "np": "!" + pf}[cc]

    def cf_of(self, d):
        kind, n = self.defkind[d]
        return self.cond(d, "b")

    # ------------------------------------------------------------ emission
    def emit(self, name, desc, outs="LIFT_OUTS"):
        self.prepare_flags()
        self.defkind = {}
        self.shiftinfo = {}
        self.decls = []
        body = []
        addrs = sorted(self.ins)
        if addrs[0] != self.entry:   # code below the entry (reached by an internal jmp): start with a jump to the entry block
            self.targets.add(self.entry); body.append("    goto L_%x;" % self.entry)
        for k, a in enumerate(addrs):
            i = self.ins[a]
            if a in self.targets: body.append("L_%x:" % a)
            if a in HOOKS and a != self.entry:
                body.append("    { auto it_ = m.natives.find(0x%xu); if (it_ != m.natives.end()) { LIFT_SYNC_OUT(); uint32_t pc_ = it_->second(m);" % a)
                body.append("        if (pc_ != reflex_vm::RUN_ORIGINAL) { LIFT_SYNC_IN(); %s throw std::runtime_error(\"hook resume\"); } } }" %
                            " ".join("if (pc_ == 0x%xu) goto L_%x;" % (t, t) for t in HOOKS[a]))
                for t in HOOKS[a]: self.targets.add(t)
            try:
                code = self.one(i)
            except LiftError:
                raise
            except Exception as e:
                raise LiftError("%x %s %s: %r" % (a, i.mnemonic, i.op_str, e))
            if a in self.multi: code = code + [self.fl_line(a)]
            for c in code: body.append("    " + c)
            # fall-through into a non-adjacent instruction
            m = i.mnemonic
            if m not in ("jmp", "ret") and not (m == "call" and False):
                nxt = a + i.size
                if k + 1 < len(addrs) and addrs[k + 1] != nxt or k + 1 == len(addrs):
                    if nxt in self.ins:
                        body.append("    goto L_%x;" % nxt); self.targets.add(nxt)
                    elif m not in ("int3",):
                        raise LiftError("falls out of the function at %x" % a)
        # labels added late (fall-through gotos): re-run label insertion
        out = []
        for line in body:
            out.append(line)
        # insert labels added after their instruction was emitted
        text = []
        seen_labels = {l[:-1] for l in out if l.startswith("L_")}
        final = []
        for line in out:
            final.append(line)
        res = []
        for a_line in final:
            res.append(a_line)
        # second pass to place missing labels
        lines = []
        idx_of = {}
        for line in res:
            lines.append(line)
        missing = {"L_%x" % t for t in self.targets} - seen_labels
        if missing:
            # re-emit with all targets known
            return None
        decl = "\n".join("    " + d for d in self.decls)
        unused_lbl = set()
        hdr = "// %08x %s%s\n" % (self.entry, name, (": " + desc) if desc else "")
        return (hdr + "REFLEX_PORT_FN(0x%08x, %s, \"%s\", %s) {\n    LIFT_ENTRY();\n%s\n%s\n}\n" %
                (self.entry, name, GROUP, outs, decl, "\n".join(lines)))

    def fl_line(self, d):
        kind, n = self.defkind[d]
        if "uint32_t FL = 0;" not in self.decls: self.decls.append("uint32_t FL = 0;")
        if kind == "fcmp": return "FL = fc_flags(fC_%x);" % d
        if kind == "sub": return "FL = fl_sub(fA_%x, fB_%x, %d);" % (d, d, n)
        if kind == "add": return "FL = fl_add(fA_%x, fB_%x, %d);" % (d, d, n)
        if kind == "logic": return "FL = fl_logic(fR_%x, %d);" % (d, n)
        if kind in ("inc", "dec"):   # CF is not written by inc / dec: keep FL's (CF consumers' reaching defs pass through inc / dec)
            return "FL = (FL & 1u) | fl_logic(fR_%x, %d) | (%s_of(fA_%x, 1u, %d) ? 0x800u : 0u);" % (d, n, "add" if kind == "inc" else "sub", d, n)
        raise LiftError("materialized flags of %s at %x" % (kind, d))

    def capture(self, i, kind, n, A=None, Bv=None):
        """flag captures for a def with consumers; returns code lines (before the operation)"""
        if i.address not in self.defs: return []
        d = i.address
        self.defkind[d] = (kind, n)
        out = []
        if kind == "fcmp": return out
        if A is not None:
            self.decls.append("uint32_t fA_%x = 0;" % d); out.append("fA_%x = %s;" % (d, A))
        if Bv is not None:
            self.decls.append("uint32_t fB_%x = 0;" % d); out.append("fB_%x = %s;" % (d, Bv))
        return out

    def result_capture(self, i, R):
        if i.address not in self.defs: return []
        self.decls.append("uint32_t fR_%x = 0;" % i.address)
        return ["fR_%x = %s;" % (i.address, R)]

    def one(self, i):
        m = i.mnemonic; ops = i.operands; a = i.address
        n = ops[0].size if ops else 4
        # lock-prefixed read-modify-write: one guest thread runs at a time (interpreter / baton), so it is the plain operation
        if m.startswith("lock ") and m[5:] in ("or", "and", "xor", "add", "sub", "inc", "dec"): m = m[5:]
        if m in ("nop", "wait", "fwait", "prefetcht0", "prefetcht1", "prefetcht2", "prefetchnta"): return []   # prefetch: no architectural effect (vm.cpp)
        if m == "lea" and i.reg_name(ops[0].reg) == i.reg_name(ops[1].mem.base) and ops[1].mem.index == 0 and ops[1].mem.disp == 0:
            return []
        # ---- data movement
        if m == "mov": return [self.wv(i, ops[0], self.rv(i, ops[1], n))]
        if m == "movzx": return [self.wv(i, ops[0], self.rv(i, ops[1]))]
        if m == "movsx": return [self.wv(i, ops[0], "sext(%s, %d)" % (self.rv(i, ops[1]), ops[1].size))]
        if m == "lea": return [self.wv(i, ops[0], "(" + self.addr(i, ops[1]) + ")")]
        if m == "push":
            v = self.rv(i, ops[0], 4)
            if "esp" in v: return ["{ uint32_t v_ = %s; esp -= 4; wr32(m, esp, v_); }" % v]
            return ["esp -= 4; wr32(m, esp, %s);" % v]
        if m == "pop":
            if ops[0].type == X.X86_OP_REG: return ["%s esp += 4;" % self.wv(i, ops[0], "rd32(m, esp)")]
            return ["{ uint32_t v_ = rd32(m, esp); esp += 4; %s }" % self.wv(i, ops[0], "v_")]
        if m == "cdq": return ["edx = (eax >> 31) ? 0xffffffffu : 0;"]
        if m == "cwde": return ["eax = uint32_t(int32_t(int16_t(uint16_t(eax))));"]
        if m in ("movaps", "movups", "movapd", "movdqa", "movupd", "movdqu"):
            return [self.wv(i, ops[0], self.rv(i, ops[1], 16), 16)]
        if m in ("movss", "movsd"):
            sz = 4 if m == "movss" else 8
            if ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_MEM:
                return ["%s = movs%s_m(%s);" % (self.xreg(i, ops[0]), "s" if sz == 4 else "d", self.rv(i, ops[1], sz))]
            if ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG:
                r = self.xreg(i, ops[0]); return ["%s = movs%s_r(%s, %s);" % (r, "s" if sz == 4 else "d", r, self.xreg(i, ops[1]))]
            return [self.wv(i, ops[0], self.rv(i, ops[1], sz), sz)]
        if m == "movq":
            d, sv = ops[0], ops[1]
            def mmreg(op):
                nm = i.reg_name(op.reg) if op.type == X.X86_OP_REG else ""
                return int(nm[2:]) if nm.startswith("mm") else None
            if sv.type == X.X86_OP_REG and mmreg(sv) is not None:
                src = "m.mmx[%d]" % mmreg(sv); pre = ["if (!m.mmx_active) throw std::runtime_error(\"MMX read before MMX write\");"]
            elif sv.type == X.X86_OP_REG: src = "%s.lo" % self.xreg(i, sv); pre = []
            else: src = self.rv(i, sv, 8); pre = []
            if d.type == X.X86_OP_REG and mmreg(d) is not None:
                return pre + ["{ uint64_t v_ = %s; if (!m.mmx_active && !m.fpu.empty()) throw std::runtime_error(\"MMX with a live x87 stack\"); m.mmx_active = true; m.mmx[%d] = v_; }" % (src, mmreg(d))]
            if d.type == X.X86_OP_REG: return pre + ["%s = movsd_m(%s);" % (self.xreg(i, d), src)]   # movq into xmm zeroes the upper lanes
            return pre + [self.wv(i, d, src, 8)]
        if m == "movd":
            if ops[0].type == X.X86_OP_REG and i.reg_name(ops[0].reg).startswith("xmm"):
                return ["%s = movd(%s);" % (self.xreg(i, ops[0]), self.rv(i, ops[1], 4))]
            return [self.wv(i, ops[0], "%s.l(0)" % self.xreg(i, ops[1]), 4)]
        if m in ("movlps", "movhps"):
            if ops[0].type == X.X86_OP_MEM:
                return [self.wv(i, ops[0], "%s.%s" % (self.xreg(i, ops[1]), "lo" if m == "movlps" else "hi"), 8)]
            r = self.xreg(i, ops[0]); return ["%s.%s = %s;" % (r, "lo" if m == "movlps" else "hi", self.rv(i, ops[1], 8))]
        # ---- SSE arithmetic
        base = m[:-2]; suf = m[-2:]
        if base in ("add", "sub", "mul", "div", "min", "max") and suf in ("ps", "ss", "sd", "pd"):
            r = self.xreg(i, ops[0]); sz = {"ps": 16, "pd": 16, "ss": 4, "sd": 8}[suf]
            return ["%s = %s(%s, %s);" % (r, m, r, self.rv(i, ops[1], sz))]
        if m in ("sqrtps", "sqrtss", "sqrtsd"):
            r = self.xreg(i, ops[0]); sz = {"sqrtps": 16, "sqrtss": 4, "sqrtsd": 8}[m]
            return ["%s = %s(%s, %s);" % (r, m, r, self.rv(i, ops[1], sz))]
        if m in ("rcpss", "rsqrtss"):
            r = self.xreg(i, ops[0]); return ["%s = %s(%s, %s);" % (r, m, r, self.rv(i, ops[1], 4))]
        if m in ("rcpps", "rsqrtps"):
            r = self.xreg(i, ops[0]); return ["%s = %s(%s, %s);" % (r, m, r, self.rv(i, ops[1], 16))]
        if m == "fnstcw": return [self.wv(i, ops[0], "uint32_t(m.fpu_cw)", 2)]
        if m == "fldcw": return ["m.fpu_cw = uint16_t(%s);" % self.rv(i, ops[0], 2)]
        if m == "emms": return ["if (m.mmx_active) { m.fpu.clear(); m.mmx_active = false; }"]
        if m == "cvtpi2ps":
            r = self.xreg(i, ops[0]); sv = ops[1]
            if sv.type == X.X86_OP_REG:
                src = "m.mmx[%d]" % int(i.reg_name(sv.reg)[2:]); pre = ["if (!m.mmx_active) throw std::runtime_error(\"MMX read before MMX write\");"]
            else: src = self.rv(i, sv, 8); pre = []
            return pre + ["{ uint64_t v_ = %s; %s.lo = uint64_t(fb(float(int32_t(uint32_t(v_))))) | (uint64_t(fb(float(int32_t(uint32_t(v_ >> 32))))) << 32); }" % (src, r)]
        if m in ("andps", "andnps", "orps", "xorps", "pxor", "andpd", "xorpd", "pand", "por", "andnpd", "orpd"):
            r = self.xreg(i, ops[0])
            fn = {"pxor": "xorps", "xorpd": "xorps", "andpd": "andps", "pand": "andps", "por": "orps", "andnpd": "andnps", "orpd": "orps"}.get(m, m)
            if ops[1].type == X.X86_OP_REG and self.xreg(i, ops[1]) == r and fn == "xorps": return ["%s = X{};" % r]
            return ["%s = %s(%s, %s);" % (r, fn, r, self.rv(i, ops[1], 16))]
        if m in ("shufps", "shufpd"):
            r = self.xreg(i, ops[0]); return ["%s = %s(%s, %s, 0x%x);" % (r, m, r, self.rv(i, ops[1], 16), ops[2].imm & 0xff)]
        if m == "pshufd":
            r = self.xreg(i, ops[0]); return ["%s = pshufd(%s, 0x%x);" % (r, self.rv(i, ops[1], 16), ops[2].imm & 0xff)]
        if m in ("unpcklps", "unpckhps", "movlhps", "movhlps", "unpcklpd", "unpckhpd"):
            r = self.xreg(i, ops[0]); return ["%s = %s(%s, %s);" % (r, m, r, self.rv(i, ops[1], 16))]
        if m.startswith("cmp") and m[-2:] in ("ps", "ss", "pd", "sd") and m[3:-2] in CMP_PRED:
            r = self.xreg(i, ops[0]); pred = CMP_PRED[m[3:-2]]; suf = m[-2:]
            sz = {"ps": 16, "pd": 16, "ss": 4, "sd": 8}[suf]
            return ["%s = cmp%s<%d>(%s, %s);" % (r, suf, pred, r, self.rv(i, ops[1], sz))]
        if m in ("comiss", "ucomiss", "comisd", "ucomisd"):
            sz = 4 if m.endswith("ss") else 8
            if a not in self.defs: return []
            self.defkind[a] = ("fcmp", 4)
            self.decls.append("FCmp fC_%x{};" % a)
            return ["fC_%x = fcmp%d(%s, %s);" % (a, 32 if sz == 4 else 64, self.rv(i, ops[0], sz), self.rv(i, ops[1], sz))]
        if m == "lahf":
            d = self.consumers[a]
            if isinstance(d, frozenset): return ["eax = (eax & 0xffff00ffu) | (((FL & 0xd5u) | 2u) << 8);"]
            if self.defkind[d][0] != "fcmp": raise LiftError("lahf after non-float compare %x" % a)
            return ["eax = (eax & 0xffff00ffu) | ((fc_flags(fC_%x) | 2u) << 8);" % d]
        # ---- conversions
        if m == "cvtss2sd": r = self.xreg(i, ops[0]); return ["%s = cvtss2sd(%s, %s);" % (r, r, self.rv(i, ops[1], 4))]
        if m == "cvtsd2ss": r = self.xreg(i, ops[0]); return ["%s = cvtsd2ss(%s, %s);" % (r, r, self.rv(i, ops[1], 8))]
        if m == "cvtps2pd": r = self.xreg(i, ops[0]); return ["%s = cvtps2pd(%s);" % (r, self.rv(i, ops[1], 8))]
        if m == "cvtpd2ps": r = self.xreg(i, ops[0]); return ["%s = cvtpd2ps(%s);" % (r, self.rv(i, ops[1], 16))]
        if m in ("cvtdq2ps", "cvttps2dq", "cvtps2dq"): r = self.xreg(i, ops[0]); return ["%s = %s(%s);" % (r, m, self.rv(i, ops[1], 16))]
        if m == "cvtsi2ss": r = self.xreg(i, ops[0]); return ["%s = cvtsi2ss(%s, %s);" % (r, r, self.rv(i, ops[1], 4))]
        if m == "cvtsi2sd": r = self.xreg(i, ops[0]); return ["%s = cvtsi2sd(%s, %s);" % (r, r, self.rv(i, ops[1], 4))]
        if m in ("cvttss2si", "cvtss2si"): return [self.wv(i, ops[0], "%s(%s)" % (m, self.rv(i, ops[1], 4)))]
        if m in ("cvttsd2si", "cvtsd2si"): return [self.wv(i, ops[0], "%s(%s)" % (m, self.rv(i, ops[1], 8)))]
        if m == "movmskps": return [self.wv(i, ops[0], "movmskps(%s)" % self.rv(i, ops[1], 16))]
        # ---- integer arithmetic
        if m in ("add", "sub", "cmp", "and", "or", "xor", "test"):
            A = self.rv(i, ops[0]); Bv = self.rv(i, ops[1], n)
            msk = {4: "", 2: " & 0xffffu", 1: " & 0xffu"}[n]
            if m in ("xor", "sub") and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg:
                if self.cap_logic(i, n): return self.result_capture(i, "0u") + [self.wv(i, ops[0], "0u")]
                return [self.wv(i, ops[0], "0u")]
            if m in ("add", "sub", "cmp"):
                pre = self.capture(i, "add" if m == "add" else "sub", n, A, Bv)
                if pre: A, Bv = "fA_%x" % a, "fB_%x" % a
                if m == "cmp": return pre
                op = "+" if m == "add" else "-"
                return pre + [self.wv(i, ops[0], "(%s %s %s)%s" % (A, op, Bv, msk) if msk else "%s %s %s" % (A, op, Bv))]
            op = {"and": "&", "test": "&", "or": "|", "xor": "^"}[m]
            res = "(%s %s %s)" % (A, op, Bv)
            if a in self.defs:
                self.defkind[a] = ("logic", n)
                cap = self.result_capture(i, res)
                if m == "test": return cap
                return cap + [self.wv(i, ops[0], "fR_%x" % a)]
            if m == "test": return []
            return [self.wv(i, ops[0], res)]
        if m in ("inc", "dec"):
            A = self.rv(i, ops[0]); msk = {4: "", 2: " & 0xffffu", 1: " & 0xffu"}[n]
            res = "(%s %s 1u)%s" % (A, "+" if m == "inc" else "-", msk)
            if a in self.defs:
                pre = self.capture(i, m, n, A); self.defkind[a] = (m, n)
                return pre + self.result_capture(i, "(fA_%x %s 1u)%s" % (a, "+" if m == "inc" else "-", msk)) + [self.wv(i, ops[0], "fR_%x" % a)]
            return [self.wv(i, ops[0], res)]
        if m == "neg":
            A = self.rv(i, ops[0]); msk = {4: "", 2: " & 0xffffu", 1: " & 0xffu"}[n]
            if a in self.defs:
                pre = self.capture(i, "neg", n, A)
                return pre + self.result_capture(i, "(0u - fA_%x)%s" % (a, msk)) + [self.wv(i, ops[0], "fR_%x" % a)]
            return [self.wv(i, ops[0], "(0u - %s)%s" % (A, msk))]
        if m == "not": return [self.wv(i, ops[0], "~%s" % self.rv(i, ops[0]))]
        if m in ("shl", "shr", "sar", "sal"):
            A = self.rv(i, ops[0]); cnt = ops[1].imm & 31 if len(ops) > 1 and ops[1].type == X.X86_OP_IMM else None
            if len(ops) == 1: cnt = 1
            if cnt is None:
                c = "(%s & 31u)" % self.rv(i, ops[1], 1)
            else: c = str(cnt)
            if n != 4:
                # 8 / 16-bit shifts (count masked to 5 bits as for 32-bit; wv keeps the register's other bytes). Flags
                # of narrow shifts are not modelled: refuse when a consumer reads them.
                if a in self.defs: raise LiftError("flags of narrow shift %x" % a)
                if m in ("shl", "sal"): res = "(%s << %s)" % (A, c)
                elif m == "shr": res = "(%s >> %s)" % (A, c)
                else: res = "uint32_t(int32_t(sext(%s, %d)) >> %s)" % (A, n, c)
                return [self.wv(i, ops[0], res)]
            if m in ("shl", "sal"): res = "(%s << %s)" % (A, c)
            elif m == "shr": res = "(%s >> %s)" % (A, c)
            else: res = "uint32_t(int32_t(%s) >> %s)" % (A, c)
            if a in self.defs:
                if cnt is None: raise LiftError("flags of variable shift %x" % a)
                if cnt == 0: raise LiftError("flags of a zero-count shift %x" % a)
                pre = self.capture(i, "shift", n, A)
                self.shiftinfo[a] = (m, cnt)
                if m in ("shl", "sal"): res = "(fA_%x << %s)" % (a, c)
                elif m == "shr": res = "(fA_%x >> %s)" % (a, c)
                else: res = "uint32_t(int32_t(fA_%x) >> %s)" % (a, c)
                return pre + self.result_capture(i, res) + [self.wv(i, ops[0], "fR_%x" % a)]
            return [self.wv(i, ops[0], res)]
        if m in ("shld", "shrd") and n == 4 and len(ops) == 3:
            # double-precision shift r/m32, r32, imm8 / cl (count & 31; a zero count leaves the destination unchanged)
            if a in self.defs: raise LiftError("flags of %s %x" % (m, a))
            A = self.rv(i, ops[0]); Bv = self.rv(i, ops[1], 4)
            c = str(ops[2].imm & 31) if ops[2].type == X.X86_OP_IMM else "(%s & 31u)" % self.rv(i, ops[2], 1)
            if m == "shld": e = "(d_ << c_) | (s_ >> (32u - c_))"
            else: e = "(d_ >> c_) | (s_ << (32u - c_))"
            return ["{ uint32_t d_ = %s, s_ = %s, c_ = %s; if (c_) { %s } }" % (A, Bv, c, self.wv(i, ops[0], e))]
        if m in ("rol", "ror"):   # 32-bit rotate by an immediate count; flags (CF / OF only) must be dead
            if a in self.defs: raise LiftError("%s flags %x" % (m, a))
            if n != 4: raise LiftError("narrow rotate %x" % a)
            cnt = 1 if len(ops) == 1 else (ops[1].imm & 31 if ops[1].type == X.X86_OP_IMM else None)
            A = self.rv(i, ops[0])
            if cnt is None:   # rotate by cl: count masked to 5 bits, 0 leaves the operand
                if not (ops[1].type == X.X86_OP_REG and i.reg_name(ops[1].reg) == "cl"): raise LiftError("variable rotate %x" % a)
                e = "((v_ << c_) | (v_ >> (32u - c_)))" if m == "rol" else "((v_ >> c_) | (v_ << (32u - c_)))"
                return ["{ uint32_t v_ = %s, c_ = ecx & 31u; if (c_) { %s } }" % (A, self.wv(i, ops[0], e))]
            if cnt == 0: return []
            l, r = (cnt, 32 - cnt) if m == "rol" else (32 - cnt, cnt)
            return ["{ uint32_t v_ = %s; %s }" % (A, self.wv(i, ops[0], "((v_ << %d) | (v_ >> %d))" % (l, r)))]
        if m == "imul":
            if a in self.defs: raise LiftError("imul flags %x" % a)
            # the low 32 bits of the signed product, computed unsigned: int32_t * int32_t overflow is undefined behaviour in C++
            # (clang -O3 then folds e.g. sign(x * x) to "not negative": Model_Submit 007f1b00 lost its 2^32 fild correction)
            if len(ops) == 3: return [self.wv(i, ops[0], "uint32_t(uint32_t(%s) * uint32_t(%s))" % (self.rv(i, ops[1]), hx(ops[2].imm)))]
            if len(ops) == 2: return [self.wv(i, ops[0], "uint32_t(uint32_t(%s) * uint32_t(%s))" % (self.rv(i, ops[0]), self.rv(i, ops[1])))]
            return ["{ int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t(%s)); eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32); }" % self.rv(i, ops[0])]
        if m in ("cmpxchg", "lock cmpxchg") and n == 4:   # (lock) cmpxchg [mem], r32: flags of cmp eax, [mem]; equal: [mem] = r32, else eax = [mem]
            A = "eax"; Bv = self.rv(i, ops[0])
            pre = self.capture(i, "sub", 4, A, Bv)
            if pre: A, Bv = "fA_%x" % a, "fB_%x" % a
            else: pre = ["{ uint32_t c_ = %s; if (eax == c_) { %s } else eax = c_; }" % (Bv, self.wv(i, ops[0], self.rv(i, ops[1])))]; return pre
            return pre + ["if (%s == %s) { %s } else eax = %s;" % (A, Bv, self.wv(i, ops[0], self.rv(i, ops[1])), Bv)]
        if m == "mul" and len(ops) == 1:
            if n != 4: raise LiftError("narrow mul %x" % a)
            out = ["{ uint64_t p_ = uint64_t(eax) * uint64_t(%s); eax = uint32_t(p_); edx = uint32_t(p_ >> 32); }" % self.rv(i, ops[0])]
            if a in self.defs:   # CF / OF read later (set iff the high dword is non-zero): capture edx
                self.defkind[a] = ("mul", 4); out += self.result_capture(i, "edx")
            return out
        if m in ("mul", "div", "idiv") and len(ops) == 1:
            if n != 4: raise LiftError("narrow %s %x" % (m, a))
            if a in self.defs: raise LiftError("%s flags %x" % (m, a))
            src = self.rv(i, ops[0])
            if m == "mul": return ["{ uint64_t p_ = uint64_t(eax) * uint64_t(%s); eax = uint32_t(p_); edx = uint32_t(p_ >> 32); }" % src]
            return ["{ uint32_t d_ = %s; %s(eax, edx, d_); }" % (src, "udiv32" if m == "div" else "sdiv32")]
        if m in ("sbb", "adc") and n == 1 and ops[0].type == X.X86_OP_REG and ops[1].type == X.X86_OP_REG and ops[0].reg == ops[1].reg and m == "sbb":
            if a in self.defs: raise LiftError("sbb flags consumed %x" % a)
            d = self.consumers[a]; cf = self.cond(d, "b")
            return [self.wv(i, ops[0], "(%s) ? 0xffu : 0u" % cf)]
        if m in ("sbb", "adc"):
            d = self.consumers[a]; cf = self.cond(d, "b")
            A = self.rv(i, ops[0]); Bv = self.rv(i, ops[1], n)
            if n != 4: raise LiftError("narrow sbb")
            op = "-" if m == "sbb" else "+"
            if a in self.defs:   # this sbb / adc itself defines flags a later instruction reads: capture A, B, the carry in
                self.defkind[a] = (m, n)
                self.decls += ["uint32_t fA_%x = 0;" % a, "uint32_t fB_%x = 0;" % a, "uint32_t fK_%x = 0;" % a]
                return ["fA_%x = %s; fB_%x = %s; fK_%x = (%s) ? 1u : 0u;" % (a, A, a, Bv, a, cf),
                        self.wv(i, ops[0], "fA_%x %s fB_%x %s fK_%x" % (a, op, a, op, a))]
            return [self.wv(i, ops[0], "%s %s %s %s (%s ? 1u : 0u)" % (A, op, Bv, op, cf))]
        cc = self.cc_of(m)
        if cc is not None:
            d = self.consumers[a]; c = self.cond(d, cc)
            if m.startswith("j"): return ["if (%s) goto L_%x;" % (c, ops[0].imm)]
            if m.startswith("set"): return [self.wv(i, ops[0], "(%s) ? 1u : 0u" % c)]
            return ["if (%s) %s" % (c, self.wv(i, ops[0], self.rv(i, ops[1])))]
        # ---- control flow
        if m == "jmp":
            op = ops[0]
            if op.type == X.X86_OP_IMM:
                if op.imm in ENTS and op.imm != self.entry and a not in getattr(self, "internal_jmps", ()):
                    return ["LIFT_TAILJMP(%s);  // %s" % (hx(op.imm), ENTS[op.imm])]
                return ["goto L_%x;" % op.imm]
            if a in getattr(self, "tailptr", set()): return ["LIFT_TAILJMP(%s);" % self.rv(i, op, 4)]
            idx, ts = self.tables[a]
            cases = " ".join("case %d: goto L_%x;" % (k, t) for k, t in enumerate(ts))
            return ["switch (%s) { %s default: throw std::runtime_error(\"jump table\"); }" % (idx, cases)]
        if m == "call":
            op = ops[0]; mac = "LIFT_CALLB" if self.entry in BLOCKING else "LIFT_CALL"
            if op.type == X.X86_OP_IMM:
                pre = ["fl = (fl & ~0x8c5u) | FL;"] if a in self.consumers else []   # FLAGS_IN: the callee reads the flags
                if op.imm in NO_RA and mac == "LIFT_CALL":
                    return pre + ["LIFT_SYNC_OUT(); m.call_function(%s, nullptr, 0); LIFT_SYNC_IN();  // %s (keeps the slot it reused)" % (hx(op.imm), ENTS.get(op.imm, ""))]
                return pre + ["%s(%s, %s);  // %s" % (mac, hx(op.imm), hx(a + i.size), ENTS.get(op.imm, ""))]
            return ["%s(%s, %s);" % (mac, self.rv(i, op, 4), hx(a + i.size))]
        if m == "ret":
            return ["LIFT_RET(%d);" % (ops[0].imm if ops else 0)]
        if m == "rep movsd":
            return ["while (ecx) { wr32(m, edi, rd32(m, esi)); esi += 4; edi += 4; ecx--; }"]
        if m == "rep stosd":
            return ["while (ecx) { wr32(m, edi, eax); edi += 4; ecx--; }"]
        if m == "rep movsb":
            return ["while (ecx) { wr8(m, edi, rd8(m, esi)); esi += 1; edi += 1; ecx--; }"]
        if m == "rep stosb":
            return ["while (ecx) { wr8(m, edi, eax & 0xffu); edi += 1; ecx--; }"]
        # (direction flag clear, as the rep forms above)
        if m == "rep stosw":
            return ["while (ecx) { wr16(m, edi, eax & 0xffffu); edi += 2; ecx--; }"]
        if m == "rep movsw":
            return ["while (ecx) { wr16(m, edi, rd16(m, esi)); esi += 2; edi += 2; ecx--; }"]
        if m == "movsb":
            return ["wr8(m, edi, rd8(m, esi)); esi += 1; edi += 1;"]
        if m == "movsw":
            return ["wr16(m, edi, rd16(m, esi)); esi += 2; edi += 2;"]
        if m == "int3": return ['throw std::runtime_error("int3 reached");']
        # ---- x87
        if m.startswith("f"): return self.x87(i)
        raise LiftError("unsupported %x %s %s" % (a, m, i.op_str))

    def cap_logic(self, i, n):
        if i.address in self.defs: self.defkind[i.address] = ("logic", n); return True
        return False

    def x87(self, i):
        m = i.mnemonic; ops = i.operands; a = i.address
        def sti(op):
            k, r = self.regkind(i.reg_name(op.reg)); assert k == "st"; return r
        if m == "fld":
            op = ops[0]
            if op.type == X.X86_OP_REG: return ["{ F80 v_ = st(m, %d); fpush(m, v_); }" % sti(op)]
            if op.size == 4: return ["fpush(m, reflex_f80::from_f32(%s));" % self.rv(i, op, 4)]
            if op.size == 8: return ["fpush(m, reflex_f80::from_f64(%s));" % self.rv(i, op, 8)]
            raise LiftError("fld size")
        if m == "fild":
            return ["fpush(m, reflex_f80::from_int(int64_t(int32_t(sext(%s, %d)))));" % (self.rv(i, ops[0]), ops[0].size)] if ops[0].size <= 4 else \
                   ["fpush(m, reflex_f80::from_int(int64_t(%s)));" % self.rv(i, ops[0], 8)]
        if m == "fld1": return ["fpush(m, reflex_f80::from_int(1));"]
        if m == "fldz": return ["fpush(m, reflex_f80::from_int(0));"]
        if m in ("fst", "fstp"):
            op = ops[0]; pop = ["fpop(m);"] if m == "fstp" else []
            if op.type == X.X86_OP_REG: return ["st(m, %d) = st(m, 0);" % sti(op)] + pop
            if op.size == 4: return [self.wv(i, op, "fst32(m)", 4)] + pop
            if op.size == 8: return [self.wv(i, op, "fst64(m)", 8)] + pop
            raise LiftError("fst size")
        if m in ("fistp", "fist"):
            pop = ["fpop(m);"] if m == "fistp" else []
            return [self.wv(i, ops[0], "uint32_t(fist(m, %d, frc(m)))" % ops[0].size) if ops[0].size <= 4 else
                    self.wv(i, ops[0], "uint64_t(fist(m, 8, frc(m)))", 8)] + pop
        if m == "fxch":
            k = sti(ops[-1]) if ops else 1
            return ["std::swap(st(m, 0), st(m, %d));" % k]
        if m == "fchs": return ["st(m, 0).sign ^= 1;"]
        if m == "fabs": return ["st(m, 0).sign = 0;"]
        if m == "fsqrt": return ["st(m, 0) = reflex_f80::sqrt(st(m, 0), fbits(m), frc(m));"]   # vm.cpp M_fsqrt
        if m in X87_CONST: return ['x87_ldconst(m, "%s");' % X87_CONST[m]]                    # vm.cpp M_fldpi ... M_fldlg2
        if m in X87_TRANS: return ["x87_%s(m);" % m]                                            # vm.cpp M_fpatan ... M_fptan
        if m in ("fcomip", "fucomip", "fcomi", "fucomi", "fcompi", "fucompi"):
            src = sti(ops[-1]) if ops else 1
            out = []
            if a in self.defs:
                self.defkind[a] = ("fcmp", 4); self.decls.append("FCmp fC_%x{};" % a)
                out.append("fC_%x = f80cmp(st(m, 0), st(m, %d));" % (a, src))
            if m.endswith("p") or m.endswith("pi"): out.append("fpop(m);")
            return out
        if m in ("fadd", "fsub", "fmul", "fdiv", "fsubr", "fdivr", "faddp", "fsubp", "fmulp", "fdivp", "fsubrp", "fdivrp",
                 "fiadd", "fimul", "fisub", "fisubr", "fidiv", "fidivr"):
            b = bytes(i.bytes); k = 0
            while b[k] in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67, 0xf2, 0xf3): k += 1
            opc, modrm = b[k], b[k + 1]; reg = (modrm >> 3) & 7
            if modrm >= 0xc0:
                s = modrm & 7
                if opc == 0xd8: return ["st(m, 0) = farith(m, %d, st(m, 0), st(m, %d));" % (reg, s)]
                if opc in (0xdc, 0xde):
                    swap = {0: 0, 1: 1, 4: 5, 5: 4, 6: 7, 7: 6}[reg]
                    out = ["st(m, %d) = farith(m, %d, st(m, %d), st(m, 0));" % (s, swap, s)]
                    if opc == 0xde: out.append("fpop(m);")
                    return out
                raise LiftError("x87 opcode")
            if opc == 0xd8: src = "reflex_f80::from_f32(%s)" % self.rv(i, ops[0], 4)
            elif opc == 0xdc: src = "reflex_f80::from_f64(%s)" % self.rv(i, ops[0], 8)
            elif opc == 0xda: src = "reflex_f80::from_int(int64_t(int32_t(%s)))" % self.rv(i, ops[0], 4)            # m32int (vm.cpp)
            elif opc == 0xde: src = "reflex_f80::from_int(int64_t(int16_t(uint16_t(%s))))" % self.rv(i, ops[0], 2)  # m16int
            else: raise LiftError("x87 int operand")
            return ["st(m, 0) = farith(m, %d, st(m, 0), %s);" % (reg, src)]
        raise LiftError("unsupported x87 %x %s" % (a, m))

def lift(entry, name, desc=""):
    L = Lifter(entry)
    r = L.emit(name, desc)
    if r is None:
        r = L.emit(name, desc)   # labels created by fall-through gotos are now known
    if r is None: raise LiftError("labels")
    return r

if __name__ == "__main__":
    a = int(sys.argv[1], 16); name = sys.argv[2]; desc = sys.argv[3] if len(sys.argv) > 3 else ""
    print("// Generated by the bike lifter from MXReflex.exe; see native/game/notes/bike.md.")
    print('#include "engine_util.h"\n')
    print(lift(a, name, desc))
