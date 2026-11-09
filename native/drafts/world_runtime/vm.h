// Instruction-level runtime for the original Reflex physics (private draft).
// Executes the unchanged original x86 code of MXReflex.exe and the VC90 CRT over a
// live-world image; the semantics mirror _re/trace/x86world.py exactly, which is
// verified bit-for-bit against the original executing under the replay harness.
#pragma once
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "f80/f80.h"

namespace reflex_vm {

struct VmError : std::runtime_error { using std::runtime_error::runtime_error; };
// Thrown (not a std::exception) where the boot would switch guest threads, or would change host state it cannot take back, while
// Machine::no_switch is set: native/game Verify catches it and runs the original unverified instead (notes/loading.md).
struct WouldSwitch {};

enum Mn : uint16_t {
#define MN(x) M_##x,
#include "mnemonics.inc"
#undef MN
    M_COUNT
};

struct Operand {
    uint8_t type = 0;      // 0 none, 1 reg, 2 imm, 3 mem
    uint8_t size = 0;
    uint8_t rclass = 0;    // 1 gpr32, 2 gpr16, 3 gpr8l, 4 gpr8h, 5 xmm, 6 st
    uint8_t ridx = 0;
    int8_t base = -1, index = -1;
    uint8_t scale = 1;
    uint8_t fs = 0;
    int32_t disp = 0;
    uint32_t imm = 0;
};

class Machine;
struct Insn;
using FastFn = uint32_t (*)(Machine&, const Insn&);   // specialized handler chosen at decode (nullptr: generic)

struct Insn {
    uint32_t addr = 0, next = 0;
    uint16_t mn = 0;
    uint8_t n = 0, len = 0;
    uint8_t raw[16] = {};
    Operand o[3];
    const Insn* fall = nullptr;   // decoded instruction at `next` (fall-through chaining)
    FastFn fast = nullptr;
    const Insn* taken = nullptr;  // last non-fall-through successor (branch-target cache)
};

struct Xmm { uint64_t lo = 0, hi = 0; };

class Memory {
public:
    Memory();
    ~Memory();
    void map(uint32_t address, const uint8_t* data, size_t size, bool readonly = false);
    uint8_t* page(uint32_t address);                // nullptr if unmapped
    const uint8_t* peek(uint32_t address) const { return pages_[address >> 12]; }   // resident page or nullptr (no fault)
    bool mapped(uint32_t address) const { return pages_[address >> 12] != nullptr || backed_[address >> 12]; }
    // Lazily paged contents: pages marked backed are filled by the loader on first touch.
    void set_loader(std::function<void(uint32_t page, uint8_t* out)> loader) { loader_ = std::move(loader); }
    void back(uint32_t address) { backed_[address >> 12] = 1; }
    size_t resident_pages() const;
    // Fast paths inline (mapped page, access within the page, page already dirty for writes).
    // Read tracking (OPEN REFLEX construction scope, boot_main REFLEX_READSET): one bit per 16-byte granule of the 4 GB space
    // (32 MB), set on every interpreted read. The JIT reads pages directly: run with it off while tracking.
    uint8_t* read_map = nullptr;
    inline uint64_t read(uint32_t a, int n) {
        if (__builtin_expect(read_map != nullptr, 0)) { read_map[a >> 7] |= uint8_t(1u << ((a >> 4) & 7)); uint32_t e = a + uint32_t(n) - 1; read_map[e >> 7] |= uint8_t(1u << ((e >> 4) & 7)); }
        uint32_t off = a & 0xfff; uint8_t* p = pages_[a >> 12];
        if (p && off + uint32_t(n) <= 4096) {
            switch (n) {
                case 1: return p[off];
                case 2: { uint16_t v; std::memcpy(&v, p + off, 2); return v; }
                case 4: { uint32_t v; std::memcpy(&v, p + off, 4); return v; }
                case 8: { uint64_t v; std::memcpy(&v, p + off, 8); return v; }
            }
        }
        return read_slow(a, n);
    }
    inline void write(uint32_t a, int n, uint64_t v) {
        uint32_t off = a & 0xfff, page = a >> 12; uint8_t* p = pages_[page];
        if (p && off + uint32_t(n) <= 4096 && dirty_[page] && !readonly_[page]) {
            switch (n) {
                case 1: p[off] = uint8_t(v); return;
                case 2: { uint16_t x = uint16_t(v); std::memcpy(p + off, &x, 2); return; }
                case 4: { uint32_t x = uint32_t(v); std::memcpy(p + off, &x, 4); return; }
                case 8: std::memcpy(p + off, &v, 8); return;
            }
        }
        write_slow(a, n, v);
    }
    uint64_t read_slow(uint32_t a, int n);
    void write_slow(uint32_t a, int n, uint64_t v);
    void read_bytes(uint32_t a, void* out, size_t n);
    void write_bytes(uint32_t a, const void* in, size_t n);
    // Bulk page-wise copies (same semantics as read_bytes/write_bytes, faster for large blocks).
    void read_block(uint32_t a, void* out, size_t n);
    void write_block(uint32_t a, const void* in, size_t n);   // nullptr: zero fill
    void unmap(uint32_t address, size_t size);              // drops pages (and their backing)
    void set_readonly(uint32_t address, size_t size, bool readonly);
    // Pages written since the last clear (for deltas).
    std::vector<uint32_t> dirty_pages() const;
    void clear_dirty();
    const uint8_t* original(uint32_t page_address) const;   // snapshot of initial contents
    void keep_originals(bool keep) { keep_originals_ = keep; }
    // Write journal (native/game port verification): after journal_begin, the first write to each page saves its pre-image.
    // journal_restore puts every journaled page back; journal_pages lists them; journal_end stops and drops the pre-images.
    // (journal_begin resets the dirty marks of the pages dirtied since the last clear, as clear_dirty does for all of them.)
    void journal_begin();
    void journal_restore();
    void journal_end();
    const std::vector<uint32_t>& journal_pages() const { return jorder_; }   // page numbers, first-write order
    const uint8_t* journal_pre(uint32_t page) const { auto it = jpre_.find(page); return it == jpre_.end() ? nullptr : it->second.get(); }
    bool journaling() const { return journal_; }
    // JIT access: resident page per page number, and the pages writable without bookkeeping (dirty, not read-only).
    uint8_t* const* page_table() const { return pages_.data(); }
    uint8_t* const* write_table() const { return wpages_.data(); }
private:
    std::vector<uint8_t*> pages_, wpages_;
    std::vector<uint8_t> readonly_, dirty_, backed_;
    std::function<void(uint32_t, uint8_t*)> loader_;
    uint8_t* fault(uint32_t page);
    std::unordered_map<uint32_t, std::unique_ptr<uint8_t[]>> originals_;
    bool keep_originals_ = false;
    uint8_t* ensure(uint32_t page);
    void first_write(uint32_t page, const uint8_t* p);   // a page's first write since its dirty mark was cleared
    std::vector<uint32_t> dlist_, jorder_;               // pages dirtied since the last clear; journaled pages
    std::unordered_map<uint32_t, std::unique_ptr<uint8_t[]>> jpre_;
    bool journal_ = false;
};

struct Module { uint32_t start, end; std::vector<uint8_t> image; };

class Machine;
class Jit;
using Native = std::function<uint32_t(Machine&)>;   // returns the next PC
// A native that returns RUN_ORIGINAL declines: the original instruction at its address runs instead.
constexpr uint32_t RUN_ORIGINAL = 0xffffffffu;
// CPUID as the interpreter answers it (vm.cpp M_cpuid; ports that execute cpuid call this so they always agree with it).
// A family-6 Intel (Core 2 stepping, 1 logical CPU) with SSE3, SSE2, SSE, MMX, CMOV, CX8, TSC, FPU (no SSSE3 / SSE4: the interpreter's
// instruction set). Leaf 1: EAX signature 0x6fb, EBX 0x00010800 (brand 0, CLFLUSH 8 qwords, 1 logical CPU), ECX 1 (SSE3),
// EDX 0x078bfbff (SSE2 bit 26, SSE 25, FXSR 24, MMX 23, CMOV 15, ...). Until 2026-10-18 leaf 1 had EBX / ECX / EDX mixed up (the feature
// word in EBX): the exe's __get_sse2_info saw no SSE2 and [0x00dcf760] __sse2_available was 0 in every world (notes/ld5_hi.md, loading.md 9).
inline void cpuid(uint32_t& eax, uint32_t& ebx, uint32_t& ecx, uint32_t& edx) {
    const uint32_t leaf = eax;
    if (leaf == 0) { eax = 1; ebx = 0x756e6547; edx = 0x49656e69; ecx = 0x6c65746e; }   // "GenuineIntel"
    else if (leaf == 1) { eax = 0x000006fb; ebx = 0x00010800; ecx = 0x00000001; edx = 0x078bfbff; }
    else if (leaf == 0x80000000u) { eax = 0x80000000u; ebx = ecx = edx = 0; }
    else { eax = ebx = ecx = edx = 0; }
}

class Machine {
    friend struct Fast;
    friend class Jit;
    friend struct JitHelpers;
public:
    Memory mem;
    uint32_t r[8] = {};
    uint32_t flags = 0x202;
    Xmm xmm[8];
    std::vector<reflex_f80::F80> fpu;   // st(0) is back()
    uint16_t fpu_cw = 0x027f, fpu_sw = 0;
    uint32_t mxcsr = 0x1f80;
    // MMX registers. MMX use requires an empty x87 stack and ends with emms, which
    // leaves the x87 stack empty again (the aliasing is then unobservable); any
    // other mix is reported instead of approximated.
    uint64_t mmx[8] = {};
    bool mmx_active = false;
    uint64_t steps = 0;
    // Time slicing (cooperative threads): run() returns the current pc at an instruction
    // boundary once steps reaches slice_end (sliced is set); UINT64_MAX disables it.
    uint64_t slice_end = UINT64_MAX;
    bool sliced = false;
    uint64_t trace_every = 0;      // debugging: checkpoint every N instructions
    void* trace_out = nullptr;
    uint64_t trace_from = 0, trace_to = 0, trace_mod = 1;
    uint32_t watch_address = 0;     // debugging: log writes covering this address
    uint32_t watch_pc = 0;          // debugging: the instruction being interpreted while watch_address is set (logged with each write)
    uint64_t* histogram = nullptr;  // debugging: per-mnemonic execution counts (M_COUNT entries)
    uint64_t* jit_histo = nullptr;
    // run() also returns at this pc once ESP is above stop_esp (native/game Verify: the original runs to its real return address;
    // a recursive inner return to the same address has a lower ESP). The JIT must be off while it is set (chained blocks).
    uint32_t stop_pc = 0, stop_esp = 0;
    // native/game Verify inside the boot: > 0 while a verified call runs (no thread switch may happen: the boot throws WouldSwitch).
    int no_switch = 0;
    // Instruction budget of call_function (nested calls of natives) and of Verify's original run; the boot raises it (UINT64_MAX / 4:
    // a port high in the loader's call tree runs billions of instructions on one thread).
    uint64_t call_budget = 2000000000ull;
    uint32_t* pc_counts = nullptr;  // coverage (native/game progress): executions per instruction, index pc - 0x400000, size 0x600000 (exe only)
    uint64_t outside_exe = 0;       // coverage: instructions outside the exe (the CRT DLLs)
    uint32_t* dll_counts = nullptr; // coverage: executions per instruction in 0x78400000..0x78600000 (msvcr90 / msvcp90), index pc - 0x78400000  // debugging with the JIT on: [mn] run by the JIT's generic helper, [M_COUNT + mn] interpreted outside blocks
    std::vector<Module> modules;
    std::unordered_map<uint32_t, std::string> import_names;
    std::unordered_map<std::string, Native> imports;
    std::unordered_map<uint32_t, Native> natives;
    std::unordered_map<std::string, uint32_t> crt_exports;

