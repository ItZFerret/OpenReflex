/* world_rider_62.c -- the rider look-at controller (0x0062xxxx): where a rider's head looks (a rider nearby ahead, a point ahead
 * of the bike, ...), run from the vehicles' update (vf35) through 00624ef0.
 *
 * The look-at state (`look`, as far as this code uses it):
 *   +0x00 mode (0 ahead, 1 at a rider, 2 hold, 3 off, 4 ...), +0x04 the previous mode
 *   +0x0c dt of the search timer, +0x30 / +0x48 random weights, +0x50 the own rider, +0x54 the search timer
 *   +0x58 the target rider, +0x5c the previous target, +0x60 the look point (vec3), +0xb0 the blend time
 * Rider: +0x1ac its vehicle body (vfunc 0x44 = position into a vec4; +0xc0 position, +0x130 forward, +0x160 velocity, +0x280
 * a flag that makes it a target whatever its speed), +0x1e4 "no look-at" flag, +0x94 the rider's group (riders list +0x268,
 * count +0x26c; each entry's +0x74 is a rider). core/notes/world.md (rider). */
#include "rider_pose_override.h"
#ifndef RX_ORACLE
#include "fx_util.h"
#include <stdlib.h>
#endif
#include "phys_terrain.h"
#include "phys_rig.h"
#include "phys_rider.h"
#include "anim_rider_a.h"
#include "world_rider.h"
#include "../src/rx_os.h"
#include "world_util.h"
#include "world_rider_extern.h"
#include "world_rider_util.h"
#include "world.h"
#include "phys_body.h"
#include "world_wreck.h"
#include "world_veh.h"
#include "phys_misc.h"
#include "world_misc.h"
#include "world_node.h"
#include "loop_jobs.h"
#include "rider_cap.h"

/* The game's random generator (0x9cb860 seed, 0x9cb864 call count): seed = seed * 0x41c64e6d + 0x3039, 15 bits out. */
static uint32_t game_rand15(void) {
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    return (seed >> 16) & 0x7fffu;
}
/* a 15-bit random value scaled to [0, 1) as the original does it: through a double product (exact) */
static float game_rand_unit(void) {
    return (float)((double)(float)(int32_t)game_rand15() * 3.0517578125e-05 /* 0x9776a0 */);
}

/* 00625390 LookAt_FindTarget (EAX = look; EAX = 1 when a target was taken): the nearest rider of the own rider's group that is
 * ahead (dot(forward, d) > 0) within 15 m (0x9d78c0 holds 225, the squared distance) and either flagged (+0x280) or moving
 * relative to the own rider faster than sqrt(0x9d78bc); skips the own rider, the previous target and riders without a body,
 * and does nothing while the own rider has +0x1e4 set. The best becomes the target (006258a0). */
