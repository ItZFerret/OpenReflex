// phys_rider.cpp -- oracle adapters for core/game/phys_rider.c (the outputs the liveness table lists for the callers).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_rider.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

static rx_regs caller_regs(reflex_vm::Machine& m) {
    rx_regs r;
    std::memset(&r, 0, sizeof r);
    for (int i = 0; i < 8; i++) r.r[i] = m.r[i];
    return r;
}

RX_PORT(0x0042a240, Rider_ClearBoneFlags, "rx_phys", 0) { phys_rider_clear_bone_flags(m.arg(0)); return m.ret(4); }
// 0042aca0: EAX = the result, ECX = the mask sum (kept when inactive)
RX_PORT(0x0042aca0, Rider_UpFacing, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t ecx = m.r[1];
    m.r[0] = phys_rider_up_facing(m.arg(0), &ecx); m.r[1] = ecx; return m.ret(4);
}
// 00425800: EAX = A, ECX = 1, EDX = the last mask lane / sum
RX_PORT(0x00425800, Rider_RestTimer, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t A = m.arg(0);
    m.r[2] = phys_rider_rest_timer(A, rx_bf(m.arg(1))); m.r[0] = A; m.r[1] = 1; return m.ret(8);
}
// 00426930: EAX = the body count reached, EDX = Node_SetWorldPosition's
RX_PORT(0x00426930, Rider_PoseNodes, "rx_phys", OUT_EAX | OUT_EDX) {
    rx_regs c = caller_regs(m), o;
    phys_rider_pose_nodes(m.r[1], &c, &o);
    m.r[0] = o.r[0]; m.r[1] = o.r[1]; m.r[2] = o.r[2]; return m.ret(0);
}
// 00425920: ECX / EDX as left (00423df0's after a clamp)
RX_PORT(0x00425920, Rider_ClampSpeed, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t ecx = m.r[1], edx = m.r[2];
    phys_rider_clamp_speed(m.arg(0), &ecx, &edx); m.r[1] = ecx; m.r[2] = edx; return m.ret(4);
}
// 004252a0: ECX as the sound calls leave it
RX_PORT(0x004252a0, Rider_CrashSound, "rx_phys", OUT_ECX) {
    rx_regs r = caller_regs(m);
    for (int i = 0; i < 8; i++) r.xmm[i] = xmm(m, i);
    phys_rider_crash_sound(m.arg(0), &r);
    m.r[1] = r.r[1]; return m.ret(4);
}
// 0042a310: EDX as left, ECX = the cookie
RX_PORT(0x0042a310, Rider_WakeIfClear, "rx_phys", OUT_ECX | OUT_EDX) {
    rx_regs c = caller_regs(m);
    m.r[2] = phys_rider_wake_if_clear(m.r[1], &c); m.r[1] = rx_rd32(0x9b45e4); return m.ret(0);
}
// 00420160 (stdcall q; RET 4; ECX / EDX / ESI / EDI the outputs): no register output is read
RX_PORT(0x00420160, Rider_SwingTwist, "rx_phys", 0) {
    phys_rider_swing_twist(m.arg(0), m.r[1], m.r[2], m.r[6], m.r[7]); return m.ret(4);
}
// 00420770 (stdcall P; RET 4): no register output is read
RX_PORT(0x00420770, Rider_JointLimits, "rx_phys", 0) {
    rx_regs c = caller_regs(m);
    phys_rider_joint_limits(m.arg(0), &c); return m.ret(4);
}
// 004263a0 (ECX body, EDX point; stack A, n, depth; RET 0xc): no register output is read
RX_PORT(0x004263a0, Rider_ContactImpulse, "rx_phys", 0) {
    phys_rider_contact_impulse(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc);
}
// 00425b20 (stdcall A, dt; RET 8): no register output is read
RX_PORT(0x00425b20, Rider_GroundContacts, "rx_phys", 0) {
    rx_regs c = caller_regs(m);
    phys_rider_ground_contacts(m.arg(0), rx_bf(m.arg(1)), &c); return m.ret(8);
}
// 00429a40 (stdcall A, dt; RET 8): no register output is read
RX_PORT(0x00429a40, Rider_BoneContacts, "rx_phys", 0) { phys_rider_bone_contacts(m.arg(0), rx_bf(m.arg(1))); return m.ret(8); }
// 0042a6c0 (stdcall A; RET 4): no register output is read
RX_PORT(0x0042a6c0, Rider_Water, "rx_phys", 0) {
    rx_regs c = caller_regs(m);
    phys_rider_water(m.arg(0), &c); return m.ret(4);
}
// 00425640 (stdcall A, dt; RET 8) / 00425600 (ESI A; stdcall dt; RET 4): ECX as left
RX_PORT(0x00425640, Rider_CrashStep, "rx_phys", OUT_ECX) {
    rx_regs c = caller_regs(m);
    m.r[1] = phys_rider_crash_step(m.arg(0), rx_bf(m.arg(1)), &c); return m.ret(8);
}
RX_PORT(0x00425600, Rider_CrashUpdate, "rx_phys", OUT_ECX) {
    rx_regs c = caller_regs(m);
    m.r[1] = phys_rider_crash_update(m.r[6], rx_bf(m.arg(0)), &c); return m.ret(4);
}
