// body_sort_probe.cpp -- the oracle's probe for the contact-handler sort's introsort pieces (core/game/world_body.c): the loop
// 00776ef0 with more than 32 handlers, the median-of-three 00778190 / median 00777c70, the partition 00777580, the heap pieces
// 00777d20 / 00777d60 / 00778200 / 00778660. The reference worlds never hold more than 32 handlers in the list (the loop goes
// straight to the insertion sort). A frame hook (Engine::Update 0084ed20, outside every verified call) builds arrays of fake
// handler objects (only +0x80, the kinematic record index, is read by the comparator 007686b0) over the live world's kinematic
// velocity records (which it fills with a pseudo-random pattern) and calls the ORIGINAL addresses; with the ports installed Verify
// runs the core function and then the original on the same state and compares memory and registers. Each case runs on a snapshot
// of the whole guest memory and machine state, restored afterwards.
//   REFLEX_PORT=rx_world,rx_BodySortProbeHook REFLEX_PORT_VERIFY=1 RX_BS_PROBE=1 [RX_BS_FRAME=<n>] [RX_BS_CASES=<n>]
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { if (mapped(a)) rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }

struct Pages {
    reflex_vm::Memory& memory;
    struct Page { uint32_t address; std::array<uint8_t, 4096> bytes; };
    std::vector<Page> saved;
    std::vector<uint8_t> was;
    explicit Pages(reflex_vm::Memory& m) : memory(m), was(1u << 20, 0) {
        for (uint32_t i = 0; i < (1u << 20); i++) if (m.mapped(i << 12)) {
            was[i] = 1; saved.push_back(Page{}); auto& p = saved.back(); p.address = i << 12;
            std::memcpy(p.bytes.data(), m.page(p.address), 4096);
        }
    }
    void restore() {
        for (auto& p : saved) std::memcpy(memory.page(p.address), p.bytes.data(), 4096);
        for (uint32_t i = 0; i < (1u << 20); i++) if (!was[i] && memory.mapped(i << 12)) memory.unmap(i << 12, 4096);
    }
};

uint32_t g_seed = 12345;
uint32_t rnd() { g_seed = g_seed * 1664525u + 1013904223u; return g_seed >> 8; }

uint32_t find_world() {
    const uint32_t g = rd(0x00a946b8);
    if (!g) return 0;
    for (uint32_t s = 0; s < 12; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t a = rd(p + 0x94);
        if (a && rd(p + 4) && rd(a + 0x234)) return rd(a + 0x234);
    }
    return 0;
}

struct Machine_state {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint64_t mmx[8];
    decltype(reflex_vm::Machine::flags) flags; decltype(reflex_vm::Machine::fpu) fpu; decltype(reflex_vm::Machine::fpu_cw) cw, sw;
    decltype(reflex_vm::Machine::mxcsr) mxcsr; bool mmx_active;
};

// pattern: 0 random distinct-ish, 1 many ties (8 distinct speeds), 2 ascending, 3 descending, 4 all equal, 5 organ pipe
struct Case { uint32_t n; int pattern; int32_t ideal; int what; };