    Machine();
    ~Machine();
    // Block JIT (jit.cpp): hot straight-line code translated to host code; exact interpreter semantics.
    std::unique_ptr<Jit> jit;
    bool jit_enabled = false;
    bool jit_exact_slices = false;          // translated blocks never run past slice_end (boot: thread switches at the interpreter's exact steps)
    std::exception_ptr jit_exception;       // a fault inside translated code (rethrown by run)
    uint8_t* const* jit_pages = nullptr; uint8_t* const* jit_wpages = nullptr; const uint8_t* jit_parity = nullptr;
    void enable_jit(bool on);
    uint32_t run(uint32_t pc, uint64_t budget = 2000000000ull);
    uint32_t call_function(uint32_t address, const uint32_t* args, int nargs, int64_t ecx = -1, int64_t eax = -1);
    // A nested guest call with the REAL return address in its slot (native/game lifted ports): runs `address` until it returns to
    // `ra` with ESP above the slot (stop_pc / stop_esp, saved and restored; the JIT never chains past stop_pc), or the run ends
    // otherwise (pc 0: a stand-in ended the thread's run). A thread blocked inside it shows the original's return address.
    // esp_below: the callee returns with ESP below its slot (frame-allocating helpers: __SEH_prolog4, __EH_prolog3_catch,
    // __alloca_probe): the run stops at `ra` whatever ESP is.
    uint32_t call_at(uint32_t address, uint32_t ra, bool esp_below = false);
    // A tail jump (jmp) at a function's entry ESP: `address` returns to the return address at [ESP], which stays in place; the run
    // stops there (as call_at). Returns that return address (the native's next pc).
    uint32_t tail_at(uint32_t address);

