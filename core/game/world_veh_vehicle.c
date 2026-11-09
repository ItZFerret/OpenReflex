/* world_veh_vehicle.c -- per-frame helpers of the motocross vehicle (0x004603f0 .. 0x0046197c): a node's world orientation, the
 * terrain-material grip / lean tables, the water wake, the controller rumble and the camera hook.
 * Vehicle (RavenVehicleMotocross, ESI / ECX): +0x150 speed, +0x190 the player, +0x280 the ATV flag, +0x16d4 the simulation;
 * vfuncs +0x94 (out: velocity), +0xa0 (out), +0xac (is airborne / off). core/notes/world.md (veh). */
#include "vehicle_water_wake.h"
#include "world_misc.h"
#include "world_veh.h"
#include "world_node.h"
#include "world_veh_util.h"
#include "world_veh_water.h"
#include "vehicle_atv_wheels.h"
#include "phys_terrain.h"
#include "phys_studio.h"

#ifndef RX_ORACLE
uint32_t rx_crt_atexit(uint32_t handler); /*008f9fbf product exit policy*/
#endif

/* 004603f0 (EAX = node, EDI = position out, stdcall quaternion out; RET 4): the node's world position (Node_GetWorldPosition
 * 007dd3a0: ESI = node, EDI = out), its world matrix evaluated (Node_EvaluateWorld 007db890) and copied from the model's matrix
 * array (model +0x1c, 64 bytes per node index +0xb) into a stack copy, converted to a quaternion (00401740). */
void rx_world_node_world_pose(rx_addr node, rx_addr pos_out, rx_addr q_out) {
    world_frame f = world_frame_enter(0x4c, 1, 1);   /* push ebp; and esp, -16; sub esp, 0x4c; push esi */
    const rx_addr m = f.esp + 0x10;
    WORLD_CALL_SP(0, rx_world_node_get_world_position(node, pos_out));   /* Node_GetWorldPosition 007dd3a0 */
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));             /* Node_EvaluateWorld 007db890 */
    const rx_addr src = U32(U32(node, 0), 0x1c) + ((uint32_t)U8(node, 0xb) << 6);
    for (uint32_t r = 0; r < 4; r++) v4_st(m + 0x10 * r, v4_ld(src + 0x10 * r));
    WORLD_CALL_SP(4, rx_world_matrix3_to_quaternion_to(q_out, m));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004603f0, rx_world_node_world_pose)

/* 00461680 (EDX = point, EDI = the terrain owner, stdcall object; RET 4; xmm0): object +0x224 plus the material's offset
 * (TerrainMaterialAt 007c5120, layer 0: 0 -> 0x931d94, 1 -> 0x931da4, 3 -> [0x9d78a0], 4 -> [0x9cc8a8], 5 -> [0x9cd970],
 * else 0.5). */
float rx_world_terrain_grip(rx_addr point, rx_addr owner, rx_addr obj) {
    uint32_t mat;
    WORLD_CALL_SP(0, mat = phys_terrain_material_at(owner, point, 0, NULL, NULL));
    float k;
    switch (mat) {
    case 0: k = 0.1f /* 0x931d94 */; break;
    case 1: k = 0.3f /* 0x931da4 */; break;
    case 3: k = F32(0, 0x009d78a0); break;
    case 4: k = F32(0, 0x009cc8a8); break;
    case 5: k = F32(0, 0x009cd970); break;
    default: k = 0.5f /* 0x9322cc */; break;
    }
    if (g_rx_ps_grip_on) k = rx_ps_grip(mat, obj, k);   /* PHYSICS STUDIO (port, phys_studio.h): the config's grip table */
    return phys_add(F32(obj, 0x224), k);
}
RX_FUNCTION(0x00461680, rx_world_terrain_grip)

/* 00461760 (EDX = point, EDI = the terrain owner; xmm0): the material factor of layer 1: 0 -> 0, 2 -> 0x935aa8, 3 -> 1, else
 * 0.25. */
rx_v4 rx_world_terrain_layer1_factor(rx_addr point, rx_addr owner) {
    world_frame f = world_frame_flat(1, 0);   /* push ecx */
    uint32_t mat;
    WORLD_CALL_SP(0, mat = phys_terrain_material_at(owner, point, 1, NULL, NULL));
    world_frame_leave(&f);
    if (g_rx_ps_grip_on) {   /* PHYSICS STUDIO (port, phys_studio.h): the config's drag factors (every vehicle) */
        const float k = rx_ps_drag(mat, mat == 0 ? 0.0f : mat == 2 ? 0.75f : mat == 3 ? 1.0f : 0.25f);
        return k == 0.0f ? v4_zero() : v4(k, 0.0f, 0.0f, 0.0f);
    }
    switch (mat) {
    case 0: return v4_zero();
    case 2: return v4(0.75f /* 0x935aa8 */, 0.0f, 0.0f, 0.0f);
    case 3: return v4(1.0f /* 0x931f24 */, 0.0f, 0.0f, 0.0f);
    default: return v4(0.25f /* 0x92ff84 */, 0.0f, 0.0f, 0.0f);
    }
}
RX_FUNCTION(0x00461760, rx_world_terrain_layer1_factor)