uint32_t rx_world_lookat_find_target(rx_addr look) {
    world_frame f = world_frame_enter(0x24, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x24; push ebx; push esi; push edi */
    float best = F32(0, 0x009d78c0);
    rx_addr best_rider = 0;
    const rx_addr group = U32(U32(look, 0x50), 0x94);
    uint32_t taken = 0;
    if (group != 0 && I32(group, 0x26c) > 0) {
        rx_addr entry = U32(group, 0x268);
        for (int32_t n = I32(group, 0x26c); n != 0; n--, entry += 4) {
            const rx_addr rider = U32(U32(entry, 0), 0x74);
            const rx_addr own = U32(look, 0x50);
            if (own == rider || U32(own, 0x1e4) != 0) continue;
            const rx_addr b = U32(rider, 0x1ac);
            if (b == 0 || U32(look, 0x5c) == rider) continue;
            const rx_addr a = U32(own, 0x1ac);
            const float dx = F32(b, 0xc0) - F32(a, 0xc0), dy = F32(b, 0xc4) - F32(a, 0xc4), dz = F32(b, 0xc8) - F32(a, 0xc8);
            const float vx = F32(b, 0x160) - F32(a, 0x160), vy = F32(b, 0x164) - F32(a, 0x164), vz = F32(b, 0x168) - F32(a, 0x168);
            const float dist2 = (dz * dz + dy * dy) + dx * dx;
            const float ahead = (F32(a, 0x138) * dz + F32(a, 0x134) * dy) + F32(a, 0x130) * dx;
            if (!(ahead > 0.0f /* 0x92e12c */)) continue;
            if (U32(b, 0x280) == 0) {
                if (!(best > dist2)) continue;
                if (!((vz * vz + vy * vy) + vx * vx > F32(0, 0x009d78bc))) continue;
            }
            best_rider = rider;
            best = dist2;
        }
        if (best_rider != 0) {
            WORLD_CALL_SP(0, rx_world_lookat_set_target(look, best_rider));
            taken = 1;
        }
    }
    world_frame_leave(&f);
    return taken;
}
RX_FUNCTION(0x00625390, rx_world_lookat_find_target)

/* 006258a0 LookAt_SetTarget (ESI = look, EAX = rider): mode 1 (look at a rider) with `rider` as the target (the old one becomes
 * the previous target; both are cleared first when the previous mode was 1), blend time 1, the look point from the rider's
 * body (vfunc 0x44), and two random weights: +0x48 in [0.5, 1], +0x30 = 0.5 + r / 2 clamped to [0.25, 1]. */
void rx_world_lookat_set_target(rx_addr look, rx_addr rider) {
    if (U32(look, 4) == 1) { SU32(look, 0x58, 0); SU32(look, 0x5c, 0); }
    SU32(look, 0x5c, U32(look, 0x58));
    SU32(look, 0x58, rider);
    SF32(look, 0xb0, 1.0f /* 0x931f24 */);
    SU32(look, 4, U32(look, 0));
    SU32(look, 0, 1);
    const rx_addr body = U32(rider, 0x1ac);
    WORLD_CTX(U32(U32(body, 0), 0x44), &WCTX(.ecx = body, .esi = look), 0, look + 0x60);   /* the body's position */
    const float w1 = game_rand_unit();
    SF32(look, 0x48, w1);
    const float w2 = game_rand_unit() * 0.5f /* 0x9322cc */ + 0.5f;
    SF32(look, 0x30, w2);
    float c1;
    if (w1 >= 1.0f) c1 = 1.0f;
    else if (0.5f >= w1) c1 = 0.5f;
    else c1 = w1;
    SF32(look, 0x48, c1);
    float c2;
    if (w2 >= 1.0f) c2 = 1.0f;
    else if (!(0.25f /* 0x92ff84 */ >= w2)) c2 = w2;
    else c2 = 0.25f;
    SF32(look, 0x30, c2);
}
RX_FUNCTION(0x006258a0, rx_world_lookat_set_target)

/* 00625560 LookAt_UpdateSearch (EAX = look; EAX = 1 when a target was taken): unless the own rider has +0x230 set, the
 * search timer +0x54 runs down by +0xc; when it is out (<= 0) a target is searched (00625390). */
uint32_t rx_world_lookat_update_search(rx_addr look) {
    world_frame f = world_frame_flat(1, 0);   /* push ecx */
    uint32_t taken = 0;
    if (U32(U32(look, 0x50), 0x230) == 0) {
        const float t = phys_sub(F32(look, 0x54), F32(look, 0xc));
        SF32(look, 0x54, t);
        if (0.0f >= t) {
            uint32_t found;
            WORLD_CALL_SP(0, found = rx_world_lookat_find_target(look));
            if (found != 0) taken = 1;
        }
    }
    world_frame_leave(&f);
    return taken;
}
RX_FUNCTION(0x00625560, rx_world_lookat_update_search)

/* 00508480 Body_Forward (ECX = vehicle body, EAX = out): out = the body's forward vector +0x130 (three floats through
 * fld / fstp: a signalling NaN comes out quiet). */
void rx_world_body_forward(rx_addr body, rx_addr out) {
    for (uint32_t i = 0; i < 3; i++) SU32(out, 4 * i, world_x87_pass(U32(body, 0x130 + 4 * i)));
}
RX_FUNCTION(0x00508480, rx_world_body_forward)

/* 00508460 Body_Up (ECX = vehicle body, EAX = out): out = the body's up vector / ground normal +0x140 (three floats through
 * fld / fstp: a signalling NaN comes out quiet). The idle rider's fidget reads lane 1 (0054cec0). */
void rx_world_body_up(rx_addr body, rx_addr out) {
    for (uint32_t i = 0; i < 3; i++) SU32(out, 4 * i, world_x87_pass(U32(body, 0x140 + 4 * i)));
}
RX_FUNCTION(0x00508460, rx_world_body_up)

/* M (the rider's 4x4 matrix, rows of 16 bytes) times the point (x, y, z), summed as the original: (r1 y + r0 x) + r2 z + r3 */
static void lookat_transform(rx_addr M, float x, float y, float z, float out[3]) {
    for (uint32_t i = 0; i < 3; i++)
        out[i] = ((F32(M, 0x10 + 4 * i) * y + F32(M, 4 * i) * x) + F32(M, 0x20 + 4 * i) * z) + F32(M, 0x30 + 4 * i);
}

/* 006255a0 LookAt_AimAhead (ESI = look, EDI = out point, EAX = local look direction; stack heading h; RET 4): with h < 0.1 the
 * point 100 m straight ahead of the rider (M = look +0x4c applied to (0, 0, 100)). Otherwise the direction scaled by +0x38
 * plus the extra distance e = max(+0xa0 - +0x40, 0), offset sideways by the swing amplitudes +0x3c * (+0xa8, +0xa4) * e
 * (weights [0x9d78a0] / [0x9d78c4], or 1 / 0 with +0xac; +0xa8 flips sign when +0xa4 * +0xa8 < 0), into the world by M;
 * taken only when it lies ahead of the bike (dot with the body's forward > 0, from the body's position, vfunc 0x44). */
void rx_world_lookat_aim_ahead(rx_addr look, rx_addr out, rx_addr dir, float h) {
    world_frame f = world_frame_enter(0x30, 0, 1);   /* push ebp; and esp, -16; sub esp, 0x30 */
    const rx_addr pos = f.esp + 0x10, fwd = f.esp + 0x20;
    if (0.1f /* 0x931d94 */ > h) {
        SF32(out, 0, 0.0f); SF32(out, 4, 0.0f); SF32(out, 8, 100.0f /* 0x93005c */);
        float w[3];
        lookat_transform(U32(look, 0x4c), 0.0f, 0.0f, 100.0f, w);
        SF32(out, 0, w[0]); SF32(out, 4, w[1]); SF32(out, 8, w[2]);
    } else {
        float e = F32(look, 0xa0) - F32(look, 0x40);
        if (!(e > 0.0f)) e = 0.0f;
        const float s = F32(look, 0x38) + e;
        const float px = F32(dir, 0) * s + 0.0f, py = F32(dir, 4) * s + 0.0f, pz = F32(dir, 8) * s + 0.0f;
        float c1 = F32(0, 0x009d78a0), c2 = F32(0, 0x009d78c4);
        if (U32(look, 0xac) != 0) { c1 = 1.0f; c2 = 0.0f; }
        if (0.0f > F32(look, 0xa4) * F32(look, 0xa8)) SF32(look, 0xa8, F32(look, 0xa8) * F32(0, 0x009cd9e8));
        const float u = (F32(look, 0x3c) * F32(look, 0xa8)) * e, u0 = u * 0.0f;
        const float x1 = u * c1 + px, y1 = u0 * c1 + py, z1 = u0 * c1 + pz;
        const float v = (F32(look, 0x3c) * F32(look, 0xa4)) * e, v0 = v * 0.0f;
        const float x2 = v * c2 + x1, y2 = v0 * c2 + y1, z2 = v0 * c2 + z1;
        float w[3];
        lookat_transform(U32(look, 0x4c), x2, y2, z2, w);
        const rx_addr body = U32(U32(look, 0x50), 0x1ac);
        WORLD_CTX(U32(U32(body, 0), 0x44), &WCTX(.ecx = body, .esi = look, .edi = out), 0, pos);   /* the body's position */
        WORLD_CALL_SP(0, rx_world_body_forward(U32(U32(look, 0x50), 0x1ac), fwd));
        const float dot = (F32(fwd, 4) * (w[1] - F32(pos, 4)) + (w[0] - F32(pos, 0)) * F32(fwd, 0)) + F32(fwd, 8) * (w[2] - F32(pos, 8));
        if (dot > 0.0f) { SF32(out, 0, w[0]); SF32(out, 4, w[1]); SF32(out, 8, w[2]); }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x006255a0, rx_world_lookat_aim_ahead)

/* mode change with the blend time 1.5 (the targets are dropped when the previous mode was 1) */
static void lookat_switch(rx_addr look, uint32_t mode) {
    if (U32(look, 4) == 1) { SU32(look, 0x58, 0); SU32(look, 0x5c, 0); }
    const uint32_t prev = U32(look, 0);
    SF32(look, 0xb0, 1.5f /* 0x9322a4 */);
    SU32(look, 4, prev);
    SU32(look, 0, mode);
}

/* 00624ef0 LookAt_Update (EAX = look; stack dt; RET 4): the look-at state machine. Mode 3 (off) only sets the blend speed +0x18
 * to 0.01. Otherwise the look direction (+0x90, as the rider matrix +0x4c rotates it, normalized) gives the heading
 * h = (d.y + d.x) * 0 + d.z; the rider's controller (+0x1f0, the key map of 0x9c52c4: vfunc 2 (7, index)) sets +0x44 (look
 * back pressed). Mode 0 (ahead): blend speed 1; unless a target is found (00625560) the point ahead (006255a0) and, past the
 * distance +0x40 < +0xa0, a switch to mode 4 when the bike's vfunc 0xac is clear or h < 0.3. Mode 4: speed 0.01, the same with
 * the opposite tests back to mode 0. Mode 1 (rider): speed 1, the target's position while it is ahead of the bike (else the
 * hold time +0x48 = -1), and once +0x48 < 0 back to mode 0 with the point ahead and a search delay +0x54 = 10. Mode 2 (hold):
 * back to mode 0 (previous mode 2) once +0x48 < 0. Then +0x48 -= dt. */
void rx_world_lookat_update(rx_addr look, float dt) {
    world_frame f = world_frame_enter(0x38, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x38; push esi; push edi */
    const rx_addr sp = f.esp, dirv = sp + 0x10, tpos = sp + 0x20, opos = sp + 0x30;
    if (U32(look, 0) == 3) {
        SF32(look, 0x18, 0.01f /* 0x9322ac */);
        world_frame_leave(&f);
        return;
    }
    const rx_addr M = U32(look, 0x4c);
    const float a = F32(look, 0x90), b = F32(look, 0x94), c = F32(look, 0x98);
    float d1 = (b * F32(M, 4) + a * F32(M, 0)) + c * F32(M, 8);
    float d2 = (b * F32(M, 0x14) + a * F32(M, 0x10)) + c * F32(M, 0x18);
    float d3 = (b * F32(M, 0x24) + a * F32(M, 0x20)) + c * F32(M, 0x28);
    const float len2 = (d3 * d3 + d2 * d2) + d1 * d1;
    SF32(dirv, 0, d1); SF32(dirv, 4, d2); SF32(dirv, 8, d3);
    if (len2 > 0.0f) {
        float len;
        WORLD_CALL_SP(4, len = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(len2)))));
        const float inv = 1.0f / len;
        d1 = d1 * inv; d2 = d2 * inv; d3 = d3 * inv;
        SF32(dirv, 0, d1); SF32(dirv, 4, d2); SF32(dirv, 8, d3);
    }
    const float h = (d2 + d1) * 0.0f + d3;
    const rx_addr own = U32(look, 0x50);
    const int32_t ctrl = I32(own, 0x1f0);
    if (ctrl > -1) {
        const rx_addr blocks = U32(0, 0x009c52c4 + 4 * (uint32_t)ctrl);
        const rx_addr keys = U32(blocks, 0x512c) == 1 ? blocks + 0x2524 : blocks + 0x1dc4;
        const rx_addr fn = U32(U32(keys, 0), 8);
        const uint32_t r = WORLD_CTX(fn, &WCTX(.eax = U32(keys, 0), .ecx = keys, .edx = fn, .esi = look), 0, 7, (uint32_t)ctrl);
        SU32(look, 0x44, r != 0);
    }
    const uint32_t harg = world_x87_pass(rx_fb(h));
    switch (U32(look, 0)) {
    case 4: {
        SF32(look, 0x18, 0.01f);
        uint32_t found;
        WORLD_CALL_SP(0, found = rx_world_lookat_update_search(look));
        if (found) break;
        WORLD_CALL_SP(4, rx_world_lookat_aim_ahead(look, look + 0x60, dirv, rx_bf(harg)));
        if (!(F32(look, 0xa0) > F32(look, 0x40))) break;
        const rx_addr body = U32(U32(look, 0x50), 0x1ac);
        if (WORLD_CTX0(U32(U32(body, 0), 0xac), &WCTX(.ecx = body, .esi = look, .edi = look + 0x60), 0) == 0 && !(h > 0.3f /* 0x931da4 */))
            break;
        lookat_switch(look, 0);
        break;
    }
    case 0: {
        SF32(look, 0x18, 1.0f);
        uint32_t found;
        WORLD_CALL_SP(0, found = rx_world_lookat_update_search(look));
        if (found) break;
        WORLD_CALL_SP(4, rx_world_lookat_aim_ahead(look, look + 0x60, dirv, rx_bf(harg)));
        if (!(F32(look, 0x40) > F32(look, 0xa0))) break;
        const rx_addr body = U32(U32(look, 0x50), 0x1ac);
        if (WORLD_CTX0(U32(U32(body, 0), 0xac), &WCTX(.ecx = body, .esi = look, .edi = look + 0x60), 0) != 0 && !(0.3f > h))
            break;
        lookat_switch(look, 4);
        break;
    }
    case 1: {
        const rx_addr target = U32(look, 0x58);
        SF32(look, 0x18, 1.0f);
        if (target != 0 && U32(target, 0x1ac) != 0) {
            const rx_addr tb = U32(target, 0x1ac);
            WORLD_CTX(U32(U32(tb, 0), 0x44), &WCTX(.ecx = tb, .esi = look), 0, tpos);
            const rx_addr ob = U32(U32(look, 0x50), 0x1ac);
            WORLD_CTX(U32(U32(ob, 0), 0x44), &WCTX(.ecx = ob, .esi = look), 0, opos);
            const rx_addr body = U32(U32(look, 0x50), 0x1ac);
            const float dot = ((F32(tpos, 8) - F32(opos, 8)) * F32(body, 0x138) + (F32(tpos, 4) - F32(opos, 4)) * F32(body, 0x134))
                              + (F32(tpos, 0) - F32(opos, 0)) * F32(body, 0x130);
            if (0.0f > dot) {
                SF32(look, 0x48, F32(0, 0x009cd9e8));
            } else {
                SU32(look, 0x60, U32(tpos, 0)); SU32(look, 0x64, U32(tpos, 4)); SU32(look, 0x68, U32(tpos, 8));
            }
        }
        if (0.0f > F32(look, 0x48)) {
            lookat_switch(look, 0);
            WORLD_CALL_SP(4, rx_world_lookat_aim_ahead(look, look + 0x60, dirv, rx_bf(harg)));
            SF32(look, 0x54, 10.0f /* 0x92fd88 */);
        }
        break;
    }
    case 2:
        if (0.0f > F32(look, 0x48)) {
            if (U32(look, 4) == 1) { SU32(look, 0x58, 0); SU32(look, 0x5c, 0); }
            SU32(look, 0, 0);
            SF32(look, 0xb0, 1.5f);
            SU32(look, 4, 2);
            WORLD_CALL_SP(4, rx_world_lookat_aim_ahead(look, look + 0x60, dirv, rx_bf(harg)));
        }
        break;
    default:
        break;
    }
    SF32(look, 0x48, phys_sub(F32(look, 0x48), dt));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00624ef0, rx_world_lookat_update)

/* 00617e60 Shape_SetRadius (stdcall shape, radius; RET 8): the collision sphere's radius vector +0x20 = (r, r, r, r). */
void rx_world_shape_set_radius(rx_addr shape, float r) {
    world_frame f = world_frame_enter(0, 0, 1);   /* push ebp; and esp, -16 */
    v4_st(shape + 0x20, v4_splat(r));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00617e60, rx_world_shape_set_radius)

/* 00624560 RiderAnim_CheckBodyHits (ESI = ra): while the rider rides (+0x5b6d0 clear) and the hit test is on (byte +0x5bb64),
 * the rider's body sphere (shape ra +0xaea0, radius +0xad9c) at the rider body's position (+0x604 handle, 0042bd90; else the
 * frame's stale vector) is queried against the physics world of the active game mode (game +0x5a54 / +0x20c8 by the views of
 * [0xa95844] +0x20 / +0x1c, its +0x20134) with World_QueryShapeContacts (10 contacts; types / ids in the frame). Water
 * (type 0x10) below the depth [0x9cc8a8] > +0x5bae4, and, faster than 10 (+0x5bb30), a generic object other than the rider and
 * its bike (type 4) or anything whose vfunc 0x58 is not 0x1033, knocks the rider off: +0x5b6d0 = 1, +0x5b568 = 10. */
