/* race_tag_ball.c -- the Tag ball's physics helpers of MXActivityTag (the "TagBall" scene entity, a PhysicsEntity at +0xff090) and the
 * network holder handover, hand-transliterated from the original (each function's address is in its comment).
 *
 * The ball's body: the entity's physics handle +0x158 (+0x154 + 4), its kinematic body (PhysicsHandle_Body 00418f30); the body's slot
 * +0x80 indexes the world's dynamic records (0x50 bytes, the velocity at +0x1d10 + 0x50 * slot of the world, the impulse accumulator at
 * +0x1d20, the second accumulator at +0x5b90). */
#include "race_tag.h"
#include "race_extern.h"
#include "race_misc.h"
#include "race.h"
#include "build_activity_int.h"
#include "world_veh.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "vehicle_atv_wheels.h"
#include "vehicle_water_wake.h"
#include "loop_cam_parts_view.h"
#include "crt.h"

#define BALL(A)    U32((A), 0xff090)
#define BALL_PH(A) U32(BALL(A) + 0x154, 4)

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static float x87_copy(float v) {
    uint32_t b = rx_fb(v);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) b |= 0x400000u;
    return rx_bf(b);
}
/* the ball's velocity (the world's record of its body; the zero vector 0xd61670 without a body or a record) */
static void ball_velocity(rx_addr act, rx_addr out) {
    rx_addr src = 0x00d61670;
    if (BALL_PH(act) != 0) {
        const rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
        const int32_t slot = I32(body, 0x80);
        if (slot >= 0) src = U32(BALL_PH(act), 4) + ((uint32_t)slot * 5u + 0x1d1u) * 0x10u;
    }
    for (uint32_t k = 0; k < 16; k += 4) SU32(out, k, U32(src, k));
}
/* the activity scene's terrain (0042d960 over the registry 0x9e30d4, initialised under the guard bit 0x9e30e0) */
static rx_addr act_terrain(rx_addr act) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    return rx_terrain_for_scene(0x009e30d4, U32(U32(act, 0x234), 0x20138));
}

/* 00770d00 (ECX = the physics world; stdcall body; RET 4): the body's velocity record and its second accumulator (+0x5b90 + 0x50 *
 * slot) cleared to the zero vector (0xd61670), for a body with a record (+0x80 >= 0). Frame: push ebp; mov ebp, esp; and esp, -16. */
void rx_world_body_velocity_clear(rx_addr world, rx_addr body) {
    const int32_t slot = I32(body, 0x80);
    if (slot < 0) return;
    for (uint32_t k = 0; k < 16; k += 4) {
        SU32(world + ((uint32_t)slot * 5u + 0x1d1u) * 0x10u, k, U32(0, 0x00d61670 + k));
        SU32(world + ((uint32_t)slot * 5u + 0x5b9u) * 0x10u, k, U32(0, 0x00d61670 + k));
    }
}
RX_FUNCTION(0x00770d00, rx_world_body_velocity_clear)

/* 0056e320 (stdcall act, impulse; RET 8): the impulse (16 bytes) added to the ball body's accumulator (+0x1d20 + 0x50 * slot of the
 * world, addps), and the reset clock [0x9c5198] = 30 s (0x931d98). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc; push esi. */
void rx_tag_ball_impulse(rx_addr act, rx_addr impulse) {
    if (BALL_PH(act) != 0) {
        const rx_addr world = U32(BALL_PH(act), 4);
        const rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
        const int32_t slot = I32(body, 0x80);
        if (slot >= 0) {
            const rx_addr acc = world + (uint32_t)slot * 0x50u + 0x1d20u;
            for (uint32_t k = 0; k < 16; k += 4) SF32(acc, k, F32(acc, k) + F32(impulse, k));
        }
    }
    SU32(0, 0x009c5198, 0x41f00000u);   /* 30.0 */
}
RX_FUNCTION(0x0056e320, rx_tag_ball_impulse)