/* The water-height singleton (0xd72aac, constructed once by 004242f0 under guard bit 1 of 0xd72aa8, destructor 0x911e90). */
static void water_guard(void) {
    if ((U8(0, 0x00d72aa8) & 1) == 0) {
        SU32(0, 0x00d72aa8, U32(0, 0x00d72aa8) | 1);
        WORLD_CALL_SP(0, (void)rx_water_heights_init());   /* 004242f0 */
#ifdef RX_ORACLE
        WORLD_ORIG(0x008f9fbf, RX_CDECL, 0, 0x00911e90);   /* _atexit */
#else
        WORLD_CALL_SP(4,(void)rx_crt_atexit(0x00911e90));
#endif
    }
}

/* 00461220 (thiscall ctx, stdcall vehicle, wake block, dt; RET 0xc): the vehicle's water wake. The block's four probe points
 * (+0x20 .. +0x50) are tested against the water (007efef0: the point is replaced by the water surface point; flags +0 .. +0xc,
 * depths +0x10 .. +0x1c when the surface is above the point). With any probe in: a splat quad (Wake_AddSplatQuad 007ec280) while
 * the speed exceeds 1 and the depth limit +0x84 exceeds the smallest depth, and once per entry (+0x70) a splat at +0x60
 * (Wake_AddSplat 007ecd80) scaled by the vertical speed and the height above the water (007c5aa0 with the entry ECX). */
