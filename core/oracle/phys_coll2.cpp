// phys_coll2.cpp -- oracle adapters for core/game/phys_coll_shape2.c and phys_coll_misc.c (second batch of the collision package).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_coll_shape2.h"
#include "../game/phys_coll_misc.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX; using reflex_port::OUT_XMM0; using reflex_port::OUT_FPU;

// 00846380 (thiscall: q*, pos*, out min*, out max*; RET 0x10): EAX = pos for the kinds 1..3 else kind - 3, EDX = out min.
RX_PORT(0x00846380, CollisionShapeStaticBox_vf3, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1];
    phys_static_box_bounds(self, m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    const uint32_t kind = rx_rd32(self + 0x30);
    m.r[0] = (kind >= 1 && kind <= 3) ? m.arg(1) : kind - 3;
    m.r[2] = m.arg(2);
    return m.ret(0x10);
}
// 00848ef0 (thiscall, stack name; RET 4): EAX = index or -1
RX_PORT(0x00848ef0, CollisionShapeStaticComposite_vf8, "rx_phys", OUT_EAX) {
    m.r[0] = uint32_t(phys_static_composite_find(m.r[1], m.arg(0)));
    return m.ret(4);
}
// 00847f60 (thiscall: q*, pos*, out min*, out max*; RET 0x10): EAX = child count
RX_PORT(0x00847f60, CollisionShapeComposite_vf2, "rx_phys", OUT_EAX) {
    m.r[0] = phys_composite_vf2(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 00848af0 (thiscall, stack scale; RET 4): EAX = the last offset's address
RX_PORT(0x00848af0, CollisionShapeComposite_vf4, "rx_phys", OUT_EAX) {
    m.r[0] = phys_composite_scale(m.r[1], argf(m, 0));
    return m.ret(4);
}
// 00848bd0 (thiscall; plain RET): EAX = child count
RX_PORT(0x00848bd0, CollisionShapeComposite_vf5, "rx_phys", OUT_EAX) {
    m.r[0] = phys_composite_restore(m.r[1]);
    return m.ret(0);
}
// 00845f90 (ECX box; plain RET) -- ECX, EDX kept
RX_PORT(0x00845f90, FUN_00845f90, "rx_phys", OUT_ECX | OUT_EDX) {
    phys_box_vertex_table(m.r[1]);
    return m.ret(0);
}
// 00846230 (ESI q*, EDX pos*, ECX box; plain RET) -- ECX kept
RX_PORT(0x00846230, FUN_00846230, "rx_phys", OUT_ECX) {
    phys_box_world_vertices(m.r[1], m.r[6], m.r[2]);
    return m.ret(0);
}
// 008472d0 (stack cylinder; RET 4)
RX_PORT(0x008472d0, FUN_008472d0, "rx_phys", 0) {
    phys_cylinder_vertex_table(m.arg(0));
    return m.ret(4);
}
// 00847630 (ECX pos*, EDX q*, stack cylinder; RET 4)
RX_PORT(0x00847630, FUN_00847630, "rx_phys", 0) {
    phys_cylinder_world_vertices(m.arg(0), m.r[1], m.r[2]);
    return m.ret(4);
}

// ---------------------------------------------------------------- vector / container helpers
// 008423d0 (EDI vector, stack value*; RET 4): EAX
RX_PORT(0x008423d0, FUN_008423d0, "rx_phys", OUT_EAX) {
    m.r[0] = phys_vector_push_back(m.r[7], m.arg(0));
    return m.ret(4);
}
// 0080d280 (EAX vector; stack result iterator*, value*, _Where proxy, _Where position; RET 0x10): EAX = the result buffer,
// ECX = its position (left by 00842fa0), EDX = its proxy.
RX_PORT(0x0080d280, FUN_0080d280, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const uint32_t out = m.arg(0);
    m.r[0] = phys_vector_insert_n(m.r[0], out, m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = rx_rd32(out + 4);
    m.r[2] = rx_rd32(out);
    return m.ret(0x10);
}
// 0080d860 (stack vector, value*, proxy, position; RET 0x10). ECX at return is the epilogue's "pop ecx" of the __security_cookie
// xor EBP (the adapter's entry ESP - 4); live according to plan/live.tsv.
RX_PORT(0x0080d860, FUN_0080d860, "rx_phys", OUT_ECX) {
    phys_vector_reallocate(m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = rx_rd32(0x9b45e4) ^ (m.r[4] - 4);
    return m.ret(0x10);
}
// 00842fa0 (ESI iterator, stack n; RET 4): EAX = iterator; ECX = new position, EDX = the container (0 without proxy)
RX_PORT(0x00842fa0, FUN_00842fa0, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t it = m.r[6];
    phys_iterator_advance(it, int32_t(m.arg(0)));
    const uint32_t proxy = rx_rd32(it);
    m.r[0] = it; m.r[1] = rx_rd32(it + 4); m.r[2] = proxy ? rx_rd32(proxy) : 0;
    return m.ret(4);
}
// 0084fe70 (ESI object, stack a, b; RET 8): EAX = index; EDX = the high word of the second division's multiply, shifted (before the sign fix)
RX_PORT(0x0084fe70, FUN_0084fe70, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t obj = m.r[6];
    m.r[0] = phys_pair_index(obj, m.arg(0), m.arg(1));
    const int32_t bytes = int32_t(rx_rd32(obj + 0x28) - rx_rd32(obj + 0x24));
    m.r[2] = uint32_t(int32_t((int64_t(bytes) * 0x2aaaaaab) >> 32) >> 1);
    return m.ret(8);
}
// 0084fe00 (ECX plane, stack v*; RET 4)
RX_PORT(0x0084fe00, FUN_0084fe00, "rx_phys", 0) {
    phys_plane_clip(m.r[1], m.arg(0));
    return m.ret(4);
}
// 00690e00 (ESI hash table, EAX key*, stack out*; RET 4): EAX = found; ECX = value (found) or hash; EDX = out* (found) or 0
RX_PORT(0x00690e00, FUN_00690e00, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    const uint32_t found = phys_hash_lookup(m.r[6], m.r[0], out);
    m.r[0] = found;
    m.r[2] = found ? out : 0;
    return m.ret(4);
}
// 007cf450 (ESI vector, stack out*, ., first, ., last; RET 0x14): EAX = out
RX_PORT(0x007cf450, FUN_007cf450, "rx_phys", OUT_EAX) {
    m.r[0] = phys_vector_erase(m.r[6], m.arg(0), m.arg(2), m.arg(4));
    return m.ret(0x14);
}

// 00845b50 (ESI point*, EDI box, stack normal out*; RET 4). ECX (live per liveness) is left as the box test and the arithmetic leave it;
// the callers overwrite it (see the report).
RX_PORT(0x00845b50, FUN_00845b50, "rx_phys", 0) {
    phys_box_clamp_point(m.r[7], m.r[6], m.arg(0));
    return m.ret(4);
}