/* 0056f210 (EAX = act): the ball's visibility (+0xa2 of the entity). A local human holder in first person (+0x230) hides it at once;
 * with nobody holding it (or a holder that is not a local human) it shows again after 3 frames (+0xff3a8). */
void rx_tag_ball_visible(rx_addr act) {
    const rx_addr holder = U32(act, 0xff078);
    int show = 1;
    if (holder != 0) {
        const rx_addr p = U32(holder, 0x74);
        if (p == 0 || U32(p, 4) == 0 || U32(p, 0x10) != 0) show = -1;   /* no change */
        else if (U32(p, 0x230) != 0) { SU32(act, 0xff3a4, 0); show = -1; }
    }
    if (show == 1 && U32(act, 0xff3a4) == 0) { SU32(act, 0xff3a8, 3); SU32(act, 0xff3a4, 1); }
    if (U32(act, 0xff3a8) == 0) SU8(BALL(act), 0xa2, U8(act, 0xff3a4));
    else SU32(act, 0xff3a8, U32(act, 0xff3a8) - 1);
}
RX_FUNCTION(0x0056f210, rx_tag_ball_visible)

/* 00570fc0 (stdcall act; RET 4) -> xmm0 lane 0: the first word of the ball body's contact record (+0x28 + 4 -> +0x20) when the record's
 * kind (+8) is 2, else 1.0. Frame: push ebp; mov ebp, esp; and esp, -16. */
float rx_tag_ball_contact_value(rx_addr act) {
    const rx_addr body = rx_world_physics_handle_body(U32(BALL(act), 0x158));
    const rx_addr rec = U32(body + 0x28, 4);
    if (I32(rec, 8) == 2) return F32(rec, 0x20);
    return 1.0f;   /* 0x931f24 */
}
RX_FUNCTION(0x00570fc0, rx_tag_ball_contact_value)

/* 0056f290 (stdcall act, dt; RET 8): the ball and the water. The fourth water kick (+0xff370 == 4) puts the ball back at its round start
 * (physics off, vf28, on) and the reset clock to 30 s. Else the ball's speed (its velocity's length, SqrtF); with the physics on (a
 * network ball valid) the ball's position (vf17) probed against the water (00424340's set, 007f0050): out of the water the kicks, the
 * splash flag and the water timer cleared. In the water (the probe's surface F + 0xa4 above the ball less 00570fc0's value): not rising,
 * the buoyancy impulse; the first splash (0.03, 8 ft: 007efed0); over more than 3 ft of water, sinking or rising slower than 5 ft/s,
 * the wake quad (007efe90); after WaterKickTime ([0x9c5194]) in the water the kick toward the start entity ((kicks x 500 + 10000) on
 * x / z, 10000 up), the big splash (-10) and the kick counted.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xb4; push ebx / esi / edi (F); the velocity F + 0xa0, the speed F + 0x38, the
 * ball's position F + 0x20 (copied to F + 0xa0 and F + 0x40), the probe's normal F + 0x10. */
