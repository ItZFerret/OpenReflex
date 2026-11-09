// phys_rig.cpp -- oracle adapters for core/game/phys_rigbody.c (the rider rig's bodies; LTCG register conventions).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_rigbody.h"
#include "../game/phys_rig.h"
}
using namespace phys_oracle;
using namespace reflex_port;

static int32_t count_of(reflex_vm::Machine& m, uint32_t P) { return int32_t(m.mem.read(P + 0x7c, 4)); }

// 00422550 (EDI = P): EAX / ECX / EDX = the loop's pointers as the original leaves them (unchanged without bodies).
RX_PORT(0x00422550, RigBody_SaveVelocities, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.r[7]; phys_rigbody_save_velocities(P);
    const int32_t n = count_of(m, P);
    if (n > 0) { m.r[0] = P + 0x1460 + 0x40u * n; m.r[2] = P + 0xb10 + 0x70u * n; m.r[1] = P + 0x90 + 0x80u * n; }
    return m.ret(0);
}
RX_PORT(0x00422590, RigBody_RestoreVelocities, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.r[7]; phys_rigbody_restore_velocities(P);
    const int32_t n = count_of(m, P);
    if (n > 0) { m.r[0] = P + 0x1460 + 0x40u * n; m.r[2] = P + 0xb10 + 0x70u * n; m.r[1] = P + 0x90 + 0x80u * n; }
    return m.ret(0);
}
RX_PORT(0x00422800, RigBody_ClearForces, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.r[6]; phys_rigbody_clear_forces(P);
    const int32_t n = count_of(m, P);
    m.r[2] = uint32_t(n > 0 ? n : 0);
    if (n > 0) { m.r[1] = P + 0xb50 + 0x70u * n; m.r[0] = P + 0xd0 + 0x80u * n; }
    return m.ret(0);
}
RX_PORT(0x00422300, RigBody_CenterOfMass, "rx_phys", OUT_EDX) {
    const uint32_t P = m.r[6]; phys_rigbody_center_of_mass(P, m.r[1]);
    const int32_t n = count_of(m, P); m.r[2] = uint32_t(n > 0 ? n : 0);
    if (n > 0) m.r[0] = P + 0xa0 + 0x80u * n;
    return m.ret(0);
}
RX_PORT(0x00422350, RigBody_MeanVelocity, "rx_phys", OUT_EDX) {
    const uint32_t P = m.r[6]; phys_rigbody_mean_velocity(P, m.r[1]);
    const int32_t n = count_of(m, P); m.r[2] = uint32_t(n > 0 ? n : 0);
    if (n > 0) m.r[0] = P + 0xa0 + 0x80u * n;
    return m.ret(0);
}
// 004223a0 (ESI = P, EDI = axis; out*, point*; RET 8): EAX = out.
RX_PORT(0x004223a0, RigBody_AxisInertia, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t P = m.r[6]; phys_rigbody_axis_inertia(P, m.r[7], m.arg(0), m.arg(1));
    const int32_t n = count_of(m, P); m.r[2] = uint32_t(n > 0 ? n : 0); m.r[0] = m.arg(0);
    if (n > 0) m.r[1] = P + 0xa0 + 0x80u * n;
    return m.ret(8);
}
// 00422180 (EDX = P, ESI = body, EDI = impulse*; lever*; RET 4): EAX / ECX as the original's address arithmetic leaves them.
RX_PORT(0x00422180, RigBody_ApplyImpulse, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.r[2], b = m.r[6];
    phys_rigbody_apply_impulse(P, int32_t(b), m.r[7], m.arg(0));
    m.r[0] = (b + 0x1a) * 0x70 + P; m.r[1] = b * 0x70 + P;
    return m.ret(4);
}
// 00422700 (ECX = P; body, torque*; RET 8): EAX = P + 0x70 body, ECX = torque*.
RX_PORT(0x00422700, RigBody_AddTorque, "rx_phys", OUT_EAX | OUT_ECX) {
    const uint32_t P = m.r[1], b = m.arg(0);
    phys_rigbody_add_torque(P, int32_t(b), m.arg(1));
    m.r[0] = b * 0x70 + P; m.r[1] = m.arg(1);
    return m.ret(8);
}
// 004241f0 (ECX = v*, EDX = P; body; RET 4): EAX = P + 0x70 body.
RX_PORT(0x004241f0, RigBody_AddAngularVelocity, "rx_phys", OUT_EAX) {
    const uint32_t P = m.r[2], b = m.arg(0);
    phys_rigbody_add_angular_velocity(m.r[1], P, int32_t(b));
    m.r[0] = b * 0x70 + P;
    return m.ret(4);
}
// 004225d0 (EDX = out*; P; RET 4): EAX = P, ECX = the root's parent index (or its record offset when it has one).
RX_PORT(0x004225d0, RigBody_ReferencePoint, "rx_phys", OUT_EAX | OUT_ECX) {
    const uint32_t P = m.arg(0);
    phys_rigbody_reference_point(m.r[2], P);
    const uint32_t parent = uint32_t(m.mem.read(P + 0x39ec + uint32_t(m.mem.read(P + 0x1970, 4)) * 0x230, 4));
    m.r[0] = P;
    m.r[1] = parent == 0xfffffffbu ? parent : uint32_t(m.mem.read(P + 0x39e4 + parent * 0x230, 4)) * 0x170;
    return m.ret(4);
}
RX_PORT(0x00423980, RigBody_Damp, "rx_phys", 0) { phys_rigbody_damp(m.arg(0), xmm(m, 0).v[0]); return m.ret(4); }
// 004240e0 (ECX = body, ESI = P; scale; RET 4): ECX = the angular velocity's address, EDX = the new draw count.
RX_PORT(0x004240e0, RigBody_RandomSpin, "rx_phys", OUT_ECX | OUT_EDX) {
    const uint32_t P = m.r[6], b = m.r[1];
    phys_rigbody_random_spin(P, int32_t(b), argf(m, 0));
    m.r[1] = b * 0x70 + P + 0xb10; m.r[2] = uint32_t(m.mem.read(0x009cb864, 4));
    m.r[0] = (uint32_t(m.mem.read(0x009cb860, 4)) >> 16) & 0x7fff;
    return m.ret(4);
}
// 004296d0 (EDX = rig, ESI = node): EAX = the bone or -1; ECX = the bone count; EDX = the scan pointer.
RX_PORT(0x004296d0, Rig_BoneOfNode, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t rig = m.r[2];
    const int32_t r = phys_rig_bone_of_node(rig, m.r[6]);
    const int32_t n = int32_t(m.mem.read(rig + 0x3fc, 4));
    m.r[0] = uint32_t(r); m.r[1] = uint32_t(n);
    if (n > 0) m.r[2] = rig + 0xa540 + 4u * uint32_t(r >= 0 ? r : n);
    return m.ret(0);
}
RX_PORT(0x0042ade0, Rig_CameraSpeed, "rx_phys", 0) { phys_rig_camera_speed(m.arg(0)); return m.ret(4); }
// 00422450 (EDX P, ESI v): ECX = the body count, EAX = past the last angular record
RX_PORT(0x00422450, RigBody_AddWorldAngularVelocity, "rx_phys", OUT_ECX) {
    const uint32_t P = m.r[2]; phys_rigbody_add_world_angular_velocity(P, m.r[6]);
    const int32_t n = count_of(m, P);
    if (n > 0) { m.r[1] = uint32_t(n); m.r[0] = P + 0xb10 + 0x70u * n; } else m.r[1] = 0;
    return m.ret(0);
}
RX_PORT(0x00423a50, RigBody_Integrate, "rx_phys", 0) { phys_rigbody_integrate(m.r[6], xmm(m, 0).v[0]); return m.ret(0); }