void rx_world_rider_check_body_hits(rx_addr ra) {
    world_frame f = world_frame_enter(0x268, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x268; push ebx; push edi */
    const rx_addr sp = f.esp, cookie = sp + 0x26c, ids = sp + 0x10, types = sp + 0x38, pos = sp + 0x70, results = sp + 0x80;
    SU32(cookie, 0, U32(0, 0x009b45e4) ^ (sp + 8));
    if (U32(ra, 0x5b6d0) == 0 && U8(ra, 0x5bb64) != 0) {
        const rx_addr views = U32(0, 0x00a95844);
        rx_addr mode = 0;
        if (U32(U32(views, 0x20), 0x34) == 1) mode = U32(U32(0, 0x00a946b8), 0x5a54);
        else if (U32(U32(views, 0x1c), 0x34) == 1) mode = U32(U32(0, 0x00a946b8), 0x20c8);
        const rx_addr world = U32(mode, 0x20134);
        SU32(sp, 8, world);
        const rx_addr handle = U32(ra, 0x604);
        if (handle != 0) {
            rx_addr rec;
            WORLD_CALL_SP(0, rec = phys_body_record(handle));
            v4_st(pos, v4_ld(rec + 0x40));
        }
        const uint32_t radius = world_x87_pass(U32(ra, 0xad9c));
        SU32(sp, 0xc, radius);
        for (uint32_t k = 0; k < 10; k++) { SU32(types, 4 * k, 0); SU32(ids, 4 * k, 0); }
        const rx_addr shape = ra + 0xaea0;
        WORLD_CALL_SP(8, rx_world_shape_set_radius(shape, rx_bf(radius)));
        int32_t n;
        WORLD_CALL_SP(0x10, n = (int32_t)rx_world_query_shape_contacts(types, shape, world, pos, results, ids));
        SU32(sp, 0xc, (uint32_t)n);
        if (n > 0) {
            const int fast = v4_all3(v4_cmplt(v4_splat(10.0f /* 0x92fd88 */), v4_splat(F32(ra, 0x5bb30))));
            for (int32_t i = 0; i < n; i++) {
                const uint32_t type = U32(types, 4 * (uint32_t)i);
                if (type == 0x10 && F32(0, 0x009cc8a8) > F32(ra, 0x5bae4)) { SU32(ra, 0x5b568, 10); SU32(ra, 0x5b6d0, 1); }
                if (!fast) continue;
                const rx_addr obj = U32(ids, 4 * (uint32_t)i);
                if (type == 4) {
                    if (obj == ra || obj == U32(ra, 0x5b554)) continue;
                } else if (WORLD_CTX0(U32(U32(obj, 0), 0x58), &WCTX(.ecx = obj, .ebx = (uint32_t)i, .esi = ra, .edi = 1), 0) == 0x1033) {
                    continue;
                }
                SU32(ra, 0x5b6d0, 1);
                SU32(ra, 0x5b568, 10);
            }
        }
    }
    phys_security_check_cookie(U32(cookie, 0) ^ (sp + 8));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00624560, rx_world_rider_check_body_hits)

/* 00623960 (EAX = rider animation, ECX = chain group 0..3; switch 0x6239c0; EAX): chain group k's first chain of the IK set
 * (+0x5bc: count +0x60, 0x80-byte groups at +0x64, chain at +8), or 0 when the set has no group k. */
rx_addr rx_world_rider_chain_group(rx_addr ra, uint32_t k) {
    const rx_addr set = U32(ra, 0x5bc);
    if (U32(set, 0x60) <= k) return 0;
    return U32(U32(set, 0x64), 8 + 0x80 * k);
}
RX_FUNCTION(0x00623960, rx_world_rider_chain_group)

/* 00623310 RiderAnim_EndWreck (stdcall ra, class; RET 8; frame push ebp; and esp, -16; one local): the wreck animation ends:
 * pose override / stunt / trick end (00621490, 00621400, 00621320); +0xaf00 = class, +0xaf10 = 0, mode +0x5b57c = 0xc,
 * +0x5b580 = 3. Only in pose mode +0x5bab8 0 or 1 the rest: with a pose (+0x5ae != 0xffff) its outputs (+0x564 byte 1,
 * +0x568 the pose id, +0x58c = 0 clamped to [+0x584, +0x588]) and the lean property (+0x4f8 listeners) to the value that
 * cancels the lean limits (+0x4ec < 0: -limit; +0x4e8 > 0: limit' - ...); the IK fades restart (006b0480); the skeleton's
 * velocities reset (0041fe60 / 0041f030), 0042a240 on +0x5d0; the bike sim's pose override ends; the trick state to 4. */
void rx_world_rider_end_wreck(rx_addr ra, uint32_t cls) {
    world_frame f = world_frame_enter(4, 3, 1);   /* push ecx (the local); push ebx; push esi; push edi */
    const rx_addr local = f.esp + 0xc;
    WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
    WORLD_CALL_SP(0, rx_world_rider_end_stunt(ra));
    WORLD_CALL_SP(0, rx_world_rider_end_trick(ra));
    SU32(ra, 0xaf00, cls);
    SU32(ra, 0xaf10, 0);
    SU32(ra, 0x5b57c, 0xc);
    SU32(ra, 0x5b580, 3);
    const uint32_t pm = U32(ra, 0x5bab8);
    if (pm != 0 && pm != 1) { world_frame_leave(&f); return; }
    if (U16(ra, 0x5ae) != 0xffff) {
        if (U32(ra, 0x564) != 0) SU8(U32(ra, 0x564), 0, 1);
        if (U32(ra, 0x568) != 0) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ae));
        const float hi = F32(ra, 0x588), lo = F32(ra, 0x584);
        float c = hi;
        if (!(0.0f >= hi)) c = 0.0f;
        if (lo >= c) c = lo;
        if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, c);
        const float up = F32(ra, 0x4ec);
        float v = 0.0f;
        if (0.0f > up) v = phys_sub(-0.0f /* 0x9774a8 */, up);
        else {
            const float dn = F32(ra, 0x4e8);
            if (dn > 0.0f) v = phys_add(phys_sub(-0.0f, dn), up);
        }
        SF32(local, 0, v);
        WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x4f8, local));
    }
    WORLD_CALL_SP(0, rx_world_ik_restart_fades(U32(ra, 0x5bc)));
    SU8(ra, 0x5b578, 1);
    SU32(ra, 0x940, 0);
    for (uint32_t i = 0; i < 16; i += 4) SU32(ra, 0x8e0 + i, U32(WORLD_ZERO4, i));
    SU32(ra, 0x668, 0);
    SU32(ra, 0x660, 0);
    WORLD_CALL_SP(4, rx_world_skeleton_set_velocity(ra + 0x950, ra + 0x5bb00, ra + 0x5bb20));
    WORLD_CALL_SP(0, rx_world_skeleton_orient(ra + 0x950));
    SU32(ra, 0xae90, 1);
    WORLD_CALL_SP(4, phys_rider_clear_bone_flags(ra + 0x5d0));
    SU8(U32(ra, 0x144), 0x1f4, 0);
    SU32(ra, 0x5bb68, 0);
    if (U32(ra, 0xc930) != 0) {
        SU32(ra, 0xc930, 0);
        WORLD_CALL_SP(0, bike_end_pose_override(U32(U32(ra, 0xaf1c), 0x234)));   /* Simulation_EndPoseOverride */
    }
    if (U32(ra, 0x5b594) == 1) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
    const uint32_t old = U32(ra, 0x5b590);
    SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
    SU32(ra, 0x5b594, old);
    SU32(ra, 0x5b590, 4);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00623310, rx_world_rider_end_wreck)

/* ---- the rider's gesture state (the "lean / look" gesture of 0x0062axxx; `s`):
 *   +0x00 state (0 idle, 1 / 4 lean one way (4 = held), 2 / 3 the other way (3 = held), 5 blend in, 6 blend out)
 *   +0x04 side (1 = left), +0x40 blend, +0x44 blend rate, +0x48 level, +0x4c level direction, +0x50 blend out flag,
 *   +0x54 hold flag, +0x90 / +0x94 wobble flags, +0x98 wobble value, +0x9c wobble time, +0xa0 / +0xb4 wobble outputs,
 *   +0xac wobble input, +0xb0 wobble amplitude, +0xb8 hold timer, +0xbc the rider (+0x1b4 its animation, +0x1ac vehicle),
 *   +0xc4 the last dt. The animation's IK set (+0x5bc) groups 2 / 3 carry the gesture weights (+0x100 / +0x180). ---- */

static rx_addr ik_group_weight(rx_addr ra, uint32_t k) {   /* group k's weight float, 0 when the set has no group k */
    const rx_addr set = U32(ra, 0x5bc);
    return U32(set, 0x60) > k ? U32(set, 0x64) + 0x80u * k : 0;
}

/* 0062a370 (EAX = s, EDI = level): a new gesture level while group 2's chain exists and its weight is <= 0: the direction
 * (+0x4c = +1 / -1 for a higher / lower level), the level, and from idle the blend in (state 5, rate 8, +0x50 = 0) */
void rx_world_gesture_set_level(rx_addr s, int32_t level) {
    const rx_addr ra = U32(U32(s, 0xbc), 0x1b4);
    const rx_addr set = U32(ra, 0x5bc);
    if (U32(set, 0x60) <= 2 || U32(U32(set, 0x64), 0x108) == 0) return;
    if (!(0.0f >= F32(ik_group_weight(ra, 2), 0))) return;
    const int32_t cur = I32(s, 0x48);
    if (level == cur) return;
    if (U32(s, 0) == 0) { SU32(s, 0x50, 0); SF32(s, 0x44, 8.0f /* 0x9322b8 */); SU32(s, 0, 5); }
    SU32(s, 0x4c, level > cur ? 1u : 0xffffffffu);
    SU32(s, 0x48, (uint32_t)level);
}
RX_FUNCTION(0x0062a370, rx_world_gesture_set_level)

/* 0062a420 (EDX = s, EDI = hold): +0x54 = hold; with group 3's chain present, its weight <= 0, idle and hold set: the hold
 * blend (state 6, rate 8) */
void rx_world_gesture_set_hold(rx_addr s, uint32_t hold) {
    SU32(s, 0x54, hold);
    const rx_addr ra = U32(U32(s, 0xbc), 0x1b4);
    const rx_addr set = U32(ra, 0x5bc);
    if (U32(set, 0x60) <= 3 || U32(U32(set, 0x64), 0x188) == 0) return;
    if (!(0.0f >= F32(ik_group_weight(ra, 3), 0))) return;
    if (U32(s, 0) != 0 || hold == 0) return;
    SF32(s, 0x44, 8.0f /* 0x9322b8 */);
    SU32(s, 0, 6);
}
RX_FUNCTION(0x0062a420, rx_world_gesture_set_hold)

/* 0062a9a0 (EAX = s; EAX): may the rider gesture: its pose is current (+0x5a0), no pose override (+0xaf10), in mode 0xc,
 * not in a strong turn (+0x5babc with +0x5bac4 > 0.5), the vehicle faster than the global minimum (0x9d78cc) and the lean
 * property (+0x308) <= 0.75 */
uint32_t rx_world_gesture_allowed(rx_addr s) {
    world_frame f = world_frame_flat(1, 0);   /* push ecx */
    const rx_addr o = U32(s, 0xbc), ra = U32(o, 0x1b4);
    uint32_t r = 0;
    const rx_addr p = U32(ra, 0x580);
    if (U16(ra, 0x5a0) != (p ? U16(p, 0) : 0)) goto out;
    if (U32(ra, 0xaf10) != 0 || U32(ra, 0x5b57c) != 0xc) goto out;
    if (U32(ra, 0x5babc) != 0 && F32(ra, 0x5bac4) > 0.5f /* 0x9322cc */) goto out;
    if (F32(0, 0x009d78cc) >= F32(U32(o, 0x1ac), 0x150)) goto out;
    {
        float lean;
        WORLD_CALL_SP(4, lean = rx_world_float_listeners_get(ra + 0x308));
        if (lean > 0.75f /* 0x935aa8 */) goto out;
    }
    r = 1;
out:
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x0062a9a0, rx_world_gesture_allowed)