void run_case(reflex_vm::Machine& m, uint32_t world, const Case& c, int idx) {
    Machine_state st;
    std::memcpy(st.r, m.r, sizeof st.r); std::memcpy(st.xmm, m.xmm, sizeof st.xmm); std::memcpy(st.mmx, m.mmx, sizeof st.mmx);
    st.flags = m.flags; st.fpu = m.fpu; st.cw = m.fpu_cw; st.sw = m.fpu_sw; st.mxcsr = m.mxcsr; st.mmx_active = m.mmx_active;
    auto host_restore = snapshot_host_state(m);
    if (!host_restore) { std::fprintf(stderr, "[bs] ERROR missing reversible host state\n"); return; }
    Pages pages(m.mem);
    const uint32_t sp0 = rx_sp();
    const uint32_t n = c.n;
    /* the kinematic velocity records of the world: record k at world + (k + 0x5d) * 0x50, speed from the pattern */
    for (uint32_t k = 0; k < n; k++) {
        float speed;
        switch (c.pattern) {
        case 0: speed = float(rnd() % 100000) / 7.0f; break;
        case 1: speed = float(rnd() % 8) * 3.0f; break;
        case 2: speed = float(k); break;
        case 3: speed = float(n - k); break;
        case 4: speed = 5.0f; break;
        default: speed = float(k < n / 2 ? k : n - k); break;
        }
        const uint32_t rec = world + (k + 0x5d) * 0x50;
        wr(rec + 0, rx_oracle::bits(speed)); wr(rec + 4, 0); wr(rec + 8, 0); wr(rec + 0xc, 0);
    }
    /* the fake handler objects and the pointer array on the game stack */
    const uint32_t objs = sp0 - 0x20000, arr = objs - 4 * n - 0x100;
    rx_set_sp(arr - 0x1000);
    for (uint32_t k = 0; k < n; k++) {
        const uint32_t o = objs + 0x90 * k;
        wr(o + 0x80, k);
        wr(arr + 4 * k, o);
    }
    /* shuffle the pointer array (pattern 0..1 keep their own order; the others stay as built) */
    if (c.pattern <= 1) for (uint32_t k = n; k > 1; k--) { const uint32_t j = rnd() % k; const uint32_t t = rd(arr + 4 * (k - 1)); wr(arr + 4 * (k - 1), rd(arr + 4 * j)); wr(arr + 4 * j, t); }
    rx_regs in = regs(), out = regs();
    switch (c.what) {
    case 0: {   /* the sort loop: first, last, ideal, world */
        const uint32_t a[4] = {arr, arr + 4 * n, uint32_t(c.ideal), world};
        rx_call_original_ex(0x00776ef0, &in, &out, 4, a);
        break; }
    case 1: {   /* the partition: out, first, last, world */
        const uint32_t o = arr - 0x80;
        const uint32_t a[4] = {o, arr, arr + 4 * n, world};
        rx_call_original_ex(0x00777580, &in, &out, 4, a);
        std::fprintf(stderr, "[bs]   partition -> pfirst %d plast %d\n", int((rd(o) - arr) / 4), int((rd(o + 4) - arr) / 4));
        break; }
    case 2: {   /* make_heap then sort_heap: EAX = last, EDI = first, stack world */
        in.r[0] = arr + 4 * n; in.r[7] = arr;
        const uint32_t a[1] = {world};
        rx_call_original_ex(0x00777d20, &in, &out, 1, a);
        in = regs(); in.r[0] = arr + 4 * n; in.r[7] = arr;
        rx_call_original_ex(0x00777d60, &in, &out, 1, a);
        break; }
    case 3: {   /* the median: EAX = first, stack mid, last - 1, world */
        in.r[0] = arr;
        const uint32_t a[3] = {arr + 4 * (n / 2), arr + 4 * (n - 1), world};
        rx_call_original_ex(0x00777c70, &in, &out, 3, a);
        break; }
    case 4: {   /* med3 on three positions: ESI = a, EDI = b, stack c, world */
        in.r[6] = arr + 4 * (n / 2); in.r[7] = arr; 
        const uint32_t a[2] = {arr + 4 * (n - 1), world};
        rx_call_original_ex(0x00778190, &in, &out, 2, a);
        break; }
    case 5: {   /* adjust_heap / push_heap on an unsorted array (hole 0 and a middle hole) */
        const uint32_t value = rd(arr);
        for (int hole : {0, int(n / 4), int(n / 2) - 1}) {
            in = regs(); in.r[0] = uint32_t(hole);
            const uint32_t a[4] = {arr, n, value, world};
            rx_call_original_ex(0x00778200, &in, &out, 4, a);
        }
        in = regs(); in.r[0] = n - 1;
        const uint32_t b[4] = {arr, 0, rd(arr + 4 * (n - 1)), world};
        rx_call_original_ex(0x00778660, &in, &out, 4, b);
        break; }
    }
    uint32_t sorted = 1, prev = 0;
    for (uint32_t k = 0; k < n; k++) { const uint32_t v = rd(rd(arr + 4 * k) + 0x80); if (k && v < prev && c.pattern == 2) sorted = 0; prev = v; }
    std::fprintf(stderr, "[bs] case %d what %d n %u pattern %d ideal %d (%u)\n", idx, c.what, n, c.pattern, c.ideal, sorted);
    rx_set_sp(sp0);
    pages.restore();
    host_restore();
    std::memcpy(m.r, st.r, sizeof st.r); std::memcpy(m.xmm, st.xmm, sizeof st.xmm); std::memcpy(m.mmx, st.mmx, sizeof st.mmx);
    m.flags = st.flags; m.fpu = st.fpu; m.fpu_cw = st.cw; m.fpu_sw = st.sw; m.mxcsr = st.mxcsr; m.mmx_active = st.mmx_active;
}
}  // namespace

RX_PORT(0x0084ed20, BodySortProbeHook, "thunk", 0) {
    if (!std::getenv("RX_BS_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_BS_FRAME") ? uint32_t(std::atoi(std::getenv("RX_BS_FRAME"))) : 600;
    if (++g_frame != at) return reflex_vm::RUN_ORIGINAL;
    g_m = &m;
    const uint32_t world = find_world();
    if (!world) { std::fprintf(stderr, "[bs] no world at frame %u\n", g_frame); return reflex_vm::RUN_ORIGINAL; }
    std::fprintf(stderr, "[bs] world %08x handler list %08x..%08x\n", world, rd(world + 0xcc00 + 0xc), rd(world + 0xcc00 + 0x10));
    int idx = 0;
    static const uint32_t ns[] = {2, 3, 5, 33, 34, 40, 41, 42, 64, 100, 129, 257, 600};
    for (uint32_t n : ns)
        for (int pattern = 0; pattern < 6; pattern++) {
            if (n > 41 && pattern > 1 && n > 129) continue;
            for (int what = 0; what < 6; what++) {
                if (what == 0) {
                    for (int32_t ideal : {int32_t(n), int32_t(40), int32_t(0), int32_t(-1)}) run_case(m, world, Case{n, pattern, ideal, 0}, idx++);
                } else if (n >= 3) {
                    run_case(m, world, Case{n, pattern, int32_t(n), what}, idx++);
                }
            }
        }
    std::fprintf(stderr, "[bs] frame %u done (%d cases)\n", g_frame, idx);
    return reflex_vm::RUN_ORIGINAL;
}
