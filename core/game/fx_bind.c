/* fx_bind.c -- a particle effect object's bind virtual (slot 1 of the 18 effect / emitter / controller classes that share it) and
 * the by-name lookup it uses. The effect set (the scene's effect description) is the object at +8; its +0xb0 holds the effect
 * table (+0x44 the type list: +0x10 array of 0x1f0-byte records whose first word is the record's name string and whose byte +0x160
 * says which of the set's two pointers +0xc0 / +0xc4 applies; +0x14 the count). notes/fx.md. */
#include "fx.h"
#include "fx_bind.h"

/* 007e3400 (ECX = the type list, stack the name; RET 4): the index of the record with this name (the inlined strcmp walks the
 * names two bytes a step), -1 when absent. */
int32_t rx_fx_effect_index_by_name(rx_addr set, rx_addr name) {
    const rx_addr recs = U32(set, 0x10);
    const int32_t n = I32(set, 0x14);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr s = U32(recs, 0x1f0u * (uint32_t)i);
        uint32_t k = 0;
        while (U8(name, k) == U8(s, k) && U8(name, k) != 0) k++;
        if (U8(name, k) == U8(s, k)) return i;
    }
    return -1;
}
RX_FUNCTION(0x007e3400, rx_fx_effect_index_by_name)

/* 0069b0e0 (ECX = this, stack set, name; RET 8): +8 = the effect set; when set: the index of `name` in its type list (clamped to
 * 0), the per-type pointer (+0x4) by the record's flag byte, the controller block at +0xd0 (+0xdc / +0xe0), the bound table
 * (+0x24 / +0x28), the squared extents of the set's +0x9c .. +0xa8 into +0xb4 .. +0xc0, +0xb0 -> the set's +0xd0. The table's
 * +0x4c is raised around the lookup (a re-entrancy mark: edi + 0x4c). */
void rx_fx_effect_bind(rx_addr self, rx_addr set, rx_addr name) {
    SU32(self, 8, set);
    if (set == 0) return;
    const rx_addr tab = U32(set, 0xb0);
    const rx_addr types = U32(tab, 0x44);
    SU32(tab, 0x4c, 1);
    int32_t idx = rx_fx_effect_index_by_name(types, name);
    SU32(self, 0xc, (uint32_t)idx);
    if (idx < 0) SU32(self, 0xc, 0);
    const uint32_t cur = U32(self, 0xc);
    const rx_addr ptr = U8(U32(types, 0x10), cur * 0x1f0u + 0x160u) != 0 ? U32(set, 0xc4) : U32(set, 0xc0);
    SU32(self, 4, ptr);
    SU32(tab, 0x4c, 0);
    SU32(self, 0xa4, self + 0xd0);
    SU32(self + 0xd0, 0xc, tab);
    SU32(self + 0xd0, 0x10, U32(self, 0xc));
    SU32(self, 0x24, tab);
    SU32(self, 0x28, U32(self, 0xc));
    SU32(self, 0x30, 1);
    SF32(self, 0x20, 0.0f);
    SF32(self, 0xa8, 0.0f);
    float a = F32(U32(self, 8), 0x9c); SF32(self, 0xb4, a * a);
    a = F32(U32(self, 8), 0xa0); SF32(self, 0xb8, a * a);
    a = F32(U32(self, 8), 0xa4); SF32(self, 0xbc, a * a);
    a = F32(U32(self, 8), 0xa8); SF32(self, 0xc0, a * a);
    SU32(self, 0xb0, U32(self, 8) + 0xd0);
}
RX_FUNCTION(0x0069b0e0, rx_fx_effect_bind)
