// The collision group's shape functions callable from its other ports.
#pragma once
#include "collision_terrain.h"

namespace collision {

// 00848650 (EDI composite, ECX child index): out = q * child rotation (composite+0x44 vec4 array, bounds-checked).
void composite_child_rotation(Guest& g, uint32_t comp, uint32_t i, uint32_t q, uint32_t out);
// 008486f0 (EDI composite, ECX child index): out = q * child offset (composite+0x5c, xyz) * conj(q) + pos.
void composite_child_position(Guest& g, uint32_t comp, uint32_t i, uint32_t q, uint32_t pos, uint32_t out);
// The exe's std::vector bounds check: index >= (end - begin) >> shift (arithmetic) calls _invalid_parameter_noinfo.
inline void vector_check(Guest& g, uint32_t vec_begin_field, uint32_t index, int shift) {
    const int32_t n = (g.i32(vec_begin_field + 4) - g.i32(vec_begin_field)) >> shift;
    if (!(index < uint32_t(n))) g.call(g.u32(kInvalidParameter));
}

}  // namespace collision
