// world_node_water.cpp -- oracle adapters for core/game/world_node_water.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_node.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x007e9dc0, FUN_007e9dc0, "rx_world", OUT_EAX) {
    return m.ret(0x14, rx_world_water_patch_height(m.arg(0), m.arg(1), m.arg(2), f32(m.arg(3)), f32(m.arg(4))));
}
RX_PORT(0x007efef0, FUN_007efef0, "rx_world", OUT_EAX) { return m.ret(4, rx_world_water_set_height(m.r[6], m.arg(0))); }
RX_PORT(0x007f8e00, PackedGrid_SampleUV, "rx_world", OUT_EAX) {
    const uint32_t saved[4] = {m.r[3], m.r[5], m.r[6], m.r[7]};
    return m.ret(8, rx_world_packed_grid_sample(m.r[1], m.r[0], f32(m.arg(0)), f32(m.arg(1)), saved));
}

RX_PORT(0x007f8d10, PackedGrid_InterleavedSampleUV, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t grid = m.r[6], layer = m.arg(2);
    const uint32_t saved[3] = {m.r[3], m.r[5], m.r[7]};
    rx_v4 xmm0;
    m.r[0] = rx_world_packed_grid_interleaved(grid, f32(m.arg(0)), f32(m.arg(1)), layer, saved, &xmm0);
    std::memcpy(&m.xmm[0], &xmm0, sizeof xmm0);
    m.r[1] = 32 - rx_rd32(grid + 0x68 + 4 * layer);
    m.r[2] = 0xffffffffu >> (m.r[1] & 31);
    return m.ret(12);
}