/* 0062aa30 (ECX = s; frame push ebp; and esp, -8; push ecx; push esi): the idle wobble: a new random amplitude (+0xb0 from
 * the table 0x9c4bf0, rand % 3) while +0x90 is clear and +0x94 set; then the target clamp(+0xac * 3, -6, 6) / 6 * amplitude
 * approached with the rate min(t, +0x9c) / +0x9c (t = the frame time [[0xa95844] + 0x10] + 0x18): +0x98 the value, +0xa0
 * the rate, +0xb4 |value| */
void rx_world_gesture_wobble(rx_addr s) {
    world_frame f = world_frame_enter(0, 2, 0);
    if (U32(s, 0x90) == 0 && U32(s, 0x94) != 0) {
        const uint32_t k = game_rand15() % 3u;
        SU32(s, 0xb0, world_x87_pass(U32(0, 0x009c4bf0 + 4u * k)));
    }
    const float x = phys_mul(F32(s, 0xac), 3.0f /* 0x92fe48 */);
    float c = 6.0f /* 0x92fe4c */;
    if (!(x >= c)) {
        const float lo = -6.0f /* 0x977728 */;
        c = x;
        if (lo >= x) c = lo;
    }
    const float t = F32(U32(U32(0, 0x00a95844), 0x10), 0x18);
    const float period = F32(s, 0x9c);
    c = phys_mul(phys_mul(c, 0.16666667f /* 0x977724 */), F32(s, 0xb0));
    float k = period > t ? t : period;
    k = phys_mul(k, phys_div(1.0f, period));
    const float old = F32(s, 0x98);
    const float v = phys_add(phys_mul(phys_sub(c, old), k), old);
    SF32(s, 0x98, v);
    SF32(s, 0xa0, k);
    SU32(s, 0xb4, rx_fb(v) & 0x7fffffffu);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0062aa30, rx_world_gesture_wobble)

/* the blend step of 0062a490: towards 1 (blend in, +0x44 * dt) or 0 (out); the clamped value at +0x40 */
static float gesture_blend_step(rx_addr s, int out, float dt) {
    float c;
    if (!out) {
        const float v = phys_add(phys_mul(F32(s, 0x44), dt), F32(s, 0x40));
        SF32(s, 0x40, v);
        c = 1.0f > v ? v : 1.0f;
    } else {
        const float v = phys_sub(F32(s, 0x40), phys_mul(F32(s, 0x44), dt));
        SF32(s, 0x40, v);
        c = v > 0.0f ? v : 0.0f;
    }
    SF32(s, 0x40, c);
    return c;
}

/* 0062a490 (ESI = s; stdcall dt; RET 4): the gesture state machine (switch 0x62a97c on +0):
 *   0 idle: when allowed (0062a9a0) a strong lean of the rider (+0x5bb48 lean, +0x5b530 grip; thresholds by side, the right
 *     side's from 0x9d78a0 / 0x9cd9b0) starts state 2 (both) or 1 (lean only) with rate 4, else the wobble (0062aa30); the IK
 *     group 3 (left) / 2 (right) weight = -0.1 (+ the wobble's |value| when it ran: its xmm0)
 *   1 / 4, 2 / 3: the lean gestures (4 / 3 = held: +0xb8 grows by rate * dt, past 1.1 back to 2 / 1 with rate 4); entering
 *     the held state on a strong lean, blending out (+0x50) when the lean ends or the gesture is no longer allowed; at a full
 *     blend out back to idle
 *   5 blend in / out by +0x50 (at 1 the flag is set; at 0 with the flag back to idle), 6 the hold blend (+0x54)
 * +0xc4 = dt. */
void rx_world_gesture_update(rx_addr s, float dt) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push edi */
    const uint32_t state = U32(s, 0);
    switch (state) {
    case 0: {
        uint32_t ok;
        WORLD_CALL_SP(0, ok = rx_world_gesture_allowed(s));
        if (!ok) break;
        const rx_addr ra = U32(U32(s, 0xbc), 0x1b4);
        const float lean = F32(ra, 0x5bb48);
        uint32_t group;
        float base = 0.0f;   /* xmm0: 0, or the wobble's |value| (0062aa30 leaves it in xmm0) */
        if (U32(s, 4) == 1) {
            if (-0.65f /* 0x977740 */ > lean && F32(ra, 0x5b530) > 0.75f /* 0x935aa8 */) {
                SU32(s, 0x50, 0); SF32(s, 0x44, 4.0f /* 0x9322c0 */); SU32(s, 0, 2);
            } else if (-0.4f /* 0x97773c */ > lean) {
                SU32(s, 0x50, 0); SF32(s, 0x44, 4.0f); SU32(s, 0, 1);
            } else { WORLD_CALL_SP(0, rx_world_gesture_wobble(s)); base = F32(s, 0xb4); }
            group = 3;
        } else {
            if (lean > F32(0, 0x009d78a0) && F32(0, 0x009cd9b0) > F32(ra, 0x5b530)) {
                SU32(s, 0x50, 0); SF32(s, 0x44, 4.0f); SU32(s, 0, 2);
            } else if (lean > 0.4f /* 0x935860 */) {
                SU32(s, 0x50, 0); SF32(s, 0x44, 4.0f); SU32(s, 0, 1);
            } else { WORLD_CALL_SP(0, rx_world_gesture_wobble(s)); base = F32(s, 0xb4); }
            group = 2;
        }
        SF32(ik_group_weight(U32(U32(s, 0xbc), 0x1b4), group), 0, phys_sub(base, 0.1f /* 0x931d94 */));
        break;
    }
    case 6: {
        const uint32_t hold = U32(s, 0x54);
        const float c = gesture_blend_step(s, hold == 0, dt);
        if (c == 0.0f && hold == 0) SU32(s, 0, 0);
        break;
    }
    case 5: {
        const uint32_t out = U32(s, 0x50);
        const float c = gesture_blend_step(s, out != 0, dt);
        if (c == 1.0f) { SU32(s, 0x50, 1); break; }
        if (c == 0.0f && out != 0) SU32(s, 0, 0);
        break;
    }
    case 1: case 4: case 2: case 3: {
        const int left_pair = state == 1 || state == 4;
        const uint32_t held = left_pair ? 4 : 3;
        if (state == held) {
            const float v = phys_add(phys_mul(F32(s, 0x44), dt), F32(s, 0xb8));
            SF32(s, 0xb8, v);
            if (v > 1.1f /* 0x9774c4 */) {
                SU32(s, 0x50, 0); SF32(s, 0x44, 4.0f); SU32(s, 0, left_pair ? 2 : 1);
            }
        } else if (left_pair) {
            const rx_addr o = U32(s, 0xbc), ra = U32(o, 0x1b4);
            if (U32(s, 4) == 1) {
                if (-0.65f > F32(ra, 0x5bb48) && F32(ra, 0x5b530) > 0.75f) { SF32(s, 0xb8, 0.0f); SU32(s, 0, 4); }
                if (F32(U32(o, 0x1b4), 0x5bb48) > -0.4f) SU32(s, 0x50, 1);
            } else {
                if (F32(ra, 0x5bb48) > F32(0, 0x009d78a0) && F32(0, 0x009cd9b0) > F32(ra, 0x5b530)) { SF32(s, 0xb8, 0.0f); SU32(s, 0, 4); }
                if (0.4f > F32(U32(o, 0x1b4), 0x5bb48)) SU32(s, 0x50, 1);
            }
        } else {
            const rx_addr ra = U32(U32(s, 0xbc), 0x1b4);
            int go;
            if (U32(s, 4) == 1) go = F32(ra, 0x5bb48) > -0.6825f /* 0x977738 */ || 0.7875f /* 0x977734 */ > F32(ra, 0x5b530);
            else go = 0.6825f /* 0x977730 */ > F32(ra, 0x5bb48) || F32(ra, 0x5b530) > -0.7875f /* 0x97772c */;
            if (go) { SU32(s, 0, 3); SF32(s, 0xb8, 0.0f); }
        }
        uint32_t ok;
        WORLD_CALL_SP(0, ok = rx_world_gesture_allowed(s));
        if (!ok) SU32(s, 0x50, 1);
        const uint32_t out = U32(s, 0x50);
        const float c = gesture_blend_step(s, out != 0, dt);
        if (c == 0.0f && out != 0) SU32(s, 0, 0);
        break;
    }
    default: break;
    }
    SF32(s, 0xc4, dt);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0062a490, rx_world_gesture_update)

/* 006243d0 (ECX = rider animation, EAX = out 4x4): the root matrix: rotation of the quaternion +0x1450 (x, y, z, w) and the
 * position +0x9d0 (the products as the original orders them: x2 = 2x ..., m00 = 1 - (zz + yy), m01 = xy + zw, ...) */
void rx_world_rider_root_matrix(rx_addr ra, rx_addr out) {
    const float x = F32(ra, 0x1450), y = F32(ra, 0x1454), z = F32(ra, 0x1458), w = F32(ra, 0x145c);
    const float x2 = phys_mul(x, 2.0f /* 0x92e330 */), y2 = phys_mul(y, 2.0f), z2 = phys_mul(z, 2.0f);
    const float xx = phys_mul(x2, x), xw = phys_mul(x2, w), yy = phys_mul(y2, y), zz = phys_mul(z2, z), zw = phys_mul(z2, w);
    const float yw = phys_mul(y2, w), xz = phys_mul(x2, z), xy = phys_mul(x2, y), yz = phys_mul(y2, z);
    SF32(out, 0x00, phys_sub(1.0f, phys_add(zz, yy)));
    SF32(out, 0x04, phys_add(xy, zw));
    SF32(out, 0x20, phys_add(xz, yw));
    SF32(out, 0x10, phys_sub(xy, zw));
    SF32(out, 0x28, phys_sub(1.0f, phys_add(yy, xx)));
    SU32(out, 0x30, U32(ra, 0x9d0));
    SF32(out, 0x14, phys_sub(1.0f, phys_add(zz, xx)));
    SF32(out, 0x08, phys_sub(xz, yw));
    SU32(out, 0x34, U32(ra, 0x9d4));
    SF32(out, 0x0c, 0.0f);
    SF32(out, 0x18, phys_add(yz, xw));
    SF32(out, 0x1c, 0.0f);
    SF32(out, 0x24, phys_sub(yz, xw));
    SF32(out, 0x2c, 0.0f);
    SU32(out, 0x38, U32(ra, 0x9d8));
    SF32(out, 0x3c, 1.0f);
}
RX_FUNCTION(0x006243d0, rx_world_rider_root_matrix)

/* 0061e950 (EDI = rider animation; frame push ebp; and esp, -16; sub 0x3c): which arm reaches out: with the water (static
 * finder 0x9e30d4: 00402dc0) the water's flow at the rider's position (007c5c90 ECX = out at [esp+0x30]; stack water, &pos)
 * against the rider's x axis (vf14 matrix row 0): flow along +x -> arm chains 3 in / 2 out over 0.25 s and +0xaf50 = 0,
 * else (or without water) 2 in / 3 out and +0xaf50 = 1 */