void rx_tag_ball_water(rx_addr act, float dt) {
    ACT_FRAME_EBP(F, 16, 0xb4, 3);
    if (U32(act, 0xff370) == 4) {
        SU32(act, 0xff370, 0);
        ACT_CALL(2, rx_tag_ball_physics(act, 0));
        { const uint32_t a[2] = {act + 0xff0a0, act + 0xff0b0}; (void)act_vcall(BALL(act), 0x70, 2, a); }
        ACT_CALL(2, rx_tag_ball_physics(act, 1));
        SU32(0, 0x009c5198, 0x41f00000u);   /* 30.0, 0x931d98 */
        ACT_END(F);
        return;
    }
    const rx_addr terrain = act_terrain(act);
    (void)terrain;
    ball_velocity(act, F + 0xa0);
    const float v0 = F32(F, 0xa0), v1 = F32(F, 0xa4), v2 = F32(F, 0xa8);
    float speed;
    ACT_CALLR(speed, 1, phys_sqrtf(v0 * v0 + v1 * v1 + v2 * v2));
    SF32(F, 0x38, speed);
    if (U32(act, 0xff388) == 0 || (U32(act, 0xff380) != 0 && (rx_net_ref_valid(U32(act, 0xff380)) & 0xff) == 0)) { ACT_END(F); return; }
    { const uint32_t a[1] = {F + 0x20}; (void)act_vcall(BALL(act), 0x44, 1, a); }
    for (uint32_t k = 0; k < 12; k += 4) { SU32(F + 0xa0, k, U32(F + 0x20, k)); SU32(F + 0x40, k, U32(F + 0x20, k)); }
    rx_addr water;
    rx_set_sp(F - 8);
    ACT_CALLR(water, 0, rx_atv_water_singleton());
    uint32_t hit;
    ACT_CALLR(hit, 0, rx_scene_ground_probe(water, F + 0xa0, F + 0x10));
    rx_set_sp(F);
    if (hit == 0) {
        SU32(act, 0xff370, 0);
        SU32(act, 0xff368, 0);
        SF32(act, 0xff364, 0.0f);
        ACT_END(F);
        return;
    }
    /* in the water: the depth below the surface the probe left at F + 0xa4 (the ball's height less the contact value 00570fc0) */
    float cv;
    ACT_CALLR(cv, 1, rx_tag_ball_contact_value(act));
    const float depth = F32(F, 0xa4) - (F32(F, 0x24) - cv);
    SF32(F, 0x3c, depth);
    if (!(depth > 0.0f)) { ACT_END(F); return; }
    ball_velocity(act, F + 0x90);
    if (F32(F, 0x94) <= 0.0f) {   /* comiss 0, vy; jb: not rising (NaN skips): the buoyancy (0x9cb4b0 * depth * 6) */
        const float k = 6.0f;   /* 0x92fe4c */
        SF32(F, 0x10, F32(0, 0x009cb4b0) * depth * k);
        SF32(F, 0x14, F32(0, 0x009cb4b4) * depth * k);
        SF32(F, 0x18, F32(0, 0x009cb4b8) * depth * k);
        ACT_CALL(2, rx_tag_ball_impulse(act, F + 0x10));
    }
    ball_velocity(act, F + 0x90);
    if (U32(act, 0xff368) == 0) {   /* the splash, once (strength 0.03 0x9cf5a8 through x87, size 8 0x9322b8) */
        SF32(F, -4, x87_copy(rx_bf(0x3cf5c28fu)));
        rx_addr set;
        rx_set_sp(F - 8);
        ACT_CALLR(set, 0, rx_atv_water_singleton());
        rx_set_sp(F - 4);
        ACT_CALL(0, rx_wake_add_guarded_splat(set, F + 0xa0, U32(F, -4), 8.0f));
        rx_set_sp(F);
        SU32(act, 0xff368, 1);
    }
    {
        uint32_t edx;
        ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, F + 0x40, &edx));
    }
    if (!(F32(F, 0xa4) - F32(F, 0x44) > 3.0f /* 0x92fe48 */)) { ACT_END(F); return; }
    const float vy = F32(F, 0x94);
    if (vy > -5.0f /* 0x97739c */ && 5.0f /* 0x935890 */ > vy) {   /* the wake: a quad around the ball (the right / forward axes x 4) */
        const float s = 4.0f;   /* 0x9322c0 */
        const float ry = F32(0, 0x009cb4a4) * s, rz = F32(0, 0x009cb4a8) * s, rx = F32(0, 0x009cb4a0) * s;
        const float fy = F32(0, 0x009cb4c4) * s, fz = F32(0, 0x009cb4c8) * s, fx = F32(0, 0x009cb4c0) * s;
        const float px = F32(F, 0x20), py = F32(F, 0x24), pz = F32(F, 0x28);
        SF32(F, 0x54, py - ry);
        SF32(F, 0x58, pz - rz);
        SF32(F, 0x50, fx + (px - rx));
        SF32(F, 0x54, fy + F32(F, 0x54));
        SF32(F, 0x58, fz + F32(F, 0x58));
        SF32(F, 0x60, rx + px); SF32(F, 0x64, ry + py); SF32(F, 0x68, rz + pz);
        SF32(F, 0x60, F32(F, 0x60) + fx); SF32(F, 0x64, F32(F, 0x64) + fy); SF32(F, 0x68, F32(F, 0x68) + fz);
        SF32(F, 0x70, px - rx); SF32(F, 0x74, py - ry); SF32(F, 0x78, pz - rz);
        SF32(F, 0x70, F32(F, 0x70) - fx); SF32(F, 0x74, F32(F, 0x74) - fy);
        SF32(F, 0x40, fx); SF32(F, 0x44, fy); SF32(F, 0x48, fz);
        SF32(F, 0x78, F32(F, 0x78) - fz);
        SF32(F, 0x80, (rx + px) - fx);
        float size = F32(F, 0x38) / 30.0f /* 0x931d98 */;
        SF32(F, 0x84, (ry + py) - fy);
        SF32(F, 0x88, (rz + pz) - fz);
        if (size > 1.0f) size = 1.0f;
        const float h = 0.5f;   /* 0x9322cc */
        SF32(F, -4, x87_copy(dt));
        SF32(F, 0x10, F32(F, 0x90) * h);
        SF32(F, -8, size * h);
        SF32(F, 0x14, F32(F, 0x94) * h);
        SF32(F, 0x18, F32(F, 0x98) * h);
        SU32(F, -12, F + 0x80); SU32(F, -16, F + 0x70); SU32(F, -20, F + 0x60); SU32(F, -24, F + 0x50);
        rx_set_sp(F - 24);
        rx_addr set;
        ACT_CALLR(set, 0, rx_atv_water_singleton());
        ACT_CALL(0, rx_tag_wake_quad(set, F + 0x10, F + 0x50, F + 0x60, F + 0x70, F + 0x80, F32(F, -8), F32(F, -4)));
        rx_set_sp(F);
    }
    const float t = F32(act, 0xff364) + dt;
    SF32(act, 0xff364, t);
    if (!(t >= F32(0, 0x009c5194))) { ACT_END(F); return; }   /* comiss t, WaterKickTime; jb */
    /* the water kick: the ball pushed toward its start entity (+0xff360), harder with each kick, and the big splash */
    {
        const rx_addr m = act_vcall(U32(act, 0xff360), 0x38, 0, 0);
        SU32(F, -4, m);
        rx_set_sp(F - 4);
        ACT_CALL(0, (void)rx_matrix_row3(F + 0x10, m, 3));
        rx_set_sp(F);
        float dy = F32(F, 0x14) - F32(F, 0x24);
        float dx = F32(F, 0x10) - F32(F, 0x20);
        float dz = F32(F, 0x18) - F32(F, 0x28);
        const float l2 = dx * dx + dy * dy + dz * dz;
        SF32(F, 0x90, dx); SF32(F, 0x98, dz); SF32(F, 0x38, l2);
        if (l2 > 0.0f) {
            float len;
            ACT_CALLR(len, 1, phys_sqrtf(x87_copy(l2)));
            const float inv = 1.0f / len;
            dx = F32(F, 0x90) * inv;
            dz = F32(F, 0x98) * inv;
        }
        const float up = 10000.0f;   /* 0x931e98 */
        const float k = (float)I32(act, 0xff370) * 500.0f /* 0x9778ec */ + up;
        SF32(F, 0x90, dx * k);
        SF32(F, 0x98, dz * k);
        SF32(F, 0x94, up);
        if (BALL_PH(act) != 0) {
            const rx_addr world = U32(BALL_PH(act), 4);
            ACT_CALL(1, rx_world_body_velocity_clear(world, rx_world_physics_handle_body(BALL_PH(act))));
        }
        ACT_CALL(2, rx_tag_ball_impulse(act, F + 0x90));
        SF32(F, -4, x87_copy(rx_bf(0xc1200000u)));   /* -10.0, 0x9d78b8 */
        rx_addr set;
        rx_set_sp(F - 8);
        ACT_CALLR(set, 0, rx_atv_water_singleton());
        rx_set_sp(F - 4);
        ACT_CALL(0, rx_wake_add_guarded_splat(set, F + 0xa0, U32(F, -4), 8.0f /* 0x9322b8 */));
        rx_set_sp(F);
        SU32(act, 0xff370, U32(act, 0xff370) + 1);
        SF32(act, 0xff364, 0.0f);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0056f290, rx_tag_ball_water)

