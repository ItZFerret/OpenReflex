// Instruction semantics mirror _re/trace/x86world.py (the verified reference).
#include "vm.h"
#include "jit.h"
#include <capstone/capstone.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace reflex_vm {
using reflex_f80::F80;
const char* mnemonic_name(int i);
static FastFn select_fast(const Insn& in);

static const char* const MN_NAMES[] = {
#define MN(x) #x,
#include "mnemonics.inc"
#undef MN
};

// ---------------------------------------------------------------- memory
Memory::Memory() : pages_(1u << 20, nullptr), wpages_(1u << 20, nullptr), readonly_(1u << 20, 0), dirty_(1u << 20, 0), backed_(1u << 20, 0) {}
Memory::~Memory() { for (auto* p : pages_) delete[] p; }

uint8_t* Memory::ensure(uint32_t page) {
    if (!pages_[page]) {
        pages_[page] = new uint8_t[4096]();
        if (backed_[page]) { backed_[page] = 0; loader_(page << 12, pages_[page]); }
    }
    return pages_[page];
}
uint8_t* Memory::fault(uint32_t page) {
    // diagnostics: REFLEX_FAULT_PAGE=<address>: the first touch of that lazily-loaded page stops the run (the stop names the pc)
    static const uint32_t watch = std::getenv("REFLEX_FAULT_PAGE") ? uint32_t(std::strtoul(std::getenv("REFLEX_FAULT_PAGE"), nullptr, 16)) >> 12 : 0;
    if (__builtin_expect(watch != 0, 0) && page == watch && backed_[page]) { char b[64]; std::snprintf(b, sizeof b, "first touch of page %08x", page << 12); throw VmError(b); }
    return backed_[page] ? ensure(page) : nullptr;
}
size_t Memory::resident_pages() const { size_t n = 0; for (auto* p : pages_) n += p != nullptr; return n; }
void Memory::map(uint32_t address, const uint8_t* data, size_t size, bool ro) {
    for (size_t done = 0; done < size;) {
        uint32_t a = address + uint32_t(done), page = a >> 12, off = a & 0xfff;
        size_t take = std::min<size_t>(size - done, 4096 - off);
        uint8_t* p = ensure(page);
        if (data) std::memcpy(p + off, data + done, take);
        if (ro) { readonly_[page] = 1; wpages_[page] = nullptr; }
        done += take;
    }
}
uint8_t* Memory::page(uint32_t address) { uint8_t* p = pages_[address >> 12]; return p ? p : fault(address >> 12); }
void Memory::set_readonly(uint32_t address, size_t size, bool ro) {
    for (uint64_t a = address & ~0xfffu; a < uint64_t(address) + size; a += 4096) { readonly_[a >> 12] = ro; wpages_[a >> 12] = nullptr; }
}
uint64_t Memory::read_slow(uint32_t a, int n) {
    uint32_t off = a & 0xfff;
    if (off + n <= 4096) {
        uint8_t* p = pages_[a >> 12];
        if (!p) p = fault(a >> 12);
        if (!p) { char b[64]; std::snprintf(b, sizeof b, "unmapped read %08x+%d", a, n); throw VmError(b); }
        uint64_t v = 0; std::memcpy(&v, p + off, n); return v;
    }
    uint64_t v = 0;
    for (int i = 0; i < n; i++) v |= read(a + i, 1) << (8 * i);
    return v;
}
void Memory::write_slow(uint32_t a, int n, uint64_t v) {
    uint32_t off = a & 0xfff;
    if (off + n <= 4096) {
        uint32_t page = a >> 12;
        uint8_t* p = pages_[page];
        if (!p) p = fault(page);
        if (!p) { char b[64]; std::snprintf(b, sizeof b, "unmapped write %08x+%d", a, n); throw VmError(b); }
        if (readonly_[page]) { char b[64]; std::snprintf(b, sizeof b, "write to read-only %08x", a); throw VmError(b); }
        if (!dirty_[page]) first_write(page, p);
        wpages_[page] = p;
        std::memcpy(p + off, &v, n);
        return;
    }
    for (int i = 0; i < n; i++) write(a + i, 1, (v >> (8 * i)) & 0xff);
}
void Memory::read_bytes(uint32_t a, void* out, size_t n) {
    auto* o = static_cast<uint8_t*>(out);
    for (size_t i = 0; i < n; i++) o[i] = uint8_t(read(a + uint32_t(i), 1));
}
void Memory::write_bytes(uint32_t a, const void* in, size_t n) {
    auto* s = static_cast<const uint8_t*>(in);
    for (size_t i = 0; i < n; i++) write(a + uint32_t(i), 1, s[i]);
}
void Memory::read_block(uint32_t a, void* out, size_t n) {
    auto* o = static_cast<uint8_t*>(out);
    for (size_t done = 0; done < n;) {
        uint32_t at = a + uint32_t(done), off = at & 0xfff; size_t take = std::min<size_t>(n - done, 4096 - off);
        uint8_t* p = page(at);
        if (!p) { char b[64]; std::snprintf(b, sizeof b, "unmapped read %08x+%zu", at, take); throw VmError(b); }
        std::memcpy(o + done, p + off, take); done += take;
    }
}
uint32_t g_block_watch = 0;   // debugging: report bulk writes / unmaps covering this address (stderr)
void Memory::write_block(uint32_t a, const void* in, size_t n) {
    if (__builtin_expect(g_block_watch != 0, 0) && g_block_watch >= a && g_block_watch - a < n) std::fprintf(stderr, "write_block %08x+%zx covers watch\n", a, n);
    auto* s = static_cast<const uint8_t*>(in);
    for (size_t done = 0; done < n;) {
        uint32_t at = a + uint32_t(done), off = at & 0xfff, pg = at >> 12; size_t take = std::min<size_t>(n - done, 4096 - off);
        uint8_t* p = pages_[pg] ? pages_[pg] : fault(pg);
        if (!p) { char b[64]; std::snprintf(b, sizeof b, "unmapped write %08x+%zu", at, take); throw VmError(b); }
        if (readonly_[pg]) { char b[64]; std::snprintf(b, sizeof b, "write to read-only %08x", at); throw VmError(b); }
        if (!dirty_[pg]) first_write(pg, p);
        wpages_[pg] = p;
        if (s) std::memcpy(p + off, s + done, take); else std::memset(p + off, 0, take);
        done += take;
    }
}
void Memory::unmap(uint32_t address, size_t size) {
    if (__builtin_expect(g_block_watch != 0, 0) && g_block_watch >= address && g_block_watch - address < size) std::fprintf(stderr, "unmap %08x+%zx covers watch\n", address, size);
    for (uint64_t a = address & ~0xfffu; a < uint64_t(address) + size; a += 4096) {
        uint32_t pg = uint32_t(a >> 12);
        delete[] pages_[pg]; pages_[pg] = nullptr; wpages_[pg] = nullptr; backed_[pg] = 0; readonly_[pg] = 0; dirty_[pg] = 0;
    }
}
std::vector<uint32_t> Memory::dirty_pages() const {
    std::vector<uint32_t> out;
    for (uint32_t i = 0; i < dirty_.size(); i++) if (dirty_[i]) out.push_back(i << 12);
    return out;
}
void Memory::clear_dirty() { std::fill(dirty_.begin(), dirty_.end(), 0); std::fill(wpages_.begin(), wpages_.end(), nullptr); dlist_.clear(); }
void Memory::first_write(uint32_t page, const uint8_t* p) {
    dirty_[page] = 1; dlist_.push_back(page);
    if (keep_originals_ && !originals_.count(page)) { std::unique_ptr<uint8_t[]> copy(new uint8_t[4096]); std::memcpy(copy.get(), p, 4096); originals_[page] = std::move(copy); }
    if (journal_ && !jpre_.count(page)) { std::unique_ptr<uint8_t[]> copy(new uint8_t[4096]); std::memcpy(copy.get(), p, 4096); jpre_[page] = std::move(copy); jorder_.push_back(page); }
}
void Memory::journal_begin() {
    for (uint32_t pg : dlist_) { dirty_[pg] = 0; wpages_[pg] = nullptr; }
    dlist_.clear(); jpre_.clear(); jorder_.clear(); journal_ = true;
}
void Memory::journal_restore() {
    for (uint32_t pg : jorder_) if (pages_[pg]) std::memcpy(pages_[pg], jpre_[pg].get(), 4096);
}
void Memory::journal_end() { journal_ = false; jpre_.clear(); jorder_.clear(); }
const uint8_t* Memory::original(uint32_t page_address) const {
    auto it = originals_.find(page_address >> 12);
    return it == originals_.end() ? nullptr : it->second.get();
}

// ---------------------------------------------------------------- machine
Machine::Machine() {
    csh h;
    if (cs_open(CS_ARCH_X86, CS_MODE_32, &h) != CS_ERR_OK) throw VmError("capstone open");
    cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);
    capstone_ = reinterpret_cast<void*>(h);
}
Machine::~Machine() {
    jit.reset();
    csh h = reinterpret_cast<csh>(capstone_);
    cs_close(&h);
}

static bool reg_info(unsigned reg, Operand& o) {
    static const unsigned g32[] = {X86_REG_EAX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBX, X86_REG_ESP, X86_REG_EBP, X86_REG_ESI, X86_REG_EDI};
    static const unsigned g16[] = {X86_REG_AX, X86_REG_CX, X86_REG_DX, X86_REG_BX, X86_REG_SP, X86_REG_BP, X86_REG_SI, X86_REG_DI};
    static const unsigned g8l[] = {X86_REG_AL, X86_REG_CL, X86_REG_DL, X86_REG_BL};
    static const unsigned g8h[] = {X86_REG_AH, X86_REG_CH, X86_REG_DH, X86_REG_BH};
    for (int i = 0; i < 8; i++) if (reg == g32[i]) { o.rclass = 1; o.ridx = i; return true; }
    for (int i = 0; i < 8; i++) if (reg == g16[i]) { o.rclass = 2; o.ridx = i; return true; }
    for (int i = 0; i < 4; i++) if (reg == g8l[i]) { o.rclass = 3; o.ridx = i; return true; }
    for (int i = 0; i < 4; i++) if (reg == g8h[i]) { o.rclass = 4; o.ridx = i; return true; }
    if (reg >= X86_REG_XMM0 && reg <= X86_REG_XMM7) { o.rclass = 5; o.ridx = reg - X86_REG_XMM0; return true; }
    if (reg >= X86_REG_ST0 && reg <= X86_REG_ST7) { o.rclass = 6; o.ridx = reg - X86_REG_ST0; return true; }
    if (reg >= X86_REG_MM0 && reg <= X86_REG_MM7) { o.rclass = 7; o.ridx = reg - X86_REG_MM0; return true; }
    return false;
}
static int gpr_index(unsigned reg) {
    static const unsigned g32[] = {X86_REG_EAX, X86_REG_ECX, X86_REG_EDX, X86_REG_EBX, X86_REG_ESP, X86_REG_EBP, X86_REG_ESI, X86_REG_EDI};
    for (int i = 0; i < 8; i++) if (reg == g32[i]) return i;
    return -2;
}

