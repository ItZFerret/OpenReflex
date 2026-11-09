#!/usr/bin/env python3
"""gen.py <addr> <name> [inline_targets,...] : instruction-level transliteration of one MXReflex.exe function into a
REFLEX_PORT_FN body using ui_util.h (registers in locals, guest stack literal, flags as vm.cpp keeps them)."""
import os, sys, struct, csv
import capstone
from capstone import x86 as X86
import pefile

R = "/Users/payton/Documents/Development/REFLEX/_re"
pe = pefile.PE(os.path.join(R, "..", "game files", "Archive", "MXReflex.exe"), fast_load=True)
img = pe.get_memory_mapped_image(); B = pe.OPTIONAL_HEADER.ImageBase
FUNCS = {}
for l in open(os.path.join(R, "out", "functions.tsv"), encoding="latin-1").readlines()[1:]:
    c = l.split("\t")
    try: FUNCS[int(c[0], 16)] = (c[1], int(c[3]))
    except ValueError: pass
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True

def dec(a):
    return next(md.disasm(bytes(img[a - B:a - B + 16]), a))

def rd32(a): return struct.unpack_from("<I", img, a - B)[0]

CF, PF, AF, ZF, SF, OF = 1, 4, 0x10, 0x40, 0x80, 0x800
FLAGS_IN = {0x8fa3bd}   # as lift.py: callees whose entry reads the flags
NO_RA = {0x8fa554}      # as lift.py: callees that reuse their return-address slot (__SEH_prolog4)
ALL = CF | PF | AF | ZF | SF | OF
ARITH = CF | PF | ZF | SF | OF
JCC = {"jo": OF, "jno": OF, "jb": CF, "jae": CF, "je": ZF, "jne": ZF, "jbe": CF | ZF, "ja": CF | ZF, "js": SF, "jns": SF,
       "jp": PF, "jnp": PF, "jl": SF | OF, "jge": SF | OF, "jle": ZF | SF | OF, "jg": ZF | SF | OF}
GENC = {"o": "c_o(fl)", "no": "!c_o(fl)", "b": "c_b(fl)", "ae": "!c_b(fl)", "e": "c_e(fl)", "ne": "!c_e(fl)", "be": "c_be(fl)",
        "a": "!c_be(fl)", "s": "c_s(fl)", "ns": "!c_s(fl)", "p": "c_p(fl)", "np": "!c_p(fl)", "l": "c_l(fl)", "ge": "!c_l(fl)",
        "le": "c_le(fl)", "g": "!c_le(fl)"}


