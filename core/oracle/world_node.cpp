// world_node.cpp -- oracle adapters for core/game/world_node*.c (group rx_world, "node" share).
#include "oracle.h"
extern "C" {
#include "../game/world_node.h"
}
using namespace reflex_port;

// 007db890 (ECX = record, stack subtree flag; RET 4). Callers read no register result (they reload ECX / EDX; xmm overwritten).
RX_PORT(0x007db890, Node_EvaluateWorld, "rx_world", 0) {
    rx_world_node_evaluate_world(m.r[1], m.arg(0));
    rx_world_node_evaluate_world_saved_regs(m.r[4], m.r[3], m.r[5], m.r[6], m.r[7]);   // its prologue's pushes (as a caller's port would)
    return m.ret(4);
}
// 007db7e0 (ECX = record; RET 0). EAX: invalid at entry: AL = the flags byte (upper bytes kept); else the topmost ancestor the
// bit-2 walk ended on (the record itself without a parent). EDX: DL = 0xfb, upper bytes from the node set when a child was walked.
RX_PORT(0x007db7e0, Node_Invalidate, "rx_world", OUT_EAX) {
    const uint32_t rec = m.r[1], eax = m.r[0];
    const uint8_t flags = uint8_t(U8(rec, 0xc));
    const bool child = U8(rec, 0xe) != 0xff;
    const uint32_t edx = m.r[2];
    rx_world_node_invalidate(rec, m.r[3], m.r[6], m.r[7]);
    if (!(flags & 1)) return m.ret(0, (eax & 0xffffff00u) | flags);
    uint32_t a = rec;
    while (U8(a, 0xf) != 0xff) a = U32(U32(a, 0), 8) + 16u * U8(a, 0xf);
    m.r[2] = ((child ? U32(rec, 0) : edx) & 0xffffff00u) | 0xfbu;
    return m.ret(0, a);
}
// 007dc890 (EDI = record, ESI = matrix; RET 0).
RX_PORT(0x007dc890, Node_SetWorld, "rx_world", 0) {
    rx_world_node_set_world(m.r[7], m.r[6], m.r[3]);
    return m.ret(0);
}
RX_PORT(0x007dcfb0, Node_SetWorldRotation, "rx_world", 0) {
    rx_world_node_set_world_rotation(m.r[1], m.arg(0), m.r[3]);
    return m.ret(4);
}
// 007dc560 (ESI = record, EDI = out; stdcall point; RET 4): EAX = out.
RX_PORT(0x007dc560, Node_WorldToLocalPoint, "rx_world", OUT_EAX) {
    return m.ret(4, rx_world_node_world_to_local_point(m.r[6], m.r[7], m.arg(0)));
}
// 007dd1b0 (stdcall record, vector; RET 8): ECX = the security cookie its epilogue check leaves.
RX_PORT(0x007dd1b0, Node_SetWorldPosition, "rx_world", OUT_ECX) {
    rx_world_node_set_world_position(m.arg(0), m.arg(1), m.r[6], m.r[7]);
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(8);
}
// 007dd3a0 (ESI = record, EDI = out): EAX = the matrix offset, ECX = the cookie, EDX = the world matrices.
RX_PORT(0x007dd3a0, Node_GetWorldPosition, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t rec = m.r[6];
    rx_world_node_get_world_position(rec, m.r[7]);
    const uint32_t idx = rec ? U8(rec, 0xb) : U8(0, 0xb);
    m.r[1] = U32(0, 0x009b45e4); m.r[2] = rec ? U32(U32(rec, 0), 0x1c) : U32(U32(0, 0), 0x18);
    return m.ret(0, idx << 6);
}
// 007dd320 (EDI = record, ESI = out): EAX = the world matrix, ECX = the set.
RX_PORT(0x007dd320, Node_GetWorldMatrix, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t rec = m.r[7];
    rx_world_node_get_world_matrix(rec, m.r[6]);
    m.r[1] = U32(rec, 0);
    return m.ret(0, U32(U32(rec, 0), 0x1c) + 64u * U8(rec, 0xb));
}