/* 007efe90 (EAX = the water set, ECX = the velocity; stack the quad's points p0 .. p3, size, dt; RET 0x18): a wake splat quad
 * (Wake_AddSplatQuad 007ec280: EDI = the set's wake, c = p2, d = p3, then p0, p1, the velocity, the size and dt through x87) when the set
 * has a wake. Frame: push edi. */
void rx_tag_wake_quad(rx_addr set, rx_addr velocity, rx_addr p0, rx_addr p1, rx_addr p2, rx_addr p3, float size, float dt) {
    const rx_addr wake = U32(set, 0);
    if (wake == 0) return;
    ACT_FRAME(F, 0, 1);
    SF32(F, -4, x87_copy(dt));
    SF32(F, -8, x87_copy(size));
    rx_set_sp(F - 8);
    ACT_CALL(3, rx_wake_add_quad(wake, p2, p3, p0, p1, velocity, U32(F, -8), F32(F, -4)));
    ACT_END(F);
}
RX_FUNCTION(0x007efe90, rx_tag_wake_quad)

/* 0056fa50 (ESI = act): the ball in a cannonball (water) zone of the level (world +0x22130). With the physics on, the ball's position
 * (vf17) in a zone (00519fa0): unless already kicked out and moving faster than 3 ft/s away from the start entity +0xff360 (the dot of
 * its velocity with the way back < 0), the ball is kicked toward the start entity: the direction normalised (SqrtF), x / z times 1000
 * ([0x9cf5b0]), y = 4000 ([0x9cf5ac]); the body's velocity cleared (00770d00), the impulse (0056e320), +0xff378 = 1. Out of the zones
 * +0xff378 = 0. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x5c; push edi (F); the position F + 0x10, the way back F + 0x30,
 * the start F + 0x40, the velocity / the kick F + 0x50, the length F + 0x2c. */
