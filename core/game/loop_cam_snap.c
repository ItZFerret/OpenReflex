/* loop_cam_snap.c -- the camera cut-in snap 00649df0 of the player's camera switch (Player_UpdateCamera 0054c150).
 *
 * The active camera entity changes from a blendable camera (the start-gate camera, class 0x100d) to an MXThirdPersonCamera
 * (class 0x1003): the old camera's pose is handed to the new one as the starting point of its cut-in transition (the fields
 * MXThirdPersonCamera::vf19 0064c110 blends from while +0x3f4 is set: the time +0x3c0, the duration +0x3c4, the elapsed time
 * +0x3c8, the fraction +0x3cc, the position +0x3d0..+0x3d8, the look point +0x3e0..+0x3e8 and the fov +0x3f0). The pose comes from
 * vf14 (slot 0x38, Entity::vf14 0088fa20 = entity + 0x30: a 4x4 matrix whose +0x10 row is up, +0x20 forward, +0x30 position).
 *
 * Notes: core/notes/loop.md. */
#include "loop_cam_snap.h"
#include "loop_cam.h"
#include "loop_util.h"
#include "../src/rx_x86.h"

/* 00649df0 (LTCG: ESI = the new camera entity, EDI = the old camera entity; stack `duration` float; ret 4):
 *   [esi+0x3f4] = 1;
 *   m1 = old->vf14();                       (called twice: the target may return a scratch matrix)
 *   m2 = old->vf14();
 *   +0x3c8 / +0x3cc / +0x3c0 = 0; +0x3c4 = duration;
 *   +0x3d0..+0x3d8 = m1 +0x30..+0x38 (position);
 *   +0x3e0..+0x3e8 = m2 +0x20..+0x28 (forward) + m1 +0x30..+0x38;
 *   +0x3f0 = (x87 copy) [old+0xe0] (fov).
 * Also: [esi+0x3c4] gets the caller's stack float through `movss` (no x87); the additions are addss with m2 as the first operand
 * for x / z and m1 (the stack copy) for y, which matters only for NaN payloads (rx_adds).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x10; the first call's position row is kept in its +0 / +4 / +8 slots. */
void rx_cam_snap(rx_addr cam, rx_addr old, float duration) {
    const rx_addr sp0 = rx_sp(), S = ((sp0 - 4u) & ~0xfu) - 0x10u;
    rx_set_sp(S);
    SU32(cam, 0x3f4, 1);
    rx_addr m = loop_tc0r(loop_vslot(old, 0x38), old, 0x00649e0cu);
    SU32(S, 0, U32(m, 0x30));
    SU32(S, 4, U32(m, 0x34));
    const uint32_t z = U32(m, 0x38);
    m = loop_tc0r(loop_vslot(old, 0x38), old, 0x00649e35u);
    SU32(S, 8, z);
    SU32(cam, 0x3c8, 0);
    SU32(cam, 0x3cc, 0);
    SU32(cam, 0x3c0, 0);
    SF32(cam, 0x3c4, duration);
    SF32(cam, 0x3d4, F32(S, 4));
    SF32(cam, 0x3d0, F32(S, 0));
    SF32(cam, 0x3d8, F32(S, 8));
    SF32(cam, 0x3e0, rx_adds(F32(m, 0x20), F32(S, 0)));
    SF32(cam, 0x3e4, rx_adds(F32(S, 4), F32(m, 0x24)));
    SF32(cam, 0x3e8, rx_adds(F32(m, 0x28), F32(S, 8)));
    cam_x87_copy(cam + 0x3f0, old + 0xe0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00649df0, rx_cam_snap)
