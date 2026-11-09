// world_wreck.cpp -- oracle adapters for core/game/world_wreck.c (group rx_world). The scratch registers these small functions
// leave are reproduced where the original's callers read them (reconstructed from the memory they leave, ABI glue only).
#include "oracle.h"
extern "C" {
#include "../game/world_wreck.h"
}
using namespace reflex_port;
using rx_oracle::bits;
static void set_xmm0(reflex_vm::Machine& m, uint32_t a) { m.xmm[0].lo = a; m.xmm[0].hi = 0; }
// EDX after a pass over a listener vector: the last enabled element's target, else the value it had.
static uint32_t last_enabled_target(uint32_t v, uint32_t edx) {
    const uint32_t begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return edx;
    for (uint32_t p = begin; p != end; p += 8) if (U32(p, 4) != 0) edx = U32(p, 0);
    return edx;
}
static uint32_t first_enabled_target(uint32_t v, uint32_t edx) {
    const uint32_t begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return edx;
    for (uint32_t p = begin; p != end; p += 8) if (U32(p, 4) != 0) return U32(p, 0);
    return edx;
}
// EAX / ECX of 0063b830 / 0063b8f0: ECX = end - begin; EAX = end (empty) or the container proxy (for the getter: its container
// when an enabled listener was found).
static uint32_t pass_eax(uint32_t v, bool getter) {
    const uint32_t begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return end;
    if (getter) for (uint32_t p = begin; p != end; p += 8) if (U32(p, 4) != 0) return U32(U32(v, 0), 0);
    return U32(v, 0);
}