void rx_tag_ball_cannonball(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x5c, 1);
    if (U32(act, 0xff388) == 0) { ACT_END(F); return; }
    if ((U32(act, 0xff380) != 0 && (rx_net_ref_valid(U32(act, 0xff380)) & 0xff) == 0)) { ACT_END(F); return; }
    if (U32(U32(act, 0x234), 0x22130) == 0) { ACT_END(F); return; }
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(BALL(act), 0x44, 1, a); }
    int inside;
    ACT_CALLR(inside, 0, rx_cannonball_inside(F + 0x10, 0));
    if (inside == 0) { SU32(act, 0xff378, 0); ACT_END(F); return; }
    const rx_addr m = act_vcall(U32(act, 0xff360), 0x38, 0, 0);
    const float sx = F32(m, 0x30), sy = F32(m, 0x34), sz = F32(m, 0x38);
    SF32(F, 0x40, sx);
    SF32(F, 0x30, sx - F32(F, 0x10));
    SF32(F, 0x34, sy - F32(F, 0x14));
    SF32(F, 0x44, sy);
    SF32(F, 0x48, sz);
    SF32(F, 0x38, sz - F32(F, 0x18));
    ball_velocity(act, F + 0x50);
    if (U32(act, 0xff378) != 0) {
        const float a = F32(F, 0x58), b = F32(F, 0x54), c = F32(F, 0x50);
        float len;
        ACT_CALLR(len, 1, phys_sqrtf(a * a + b * b + c * c));
        if (!(3.0f /* 0x92fe48 */ > len)) {
            const float dot = F32(F, 0x58) * F32(F, 0x38) + F32(F, 0x54) * F32(F, 0x34) + F32(F, 0x50) * F32(F, 0x30);
            if (!(0.0f > dot)) { ACT_END(F); return; }
        }
    }
    float x = F32(F, 0x40) - F32(F, 0x10);
    float z = F32(F, 0x48) - F32(F, 0x18);
    const float y = F32(F, 0x44) - F32(F, 0x14);
    const float l2 = z * z + x * x + y * y;
    SF32(F, 0x50, x);
    SF32(F, 0x58, z);
    SF32(F, 0x2c, l2);
    if (l2 > 0.0f) {
        float len;
        ACT_CALLR(len, 1, phys_sqrtf(x87_copy(l2)));
        const float inv = 1.0f / len;
        x = F32(F, 0x50) * inv;
        z = F32(F, 0x58) * inv;
    }
    const float k = 1000.0f;   /* 0x9cf5b0 */
    x = x * k;
    z = z * k;
    if (BALL_PH(act) != 0) {
        const rx_addr world = U32(BALL_PH(act), 4);
        const rx_addr body = rx_world_physics_handle_body(BALL_PH(act));
        SF32(F, 0x50, x); SF32(F, 0x58, z); SF32(F, 0x54, 4000.0f /* 0x9cf5ac */);
        ACT_CALL(1, rx_world_body_velocity_clear(world, body));
    } else {
        SF32(F, 0x50, x); SF32(F, 0x58, z); SF32(F, 0x54, 4000.0f /* 0x9cf5ac */);
    }
    ACT_CALL(2, rx_tag_ball_impulse(act, F + 0x50));
    SU32(act, 0xff378, 1);
    ACT_END(F);
}
RX_FUNCTION(0x0056fa50, rx_tag_ball_cannonball)