const Insn& Machine::decode(uint32_t pc) {
    if (decoded_.empty()) decoded_.resize(1u << 20);
    auto& page = decoded_[pc >> 12];
    if (page && page[pc & 0xfff]) return *page[pc & 0xfff];
    const uint8_t* code = nullptr; size_t avail = 0;
    for (auto& m : modules) if (pc >= m.start && pc < m.end) { code = m.image.data() + (pc - m.start); avail = std::min<size_t>(16, m.end - pc); break; }
    if (!code) { char b[64]; std::snprintf(b, sizeof b, "execution outside mapped code %08x", pc); throw VmError(b); }
    csh h = reinterpret_cast<csh>(capstone_);
    cs_insn* ci = nullptr;
    if (cs_disasm(h, code, avail, pc, 1, &ci) != 1) { char b[64]; std::snprintf(b, sizeof b, "undecodable %08x", pc); throw VmError(b); }
    if (block_used_ == 0 || block_used_ == 4096) { blocks_.emplace_back(new Insn[4096]); block_used_ = 0; }
    Insn& in = blocks_.back()[block_used_++];
    in.addr = pc; in.len = uint8_t(ci->size); in.next = pc + ci->size;
    std::memcpy(in.raw, ci->bytes, std::min<size_t>(16, ci->size));
    std::string m = ci->mnemonic;
    if (m == "repz ret" || m == "rep ret" || m == "bnd ret") m = "ret";
    if (m == "fucompi") m = "fucomip";
    if (m == "fcompi") m = "fcomip";
    // lock prefix: the interpreter is single-threaded (workers are serialized), so the
    // locked read-modify-write has exactly the unlocked semantics.
    if (m.rfind("lock ", 0) == 0) m = m.substr(5);
    for (auto& ch : m) if (ch == ' ') ch = '_';
    cs_x86& x = ci->detail->x86;
    // SSE movsd vs string movsd: SSE form has an XMM operand.
    if (m == "movsd") {
        bool sse = false;
        for (int i = 0; i < x.op_count; i++) if (x.operands[i].type == X86_OP_REG && x.operands[i].reg >= X86_REG_XMM0 && x.operands[i].reg <= X86_REG_XMM7) sse = true;
        if (!sse) m = "movsd_string";
    }
    int found = -1;
    for (int i = 0; i < M_COUNT; i++) if (m == MN_NAMES[i]) { found = i; break; }
    if (found == -1) { cs_free(ci, 1); char b[96]; std::snprintf(b, sizeof b, "unsupported instruction %s at %08x", m.c_str(), pc); throw VmError(b); }
    if (found == -2) { cs_free(ci, 1); char b[96]; std::snprintf(b, sizeof b, "unsupported string movsd at %08x", pc); throw VmError(b); }
    in.mn = uint16_t(found);
    in.n = x.op_count;
    if (in.n > 3) { cs_free(ci, 1); throw VmError("too many operands"); }
    for (int i = 0; i < in.n; i++) {
        const cs_x86_op& s = x.operands[i]; Operand& o = in.o[i];
        o.size = s.size;
        if (s.type == X86_OP_REG) {
            o.type = 1;
            if (!reg_info(s.reg, o)) { cs_free(ci, 1); char b[96]; std::snprintf(b, sizeof b, "register %u at %08x", s.reg, pc); throw VmError(b); }
        } else if (s.type == X86_OP_IMM) {
            o.type = 2; o.imm = uint32_t(s.imm);
        } else if (s.type == X86_OP_MEM) {
            o.type = 3; o.disp = int32_t(s.mem.disp); o.scale = uint8_t(s.mem.scale);
            o.fs = s.mem.segment == X86_REG_FS;
            o.base = s.mem.base ? int8_t(gpr_index(s.mem.base)) : -1;
            o.index = s.mem.index ? int8_t(gpr_index(s.mem.index)) : -1;
            if (o.base == -2 || o.index == -2) { cs_free(ci, 1); throw VmError("unsupported address register"); }
        }
    }
    cs_free(ci, 1);
    if (!page) { page.reset(new Insn*[4096]); std::fill(page.get(), page.get() + 4096, nullptr); }
    in.fast = select_fast(in);
    page[pc & 0xfff] = &in;
    return in;
}

static constexpr uint32_t FS_BASE = 0x7ffd0000;
#define HOT __attribute__((always_inline)) inline
[[noreturn]] __attribute__((noinline, cold)) static void fail(const char* what) { throw VmError(what); }
static thread_local uint32_t g_watch_pc = 0;
static thread_local const Machine* g_watch_machine = nullptr;   // the machine whose write is logged (the stack's return addresses)
__attribute__((noinline, cold)) static void watch_log(void* out, uint64_t step, uint32_t a, int n, uint64_t hi, uint64_t lo) {
    FILE* f = static_cast<FILE*>(out);
    if (n == 16) std::fprintf(f, "W %llu %08x 16 %016llx%016llx pc %08x", (unsigned long long)step, a, (unsigned long long)hi, (unsigned long long)lo, g_watch_pc);
    else std::fprintf(f, "W %llu %08x %d %016llx pc %08x", (unsigned long long)step, a, n, (unsigned long long)lo, g_watch_pc);
    if (Machine* m = const_cast<Machine*>(g_watch_machine)) {   // diagnostics: ESP and the code addresses on the stack above it (a call chain guess)
        std::fprintf(f, " esp %08x", m->r[4]);
        for (uint32_t sp = m->r[4], k = 0, c = 0; k < 0x300 && c < 10 && m->mem.mapped(sp); sp += 4, k++) {
            uint32_t v = uint32_t(m->mem.read(sp, 4));
            if ((v > 0x401000 && v < 0x916000) || (v >= 0x78400000 && v < 0x78600000)) { std::fprintf(f, " %x:%08x", sp - m->r[4], v); c++; }
        }
    }
    std::fputc('\n', f);
}
HOT uint32_t Machine::ea(const Operand& o) const {
    uint32_t a = uint32_t(o.disp);
    if (o.fs) a += FS_BASE;
    if (o.base >= 0) a += r[o.base];
    if (o.index >= 0) a += r[o.index] * o.scale;
    return a;
}
static inline uint64_t mask(int n) { return n >= 8 ? ~0ull : ((1ull << (8 * n)) - 1); }
HOT uint64_t Machine::get(const Operand& o, int size) {
    int n = size ? size : o.size;
    if (o.type == 3) return mem.read(ea(o), n);
    if (o.type == 1) {
        switch (o.rclass) {
            case 1: return r[o.ridx] & mask(n);
            case 2: return r[o.ridx] & 0xffff;
            case 3: return r[o.ridx] & 0xff;
            case 4: return (r[o.ridx] >> 8) & 0xff;
            case 5: return n >= 8 ? xmm[o.ridx].lo : (xmm[o.ridx].lo & mask(n));
            case 7: if (!mmx_active) fail("MMX read before MMX write"); return n >= 8 ? mmx[o.ridx] : (mmx[o.ridx] & mask(n));
        }
        fail("get register class");
    }
    return (uint64_t(o.imm) & mask(n)) | ((n == 8 && (o.imm >> 31)) ? 0xffffffff00000000ull : 0);
}
HOT void Machine::put(const Operand& o, uint64_t v, int size) {
    int n = size ? size : o.size;
    v &= mask(n);
    if (o.type == 1) {
        switch (o.rclass) {
            case 1: r[o.ridx] = uint32_t(v); return;
            case 2: r[o.ridx] = (r[o.ridx] & 0xffff0000u) | uint32_t(v); return;
            case 3: r[o.ridx] = (r[o.ridx] & 0xffffff00u) | uint32_t(v); return;
            case 4: r[o.ridx] = (r[o.ridx] & 0xffff00ffu) | (uint32_t(v) << 8); return;
            case 7:
                if (!mmx_active && !fpu.empty()) fail("MMX with a live x87 stack");
                mmx_active = true; mmx[o.ridx] = v; return;
            case 5:
                if (n >= 16) { xmm[o.ridx].lo = v; xmm[o.ridx].hi = 0; return; }   // never used: 16-byte puts go through put128
                xmm[o.ridx].lo = (xmm[o.ridx].lo & ~mask(n)) | v; return;
        }
        fail("put register class");
    }
    if (o.type == 3) {
        uint32_t a = ea(o);
        if (__builtin_expect(watch_address != 0, 0) && trace_out && watch_address >= a && watch_address < a + uint32_t(n)) (g_watch_machine = this), watch_log(trace_out, steps, a, n, 0, v);
        mem.write(a, n, v); return; }
    fail("write to immediate");
}
HOT Xmm Machine::get128(const Operand& o) {
    if (o.type == 1 && o.rclass == 5) return xmm[o.ridx];
    if (o.type == 3) { uint32_t a = ea(o); Xmm v; v.lo = mem.read(a, 8); v.hi = mem.read(a + 8, 8); return v; }
    fail("get128 operand");
}
HOT void Machine::put128(const Operand& o, const Xmm& v) {
    if (o.type == 1 && o.rclass == 5) { xmm[o.ridx] = v; return; }
    if (o.type == 3) { uint32_t a = ea(o);
        if (__builtin_expect(watch_address != 0, 0) && trace_out && watch_address >= a && watch_address < a + 16) (g_watch_machine = this), watch_log(trace_out, steps, a, 16, v.hi, v.lo);
        mem.write(a, 8, v.lo); mem.write(a + 8, 8, v.hi); return; }
    fail("put128 operand");
}