void rx_world_rider_arm_side(rx_addr ra) {
    world_frame f = world_frame_enter(0x3c, 1, 1);
    const rx_addr sp = f.esp, row0 = sp + 0x10, pos = sp + 0x20, flow = sp + 0x30;
    if ((U32(0, 0x009e30e0) & 1u) == 0) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1u);
        SU32(0, 0x009e30dc, 0);
    }
    rx_addr water;
    WORLD_CALL_SP(0, water = rx_world_find_by_controller(0x009e30d4));
    int along = 0;
    if (water != 0) {
        const rx_addr fn = U32(U32(ra, 0), 0x38);
        rx_addr (*mf)(rx_addr) = RX_FN(fn, rx_addr (*)(rx_addr));
        rx_addr mtx;
        if (mf) WORLD_CALL_SP(0, mtx = mf(ra)); else mtx = WORLD_ORIG0(fn, RX_THISCALL, ra);
        SU32(row0, 0, U32(mtx, 0)); SU32(row0, 4, U32(mtx, 4)); SU32(row0, 8, U32(mtx, 8));
        SU32(pos, 0, U32(mtx, 0x30)); SU32(pos, 4, U32(mtx, 0x34)); SU32(pos, 8, U32(mtx, 0x38));
        WORLD_CALL_SP(8, (void)phys_terrain_base_height_normal(flow, water, pos));
        const float d = phys_add(phys_add(phys_mul(F32(flow, 8), F32(row0, 8)), phys_mul(F32(flow, 4), F32(row0, 4))),
                                 phys_mul(F32(flow, 0), F32(row0, 0)));
        along = d > 0.0f;
    }
    const uint32_t in = along ? 3 : 2;
    WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, in, 0, 0.25f /* 0x92ff84 */));
    WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 5 - in, 0.25f, 0));
    SF32(ra, 0xaf50, along ? 0.0f : 1.0f);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061e950, rx_world_rider_arm_side)

/* A clamped step of a pointed-to float (the original's comiss / movaps pairs: a value above the upper limit, then below the
 * lower, is replaced; a NaN passes both tests). */
static float rider_clamp_hi_lo(float value, float hi, float lo) {
    if (value >= hi) value = hi;
    if (lo >= value) value = lo;
    return value;
}

/* 0061db10 RiderAnim_UpdateScriptedLean (EAX = ra; stdcall dt; RET 4; frame push ecx (the local float at entry - 4), push esi),
 * run by the locomotion update (0061e3d0) in a scripted mode (+0x5bb50 == 7) or a crash (+0x5b2e0):
 *   scripted mode with the steer input +0x5b534 below 0.1: the crash state is entered once (+0x5b2e0 = 1; the animation mode
 *   +0x5b594 saved from +0x5b590, which becomes 3; +0x5b640 = 1.5; the three blend targets +0x5b704 / +0x5b718 / +0x5b72c taken
 *   from +0x5b700 / +0x5b6fc / +0x5b6f8 unless they hold FLT_MAX; +0x5b740 and +0x5b6f0 cleared). Then the lean weight +0x330
 *   moves toward -0.0 (!) by min(+0x328, +0x32c) / +0x328 of the way, is clamped to [+0x320, +0x324] and broadcast normalised
 *   to the listeners +0x334; the output +0x3d0 (a pointer) grows by 3 dt inside [+0x3c8, +0x3cc].
 *   Otherwise, while crashed (+0x5b2e0): the output shrinks by 3 dt (same clamps), and when it reached 0 outside the scripted
 *   mode the stunt ends (00621400).
 * x87 is not used; all SSE single precision (NaN rules through the phys_ helpers). */
void rx_world_rider_update_scripted_lean(rx_addr ra, float dt) {
    const rx_addr entry = rx_sp();
    const rx_addr slot = entry - 4;
    rx_set_sp(entry - 8);
    if (0.1f /* 0x931d94 */ > F32(ra, 0x5b534) && I32(ra, 0x5bb50) == 7) {
        if (U32(ra, 0x5b2e0) == 0) {
            SU32(ra, 0x5b2e0, 1);
            if (U32(ra, 0x5b594) == 1) {
                SU32(ra, 0x5b5e8, 0);
                SU32(ra, 0x5b5ec, 0);
            }
            SU32(ra, 0x5b594, U32(ra, 0x5b590));
            SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
            SU32(ra, 0x5b590, 3);
            const float unset = 3.40282347e+38f /* 0x977494 */;
            if (F32(ra, 0x5b700) != unset) SF32(ra, 0x5b704, F32(ra, 0x5b700));
            if (F32(ra, 0x5b6fc) != unset) SF32(ra, 0x5b718, F32(ra, 0x5b6fc));
            if (F32(ra, 0x5b6f8) != unset) SF32(ra, 0x5b72c, F32(ra, 0x5b6f8));
            SF32(ra, 0x5b740, 0.0f);
            SU32(ra, 0x5b6f0, 0);
        }
        const float rate_a = F32(ra, 0x328), rate_b = F32(ra, 0x32c);
        const float rate = (rate_a > rate_b) ? rate_b : rate_a;
        const float weight = F32(ra, 0x330);
        const float moved = phys_add(phys_mul(phys_div(rate, rate_a), phys_sub(-0.0f /* 0x9774a8 */, weight)), weight);
        SF32(ra, 0x330, moved);
        const float hi = F32(ra, 0x324), lo = F32(ra, 0x320);
        const float clamped = rider_clamp_hi_lo(moved, hi, lo);
        SF32(slot, 0, phys_div(phys_sub(clamped, lo), phys_sub(hi, lo)));
        WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x334, slot));
        const rx_addr out = U32(ra, 0x3d0);
        const float step = phys_mul(dt, 3.0f /* 0x92fe48 */);
        const float level = rider_clamp_hi_lo(phys_add(out != 0 ? F32(out, 0) : 0.0f, step), F32(ra, 0x3cc), F32(ra, 0x3c8));
        if (out != 0) SF32(out, 0, level);
    } else if (U32(ra, 0x5b2e0) != 0) {
        const rx_addr out = U32(ra, 0x3d0);
        const float step = phys_mul(dt, 3.0f /* 0x92fe48 */);
        const float level = rider_clamp_hi_lo(phys_sub(out != 0 ? F32(out, 0) : 0.0f, step), F32(ra, 0x3cc), F32(ra, 0x3c8));
        if (out != 0) SF32(out, 0, level);
        const rx_addr out_again = U32(ra, 0x3d0);
        if ((out_again != 0 ? F32(out_again, 0) : 0.0f) == 0.0f && I32(ra, 0x5bb50) != 7) {
            WORLD_CALL_SP(0, rx_world_rider_end_stunt(ra));
        }
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x0061db10, rx_world_rider_update_scripted_lean)

/* 00621850 RiderAnim_RagdollBlend (EAX = ra; stdcall freeze; RET 4; frame push ecx (local float at entry - 4), push esi; the
 * argument slot (entry + 4) is reused as the listeners' float): the blend targets of the ragdoll mode, by the state byte
 * +0x5bab8 (0: x = 0.5 / y = [0x9d78b0] / z = [0xd94ab0] / w = 0.5, 1: 0.4 / [0x9d78a0] / 0.4 / 0.5, else all 0): the clamped
 * outputs +0x3f4 (x within [+0x3ec, +0x3f0]) and +0x400 (y within [+0x3f8, +0x3fc]) are written; with `freeze` the weights
 * +0x3b4, +0x304, +0x35c take w, z and 1.0 (0 for other states) and nothing else moves; otherwise +0x3b4 / +0x304 / +0x35c move
 * toward them by min(rate, rate limit) / rate of the way: +0x3b4 (rates +0x3ac / +0x3b0, limits [+0x3a4, +0x3a8]) to the output
 * +0x3b8, +0x304 (+0x2fc / +0x300, limits [+0x2f4, +0x2f8]) normalised to the listeners +0x308, +0x35c (+0x354 / +0x358,
 * limits [+0x34c, +0x350]) normalised to the listeners +0x360. */
void rx_world_rider_ragdoll_blend(rx_addr ra, uint32_t freeze) {
    const rx_addr entry = rx_sp();
    const rx_addr arg_slot = entry + 4;
    rx_set_sp(entry - 8);
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f, one = 0.0f;
    const uint32_t state = U32(ra, 0x5bab8);
    if (state == 0) {
        x = 0.5f /* 0x9322cc */;
        y = F32(0, 0x009d78b0);
        z = F32(0, 0x00d94ab0);
        w = x;
        one = 1.0f /* 0x931f24 */;
    } else if (state == 1) {
        x = 0.4f /* 0x935860 */;
        y = F32(0, 0x009d78a0);
        w = 0.5f /* 0x9322cc */;
        z = x;
        one = 1.0f /* 0x931f24 */;
    }
    SF32(entry, -4, one);
    {
        const float v = rider_clamp_hi_lo(x, F32(ra, 0x3f0), F32(ra, 0x3ec));
        const rx_addr out = U32(ra, 0x3f4);
        if (out != 0) SF32(out, 0, v);
    }
    {
        const float v = rider_clamp_hi_lo(y, F32(ra, 0x3fc), F32(ra, 0x3f8));
        const rx_addr out = U32(ra, 0x400);
        if (out != 0) SF32(out, 0, v);
    }
    if (freeze != 0) {
        SF32(ra, 0x3b4, w);
        SF32(ra, 0x304, z);
        SF32(ra, 0x35c, one);
    } else {
        {
            const float a = F32(ra, 0x3ac), b = F32(ra, 0x3b0);
            const float rate = (a > b) ? b : a;
            const float cur = F32(ra, 0x3b4);
            const float moved = phys_add(phys_mul(phys_div(rate, a), phys_sub(w, cur)), cur);
            SF32(ra, 0x3b4, moved);
            const float v = rider_clamp_hi_lo(moved, F32(ra, 0x3a8), F32(ra, 0x3a4));
            const rx_addr out = U32(ra, 0x3b8);
            if (out != 0) SF32(out, 0, v);
        }
        {
            const float a = F32(ra, 0x2fc), b = F32(ra, 0x300);
            const float rate = (a > b) ? b : a;
            const float cur = F32(ra, 0x304);
            const float moved = phys_add(phys_mul(phys_sub(z, cur), phys_div(rate, a)), cur);
            SF32(ra, 0x304, moved);
            const float hi = F32(ra, 0x2f8), lo = F32(ra, 0x2f4);
            const float v = rider_clamp_hi_lo(moved, hi, lo);
            SF32(arg_slot, 0, phys_div(phys_sub(v, lo), phys_sub(hi, lo)));
            WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x308, arg_slot));
        }
        {
            const float a = F32(ra, 0x354), b = F32(ra, 0x358);
            const float rate = (a > b) ? b : a;
            const float cur = F32(ra, 0x35c);
            const float moved = phys_add(phys_mul(phys_sub(F32(entry, -4), cur), phys_div(rate, a)), cur);
            SF32(ra, 0x35c, moved);
            const float hi = F32(ra, 0x350), lo = F32(ra, 0x34c);
            const float v = rider_clamp_hi_lo(moved, hi, lo);
            SF32(arg_slot, 0, phys_div(phys_sub(v, lo), phys_sub(hi, lo)));
            WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x360, arg_slot));
        }
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x00621850, rx_world_rider_ragdoll_blend)