def _emit(addr, name, group="ui", inline_targets=frozenset()):

    # ---------------------------------------------------------------- CFG
    insns = {}; succ = {}; labels = set(); work = [addr]; tables = {}
    def is_other_entry(t): return t != addr and t in FUNCS and t not in inline_targets
    while work:
        a = work.pop()
        while a not in insns:
            i = dec(a); insns[a] = i; mn = i.mnemonic; nxt = a + i.size
            if mn == "ret":
                succ[a] = []; break
            if mn == "int3":
                succ[a] = []; break
            if mn == "jmp":
                op = i.operands[0]
                if op.type == X86.X86_OP_IMM:
                    t = op.imm
                    if is_other_entry(t): succ[a] = []; break
                    succ[a] = [t]; labels.add(t); work.append(t); break
                if op.type == X86.X86_OP_MEM and op.mem.base == 0 and op.mem.index != 0 and op.mem.scale == 4:
                    tb = op.mem.disp & 0xffffffff; ents = []
                    f0 = addr; f1 = addr + FUNCS.get(addr, ("", 0x4000))[1] + 0x400
                    k = 0
                    while True:
                        v = rd32(tb + 4 * k)
                        if not (f0 <= v < f1) or (k > 0 and tb + 4 * k in insns): break
                        ents.append(v); k += 1
                        if k > 512: break
                    tables[a] = ents
                    for v in set(ents): labels.add(v); work.append(v)
                    succ[a] = list(set(ents)); break
                succ[a] = []; break   # indirect tail jump
            if mn.startswith("j") and mn in JCC:
                t = i.operands[0].imm
                if is_other_entry(t):
                    succ[a] = [nxt]; a = nxt; continue
                succ[a] = [t, nxt]; labels.add(t); work.append(t); a = nxt; continue
            succ[a] = [nxt]; a = nxt

    order = sorted(insns)

    # ---------------------------------------------------------------- flags liveness
    def flag_info(i):
        """(uses, defs) masks as vm.cpp."""
        mn = i.mnemonic
        if mn in JCC: return JCC[mn], 0
        if mn.startswith("set"): return JCC["j" + mn[3:]], 0
        if mn.startswith("cmov"): return JCC["j" + mn[4:]], 0
        if mn in ("add", "sub", "cmp", "neg", "and", "or", "xor", "test"): return 0, ARITH
        if mn in ("adc", "sbb"): return CF, ARITH
        if mn in ("inc", "dec"): return 0, PF | ZF | SF | OF
        if mn in ("shl", "sal", "shr", "sar"):
            if len(i.operands) > 1 and i.operands[1].type == X86.X86_OP_IMM:
                return (0, ARITH) if (i.operands[1].imm & 31) else (0, 0)
            if len(i.operands) == 1: return 0, ARITH
            return ARITH, ARITH  # dynamic count: may keep
        if mn in ("imul", "mul"): return 0, CF | OF
        if mn in ("comiss", "ucomiss", "comisd", "ucomisd"): return 0, ALL
        if mn == "lahf": return SF | ZF | AF | PF | CF, 0
        if mn == "sahf": return 0, SF | ZF | AF | PF | CF
        if mn in ("fcomi", "fcomip", "fucomi", "fucomip", "fcompi", "fucompi"): return 0, CF | PF | ZF
        if mn == "call":   # a callee whose entry reads the caller's flags (lift.FLAGS_IN: __RTC_CheckEsp): ZF is used
            if i.operands[0].type == X86.X86_OP_IMM and i.operands[0].imm in FLAGS_IN: return ZF, ALL
            return 0, ALL
        if mn in ("rep movsd", "rep stosd", "rep movsb", "rep stosb"): return 0x400 and 0, 0
        return 0, 0

    live_in = {a: 0 for a in order}
    changed = True
    while changed:
        changed = False
        for a in reversed(order):
            u, d = flag_info(insns[a])
            lo = 0
            for s in succ[a]:
                lo |= live_in.get(s, 0)
            li = u | (lo & ~d)
            if li != live_in[a]: live_in[a] = li; changed = True
    def live_out(a):
        lo = 0
        for s in succ[a]: lo |= live_in.get(s, 0)
        return lo

    # ---------------------------------------------------------------- operands
    R32 = {"eax": "eax", "ecx": "ecx", "edx": "edx", "ebx": "ebx", "esp": "esp", "ebp": "ebp", "esi": "esi", "edi": "edi"}
    R16 = {"ax": "eax", "cx": "ecx", "dx": "edx", "bx": "ebx", "sp": "esp", "bp": "ebp", "si": "esi", "di": "edi"}
    R8L = {"al": "eax", "cl": "ecx", "dl": "edx", "bl": "ebx"}
    R8H = {"ah": "eax", "ch": "ecx", "dh": "edx", "bh": "ebx"}

    def rn(i, r): return i.reg_name(r)

    def ea(i, op, esp_adj=0):
        m = op.mem; parts = []
        if m.base: parts.append(R32[rn(i, m.base)] + (f" + {esp_adj}" if esp_adj and rn(i, m.base) == "esp" else ""))
        if m.index:
            parts.append(R32[rn(i, m.index)] + (f" * {m.scale}" if m.scale != 1 else ""))
        d = m.disp & 0xffffffff
        if d or not parts:
            if parts and d >= 0x80000000: parts.append(f"- 0x{(0x100000000 - d):x}u")
            else: parts.append(f"0x{d:x}u")
        if m.segment and i.reg_name(m.segment) == "fs": parts.insert(0, "FS_BASE")
        elif m.segment and i.reg_name(m.segment) not in ("ds", "ss", "es"): raise Exception("segment " + i.reg_name(m.segment))
        s = " + ".join(parts).replace("+ -", "-")
        return s if len(parts) == 1 else f"({s})"

    def rget(name):
        if name in R32: return R32[name]
        if name in R16: return f"({R16[name]} & 0xffffu)"
        if name in R8L: return f"({R8L[name]} & 0xffu)"
        if name in R8H: return f"(({R8H[name]} >> 8) & 0xffu)"
        raise Exception("reg " + name)

    def rset(name, v):
        if name in R32: return f"{R32[name]} = {v};"
        if name in R16: r = R16[name]; return f"{r} = ({r} & 0xffff0000u) | (uint32_t({v}) & 0xffffu);"
        if name in R8L: r = R8L[name]; return f"{r} = ({r} & 0xffffff00u) | (uint32_t({v}) & 0xffu);"
        if name in R8H: r = R8H[name]; return f"{r} = ({r} & 0xffff00ffu) | ((uint32_t({v}) & 0xffu) << 8);"
        raise Exception("reg " + name)

    def get(i, op, size=None):
        size = size or op.size
        if op.type == X86.X86_OP_REG: return rget(rn(i, op.reg))
        if op.type == X86.X86_OP_IMM: return f"0x{(op.imm & ((1 << (8 * size)) - 1)):x}u"
        if op.type == X86.X86_OP_MEM:
            return {1: "rd8", 2: "rd16", 4: "rd32"}[size] + f"(m, {ea(i, op)})"
        raise Exception("op")

    def put(i, op, v, size=None):
        size = size or op.size
        if op.type == X86.X86_OP_REG: return rset(rn(i, op.reg), v)
        if op.type == X86.X86_OP_MEM:
            return {1: "wr8", 2: "wr16", 4: "wr32"}[size] + f"(m, {ea(i, op)}, {v});"
        raise Exception("put")

    def isx(i, op): return op.type == X86.X86_OP_REG and rn(i, op.reg).startswith("xmm")
    def xn(i, op): return "x" + rn(i, op.reg)[3:]
    def src32(i, op): return f"{xn(i, op)}.l(0)" if isx(i, op) else (f"rd32(m, {ea(i, op)})" if op.type == X86.X86_OP_MEM else get(i, op, 4))
    def src64(i, op): return f"{xn(i, op)}.lo" if isx(i, op) else f"rd64(m, {ea(i, op)})"
    def src128(i, op): return xn(i, op) if isx(i, op) else f"ldx(m, {ea(i, op)})"

    def mask(n): return {1: "0xffu", 2: "0xffffu", 4: "0xffffffffu"}[n]

    # fused compare: returns C expression for condition cc given defining instruction d
    def fused(d, cc):
        mn = d.mnemonic; o = d.operands
        if mn == "cmp":
            n = o[0].size; a = get(d, o[0]); b = get(d, o[1], n)
            if o[1].type == X86.X86_OP_IMM and n == 4:
                b = f"0x{(o[1].imm & 0xffffffff):x}u"
            sa = {1: "int8_t", 2: "int16_t", 4: "int32_t"}[n]
            ua = {1: "uint8_t", 2: "uint16_t", 4: "uint32_t"}[n]
            A, Bv = f"{ua}({a})", f"{ua}({b})"
            SA, SB = f"{sa}({a})", f"{sa}({b})"
            return {"e": f"{A} == {Bv}", "ne": f"{A} != {Bv}", "b": f"{A} < {Bv}", "ae": f"{A} >= {Bv}", "be": f"{A} <= {Bv}",
                    "a": f"{A} > {Bv}", "l": f"{SA} < {SB}", "ge": f"{SA} >= {SB}", "le": f"{SA} <= {SB}", "g": f"{SA} > {SB}",
                    "s": f"{sa}({ua}({A} - {Bv})) < 0", "ns": f"{sa}({ua}({A} - {Bv})) >= 0"}.get(cc)
        if mn == "test":
            n = o[0].size; a = get(d, o[0]); b = get(d, o[1], n)
            sa = {1: "int8_t", 2: "int16_t", 4: "int32_t"}[n]; ua = {1: "uint8_t", 2: "uint16_t", 4: "uint32_t"}[n]
            v = f"{ua}({a} & {b})" if a != b else f"{ua}({a})"
            return {"e": f"{v} == 0", "ne": f"{v} != 0", "s": f"{sa}({v}) < 0", "ns": f"{sa}({v}) >= 0", "le": f"{sa}({v}) <= 0",
                    "g": f"{sa}({v}) > 0", "l": f"{sa}({v}) < 0", "ge": f"{sa}({v}) >= 0", "p": f"parity8({v})",
                    "np": f"!parity8({v})", "be": f"{v} == 0", "a": f"{v} != 0"}.get(cc)
        if mn in ("comiss", "ucomiss"):
            a = f"bf({xn(d, o[0])}.l(0))"; b = f"bf({src32(d, o[1])})"
            return {"b": f"fb_b({a}, {b})", "ae": f"!fb_b({a}, {b})", "be": f"fb_be({a}, {b})", "a": f"!fb_be({a}, {b})",
                    "e": f"fb_e({a}, {b})", "ne": f"!fb_e({a}, {b})", "p": f"fb_p({a}, {b})", "np": f"!fb_p({a}, {b})"}.get(cc)
        return None

    def flagdef(i):
        """statement updating fl for a flag-defining instruction (given operand values before the op)."""
        raise NotImplementedError

    out = []
    def E(s): out.append("    " + s)

    x87_ops = {0: "0", 1: "1", 4: "4", 5: "5", 6: "6", 7: "7"}

    fused_skip = set()   # defining instructions whose flags are consumed by a fused next instruction
    fuse_at = {}         # consumer addr -> defining insn
    for idx, a in enumerate(order):
        i = insns[a]
        if i.mnemonic in ("cmp", "test", "comiss", "ucomiss"):
            nxt = a + i.size
            if nxt in insns and nxt not in labels and succ[a] == [nxt]:
                c = insns[nxt]; mn = c.mnemonic
                cc = mn[1:] if mn in JCC else (mn[3:] if mn.startswith("set") else None)
                if cc is not None:
                    _, d = flag_info(i)
                    if (live_out(nxt) & d) == 0 and fused(i, cc) is not None:
                        fused_skip.add(a); fuse_at[nxt] = i

    def emit(i):
        a = i.address; mn = {"fcompi": "fcomip", "fucompi": "fucomip"}.get(i.mnemonic, i.mnemonic); o = i.operands; nxt = a + i.size
        lo = live_out(a); _, dmask = flag_info(i)
        need_fl = (lo & dmask) != 0 and a not in fused_skip
        cm = f"// {mn} {i.op_str}".rstrip()
        def S(s): E(f"{s:<100s}{cm}" if len(s) < 100 else s + "  " + cm)
        n = o[0].size if o else 4
        if mn in ("nop", "prefetcht0", "prefetcht1", "prefetcht2", "prefetchnta"): S(";"); return   # prefetch: no architectural effect
        if mn == "int3": S(f"LIFT_BAD(0x{a:08x});"); return
        if mn == "mov":
            S(put(i, o[0], get(i, o[1], n))); return
        if mn in ("movzx", "movsx"):
            v = get(i, o[1])
            if mn == "movsx": v = f"sext({v}, {o[1].size})"
            S(put(i, o[0], v)); return
        if mn == "lea":
            e_ = ea(i, o[1])
            if o[0].type == X86.X86_OP_REG and e_ == R32.get(rn(i, o[0].reg)): S(";"); return
            S(put(i, o[0], e_)); return
        if mn == "push":
            v = get(i, o[0], 4)
            S(f"LIFT_PUSH({v});"); return
        if mn == "pop":
            if o[0].type == X86.X86_OP_REG and rn(i, o[0].reg) == "esp": S("esp = rd32(m, esp);"); return
            if o[0].type == X86.X86_OP_REG and rn(i, o[0].reg) in R32: S(f"{R32[rn(i, o[0].reg)]} = LIFT_POP();"); return
            S(f"{{ uint32_t v_ = LIFT_POP(); {put(i, o[0], 'v_')} }}"); return
        if mn in ("add", "sub", "cmp", "adc", "sbb"):
            a_ = get(i, o[0]); b_ = get(i, o[1], n)
            if o[1].type == X86.X86_OP_IMM: b_ = f"0x{(o[1].imm & ((1 << (8 * n)) - 1)):x}u"
            car = "(fl & F_CF)" if mn in ("adc", "sbb") else "0"
            if not need_fl and mn in ("add", "sub") and n == 4 and o[0].type == X86.X86_OP_REG:
                r_ = R32[rn(i, o[0].reg)]; op = "+" if mn == "add" else "-"
                if o[1].type == X86.X86_OP_IMM and (o[1].imm & 0xffffffff) >= 0x80000000:
                    op = "-" if op == "+" else "+"; b_ = f"0x{(-o[1].imm) & 0xffffffff:x}u"
                S(f"{r_} {op}= {b_};"); return
            st = []
            st.append(f"uint32_t a_ = {a_}, b_ = {b_};")
            if need_fl: st.append(f"uint32_t f_ = f_{'add' if mn in ('add', 'adc') else 'sub'}(fl, a_, b_, {n}, {car});")
            if mn != "cmp":
                op = "+" if mn in ("add", "adc") else "-"
                v = f"a_ {op} b_" + (f" {op} {car}" if car != "0" else "")
                st.append(put(i, o[0], f"({v})" if n == 4 else f"(({v}) & {mask(n)})"))
            if need_fl: st.append("fl = f_;")
            if mn == "cmp" and not need_fl: S(";"); return
            S("{ " + " ".join(st) + " }"); return
        if mn in ("and", "or", "xor", "test"):
            a_ = get(i, o[0]); b_ = get(i, o[1], n)
            if o[1].type == X86.X86_OP_IMM: b_ = f"0x{(o[1].imm & ((1 << (8 * n)) - 1)):x}u"
            op = {"and": "&", "test": "&", "or": "|", "xor": "^"}[mn]
            if mn == "xor" and o[0].type == X86.X86_OP_REG and o[1].type == X86.X86_OP_REG and o[0].reg == o[1].reg:
                S(put(i, o[0], "0u") + (f" fl = f_logic(fl, 0, {n});" if need_fl else "")); return
            st = [f"uint32_t r_ = {a_} {op} {b_};"]
            if mn != "test": st.append(put(i, o[0], "r_"))
            if need_fl: st.append(f"fl = f_logic(fl, r_, {n});")
            if mn == "test" and not need_fl: S(";"); return
            S("{ " + " ".join(st) + " }"); return
        if mn in ("inc", "dec"):
            a_ = get(i, o[0]); op = "+" if mn == "inc" else "-"
            if not need_fl and n == 4 and o[0].type == X86.X86_OP_REG: S(f"{R32[rn(i, o[0].reg)]} {op}= 1;"); return
            st = [f"uint32_t a_ = {a_};"]
            if need_fl: st.append(f"fl = f_{mn}(fl, a_, {n});")
            st.append(put(i, o[0], f"(a_ {op} 1)" if n == 4 else f"((a_ {op} 1) & {mask(n)})"))
            S("{ " + " ".join(st) + " }"); return
        if mn == "neg":
            a_ = get(i, o[0]); st = [f"uint32_t a_ = {a_};"]
            if need_fl: st.append(f"fl = f_sub(fl, 0, a_, {n});")
            st.append(put(i, o[0], "(0u - a_)" if n == 4 else f"((0u - a_) & {mask(n)})"))
            S("{ " + " ".join(st) + " }"); return
        if mn == "not":
            S(put(i, o[0], f"(~{get(i, o[0])})" if n == 4 else f"((~{get(i, o[0])}) & {mask(n)})")); return
        if mn in ("shl", "sal", "shr", "sar"):
            a_ = get(i, o[0])
            c = f"{o[1].imm & 31}u" if len(o) > 1 and o[1].type == X86.X86_OP_IMM else ("1u" if len(o) == 1 else "(ecx & 31u)")
            if mn in ("shl", "sal"): v = f"uint32_t((uint64_t(a_) << c_) & {mask(n)})"; fn = "f_shl"
            elif mn == "shr": v = f"((a_ & {mask(n)}) >> c_)"; fn = "f_shr"
            else: v = f"uint32_t(uint64_t(int64_t(int32_t(sext(a_, {n}))) >> c_) & {mask(n)})"; fn = "f_sar"
            if not need_fl and n == 4 and o[0].type == X86.X86_OP_REG and c != "(ecx & 31u)":
                r_ = R32[rn(i, o[0].reg)]
                if mn in ("shl", "sal"): S(f"{r_} <<= {c};"); return
                if mn == "shr": S(f"{r_} >>= {c};"); return
                S(f"{r_} = uint32_t(int32_t({r_}) >> {c});"); return
            st = [f"uint32_t a_ = {a_}; unsigned c_ = {c};"]
            if need_fl: st.append(f"fl = {fn}(fl, a_, c_, {n});")
            st.append(put(i, o[0], v))
            S("{ " + " ".join(st) + " }"); return
        if mn == "imul":
            if len(o) == 1:
                if n != 4: raise Exception("imul size")
                st = [f"int64_t p_ = int64_t(int32_t(eax)) * int64_t(int32_t({get(i, o[0])}));", "eax = uint32_t(p_); edx = uint32_t(uint64_t(p_) >> 32);"]
            else:
                x = get(i, o[1]) if len(o) == 3 else get(i, o[0])
                y = (f"0x{(o[2].imm & 0xffffffff):x}u" if len(o) == 3 else get(i, o[1]))
                if n != 4: raise Exception("imul size")
                st = [f"int64_t p_ = int64_t(int32_t({x})) * int64_t(int32_t({y}));", put(i, o[0], "uint32_t(p_)")]
            if need_fl: st.append("fl = f_mulover(fl, p_ < INT32_MIN || p_ > INT32_MAX);")
            S("{ " + " ".join(st) + " }"); return
        if mn == "mul":
            if n != 4: raise Exception("mul size")
            st = [f"uint64_t p_ = uint64_t(eax) * uint64_t({get(i, o[0])});", "eax = uint32_t(p_); edx = uint32_t(p_ >> 32);"]
            if need_fl: st.append("fl = f_mulover(fl, edx != 0);")
            S("{ " + " ".join(st) + " }"); return
        if mn in ("div", "idiv"):
            if n != 4: raise Exception("div size")
            if mn == "div":
                S(f"{{ uint64_t n_ = (uint64_t(edx) << 32) | eax, d_ = {get(i, o[0])}; eax = uint32_t(n_ / d_); edx = uint32_t(n_ % d_); }}")
            else:
                S(f"{{ int64_t n_ = int64_t((uint64_t(edx) << 32) | eax), d_ = int32_t({get(i, o[0])}); eax = uint32_t(n_ / d_); edx = uint32_t(n_ % d_); }}")
            return
        if mn == "cdq": S("edx = (eax >> 31) ? 0xffffffffu : 0u;"); return
        if mn == "lahf": S("eax = (eax & 0xffff00ffu) | (((fl & 0xd5u) | 2u) << 8);"); return
        if mn.startswith("set"):
            cc = mn[3:]
            cond = fused(fuse_at[a], cc) if a in fuse_at else GENC[cc]
            S(put(i, o[0], f"(({cond}) ? 1u : 0u)")); return
        if mn in JCC:
            cc = mn[1:]
            cond = fused(fuse_at[a], cc) if a in fuse_at else GENC[cc]
            t = o[0].imm
            if is_other_entry(t): S(f"if ({cond}) LIFT_TAILJMP(0x{t:08x}u);"); return
            S(f"if ({cond}) goto L_{t:08x};"); return
        if mn == "jmp":
            op = o[0]
            if op.type == X86.X86_OP_IMM:
                t = op.imm
                if is_other_entry(t): S(f"LIFT_TAILJMP(0x{t:08x}u);  // tail jump ({FUNCS[t][0]})"); return
                S(f"goto L_{t:08x};"); return
            if a in tables:
                E(f"{{ uint32_t t_ = rd32(m, {ea(i, op)}); switch (t_) {{  {cm}")
                for v in sorted(set(tables[a])): E(f"    case 0x{v:08x}u: goto L_{v:08x};")
                E(f"    default: LIFT_BAD(0x{a:08x}); }} }}")
                return
            S(f"LIFT_TAILJMP({get(i, op, 4)});"); return
        if mn == "call":
            op = o[0]
            if op.type == X86.X86_OP_IMM:
                nm = FUNCS.get(op.imm, ("?",))[0]
                if op.imm in NO_RA: S(f"LIFT_SYNC_OUT(); m.call_function(0x{op.imm:08x}u, nullptr, 0); LIFT_SYNC_IN();  // {nm} (keeps the slot it reused)"); return
                S(f"LIFT_CALL(0x{op.imm:08x}u, 0x{nxt:08x}u);  // {nm}"); return
            S(f"LIFT_CALL({get(i, op, 4)}, 0x{nxt:08x}u);"); return
        if mn == "ret":
            n_ = o[0].imm if o else 0
            S(f"LIFT_RET({n_});"); return
        if mn in ("rep movsd", "rep stosd"):
            if mn == "rep movsd": body = "wr32(m, edi, rd32(m, esi)); esi += st_;"
            else: body = "wr32(m, edi, eax);"
            S(f"{{ int32_t st_ = (fl & F_DF) ? -4 : 4; while (ecx) {{ {body} edi += st_; ecx--; }} }}"); return
        if mn in ("rep movsw", "rep movsb", "rep stosw", "rep stosb"):
            k = 2 if mn[-1] == "w" else 1; w = {2: "wr16", 1: "wr8"}[k]; r = {2: "rd16", 1: "rd8"}[k]
            body = f"{w}(m, edi, {r}(m, esi)); esi += st_;" if "movs" in mn else f"{w}(m, edi, eax);"
            S(f"{{ int32_t st_ = (fl & F_DF) ? -{k} : {k}; while (ecx) {{ {body} edi += st_; ecx--; }} }}"); return
        if mn == "cwde": S("eax = sext(eax & 0xffffu, 2);"); return
        if mn == "sahf": S("fl = (fl & ~0xd5u) | ((eax >> 8) & 0xd5u);"); return
        if mn in ("cld",): S("fl &= ~F_DF;"); return
        if mn == "leave": S("esp = ebp; ebp = LIFT_POP();"); return
        if mn == "xchg":
            a_ = get(i, o[0]); b_ = get(i, o[1])
            S("{ uint32_t a_ = " + a_ + ", b_ = " + b_ + "; " + put(i, o[0], "b_") + " " + put(i, o[1], "a_") + " }"); return
        if mn.startswith("cmov"):
            cc = mn[4:]
            S(f"if ({GENC[cc]}) {{ uint32_t v_ = {get(i, o[1], 4)}; {put(i, o[0], 'v_')} }}"); return
        if mn in ("fnstsw",):
            S(put(i, o[0], "(uint32_t(m.fpu_sw & ~0x3800u) | (uint32_t((8 - m.fpu.size()) % 8) << 11))", 2)); return
        if mn in ("fcomi", "fcomip", "fucomi", "fucomip"):
            k = rn(i, o[-1].reg)[3] if o else "1"
            st_ = [f"int c_ = reflex_f80::compare(st(m, 0), st(m, {k}));"]
            st_.append("fl = (fl & ~(F_CF | F_PF | F_ZF)) | (c_ == 2 ? (F_CF | F_PF | F_ZF) : c_ < 0 ? F_CF : c_ == 0 ? F_ZF : 0u);")
            if mn.endswith("p"): st_.append("fpop(m);")
            S("{ " + " ".join(st_) + " }"); return
        if mn in ("fcom", "fcomp", "fcompp", "fucom", "fucomp", "fucompp", "ftst"):
            if mn == "ftst": b_ = "reflex_f80::from_int(0)"
            elif o and o[-1].type == X86.X86_OP_MEM: b_ = f"f80_m{o[-1].size * 8}(m, {ea(i, o[-1])})"
            elif o: b_ = f"st(m, {rn(i, o[-1].reg)[3]})"
            else: b_ = "st(m, 1)"
            pops = 2 if mn.endswith("pp") else 1 if mn.endswith("p") else 0
            st_ = [f"int c_ = reflex_f80::compare(st(m, 0), {b_});",
                   "m.fpu_sw = uint16_t((m.fpu_sw & ~0x4500u) | (c_ == 2 ? 0x4500u : c_ < 0 ? 0x100u : c_ == 0 ? 0x4000u : 0u));"]
            st_ += ["fpop(m);"] * pops
            S("{ " + " ".join(st_) + " }"); return
        if mn == "movq":
            if isx(i, o[0]): S(f"{xn(i, o[0])} = X{{{src64(i, o[1])}, 0}};")
            else: S(f"wr64(m, {ea(i, o[0])}, {xn(i, o[1])}.lo);")
            return
        if mn in ("pxor", "por", "pand", "pandn"):
            d = xn(i, o[0]); f = {"pxor": "xorps", "por": "orps", "pand": "andps", "pandn": "andnps"}[mn]
            if mn == "pxor" and isx(i, o[1]) and o[0].reg == o[1].reg: S(f"{d} = X{{0, 0}};"); return
            S(f"{d} = {f}({d}, {src128(i, o[1])});"); return
        # ---------------- SSE
        if mn in ("movaps", "movups", "movapd", "movupd", "movdqa", "movdqu"):
            if isx(i, o[0]): S(f"{xn(i, o[0])} = {src128(i, o[1])};")
            else: S(f"stx(m, {ea(i, o[0])}, {xn(i, o[1])});")
            return
        if mn == "movss":
            if isx(i, o[0]):
                if isx(i, o[1]): S(f"{xn(i, o[0])} = movss_r({xn(i, o[0])}, {xn(i, o[1])});")
                else: S(f"{xn(i, o[0])} = movss_m(rd32(m, {ea(i, o[1])}));")
            else: S(f"wr32(m, {ea(i, o[0])}, {xn(i, o[1])}.l(0));")
            return
        if mn == "movsd":
            if isx(i, o[0]):
                if isx(i, o[1]): S(f"{xn(i, o[0])} = movsd_r({xn(i, o[0])}, {xn(i, o[1])});")
                else: S(f"{xn(i, o[0])} = movsd_m(rd64(m, {ea(i, o[1])}));")
            else: S(f"wr64(m, {ea(i, o[0])}, {xn(i, o[1])}.lo);")
            return
        if mn == "movd":
            if isx(i, o[0]): S(f"{xn(i, o[0])} = movd({get(i, o[1], 4)});")
            else: S(put(i, o[0], f"{xn(i, o[1])}.l(0)", 4))
            return
        for kind, srcf in (("ss", src32), ("sd", src64)):
            for op_ in ("add", "sub", "mul", "div", "min", "max"):
                if mn == op_ + kind:
                    d = xn(i, o[0]); S(f"{d} = {op_}{kind}({d}, {srcf(i, o[1])});"); return
        for op_ in ("add", "sub", "mul", "div", "min", "max"):
            if mn in (op_ + "ps", op_ + "pd"):
                d = xn(i, o[0]); S(f"{d} = {mn}({d}, {src128(i, o[1])});"); return
        if mn in ("andps", "andnps", "orps", "xorps", "andpd", "andnpd", "orpd", "xorpd", "unpcklps", "unpckhps", "movlhps", "movhlps", "unpcklpd", "unpckhpd"):
            d = xn(i, o[0]); f = mn.replace("pd", "ps") if mn in ("andpd", "andnpd", "orpd", "xorpd") else mn
            if mn in ("xorps", "xorpd") and isx(i, o[1]) and o[0].reg == o[1].reg: S(f"{d} = X{{0, 0}};"); return
            S(f"{d} = {f}({d}, {src128(i, o[1])});"); return
        if mn in ("shufps", "shufpd"):
            d = xn(i, o[0]); S(f"{d} = {mn}({d}, {src128(i, o[1])}, 0x{o[2].imm:x});"); return
        if mn == "pshufd":
            d = xn(i, o[0]); S(f"{d} = pshufd({src128(i, o[1])}, 0x{o[2].imm:x});"); return
        if mn == "sqrtps": d = xn(i, o[0]); S(f"{d} = sqrtps({d}, {src128(i, o[1])});"); return
        if mn == "sqrtss": d = xn(i, o[0]); S(f"{d} = sqrtss({d}, {src32(i, o[1])});"); return
        if mn == "sqrtsd": d = xn(i, o[0]); S(f"{d} = sqrtsd({d}, {src64(i, o[1])});"); return
        CMPP = {"eq": 0, "lt": 1, "le": 2, "unord": 3, "neq": 4, "nlt": 5, "nle": 6, "ord": 7}
        for p, k in CMPP.items():
            if mn == f"cmp{p}ps": d = xn(i, o[0]); S(f"{d} = cmpps<{k}>({d}, {src128(i, o[1])});"); return
            if mn == f"cmp{p}ss": d = xn(i, o[0]); S(f"{d} = cmpss<{k}>({d}, {src32(i, o[1])});"); return
        if mn in ("comiss", "ucomiss"):
            if need_fl: S(f"fl = f_comiss(fl, {xn(i, o[0])}.l(0), {src32(i, o[1])});")
            else: S(";")
            return
        if mn in ("comisd", "ucomisd"):
            if need_fl: S(f"fl = f_comisd(fl, {xn(i, o[0])}.lo, {src64(i, o[1])});")
            else: S(";")
            return
        if mn == "cvtss2sd": d = xn(i, o[0]); S(f"{d} = cvtss2sd({d}, {src32(i, o[1])});"); return
        if mn == "cvtsd2ss": d = xn(i, o[0]); S(f"{d} = cvtsd2ss({d}, {src64(i, o[1])});"); return
        if mn == "cvtps2pd": d = xn(i, o[0]); S(f"{d} = cvtps2pd({src64(i, o[1])});"); return
        if mn == "cvtpd2ps": d = xn(i, o[0]); S(f"{d} = cvtpd2ps({src128(i, o[1])});"); return
        if mn == "cvtdq2ps": d = xn(i, o[0]); S(f"{d} = cvtdq2ps({src128(i, o[1])});"); return
        if mn in ("cvttps2dq", "cvtps2dq"): d = xn(i, o[0]); S(f"{d} = {mn}({src128(i, o[1])});"); return
        if mn == "cvtsi2ss": d = xn(i, o[0]); S(f"{d} = cvtsi2ss({d}, {get(i, o[1], 4)});"); return
        if mn == "cvtsi2sd": d = xn(i, o[0]); S(f"{d} = cvtsi2sd({d}, {get(i, o[1], 4)});"); return
        if mn in ("cvttss2si", "cvtss2si"): S(put(i, o[0], f"{mn}({src32(i, o[1])})")); return
        if mn in ("cvttsd2si", "cvtsd2si"): S(put(i, o[0], f"{mn}({src64(i, o[1])})")); return
        if mn == "movmskps": S(put(i, o[0], f"movmskps({xn(i, o[1])})")); return
        # ---------------- x87
        if mn in ("fld", "fild", "fld1", "fldz", "fst", "fstp", "fistp", "fist", "fchs", "fabs", "fxch") or \
           mn in ("fadd", "fsub", "fmul", "fdiv", "fsubr", "fdivr", "faddp", "fsubp", "fmulp", "fdivp", "fsubrp", "fdivrp"):
            if mn == "fld":
                op = o[0]
                if op.type == X86.X86_OP_REG: S(f"fpush_copy(m, {rn(i, op.reg)[3]});"); return
                S(f"fpush(m, f80_m{op.size * 8}(m, {ea(i, op)}));"); return
            if mn == "fild":
                op = o[0]; sz = op.size
                v = {2: f"f80_i16(rd16(m, {ea(i, op)}))", 4: f"f80_i32(rd32(m, {ea(i, op)}))", 8: f"f80_i64(rd64(m, {ea(i, op)}))"}[sz]
                S(f"fpush(m, {v});"); return
            if mn == "fld1": S("fpush(m, reflex_f80::from_int(1));"); return
            if mn == "fldz": S("fpush(m, reflex_f80::from_int(0));"); return
            if mn in ("fst", "fstp"):
                op = o[0]; pop = " fpop(m);" if mn == "fstp" else ""
                if op.type == X86.X86_OP_REG:
                    k = rn(i, op.reg)[3]
                    S(f"{{ F80 v_ = st(m, 0); st(m, {k}) = v_;{pop} }}"); return
                if op.size == 4: S(f"wr32(m, {ea(i, op)}, fst32(m));{pop}"); return
                if op.size == 8: S(f"wr64(m, {ea(i, op)}, fst64(m));{pop}"); return
                raise Exception("fst size")
            if mn in ("fistp", "fist"):
                op = o[0]; pop = " fpop(m);" if mn == "fistp" else ""
                w = {2: "wr16", 4: "wr32", 8: "wr64"}[op.size]
                S(f"{w}(m, {ea(i, op)}, uint64_t(fist(m, {op.size}, frc(m))));{pop}"); return
            if mn == "fchs": S("st(m, 0).sign ^= 1;"); return
            if mn == "fabs": S("st(m, 0).sign = 0;"); return
            if mn == "fxch":
                k = rn(i, o[-1].reg)[3] if o else "1"
                S(f"std::swap(st(m, 0), st(m, {k}));"); return
            # arithmetic: decode as vm.cpp (opcode bytes)
            b = bytes(i.bytes); k = 0
            while b[k] in (0x26, 0x2e, 0x36, 0x3e, 0x64, 0x65, 0x66, 0x67, 0xf2, 0xf3): k += 1
            opc, modrm = b[k], b[k + 1]; reg = (modrm >> 3) & 7
            if modrm >= 0xc0:
                r_ = modrm & 7
                if opc == 0xd8: S(f"st(m, 0) = farith(m, {reg}, st(m, 0), st(m, {r_}));"); return
                if opc in (0xdc, 0xde):
                    sw = {0: 0, 1: 1, 4: 5, 5: 4, 6: 7, 7: 6}[reg]
                    S(f"st(m, {r_}) = farith(m, {sw}, st(m, {r_}), st(m, 0));" + (" fpop(m);" if opc == 0xde else "")); return
                raise Exception("x87 opcode")
            op = o[-1]
            if opc == 0xd8: s_ = f"f80_m32(m, {ea(i, op)})"
            elif opc == 0xdc: s_ = f"f80_m64(m, {ea(i, op)})"
            elif opc == 0xda: s_ = f"f80_i32(rd32(m, {ea(i, op)}))"
            elif opc == 0xde: s_ = f"f80_i16(rd16(m, {ea(i, op)}))"
            else: raise Exception("x87 opcode")
            S(f"st(m, 0) = farith(m, {reg}, st(m, 0), {s_});"); return
        raise Exception(f"unhandled {a:08x} {mn} {i.op_str}")

    # fallthroughs into non-adjacent code need a goto and a label
    ft_goto = {}
    for idx, a in enumerate(order):
        i = insns[a]
        if succ[a] and i.mnemonic not in ("jmp", "ret"):
            nx = a + i.size
            if nx in insns and (idx + 1 >= len(order) or order[idx + 1] != nx):
                ft_goto[a] = nx; labels.add(nx)
    # the entry is not the lowest address (code below it reached by an internal jmp): start with a jump to the entry block
    entry_first = order[0] == addr
    if not entry_first: E(f"goto L_{addr:08x};")
    for a in order:
        i = insns[a]
        if a in labels and a != addr: out.append(f"L_{a:08x}:")
        elif a == addr and not entry_first: out.append(f"L_{a:08x}:")
        emit(i)
        if a in ft_goto: E(f"goto L_{ft_goto[a]:08x};")
    if addr in labels and entry_first: out.insert(0, f"L_{addr:08x}:")
    _full = ("// %08x %s\n" % (addr, name)) + ('REFLEX_PORT_FN(0x%08x, %s, "%s", LIFT_OUTS) {\n    LIFT_ENTRY();\n' % (addr, name, group)) + "\n".join(out) + "\n}\n"
    _body = "\n".join(out)
    return _full, _body

def lift_full(addr, name, group="ui", inline_targets=frozenset()):
    return _emit(addr, name, group, inline_targets)[0]

def lift_body(addr, name, group="ui", inline_targets=frozenset()):
    return _emit(addr, name, group, inline_targets)[1]

if __name__ == "__main__":
    a = int(sys.argv[1], 16); nm = sys.argv[2]
    inl = frozenset(int(x, 16) for x in sys.argv[3].split(",")) if len(sys.argv) > 3 and sys.argv[3] else frozenset()
    print('#include "lift_rt.h"\n')
    print(lift_full(a, nm, sys.argv[4] if len(sys.argv) > 4 else "ui", inl))