// ---------------------------------------------------------------- flags
static const uint32_t CF = 1, PF = 4, ZF = 0x40, SF = 0x80, DF = 0x400, OF = 0x800;
static inline bool parity(uint64_t v) { return (__builtin_popcount(unsigned(v & 0xff)) & 1) == 0; }
void Machine::set_logic(uint64_t res, int size) {
    int bits = 8 * size; res &= mask(size);
    uint32_t f = flags & ~(CF | PF | ZF | SF | OF);
    if (res == 0) f |= ZF;
    if ((res >> (bits - 1)) & 1) f |= SF;
    if (parity(res)) f |= PF;
    flags = f;
}
uint64_t Machine::set_add(uint64_t a, uint64_t b, uint64_t carry, int size, bool sub) {
    int bits = 8 * size; uint64_t m = mask(size), res; bool cf, of;
    if (sub) {
        __int128 full = __int128(a) - __int128(b) - __int128(carry);
        res = uint64_t(full) & m; cf = full < 0;
        of = ((((a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    } else {
        unsigned __int128 full = (unsigned __int128)a + b + carry;
        res = uint64_t(full) & m; cf = full > m;
        of = (((~(a ^ b) & (a ^ res)) >> (bits - 1)) & 1) != 0;
    }
    set_logic(res, size);
    if (cf) flags |= CF;
    if (of) flags |= OF;
    return res;
}
bool Machine::cond(uint16_t mn) const {
    bool cf = flags & CF, zf = flags & ZF, sf = flags & SF, of = flags & OF, pf = flags & PF;
    switch (mn) {
#define C3(J, S, CM, E) case M_##J: case M_##S: case M_##CM: return (E);
        C3(jo, seto, cmovo, of) C3(jno, setno, cmovno, !of) C3(jb, setb, cmovb, cf) C3(jae, setae, cmovae, !cf)
        C3(je, sete, cmove, zf) C3(jne, setne, cmovne, !zf) C3(jbe, setbe, cmovbe, cf || zf) C3(ja, seta, cmova, !cf && !zf)
        C3(js, sets, cmovs, sf) C3(jns, setns, cmovns, !sf) C3(jp, setp, cmovp, pf) C3(jnp, setnp, cmovnp, !pf)
        C3(jl, setl, cmovl, sf != of) C3(jge, setge, cmovge, sf == of) C3(jle, setle, cmovle, zf || sf != of) C3(jg, setg, cmovg, !zf && sf == of)
#undef C3
    }
    throw VmError("condition");
}

// ---------------------------------------------------------------- stack/natives
void Machine::push(uint32_t v) { r[4] -= 4; if (__builtin_expect(watch_address != 0, 0) && trace_out && watch_address >= r[4] && watch_address < r[4] + 4u) (g_watch_machine = this), watch_log(trace_out, steps, r[4], 4, 0, v); mem.write(r[4], 4, v); }
uint32_t Machine::pop() { uint32_t v = uint32_t(mem.read(r[4], 4)); r[4] += 4; return v; }
uint32_t Machine::arg(int i) { return uint32_t(mem.read(r[4] + 4 + 4 * i, 4)); }
uint32_t Machine::ret(uint32_t cleanup, int64_t eax) {
    if (eax >= 0) r[0] = uint32_t(eax);
    uint32_t target = pop(); r[4] += cleanup; return target;
}
uint32_t Machine::enter_native(uint32_t pc) {
    auto n = natives.find(pc);
    if (n != natives.end()) return n->second(*this);
    // the handler of an import sentinel, resolved by name once (handlers live in `imports`' nodes: replacing one keeps its node)
    if (import_cache_size_ != imports.size()) { import_cache_.clear(); import_cache_size_ = imports.size(); }
    if (auto c = import_cache_.find(pc); c != import_cache_.end()) return (*c->second)(*this);
    auto name = import_names.find(pc);
    auto h = imports.find(name->second);
    if (h != imports.end()) import_cache_[pc] = &h->second;
    if (h == imports.end()) {
        char where[64]; std::snprintf(where, sizeof where, " (return %08x, step %llu)", uint32_t(mem.read(r[4], 4)), (unsigned long long)steps);
        throw VmError("unmodelled import " + name->second + where);
    }
    return h->second(*this);
}
void Machine::enable_jit(bool on) {
    jit_enabled = on;
    if (on && !jit) { jit.reset(new Jit(*this)); if (!jit->ok()) { jit.reset(); jit_enabled = false; } }
}
void Machine::refresh_special() {
    if (jit) jit->flush();   // natives added or removed: translated blocks may now contain one
    import_cache_.clear();
    special_.assign(1u << 20, 0);
    for (auto& [a, f] : natives) special_[a >> 12] = 1;
    for (auto& [a, n] : import_names) special_[a >> 12] = 1;
    special_count_ = natives.size() + import_names.size();
}
uint32_t Machine::run(uint32_t pc, uint64_t budget) {
    uint64_t limit = steps + budget;
    const Insn* prev = nullptr;
    bool bypass = false, jit_declined = false;
    const bool plain = !trace_out && !trace_every && !histogram && !pc_counts;
    while (pc != 0) {
        if (__builtin_expect(pc == stop_pc, 0) && r[4] > stop_esp) break;
        if (steps >= limit) throw VmError("instruction budget");
        if (__builtin_expect(steps >= slice_end, 0)) { sliced = true; return pc; }
        if (natives.size() + import_names.size() != special_count_) refresh_special();
        if (special_[pc >> 12] && (natives.count(pc) || import_names.count(pc)) && !bypass) {
            uint32_t next_pc = enter_native(pc); prev = nullptr;
            if (next_pc == RUN_ORIGINAL) bypass = true; else pc = next_pc;
            continue;
        }
        if (jit_enabled && plain && !bypass && !jit_declined) {
            if (Jit::BlockFn fn = jit->lookup(pc)) {
                uint64_t res = fn(this);
                if ((res >> 33) == 0) {
                    if (res >> 32) { std::exception_ptr e = jit_exception; jit_exception = nullptr; std::rethrow_exception(e); }
                    pc = uint32_t(res); prev = nullptr;
                    continue;
                }
                // declined (possibly by a block chained from this one): interpret from that block's start
                pc = uint32_t(res); prev = nullptr; jit_declined = true;
                continue;
            }
        }
        jit_declined = false;
        bypass = false;
        const Insn* ip;
        if (prev && pc == prev->next && prev->fall) ip = prev->fall;
        else if (prev && prev->taken && prev->taken->addr == pc) ip = prev->taken;
        else {
            ip = &decode(pc);
            if (prev) { if (pc == prev->next) const_cast<Insn*>(prev)->fall = ip; else const_cast<Insn*>(prev)->taken = ip; }
        }
        const Insn& in = *ip; prev = ip;
        if (__builtin_expect(watch_address != 0, 0)) g_watch_pc = in.addr;
        if (__builtin_expect(histogram != nullptr, 0)) { histogram[in.mn]++; if (!in.fast) histogram[M_COUNT + in.mn]++; }
        if (__builtin_expect(pc_counts != nullptr, 0)) {
            if (pc - 0x400000u < 0x600000u) pc_counts[pc - 0x400000u]++;
            else { outside_exe++; if (dll_counts && pc - 0x78400000u < 0x200000u) dll_counts[pc - 0x78400000u]++; }
        }   // [M_COUNT..): generic (no fast path)
        if (trace_out && steps >= trace_from && steps < trace_to && steps % trace_mod == 0) {
            std::fprintf(static_cast<FILE*>(trace_out), "%llu %08x", (unsigned long long)steps, pc);
            for (int i = 0; i < 8; i++) std::fprintf(static_cast<FILE*>(trace_out), " %08x", r[i]);
            for (int i = 0; i < 8; i++) std::fprintf(static_cast<FILE*>(trace_out), " %016llx%016llx", (unsigned long long)xmm[i].hi, (unsigned long long)xmm[i].lo);
            std::fprintf(static_cast<FILE*>(trace_out), " %03x\n", flags & 0x8d5);
        }
        if (trace_every && steps % trace_every == 0 && trace_out) {
            std::fprintf(static_cast<FILE*>(trace_out), "%llu %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %016llx%016llx", (unsigned long long)steps, pc,
                         r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], flags & 0x8d5, (unsigned long long)xmm[0].hi, (unsigned long long)xmm[0].lo);
            // profiling: words on the stack that look like return addresses into the executable (outermost last)
            for (uint32_t a = r[4], k = 0, n = 0; k < 2048 && n < 24 && mem.mapped(a); a += 4, k++) {
                uint32_t v = uint32_t(mem.read(a, 4));
                if (v > 0x401000 && v < 0x916000) { std::fprintf(static_cast<FILE*>(trace_out), " r%08x", v); n++; }
            }
            std::fputc('\n', static_cast<FILE*>(trace_out));
        }
        // Execute; while not tracing, follow decoded fall-through chains directly.
        const Insn* cur = &in;
        try {
            for (;;) {
                steps++;
                if (__builtin_expect(jit_histo != nullptr, 0)) jit_histo[M_COUNT + cur->mn]++;
                uint32_t npc = cur->fast ? cur->fast(*this, *cur) : execute(*cur);
                if (jit_enabled && npc != cur->next) { pc = npc; prev = cur; break; }   // a branch target: back to the JIT's lookup
                if (plain && steps < limit && steps < slice_end && !special_[npc >> 12] && npc != stop_pc) {   // (stop_pc: Verify's return)
                    if (npc == cur->next) { if (cur->fall) { cur = cur->fall; continue; } }
                    else if (cur->taken && cur->taken->addr == npc) { cur = cur->taken; continue; }
                }
                pc = npc; prev = cur;
                break;
            }
        }
        catch (const VmError& e) {
            if (std::strncmp(e.what(), "[step", 5) == 0) throw;
            char b[96]; std::snprintf(b, sizeof b, "[step %llu] %08x %s: ", (unsigned long long)steps, cur->addr, MN_NAMES[cur->mn]); throw VmError(b + std::string(e.what())); }
    }
    return pc;
}
uint32_t Machine::call_function(uint32_t address, const uint32_t* args, int nargs, int64_t ecx, int64_t eax) {
    if (ecx >= 0) r[1] = uint32_t(ecx);
    if (eax >= 0) r[0] = uint32_t(eax);
    for (int i = nargs - 1; i >= 0; i--) push(args[i]);
    push(0);
    run(address, call_budget);
    return r[0];
}

uint32_t Machine::call_at(uint32_t address, uint32_t ra, bool esp_below) {
    const uint32_t sp0 = stop_pc, se0 = stop_esp;
    // frame-allocating helpers return with ESP below the slot: __SEH_prolog4, __EH_prolog3_catch, __alloca_probe
    // (and the CRT DLLs' own: msvcp90 __EH_prolog3 784b952f, __SEH_prolog4 784b9ff8; msvcr90 _alloca_probe_16 78545f40,
    // _alloca_probe_8 78545f56, _chkstk 78545f80)
    switch (address) {
        case 0x008fa554u: case 0x008fa66eu: case 0x008fa230u:
        case 0x784b952fu: case 0x784b9ff8u: case 0x78545f40u: case 0x78545f56u: case 0x78545f80u: esp_below = true; break;
        default: break;
    }
    push(ra); stop_pc = ra; stop_esp = esp_below ? 0 : r[4];
    uint32_t pc;
    try { pc = run(address, call_budget); } catch (...) { stop_pc = sp0; stop_esp = se0; throw; }
    stop_pc = sp0; stop_esp = se0;
    return pc;
}

uint32_t Machine::tail_at(uint32_t address) {
    const uint32_t ra = uint32_t(mem.read(r[4], 4)), sp0 = stop_pc, se0 = stop_esp;
    stop_pc = ra; stop_esp = r[4];
    try { run(address, call_budget); } catch (...) { stop_pc = sp0; stop_esp = se0; throw; }
    stop_pc = sp0; stop_esp = se0;
    return ra;
}

// ---------------------------------------------------------------- SSE helpers
static inline float bf(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
static inline uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
static inline double bd(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
static inline uint64_t db(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
static inline bool nan32(uint32_t b) { return (b & 0x7fffffff) > 0x7f800000; }
static inline bool nan64(uint64_t b) { return (b & 0x7fffffffffffffffull) > 0x7ff0000000000000ull; }
// RSQRTPS/RCPPS: the approximations as executed for the original (exact tables;
// result depends on exponent parity (rsqrt) and the top 10/11 mantissa bits).
#include "rsqrt_rcp_tables.inc"
static uint32_t rsqrt32(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s ? 0xffc00000u : 0u);
    if (e == 0) return s ? 0xff800000u : 0x7f800000u;           // denormals count as zero
    if (s) return 0xffc00000u;
    uint32_t ref = (e & 1) ? 127 : 126;
    uint32_t r = (e & 1) ? RSQRT_127[m >> 13] : RSQRT_126[m >> 13];
    int32_t re = int32_t((r >> 23) & 255) + (int32_t(ref) - int32_t(e)) / 2;
    return (uint32_t(re) << 23) | (r & 0x7fffff);
}
static uint32_t rcp32(uint32_t x) {
    uint32_t s = x >> 31, e = (x >> 23) & 255, m = x & 0x7fffff;
    if (e == 255) return m ? (x | 0x400000) : (s << 31);
    if (e == 0) return (s << 31) | 0x7f800000u;
    uint32_t r = RCP_127[m >> 12];
    int32_t re = int32_t((r >> 23) & 255) + (127 - int32_t(e));
    if (re <= 0) return s << 31;                                   // tiny results flush to zero
    return (s << 31) | (uint32_t(re) << 23) | (r & 0x7fffff);
}
enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MIN, OP_MAX };
static uint32_t sse32(int op, uint32_t a, uint32_t b) {
    // MINSS / MAXSS / MINPS / MAXPS: the second (source) operand unless the comparison holds, which it never does with a NaN, so a NaN in
    // either operand gives the source unchanged (Intel SDM; no quieting). Not the NaN propagation of the arithmetic below.
    if (op == OP_MIN) return bf(a) < bf(b) ? a : b;
    if (op == OP_MAX) return bf(a) > bf(b) ? a : b;
    if (nan32(a)) return a | 0x400000;
    if (nan32(b)) return b | 0x400000;
    volatile float x = bf(a), y = bf(b), r;
    switch (op) {
        case OP_ADD: r = x + y; break;
        case OP_SUB: r = x - y; break;
        case OP_MUL: r = x * y; break;
        case OP_DIV: r = x / y; break;
        case OP_MIN: return x < y ? a : b;
        default: return x > y ? a : b;
    }
    uint32_t out = fb(r);
    return nan32(out) ? 0xffc00000u : out;
}
// Packed single add/sub/mul/div on the host vector unit. IEEE results are identical
// to x86 when no lane involves a NaN (x86 NaN propagation differs from the host's)
// and MXCSR is round-to-nearest without FTZ/DAZ; otherwise the exact per-lane path runs.
#if defined(__aarch64__)
#include <arm_neon.h>
static inline bool ps_fast(int op, Xmm& a, const Xmm& b, uint32_t mxcsr) {
    if ((mxcsr & 0xe040) != 0) return false;
    float32x4_t x = vreinterpretq_f32_u64(vcombine_u64(vcreate_u64(a.lo), vcreate_u64(a.hi)));
    float32x4_t y = vreinterpretq_f32_u64(vcombine_u64(vcreate_u64(b.lo), vcreate_u64(b.hi)));
    float32x4_t r;
    switch (op) {
        case OP_ADD: r = vaddq_f32(x, y); break;
        case OP_SUB: r = vsubq_f32(x, y); break;
        case OP_MUL: r = vmulq_f32(x, y); break;
        default: r = vdivq_f32(x, y); break;
    }
    // any NaN among inputs or results -> exact path
    uint32x4_t ok = vandq_u32(vandq_u32(vceqq_f32(x, x), vceqq_f32(y, y)), vceqq_f32(r, r));
    if (vminvq_u32(ok) == 0) return false;
    uint64x2_t out = vreinterpretq_u64_f32(r);
    a.lo = vgetq_lane_u64(out, 0); a.hi = vgetq_lane_u64(out, 1);
    return true;
}
#else
static inline bool ps_fast(int, Xmm&, const Xmm&, uint32_t) { return false; }
#endif
static uint64_t sse64(int op, uint64_t a, uint64_t b) {
    if (op == OP_MIN) return bd(a) < bd(b) ? a : b;   // MINSD / MAXSD / MINPD / MAXPD: as sse32
    if (op == OP_MAX) return bd(a) > bd(b) ? a : b;
    if (nan64(a)) return a | 0x8000000000000ull;
    if (nan64(b)) return b | 0x8000000000000ull;
    volatile double x = bd(a), y = bd(b), r;
    switch (op) {
        case OP_ADD: r = x + y; break;
        case OP_SUB: r = x - y; break;
        case OP_MUL: r = x * y; break;
        case OP_DIV: r = x / y; break;
        case OP_MIN: return x < y ? a : b;
        default: return x > y ? a : b;
    }
    uint64_t out = db(r);
    return nan64(out) ? 0xfff8000000000000ull : out;
}
static uint32_t sqrt32(uint32_t a) {
    if (nan32(a)) return a | 0x400000;
    if (a == 0x80000000u) return a;
    if (a >> 31) return 0xffc00000u;
    volatile float x = bf(a); volatile float r = std::sqrt(x); return fb(r);
}
static inline uint32_t lane32(const Xmm& v, int i) { return uint32_t((i < 2 ? v.lo : v.hi) >> (32 * (i & 1))); }
static inline void set_lane32(Xmm& v, int i, uint32_t x) {
    uint64_t& w = i < 2 ? v.lo : v.hi; int s = 32 * (i & 1);
    w = (w & ~(0xffffffffull << s)) | (uint64_t(x) << s);
}
static inline uint64_t lane64(const Xmm& v, int i) { return i ? v.hi : v.lo; }
static inline int32_t to_int32_trunc(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::trunc(x);
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}
static inline int32_t to_int32_nearest(double x, bool& ok) {
    if (std::isnan(x) || std::isinf(x)) { ok = false; return 0; }
    double t = std::nearbyint(x);            // default rounding: nearest-even
    if (t < -2147483648.0 || t >= 2147483648.0) { ok = false; return 0; }
    ok = true; return int32_t(t);
}

// ---------------------------------------------------------------- execute
#include "fast.inc"

uint32_t Machine::execute(const Insn& in) {
    const Operand* o = in.o;
    const uint32_t next = in.next;
    switch (in.mn) {
    case M_mov: put(o[0], get(o[1], o[0].size)); return next;
    case M_movaps: case M_movups: case M_movdqa: case M_movdqu: case M_movapd: case M_movupd: put128(o[0], get128(o[1])); return next;
    case M_movss: {
        uint64_t v = get(o[1], 4);
        if (o[0].type == 1 && o[1].type == 3) { xmm[o[0].ridx].lo = v; xmm[o[0].ridx].hi = 0; }
        else put(o[0], v, 4);
        return next; }
    case M_movsd: {
        uint64_t v = get(o[1], 8);
        if (o[0].type == 1 && o[1].type == 3) { xmm[o[0].ridx].lo = v; xmm[o[0].ridx].hi = 0; }
        else put(o[0], v, 8);
        return next; }
    case M_movd: {
        uint64_t v = get(o[1], 4);
        if (o[0].type == 1 && o[0].rclass == 5) { xmm[o[0].ridx].lo = v; xmm[o[0].ridx].hi = 0; } else put(o[0], v, 4);
        return next; }
    case M_movq: {
        uint64_t v = get(o[1], 8);
        if (o[0].type == 1 && o[0].rclass == 5) { xmm[o[0].ridx].lo = v; xmm[o[0].ridx].hi = 0; } else put(o[0], v, 8);
        return next; }
    case M_movzx: put(o[0], get(o[1])); return next;
    case M_movsx: { int n = o[1].size; uint64_t v = get(o[1]); uint64_t sign = 1ull << (8 * n - 1); put(o[0], uint64_t((int64_t)((v ^ sign) - sign))); return next; }
    case M_lea: put(o[0], ea(o[1])); return next;
    case M_xchg: { uint64_t a = get(o[0]), b = get(o[1]); put(o[0], b); put(o[1], a); return next; }
    case M_push: push(uint32_t(get(o[0], 4))); return next;
    case M_pop: { uint32_t v = pop(); put(o[0], v, 4); return next; }
    case M_leave: r[4] = r[5]; r[5] = pop(); return next;
    case M_pushal: { uint32_t sp = r[4]; for (int i = 0; i < 8; i++) push(i == 4 ? sp : r[i]); return next; }
    case M_popal: for (int i = 7; i >= 0; i--) { uint32_t v = pop(); if (i != 4) r[i] = v; } return next;
    case M_pushfd: push(flags); return next;
    case M_popfd: flags = (pop() & 0x00244dd5u) | 2; return next;
    case M_xlatb: r[0] = (r[0] & 0xffffff00u) | uint32_t(mem.read(r[3] + (r[0] & 0xff), 1)); return next;
    case M_cdq: r[2] = (r[0] >> 31) ? 0xffffffffu : 0; return next;
    case M_cwde: { uint32_t v = r[0] & 0xffff; r[0] = uint32_t(int32_t(int16_t(v))); return next; }
    case M_lahf: r[0] = (r[0] & 0xffff00ffu) | (((flags & 0xd5) | 2) << 8); return next;
    case M_sahf: flags = (flags & ~0xd5u) | ((r[0] >> 8) & 0xd5); return next;
    case M_cld: flags &= ~DF; return next;
    case M_std: flags |= DF; return next;
    case M_nop: case M_wait: case M_fwait: case M_prefetcht0: case M_prefetchnta: case M_prefetcht1: case M_prefetcht2: return next;
    // CPUID: a family-6 Intel with SSE3 (no SSSE3/SSE4: the interpreter's instruction set), 1 logical CPU per package.
    case M_cpuid: { cpuid(r[0], r[3], r[1], r[2]); return next; }   // r[]: eax ecx edx ebx
    case M_rdtsc: { uint64_t t = steps; r[0] = uint32_t(t); r[2] = uint32_t(t >> 32); return next; }
    case M_jmp: return o[0].type == 2 ? o[0].imm : uint32_t(get(o[0], 4));
    case M_jecxz: return r[1] == 0 ? o[0].imm : next;
    case M_jo: case M_jno: case M_jb: case M_jae: case M_je: case M_jne: case M_jbe: case M_ja:
    case M_js: case M_jns: case M_jp: case M_jnp: case M_jl: case M_jge: case M_jle: case M_jg:
        return cond(in.mn) ? o[0].imm : next;
    case M_seto: case M_setno: case M_setb: case M_setae: case M_sete: case M_setne: case M_setbe: case M_seta:
    case M_sets: case M_setns: case M_setp: case M_setnp: case M_setl: case M_setge: case M_setle: case M_setg:
        put(o[0], cond(in.mn) ? 1 : 0, 1); return next;
    case M_cmovo: case M_cmovno: case M_cmovb: case M_cmovae: case M_cmove: case M_cmovne: case M_cmovbe: case M_cmova:
    case M_cmovs: case M_cmovns: case M_cmovp: case M_cmovnp: case M_cmovl: case M_cmovge: case M_cmovle: case M_cmovg:
        if (cond(in.mn)) put(o[0], get(o[1]));
        return next;
    case M_call: { uint32_t target = o[0].type == 2 ? o[0].imm : uint32_t(get(o[0], 4)); push(next); return target; }
    case M_ret: { uint32_t v = pop(); if (in.n) r[4] += o[0].imm; return v; }
    case M_add: case M_adc: case M_sub: case M_sbb: case M_cmp: {
        int size = o[0].size; uint64_t a = get(o[0]), b = get(o[1], size);
        uint64_t carry = (in.mn == M_adc || in.mn == M_sbb) ? (flags & CF) : 0;
        uint64_t res = set_add(a, b, carry, size, in.mn == M_sub || in.mn == M_sbb || in.mn == M_cmp);
        if (in.mn != M_cmp) put(o[0], res);
        return next; }
    case M_and: case M_or: case M_xor: case M_test: {
        int size = o[0].size; uint64_t a = get(o[0]), b = get(o[1], size);
        uint64_t res = (in.mn == M_and || in.mn == M_test) ? (a & b) : in.mn == M_or ? (a | b) : (a ^ b);
        set_logic(res, size);
        if (in.mn != M_test) put(o[0], res);
        return next; }
    case M_scasb: case M_repe_scasb: case M_repne_scasb: case M_rep_scasb: case M_scasw: case M_repe_scasw: case M_repne_scasw: case M_rep_scasw: case M_scasd: case M_repe_scasd: case M_repne_scasd: case M_rep_scasd: case M_cmpsb: case M_repe_cmpsb: case M_repne_cmpsb: case M_rep_cmpsb: case M_cmpsw: case M_repe_cmpsw: case M_repne_cmpsw: case M_rep_cmpsw: case M_cmpsd: case M_repe_cmpsd: case M_repne_cmpsd: case M_rep_cmpsd: case M_lodsb: case M_rep_lodsb: case M_lodsw: case M_rep_lodsw: case M_lodsd: case M_rep_lodsd: {
        // String compare/scan/load: scas (accumulator vs [edi]), cmps ([esi] vs [edi]),
        // lods (accumulator <- [esi]); rep/repe repeat while ZF=1, repne while ZF=0.
        const char* nm = MN_NAMES[in.mn];
        std::string name(nm);
        bool repe = name.rfind("repe_", 0) == 0 || name.rfind("rep_", 0) == 0, repne = name.rfind("repne_", 0) == 0, rep = repe || repne;
        std::string op = name.substr(rep ? name.find('_') + 1 : 0);
        int size = op.back() == 'b' ? 1 : op.back() == 'w' ? 2 : 4;
        int32_t step = (flags & DF) ? -size : size;
        bool scas = op.rfind("scas", 0) == 0, cmps = op.rfind("cmps", 0) == 0;
        if (rep && r[1] == 0) return next;
        while (true) {
            if (scas) { set_add(r[0] & mask(size), mem.read(r[7], size), 0, size, true); r[7] += step; }
            else if (cmps) { set_add(mem.read(r[6], size), mem.read(r[7], size), 0, size, true); r[6] += step; r[7] += step; }
            else { uint64_t v = mem.read(r[6], size); r[6] += step;
                   if (size == 4) r[0] = uint32_t(v); else if (size == 2) r[0] = (r[0] & 0xffff0000u) | uint32_t(v); else r[0] = (r[0] & 0xffffff00u) | uint32_t(v); }
            if (!rep) break;
            r[1]--;
            if (r[1] == 0) break;
            if (scas || cmps) { bool zf = (flags & ZF) != 0; if ((repe && !zf) || (repne && zf)) break; }
        }
        return next; }
    case M_cmpxchg: {            // compare accumulator with dst: equal -> dst = src, else accumulator = dst
        int size = o[0].size; uint64_t dst = get(o[0]), acc = r[0] & mask(size);
        set_add(acc, dst, 0, size, true);
        if (flags & ZF) put(o[0], get(o[1], size));
        else { if (size == 4) r[0] = uint32_t(dst); else if (size == 2) r[0] = (r[0] & 0xffff0000u) | uint32_t(dst); else r[0] = (r[0] & 0xffffff00u) | uint32_t(dst); }
        return next; }
    case M_xadd: {               // tmp = dst + src; src = dst; dst = tmp
        int size = o[0].size; uint64_t dst = get(o[0]), src = get(o[1], size);
        uint64_t res = set_add(dst, src, 0, size, false);
        put(o[1], dst); put(o[0], res); return next; }
    case M_inc: case M_dec: {
        int size = o[0].size; uint32_t cf = flags & CF;
        uint64_t res = set_add(get(o[0]), 1, 0, size, in.mn == M_dec);
        flags = (flags & ~CF) | cf; put(o[0], res); return next; }
    case M_neg: { int size = o[0].size; uint64_t res = set_add(0, get(o[0]), 0, size, true); put(o[0], res); return next; }
    case M_not: put(o[0], ~get(o[0])); return next;
    case M_shl: case M_sal: case M_shr: case M_sar: case M_rol: case M_ror: {
        int size = o[0].size, bits = 8 * size; uint64_t v = get(o[0]);
        unsigned count = unsigned(in.n > 1 ? get(o[1], 1) : 1) & 31;
        if (!count) return next;
        uint64_t res; bool cf, of;
        if (in.mn == M_shl || in.mn == M_sal) {
            res = (v << count) & mask(size); cf = (v >> (bits - count)) & 1; set_logic(res, size); of = (((res >> (bits - 1)) & 1) ^ cf) != 0;
        } else if (in.mn == M_shr) {
            res = v >> count; cf = (v >> (count - 1)) & 1; set_logic(res, size); of = (v >> (bits - 1)) & 1;
        } else if (in.mn == M_sar) {
            int64_t sv = int64_t((v ^ (1ull << (bits - 1))) - (1ull << (bits - 1)));
            res = uint64_t(sv >> count) & mask(size); cf = (sv >> (count - 1)) & 1; set_logic(res, size); of = false;
        } else {
            unsigned c = count % bits;
            res = in.mn == M_rol ? (((v << c) | (v >> (bits - c))) & mask(size)) : (((v >> c) | (v << (bits - c))) & mask(size));
            if (c == 0) res = v;
            cf = in.mn == M_rol ? (res & 1) : ((res >> (bits - 1)) & 1); of = false;
        }
        flags = (flags & ~(CF | OF)) | (cf ? CF : 0) | ((of && count == 1) ? OF : 0);
        put(o[0], res); return next; }
    case M_shld: case M_shrd: {
        int size = o[0].size, bits = 8 * size; uint64_t a = get(o[0]), b = get(o[1]); unsigned count = unsigned(get(o[2], 1)) & 31;
        if (count) {
            uint64_t res; bool cf;
            if (in.mn == M_shld) { res = ((a << count) | (b >> (bits - count))) & mask(size); cf = (a >> (bits - count)) & 1; }
            else { res = ((a >> count) | (b << (bits - count))) & mask(size); cf = (a >> (count - 1)) & 1; }
            set_logic(res, size); if (cf) flags |= CF; put(o[0], res);
        }
        return next; }
    case M_imul: {
        int64_t p; int size = o[0].size;
        auto sx = [](uint64_t v, int n) { return n == 1 ? int64_t(int8_t(v)) : n == 2 ? int64_t(int16_t(v)) : int64_t(int32_t(uint32_t(v))); };
        if (in.n == 1) {
            if (size == 4) { p = int64_t(int32_t(r[0])) * sx(get(o[0]), 4); r[0] = uint32_t(p); r[2] = uint32_t(uint64_t(p) >> 32); }
            else if (size == 2) { p = sx(r[0], 2) * sx(get(o[0]), 2); r[0] = (r[0] & 0xffff0000u) | (uint32_t(p) & 0xffff); r[2] = (r[2] & 0xffff0000u) | ((uint32_t(p) >> 16) & 0xffff); }
            else { p = sx(r[0], 1) * sx(get(o[0]), 1); r[0] = (r[0] & 0xffff0000u) | (uint32_t(p) & 0xffff); }
        } else {
            uint64_t x = in.n == 3 ? get(o[1]) : get(o[0]);
            uint64_t y = in.n == 3 ? get(o[2], size) : get(o[1]);
            p = sx(x, size) * sx(y, size);
            put(o[0], uint64_t(p));
        }
        int64_t lo = size == 1 ? INT8_MIN : size == 2 ? INT16_MIN : INT32_MIN, hi = size == 1 ? INT8_MAX : size == 2 ? INT16_MAX : INT32_MAX;
        bool over = p < lo || p > hi;                          // result differs from its sign-extended low half
        flags = (flags & ~(CF | OF)) | (over ? (CF | OF) : 0);
        return next; }
    case M_mul: {
        int size = o[0].size; bool high;
        if (size == 4) { uint64_t p = uint64_t(r[0]) * get(o[0]); r[0] = uint32_t(p); r[2] = uint32_t(p >> 32); high = r[2] != 0; }
        else if (size == 2) { uint32_t p = (r[0] & 0xffff) * uint32_t(get(o[0])); r[0] = (r[0] & 0xffff0000u) | (p & 0xffff); r[2] = (r[2] & 0xffff0000u) | (p >> 16); high = (p >> 16) != 0; }
        else { uint32_t p = (r[0] & 0xff) * uint32_t(get(o[0])); r[0] = (r[0] & 0xffff0000u) | (p & 0xffff); high = (p >> 8) != 0; }
        flags = (flags & ~(CF | OF)) | (high ? (CF | OF) : 0); return next; }
    case M_div: case M_idiv: {
        int size = o[0].size;
        uint64_t d = get(o[0]);
        if (d == 0) throw VmError("division by zero");
        if (size == 4) {
            uint64_t n = (uint64_t(r[2]) << 32) | r[0];
            if (in.mn == M_div) {
                uint64_t q = n / d, rem = n % d; if (q > 0xffffffffull) throw VmError("div overflow");
                r[0] = uint32_t(q); r[2] = uint32_t(rem);
            } else {
                int64_t sn = int64_t(n), sd = int32_t(uint32_t(d));
                int64_t q = sn / sd, rem = sn % sd;                    // C++: truncation toward zero, as x86
                if (q < INT32_MIN || q > INT32_MAX) throw VmError("idiv overflow");
                r[0] = uint32_t(q); r[2] = uint32_t(rem);
            }
        } else if (size == 2) {
            uint32_t n = ((r[2] & 0xffff) << 16) | (r[0] & 0xffff);
            int64_t q, rem;
            if (in.mn == M_div) { q = n / d; rem = n % d; if (q > 0xffff) throw VmError("div overflow"); }
            else { int32_t sn = int32_t(n), sd = int16_t(d); q = sn / sd; rem = sn % sd; if (q < INT16_MIN || q > INT16_MAX) throw VmError("idiv overflow"); }
            r[0] = (r[0] & 0xffff0000u) | (uint32_t(q) & 0xffff); r[2] = (r[2] & 0xffff0000u) | (uint32_t(rem) & 0xffff);
        } else {
            uint32_t n = r[0] & 0xffff;
            int64_t q, rem;
            if (in.mn == M_div) { q = n / d; rem = n % d; if (q > 0xff) throw VmError("div overflow"); }
            else { int32_t sn = int16_t(n), sd = int8_t(d); q = sn / sd; rem = sn % sd; if (q < INT8_MIN || q > INT8_MAX) throw VmError("idiv overflow"); }
            r[0] = (r[0] & 0xffff0000u) | (uint32_t(q) & 0xff) | ((uint32_t(rem) & 0xff) << 8);
        }
        return next; }
    case M_bt: case M_bts: case M_btr: case M_btc: {
        // Bit test family. A memory destination with a register bit offset addresses a
        // bit string: the signed offset selects the operand-sized word (x86 semantics).
        int size = o[0].size, bits = 8 * size;
        uint32_t off = uint32_t(get(o[1]));
        Operand d = o[0];
        if (d.type == 3 && o[1].type == 1) {
            int32_t so = size == 2 ? int32_t(int16_t(off)) : int32_t(off);
            d.disp += (so >> (size == 2 ? 4 : 5)) * size;
            off = uint32_t(so) & uint32_t(bits - 1);
        } else off &= uint32_t(bits - 1);
        uint64_t v = get(d, size), m = 1ull << off;
        flags = (flags & ~CF) | ((v >> off) & 1);
        if (in.mn == M_bts) put(d, v | m, size);
        else if (in.mn == M_btr) put(d, v & ~m, size);
        else if (in.mn == M_btc) put(d, v ^ m, size);
        return next; }
    case M_bsf: case M_bsr: {
        uint64_t v = get(o[1]);
        if (!v) { flags |= ZF; return next; }
        flags &= ~ZF; put(o[0], in.mn == M_bsf ? __builtin_ctzll(v) : 63 - __builtin_clzll(v)); return next; }
    case M_rep_movsd: case M_rep_movsb: case M_movsb: case M_movsd_string: case M_rep_stosd: case M_rep_stosb: case M_stosd: case M_stosb:
    case M_movsw: case M_rep_movsw: case M_stosw: case M_rep_stosw: {
        bool rep = in.mn == M_rep_movsd || in.mn == M_rep_movsb || in.mn == M_rep_stosd || in.mn == M_rep_stosb || in.mn == M_rep_movsw || in.mn == M_rep_stosw;
        bool movs = in.mn == M_rep_movsd || in.mn == M_rep_movsb || in.mn == M_movsb || in.mn == M_movsd_string || in.mn == M_movsw || in.mn == M_rep_movsw;
        int size = (in.mn == M_rep_movsd || in.mn == M_rep_stosd || in.mn == M_stosd || in.mn == M_movsd_string) ? 4
                 : (in.mn == M_movsw || in.mn == M_rep_movsw || in.mn == M_stosw || in.mn == M_rep_stosw) ? 2 : 1;
        int32_t step = (flags & DF) ? -size : size;
        uint32_t count = rep ? r[1] : 1;
        // long forward copies / fills whose ranges do not overlap: the same bytes as the element loop, page-wise
        uint64_t bytes = uint64_t(count) * uint32_t(size);
        if (rep && step > 0 && count >= 16 && bytes <= (1u << 24) && uint64_t(r[7]) + bytes <= 0x100000000ull &&
            (!movs || (uint64_t(r[6]) + bytes <= 0x100000000ull && (uint64_t(r[7]) + bytes <= r[6] || uint64_t(r[6]) + bytes <= r[7])))) {
            std::vector<uint8_t> buf(bytes);
            if (movs) { mem.read_block(r[6], buf.data(), bytes); r[6] += uint32_t(bytes); }
            else for (uint64_t i = 0; i < bytes; i += uint32_t(size)) std::memcpy(buf.data() + i, &r[0], size_t(size));
            mem.write_block(r[7], buf.data(), bytes); r[7] += uint32_t(bytes);
            count = 0;
        }
        for (uint32_t i = 0; i < count; i++) {
            if (movs) { mem.write(r[7], size, mem.read(r[6], size)); r[6] += step; }
            else mem.write(r[7], size, r[0] & mask(size));
            r[7] += step;
        }
        if (rep) r[1] = 0;
        return next; }
    default:
        return execute_fp(in);
    }
}

// ---------------------------------------------------------------- x87 helpers
int Machine::fbits() const {
    int pc = (fpu_cw >> 8) & 3;
    if (pc == 1) throw VmError("x87 reserved precision control");
    return pc == 0 ? 24 : pc == 2 ? 53 : 64;
}
F80& Machine::st(int i) {
    if (i >= int(fpu.size())) throw VmError("x87 stack underflow");
    return fpu[fpu.size() - 1 - i];
}
void Machine::fpush(const F80& v) { if (fpu.size() >= 8) throw VmError("x87 stack overflow"); fpu.push_back(v); }
void Machine::fpop() { if (fpu.empty()) throw VmError("x87 stack underflow"); fpu.pop_back(); }
F80 Machine::fload(const Operand& o) {
    if (o.type == 1) return st(o.ridx);
    if (o.size == 4) return reflex_f80::from_f32(uint32_t(get(o)));
    if (o.size == 8) return reflex_f80::from_f64(get(o));
    if (o.size == 10) { uint8_t raw[10]; mem.read_bytes(ea(o), raw, 10); return reflex_f80::from_f80(raw); }
    throw VmError("x87 memory operand size");
}
void Machine::fstore(const Operand& o, const F80& v) {
    if (o.type == 1) { st(o.ridx) = v; return; }
    if (o.size == 4) put(o, reflex_f80::to_f32(v, frc()), 4);
    else if (o.size == 8) put(o, reflex_f80::to_f64(v, frc()), 8);
    else if (o.size == 10) { uint8_t raw[10]; reflex_f80::to_f80(v, raw); mem.write_bytes(ea(o), raw, 10); }
    else throw VmError("x87 store size");
}
static F80 f80_int(int64_t v) { return reflex_f80::from_int(v); }
static int64_t sx(uint64_t v, int bytes) { int bits = 8 * bytes; uint64_t s = 1ull << (bits - 1); return int64_t((v ^ s) - s) ; }

uint32_t Machine::execute_fp(const Insn& in) {
    if (mmx_active && MN_NAMES[in.mn][0] == 'f') throw VmError("x87 instruction while MMX registers are live");
    const Operand* o = in.o;
    const uint32_t next = in.next;
    using namespace reflex_f80;
    switch (in.mn) {
    // ---- SSE arithmetic ----
#define ARITH(SS, PS, SD, PD, OP) \
    case M_##SS: case M_##PS: case M_##SD: case M_##PD: { \
        bool dbl = in.mn == M_##SD || in.mn == M_##PD; bool packed = in.mn == M_##PS || in.mn == M_##PD; \
        Xmm a = xmm[o[0].ridx]; \
        if (packed && !dbl && OP <= OP_DIV) { Xmm b = get128(o[1]); \
            if (ps_fast(OP, a, b, mxcsr)) { xmm[o[0].ridx] = a; return next; } \
            for (int i = 0; i < 4; i++) set_lane32(a, i, sse32(OP, lane32(a, i), lane32(b, i))); \
            xmm[o[0].ridx] = a; return next; } \
        if (packed) { Xmm b = get128(o[1]); \
            if (dbl) { a.lo = sse64(OP, a.lo, b.lo); a.hi = sse64(OP, a.hi, b.hi); } \
            else for (int i = 0; i < 4; i++) set_lane32(a, i, sse32(OP, lane32(a, i), lane32(b, i))); } \
        else if (dbl) a.lo = sse64(OP, a.lo, get(o[1], 8)); \
        else set_lane32(a, 0, sse32(OP, lane32(a, 0), uint32_t(get(o[1], 4)))); \
        xmm[o[0].ridx] = a; return next; }
    ARITH(addss, addps, addsd, addpd, OP_ADD)
    ARITH(subss, subps, subsd, subpd, OP_SUB)
    ARITH(mulss, mulps, mulsd, mulpd, OP_MUL)
    ARITH(divss, divps, divsd, divpd, OP_DIV)
    ARITH(minss, minps, minsd, minpd, OP_MIN)
    ARITH(maxss, maxps, maxsd, maxpd, OP_MAX)
#undef ARITH
    case M_sqrtsd: {
        uint64_t v = get(o[1], 8); uint64_t res;
        if (nan64(v)) res = v | 0x8000000000000ull; else if (v == 0x8000000000000000ull) res = v; else if (v >> 63) res = 0xfff8000000000000ull; else { volatile double d = std::sqrt(bd(v)); res = db(d); }
        xmm[o[0].ridx].lo = res; return next; }
    case M_sqrtss: case M_sqrtps: {
        int lanes = in.mn == M_sqrtps ? 4 : 1;
        Xmm b = lanes == 4 ? get128(o[1]) : Xmm{get(o[1], 4), 0};
        Xmm a = xmm[o[0].ridx];
        for (int i = 0; i < lanes; i++) set_lane32(a, i, sqrt32(lane32(b, i)));
        xmm[o[0].ridx] = a; return next; }
    case M_rsqrtss: case M_rsqrtps: case M_rcpss: case M_rcpps: {
        int lanes = (in.mn == M_rsqrtps || in.mn == M_rcpps) ? 4 : 1;
        bool rs = in.mn == M_rsqrtss || in.mn == M_rsqrtps;
        Xmm b = lanes == 4 ? get128(o[1]) : Xmm{get(o[1], 4), 0};
        Xmm a = xmm[o[0].ridx];
        for (int i = 0; i < lanes; i++) set_lane32(a, i, rs ? rsqrt32(lane32(b, i)) : rcp32(lane32(b, i)));
        xmm[o[0].ridx] = a; return next; }
    case M_andps: case M_andpd: case M_pand: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.lo & b.lo, a.hi & b.hi}); return next; }
    case M_andnps: case M_andnpd: case M_pandn: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {~a.lo & b.lo, ~a.hi & b.hi}); return next; }
    case M_orps: case M_orpd: case M_por: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.lo | b.lo, a.hi | b.hi}); return next; }
    case M_xorps: case M_xorpd: case M_pxor: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.lo ^ b.lo, a.hi ^ b.hi}); return next; }
    case M_shufps: {
        Xmm a = get128(o[0]), b = get128(o[1]), res; unsigned imm = o[2].imm;
        set_lane32(res, 0, lane32(a, imm & 3)); set_lane32(res, 1, lane32(a, (imm >> 2) & 3));
        set_lane32(res, 2, lane32(b, (imm >> 4) & 3)); set_lane32(res, 3, lane32(b, (imm >> 6) & 3));
        put128(o[0], res); return next; }
    case M_pshufd: {
        Xmm b = get128(o[1]), res; unsigned imm = o[2].imm;
        for (int k = 0; k < 4; k++) set_lane32(res, k, lane32(b, (imm >> (2 * k)) & 3));
        put128(o[0], res); return next; }
    case M_unpcklps: case M_unpckhps: {
        Xmm a = get128(o[0]), b = get128(o[1]), res; int off = in.mn == M_unpcklps ? 0 : 2;
        set_lane32(res, 0, lane32(a, off)); set_lane32(res, 1, lane32(b, off)); set_lane32(res, 2, lane32(a, off + 1)); set_lane32(res, 3, lane32(b, off + 1));
        put128(o[0], res); return next; }
    case M_movlhps: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.lo, b.lo}); return next; }
    case M_movhlps: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {b.hi, a.hi}); return next; }
    case M_movlps: case M_movlpd: case M_movhps: case M_movhpd: {
        bool low = in.mn == M_movlps || in.mn == M_movlpd;
        if (o[0].type == 3) { Xmm v = xmm[o[1].ridx]; put(o[0], low ? v.lo : v.hi, 8); }
        else { uint64_t v = get(o[1], 8); if (low) xmm[o[0].ridx].lo = v; else xmm[o[0].ridx].hi = v; }
        return next; }
    case M_comiss: case M_ucomiss: case M_comisd: case M_ucomisd: {
        double a, b;
        if (in.mn == M_comisd || in.mn == M_ucomisd) { a = bd(get(o[0], 8)); b = bd(get(o[1], 8)); }
        else { a = bf(uint32_t(get(o[0], 4))); b = bf(uint32_t(get(o[1], 4))); }
        uint32_t f = flags & ~(CF | PF | ZF | SF | OF | 0x10);
        if (std::isnan(a) || std::isnan(b)) f |= ZF | PF | CF; else if (a < b) f |= CF; else if (a == b) f |= ZF;
        flags = f; return next; }
    case M_cvtsi2ss: { int32_t v = int32_t(uint32_t(get(o[1], 4))); volatile float f = float(v); xmm[o[0].ridx].lo = (xmm[o[0].ridx].lo & ~0xffffffffull) | fb(f); return next; }
    case M_cvtpi2ps: {            // two int32 -> two floats in the low lanes (round to nearest, as cvtsi2ss)
        uint64_t v = get(o[1], 8);
        volatile float f0 = float(int32_t(uint32_t(v))), f1 = float(int32_t(uint32_t(v >> 32)));
        xmm[o[0].ridx].lo = uint64_t(fb(f0)) | (uint64_t(fb(f1)) << 32); return next; }
    case M_emms: if (mmx_active) { fpu.clear(); mmx_active = false; } return next;
    case M_cvtsi2sd: { int32_t v = int32_t(uint32_t(get(o[1], 4))); xmm[o[0].ridx].lo = db(double(v)); return next; }
    case M_cvttss2si: case M_cvtss2si: case M_cvttsd2si: case M_cvtsd2si: {
        double x = (in.mn == M_cvttss2si || in.mn == M_cvtss2si) ? double(bf(uint32_t(get(o[1], 4)))) : bd(get(o[1], 8));
        bool ok; int32_t v = (in.mn == M_cvttss2si || in.mn == M_cvttsd2si) ? to_int32_trunc(x, ok) : to_int32_nearest(x, ok);
        put(o[0], ok ? uint32_t(v) : 0x80000000u); return next; }
    case M_cvtss2sd: {
        uint32_t v = uint32_t(get(o[1], 4)); uint64_t res;
        if (nan32(v)) res = (uint64_t(v >> 31) << 63) | 0x7ff8000000000000ull | (uint64_t(v & 0x3fffff) << 29); else res = db(double(bf(v)));
        xmm[o[0].ridx].lo = res; return next; }
    case M_cvtsd2ss: {
        uint64_t v = get(o[1], 8); uint32_t res;
        if (nan64(v)) res = uint32_t(((v >> 63) << 31) | 0x7fc00000u | ((v >> 29) & 0x3fffff)); else { volatile float f = float(bd(v)); res = fb(f); }
        xmm[o[0].ridx].lo = (xmm[o[0].ridx].lo & ~0xffffffffull) | res; return next; }
    case M_cvtps2pd: { uint64_t v = get(o[1], 8); Xmm res{db(double(bf(uint32_t(v)))), db(double(bf(uint32_t(v >> 32))))}; put128(o[0], res); return next; }
    case M_cvtpd2ps: { Xmm v = get128(o[1]); volatile float a = float(bd(v.lo)), b = float(bd(v.hi)); put128(o[0], {uint64_t(fb(a)) | (uint64_t(fb(b)) << 32), 0}); return next; }
    case M_cvtdq2ps: { Xmm v = get128(o[1]), res; for (int k = 0; k < 4; k++) { volatile float f = float(int32_t(lane32(v, k))); set_lane32(res, k, fb(f)); } put128(o[0], res); return next; }
    case M_cvttps2dq: case M_cvtps2dq: {
        Xmm v = get128(o[1]), res;
        for (int k = 0; k < 4; k++) { bool ok; double x = bf(lane32(v, k)); int32_t t = in.mn == M_cvttps2dq ? to_int32_trunc(x, ok) : to_int32_nearest(x, ok); set_lane32(res, k, ok ? uint32_t(t) : 0x80000000u); }
        put128(o[0], res); return next; }
    case M_cvtdq2pd: { uint64_t v = get(o[1], 8); put128(o[0], {db(double(int32_t(uint32_t(v)))), db(double(int32_t(uint32_t(v >> 32))))}); return next; }
    case M_cvtpd2dq: case M_cvttpd2dq: {
        Xmm v = get128(o[1]); Xmm res;
        for (int k = 0; k < 2; k++) { bool ok; double x = bd(lane64(v, k)); int32_t t = in.mn == M_cvttpd2dq ? to_int32_trunc(x, ok) : to_int32_nearest(x, ok); set_lane32(res, k, ok ? uint32_t(t) : 0x80000000u); }
        put128(o[0], res); return next; }
    case M_stmxcsr: put(o[0], mxcsr, 4); return next;
    case M_ldmxcsr: { uint32_t v = uint32_t(get(o[0], 4)); if (v & 0xe040) throw VmError("unsupported MXCSR"); mxcsr = v; return next; }
    case M_movmskps: { Xmm v = get128(o[1]); uint32_t m = 0; for (int i = 0; i < 4; i++) m |= (lane32(v, i) >> 31) << i; put(o[0], m, 4); return next; }
    case M_movmskpd: { Xmm v = get128(o[1]); put(o[0], uint32_t((v.lo >> 63) | ((v.hi >> 63) << 1)), 4); return next; }
    case M_pcmpeqd: case M_pcmpgtd: case M_paddd: case M_psubd: case M_pslld: case M_psrld: case M_psrad: case M_punpckldq: {
        Xmm a = get128(o[0]), res;
        bool imm = o[1].type == 2; Xmm b = imm ? Xmm{o[1].imm, 0} : get128(o[1]);
        uint64_t c = imm ? (o[1].imm & 0xff) : b.lo;
        for (int i = 0; i < 4; i++) {
            uint32_t x = lane32(a, i), y;
            switch (in.mn) {
                case M_pcmpeqd: y = x == lane32(b, i) ? 0xffffffffu : 0; break;
                case M_pcmpgtd: y = int32_t(x) > int32_t(lane32(b, i)) ? 0xffffffffu : 0; break;
                case M_paddd: y = x + lane32(b, i); break;
                case M_psubd: y = x - lane32(b, i); break;
                case M_punpckldq: { uint32_t pick[4] = {lane32(a, 0), lane32(b, 0), lane32(a, 1), lane32(b, 1)}; y = pick[i]; break; }
                case M_pslld: y = c < 32 ? (x << c) : 0; break;
                case M_psrld: y = c < 32 ? (x >> c) : 0; break;
                default: y = uint32_t(int32_t(x) >> (c < 31 ? c : 31)); break;
            }
            set_lane32(res, i, y);
        }
        put128(o[0], res); return next; }
    // ---- SSE2 packed byte/word integer family (xmm operands; lanes as little-endian arrays) ----
    case M_pmaxsw: case M_pminsw: case M_pmaxub: case M_pminub: case M_paddb: case M_paddw: case M_psubb: case M_psubw: case M_paddsb: case M_paddsw: case M_paddusb: case M_paddusw: case M_psubsb: case M_psubsw: case M_psubusb: case M_psubusw: case M_pcmpeqb: case M_pcmpeqw: case M_pcmpgtb: case M_pcmpgtw: case M_pmullw: case M_pmulhw: case M_pmulhuw: case M_pmuludq: case M_pavgb: case M_pavgw: case M_psraw: case M_psrlw: case M_psllw: case M_packsswb: case M_packuswb: case M_packssdw: case M_punpcklbw: case M_punpckhbw: case M_punpcklwd: case M_punpckhwd: case M_punpckhdq: case M_pshuflw: case M_pshufhw: case M_pmovmskb: case M_psadbw: case M_pmaddwd: {
        Xmm a = in.mn == M_pshuflw || in.mn == M_pshufhw || in.mn == M_pmovmskb ? Xmm{} : get128(o[0]);
        bool imm = o[1].type == 2; Xmm b = imm ? Xmm{o[1].imm, 0} : get128(o[1]);
        uint8_t A[16], B[16], R[16]; std::memcpy(A, &a.lo, 8); std::memcpy(A + 8, &a.hi, 8); std::memcpy(B, &b.lo, 8); std::memcpy(B + 8, &b.hi, 8);
        auto w = [](const uint8_t* x, int i) { uint16_t v; std::memcpy(&v, x + 2 * i, 2); return v; };
        auto sw = [&](int i, uint32_t v) { uint16_t x = uint16_t(v); std::memcpy(R + 2 * i, &x, 2); };
        auto d = [](const uint8_t* x, int i) { uint32_t v; std::memcpy(&v, x + 4 * i, 4); return v; };
        auto sd = [&](int i, uint32_t v) { std::memcpy(R + 4 * i, &v, 4); };
        auto sat8s = [](int v) { return uint8_t(v < -128 ? -128 : v > 127 ? 127 : v); };
        auto sat8u = [](int v) { return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v); };
        auto sat16s = [](int v) { return uint16_t(v < -32768 ? -32768 : v > 32767 ? 32767 : v); };
        auto sat16u = [](int v) { return uint16_t(v < 0 ? 0 : v > 65535 ? 65535 : v); };
        uint64_t c = imm ? (o[1].imm & 0xff) : b.lo;
        switch (in.mn) {
            case M_pmaxsw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) > int16_t(w(B, i)) ? w(A, i) : w(B, i)); break;
            case M_pminsw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) < int16_t(w(B, i)) ? w(A, i) : w(B, i)); break;
            case M_pmaxub: for (int i = 0; i < 16; i++) R[i] = std::max(A[i], B[i]); break;
            case M_pminub: for (int i = 0; i < 16; i++) R[i] = std::min(A[i], B[i]); break;
            case M_paddb: for (int i = 0; i < 16; i++) R[i] = uint8_t(A[i] + B[i]); break;
            case M_psubb: for (int i = 0; i < 16; i++) R[i] = uint8_t(A[i] - B[i]); break;
            case M_paddw: for (int i = 0; i < 8; i++) sw(i, w(A, i) + w(B, i)); break;
            case M_psubw: for (int i = 0; i < 8; i++) sw(i, w(A, i) - w(B, i)); break;
            case M_paddsb: for (int i = 0; i < 16; i++) R[i] = sat8s(int8_t(A[i]) + int8_t(B[i])); break;
            case M_psubsb: for (int i = 0; i < 16; i++) R[i] = sat8s(int8_t(A[i]) - int8_t(B[i])); break;
            case M_paddusb: for (int i = 0; i < 16; i++) R[i] = sat8u(A[i] + B[i]); break;
            case M_psubusb: for (int i = 0; i < 16; i++) R[i] = sat8u(int(A[i]) - B[i]); break;
            case M_paddsw: for (int i = 0; i < 8; i++) sw(i, sat16s(int16_t(w(A, i)) + int16_t(w(B, i)))); break;
            case M_psubsw: for (int i = 0; i < 8; i++) sw(i, sat16s(int16_t(w(A, i)) - int16_t(w(B, i)))); break;
            case M_paddusw: for (int i = 0; i < 8; i++) sw(i, sat16u(int(w(A, i)) + w(B, i))); break;
            case M_psubusw: for (int i = 0; i < 8; i++) sw(i, sat16u(int(w(A, i)) - w(B, i))); break;
            case M_pcmpeqb: for (int i = 0; i < 16; i++) R[i] = A[i] == B[i] ? 0xff : 0; break;
            case M_pcmpgtb: for (int i = 0; i < 16; i++) R[i] = int8_t(A[i]) > int8_t(B[i]) ? 0xff : 0; break;
            case M_pcmpeqw: for (int i = 0; i < 8; i++) sw(i, w(A, i) == w(B, i) ? 0xffff : 0); break;
            case M_pcmpgtw: for (int i = 0; i < 8; i++) sw(i, int16_t(w(A, i)) > int16_t(w(B, i)) ? 0xffff : 0); break;
            case M_pmullw: for (int i = 0; i < 8; i++) sw(i, uint32_t(int32_t(int16_t(w(A, i))) * int16_t(w(B, i)))); break;
            case M_pmulhw: for (int i = 0; i < 8; i++) sw(i, uint32_t((int32_t(int16_t(w(A, i))) * int16_t(w(B, i))) >> 16)); break;
            case M_pmulhuw: for (int i = 0; i < 8; i++) sw(i, (uint32_t(w(A, i)) * w(B, i)) >> 16); break;
            case M_pmuludq: { uint64_t lo = uint64_t(d(A, 0)) * d(B, 0), hi = uint64_t(d(A, 2)) * d(B, 2); std::memcpy(R, &lo, 8); std::memcpy(R + 8, &hi, 8); break; }
            case M_pmaddwd: for (int i = 0; i < 4; i++) sd(i, uint32_t(int32_t(int16_t(w(A, 2 * i))) * int16_t(w(B, 2 * i)) + int32_t(int16_t(w(A, 2 * i + 1))) * int16_t(w(B, 2 * i + 1)))); break;
            case M_pavgb: for (int i = 0; i < 16; i++) R[i] = uint8_t((A[i] + B[i] + 1) >> 1); break;
            case M_pavgw: for (int i = 0; i < 8; i++) sw(i, (uint32_t(w(A, i)) + w(B, i) + 1) >> 1); break;
            case M_psadbw: { uint64_t s0 = 0, s1 = 0; for (int i = 0; i < 8; i++) { s0 += uint64_t(std::abs(int(A[i]) - B[i])); s1 += uint64_t(std::abs(int(A[i + 8]) - B[i + 8])); } std::memcpy(R, &s0, 8); std::memcpy(R + 8, &s1, 8); break; }
            case M_psraw: for (int i = 0; i < 8; i++) sw(i, uint32_t(int32_t(int16_t(w(A, i))) >> (c < 15 ? c : 15))); break;
            case M_psrlw: for (int i = 0; i < 8; i++) sw(i, c < 16 ? (w(A, i) >> c) : 0); break;
            case M_psllw: for (int i = 0; i < 8; i++) sw(i, c < 16 ? uint32_t(w(A, i)) << c : 0); break;
            case M_packsswb: for (int i = 0; i < 8; i++) { R[i] = sat8s(int16_t(w(A, i))); R[i + 8] = sat8s(int16_t(w(B, i))); } break;
            case M_packuswb: for (int i = 0; i < 8; i++) { R[i] = sat8u(int16_t(w(A, i))); R[i + 8] = sat8u(int16_t(w(B, i))); } break;
            case M_packssdw: for (int i = 0; i < 4; i++) { sw(i, sat16s(int32_t(d(A, i)) < -32768 ? -32768 : int32_t(d(A, i)) > 32767 ? 32767 : int32_t(d(A, i)))); sw(i + 4, sat16s(int32_t(d(B, i)) < -32768 ? -32768 : int32_t(d(B, i)) > 32767 ? 32767 : int32_t(d(B, i)))); } break;
            case M_punpcklbw: for (int i = 0; i < 8; i++) { R[2 * i] = A[i]; R[2 * i + 1] = B[i]; } break;
            case M_punpckhbw: for (int i = 0; i < 8; i++) { R[2 * i] = A[i + 8]; R[2 * i + 1] = B[i + 8]; } break;
            case M_punpcklwd: for (int i = 0; i < 4; i++) { sw(2 * i, w(A, i)); sw(2 * i + 1, w(B, i)); } break;
            case M_punpckhwd: for (int i = 0; i < 4; i++) { sw(2 * i, w(A, i + 4)); sw(2 * i + 1, w(B, i + 4)); } break;
            case M_punpckhdq: { uint32_t pick[4] = {d(A, 2), d(B, 2), d(A, 3), d(B, 3)}; for (int i = 0; i < 4; i++) sd(i, pick[i]); break; }
            case M_pshuflw: case M_pshufhw: {
                unsigned k = o[2].imm; std::memcpy(R, B, 16); int base = in.mn == M_pshuflw ? 0 : 4;
                for (int i = 0; i < 4; i++) sw(base + i, w(B, base + ((k >> (2 * i)) & 3)));
                break; }
            case M_pmovmskb: { uint32_t m = 0; for (int i = 0; i < 16; i++) m |= uint32_t(B[i] >> 7) << i; put(o[0], m, 4); return next; }
            default: break;
        }
        Xmm res; std::memcpy(&res.lo, R, 8); std::memcpy(&res.hi, R + 8, 8);
        put128(o[0], res); return next; }
    case M_psrlq: case M_psllq: case M_paddq: case M_psubq: {
        Xmm a = get128(o[0]);
        if (in.mn == M_psrlq || in.mn == M_psllq) {
            uint64_t c = o[1].type == 2 ? o[1].imm : get128(o[1]).lo;
            auto sh = [&](uint64_t x) { return c > 63 ? 0ull : (in.mn == M_psrlq ? (x >> c) : (x << c)); };
            put128(o[0], {sh(a.lo), sh(a.hi)});
        } else {
            Xmm b = get128(o[1]);
            put128(o[0], in.mn == M_paddq ? Xmm{a.lo + b.lo, a.hi + b.hi} : Xmm{a.lo - b.lo, a.hi - b.hi});
        }
        return next; }
    case M_psrldq: case M_pslldq: {
        Xmm a = get128(o[0]); unsigned c = std::min<unsigned>(o[1].imm, 16) * 8;
        unsigned __int128 v = ((unsigned __int128)a.hi << 64) | a.lo;
        v = c >= 128 ? 0 : (in.mn == M_psrldq ? (v >> c) : (v << c));
        put128(o[0], {uint64_t(v), uint64_t(v >> 64)}); return next; }
    case M_pextrw: { Xmm v = get128(o[1]); unsigned k = o[2].imm & 7; uint64_t w = k < 4 ? (v.lo >> (16 * k)) : (v.hi >> (16 * (k - 4))); put(o[0], w & 0xffff, 4); return next; }
    case M_pinsrw: {
        unsigned k = o[2].imm & 7; uint64_t w = o[1].type == 3 ? get(o[1], 2) : (r[o[1].ridx] & 0xffff);
        Xmm& x = xmm[o[0].ridx]; uint64_t& lane = k < 4 ? x.lo : x.hi; unsigned s = 16 * (k & 3);
        lane = (lane & ~(0xffffull << s)) | (w << s); return next; }
    case M_unpcklpd: case M_punpcklqdq: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.lo, b.lo}); return next; }
    case M_unpckhpd: case M_punpckhqdq: { Xmm a = get128(o[0]), b = get128(o[1]); put128(o[0], {a.hi, b.hi}); return next; }
    case M_shufpd: { Xmm a = get128(o[0]), b = get128(o[1]); unsigned imm = o[2].imm; put128(o[0], {(imm & 1) ? a.hi : a.lo, (imm & 2) ? b.hi : b.lo}); return next; }
    default: break;
    }
    // ---- SSE compares with predicate in the mnemonic ----
    if (in.mn >= M_cmpeqps && in.mn <= M_cmpordsd) {
        int idx = in.mn - M_cmpeqps;            // 8 predicates x {ps, ss, pd, sd}
        int pred = idx % 8, kind = idx / 8;      // kind 0 ps, 1 ss, 2 pd, 3 sd
        bool dbl = kind >= 2, packed = kind == 0 || kind == 2;
        int lanes = packed ? (dbl ? 2 : 4) : 1;
        Xmm a = xmm[o[0].ridx];
        Xmm b = packed ? get128(o[1]) : Xmm{get(o[1], dbl ? 8 : 4), 0};
        for (int i = 0; i < lanes; i++) {
            double x = dbl ? bd(lane64(a, i)) : double(bf(lane32(a, i)));
            double y = dbl ? bd(lane64(b, i)) : double(bf(lane32(b, i)));
            bool un = std::isnan(x) || std::isnan(y), ok;
            switch (pred) {
                case 0: ok = x == y; break; case 1: ok = x < y; break; case 2: ok = x <= y; break; case 3: ok = un; break;
                case 4: ok = un || x != y; break; case 5: ok = un || !(x < y); break; case 6: ok = un || !(x <= y); break; default: ok = !un;
            }
            if (dbl) { if (i) a.hi = ok ? ~0ull : 0; else a.lo = ok ? ~0ull : 0; }
            else set_lane32(a, i, ok ? 0xffffffffu : 0);
        }
        xmm[o[0].ridx] = a; return next;
    }
    // ---- x87 ----
    int rc = frc();
    switch (in.mn) {
    case M_fld: fpush(fload(o[0])); return next;
    case M_fild: fpush(f80_int(sx(get(o[0]), o[0].size))); return next;
    case M_fld1: fpush(from_int(1)); return next;
    case M_fldz: fpush(from_int(0)); return next;
    case M_fldpi: fpush(transcendental("pi", nullptr, nullptr, rc)); return next;
    case M_fldl2e: fpush(transcendental("l2e", nullptr, nullptr, rc)); return next;
    case M_fldln2: fpush(transcendental("ln2", nullptr, nullptr, rc)); return next;
    case M_fldl2t: fpush(transcendental("l2t", nullptr, nullptr, rc)); return next;
    case M_fldlg2: fpush(transcendental("lg2", nullptr, nullptr, rc)); return next;
    case M_fst: case M_fstp: { F80 v = st(0); fstore(o[0], v); if (in.mn == M_fstp) fpop(); return next; }
    case M_fistp: case M_fist: case M_fisttp: {
        F80 v = st(0); int size = o[0].size; bool valid = v.kind == 0; int64_t n = 0;
        if (valid) {
            F80 ri = round_to_integer(v, in.mn == M_fisttp ? 3 : rc);
            // ri is an exact integer; convert magnitude
            if (ri.mant == 0) n = 0;
            else {
                int shift = 63 - (ri.exp);        // value = mant * 2^(exp-63)
                if (ri.exp > 63 || (ri.exp == 63 && !(ri.sign && ri.mant == (1ull << 63)))) valid = false;
                else if (ri.exp == 63) { n = INT64_MIN; }
                if (valid && ri.exp < 63) { uint64_t mag = ri.mant >> shift; n = ri.sign ? -int64_t(mag) : int64_t(mag); }
            }
        }
        // Integer indefinite (most negative value) when NaN/inf or out of range.
        if (size == 8) { if (!valid) n = INT64_MIN; }
        else {
            int64_t lim = int64_t(1) << (8 * size - 1);
            if (!valid || n < -lim || n >= lim) n = -lim;
        }
        put(o[0], uint64_t(n));
        if (in.mn != M_fist) fpop();
        return next; }
    case M_fadd: case M_fsub: case M_fmul: case M_fdiv: case M_fsubr: case M_fdivr:
    case M_faddp: case M_fsubp: case M_fmulp: case M_fdivp: case M_fsubrp: case M_fdivrp:
    case M_fiadd: case M_fimul: case M_fisub: case M_fidiv: case M_fisubr: case M_fidivr: {
        // Decoded from the opcode bytes (Intel SDM x87 tables), as in x86world.py.
        int k = 0;
        while (k < in.len && (in.raw[k] == 0x26 || in.raw[k] == 0x2e || in.raw[k] == 0x36 || in.raw[k] == 0x3e || in.raw[k] == 0x64 ||
                              in.raw[k] == 0x65 || in.raw[k] == 0x66 || in.raw[k] == 0x67 || in.raw[k] == 0xf2 || in.raw[k] == 0xf3)) k++;
        uint8_t opcode = in.raw[k], modrm = in.raw[k + 1]; int reg = (modrm >> 3) & 7; int bits = fbits();
        auto apply = [&](int op, const F80& a, const F80& b) -> F80 {
            switch (op) {
                case 0: return add(a, b, bits, rc);
                case 1: return mul(a, b, bits, rc);
                case 4: return add(a, b, bits, rc, true);        // sub
                case 5: return add(b, a, bits, rc, true);        // subr
                case 6: return div(a, b, bits, rc);
                case 7: return div(b, a, bits, rc);              // divr
            }
            throw VmError("x87 op");
        };
        if (modrm >= 0xc0) {
            int i = modrm & 7;
            if (opcode == 0xd8) st(0) = apply(reg, st(0), st(i));
            else if (opcode == 0xdc || opcode == 0xde) {
                static const int swap[8] = {0, 1, -1, -1, 5, 4, 7, 6};
                st(i) = apply(swap[reg], st(i), st(0));
                if (opcode == 0xde) fpop();
            } else throw VmError("x87 opcode");
        } else {
            F80 src;
            if (opcode == 0xd8) src = from_f32(uint32_t(get(o[0], 4)));
            else if (opcode == 0xdc) src = from_f64(get(o[0], 8));
            else if (opcode == 0xda) src = f80_int(sx(get(o[0], 4), 4));
            else if (opcode == 0xde) src = f80_int(sx(get(o[0], 2), 2));
            else throw VmError("x87 opcode");
            st(0) = apply(reg, st(0), src);
        }
        return next; }
    case M_fchs: st(0).sign ^= 1; return next;
    case M_fabs: st(0).sign = 0; return next;
    case M_fsqrt: st(0) = reflex_f80::sqrt(st(0), fbits(), rc); return next;
    case M_frndint: st(0) = round_to_integer(st(0), rc); return next;
    case M_fscale: {
        F80 a = st(0), b = st(1);
        if (a.kind != 0 || b.kind != 0) throw VmError("fscale special operand");
        F80 t = round_to_integer(b, 3); int64_t n = 0;
        if (t.mant) { if (t.exp > 62) throw VmError("fscale range"); n = int64_t(t.mant >> (63 - t.exp)); if (t.sign) n = -n; }
        st(0) = scale(a, n); return next; }
    case M_fpatan: case M_fyl2x: case M_fyl2xp1: {
        F80 a = st(0), b = st(1);
        // The multi-precision evaluation (f80.cpp) is a pure function of (op, a, b, rc): results are memoized in a small
        // direct-mapped table (the game repeats many of the same arguments within and across frames).
        struct Memo { uint64_t am, bm; int32_t ae, be; uint16_t key; F80 r; };
        static thread_local Memo memo[4096]; static thread_local uint64_t memo_calls, memo_hits;
        uint16_t key = uint16_t(1 + (in.mn == M_fpatan ? 0 : in.mn == M_fyl2x ? 1 : 2) + 3 * rc + 12 * (a.kind + 3 * a.sign) + 72 * (b.kind + 3 * b.sign));
        uint64_t h = (a.mant ^ (b.mant * 0x9e3779b97f4a7c15ull) ^ (uint64_t(uint32_t(a.exp)) << 17) ^ (uint64_t(uint32_t(b.exp)) << 41) ^ key) * 0xff51afd7ed558ccdull;
        Memo& e = memo[h >> 52];
        memo_calls++;
        F80 res;
        if (e.key == key && e.am == a.mant && e.bm == b.mant && e.ae == a.exp && e.be == b.exp) { res = e.r; memo_hits++; }
        else { res = transcendental(in.mn == M_fpatan ? "fpatan" : in.mn == M_fyl2x ? "fyl2x" : "fyl2xp1", &a, &b, rc); e = Memo{a.mant, b.mant, a.exp, b.exp, key, res}; }
        if (__builtin_expect((memo_calls & ((1u << 14) - 1)) == 0, 0) && std::getenv("REFLEX_MEMO_STATS")) std::fprintf(stderr, "fpatan/fyl2x memo: %llu calls, %llu hits\n", (unsigned long long)memo_calls, (unsigned long long)memo_hits);
        fpop(); st(0) = res; return next; }
    case M_f2xm1: { F80 a = st(0); st(0) = transcendental("f2xm1", &a, nullptr, rc); return next; }
    // the hardware's range rule (w-crtnan4): a finite operand with abs >= 2^63 sets C2 and leaves the stack unchanged (the caller reduces it, fprem1)
    case M_fsin: case M_fcos: { F80 a = st(0); if (a.kind == 0 && a.mant && a.exp >= 63) { fpu_sw |= 0x400; return next; }
        st(0) = transcendental(in.mn == M_fsin ? "fsin" : "fcos", &a, nullptr, rc); fpu_sw &= ~0x400; return next; }
    case M_fptan: { F80 a = st(0); if (a.kind == 0 && a.mant && a.exp >= 63) { fpu_sw |= 0x400; return next; }
        st(0) = transcendental("fptan", &a, nullptr, rc); fpush(from_int(1)); fpu_sw &= ~0x400; return next; }
    case M_fsincos: { F80 a = st(0); if (a.kind == 0 && a.mant && a.exp >= 63) { fpu_sw |= 0x400; return next; }
        F80 s = transcendental("fsin", &a, nullptr, rc), c = transcendental("fcos", &a, nullptr, rc); st(0) = s; fpush(c); fpu_sw &= ~0x400; return next; }
    case M_fxch: { int i = in.n ? o[in.n - 1].ridx : 1; std::swap(st(0), st(i)); return next; }
    case M_fcomp: case M_fcom: case M_fcompp: case M_fucomp: case M_fucom: case M_fucompp:
    case M_fcomi: case M_fcomip: case M_fucomi: case M_fucomip: case M_ficom: case M_ficomp: case M_ftst: {
        F80 a = st(0), b;
        if (in.mn == M_ftst) b = from_int(0);
        else if (in.mn == M_ficom || in.mn == M_ficomp) b = f80_int(sx(get(o[0]), o[0].size));
        else b = in.n ? fload(o[in.n - 1]) : st(1);
        int c = compare(a, b);
        if (in.mn == M_fcomi || in.mn == M_fcomip || in.mn == M_fucomi || in.mn == M_fucomip) {
            uint32_t f = flags & ~(CF | PF | ZF);
            if (c == 2) f |= CF | PF | ZF; else if (c < 0) f |= CF; else if (c == 0) f |= ZF;
            flags = f;
        } else {
            uint16_t sw = fpu_sw & ~0x4500;
            if (c == 2) sw |= 0x4500; else if (c < 0) sw |= 0x100; else if (c == 0) sw |= 0x4000;
            fpu_sw = sw;
        }
        int pops = (in.mn == M_fcompp || in.mn == M_fucompp) ? 2 : (in.mn == M_fcomp || in.mn == M_fucomp || in.mn == M_fcomip || in.mn == M_fucomip || in.mn == M_ficomp) ? 1 : 0;
        for (int i = 0; i < pops; i++) fpop();
        return next; }
    case M_fprem: case M_fprem1: {
        F80 x = st(0), y = st(1); uint16_t sw = fpu_sw & ~0x4700;
        int q3 = 0; st(0) = remainder(x, y, in.mn == M_fprem1, &q3);
        if (q3 & 4) sw |= 0x100;
        if (q3 & 2) sw |= 0x4000;
        if (q3 & 1) sw |= 0x200;
        fpu_sw = sw; return next; }
    case M_fxam: {
        uint16_t sw = fpu_sw & ~0x4700;
        if (fpu.empty()) sw |= 0x4100;
        else {
            const F80& v = st(0);
            if (v.sign) sw |= 0x200;
            if (v.kind == 2) sw |= 0x100; else if (v.kind == 1) sw |= 0x500; else if (v.mant == 0) sw |= 0x4000; else sw |= 0x400;
        }
        fpu_sw = sw; return next; }
    case M_fnstsw: put(o[0], (fpu_sw & ~0x3800) | (((8 - fpu.size()) % 8) << 11), 2); return next;
    case M_fnstcw: put(o[0], fpu_cw, 2); return next;
    case M_fldcw: fpu_cw = uint16_t(get(o[0], 2)); return next;
    case M_fnclex: fpu_sw &= ~0x80ff; return next;
    default: break;
    }
    throw VmError(std::string("unsupported instruction ") + MN_NAMES[in.mn]);
}

}  // namespace reflex_vm

namespace reflex_vm { const char* mnemonic_name(int i) { return MN_NAMES[i]; } }
