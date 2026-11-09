/* phys_rig.c -- the rider rig's physics side (0x0042xxxx): the rider animation object R (RiderAnimation, EDI / ECX in the
 * originals) keeps per-bone poses for the rider's physics bodies:
 *   +0x37c skeleton instance (+0x240 -> node array at +8, 16-byte node records: +0 model (+0x1c world matrices, 64 bytes
 *          each), +0xb matrix index)
 *   +0x3fc bone count, +0xa540 + 4 i node index of bone i
 *   +0x400 + 0x80 i bone position (bone 0: world position; bones >= 1: a reference point transformed by the bone matrix)
 *   +0xe80 + 0x70 i bone rotation (quaternion)
 *   +0x380 + 0x1974 + 4 (i - 1) -> +0x3d6c (stride 0x230) -> +0x3d64 (stride 0x230) -> +0x1ee0 (stride 0x170): per-bone
 *          reference offset (from the rig description) */
#include "phys_rig.h"
#include "world_node.h"
#include "world_veh.h"
#include "world_misc.h"

/* Node_EvaluateWorld 007db890 (thiscall node, subtree flag) and Matrix3_ToQuaternionToEsi 00401740 (ESI = out, cdecl
 * matrix*): the world subsystem's core functions, called with the original's pushed arguments */
static void node_evaluate_world(rx_addr node) {
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, 0);                                        /* the subtree flag pushed */
    rx_set_sp(sp - 8);
    rx_world_node_evaluate_world(node, 0);
    rx_set_sp(sp);
}
static void matrix_to_quaternion(rx_addr out, rx_addr matrix) {
    const rx_addr sp = rx_sp();
    SU32(sp - 4, 0, matrix);                                   /* cdecl: the matrix pointer pushed */
    rx_set_sp(sp - 8);
    rx_world_matrix3_to_quaternion_to(out, matrix);
    rx_set_sp(sp);
}

/* 00426a50 (EDI = R; RiderAnimation::vf19): evaluates every bone's world node and stores its pose: bone 0 the world
 * position (x, y, z, z) at +0x400 and the rotation at +0xe80; bone i >= 1 the reference point (runtime zero vec4 0x00d61670
 * minus the bone's offset) transformed by the bone's world matrix at +0x400 + 0x80 i (w = bone 0's z: the original's stack
 * vector keeps it), the rotation at +0xe80 + 0x70 i. The matrix and the quaternion go through the stack (the callee reads /
 * writes them there): frame push ebp; and esp, -16; sub esp, 0xa8; push ebx; push esi. */
void phys_rig_capture_bone_poses(rx_addr rig) {
    const rx_addr sp0 = rx_sp(), B = ((sp0 - 4) & ~15u) - 0xa8 - 8;
    const rx_addr matrix = B + 0x40, quat = B + 0x80;
    rx_set_sp(B);
    float w_lane = 0.0f;
    for (int32_t i = 0; i == 0 || i < I32(rig, 0x3fc); i++) {
        const rx_addr nodes = U32(U32(U32(rig, 0x37c), 0x240), 8);
        const rx_addr node = (U32(rig, 0xa540 + 4 * i) << 4) + nodes;
        node_evaluate_world(node);
        const rx_addr world = ((uint32_t)U8(node, 0xb) << 6) + U32(U32(node, 0), 0x1c);
        for (uint32_t row = 0; row < 0x40; row += 0x10) v4_st(matrix + row, v4_ld(world + row));
        matrix_to_quaternion(quat, matrix);
        const rx_v4 rot = v4_ld(quat);
        if (i == 0) {
            w_lane = F32(matrix, 0x38);
            v4_st(rig + 0x400, v4(F32(matrix, 0x30), F32(matrix, 0x34), w_lane, w_lane));
            v4_st(rig + 0xe80, rot);
            continue;
        }
        uint32_t k = U32(rig + 0x380 + 0x1974 + 4u * (uint32_t)(i - 1), 0) * 0x230;
        k = U32(rig + 0x3d6c, k) * 0x230;
        k = U32(rig + 0x3d64, k) * 0x170;
        const rx_v4 d = v4_sub(v4_ld(0x00d61670), v4_ld(rig + 0x1ee0 + k));
        const float m00 = F32(matrix, 0x00), m01 = F32(matrix, 0x04), m02 = F32(matrix, 0x08);
        const float m10 = F32(matrix, 0x10), m11 = F32(matrix, 0x14), m12 = F32(matrix, 0x18);
        const float m20 = F32(matrix, 0x20), m21 = F32(matrix, 0x24), m22 = F32(matrix, 0x28);
        const float m30 = F32(matrix, 0x30), m31 = F32(matrix, 0x34), m32 = F32(matrix, 0x38);
        const float x = ((d.v[1] * m10 + m00 * d.v[0]) + m20 * d.v[2]) + m30;
        const float y = ((m11 * d.v[1] + m01 * d.v[0]) + m21 * d.v[2]) + m31;
        const float z = ((m12 * d.v[1] + m02 * d.v[0]) + m22 * d.v[2]) + m32;
        v4_st(rig + 0x400 + 0x80u * (uint32_t)i, v4(x, y, z, w_lane));
        v4_st(rig + 0xe80 + 0x70u * (uint32_t)i, rot);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00426a50, phys_rig_capture_bone_poses)

/* 004296d0 (EDX = R, ESI = node index): the bone whose node is `node` (R+0xa540[i]), or -1. */
int32_t phys_rig_bone_of_node(rx_addr rig, uint32_t node) {
    for (int32_t i = 0; i < I32(rig, 0x3fc); i++)
        if (U32(rig, 0xa540 + 4 * i) == node) return i;
    return -1;
}
RX_FUNCTION(0x004296d0, phys_rig_bone_of_node)

/* 0042ade0 (stack R; RET 4): when the player (R+0x9c) has no +0x10 override, the rider's speed along the axis R+0xa840
 * (dot with body 0's velocity R+0x410), splatted into a vec4 on the stack, goes to the active camera entity's vfunc 34
 * (Player_ActiveCameraEntity 0x0054c020, EDI = player; (vec4*, 0.25f)). Frame: push ebp; and esp, -16; sub esp, 0x2c;
 * push edi; the vector at +0x10 above that. */
void phys_rig_camera_speed(rx_addr rig) {
    const rx_addr player = U32(rig, 0x9c);
    if (U32(player, 0x10) != 0) return;
    const rx_addr sp0 = rx_sp(), A = ((sp0 - 4) & ~15u) - 0x2c - 4, speed = A + 0x10;
    rx_set_sp(A);
    v4_st(speed, v4_splat(phys_dot3(v4_ld(rig + 0xa840), v4_ld(rig + 0x410))));
    rx_addr camera;
    PHYS_CALL_SP(0, camera = rx_world_player_active_camera_entity(player));   /* 0054c020: EDI = player */
    const uint32_t args[2] = {speed, 0x3e800000u};          /* 0.25f (0092ff84, through fld / fstp) */
    rx_call_original(U32(U32(camera, 0), 0x88), RX_THISCALL, camera, 2, args);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0042ade0, phys_rig_camera_speed)
