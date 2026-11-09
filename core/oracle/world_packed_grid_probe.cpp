/* Synthetic packed-grid boundary stimuli; original calls exercise the rx_world adapter.
 * RX_PACKED_GRID_PROBE=1 REFLEX_PORT_VERIFY=1
 * REFLEX_PORT=rx_world,rx_PackedGridProbeHook (a normal race world, >=5 frames).
 * No assets change: allocate private descriptors and patterned words. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
namespace {
unsigned frame;
uint32_t alloc(uint32_t size) {
    const uint32_t args[] = {size};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, args);
    for (uint32_t i = 0; i < size; i += 4) rx_wr32(p + i, 0);
    return p;
}
}
RX_PORT(0x0084ed20, PackedGridProbeHook, "thunk", 0) {
    if (!std::getenv("RX_PACKED_GRID_PROBE") || ++frame != 5) return reflex_vm::RUN_ORIGINAL;
    uint32_t regs[8]; reflex_vm::Xmm xmm[8];
    std::memcpy(regs, m.r, sizeof regs); std::memcpy(xmm, m.xmm, sizeof xmm);
    const uint32_t flags = m.flags; auto fpu = m.fpu;
    const uint32_t grid = alloc(0x100), data = alloc(2048);
    rx_wr32(grid + 0x48, rx_oracle::bits(8.0f));
    rx_wr32(grid + 0x38, rx_oracle::bits(9.0f));
    rx_wr32(grid + 0x88, data);
    for (uint32_t i = 0; i < 512; ++i) rx_wr32(data + 4*i, 0x9e3779b9u * (i + 1));
    unsigned count = 0;
    const unsigned totals[] = {1, 5, 8, 13, 16, 31, 32};
    const float us[] = {-0.125f, 0.0f, 0.125f, 0.49999997f, 0.5f, 0.99999994f, 1.0f, 1.125f};
    for (unsigned total : totals) {
        rx_wr32(grid + 0x14, total);
        for (unsigned layer = 0; layer < 4; ++layer) {
            const unsigned widths[] = {total, 1, total > 5 ? 5u : total, 0};
            rx_wr32(grid + 0x68 + 4*layer, widths[layer]);
            rx_wr32(grid + 0x78 + 4*layer, layer == 2 ? total - widths[layer] : 0);
        }
        for (unsigned layer = 0; layer < 4; ++layer) for (float u : us) for (unsigned j = 0; j < 4; ++j) {
            const float v = u < 0 ? 2.0f : float(j) * 0.125f;
            rx_regs in{}, out{};
            in.r[6] = grid; in.r[1] = 0x12345678; // ECX must be ignored.
            in.r[3] = 0x24681357; in.r[5] = 0x13572468; in.r[7] = 0x10203040;
            const uint32_t args[] = {rx_oracle::bits(u), rx_oracle::bits(v), layer};
            rx_call_original_ex(0x007f8d10, &in, &out, 3, args); ++count;
        }
    }
    // Also enter through TerrainMaterialAt: cover the replaced interleaved caller,
    // busy-flag bracket and full sampler XMM0 propagation (including negative u).
    const uint32_t terrain = alloc(0x4500), materials = alloc(0x60), point = alloc(16);
    rx_wr32(terrain + 0x44ac, materials); rx_wr32(materials + 0x44, grid);
    rx_wr32(terrain + 0x44d0, rx_oracle::bits(2.0f));
    rx_wr32(terrain + 0x44d4, rx_oracle::bits(2.0f));
    rx_wr32(grid + 0x10, 1);
    unsigned terrain_count = 0;
    for (unsigned layer = 0; layer < 4; ++layer) for (float u : us) {
        const float v = u < 0 ? 2.0f : 0.25f;
        rx_wr32(point, rx_oracle::bits(2.0f*u - 1.0f));
        rx_wr32(point + 8, rx_oracle::bits(1.0f - 2.0f*v));
        rx_regs in{}, out{};
        in.r[0] = layer; in.r[2] = point; in.r[7] = terrain;
        in.r[3] = 0x24681357; in.r[5] = 0x13572468; in.r[6] = 0x10203040;
        rx_call_original_ex(0x007c5120, &in, &out, 0, nullptr); ++terrain_count;
    }
    std::memcpy(m.r, regs, sizeof regs); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu = fpu;
    std::fprintf(stderr, "[packed-grid-probe] %u calls: total widths 1/5/8/13/16/31/32, layer widths 0/1/5/full, cross-word and unclamped UV boundaries\n", count);
    std::fprintf(stderr, "[packed-grid-probe] %u TerrainMaterialAt interleaved caller cases\n", terrain_count);
    return reflex_vm::RUN_ORIGINAL;
}
