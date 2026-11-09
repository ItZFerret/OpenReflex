#!/usr/bin/env python3
"""build_globals_gen.py -- translates the static initializers of MXReflex.exe (the _initterm table 0x009166e4 .. 0x00917660) and the
small constructors they call to readable C: core/game/build_globals_gen.c (unit 12b, core/notes/construction.md section 14).

Each initializer is a few instructions: stores of constants, copies of constants between globals, SSE vector constants, constructor
calls of global objects (KeyMap, ConVar, tables ...), guard bits of function-local statics and `_atexit` registrations. The
translator runs the instructions symbolically (registers hold constants, address expressions or values loaded at their place in the
instruction stream) and writes the stores and calls it finds as C over the game memory model (rx_mem.h). A callee is either one of
the core's hand-written ports (CALLEES: address -> C name and the registers / stack slots the original passes its arguments in) or a
straight-line function that is translated the same way (its parameters are the registers it reads before writing and the stack slots
above its return address; `vector constructor iterator' calls become loops). What it cannot translate (a call to code without an
entry, a loop, the address of a stack object) is reported and left to the hand-written files (build_globals_*.c).
  usage: build_globals_gen.py <MXReflex.exe> <functions.tsv> <out.c> [--hand a,b] [--func addr:name,..] [--list]
Checked against the original at every step: tools/build_globals_test + build_globals_cmp.py (the state at ReflexWinMain) and per call in
the oracle (oracle/build_globals_ports.cpp). Not part of the product build."""
import argparse, collections, re, struct, sys

try:
    import capstone
    from capstone import x86 as cx
except ImportError:
    sys.exit('capstone is needed (_re/.venv/bin/python)')

BASE = 0x400000
TABLE_LO, TABLE_HI = 0x9166e4, 0x917660

# ---------------------------------------------------------------------------------------------------------------------------
# hand-written callees: address -> (C name, argument sources in C parameter order, bytes the callee pops, comment, returns eax)
# a source is a register ('eax' 'ecx' 'edx' 'esi' 'edi' 'ebx') or a stack slot ('s0' = the last pushed argument, 's1' ...)
# ---------------------------------------------------------------------------------------------------------------------------
CALLEES = {}

def callee(addr, name, args, pop=0, note='', ret=False):
    CALLEES[addr] = (name, args, pop, note, ret)

callee(0x8f9fbf, None, ['s0'], 0, '_atexit')
callee(0x8f9efe, '', ['ecx'], 0, '__security_check_cookie')      # (stack protection: nothing to do)                      # skipped: the destructor is never run
callee(0x78fbe0, 'rx_reflex_aligned_alloc', ['s0'], 0, 'ReflexAlignedAlloc', ret=True)
callee(0x811230, 'rx_keymap_ctor', ['esi', 's0', 's1'], 8, 'KeyMap::KeyMap(name, id)')
callee(0x417230, 'rx_convar_ctor', ['esi', 'eax', 's0'], 4, 'ConVar::ConVar(name, value)')
callee(0x402be0, 'rx_convar_list_init', [], 0, 'ConVar list: first construction')
callee(0x402b40, 'rx_convar_register', ['eax', 's0'], 4, 'ConVar list: add')
callee(0x812170, 'rx_input_names_init', [], 0, 'InputNames_Init')
callee(0x812e40, 'rx_input_names_set_pad', ['eax', 's0'], 4, 'InputNames_SetPadName(id, name)')

# imports called through the IAT: slot -> (C name, args, comment, bytes popped)
IAT = {
    0x916014: ('rx_critical_section_init', ['s0'], 'InitializeCriticalSection', 0, False),
    0x9161a4: ('crt_str_ctor', ['ecx'], 'std::string()', 0, False),
    0x9161ac: ('crt_str_ctor_cstr', ['ecx', 's0'], 'std::string(const char*)', 4, False),
    0x9161b0: ('crt_str_ctor_copy', ['ecx', 's0'], 'std::string(const std::string&)', 4, False),
    0x9161b4: ('crt_str_dtor', ['ecx'], '~std::string()', 0, False),
    0x916260: ('crt_strncpy', ['s0', 's1', 's2'], 'strncpy', 0, False),
    0x916204: ('rx_sprintf_su', ['s0', 's2', 's3'], 'sprintf', 0, False),      # (the "%s%u" form only: do_call checks the format)
    0x9162bc: ('crt_stricmp', ['s0', 's1'], '_stricmp', 0, True),
    0x916414: ('crt_strnicmp', ['s0', 's1', 's2'], '_strnicmp', 0, True),
    0x91621c: ('crt_strstr', ['s0', 's1'], 'strstr', 0, True),
    0x916220: ('crt_atoi', ['s0'], 'atoi', 0, True),
    0x916234: ('crt_strncmp', ['s0', 's1', 's2'], 'strncmp', 0, True),
    0x916248: ('crt_strchr', ['s0', 's1'], 'strchr', 0, True),
    0x916298: ('crt_memset', ['s0', 's1', 's2'], 'memset', 0, True),
    0x91625c: ('crt_tolower', ['s0'], 'tolower', 0, True),
    0x916224: ('', [], '_invalid_parameter_noinfo (a checked-iterator failure path: never taken)', 0, False),
    0x916364: ('crt_strcpy_s', ['s0', 's1', 's2'], 'strcpy_s', 0, True),
}

# indirect calls (virtual calls through a vtable the generator does not follow): call site -> (C name, args, bytes popped, comment, returns eax)
INDIRECT = {
    0x8434d3: ('rx_reflex_aligned_alloc', ['s0'], 4, 'StdMemoryAllocator::vf1 004eef20 (= ReflexAlignedAlloc)', True),
}
VECTOR_CTOR_ITERATOR = 0x4022d0
HAND = set(range(0x903b20, 0x904161, 0x10))   # (the table objects 00903b20 .. 00904160 are build_tables_init.c's)

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
md.detail = True

class Unsupported(Exception):
    pass

class NeedFrame(Exception):
    """the function takes the address of a stack object: translate it again with its stack frame in game memory"""

REGS = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
R8 = {'al': 'eax', 'cl': 'ecx', 'dl': 'edx', 'bl': 'ebx'}
R16 = {'ax': 'eax', 'cx': 'ecx', 'dx': 'edx', 'bx': 'ebx'}

def reg32(r):
    n = md.reg_name(r)
    if n in REGS: return n
    if n in R8: return R8[n]
    if n in R16: return R16[n]
    raise Unsupported('register ' + n)

class V:
    """A value: 'c' constant, 'a' address / expression base + off (base None = absolute), 'iat' an import slot, 'u' unknown."""
    __slots__ = ('k', 'v', 'off')
    def __init__(self, k, v=None, off=0): self.k, self.v, self.off = k, v, off
    def is_c(self): return self.k == 'c'
    def __repr__(self): return '%s:%s+%s' % (self.k, self.v, self.off)

def C(x): return V('c', x & 0xffffffff)
def E(s): return V('a', s, 0)
U = lambda: V('u')

def hexlit(x):
    x &= 0xffffffff
    return '0x%08x' % x if x > 0xffff else ('0x%x' % x if x > 9 else '%d' % x)

class Fn:
    """A translated function."""
    def __init__(self, addr):
        self.addr = addr; self.name = 'rx_fn_%08x' % addr
        self.params = []        # [('reg','ecx') | ('stack', k)]
        self.pop = 0            # bytes the callee pops (ret n)
        self.clobbers = set()   # registers whose value differs from the entry value at a ret
        self.xclob = set()      # xmm registers the function writes
        self.retreg = 'eax'     # where the result is: eax, or xmm0 lane 0
        self.returns = False
        self.lines = []