/* 0056fcf0 (ESI = act): the ball kept near the ground. With the physics on (a network ball valid) the ball's position (vf17) against the
 * terrain under it (0042d960, 007c5950 on a copy): more than 20 ft (0x935aac) below the ground it goes to the ground, more than 1000 ft
 * (0x9cf5b0) above it to 1000 ft above (the physics off, vf28 at the identity orientation 0xd61640, on).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x2c; push edi (F); the position F + 0x10, the probe F + 0x20. */
void rx_tag_ball_ground(rx_addr act) {
    ACT_FRAME_EBP(F, 16, 0x2c, 1);
    if (U32(act, 0xff388) == 0) { ACT_END(F); return; }
    if ((U32(act, 0xff380) != 0 && (rx_net_ref_valid(U32(act, 0xff380)) & 0xff) == 0)) { ACT_END(F); return; }
    { const uint32_t a[1] = {F + 0x10}; (void)act_vcall(BALL(act), 0x44, 1, a); }
    SU32(F, 0x20, U32(F, 0x10)); SU32(F, 0x24, U32(F, 0x14)); SU32(F, 0x28, U32(F, 0x18));
    const rx_addr terrain = act_terrain(act);
    uint32_t edx, found;
    ACT_CALLR(found, 1, phys_terrain_base_height_at(terrain, F + 0x20, &edx));
    if (found == 0) { ACT_END(F); return; }
    const float ground = F32(F, 0x24), y = F32(F, 0x14);
    if (y >= ground - 20.0f /* 0x935aac */) {
        if (!(y - ground > 1000.0f /* 0x9cf5b0 */)) { ACT_END(F); return; }
        SF32(F, 0x14, ground + 1000.0f);
    } else SF32(F, 0x14, ground);
    ACT_CALL(2, rx_tag_ball_physics(act, 0));
    { const uint32_t a[2] = {F + 0x10, 0x00d61640}; (void)act_vcall(BALL(act), 0x70, 2, a); }
    ACT_CALL(2, rx_tag_ball_physics(act, 1));
    ACT_END(F);
}
RX_FUNCTION(0x0056fcf0, rx_tag_ball_ground)