/* 006217a0 RiderAnim_UpdateRagdoll (ESI = ra; stdcall dt; RET 4; no frame): the two outputs +0x58c (limits [+0x584, +0x588]) and
 * +0x598 (limits [+0x590, +0x594]) grow by 3 dt, then the blend targets (00621850, not frozen), and with no pose override
 * (+0xaf10 != 0 runs it) the crash animation update 00620a60 (dt passes through x87: fld / fstp). */
void rx_world_rider_update_ragdoll(rx_addr ra, float dt) {
    const rx_addr entry = rx_sp();
    const float step = phys_mul(dt, 3.0f /* 0x92fe48 */);
    {
        const rx_addr out = U32(ra, 0x58c);
        const float v = rider_clamp_hi_lo(phys_add(out != 0 ? F32(out, 0) : 0.0f, step), F32(ra, 0x588), F32(ra, 0x584));
        if (out != 0) SF32(out, 0, v);
    }
    {
        const rx_addr out = U32(ra, 0x598);
        const float v = rider_clamp_hi_lo(phys_add(out != 0 ? F32(out, 0) : 0.0f, step), F32(ra, 0x594), F32(ra, 0x590));
        if (out != 0) SF32(out, 0, v);
    }
    SU32(entry, -4, 0);                                     /* push 0 */
    WORLD_CALL_SP(4, rx_world_rider_ragdoll_blend(ra, 0));
    if (U32(ra, 0xaf10) != 0) {
        const uint32_t dt_bits = world_x87_pass(rx_fb(dt));
        SU32(entry, -4, dt_bits);                           /* push ecx; fstp [esp] */
        SU32(entry, -8, ra);                                /* push esi */
        WORLD_CALL_SP(8, rx_rider_crash_update(ra, rx_bf(dt_bits)));
    }
}
RX_FUNCTION(0x006217a0, rx_world_rider_update_ragdoll)

/* 0061e3d0 RiderAnim_UpdateLocomotion (EAX = ra; stdcall dt; RET 4; frame sub 0x14; push edi): the lean angle +0x5bb48 =
 * asin(+0x5bad4) (rider_asin); the four steer inputs a b c d (+0x5b528 .. +0x5b534, or 0 while +0x5bb38 is set). Without a
 * pose override (+0xaf10): a scripted mode (+0x5bb50 == 7) or a crash (+0x5b2e0) runs 0061db10 only; a trick (byte +0xc8cc
 * with +0x5bb4c or +0x5b2dc) runs the trick update, the body weight and the limb ranges only. Otherwise the steer (00620110),
 * steer weight, arm chains (unless +0x5b2e8), body weight, limb ranges and crouch, and without a pose override the gesture
 * (0061eac0) and the state switcher (0061f160). */
void rx_world_rider_update_locomotion(rx_addr ra, float dt) {
    world_frame f = world_frame_flat(1, 0x14);
    SF32(ra, 0x5bb48, rider_asin(F32(ra, 0x5bad4)));
    float a = 0.0f, b = 0.0f, c = 0.0f, d = 0.0f;
    if (U32(ra, 0x5bb38) == 0) {
        c = F32(ra, 0x5b530); d = F32(ra, 0x5b534); a = F32(ra, 0x5b528); b = F32(ra, 0x5b52c);
    }
    SF32(f.esp, 8, a); SF32(f.esp, 0xc, b); SF32(f.esp, 0x10, c); SF32(f.esp, 0x14, d);
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));   /* fld / fstp of the argument */
    if (U32(ra, 0xaf10) == 0) {
        if (U32(ra, 0x5bb50) == 7 || U32(ra, 0x5b2e0) != 0) {
            WORLD_CALL_SP(4, rx_world_rider_update_scripted_lean(ra, dtp));
            world_frame_leave(&f);
            return;
        }
        if (U8(ra, 0xc8cc) != 0 && (U32(ra, 0x5bb4c) != 0 || U32(ra, 0x5b2dc) != 0)) {
            WORLD_CALL_SP(4, rx_world_rider_update_trick(ra, dtp));
            WORLD_CALL_SP(0x14, rx_world_rider_update_body_weight(ra, dtp));
            WORLD_CALL_SP(0, rx_world_rider_update_limb_ranges(ra));
            world_frame_leave(&f);
            return;
        }
    }
    WORLD_CALL_SP(0x10, rx_world_rider_update_steer(ra, a, b, c, d));
    WORLD_CALL_SP(4, rx_world_rider_update_steer_weight(ra, dtp));
    if (U32(ra, 0x5b2e8) == 0) WORLD_CALL_SP(0, rx_world_rider_update_arm_chains(ra));
    WORLD_CALL_SP(0x14, rx_world_rider_update_body_weight(ra, dtp));
    WORLD_CALL_SP(0, rx_world_rider_update_limb_ranges(ra));
    WORLD_CALL_SP(0x10, rx_world_rider_update_crouch(ra));
    if (U32(ra, 0xaf10) == 0) {
        WORLD_CALL_SP(4, rx_world_rider_update_gesture(ra, dtp));
        WORLD_CALL_SP(8, rx_world_rider_update_state(ra, dtp));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061e3d0, rx_world_rider_update_locomotion)

#ifndef RX_ORACLE
static int capture_deferred(rx_addr sk);
#else
static int capture_deferred(rx_addr sk) { (void)sk; return 0; }
#endif
/* 0061c210 RiderAnimation::vf19 (thiscall ra; dt; RET 4; frame push ebp; and esp, -16; sub 0x24; 3 pushes): the rider
 * animation's frame update (only while +0x5bb5c is set):
 *   a pose override (+0xaf08): 0061c0c0 (EAX = ra; dt);
 *   else by animation mode +0xaee0: 2 the ragdoll (006217a0 ESI = ra) and the blend times = dt; 1 the wreck lean (the clamped
 *   output +0x58c += 3 dt), the blend times, the steer (00620110 with +0x5b528 .. +0x5b534) and the pose weights; 0 the ride:
 *   the turn timer +0x5bac4, the blend times (+0x410 .. +0x458 too outside pose mode), then the scripted update (mode
 *   +0x5b57c != 0xc), or the locomotion, blends and lean (no wreck class +0xaf00), or the idle; both wreck states' reactions
 *   (00632aa0 on +0x5b760 / +0x5b850), the blend overlay, the scripted start and the pose weights.
 * Then +0x5b538 = dt; under the lock 0xaf6c98 the rider's entry in the frame tables (0xa9411c / 0xa940bc, slot +0x2bc, unless
 * +0x2c8) and the frame count 0xa94080; the head position into the vehicle's camera targets (00623bd0, double-buffered by the
 * frame parity); the skeleton (+0x5d0) either simulated (byte +0x5b578: 00425600 / 00425800 / 00426930, then +0x5bb68 =
 * (+0x944 or byte +0x658) unless already set) or captured from the pose (00426a50) with the body hit check (00624560). */
void rx_world_rider_animation_vf19(rx_addr ra, float dt) {
    world_frame f = world_frame_enter(0x24, 3, 1);
    const rx_addr S = f.esp;
    if (U32(ra, 0x5bb5c) == 0) { world_frame_leave(&f); return; }
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));   /* the argument through fld / fstp */
#define BLEND_TIMES() do { SF32(ra, 0x384, dt); SF32(ra, 0x2d4, dt); SF32(ra, 0x300, dt); SF32(ra, 0x32c, dt); SF32(ra, 0x358, dt); } while (0)
    if (U32(ra, 0xaf08) != 0) {
#ifdef RX_ORACLE
        WORLD_CTX(0x0061c0c0, &WCTX(.eax = ra, .ebx = ra), 0, rx_fb(dtp));
#else
        /* Actual0061c242 call: EAX=self, materialized dt, RET4. The typed
         * callee retains its entry SP; this caller models the return. */
        rx_addr override_sp=rx_sp();
        SU32(override_sp-4,0,rx_fb(dtp));SU32(override_sp-8,0,0x61c247);
        rx_set_sp(override_sp-8);fx_set_reg(FX_EAX,ra);
        rx_rider_pose_override(ra,dtp);
        if(rx_sp()!=override_sp-8)abort();rx_set_sp(override_sp);
#endif
    } else {
        const uint32_t mode = U32(ra, 0xaee0);
        if (mode == 2) {
            WORLD_CALL_SP(4, rx_world_rider_update_ragdoll(ra, dtp));
            SF32(U32(ra, 0x5bc), 0x6c, dt);
            BLEND_TIMES();
            SF32(ra, 0x3b0, dt);
        } else if (mode == 1) {
            const rx_addr out = U32(ra, 0x58c);
            float v = phys_add(out ? F32(out, 0) : 0.0f, phys_mul(dt, 3.0f /* 0x92fe48 */));
            if (v >= F32(ra, 0x588)) v = F32(ra, 0x588);
            if (F32(ra, 0x584) >= v) v = F32(ra, 0x584);
            if (U32(ra, 0x58c) != 0) SF32(U32(ra, 0x58c), 0, v);
            BLEND_TIMES();
            const float a = rx_bf(world_x87_pass(U32(ra, 0x5b528))), b = rx_bf(world_x87_pass(U32(ra, 0x5b52c)));
            const float c = rx_bf(world_x87_pass(U32(ra, 0x5b530))), d = rx_bf(world_x87_pass(U32(ra, 0x5b534)));
            WORLD_CALL_SP(0x10, rx_world_rider_update_steer(ra, a, b, c, d));
            WORLD_CALL_SP(4, rx_world_rider_update_pose_weights(ra, dtp));
        } else if (mode == 0) {
            SF32(ra, 0x5bac4, U32(ra, 0x5babc) != 0 ? phys_add(dt, F32(ra, 0x5bac4)) : 0.0f);
            SF32(U32(ra, 0x5bc), 0x6c, dt);
            BLEND_TIMES();
            if (U32(ra, 0x5bab8) == 0) { SF32(ra, 0x410, dt); SF32(ra, 0x428, dt); SF32(ra, 0x440, dt); SF32(ra, 0x458, dt); }
            SF32(ra, 0x3b0, dt);
            if (U32(ra, 0x5b57c) != 0xc) WORLD_CALL_SP(4, rx_world_rider_update_scripted(ra, dtp));
            else if (U32(ra, 0xaf00) == 0) {
                WORLD_CALL_SP(4, rx_world_rider_update_locomotion(ra, dtp));
                WORLD_CALL_SP(4, rx_world_rider_update_blends(ra, dtp));
                WORLD_CALL_SP(4, rx_world_rider_update_lean(ra, dtp));
            } else WORLD_CALL_SP(4, rx_world_rider_update_idle(ra, dtp));
            WORLD_CALL_SP(0, rx_world_wreck_state_react(ra + 0x5b760, ra));
            WORLD_CALL_SP(0, rx_world_wreck_state_react(ra + 0x5b850, ra));
            WORLD_CALL_SP(0, rx_world_rider_blend_overlay(ra, dt));
            WORLD_CALL_SP(0, rx_world_rider_update_scripted_start(ra));
            WORLD_CALL_SP(4, rx_world_rider_update_pose_weights(ra, dtp));
        }
    }