class Tr:
    def __init__(self, gen, addr, size, frame=0):
        self.gen = gen
        self.addr = addr
        self.frame = frame          # 0: stack slots are virtual; else the size of the stack frame kept in game memory (F)
        self.maxdepth = 0
        code = gen.img[addr - BASE: addr - BASE + max(size, 16) + 64]
        self.ins = []
        for i in md.disasm(code, addr):
            self.ins.append(i)
            if i.mnemonic == 'ret' and not self.branch_past(i):
                break
        self.by_addr = {j.address: j for j in self.ins}
        self.out = []
        self.ind = 1
        self.t = 0
        self.regs = {r: U() for r in REGS}
        for r in ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi', 'ebp'): self.regs[r] = E('in_' + r)
        self.xmm = {}
        self.fpu = []
        self.stack = []
        self.stack_offs = []        # the frame offset each pushed value lives at (a later store into the slot changes the argument)
        self.slots = {}
        self.esp = 0
        self.ebp_off = None
        self.flags = None
        self.pop = 0
        self.ret_val = None
        self.rets = []
        self.nphi = 0
        self.xwritten = set()
        self.xrets = []
        self.clobbers = set()
        self.phis = {}
        self.nblk = 0

    def note_ret_regs(self):
        x0 = self.xmm.get(0)
        self.xrets.append(x0[0] if x0 and 0 in self.xwritten and x0[0].k in ('c', 'a') else None)
        for r in ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi'):
            v = self.regs[r]
            if not (v.k == 'a' and v.v == 'in_' + r and v.off == 0): self.clobbers.add(r)

    def branch_past(self, ret):
        """a forward jump past this ret keeps the function going"""
        for j in self.ins:
            if j.mnemonic.startswith('j') and j.operands[0].type == cx.X86_OP_IMM and j.operands[0].imm > ret.address: return True
        return False

    # ---- helpers
    def emit(self, s): self.out.append('    ' * self.ind + s)
    def tmp(self): self.t += 1; return 'v%d' % (self.t - 1)

    def expr(self, val):
        if val.k == 'c': return hexlit(val.v)
        if val.k == 'a':
            if val.off == 0: return val.v
            return '(%s %s %s)' % (val.v, '+' if val.off > 0 else '-', hexlit(abs(val.off)))
        raise Unsupported('unknown value used')

    def addr_of(self, op):
        """-> ('abs', addr) | ('slot', frame offset) | ('mem', base expression, disp)"""
        m = op.mem
        b = md.reg_name(m.base) if m.base else None
        ix = md.reg_name(m.index) if m.index else None
        if b is None and ix is None: return ('abs', m.disp & 0xffffffff)
        if ix is None and b == 'esp': return ('slot', self.esp + m.disp)
        if ix is None and b == 'ebp' and self.ebp_off is not None: return ('slot', self.ebp_off + m.disp)
        if ix is None:
            base = self.regs[reg32(m.base)]
            if base.k == 'c': return ('abs', (base.v + m.disp) & 0xffffffff)
            if base.k == 'a': return ('mem', base.v if base.v else '0', base.off + m.disp)
            raise Unsupported('address from an unknown register')
        # base + index * scale + disp
        bi = self.regs[reg32(m.index)]
        sc = m.scale
        if b is None:
            if bi.k == 'a' and bi.v: return ('mem', '(%s * %d)' % (self.expr(V('a', bi.v, 0)), sc), m.disp + bi.off * sc)
            raise Unsupported('indexed address')
        bb = self.regs[reg32(m.base)]
        if bb.k == 'c' and bi.k == 'a' and bi.v: return ('mem', '(%s * %d)' % (self.expr(V('a', bi.v, 0)), sc), bb.v + m.disp + bi.off * sc)
        if bb.k == 'a' and bi.k == 'a' and bb.v and bi.v:
            return ('mem', '(%s + %s * %d)' % (self.expr(V('a', bb.v, 0)), self.expr(V('a', bi.v, 0)), sc), bb.off + m.disp + bi.off * sc)
        raise Unsupported('indexed address')

    def fslot(self, off):
        """frame mode: the offset of a slot inside the frame block (the entry esp is at the frame's top)"""
        self.maxdepth = max(self.maxdepth, -off)
        return self.frame + off

    def slot_read(self, off, size=4):
        if off >= 4 and off % 4 == 0 and off - 4 < 4 * 16:
            return E('a%d' % ((off - 4) // 4))      # a stack argument above the return address
        if self.frame:
            t = self.tmp(); self.emit('uint32_t %s = U32(F, %s);' % (t, hexlit(self.fslot(off)))); return E(t)
        if off in self.slots: return self.slots[off]
        return U()

    def load32(self, op):
        k = self.addr_of(op)
        if k[0] == 'abs':
            if k[1] in IAT: return V('iat', k[1])
            t = self.tmp(); self.emit('uint32_t %s = U32(0, %s);' % (t, hexlit(k[1]))); return E(t)
        if k[0] == 'slot': return self.slot_read(k[1])
        t = self.tmp(); self.emit('uint32_t %s = U32(%s, %s);' % (t, k[1], hexlit(k[2]))); return E(t)

    def store(self, op, val, size=4):
        k = self.addr_of(op)
        fn = {4: 'SU32', 2: 'SU16', 1: 'SU8'}[size]
        if k[0] == 'abs': self.emit('%s(0, %s, %s);' % (fn, hexlit(k[1]), self.expr(val)))
        elif k[0] == 'slot': self.slot_write(k[1], val, fn)
        else: self.emit('%s(%s, %s, %s);' % (fn, k[1], hexlit(k[2]), self.expr(val)))

    def sp_pop(self):
        self.stack_offs.pop()
        return self.stack.pop()

    def slot_write(self, off, val, fn='SU32'):
        if fn == 'SU32' and off in self.stack_offs: self.stack[self.stack_offs.index(off)] = val
        if self.frame and val.k == 'u': self.fslot(off)           # (a saved register: nothing the function reads back)
        elif self.frame: self.emit('%s(F, %s, %s);' % (fn, hexlit(self.fslot(off)), self.expr(val)))
        else: self.slots[off] = val

    def val(self, op):
        if op.type == cx.X86_OP_REG: return self.regs[reg32(op.reg)]
        if op.type == cx.X86_OP_IMM: return C(op.imm)
        if op.type == cx.X86_OP_MEM: return self.load32(op)
        raise Unsupported('operand')

    def setreg(self, op, v): self.regs[reg32(op.reg)] = v

    # ---- xmm lanes
    def xlane(self, op):
        n = md.reg_name(op.reg)
        if n.startswith('xmm'):
            k = int(n[3:])
            if k not in self.xmm: self.xmm[k] = [E('in_xmm%d' % k), U(), U(), U()]       # an input register (LTCG): lane 0 is a parameter
            return list(self.xmm[k])
        raise Unsupported('xmm operand')

    def xload(self, op, lanes):
        k = self.addr_of(op); out = []
        if k[0] == 'abs':
            for l in range(lanes):
                t = self.tmp(); self.emit('uint32_t %s = U32(0, %s);' % (t, hexlit(k[1] + 4 * l))); out.append(E(t))
            return out
        if k[0] == 'slot': return [self.slot_read(k[1] + 4 * l) for l in range(lanes)]
        for l in range(lanes):
            t = self.tmp(); self.emit('uint32_t %s = U32(%s, %s);' % (t, k[1], hexlit(k[2] + 4 * l))); out.append(E(t))
        return out

    def xstore(self, op, lanes_v):
        k = self.addr_of(op)
        for l, v in enumerate(lanes_v):
            if k[0] == 'abs': self.emit('SU32(0, %s, %s);' % (hexlit(k[1] + 4 * l), self.expr(v)))
            elif k[0] == 'slot': self.slot_write(k[1] + 4 * l, v)
            else: self.emit('SU32(%s, %s, %s);' % (k[1], hexlit(k[2] + 4 * l), self.expr(v)))

    def setx(self, op, lanes):
        k = int(md.reg_name(op.reg)[3:]); self.xmm[k] = lanes; self.xwritten.add(k)

    def fexpr(self, v):
        if v.k in ('c', 'a'): return 'rx_bf(%s)' % self.expr(v)
        raise Unsupported('unknown float value')

    def float_op(self, sym, a, b):
        t = self.tmp()
        self.emit('uint32_t %s = rx_fb(%s %s %s);' % (t, self.fexpr(a), sym, self.fexpr(b)))
        return E(t)

    # ---- calls
    def arg_values(self, spec, regs):
        vals = []
        for a in spec:
            if a.startswith('s'):
                idx = len(self.stack) - 1 - int(a[1:])
                if idx < 0: raise Unsupported('stack argument missing')
                vals.append(self.stack[idx])
            else: vals.append(regs[a])
        return vals

    def resolve(self, target):
        """-> ('hand', entry) | ('auto', Fn) for a direct call target"""
        if target in CALLEES: return ('hand', CALLEES[target])
        code = self.gen.img[target - BASE:target - BASE + 6]
        if code[:2] == b'\xff\x25' and struct.unpack('<I', code[2:6])[0] in IAT:       # an import thunk: jmp [slot]
            name, spec, note, pop, iret = IAT[struct.unpack('<I', code[2:6])[0]]
            return ('hand', (name, spec, pop, note, iret))
        return ('auto', self.gen.compile(target))     # may raise Unsupported

    def call_text(self, kind, ent, target, regs):
        """-> (statement text, has_return, pop)"""
        if kind == 'hand':
            name, spec, pop, note, hasret = ent
            if name is None: return None, False, 0
            if name == '': return '', False, pop
            vals = self.arg_values(spec, regs)
            txt = '%s(%s)' % (name, ', '.join(self.expr(v) for v in vals))
            return (txt + (';  /* %08x %s */' % (target, note) if note else ';')), hasret, pop
        fn = ent
        vals = []
        for p in fn.params:
            if p[0] == 'reg': vals.append(regs[p[1]])
            elif p[0] == 'xmm': vals.append(self.xmm[p[1]][0] if p[1] in self.xmm else U())
            else:
                idx = len(self.stack) - 1 - p[1]
                if idx < 0: raise Unsupported('stack argument missing for %08x' % fn.addr)
                vals.append(self.stack[idx])
        txt = '%s(%s)' % (fn.name, ', '.join(self.expr(v) for v in vals))
        return txt + ';  /* %08x */' % fn.addr, fn.returns, fn.pop

    def do_call(self, i):
        op = i.operands[0]
        regs = {r: self.regs[r] for r in REGS}
        if op.type == cx.X86_OP_IMM:
            target = op.imm
            if target == VECTOR_CTOR_ITERATOR: return self.vector_ctor_iterator()
            kind, ent = self.resolve(target)
        else:
            if op.type == cx.X86_OP_REG and self.regs[reg32(op.reg)].k == 'iat': slot = self.regs[reg32(op.reg)].v
            elif op.type == cx.X86_OP_MEM and op.mem.base == 0 and op.mem.index == 0: slot = op.mem.disp & 0xffffffff
            elif i.address in INDIRECT:
                kind, ent, target = 'hand', INDIRECT[i.address], i.address
                slot = None
            else: raise Unsupported('indirect call')
            if slot == 0x916204:        # sprintf(buf, "%s%u", s, u): the only format the initializers use (the CRT's own needs per-thread data)
                buf, fmt, sarg, uarg = self.arg_values(['s0', 's1', 's2', 's3'], self.regs)
                if not fmt.is_c() or self.gen.cstr(fmt.v) != '%s%u': raise Unsupported('sprintf with another format')
                self.emit('rx_sprintf_su(%s, %s, %s);  /* sprintf(buf, "%%s%%u", name, n) */' % (self.expr(buf), self.expr(sarg), self.expr(uarg)))
                for r in ('eax', 'ecx', 'edx'): self.regs[r] = U()
                return
            if slot is not None:
                if slot not in IAT: raise Unsupported('import call [%08x]' % slot)
                name, spec, note, pop, iret = IAT[slot]
                kind, ent, target = 'hand', (name, spec, pop, note, iret), slot
        txt, hasret, pop = self.call_text(kind, ent, target, regs)
        if txt is None:   # _atexit
            a = self.stack[-1] if self.stack else None
            self.emit('RX_ATEXIT(%s);' % (hexlit(a.v) if a is not None and a.k == 'c' else '0'))
            hasret = False
        elif txt == '': hasret = False
        elif hasret:
            t = self.tmp(); self.emit('uint32_t %s = %s' % (t, txt)); self.pending = E(t)
        else: self.emit(txt)
        for _ in range(pop // 4):
            if self.stack: self.sp_pop()
        self.esp += pop
        clob = ent.clobbers if kind == 'auto' else ('eax', 'ecx', 'edx')
        for r in clob: self.regs[r] = U()
        for k in (ent.xclob if kind == 'auto' else range(8)): self.xmm.pop(k, None)
        if hasret and kind == 'auto' and ent.retreg == 'xmm0': self.xmm[0] = [self.pending, U(), U(), U()]
        elif hasret: self.regs['eax'] = self.pending

    def vector_ctor_iterator(self):
        """`vector constructor iterator'(base, size, count, ctor) (stdcall, ECX = the element for the constructor)"""
        base, size, count, ctor = self.arg_values(['s0', 's1', 's2', 's3'], self.regs)
        if not (size.is_c() and count.is_c() and ctor.is_c()): raise Unsupported('vector constructor with computed arguments')
        kind, ent = self.resolve(ctor.v)
        regs = {r: self.regs[r] for r in REGS}
        if count.v <= 1:
            regs['ecx'] = base
            txt, hasret, pop = self.call_text(kind, ent, ctor.v, regs)
            self.emit(txt)
        else:
            regs['ecx'] = E('elem')
            txt, hasret, pop = self.call_text(kind, ent, ctor.v, regs)
            self.emit('for (uint32_t k = 0; k < %d; k++) {  /* `vector constructor iterator\'(%s, %s, %d, %08x) */' % (count.v, self.expr(base), hexlit(size.v), count.v, ctor.v))
            self.emit('    uint32_t elem = %s + k * %s;' % (self.expr(base), hexlit(size.v)))
            self.emit('    ' + txt)
            self.emit('}')
        for _ in range(4):
            if self.stack: self.sp_pop()
        self.esp += 16
        for r in (ent.clobbers if kind == 'auto' else ('eax', 'ecx', 'edx')): self.regs[r] = U()
        for k in (ent.xclob if kind == 'auto' else range(8)): self.xmm.pop(k, None)

    # ---- structured forward branches
    def snapshot(self):
        return (dict(self.regs), {k: list(v) for k, v in self.xmm.items()}, list(self.fpu), dict(self.slots), list(self.stack), self.esp, list(self.stack_offs))

    def restore(self, st):
        self.regs, self.xmm, self.fpu, self.slots = dict(st[0]), {k: list(v) for k, v in st[1].items()}, list(st[2]), dict(st[3])
        self.stack, self.esp, self.stack_offs = list(st[4]), st[5], list(st[6])

    def merge(self, a, b, cmpinfo=None, bid=None, noelse=False):
        """the state after a join of a (the branch just ended) and b (the other path). Values that differ become variables assigned on
        both paths (placeholders @PHI..@, resolved in Gen.compile); with noelse the other path is the state before the branch."""
        def same(x, y): return x is y or (x.k == y.k and x.v == y.v and x.off == y.off and x.k in ('c', 'a', 'iat'))
        def mrg(x, y):
            if same(x, y): return x
            if cmpinfo:      # `if (t != 0) {... eax = t} else {eax = 0}`: both sides leave t
                t = cmpinfo
                if x.k == 'a' and x.v == t and x.off == 0 and y.k == 'c' and y.v == 0: return x
                if y.k == 'a' and y.v == t and y.off == 0 and x.k == 'c' and x.v == 0: return y
            if bid is not None and x.k in ('c', 'a') and y.k in ('c', 'a'):
                var = 'p%d' % self.nphi; self.nphi += 1
                self.phis.setdefault(bid, []).append((var, self.expr(x), self.expr(y)))
                return E(var)
            return U()
        regs = {r: mrg(a[0][r], b[0][r]) for r in REGS}
        xmm = {}
        for k in set(a[1]) | set(b[1]):
            la, lb = a[1].get(k), b[1].get(k)
            xmm[k] = [mrg(x, y) for x, y in zip(la, lb)] if la and lb else [U()] * 4
        slots = {k: (a[3][k] if k in a[3] and k in b[3] and (a[3][k] is b[3][k]) else U()) for k in set(a[3]) | set(b[3])}
        return (regs, xmm, list(a[2]) if a[2] == b[2] else [], slots, list(a[4]), a[5], list(a[6]))

    def cond_expr(self, m):
        """-> (condition for `the jcc is taken`, its negation) as C expressions"""
        if self.flags is None: raise Unsupported('branch without flags')
        kind, a, b = self.flags
        if kind == 'fcmp':               # comiss a, b: CF = a < b or unordered, ZF = a == b or unordered
            fa, fb = self.fexpr(a), self.fexpr(b)
            pos = {'jbe': '!(%s > %s)', 'ja': '(%s > %s)', 'jb': '!(%s >= %s)', 'jae': '(%s >= %s)'}.get(m)
            neg = {'jbe': '(%s > %s)', 'ja': '!(%s > %s)', 'jb': '(%s >= %s)', 'jae': '!(%s >= %s)'}.get(m)
            if pos: return pos % (fa, fb), neg % (fa, fb)
            raise Unsupported('condition ' + m)
        ea, eb = self.expr(a), (self.expr(b) if b is not None else None)
        if kind == 'test':
            if m in ('je', 'jz'): return '(%s & %s) == 0' % (ea, eb), '(%s & %s) != 0' % (ea, eb)
            if m in ('jne', 'jnz'): return '(%s & %s) != 0' % (ea, eb), '(%s & %s) == 0' % (ea, eb)
        if kind == 'res':
            t = {'je': ('%s == 0', '%s != 0'), 'jz': ('%s == 0', '%s != 0'), 'jne': ('%s != 0', '%s == 0'), 'jnz': ('%s != 0', '%s == 0'),
                 'jns': ('(int32_t)%s >= 0', '(int32_t)%s < 0'), 'js': ('(int32_t)%s < 0', '(int32_t)%s >= 0')}.get(m)
            if t: return t[0] % ea, t[1] % ea
        if kind == 'cmp':
            sym = {'je': ('==', '!='), 'jz': ('==', '!='), 'jne': ('!=', '=='), 'jnz': ('!=', '=='), 'jb': ('<', '>='), 'jbe': ('<=', '>'), 'ja': ('>', '<='), 'jae': ('>=', '<')}.get(m)
            if sym: return '%s %s %s' % (ea, sym[0], eb), '%s %s %s' % (ea, sym[1], eb)
            sym = {'jl': ('<', '>='), 'jle': ('<=', '>'), 'jg': ('>', '<='), 'jge': ('>=', '<')}.get(m)
            if sym: return '(int32_t)%s %s (int32_t)%s' % (ea, sym[0], eb), '(int32_t)%s %s (int32_t)%s' % (ea, sym[1], eb)
        raise Unsupported('condition ' + m)

    def find_loops(self):
        """backward conditional branches: head address -> index of the branch"""
        heads = {}
        for j, i in enumerate(self.ins):
            if i.mnemonic.startswith('j') and i.operands[0].type == cx.X86_OP_IMM and i.operands[0].imm <= i.address:
                if i.mnemonic == 'jmp': raise Unsupported('backward jmp')
                heads[i.operands[0].imm] = j
        return heads

    def loop_begin(self, idx, tail):
        """a do { } while loop: the registers the body writes become C variables (loop-carried)"""
        body = self.ins[idx:tail + 1]
        written, readfirst = set(), set()
        for b in body:
            rr, rw = b.regs_access()
            if b.mnemonic in ('xorps', 'pxor') and b.operands[0].reg == b.operands[1].reg: rr = []      # (a zeroing, not a read)
            for r in rr:
                n = md.reg_name(r)
                n = R8.get(n, n)
                if n in REGS and n not in written and n not in ('esp', 'ebp'): readfirst.add(n)
                if n.startswith('xmm') and n not in written: readfirst.add(n)
            for r in rw:
                n = md.reg_name(r); n = R8.get(n, n)
                if n in REGS and n not in ('esp', 'ebp') or n.startswith('xmm'): written.add(n)
            if b.mnemonic == 'call': written |= {'eax', 'ecx', 'edx'}
        vars_ = {}
        self.loop_temps = getattr(self, 'loop_temps', [])
        temps = set()
        for n in sorted(written):
            if n.startswith('xmm'):
                if n in readfirst: raise Unsupported('loop-carried SSE register')
                continue
            if n not in readfirst: temps.add(n); continue          # written before it is read in every iteration: not carried
            v = self.regs[n]
            if v.k not in ('c', 'a'): raise Unsupported('loop with an unknown register')
            var = 'l%d_%s' % (self.nblk, n); self.nblk += 1
            self.emit('uint32_t %s = %s;' % (var, self.expr(v)))
            self.regs[n] = E(var); vars_[n] = var
        self.emit('for (;;) {'); self.ind += 1
        return {'tail': tail, 'vars': vars_, 'depth': 0, 'temps': temps}

    def loop_end(self, i, lp):
        c, _ = self.cond_expr(i.mnemonic)
        t = self.tmp(); self.emit('uint32_t %s = (%s);' % (t, c))
        for n, var in lp['vars'].items():
            v = self.regs[n]
            if v.k not in ('c', 'a'): raise Unsupported('loop register left unknown')
            if not (v.k == 'a' and v.v == var and v.off == 0): self.emit('%s = %s;' % (var, self.expr(v)))
        self.emit('if (!%s) break;' % t)
        self.ind -= 1; self.emit('}')
        for n, var in lp['vars'].items(): self.regs[n] = E(var)
        for n in lp['temps']: self.regs[n] = U()
        self.flags = None

    def run(self):
        ins = self.ins
        blocks = []
        heads = self.find_loops()
        loops = []
        for idx, i in enumerate(ins):
            m = i.mnemonic
            if i.address in heads and not any(l['tail'] == heads[i.address] for l in loops):
                lp = self.loop_begin(idx, heads[i.address]); lp['depth'] = len(blocks); loops.append(lp)
            if loops and idx == loops[-1]['tail']:
                if len(blocks) != loops[-1]['depth']: raise Unsupported('branch crossing a loop end')
                self.loop_end(i, loops.pop()); continue
            while blocks and blocks[-1]['end'] == i.address:
                b = blocks.pop()
                if not b.get('returned'): self.emit('@PHIE@%d@%s' % (b['id'], 'else' if b['else_'] else 'then'))
                self.ind -= 1; self.emit('}')
                cur = self.snapshot()
                if b.get('returned') and not b['else_']: self.restore(b['entry'])
                elif b['else_']: self.restore(self.merge(b['then_end'], cur, b['cmp'], b['id']))
                else: self.restore(self.merge(cur, b['entry'], b['cmp'], b['id'], noelse=True))
            if m in ('int3', 'nop'): continue
            if m == 'ret':
                self.pop = i.operands[0].imm if i.operands else 0
                rv = self.regs['eax']
                if blocks:                         # an early return: the block does not fall through to the join
                    self.rets.append(rv); self.note_ret_regs(); self.emit('@RET@%d' % (len(self.rets) - 1)); blocks[-1]['returned'] = True
                    continue
                self.rets.append(rv); self.ret_val = rv; self.note_ret_regs()
                self.emit('@RET@%d' % (len(self.rets) - 1))
                return self
            if m in ('test', 'cmp'):
                a, c = i.operands
                if m == 'test' and a.type == cx.X86_OP_MEM and a.size == 1 and c.type == cx.X86_OP_REG:
                    k = self.addr_of(a)
                    if k[0] != 'abs': raise Unsupported('test of a computed address')
                    t = self.tmp(); self.emit('uint32_t %s = U8(0, %s);' % (t, hexlit(k[1])))
                    self.flags = ('test', E(t), self.regs[reg32(c.reg)])
                elif m == 'test' and a.type == cx.X86_OP_REG and c.type == cx.X86_OP_REG and a.reg == c.reg: self.flags = ('cmp', self.val(a), C(0))
                else: self.flags = (m, self.val(a), self.val(c))
                continue
            if m.startswith('j'):
                tgt = i.operands[0].imm
                if m == 'jmp':
                    if blocks and blocks[-1]['end'] == ins[idx + 1].address and not blocks[-1]['else_']:
                        b = blocks[-1]; b['else_'] = True; b['then_end'] = self.snapshot(); b['end'] = tgt
                        if not b.get('returned'): self.emit('@PHIE@%d@then' % b['id'])
                        self.restore(b['entry']); self.ind -= 1; self.emit('} else {'); self.ind += 1
                        continue
                    raise Unsupported('forward jmp')
                _, c = self.cond_expr(m)
                bid = self.nblk; self.nblk += 1
                self.emit('@PHIPRE@%d' % bid)
                self.emit('if (%s) {' % c); self.ind += 1
                ci = None
                if self.flags and self.flags[0] == 'cmp' and m == 'je' and self.flags[1].k == 'a' and self.flags[1].off == 0 and self.flags[2].k == 'c' and self.flags[2].v == 0:
                    ci = self.flags[1].v
                blocks.append({'end': tgt, 'entry': self.snapshot(), 'else_': False, 'then_end': None, 'cmp': ci, 'returned': False, 'id': bid})
                continue
            try:
                self.step(i)
            except Unsupported as e:
                if ' @' not in str(e): raise Unsupported('%s @%x %s %s' % (e, i.address, i.mnemonic, i.op_str))
                raise
        raise Unsupported('no ret')

    def step(self, i):
        m = i.mnemonic
        ops = i.operands
        if m == 'push':
            if ops[0].type == cx.X86_OP_REG and reg32(ops[0].reg) == 'ebp':
                nxt = self.by_addr.get(i.address + i.size)
                frame_save = nxt is not None and nxt.mnemonic == 'mov' and nxt.op_str == 'ebp, esp'      # push ebp; mov ebp, esp
                v = U() if frame_save or self.regs['ebp'].k not in ('c', 'a') else self.regs['ebp']
            else: v = self.val(ops[0])
            self.stack.append(v); self.esp -= 4; self.stack_offs.append(self.esp); self.slot_write(self.esp, v)
        elif m == 'pop':
            v = self.sp_pop() if self.stack else (self.slot_read(self.esp) if ops[0].type == cx.X86_OP_REG and reg32(ops[0].reg) not in ('ebp',) else U())
            self.esp += 4
            if ops[0].type == cx.X86_OP_REG: self.setreg(ops[0], v)
        elif m == 'mov' and any(o.type == cx.X86_OP_MEM and o.mem.segment == cx.X86_REG_FS for o in ops):
            if ops[0].type == cx.X86_OP_REG: self.setreg(ops[0], U())          # the SEH chain (fs:[0]): exception frames are not modelled
        elif m == 'xor' and ops[0].type == cx.X86_OP_REG and ops[1].type == cx.X86_OP_REG and ops[0].reg == ops[1].reg: self.setreg(ops[0], C(0))
        elif m == 'xor' and ops[0].type == cx.X86_OP_REG:
            a = self.val(ops[0]); b = self.val(ops[1])
            if a.is_c() and b.is_c(): self.setreg(ops[0], C(a.v ^ b.v))
            elif a.k in ('c', 'a') and b.k in ('c', 'a'):
                t = self.tmp(); self.emit('uint32_t %s = %s ^ %s;' % (t, self.expr(a), self.expr(b))); self.setreg(ops[0], E(t))
            else: self.setreg(ops[0], U())         # (the stack cookie)
        elif m == 'mov':
            d, s = ops
            if d.type == cx.X86_OP_REG:
                n = md.reg_name(d.reg)
                if n in R8 and s.type == cx.X86_OP_MEM:        # mov r8, byte [mem]: the upper 24 bits keep the old value
                    k = self.addr_of(s); t = self.tmp()
                    if k[0] == 'abs': self.emit('uint32_t %s = U8(0, %s);' % (t, hexlit(k[1])))
                    elif k[0] == 'mem': self.emit('uint32_t %s = U8(%s, %s);' % (t, k[1], hexlit(k[2])))
                    else: raise Unsupported('8-bit load from the stack')
                    old = self.regs[R8[n]]
                    if old.k in ('c', 'a'): self.regs[R8[n]] = E('(%s & 0xffffff00 | %s)' % (self.expr(old), t))
                    else: self.regs[R8[n]] = E(t)       # (the upper bits of an unknown register: taken as dead)
                    return
                if n in R8 and not (self.regs[R8[n]].k == 'a'):
                    old = self.regs[R8[n]]; nv = self.val(s) if s.type != cx.X86_OP_REG else self.regs[reg32(s.reg)]
                    if not (old.is_c() and nv.is_c()): raise Unsupported('8-bit move into an unknown register')
                    self.regs[R8[n]] = C((old.v & ~0xff) | (nv.v & 0xff)); return
                if n not in REGS: raise Unsupported('mov to ' + n)
                if n == 'ebp' and s.type == cx.X86_OP_REG and md.reg_name(s.reg) == 'esp': self.ebp_off = self.esp; self.regs['ebp'] = U(); return
                if n == 'esp' and s.type == cx.X86_OP_REG and md.reg_name(s.reg) == 'ebp': self.esp = self.ebp_off; self.stack = []; self.stack_offs = []; return
                self.setreg(d, self.val(s))
            else:
                if s.type == cx.X86_OP_REG and md.reg_name(s.reg) in R16:
                    v = self.regs[reg32(s.reg)]
                    self.store(d, C(v.v & 0xffff) if v.is_c() else E('(%s & 0xffff)' % self.expr(v)), 2)
                elif s.type == cx.X86_OP_REG and md.reg_name(s.reg) in R8: self.store(d, self.regs[reg32(s.reg)], 1)
                else: self.store(d, self.val(s), d.size)
        elif m == 'lea' and md.reg_name(ops[0].reg) == 'esp' and ops[1].mem.base and md.reg_name(ops[1].mem.base) == 'esp' and ops[1].mem.disp == 0: pass
        elif m == 'lea':
            d, s = ops
            k = self.addr_of(s)
            if k[0] == 'abs': self.setreg(d, C(k[1]))
            elif k[0] == 'mem': self.setreg(d, V('a', k[1], k[2]))
            elif self.frame: self.setreg(d, V('a', 'F', self.fslot(k[1])))
            else: raise NeedFrame()
        elif m == 'xor' and ops[0].type == cx.X86_OP_REG and ops[1].type == cx.X86_OP_REG and ops[0].reg == ops[1].reg:
            self.setreg(ops[0], C(0))
        elif m in ('add', 'sub') and ops[0].type == cx.X86_OP_REG and md.reg_name(ops[0].reg) == 'esp':
            d = ops[1].imm
            if m == 'add':
                self.esp += d
                while self.stack and d >= 4: self.sp_pop(); d -= 4
            else: self.esp -= d
        elif m == 'and' and ops[0].type == cx.X86_OP_REG and md.reg_name(ops[0].reg) == 'esp':
            if self.frame:
                mask = ops[1].imm & 0xffffffff; mask = mask - (1 << 32) if mask >= 0x80000000 else mask
                self.esp = ((self.esp + 4) & mask) - 4      # (entry esp = 12 mod 16: the caller's esp is aligned before the call)
            else: self.esp -= 1000000
            self.stack = []; self.stack_offs = []
        elif m in ('add', 'sub', 'or', 'and', 'shl', 'shr', 'sar') and ops[0].type == cx.X86_OP_REG:
            a = self.val(ops[0]); b = self.val(ops[1])
            if m == 'and': self.flags = ('test', a, b)
            if a.is_c() and b.is_c():
                r = {'add': a.v + b.v, 'sub': a.v - b.v, 'or': a.v | b.v, 'and': a.v & b.v, 'shl': a.v << (b.v & 31), 'shr': a.v >> (b.v & 31)}.get(m)
                if m == 'sar': r = ((a.v - (1 << 32) if a.v & 0x80000000 else a.v) >> (b.v & 31))
                self.setreg(ops[0], C(r))
            elif m in ('add', 'sub') and b.is_c() and a.k == 'a':
                self.setreg(ops[0], V('a', a.v, a.off + (b.v if m == 'add' else -b.v)))
            elif m == 'add' and a.is_c() and b.k == 'a':
                self.setreg(ops[0], V('a', b.v, b.off + a.v))
            else:
                t = self.tmp(); sym = {'add': '+', 'sub': '-', 'or': '|', 'and': '&', 'shl': '<<', 'shr': '>>'}.get(m)
                if m == 'sar': self.emit('uint32_t %s = (uint32_t)((int32_t)%s >> (%s & 31));' % (t, self.expr(a), self.expr(b)))
                elif sym is None: raise Unsupported(m)
                else: self.emit('uint32_t %s = %s %s %s;' % (t, self.expr(a), sym, self.expr(b)))
                self.setreg(ops[0], E(t))
            if m != 'and' and self.regs[reg32(ops[0].reg)].k in ('c', 'a'): self.flags = ('res', self.regs[reg32(ops[0].reg)], None)
        elif m == 'or' and ops[0].type == cx.X86_OP_MEM:
            v = self.load32(ops[0]); b = self.val(ops[1])
            self.store(ops[0], E('%s | %s' % (self.expr(v), self.expr(b))), 4)
        elif m in ('inc', 'dec') and ops[0].type == cx.X86_OP_REG:
            a = self.val(ops[0]); d = 1 if m == 'inc' else -1
            if a.is_c(): self.setreg(ops[0], C(a.v + d))
            elif a.k == 'a': self.setreg(ops[0], V('a', a.v, a.off + d))
            else: raise Unsupported(m)
            self.flags = ('res', self.regs[reg32(ops[0].reg)], None)
        elif m == 'mul' and len(ops) == 1:
            a = self.regs['eax']; b = self.val(ops[0])
            if not (a.is_c() and b.is_c()): raise Unsupported('mul of unknown values')
            p = a.v * b.v
            self.regs['eax'] = C(p); self.regs['edx'] = C(p >> 32); self.flags = ('of', C(1 if p >> 32 else 0), None)
        elif m == 'div' and len(ops) == 1:
            a = self.regs['eax']; d = self.regs['edx']; b = self.val(ops[0])
            if a.is_c() and d.is_c() and d.v == 0 and b.is_c() and b.v:
                self.regs['eax'] = C(a.v // b.v); self.regs['edx'] = C(a.v % b.v)
            elif a.k in ('c', 'a') and d.is_c() and d.v == 0 and b.k in ('c', 'a'):
                t1, t2 = self.tmp(), self.tmp()
                self.emit('uint32_t %s = %s / %s;' % (t1, self.expr(a), self.expr(b))); self.emit('uint32_t %s = %s %% %s;' % (t2, self.expr(a), self.expr(b)))
                self.regs['eax'] = E(t1); self.regs['edx'] = E(t2)
            else: raise Unsupported('div of unknown values')
        elif m == 'seto':
            if self.flags is None or self.flags[0] != 'of': raise Unsupported('seto')
            r = reg32(ops[0].reg); old = self.regs[r]
            if not old.is_c(): raise Unsupported('seto into an unknown register')
            self.regs[r] = C((old.v & ~0xff) | self.flags[1].v)
        elif m == 'neg' and ops[0].type == cx.X86_OP_REG:
            a = self.val(ops[0])
            if not a.is_c(): raise Unsupported('neg of an unknown value')
            self.setreg(ops[0], C(-a.v))
        elif m == 'movzx':
            d, s = ops
            if s.type != cx.X86_OP_MEM: raise Unsupported('movzx')
            k = self.addr_of(s); t = self.tmp()
            if k[0] == 'abs': self.emit('uint32_t %s = U8(0, %s);' % (t, hexlit(k[1])))
            elif k[0] == 'mem': self.emit('uint32_t %s = U8(%s, %s);' % (t, k[1], hexlit(k[2])))
            else: raise Unsupported('movzx from the stack')
            self.setreg(d, E(t))
        elif m == 'call': self.do_call(i)
        # ---- SSE / x87
        elif m in ('movss', 'movd'):
            d, s = ops
            if d.type == cx.X86_OP_REG:
                n = md.reg_name(d.reg)
                if n in REGS:      # movd eax, xmm
                    self.setreg(d, self.xlane(s)[0]); return
                lane = self.xload(s, 1)[0] if s.type == cx.X86_OP_MEM else self.xlane(s)[0]
                cur = self.xmm.get(int(n[3:]))
                if s.type == cx.X86_OP_MEM: self.setx(d, [lane, C(0), C(0), C(0)])
                else: self.setx(d, [lane] + (cur[1:] if cur else [U()] * 3))
            else: self.xstore(d, [self.xlane(s)[0]])
        elif m in ('movaps', 'movdqa', 'movups', 'movdqu'):
            d, s = ops
            if d.type == cx.X86_OP_REG: self.setx(d, self.xload(s, 4) if s.type == cx.X86_OP_MEM else self.xlane(s))
            else: self.xstore(d, self.xlane(s))
        elif m == 'movq':
            d, s = ops
            if d.type == cx.X86_OP_REG: self.setx(d, (self.xload(s, 2) if s.type == cx.X86_OP_MEM else self.xlane(s)[:2]) + [C(0), C(0)])
            else: self.xstore(d, self.xlane(s)[:2])
        elif m in ('xorps', 'pxor') and ops[0].reg == ops[1].reg: self.setx(ops[0], [C(0)] * 4)
        elif m == 'shufps':
            d, s, imm = ops[0], ops[1], ops[2].imm
            a = self.xlane(d); b = self.xlane(s)
            self.setx(d, [a[imm & 3], a[(imm >> 2) & 3], b[(imm >> 4) & 3], b[(imm >> 6) & 3]])
        elif m == 'unpcklps':
            a = self.xlane(ops[0]); b = self.xlane(ops[1])
            self.setx(ops[0], [a[0], b[0], a[1], b[1]])
        elif m in ('andps', 'orps', 'xorps', 'andnps', 'pand', 'por', 'pxor') and not (m in ('xorps', 'pxor') and ops[0].reg == ops[1].reg):
            a = self.xlane(ops[0])
            b = self.xload(ops[1], 4) if ops[1].type == cx.X86_OP_MEM else self.xlane(ops[1])
            sym = {'andps': '&', 'pand': '&', 'orps': '|', 'por': '|', 'xorps': '^', 'pxor': '^'}.get(m)
            if sym is None: raise Unsupported(m)
            out = []
            for x, y in zip(a, b):
                if x.k == 'u' or y.k == 'u': out.append(U()); continue
                t = self.tmp(); self.emit('uint32_t %s = %s %s %s;' % (t, self.expr(x), sym, self.expr(y))); out.append(E(t))
            self.setx(ops[0], out)
        elif m in ('comiss', 'ucomiss'):
            a = self.xlane(ops[0])[0]
            b = self.xload(ops[1], 1)[0] if ops[1].type == cx.X86_OP_MEM else self.xlane(ops[1])[0]
            self.flags = ('fcmp', a, b)
        elif m in ('addss', 'subss', 'mulss', 'divss'):
            d, s = ops
            a = self.xlane(d)
            b = self.xload(s, 1)[0] if s.type == cx.X86_OP_MEM else self.xlane(s)[0]
            self.setx(d, [self.float_op({'addss': '+', 'subss': '-', 'mulss': '*', 'divss': '/'}[m], a[0], b)] + a[1:])
        elif m == 'cvtsi2ss':
            d, s = ops
            v = self.val(s)
            t = self.tmp(); self.emit('uint32_t %s = rx_fb((float)(int32_t)%s);' % (t, self.expr(v)))
            cur = self.xmm.get(int(md.reg_name(d.reg)[3:])) or [U()] * 4
            self.setx(d, [E(t)] + cur[1:])
        elif m == 'fld1': self.fpu.append(C(0x3f800000))
        elif m == 'fldz': self.fpu.append(C(0))
        elif m == 'fld':
            if ops[0].type == cx.X86_OP_MEM and ops[0].size == 4:
                src = self.xload(ops[0], 1)[0]
                t = self.tmp(); self.emit('uint32_t %s = rx_x87_load_f32(%s);' % (t, self.expr(src))); self.fpu.append(E(t))
            elif ops[0].type == cx.X86_OP_REG: self.fpu.append(self.fpu[-1 - int(md.reg_name(ops[0].reg)[3:-1])])
            else: raise Unsupported('fld')
        elif m in ('fst', 'fstp'):
            if ops[0].type == cx.X86_OP_MEM and ops[0].size == 4: self.xstore(ops[0], [self.fpu[-1]])
            else: raise Unsupported(m)
            if m == 'fstp': self.fpu.pop()
        elif m == 'fxch':
            j = int(md.reg_name(ops[-1].reg)[3:-1]) if ops else 1
            self.fpu[-1], self.fpu[-1 - j] = self.fpu[-1 - j], self.fpu[-1]
        else:
            raise Unsupported('instruction ' + m)


def resolve_phis(lines, phis, nphi):
    """turns the join placeholders into assignments of the join variables (declared at the top)"""
    out = []
    for l in lines:
        mm = re.match(r'(\s*)@PHI(PRE|E)@(\d+)(?:@(\w+))?$', l)
        if not mm: out.append(l); continue
        ind, kind, bid, side = mm.group(1), mm.group(2), int(mm.group(3)), mm.group(4)
        for var, ea, eb in phis.get(bid, []):
            # a = the path that ran the block (then / else side as emitted), b = the other one
            if kind == 'PRE': out.append('%s%s = %s;' % (ind, var, eb))                    # no else: the value before the branch
            else: out.append('%s%s = %s;' % (ind, var, ea if side == 'then' else eb))
    if nphi: out.insert(0, '    uint32_t %s;' % ', '.join('p%d' % i for i in range(nphi)))
    return out


def prune(lines):
    """drops `uint32_t vN = ...;` definitions that nothing uses"""
    changed = True
    while changed:
        changed = False
        text = '\n'.join(lines)
        out = []
        for l in lines:
            mm = re.match(r'(\s*)uint32_t (v\d+) = (?!rx_|crt_)', l)
            if mm and len(re.findall(r'\b%s\b' % mm.group(2), text)) == 1:
                changed = True; continue
            mc = re.match(r'(\s*)uint32_t (v\d+) = ((?:rx_|crt_).*)$', l)      # a call whose result nothing uses
            if mc and len(re.findall(r'\b%s\b' % mc.group(2), text)) == 1:
                changed = True; out.append(mc.group(1) + mc.group(3)); continue
            out.append(l)
        lines = out
    return lines


def inline_single(lines):
    """a temp defined by a load and used once, with only loads between the definition and the use, is inlined"""
    out = list(lines)
    i = 0
    while i < len(out):
        m = re.match(r'(\s*)uint32_t (v\d+) = ((?:U32|U8)\(.*\));$', out[i])
        if m:
            name = m.group(2)
            uses = [j for j in range(len(out)) if j != i and re.search(r'\b%s\b' % name, out[j])]
            if len(uses) == 1 and uses[0] > i:
                j = uses[0]
                if all(re.match(r'\s*uint32_t v\d+ = ', b) for b in out[i + 1:j]):
                    out[j] = re.sub(r'\b%s\b' % name, lambda _m: m.group(3), out[j]); del out[i]; continue
        i += 1
    return out


def tidy(lines):
    out = []
    for l in lines:
        if out and l.strip() == '}' and out[-1].strip() == '} else {':
            out[-1] = out[-1].replace('} else {', '}'); continue
        out.append(l)
    return out


class Gen:
    def __init__(self, img, fn):
        self.img = img; self.fsize = fn
        self.fns = collections.OrderedDict()    # addr -> Fn (translated, in dependency order)
        self.failed = {}
        self.busy = set()

    def size_of(self, addr): return self.fsize.get(addr, ('?', 96))[1]

    def cstr(self, addr):
        b = self.img[addr - BASE:addr - BASE + 64]
        return b.split(b'\0')[0].decode('latin-1')

    def compile(self, addr):
        if addr in self.fns: return self.fns[addr]
        if addr in self.failed: raise Unsupported(self.failed[addr])
        if addr in self.busy: raise Unsupported('recursion at %08x' % addr)
        self.busy.add(addr)
        frame = 0
        try:
            try:
                t = Tr(self, addr, self.size_of(addr)).run()
            except NeedFrame:
                probe = Tr(self, addr, self.size_of(addr), frame=0x10000); probe.run()
                frame = (probe.maxdepth + 16 + 15) & ~15
                t = Tr(self, addr, self.size_of(addr), frame=frame).run()
        except Unsupported as e:
            self.failed[addr] = str(e)
            self.busy.discard(addr)
            raise
        self.busy.discard(addr)
        fn = Fn(addr)
        fn.pop = t.pop
        fn.clobbers = set(t.clobbers)
        def known(rv): return rv.k in ('c', 'a') and not (rv.k == 'a' and rv.v == 'in_eax' and rv.off == 0)
        ks = [known(rv) for rv in t.rets]
        if any(ks) and not all(ks): raise Unsupported('returns a value on some paths only')
        fn.returns = bool(ks) and all(ks)
        fn.xclob = set(t.xwritten)
        xs = [x is not None for x in t.xrets]
        if any(xs) and not fn.returns:                  # a float result in xmm0 (when eax is the result, xmm0 is just clobbered)
            if not all(xs): raise Unsupported('returns xmm0 on some paths only')
            fn.returns = True; fn.retreg = 'xmm0'; t.rets = list(t.xrets)
        raw = []
        leave = '    ' * 1 + 'RX_FRAME_LEAVE(F, %s);' % hexlit(frame) if frame else None
        for l in t.out:
            mm = re.match(r'(\s*)@RET@(\d+)$', l)
            if mm:
                if frame: raw.append('%sRX_FRAME_LEAVE(F, %s);' % (mm.group(1), hexlit(frame)))
                raw.append('%s%s' % (mm.group(1), ('return %s;' % t.expr(t.rets[int(mm.group(2))])) if fn.returns else 'return;'))
            else: raw.append(l)
        if not fn.returns and raw and raw[-1].strip() == 'return;': raw.pop()      # the trailing `return;` of a void function is noise
        if frame: raw.insert(0, '    RX_FRAME_ENTER(F, %s);' % hexlit(frame))
        raw = resolve_phis(raw, t.phis, t.nphi)
        lines = tidy(inline_single(prune(raw)))
        text = '\n'.join(lines)
        for r in ('eax', 'ecx', 'edx', 'ebx', 'esi', 'edi', 'ebp'):
            if re.search(r'\bin_%s\b' % r, text): fn.params.append(('reg', r))
        for k in range(8):
            if re.search(r'\bin_xmm%d\b' % k, text): fn.params.append(('xmm', k))
        used = sorted({int(x) for x in re.findall(r'\ba(\d+)\b', text)})
        maxs = max(used) + 1 if used else 0
        for k in range(maxs): fn.params.append(('stack', k))   # stack parameters a0 .. a(maxs-1); the callee pops them (ret n)
        fn.lines = lines
        self.fns[addr] = fn
        return fn

    def emit_fn(self, fn, name=None, static=True, register=False):
        ret = 'uint32_t' if fn.returns else 'void'
        pars = ['uint32_t in_%s' % p[1] if p[0] == 'reg' else 'uint32_t in_xmm%d' % p[1] if p[0] == 'xmm' else 'uint32_t a%d' % p[1] for p in fn.params]
        s = '%s%s %s(%s) {\n%s\n}\n' % ('static ' if static else '', ret, name or fn.name, ', '.join(pars) or 'void', '\n'.join(fn.lines))
        if register: s += 'RX_FUNCTION(0x%08x, %s)\n' % (fn.addr, name or fn.name)
        return s


def load_functions(tsv):
    fn = {}
    for l in open(tsv, encoding='latin-1').readlines()[1:]:
        c = l.rstrip('\n').split('\t')
        fn[int(c[0], 16)] = (c[1], int(c[3]))
    return fn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe'); ap.add_argument('functions'); ap.add_argument('out')
    ap.add_argument('--hand', default=''); ap.add_argument('--list', action='store_true')
    ap.add_argument('--func', default='', help='addr:name,..: translate these functions too and register them')
    a = ap.parse_args()
    import pefile
    pe = pefile.PE(a.exe, fast_load=True); img = pe.get_memory_mapped_image()
    gen = Gen(img, load_functions(a.functions))
    hand = HAND | {int(x, 16) for x in a.hand.split(',') if x}
    slots = []
    for s in range(TABLE_LO, TABLE_HI, 4):
        v = struct.unpack_from('<I', img, s - BASE)[0]
        if v: slots.append((s, v))
    bad = collections.OrderedDict()
    inits = []
    for v, name in [(int(x.split(':')[0], 16), x.split(':')[1]) for x in a.func.split(',') if x]:
        try: inits.append((None, v, name, gen.compile(v)))
        except Unsupported as e: bad[v] = str(e)
    for s, v in slots:
        if v in hand: continue
        try: inits.append((s, v, 'rx_init_%08x' % v, gen.compile(v)))
        except Unsupported as e: bad[v] = str(e)
    if a.list:
        for v, why in bad.items(): print('%08x %s' % (v, why))
        c = collections.Counter(re.sub(r'^call [0-9a-f]+: ', '', w) for w in bad.values())
        print(len(inits), 'translated;', len(bad), 'not;', len(gen.fns) - len(inits), 'helper functions:')
        for w, n in c.most_common(45): print(' ', n, w)
        return
    initaddrs = {i[1] for i in inits}
    helpers = [f for f in gen.fns.values() if f.addr not in initaddrs]
    with open(a.out, 'w') as f:
        f.write('/* build_globals_gen.c -- GENERATED by core/tools/build_globals_gen.py from MXReflex.exe: the static initializers of the _initterm table\n'
                ' * 0x009166e4 .. 0x00917660 and the constructors they call, translated instruction by instruction (constants folded, loads kept in\n'
                ' * order, `vector constructor iterator\' calls as loops). Do not edit; hand-written parts are in build_globals_*.c.\n'
                ' * See core/notes/construction.md section 14. */\n')
        f.write('#include "build_globals_int.h"\n\n')
        # only the helpers some emitted function calls
        needed = set(initaddrs)
        changed = True
        while changed:
            changed = False
            for fn in gen.fns.values():
                if fn.addr in needed:
                    for mm in re.finditer(r'rx_fn_([0-9a-f]{8})', '\n'.join(fn.lines)):
                        ca = int(mm.group(1), 16)
                        if ca not in needed: needed.add(ca); changed = True
        order = [x for x in gen.fns.values() if x.addr in needed]
        done = set()
        # dependency order: a function is written after the ones it calls (gen.fns is filled callee-first)
        for fn in order:
            if fn.addr in initaddrs:
                for s, v, name, ifn in inits:
                    if v == fn.addr and v not in done:
                        f.write(('/* slot %08x */\n' % s if s else '/* %08x */\n' % v) + gen.emit_fn(ifn, name, static=s is not None, register=True) + '\n'); done.add(v)
            else:
                f.write('/* %08x */\n' % fn.addr + gen.emit_fn(fn) + '\n')
    ports = a.out.replace('game/build_globals_gen.c', 'oracle/build_globals_ports.cpp')
    if ports != a.out:
        with open(ports, 'w') as f:
            f.write('// build_globals_ports.cpp -- GENERATED by core/tools/build_globals_gen.py: one oracle adapter per generated initializer (Verify: the core function\n'
                    '// against the original, memory and registers, at every call of the boot). Group rx_globals. Development only.\n'
                    '#include "oracle.h"\n#include "../game/build_globals.h"\n#include <stdexcept>\n'
                    'static void run_init(rx_addr a) { void (*f)(void) = RX_FN(a, void (*)(void)); if (!f) throw std::runtime_error("rx_globals: no core initializer"); f(); }\n\n')
            for s_, v, name, fn in inits:
                if s_ is None: continue
                f.write('RX_PORT(0x%08x, init_%08x, "rx_globals", 0) { run_init(0x%08x); return m.ret(0); }\n' % (v, v, v))
    print('%d translated (+%d helpers), %d left' % (len(inits), len(helpers), len(bad)))
    open(a.out + '.left', 'w').write(''.join('%08x %s\n' % (v, w) for v, w in bad.items()))

if __name__ == '__main__':
    main()
