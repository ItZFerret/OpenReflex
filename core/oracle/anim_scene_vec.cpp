// anim_scene_vec.cpp -- oracle adapters for core/game/anim_scene_vec.c (group rx_anim): the same registers as the collision code's 0080d280 / 0080d860.
#include "oracle.h"
extern "C" {
#include "../game/anim_scene_vec.h"
}
using namespace reflex_port;

// 007f90a0 (EAX vector; stack result iterator*, value*, _Where proxy, _Where position; RET 0x10): EAX = the result buffer, EDX = its proxy.
RX_PORT(0x007f90a0, SceneVectorInsert, "rx_anim", OUT_EAX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    m.r[0] = rx_scene_vector_insert(m.r[0], out, m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = rx_rd32(out + 4);
    m.r[2] = rx_rd32(out);
    return m.ret(0x10);
}
// 007f95a0 (stack vector, value*, proxy, position; RET 0x10)
RX_PORT(0x007f95a0, SceneVectorReallocate, "rx_anim", 0) {
    rx_scene_vector_reallocate(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
