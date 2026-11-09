// phys_misc.cpp -- oracle adapters for core/game/phys_misc.c and phys_body.c.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_misc.h"
#include "../game/phys_body.h"
}
using namespace phys_oracle;
using namespace reflex_port;

// 0042d6b0 (EAX = filter, xmm2 = dt, arg target; RET 4): xmm0 = (new value, 0, 0, 0).
RX_PORT(0x0042d6b0, LagFilter_Update, "rx_phys", OUT_XMM0) {
    const float v = phys_lag_filter_update(m.r[0], xmm(m, 2).v[0], argf(m, 0));
    set_xmm(m, 0, rx_v4{{v, 0.0f, 0.0f, 0.0f}});
    return m.ret(4);
}
RX_PORT(0x0042d670, LagFilter_Set, "rx_phys", 0) { phys_lag_filter_set(m.r[1], xmm(m, 1).v[0], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0042f1d0, Matrix6x6_Construct, "rx_phys", OUT_ECX | OUT_EDX) {
    phys_matrix6x6_construct(m.r[0]); m.r[1] = 3; m.r[2] = 0x0093ca04; return m.ret(0);
}
RX_PORT(0x0042f230, Matrix4x6_Construct, "rx_phys", OUT_ECX | OUT_EDX) {
    phys_matrix4x6_construct(m.r[0]); m.r[1] = 4; m.r[2] = 0x0093ca4c; return m.ret(0);
}
RX_PORT(0x0042f2a0, Matrix6x4_Construct, "rx_phys", OUT_ECX | OUT_EDX) {
    phys_matrix6x4_construct(m.r[0]); m.r[1] = 4; m.r[2] = 0x0093ca28; return m.ret(0);
}
RX_PORT(0x0042e5f0, Pose_Clear, "rx_phys", OUT_EAX) { m.r[0] = phys_pose_clear(m.r[1]); return m.ret(0); }
RX_PORT(0x0042ef70, SimulationClock_Advance, "rx_phys", 0) { phys_simulation_clock_advance(argf(m, 0)); return m.ret(0); }

// 0042bd90 (EAX = handle): EAX = the record; ECX = end - begin and EDX = the quotient register as the original's division
// leaves them.
RX_PORT(0x0042bd90, Body_Record, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t vec = uint32_t(m.mem.read(m.r[0] + 4, 4)) + 0x1954;
    const int32_t bytes = int32_t(uint32_t(m.mem.read(vec + 0x10, 4)) - uint32_t(m.mem.read(vec + 0xc, 4)));
    m.r[0] = phys_body_record(m.r[0]);
    m.r[1] = uint32_t(bytes);
    m.r[2] = uint32_t(int32_t((int64_t(bytes) * int64_t(int32_t(0x92492493))) >> 32) + bytes) >> 7 | 0;
    m.r[2] = uint32_t((int32_t((int64_t(bytes) * int64_t(int32_t(0x92492493))) >> 32) + bytes) >> 7);
    return m.ret(0);
}
RX_PORT(0x0042b000, Body_UpdateGrid, "rx_phys", 0) { phys_body_update_grid(m.r[6], int32_t(m.r[1])); return m.ret(0); }
RX_PORT(0x00429910, Rig_BodiesFromBones, "rx_phys", 0) { phys_rig_bodies_from_bones(m.r[1]); return m.ret(0); }
extern "C" {
#include "../game/phys_rig.h"
}
// 00426a50 (EDI = rider animation): EDX as the original leaves it (the next position slot, or bone 0's node model).
RX_PORT(0x00426a50, Rig_CaptureBonePoses, "rx_phys", OUT_EDX) {
    const uint32_t rig = m.r[7];
    phys_rig_capture_bone_poses(rig);
    const int32_t count = int32_t(m.mem.read(rig + 0x3fc, 4));
    if (count > 1) m.r[2] = rig + 0x400 + 0x80u * uint32_t(count);
    else {
        const uint32_t nodes = uint32_t(m.mem.read(uint32_t(m.mem.read(uint32_t(m.mem.read(rig + 0x37c, 4)) + 0x240, 4)) + 8, 4));
        m.r[2] = uint32_t(m.mem.read((uint32_t(m.mem.read(rig + 0xa540, 4)) << 4) + nodes, 4));
    }
    return m.ret(0);
}
// 00424370 (xmm0, xmm1, cdecl value, out_lo, out_hi): xmm0 = (result, 0, 0, 0).
RX_PORT(0x00424370, RemapClamped, "rx_phys", OUT_XMM0) {
    const float r = phys_remap_clamped(xmm(m, 0).v[0], xmm(m, 1).v[0], argf(m, 0), argf(m, 1), argf(m, 2));
    set_xmm(m, 0, rx_v4{{r, 0.0f, 0.0f, 0.0f}});
    return m.ret(0);
}