void rx_world_water_wake(rx_addr ctx, rx_addr veh, rx_addr W, float dt) {
    world_frame f = world_frame_enter(0x44, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x44; push ebx, esi, edi */
    const rx_addr sp = f.esp, s20 = sp + 0x20, s30 = sp + 0x30;
    SU32(sp, 0x18, ctx); SU32(sp, 0x1c, veh);
    for (uint32_t k = 0; k < 4; k++) {
        const rx_addr p = W + 0x20 + 0x10 * k;
        v4_st(s30, v4_ld(p));
        SU32(W, 4 * k, 0);
        water_guard();
        uint32_t found;
        WORLD_CALL_SP(4, found = rx_world_water_set_height(0x00d72aac, s30));   /* 007efef0 */
        if (found != 0) {
            const rx_v4 d = v4_lane(v4_sub(v4_ld(s30), v4_ld(p)), 1);
            if (v4_all3(v4_cmplt(v4_ld(WORLD_ZERO4), d))) { SF32(W, 0x10 + 4 * k, d.v[0]); SU32(W, 4 * k, 1); }
        }
    }
    /* the smallest depth (the original's compare chain) */
    float dmin = F32(W, 0x1c) > F32(W, 0x18) ? F32(W, 0x18) : F32(W, 0x1c);
    if (dmin > F32(W, 0x14)) dmin = F32(W, 0x14);
    if (dmin > F32(W, 0x10)) dmin = F32(W, 0x10);
    if (U32(W, 0) == 0 && U32(W, 4) == 0 && U32(W, 8) == 0 && U32(W, 0xc) == 0) {
        SU32(W, 0x70, 0);
        world_frame_leave(&f);
        return;
    }
    WORLD_VCALL(veh, 0x94, s20);   /* the velocity */
    float s = dmin;
    if (F32(W, 0x84) > dmin && F32(veh, 0x150) > 1.0f) {
        s = phys_div(phys_sub(F32(veh, 0x150), 1.0f), F32(W, 0x80));
        if (s > 1.0f) s = 1.0f;
        WORLD_CALL_SP(0, (void)rx_atv_water_singleton());
        const rx_addr wake = U32(0, 0x00d72aac);
        if (wake != 0)
            WORLD_CALL_SP(20, rx_wake_add_quad(wake, W + 0x40, W + 0x50,
                      W + 0x20, W + 0x30, s20, rx_fb(phys_mul(phys_mul(s, s), F32(W, 0x7c))), rx_bf(world_x87_pass(rx_fb(dt)))));
    }
    if (U32(W, 0x70) == 0) {
        const float e = phys_sub(-0.0f /* 0x9774a8 */, phys_div(F32(s20, 4), F32(W, 0x94)));
        const float lo = F32(0, 0x009cd9e8);
        s = e >= 1.0f ? 1.0f : (lo < e ? e : lo);
        v4_st(s30, v4_ld(W + 0x60));
        { uint32_t edx; WORLD_CALL_SP(8, (void)phys_terrain_height_at(ctx, s30, &edx)); }
        float h = phys_mul(F32(W, 0x88), phys_sub(F32(W, 0x64), F32(s30, 4)));
        if (h > F32(0, 0x009cd970)) h = F32(0, 0x009cd970);
        h = phys_add(h, 0.1f /* 0x931d94 */);
        SF32(sp, 0x18, h);
        water_guard();
        const rx_addr wake = U32(0, 0x00d72aac);
        if (wake != 0) {
            const float strength = phys_mul(phys_mul(F32(W, 0x8c), h), s);
            const float size = phys_mul(phys_add(s, 1.0f), F32(W, 0x90));
            WORLD_CALL_SP(8, rx_wake_add_splat(wake, W + 0x60, rx_fb(strength), size));
        }
        SU32(W, 0x70, 1);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00461220, rx_world_water_wake)

/* 004617b0 (ESI = vehicle, xmm0 = the impact): the controller rumble of a landing (not in the 0xa942b4 == 2 mode): nothing
 * while airborne (vfunc 0xac); with speed > 10, no ATV flag and a local player (+0x190: +4 active, +0x10 clear, +0x214 rumble
 * enabled), a light motor (0.3 < v < 0.6), a heavy motor (0.6 < v < [0x9cd958]) or both above, through 00548b00 (cdecl
 * player +0x1f0, motor, strength). */
void rx_world_landing_rumble(rx_addr veh, float impact) {
    world_frame f = world_frame_flat(0, 8);   /* sub esp, 8 */
    if (U32(0, 0x00a942b4) == 2) { world_frame_leave(&f); return; }
    SF32(f.esp, 4, impact);
    float v = impact;
    if (WORLD_VCALL0(veh, 0xac) != 0) v = 0.0f;
    float lo = 0.0f, hi = 0.0f;
    SF32(f.esp, 4, lo); SF32(f.esp, 0, hi);
    const rx_addr pl = U32(veh, 0x190);
    if (F32(veh, 0x150) > 10.0f /* 0x92fd88 */ && U32(veh, 0x280) == 0 && pl != 0 && U32(pl, 4) != 0 && U32(pl, 0x10) == 0 &&
        U32(pl, 0x214) != 0) {
        const float heavy = F32(0, 0x009cd958);
        if (v > 0.3f /* 0x931da4 */ && 0.6f /* 0x931da8 */ > v) lo = v;
        if (v > 0.6f && heavy > v) hi = v;
        if (v > heavy) { lo = v; hi = v; }
        SF32(f.esp, 4, lo); SF32(f.esp, 0, hi);
        if (lo > 0.0f) WORLD_CALL_SP(0xc, (void)rx_world_pad_rumble(U32(pl, 0x1f0), 1, world_x87_pass(rx_fb(lo))));   /* 00548b00 */
        if (hi > 0.0f) WORLD_CALL_SP(0xc, (void)rx_world_pad_rumble(U32(U32(veh, 0x190), 0x1f0), 0, world_x87_pass(rx_fb(hi))));
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x004617b0, rx_world_landing_rumble)

/* 004618e0 (ESI = vehicle, stdcall dt; RET 4): with a player whose active camera entity exists (Player_ActiveCameraEntity
 * 0054c020, EDI = player), the camera entity's vfunc 0x84 (dt, &a, &b, airborne) with a = vehicle vfunc 0xa0, b = vfunc 0x94. */
void rx_world_vehicle_camera_update(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0x38, 2, 1);   /* push ebp; and esp, -16; sub esp, 0x38; push ebx; push edi */
    const rx_addr a = f.esp + 0x20, b = f.esp + 0x10;
    const rx_addr pl = U32(veh, 0x190);
    rx_addr ent = 0;
    if (pl != 0) WORLD_CALL_SP(0, ent = rx_world_player_active_camera_entity(pl));   /* 0054c020 */
    if (pl != 0 && ent != 0) {
        WORLD_VCALL(veh, 0xa0, a);
        WORLD_VCALL(veh, 0x94, b);
        rx_addr cam;
        WORLD_CALL_SP(0, cam = rx_world_player_active_camera_entity(U32(veh, 0x190)));
        const uint32_t air = WORLD_VCALL0(veh, 0xac) != 0;
        WORLD_CTX(U32(U32(cam, 0), 0x84), &WCTX(.ecx = cam, .ebx = cam, .esi = veh, .edi = U32(cam, 0) + 0x84), 0,
                  world_x87_pass(rx_fb(dt)), a, b, air);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x004618e0, rx_world_vehicle_camera_update)