RX_PORT(0x0063b830, FloatListeners_Set, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t v = m.arg(0);
    rx_world_float_listeners_set(v, m.arg(1));
    m.r[1] = U32(v, 0x10) - U32(v, 0xc);
    m.r[2] = last_enabled_target(v, m.r[2]);
    return m.ret(8, pass_eax(v, false));
}
RX_PORT(0x0063b8f0, FloatListeners_Get, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t v = m.arg(0);
    set_xmm0(m, bits(rx_world_float_listeners_get(v)));
    m.r[1] = U32(v, 0x10) - U32(v, 0xc);
    m.r[2] = first_enabled_target(v, m.r[2]);
    return m.ret(4, pass_eax(v, true));
}
RX_PORT(0x0063b0e0, RangedFloat_Get, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t v = m.r[0] + 0x14;
    set_xmm0(m, bits(rx_world_ranged_float_get(m.r[0])));
    m.r[1] = U32(v, 0x10) - U32(v, 0xc);
    m.r[2] = first_enabled_target(v, m.r[2]);
    return m.ret(0, pass_eax(v, true));
}
// 0063b0f0 (ESI = ranged float, EAX = float* input): EAX = ESI, ECX = the broadcast value, EDX = the last target (or +0x14).
RX_PORT(0x0063b0f0, RangedFloat_Step, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[6], slot = m.r[4] - 4;
    rx_world_ranged_float_step(obj, m.r[0]);
    m.r[1] = U32(slot, 0);
    m.r[2] = last_enabled_target(obj + 0x14, obj + 0x14);
    return m.ret(0, obj);
}
// 0063b160 (EAX = ranged float, ECX = float* input): EAX = 1 with listeners, ECX = the value, EDX = the last target (or the slot).
RX_PORT(0x0063b160, RangedFloat_StepAny, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[0], slot = m.r[4] - 4;
    const int r = rx_world_ranged_float_step_any(obj, m.r[1]);
    m.r[1] = U32(slot, 0);
    m.r[2] = last_enabled_target(obj + 0x14, slot);
    return m.ret(0, r ? 1u : 0u);
}
// 0063b480 (EAX = object, ECX = input): EAX kept, ECX = the out pointer.
RX_PORT(0x0063b480, ClampedFloat_Write, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t obj = m.r[0];
    rx_world_clamped_float_write(obj, m.r[1]);
    m.r[1] = U32(obj, 8);
    return m.ret(0, obj);
}
// 0063b500 (ECX = object; stdcall b; RET 4): xmm0 = the value, EAX = b.
RX_PORT(0x0063b500, ClampedFloat_Diff, "rx_world", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const uint32_t b = m.arg(0);
    set_xmm0(m, bits(rx_world_clamped_float_diff(m.r[1], b)));
    return m.ret(4, b);
}
// 0063b540 (ESI = object, EAX = input): EAX = ESI, ECX = the value, EDX = the last target (or +8).
RX_PORT(0x0063b540, ClampedFloat_Set, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[6], slot = m.r[4] - 4;
    rx_world_clamped_float_set(obj, m.r[0]);
    m.r[1] = U32(slot, 0);
    m.r[2] = last_enabled_target(obj + 8, obj + 8);
    return m.ret(0, obj);
}
// 0063b590 / 0063b650 (ESI = object; stdcall delta; RET 4): EAX = ESI, ECX kept, EDX = the last enabled target (or kept).
RX_PORT(0x0063b590, ClampedFloat_Add, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[6], edx = m.r[2];
    rx_world_clamped_float_add(obj, m.arg(0));
    m.r[2] = last_enabled_target(obj + 8, edx);
    return m.ret(4, obj);
}
RX_PORT(0x0063b650, WrappedFloat_Add, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[6], edx = m.r[2];
    rx_world_wrapped_float_add(obj, m.arg(0));
    m.r[2] = last_enabled_target(obj + 0x10, edx);
    return m.ret(4, obj);
}
// 0063b6c0 (EAX = object, EDX = short* input): ECX = the out pointer, DX = the value copied (when out is set).
RX_PORT(0x0063b6c0, ShortProperty_Set, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t obj = m.r[0], in = m.r[2];
    rx_world_short_property_set(obj, in);
    m.r[1] = U32(obj, 8);
    if (m.r[1]) m.r[2] = (in & 0xffff0000u) | U16(in, 0);
    return m.ret(0);
}
RX_PORT(0x00668910, Grid_CellContains, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_grid_cell_contains(m.r[1], m.arg(0), m.arg(1)) ? 1u : 0u);
}
RX_PORT(0x00668820, Grid_CellOf, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_grid_cell_of(m.r[1], m.r[2]));
}
RX_PORT(0x00668710, Grid_Neighbours, "rx_world", OUT_EAX) {
    rx_world_grid_neighbours(m.r[1], m.r[2]);
    return m.ret(0, 9);
}
// 006689c0 (EDI = grid; stdcall point; RET 4): AL = the result; the rest of EAX is what the last tested cell left there
// (-1's upper bytes for a cell outside the grid, else 0), reconstructed from the hit flags.
RX_PORT(0x006689c0, Grid_TestNeighbours, "rx_world", OUT_EAX) {
    const uint32_t grid = m.r[7], p = m.arg(0);
    const int r = rx_world_grid_test_neighbours(grid, p);
    uint32_t eax = p, bound = 9; bool up = false;
    for (uint32_t i = 0; i < bound; i++) {
        if (up && (i == 4 || i == 5 || i == 6)) continue;
        const uint32_t cell = U32(grid, 0x188 + 4 * i);
        eax = cell;
        if (int32_t(cell) < 0) continue;
        const bool hit = U32(grid, 0x1ac + 4 * i) == 1;
        eax = hit ? 1u : 0u;
        if (!hit) continue;
        if (i == 1) up = true; else if (i == 2) bound = 4; else if (i == 3 || i == 4) bound = 6;
    }
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4, (eax & 0xffffff00u) | (r ? 1u : 0u));
}
RX_PORT(0x00668690, Grid_Locate, "rx_world", OUT_EAX) {
    return m.ret(4, uint32_t(rx_world_grid_locate(m.r[0], m.arg(0))));
}
RX_PORT(0x00668650, Grid_CollectHits, "rx_world", 0) {
    rx_world_grid_collect_hits(m.r[0], m.r[2]);
    return m.ret(0);
}
// 00667e40 (ECX = shape, EDX = set 2; 6 stack args; RET 0x18): ECX = the security cookie its epilogue check leaves.
RX_PORT(0x00667e40, Grid_ReportOverlaps, "rx_world", OUT_ECX) {
    rx_world_grid_report_overlaps(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(0x18);
}
