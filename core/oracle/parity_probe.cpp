// parity_probe.cpp -- w-parity (development only): the original's side of core/tools/parity_pair.inc. rx_ParityCompositeLog (group
// "rx_probe", select it by name) logs every CollisionPair_CompositeVsComposite 008732d0 call to RX_PARITY_LOG: its inputs at the entry,
// then the original runs (tail_at) and its new contact records are logged; the call's own result is the original's.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace {
reflex_vm::Machine* g_pm = nullptr;
uint32_t par_rd(uint32_t a) { return g_pm->mem.mapped(a) && g_pm->mem.mapped(a + 3) ? uint32_t(g_pm->mem.read(a, 4)) : 0u; }
#define PAR_RD(a) par_rd(uint32_t(a))
#include "../tools/parity_pair.inc"
#undef PAR_RD
bool g_in = false;
unsigned long g_seq = 0;
FILE* log_file() { static FILE* f = nullptr; if (!f) { const char* p = std::getenv("RX_PARITY_LOG"); f = p && *p ? std::fopen(p, "w") : stderr; } return f; }
}  // namespace

RX_PORT_RAW(0x008732d0, ParityCompositeLog, "rx_probe", 0) {
    if (g_in) return reflex_vm::RUN_ORIGINAL;
    g_pm = &m;
    const uint32_t M = m.r[1], before = par_rd(M), swap = m.arg(6), seq = g_seq++;
    parity_pair_entry(log_file(), seq, m.r[4], M, m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), swap);
    g_in = true;
    uint32_t ra;
    try { ra = m.tail_at(0x008732d0u); } catch (...) { g_in = false; throw; }
    g_in = false;
    parity_pair_exit(log_file(), seq, M, before, swap);
    return ra;
}

// rx_ParityDispatchLog: every Collision_ContactDispatch 0085ddd0 call near PARITY_NEAR (nested calls included: a re-entry at the ESP of
// the call being run is the original itself)
namespace { std::vector<uint32_t> g_desp; unsigned long g_dseq = 0; }
RX_PORT_RAW(0x0085ddd0, ParityDispatchLog, "rx_probe", 0) {
    if (!g_desp.empty() && g_desp.back() == m.r[4]) return reflex_vm::RUN_ORIGINAL;
    g_pm = &m;
    const uint32_t M = m.r[1], pA = m.arg(2), pB = m.arg(5);
    static const bool d0 = std::getenv("PARITY_DEPTH0") != nullptr;
    if ((!parity_near(pA) && !parity_near(pB)) || (d0 && !g_desp.empty())) {
        g_desp.push_back(m.r[4]); uint32_t ra;
        try { ra = m.tail_at(0x0085ddd0u); } catch (...) { g_desp.pop_back(); throw; }
        g_desp.pop_back(); return ra;
    }
    const unsigned long seq = g_dseq++; const uint32_t before = par_rd(M);
    parity_dispatch_entry(log_file(), seq, int(g_desp.size()), M, m.arg(0), m.arg(1), pA, m.arg(3), m.arg(4), pB, m.arg(6));
    g_desp.push_back(m.r[4]); uint32_t ra;
    try { ra = m.tail_at(0x0085ddd0u); } catch (...) { g_desp.pop_back(); throw; }
    g_desp.pop_back();
    g_pm = &m;
    parity_dispatch_exit(log_file(), seq, M, before);
    return ra;
}


// rx_ParityIKLog (w-parity2): every InverseKinematics two-bone limb solve 006b0eb0 (EAX = skeleton; stack limb, pose, dt) whose target
// pointer is PARITY_IKTGT, logged before and after the original runs (core/tools/parity/ik_log.inc; RX_PARITY_LOG=<file>)
namespace {
#define PAR_RD(a) par_rd(uint32_t(a))
#include "../tools/parity/ik_log.inc"
#undef PAR_RD
bool g_ik_in = false;
}
RX_PORT_RAW(0x006b0eb0, ParityIKLog, "rx_probe", 0) {
    if (g_ik_in) return reflex_vm::RUN_ORIGINAL;
    g_pm = &m;
    const uint32_t limb = m.arg(0), pose = m.arg(1);
    const unsigned long s = ik_entry(log_file(), limb, pose);
    g_ik_in = true;
    uint32_t ra;
    try { ra = m.tail_at(0x006b0eb0u); } catch (...) { g_ik_in = false; throw; }
    g_ik_in = false;
    g_pm = &m;
    ik_exit(log_file(), s, limb, pose);
    return ra;
}

// rx_ParityRandLog (w-parity2): every AI track-node reaction 005118c0 (stack driver, dt) with the game LCG at its entry
// (core/tools/parity/rand_log.inc; RX_PARITY_LOG=<file>)
namespace {
#define PAR_RD(a) par_rd(uint32_t(a))
#include "../tools/parity/rand_log.inc"
#undef PAR_RD
}
RX_PORT_RAW(0x005118c0, ParityRandLog, "rx_probe", 0) {
    g_pm = &m;
    rn_entry(log_file(), m.arg(0));
    return reflex_vm::RUN_ORIGINAL;
}

// rx_ParityCylLog (w-parity2): every CollisionPair_CylinderVsCylinder 0086bd80 near PARITY_NEAR with the stack below its entry ESP
// (core/tools/parity/cyl_log.inc; RX_PARITY_LOG=<file>)
namespace {
#define PAR_RD(a) par_rd(uint32_t(a))
#include "../tools/parity/cyl_log.inc"
#undef PAR_RD
}
RX_PORT_RAW(0x0086bd80, ParityCylLog, "rx_probe", 0) {
    g_pm = &m;
    if (parity_near(m.arg(3))) std::fprintf(log_file(), "CYS %llu\n", (unsigned long long)m.steps);   // the machine step (REFLEX_WATCH's W lines' second field)
    cy_entry(log_file(), m.r[4], m.arg(3));
    return reflex_vm::RUN_ORIGINAL;
}
