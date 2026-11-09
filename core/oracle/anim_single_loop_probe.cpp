// Development-only lifecycle stimulus, never linked into the product.
// REFLEX_PORT=rx_anim,rx_AnimLoopLifecycleProbeHook REFLEX_PORT_VERIFY=1 RX_ANIM_LOOP_PROBE=1
// Optional RX_ANIM_LOOP_PROBE_FRAME (default 5), using the existing original-world oracle CLI.
// Each case allocates and constructs a fresh object through ORIGINAL allocator/constructor, then
// calls ORIGINAL 006b9b20 through its installed Verify adapter. No native cleanup is substituted.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
namespace {
uint32_t frame = 0;
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t function, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs(); rx_call_original_ex(function, &in, &out, n, args); return out;
}
void exercise(uint32_t flags) {
    const uint32_t size = 0x750;
    const uint32_t entity = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, &size);
    if (!entity) { std::fprintf(stderr, "[animloopprobe] allocation failed\n"); return; }
    // Constructor leaves some fields uninitialized; a deterministic fresh block is a valid controlled object.
    for (uint32_t at = 0; at < size; at += 4) rx_wr32(entity + at, 0);
    rx_regs ctor = regs(); ctor.r[6] = entity; // 006b9ad0: ESI, no stack arguments
    call(0x006b9ad0, ctor);
    rx_regs dtor = regs(); dtor.r[1] = entity;
    const rx_regs out = call(0x006b9b20, dtor, 1, &flags);
    std::fprintf(stderr, "[animloopprobe] submitted flags=%u entity=%08x return=%08x registries=%08x/%08x\n",
        flags, entity, out.r[0], rx_rd32(0x00a95824), rx_rd32(0x00a95828));
    if (!(flags & 1)) rx_call_original(0x0078fc50, RX_CDECL, 0, 1, &entity);
    // With bit0 the entity is already freed; no object memory is read after destruction.
}
void probe() {
    const uint32_t first = rx_rd32(0x00a95824), second = rx_rd32(0x00a95828);
    rx_wr32(0x00a95824, 0); rx_wr32(0x00a95828, 0);
    for (uint32_t flags = 0; flags <= 2; flags++) exercise(flags);
    rx_wr32(0x00a95824, first); rx_wr32(0x00a95828, second);
    if (first || second) {
        // Actual original-world audio collision / vehicle audio registries, never fabricated vtables.
        // Original constructor-only objects have not been registered; real removal follows the normal absent-entry path.
        for (uint32_t flags = 0; flags <= 2; flags++) exercise(flags);
    }
    std::fprintf(stderr, "[animloopprobe] stimulus complete: flags0/1/2 with null registries%s; inspect Verify counters\n",
        first || second ? " and live original registries" : " (live original registries absent)");
}
}
RX_PORT(0x0084ed20, AnimLoopLifecycleProbeHook, "thunk", 0) {
    const char* enabled = std::getenv("RX_ANIM_LOOP_PROBE");
    if (!enabled || !*enabled) return reflex_vm::RUN_ORIGINAL;
    const uint32_t at = std::getenv("RX_ANIM_LOOP_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_ANIM_LOOP_PROBE_FRAME"))) : 5;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;
    uint32_t saved[8]; reflex_vm::Xmm xmm[8];
    std::memcpy(saved, m.r, sizeof saved); std::memcpy(xmm, m.xmm, sizeof xmm);
    const uint32_t flags = m.flags; const auto fpu = m.fpu;
    probe();
    std::memcpy(m.r, saved, sizeof saved); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    return reflex_vm::RUN_ORIGINAL;
}