#undef BLEND_TIMES
    RX_OS(0x0091600c, 0x00af6c98);   /* EnterCriticalSection */
    SF32(ra, 0x5b538, dt);
    if (U32(ra, 0x2c8) == 0) {
        const uint32_t slot = U32(ra, 0x2bc);
        SU32(rx_rc_tab(0x00a9411c, 4, slot), 0, U32(ra, 0x2c4));                    /* (w-rc-core) slots 12..31 in side storage */
        SU32(rx_rc_tab(0x00a940bc, 4, slot), 0, rx_rc_tab(0x00af6c08, 4, slot));
    }
    SU32(0, 0x00a94080, U32(0, 0x00a94080) + 1);
    RX_OS(0x009160f8, 0x00af6c98);   /* LeaveCriticalSection */
    const rx_addr veh = U32(ra, 0xaf1c);
    if (veh != 0 && U8(ra, 0xa2) != 0 && U32(U32(veh, 0x190), 0x9ac) != 0) {
        WORLD_CALL_SP(4, rx_rider_anim_head_transform(ra, S + 0x10, S + 0x20));   /* RiderAnim_HeadTransform */
        const uint32_t parity = U32(U32(U32(0, 0x00a95844), 0x10), 0x58) & 1u;
        const rx_addr cam = U32(U32(ra, 0xaf1c), 0x190) + 0x9b0 + (parity << 6);
        SU32(cam, 0x30, U32(S, 0x10)); SU32(cam, 0x34, U32(S, 0x14)); SU32(cam, 0x38, U32(S, 0x18));
    }
    const rx_addr sk = ra + 0x5d0;
    if (U8(ra, 0x5b578) != 0) {
        const rx_addr v = U32(U32(ra, 0x5b554), 0x190);
        SU32(ra, 0x66c, v);
        { const rx_regs caller = world_ctx_regs(&WCTX(.esi = sk, .ecx = v, .ebx = ra)); WORLD_CALL_SP(4, (void)phys_rider_crash_update(sk, dtp, &caller)); }
        WORLD_CALL_SP(8, (void)phys_rider_rest_timer(sk, dtp));
        { const rx_regs caller = world_ctx_regs(&WCTX(.ecx = sk, .esi = sk, .ebx = ra)); WORLD_CALL_SP(0, phys_rider_pose_nodes(sk, &caller, NULL)); }
        if (U32(ra, 0x5bb68) == 0) SU32(ra, 0x5bb68, (U32(ra, 0x944) != 0 || U8(ra, 0x658) != 0) ? 1u : 0u);
    } else {
        if (!capture_deferred(sk)) {
            RX_WATCH_MARK(1, sk);
            WORLD_CALL_SP(0, phys_rig_capture_bone_poses(sk));   /* Rig_CaptureBonePoses */
            RX_WATCH_MARK(2, sk);
        }
        WORLD_CALL_SP(0, rx_world_rider_check_body_hits(ra));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061c210, rx_world_rider_animation_vf19)

/* ---- w-jobthreads3: the riders' bone-pose captures (Rig_CaptureBonePoses 00426a50) as parallel jobs (loop_jobs.h) ----
 * In place, each rider's vf19 in the post-step listeners (World_PostListeners 00769ab0 -> the vehicles' vf29 / vf35) ends with
 * the capture of its skeleton's bone poses. While the listeners run, the capture is queued instead and the queue runs when they
 * are done (rx_rider_capture_flush, before the contact events), one job per rider (contexts RX_JOB_CTX_CAP + k), in the queue
 * order. A capture reads its own rider's node set (the world matrices evaluated on the way) and writes its rig's bone poses
 * (+0x400 / +0xe80); a -DRX_WATCH run (core/plan/QUEUE.md w-jobthreads3) checks that nothing between the in-place capture and
 * the end of the listeners reads what it writes or writes what it reads. */
#ifndef RX_ORACLE
#define CAP_MAX 16   /* (contexts RX_JOB_CTX_CAP .. + 15; 12 riders at most) */
static rx_addr g_cap[CAP_MAX];
static int g_cap_n, g_cap_on;
void rx_rider_capture_begin(void) { g_cap_on = rx_jobs_on(RX_JOB_CAPTURE) && !rx_jobs_in_job(); g_cap_n = 0; }
static int capture_deferred(rx_addr sk) {
    if (!g_cap_on || g_cap_n == CAP_MAX) return 0;
    g_cap[g_cap_n++] = sk;
    return 1;
}
static void capture_body(rx_addr k) { WORLD_CALL_SP(0, phys_rig_capture_bone_poses(g_cap[k])); }
void rx_rider_capture_flush(void) {
    if (g_cap_on && g_cap_n > 0) {
        rx_job jobs[CAP_MAX];
        for (int k = 0; k < g_cap_n; k++) {
            jobs[k].body = capture_body; jobs[k].arg = (rx_addr)k; jobs[k].ctx = RX_JOB_CTX_CAP + k; jobs[k].tls = U32(rx_teb(), 0x2c);
        }
        rx_jobs_run(jobs, g_cap_n);
    }
    g_cap_on = 0; g_cap_n = 0;
}
#else
void rx_rider_capture_begin(void) {}
void rx_rider_capture_flush(void) {}
#endif

/* ---- 0061f160 helpers: the pose property (+0x578 ShortProperty, current id u16 [+0x580]) and the clamped blend output
 *      +0x598 reset to clamp(0, +0x590, +0x594) ---- */
static uint32_t pose_id(rx_addr ra) { const rx_addr p = U32(ra, 0x580); return p ? U16(p, 0) : 0; }
static void pose_set(rx_addr ra, rx_addr id) { WORLD_CALL_SP(0, rx_world_short_property_set(ra + 0x578, id)); }
static void pose_blend_reset(rx_addr ra) {
    const float hi = F32(ra, 0x594), lo = F32(ra, 0x590);
    float c = hi;
    if (!(0.0f >= hi)) c = 0.0f;
    if (lo >= c) c = lo;
    if (U32(ra, 0x598) != 0) SF32(U32(ra, 0x598), 0, c);
}
/* the boost state back to the ride (trick state 4, timer 1.5) */
static void trick_state_reset(rx_addr ra) {
    if (U32(ra, 0x5b594) == 1) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
    const uint32_t old = U32(ra, 0x5b590);
    SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
    SU32(ra, 0x5b594, old);
    SU32(ra, 0x5b590, 4);
}
static float listeners_get(rx_addr v) { float r; WORLD_CALL_SP(4, r = rx_world_float_listeners_get(v)); return r; }
static void fade_in(rx_addr ra, uint32_t sel) { WORLD_CALL_SP(4, rx_world_rider_chains_fade_in(ra, sel, 0, 0.25f /* 0x92ff84 */)); }
/* the property listener value of the range (+0x538, +0x53c) / (+0x4c0, +0x4c4) as 00621100 / 006211d0 compute it: -max when
 * the max is below 0, else (-min + max) when the min is above 0, else 0 (the comiss / jbe ladders take a NaN on the 0 side) */
static float range_listener_value(rx_addr ra, uint32_t min_off, uint32_t max_off) {
    const float c = F32(0, 0x009774a8);   /* -0.0 */
    const float hi = F32(ra, max_off);
    if (0.0f > hi) return phys_sub(c, hi);
    const float lo = F32(ra, min_off);
    if (lo > 0.0f) return phys_add(phys_sub(c, lo), hi);
    return 0.0f;
}
/* the dirty byte +0x57c set and the pose property +0x580 written with the id at `id_off` (the two 00621xxx pose setters write
 * the property's u16 directly; 0063b6c0 is the other path the 0061f160 ladder uses) */
static void pose_direct(rx_addr ra, uint32_t id_off) {
    if (U32(ra, 0x57c) != 0) SU8(U32(ra, 0x57c), 0, 1);
    if (U32(ra, 0x580) != 0) SU16(U32(ra, 0x580), 0, U16(ra, id_off));
}

/* 00621100 RiderAnim_SlowPose (ESI = ra): the slow-roll pose: clear the slow-roll timer +0x5bb58, the property listener +0x4d0
 * gets the value of the range (+0x4c0, +0x4c4), the dirty byte +0x57c is set, the pose property +0x580 takes +0x5a8, the blend
 * output +0x598 is snapped to clamp(0) and chain 5 fades out over 0.25 (unless this is the wrecked rider, +0x5bab8). */
void rx_world_rider_slow_pose(rx_addr ra) {
    world_frame f = world_frame_flat(1, 0);   /* push ecx */
    SF32(ra, 0x5bb58, 0.0f);
    SF32(f.esp, 0, range_listener_value(ra, 0x4c0, 0x4c4));
    WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x4d0, f.esp));
    pose_direct(ra, 0x5a8);
    pose_blend_reset(ra);
    if (U32(ra, 0x5bab8) == 0) WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 5, 0.25f /* 0x92ff84 */, 0));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00621100, rx_world_rider_slow_pose)

/* 006211d0 RiderAnim_CrouchPose (EAX = ra): enter the crouch pose: +0xaf54 = 1, the lean property listener +0x548 gets the
 * value of the range (+0x538, +0x53c), the pose property +0x580 takes +0x5a2 when the lean threshold +0xaf78 is above the lean
 * property +0x38c, else +0x5a4, chain 5 fades out over 0.25 (unless wrecked), and the trick state goes back to the ride:
 * +0x5b594 = the old +0x5b590 (its +0x5b5e8 / +0x5b5ec cleared when it was 1), +0x5b590 = 3, +0x5b640 = 1.5. */
void rx_world_rider_crouch_pose(rx_addr ra) {
    world_frame f = world_frame_flat(1, 8);   /* sub esp, 8; push esi */
    SU32(ra, 0xaf54, 1);
    SF32(f.esp, 8, range_listener_value(ra, 0x538, 0x53c));
    WORLD_CALL_SP(8, rx_world_float_listeners_set(ra + 0x548, f.esp + 8));
    const float lean = listeners_get(ra + 0x38c);
    pose_direct(ra, F32(ra, 0xaf78) > lean ? 0x5a2u : 0x5a4u);
    if (U32(ra, 0x5bab8) == 0) WORLD_CALL_SP(8, rx_world_rider_chains_fade_out(ra, 5, 0.25f /* 0x92ff84 */, 0));
    if (U32(ra, 0x5b594) == 1) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
    const uint32_t old = U32(ra, 0x5b590);
    SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
    SU32(ra, 0x5b594, old);
    SU32(ra, 0x5b590, 3);
    pose_blend_reset(ra);
    world_frame_leave(&f);
}
RX_FUNCTION(0x006211d0, rx_world_rider_crouch_pose)

