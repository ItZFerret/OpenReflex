// oracle.cpp -- the RX_ORACLE memory backend and rx_call_original over the emulated machine (see oracle.h).
#include "oracle.h"
#include <stdexcept>
#include <map>
#include <cstdlib>
#include <cstdio>
#include "f80/f80.h"

namespace { thread_local reflex_vm::Machine* g_m = nullptr; }
rx_oracle::Bind::Bind(reflex_vm::Machine& m) : prev(g_m) { g_m = &m; }
rx_oracle::Bind::~Bind() { g_m = prev; }

static reflex_vm::Machine& M() { if (!g_m) throw std::runtime_error("rxcore oracle: memory access outside a bound call"); return *g_m; }
extern "C" {
uint8_t  rx_rd8(rx_addr a)  { return uint8_t(M().mem.read(a, 1)); }
uint16_t rx_rd16(rx_addr a) { return uint16_t(M().mem.read(a, 2)); }
uint32_t rx_rd32(rx_addr a) { return uint32_t(M().mem.read(a, 4)); }
uint64_t rx_rd64(rx_addr a) { return M().mem.read(a, 8); }
// diagnostics (REFLEX_WATCH): the core's own writes covering the watched address, with the host return address (atos -o reflex_boot)
__attribute__((noinline, cold)) static void core_watch(reflex_vm::Machine& m, rx_addr a, int n, uint64_t v, void* ret) {
    static bool slide = false;
    if (!slide) { slide = true; std::fprintf(static_cast<FILE*>(m.trace_out), "C-SLIDE %p\n", (void*)&core_watch); }
    std::fprintf(static_cast<FILE*>(m.trace_out), "C %llu %08x %d %016llx host %p\n", (unsigned long long)m.steps, a, n, (unsigned long long)v, ret);
}
// (the caller's address is taken first: on arm64 __builtin_return_address(0) reads LR, which the M() call clobbers)
#define CORE_WRITE(a, n, v) do { void* volatile ra_ = __builtin_return_address(0); reflex_vm::Machine& m_ = M(); \
    if (__builtin_expect(m_.watch_address != 0, 0) && m_.trace_out && m_.watch_address >= (a) && m_.watch_address < (a) + (n)) core_watch(m_, (a), (n), (v), ra_); \
    m_.mem.write((a), (n), (v)); } while (0)
__attribute__((noinline)) void rx_wr8(rx_addr a, uint8_t v)   { CORE_WRITE(a, 1u, v); }
__attribute__((noinline)) void rx_wr16(rx_addr a, uint16_t v) { CORE_WRITE(a, 2u, v); }
__attribute__((noinline)) void rx_wr32(rx_addr a, uint32_t v) { CORE_WRITE(a, 4u, v); }
__attribute__((noinline)) void rx_wr64(rx_addr a, uint64_t v) { CORE_WRITE(a, 8u, v); }
// REFLEX_ORIG_CENSUS=1 (REFLEX_ORIG_LOG=1: each call at once): every address the core calls through rx_call_original / _ex (what the product would stop at), with its count, printed at exit.
namespace {
struct OrigCensus {
    std::map<uint32_t, uint64_t> n; bool on = getenv("REFLEX_ORIG_CENSUS") != nullptr;
    bool log = getenv("REFLEX_ORIG_LOG") != nullptr;   // w-parity5 (diagnostics): also each call at once on stderr ("orig-call <fn>")
    void hit(uint32_t fn) { if (on) n[fn]++; if (log) fprintf(stderr, "orig-call %08x\n", fn); }
    ~OrigCensus() { if (on) for (auto& kv : n) fprintf(stderr, "orig-census %08x %llu\n", kv.first, (unsigned long long)kv.second); }
} g_census;
}
// Unported callee: run the original in the emulator on the guest stack (ESP restored afterwards for every convention).
uint32_t rx_call_original(rx_addr fn, enum rx_conv conv, uint32_t ecx, int nargs, const uint32_t* args) {
    reflex_vm::Machine& m = M();
    g_census.hit(fn);
    const uint32_t esp = m.r[4];
    uint32_t r = m.call_function(fn, args, nargs, conv == RX_THISCALL ? int64_t(ecx) : -1);
    m.r[4] = esp;
    return r;
}
void rx_call_original_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int nargs, const uint32_t* args) {
    reflex_vm::Machine& m = M();
    g_census.hit(fn);
    const uint32_t esp = m.r[4];
    for (int i = 0; i < 8; i++) if (i != 4) m.r[i] = in->r[i];
    for (int i = 0; i < 8; i++) { uint32_t b[4]; std::memcpy(b, in->xmm[i].v, 16);
        m.xmm[i].lo = b[0] | (uint64_t(b[1]) << 32); m.xmm[i].hi = b[2] | (uint64_t(b[3]) << 32); }
    const size_t depth = m.fpu.size();
    for (int i = 0; i < in->fpu_in && i < 8; i++) {
        reflex_f80::F80 v; v.kind = in->fpu[i].kind; v.sign = in->fpu[i].sign; v.mant = in->fpu[i].mant; v.exp = in->fpu[i].exp;
        m.fpu.push_back(v);
    }
    m.call_function(fn, args, nargs);
    if (out) {
        out->fpu_depth = m.fpu.size() > depth ? int(m.fpu.size() - depth) : 0;
        for (int i = 0; i < out->fpu_depth && i < 8; i++) {
            const reflex_f80::F80& v = m.fpu[m.fpu.size() - 1 - size_t(i)];
            out->st[i].kind = v.kind; out->st[i].sign = v.sign; out->st[i].mant = v.mant; out->st[i].exp = v.exp;
        }
    }
    const float top = m.fpu.size() > depth ? rx_oracle::f32(reflex_f80::to_f32(m.fpu.back(), 0)) : 0.0f;
    if (m.fpu.size() > depth) m.fpu.resize(depth);
    if (out) {
        for (int i = 0; i < 8; i++) out->r[i] = m.r[i];
        for (int i = 0; i < 8; i++) { uint32_t b[4] = {uint32_t(m.xmm[i].lo), uint32_t(m.xmm[i].lo >> 32), uint32_t(m.xmm[i].hi), uint32_t(m.xmm[i].hi >> 32)};
            std::memcpy(out->xmm[i].v, b, 16); }
        out->st0 = top;
    }
    m.r[4] = esp;
}
uint32_t rx_call_original_regs(rx_addr fn, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi, int nargs, const uint32_t* args) {
    reflex_vm::Machine& m = M();
    g_census.hit(fn);
    const uint32_t esp = m.r[4], k3 = m.r[3], k5 = m.r[5], k6 = m.r[6], k7 = m.r[7];
    m.r[0] = 0; m.r[1] = 0; m.r[2] = 0; m.r[3] = ebx; m.r[5] = ebp; m.r[6] = esi; m.r[7] = edi;
    const uint32_t r = m.call_function(fn, args, nargs);
    m.r[4] = esp; m.r[3] = k3; m.r[5] = k5; m.r[6] = k6; m.r[7] = k7;
    return r;
}
rx_addr rx_sp(void) { return M().r[4]; }
void rx_set_sp(rx_addr sp) { M().r[4] = sp; }
int rx_mem_init(void) { return 1; }
void rx_mem_shutdown(void) {}
}