    // stack and native helpers
    void push(uint32_t v);
    uint32_t pop();
    uint32_t arg(int i);                                  // i-th stack argument at native entry
    uint32_t ret(uint32_t cleanup, int64_t eax = -1);     // native return

private:
    std::vector<std::unique_ptr<Insn*[]>> decoded_;   // page -> 4096 decoded-instruction slots
    std::vector<uint8_t> special_;                     // pages holding natives or import sentinels
    size_t special_count_ = SIZE_MAX;
    void refresh_special();
    std::unordered_map<uint32_t, Native*> import_cache_;   // import sentinel -> its handler in `imports`
    size_t import_cache_size_ = 0;
    std::vector<std::unique_ptr<Insn[]>> blocks_;
    size_t block_used_ = 0;
    void* capstone_ = nullptr;
    const Insn& decode(uint32_t pc);
    uint32_t enter_native(uint32_t pc);
    uint32_t execute(const Insn& in);
    uint32_t execute_fp(const Insn& in);
    uint32_t ea(const Operand& o) const;
    uint64_t get(const Operand& o, int size = 0);
    void put(const Operand& o, uint64_t v, int size = 0);
    Xmm get128(const Operand& o);
    void put128(const Operand& o, const Xmm& v);
    void set_logic(uint64_t r, int size);
    uint64_t set_add(uint64_t a, uint64_t b, uint64_t carry, int size, bool sub);
    bool cond(uint16_t mn) const;
    // x87
    int fbits() const;
    int frc() const { return (fpu_cw >> 10) & 3; }
    reflex_f80::F80& st(int i);
    void fpush(const reflex_f80::F80& v);
    void fpop();
    reflex_f80::F80 fload(const Operand& o);
    void fstore(const Operand& o, const reflex_f80::F80& v);
};

}  // namespace reflex_vm