/* 0061f160 RiderAnim_UpdateState (stdcall ra, dt; RET 8; frame push ebp; and esp, -16; sub 0x54, cookie): the pose state
 * switcher, only with both blend outputs (+0x598, +0x58c) at exactly 1:
 *   a turn (+0x5babc): end the pose override and go back to the base pose +0x5a0 (unless it or +0x5aa is current);
 *   boost (+0x5bb54 == 1): above the boost speed (+0xaf68 > +0x5bb30) the base pose with the trick state reset; else with
 *     the crouch flag +0xaf54 the crouch poses +0x5a2 / +0x5a4 by the lean property (+0x38c vs +0xaf78), without it 006211d0;
 *   moving (sim +0x90 == 0 and speed +0x5bb30 > 3): the slow-roll timer +0x5bb58 (00621100 after 1 s within the band
 *     (0x9d789c, 0x935aac)) or the base pose by the speed limit +0xaf60 (arm chains 5 in);
 *   standing: the bike sim's pose override flags (+0x2e74 ends it), the running idle clip (+0x5b2e8: its events between the
 *     blend values (00620fc0), its key frame blend, the pose from the clip into the vehicle sim (vf10)), and a new random idle
 *     clip (rand, msvcr90 [0x91624c]) from the scripted set (+0xc8c8 records of 40 at +0xc848) or the idle set (+0xc594 at
 *     +0xc454, with the anti-repeat counter +0x5b2ec), its clip (00830130), pose and blend range; or back to the idle pose
 *     +0x5a6 (arm side 0061e950, chains 4 in). */
void rx_world_rider_update_state(rx_addr ra, float dt) {
    world_frame f = world_frame_enter(0x54, 3, 1);
    const rx_addr S = f.esp, A = S + 0x60;
    SU32(A, -4, U32(0, 0x009b45e4) ^ (A - 0x54));
    const rx_addr dt_slot = f.ebp + 0xc;
    SF32(dt_slot, 0, dt);
    const rx_addr p598 = U32(ra, 0x598), p58c = U32(ra, 0x58c);
    if (p598 == 0 || !(F32(p598, 0) == 1.0f) || p58c == 0 || !(F32(p58c, 0) == 1.0f)) goto out;
    if (U32(ra, 0x5babc) != 0) {
        WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
        SU32(ra, 0x5b2e4, 0);
        if (pose_id(ra) == U16(ra, 0x5a0) || pose_id(ra) == U16(ra, 0x5aa)) goto out;
        pose_set(ra, ra + 0x5a0);
        pose_blend_reset(ra);
        goto out;
    }
    const uint32_t move = U32(ra, 0x5bb54);
    if (move == 1) {
        WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
        SU32(ra, 0x5b2e4, 0);
        if (F32(ra, 0xaf68) > F32(ra, 0x5bb30)) {
            trick_state_reset(ra);
            SU32(ra, 0xaf54, 0);
            if (pose_id(ra) == U16(ra, 0x5a0)) goto out;
            pose_blend_reset(ra);
            pose_set(ra, ra + 0x5a0);
            goto out;
        }
        if (U32(ra, 0xaf54) == 0) { WORLD_CALL_SP(0, rx_world_rider_crouch_pose(ra)); goto out; }
        if (listeners_get(ra + 0x38c) > F32(ra, 0xaf78) && pose_id(ra) == U16(ra, 0x5a2)) {
            pose_blend_reset(ra);
            pose_set(ra, ra + 0x5a4);
            goto out;
        }
        if (!(F32(ra, 0xaf78) > listeners_get(ra + 0x38c)) || pose_id(ra) != U16(ra, 0x5a4)) goto out;
        pose_blend_reset(ra);
        pose_set(ra, ra + 0x5a2);
        goto out;
    }
    if (U32(ra, 0xaf54) != 0 && move == 0) trick_state_reset(ra);
    SU32(ra, 0xaf54, 0);
    rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
    if (U32(sim, 0x90) == 0 && F32(ra, 0x5bb30) > 3.0f /* 0x92fe48 */) {
        WORLD_CALL_SP(4, rx_world_rider_end_pose_override(ra));
        const float speed = F32(ra, 0x5bb30);
        SU32(ra, 0x5b2e4, 0);
        if (20.0f /* 0x935aac */ > speed && speed > F32(0, 0x009d789c) && U32(ra, 0x5bab8) != 1) {
            uint32_t cur;
            WORLD_CALL_SP(0, cur = rx_world_rider_pose_is_current(ra));
            if (cur != 0) goto out;
            const float t = phys_add(F32(dt_slot, 0), F32(ra, 0x5bb58));
            SF32(ra, 0x5bb58, t);
            if (t > 1.0f) WORLD_CALL_SP(0, rx_world_rider_slow_pose(ra));
            goto out;
        }
        SF32(ra, 0x5bb58, 0.0f);
        if (pose_id(ra) != U16(ra, 0x5a0) && F32(ra, 0xaf60) > speed) {
            if (U32(ra, 0xaf54) != 0 && U32(ra, 0x5bab8) == 0) fade_in(ra, 5);
            pose_set(ra, ra + 0x5a0);
            pose_blend_reset(ra);
        }
        if (pose_id(ra) == U16(ra, 0x5a0) || !(F32(ra, 0x5bb30) > F32(ra, 0xaf60))) goto out;
        if (U32(ra, 0x5bab8) == 0) fade_in(ra, 5);
        pose_set(ra, ra + 0x5a0);
        pose_blend_reset(ra);
        goto out;
    }
    {
        const uint32_t hold = U32(sim, 0x2e78), override = U32(sim, 0x2e74);
        SU32(S, 0x10, hold); SU32(S, 0x14, override);
        if (override != 0) {
            SU32(sim, 0x2de4, 1);
            WORLD_CALL_SP(0, bike_end_pose_override(U32(U32(ra, 0xaf1c), 0x234)));   /* Simulation_EndPoseOverride */
        }
        int to_idle = 0;
        if (U32(ra, 0x5b2e8) != 0) {
            const float w1 = listeners_get(ra + 0x46c);
            SF32(S, 0x18, w1);
            WORLD_CALL_SP(4, rx_world_clamped_float_add(ra + 0x464, dt_slot));
            const float w2 = listeners_get(ra + 0x46c);
            SF32(S, 0x1c, w2);
            WORLD_CALL_SP(0x10, rx_world_rider_play_clip_events(ra, U32(ra, 0xc598), rx_bf(world_x87_pass(rx_fb(w1))),
                                                                rx_bf(world_x87_pass(rx_fb(w2)))));
            const float w3 = listeners_get(ra + 0x46c);
            const rx_addr e = U32(ra, 0xc934);
            SF32(e, 0xdb4, w3);
            if (U32(e, 0xda8) != 0) WORLD_CALL_SP(0, rx_world_keyframe_blend_select(e));
            WORLD_CALL_SP(0, phys_pose_clear(S + 0x20));
            WORLD_CALL_SP(4, rx_world_get_three_vectors(U32(ra, 0xc934), S + 0x20));
            const rx_addr sim2 = U32(U32(ra, 0xaf1c), 0x234);
            const rx_addr fn = U32(U32(sim2, 0), 0x28);
            WORLD_ORIG(fn, RX_THISCALL, sim2, S + 0x20);
            if (override == 0) {
                if (!(listeners_get(ra + 0x46c) >= F32(U32(ra, 0xc598), 0x94))) goto out;
            } else {
                SU32(ra, 0xc598, 0);
                SU32(ra, 0x5b2e8, 0);
                fade_in(ra, 4);
                SU32(ra, 0xc934, 0);
                to_idle = 1;
            }
        } else if (override != 0) to_idle = 1;
        if (!to_idle && U32(ra, 0x5b2e8) == 0 && pose_id(ra) != U16(ra, 0x5a6)) to_idle = 1;
        if (to_idle) {
            pose_blend_reset(ra);
            pose_set(ra, ra + 0x5a6);
            if (U32(ra, 0x5bab8) != 1) WORLD_CALL_SP(0, rx_world_rider_arm_side(ra));
            fade_in(ra, 4);
            goto out;
        }
        if (hold == 0 && U32(U32(U32(ra, 0xaf1c), 0x234), 0x2de0) == 0) goto out;
        const uint32_t n_idle = U32(ra, 0xc594);
        if (n_idle == 0 && U32(ra, 0xc8c8) == 0) goto out;
        SU32(ra, 0xc598, 0);
        int32_t k;
        if (U32(ra, 0x5b548) != 0 && U32(ra, 0xc8c8) != 0) {
            k = crt_rand() % I32(ra, 0xc8c8);   /* rand (IAT 0x91624c) */
            pose_set(ra, ra + 40u * (uint32_t)k + 0xc848);
        } else {
            if (n_idle == 0 || U32(ra, 0x5bb60) == 0) goto out;
            if (U32(ra, 0x5b2e4) == 0) {
                k = I32(ra, 0x5b2f0);
                SU32(ra, 0x5b2ec, U32(ra, 0x5b2ec) + 1);
                SU32(ra, 0x5b2e4, 1);
            } else {
                SU32(ra, 0x5b2e4, 0);
                k = crt_rand() % (int32_t)n_idle;
                if (I32(ra, 0x5b2f0) == k) {
                    if (U32(ra, 0x5b2ec) > 2) {
                        for (int i = 0; i < 10 && I32(ra, 0x5b2f0) == k; i++) k = crt_rand() % I32(ra, 0xc594);
                        SU32(ra, 0x5b2ec, 0);
                    } else SU32(ra, 0x5b2ec, U32(ra, 0x5b2ec) + 1);
                } else SU32(ra, 0x5b2ec, 0);
            }
            const rx_addr rec = ra + 40u * (uint32_t)k;
            uint32_t clip;
            WORLD_CALL_SP(4, clip = rx_world_object_query_248(U32(ra, 0x144), rec + 0xc454));
            SU32(ra, 0xc598, clip);
            if (pose_id(ra) != U16(rec, 0xc474)) {
                pose_set(ra, rec + 0xc474);
                pose_blend_reset(ra);
            }
        }
        if (k < 0) goto out;
        SU32(ra, 0x5b2e8, 1);
        SF32(S, 0x10, 0.0f);
        WORLD_CALL_SP(0, rx_world_clamped_float_set(ra + 0x464, S + 0x10));
        const rx_addr e = ra + (uint32_t)k * 0xe10u + 0x464b0;
        SU32(ra, 0xc934, e);
        const float len = F32(e, 0xdb0);
        if (!(0.0f >= len)) { SF32(ra, 0x464, 0.0f); SF32(ra, 0x468, len); }
        if (U32(0, 0x00a942b4) == 2) goto out;
        sim = U32(U32(ra, 0xaf1c), 0x234);
        SU32(sim, 0x2de4, 2);
        WORLD_CALL_SP(0, bike_end_pose_override(U32(U32(ra, 0xaf1c), 0x234)));
        const rx_addr sim3 = U32(U32(ra, 0xaf1c), 0x234);
        WORLD_ORIG0(U32(U32(sim3, 0), 0x84), RX_THISCALL, sim3);
    }
out:
    phys_security_check_cookie(U32(A, -4) ^ (A - 0x54));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0061f160, rx_world_rider_update_state)
