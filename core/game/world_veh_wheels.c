/* world_veh_wheels.c -- 00468300, the motocross vehicle's wheel / contact block of the per-frame update (vf29 calls it):
 * the wheel contact points and the water surface under them, both wheels' deformation and track marks (004607e0 with the
 * parameter block at frame +0x70) and slip marks (00468120 with the block at +0x1a0), the terrain grip / layer factors into the
 * simulation, the body contact points (+0x1650 .. +0x1680) and the wheel-lift forces (+0x8a50 / +0x8a40), the water splash
 * state (+0x1310 .. +0x131c) and the water wake (00461220), and the terrain track contact points of the bike (+0x1020) and of
 * the rider's limbs (+0x1330) (TerrainTracks_UpdateContactSlots / AddContactPoints).
 * The original keeps its parameter blocks in the frame and reuses them between calls (stale fields included), so every local
 * lives at the original's frame offset (S = the frame after push ebp; and esp, -16; sub 0x2b4; 3 pushes).
 * Simulation (+0x16d4): +0x240 / +0x2e0 the wheel normals, +0x2c0 / +0x360 the wheel centres, +0x730 / +0x7c0 rotations,
 * +0x8784 / +0x8780 the wheels' contact objects (vf22 == 5: water), +0x88b0 / +0x8a00 suspension axes, +0x2e80 replay flag. */
#include "world_veh.h"
#include "vehicle_atv_wheels.h"
#include "world_veh_util.h"
#include "world_veh_water.h"
#include "phys_terrain.h"
#include "world_rider.h"
#include "world_node.h"
#include "phys_crt.h"

#ifndef RX_ORACLE
uint32_t rx_crt_atexit(uint32_t handler); /*008f9fbf product exit policy*/
#endif

#define XYZ_MASK 0x0092db80u

/* all three of x, y, z set in a compare mask (the original's lane sum == -3) */
static int all3(rx_v4 m) { return v4_all3(m); }
static rx_v4 cmplt_v(rx_v4 a, rx_v4 b) { return v4_cmplt(a, b); }
/* the 3-lane dot (p0 + p1) + p2 */
static float dot3_012(rx_v4 a, rx_v4 b) { const rx_v4 p = v4_mul(a, b); return phys_add(phys_add(p.v[0], p.v[1]), p.v[2]); }
/* the contact object of a wheel is water (vf22 == 5) */
static int wheel_on_water(rx_addr obj) { return obj != 0 && WORLD_VCALL0(obj, 0x58) == 5; }
/* the static splat constants of the wheel blocks (guard bits in 0xdcf030) */
static void wheel_static(uint32_t bit, rx_addr at, rx_v4 v) {
    const uint32_t g = U32(0, 0x00dcf030);
    if (g & bit) return;
    SU32(0, 0x00dcf030, g | bit);
    v4_st(at, v);
}
/* the normalisation of the inlined 3-lane form (0 for a zero length, the NaN pattern for an infinite one) */
static rx_v4 unit3(rx_v4 v) {
    const float l2 = dot3_012(v, v);
    const rx_v4 L2 = v4_splat(l2), len = v4_sqrt(L2), finite = v4_cmpneq(L2, v4_ld(WORLD_INF4));
    rx_v4 r = v4_and(v4_and(v4_div(v, len), v4_cmpneq(len, v4_zero())), finite);
    return v4_or(r, v4_andn(finite, v4_ld(WORLD_NAN4)));
}
/* the wheel's axis turned to face the contact normal (negated when every lane of the splatted dot is < 0) */
static rx_v4 axis_facing(rx_v4 axis, rx_v4 normal) {
    return all3(cmplt_v(v4_splat(dot3_012(axis, normal)), v4_ld(WORLD_ZERO4))) ? v4_sub(v4_zero(), axis) : axis;
}

static void rx_world_vehicle_wheels_contacts(rx_addr veh, float dtp, rx_addr S);

/* 00468300 (stdcall vehicle, dt; RET 8; frame push ebp; and esp, -16; sub 0x2b4, cookie; 3 pushes): see the file comment.
 * Nothing without a live simulation (+0xb0) or a rider animation (+0x1a0); the wheel blocks are skipped in a replay. */
void rx_world_vehicle_wheels(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0x2b4, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0x2bc, U32(0, 0x009b45e4) ^ (S + 0xc));
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));
    rx_addr sim = U32(veh, 0x16d4);
    if (U32(sim, 0xb0) == 0 || U32(veh, 0x1a0) == 0) goto out;
    if ((U32(0, 0x009e30e0) & 1u) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1u); SU32(0, 0x009e30dc, 0); }
    {
        rx_addr water;
        WORLD_CALL_SP(0, water = rx_world_find_by_controller(0x009e30d4));
        SU32(S, 0xc, water);
    }
    sim = U32(veh, 0x16d4);
    v4_st(S + 0x240, v4_ld(sim + 0x360));
    v4_st(S + 0x240, v4_add(v4_ld(sim + 0x8800), v4_ld(sim + 0x360)));
    v4_st(S + 0x290, v4_ld(sim + 0x2e0));
    v4_st(S + 0x230, v4_ld(sim + 0x2c0));
    v4_st(S + 0x230, v4_add(v4_ld(sim + 0x8950), v4_ld(sim + 0x2c0)));
    v4_st(S + 0x2a0, v4_ld(sim + 0x240));
    if (U32(sim, 0x2e80) == 0) {
        /* ---- both wheels' contact with the water surface, deformation and slip marks ---- */
        const uint32_t water1 = wheel_on_water(U32(sim, 0x8784));
        SU32(S, 0x1c, water1);
        const uint32_t water2 = wheel_on_water(U32(U32(veh, 0x16d4), 0x8780));
        SU32(S, 0x18, water2);
        rx_addr s2 = U32(veh, 0x16d4);
        if (U32(s2, 0x8890) != 0 && !water1) SU32(veh, 0x1588, 1);
        if (U32(s2, 0x89e0) != 0 && !water2) SU32(veh, 0x158c, 1);
        const rx_addr ra = U32(veh, 0x1a0);
        v4_st(S + 0x210, v4_ld(ra + 0x5b770)); v4_st(S + 0x190, v4_ld(ra + 0x5b770));
        v4_st(S + 0x250, v4_ld(veh + 0x15c0));
        v4_st(S + 0x1f0, v4_ld(veh + 0x15d0));
        v4_st(S + 0x60, v4_splat(F32(0, 0x009c4068)));
        v4_st(S + 0x1a0, v4_ld(ra + 0x5b860)); v4_st(S + 0x280, v4_ld(ra + 0x5b860));
        v4_st(S + 0x270, v4_zero()); v4_st(S + 0x220, v4_zero()); v4_st(S + 0x200, v4_zero());
        v4_st(S + 0x290, v4_ld(s2 + 0x2e0));
        v4_st(S + 0x1e0, axis_facing(v4_ld(s2 + 0x88b0), v4_ld(S + 0x290)));
        uint32_t hit1 = 0, hit2 = 0;
        SU32(S, 0x10, 0); SU32(S, 0x14, 0);
        uint32_t edx_unused, found1;
        WORLD_CALL_SP(8, found1 = phys_terrain_height_at(U32(S, 0xc), S + 0x190, &edx_unused));
        if (found1 != 0) {
            const rx_v4 p = v4_ld(S + 0x190);
            const rx_v4 dy = v4_sub(v4_splat(F32(S, 0x214)), v4_splat(p.v[1]));
            v4_st(S + 0x220, dy);
            v4_st(S + 0x250, p);
            if (all3(cmplt_v(dy, v4_ld(S + 0x60)))) {
                hit1 = 1; SU32(S, 0x10, 1);
                if (U32(veh, 0x1590) == 0) {
                    v4_st(veh + 0x15c0, v4_sub(p, v4_ld(S + 0x1e0)));
                    v4_st(S + 0x220, v4_ld(WORLD_ZERO4));
                }
            }
        }
        uint32_t found2;
        WORLD_CALL_SP(8, found2 = phys_terrain_height_at(U32(S, 0xc), S + 0x280, &edx_unused));
        if (found2 != 0) {
            const rx_v4 p = v4_ld(S + 0x280);
            const rx_v4 dy = v4_sub(v4_splat(F32(S, 0x1a4)), v4_splat(p.v[1]));
            v4_st(S + 0x200, dy);
            v4_st(S + 0x1f0, p);
            if (all3(cmplt_v(dy, v4_ld(S + 0x60)))) {
                hit2 = 1; SU32(S, 0x14, 1);
                if (U32(veh, 0x1594) == 0) {
                    v4_st(veh + 0x15d0, v4_sub(p, v4_ld(S + 0x1e0)));
                    v4_st(S + 0x200, v4_ld(WORLD_ZERO4));
                }
            }
        }
        if (hit1) SU32(veh, 0x1590, 1);
        if (hit2) SU32(veh, 0x1594, 1);
        s2 = U32(veh, 0x16d4);
        v4_st(S + 0x1e0, axis_facing(v4_ld(s2 + 0x88b0), v4_ld(S + 0x290)));
        {
            const rx_v4 a = v4_ld(s2 + 0x88b0);
            v4_st(S + 0x260, all3(cmplt_v(v4_splat(F32(s2, 0x920)), v4_ld(WORLD_ZERO4))) ? v4_sub(v4_zero(), a) : a);
        }
        /* the first wheel's parameter block */
        const rx_addr P = S + 0x70;
        SU32(P, 0x00, s2 + 0x8790); SU32(P, 0x04, S + 0x240); SU32(P, 0x0c, S + 0x1e0); SU32(P, 0x14, S + 0x290);
        SU32(P, 0x1c, veh + 0x15f0); SU32(P, 0x08, veh + 0x15e0); SU32(P, 0x34, U32(veh, 0x1578)); SU32(P, 0x10, S + 0x260);
        SU32(P, 0x3c, veh + 0x1588); SU32(P, 0x18, veh + 0x1608); SU32(P, 0x20, U32(veh, 0x15ac)); SU32(P, 0x40, veh + 0x1598);
        SU32(P, 0x30, veh + 0x15f4); SU32(P, 0x24, U32(veh, 0x15b0)); SF32(P, 0x28, 1.0f); SF32(P, 0x2c, 0.5f);
        SU32(P, 0x38, veh + 0x15a8); SU32(P, 0x44, 1);
        wheel_static(0x1, 0x00dcf020, v4_splat(0.4f /* 0x935860 */));
        wheel_static(0x2, 0x00dcf010, v4_splat(F32(0, 0x009cf5b0)));
        wheel_static(0x4, 0x00dcf000, v4_splat(30.0f /* 0x931d98 */));
        wheel_static(0x8, 0x00dceff0, v4_splat(0.2f /* 0x92e388 */));
        wheel_static(0x10, 0x00dcefe0, v4_splat(F32(0, 0x009cc37c)));
        wheel_static(0x20, 0x00dcefd0, v4_splat(0.2f));
        wheel_static(0x40, 0x00dcefc0, v4_splat(250.0f /* 0x977394 */));
        wheel_static(0x80, 0x00dcefb0, v4_splat(F32(0, 0x009cf5b0)));
        wheel_static(0x100, 0x00dcefa0, v4_splat(0.5f));
        wheel_static(0x200, 0x00dcef90, v4_splat(15.0f /* 0x9322d0 */));
        wheel_static(0x400, 0x00dcef80, v4_splat(0.5f));
        wheel_static(0x800, 0x00dcef70, v4_splat(1.0f));
        wheel_static(0x1000, 0x00dcef60, v4_splat(0.4f));
        wheel_static(0x2000, 0x00dcef50, v4_splat(F32(0, 0x009d78dc)));
        wheel_static(0x4000, 0x00dcef40, v4_splat(0.1f /* 0x931d94 */));
        wheel_static(0x8000, 0x00dcef30, v4_splat(3.0f /* 0x92fe48 */));
        wheel_static(0x10000, 0x00dcef20, v4_ld(S + 0x270));
        wheel_static(0x20000, 0x00dcef10, v4_splat(1.0f));
        wheel_static(0x40000, 0x00dcef00, v4_splat(6.0f /* 0x92fe4c */));
        static const uint32_t tables[19] = {0xdcf020, 0xdcf010, 0, 0xdcf000, 0xdceff0, 0xdcefe0, 0xdcefd0, 0xdcefc0, 0xdcefb0,
                                            0xdcefa0, 0xdcef90, 0xdcef80, 0xdcef70, 0xdcef60, 0xdcef50, 0xdcef40, 0, 0, 0};
        for (int i = 0; i < 16; i++) if (tables[i]) SU32(P, 0x48 + 4 * (uint32_t)i, tables[i]);
        SU32(P, 0x50, U32(0, 0x009cc8a8));
        SF32(P, 0x88, 1.0f); SF32(P, 0x8c, 3.0f);
        SU32(P, 0x90, 0xdcef30); SU32(P, 0x94, 0xdcef20); SU32(P, 0x98, 0xdcef10); SU32(P, 0x9c, 0xdcef00);
        WORLD_CALL_SP(0xc, rx_world_wheel_deform_and_track_marks(P, veh, U32(S, 0xc), dtp));
        s2 = U32(veh, 0x16d4);
        v4_st(S + 0x1e0, axis_facing(v4_ld(s2 + 0x8a00), v4_ld(S + 0x2a0)));
        /* the second wheel: the block's changed fields */
        SU32(P, 0x00, s2 + 0x88e0);
        SU32(P, 0x20, U32(veh, 0x15b4)); SU32(P, 0x24, U32(veh, 0x15b8));
        SU32(P, 0x04, S + 0x230); SU32(P, 0x0c, S + 0x1e0); SU32(P, 0x14, S + 0x2a0);
        SU32(P, 0x08, 0); SU32(P, 0x10, 0); SU32(P, 0x18, 0); SU32(P, 0x1c, 0); SF32(P, 0x28, 1.0f); SU32(P, 0x30, 0);
        SU32(P, 0x38, 0); SU32(P, 0x44, 0);
        SF32(P, 0x2c, 0.25f /* 0x92ff84 */); SU32(P, 0x34, U32(veh, 0x157c)); SU32(P, 0x3c, veh + 0x158c); SU32(P, 0x40, veh + 0x159c);
        WORLD_CALL_SP(0xc, rx_world_wheel_deform_and_track_marks(P, veh, U32(S, 0xc), dtp));
        /* the slip marks */
        const rx_addr Q = S + 0x1a0;
        {
            const rx_v4 lim = v4_ld(0x00d61650);
            const rx_v4 r = v4_div(v4_ld(S + 0x220), v4_ld(S + 0x60));
            SU32(Q, 0x00, S + 0x250); SU32(Q, 0x04, veh + 0x15c0); SU32(Q, 0x18, veh + 0x1590);
            SF32(Q, 0x08, phys_sub(lim.v[0], phys_max(F32(WORLD_ZERO4, 0), phys_min(lim.v[0], r.v[0]))));
            SU32(Q, 0x0c, U32(veh, 0x15bc)); SU32(Q, 0x1c, veh + 0x15a0); SF32(Q, 0x10, 1.0f); SU32(Q, 0x14, U32(veh, 0x1580));
        }
        wheel_static(0x80000, 0x00dceef0, v4_ld(S + 0x270));
        wheel_static(0x100000, 0x00dceee0, v4_splat(2.0f));
        SU32(Q, 0x20, 0x00dceef0); SU32(Q, 0x24, 0x00dceee0);
        WORLD_CALL_SP(4, rx_world_slip_track_mark(Q, veh));
        {
            const rx_v4 lim = v4_ld(0x00d61650);
            const rx_v4 r = v4_div(v4_ld(S + 0x200), v4_ld(S + 0x60));
            SU32(Q, 0x04, veh + 0x15d0); SU32(Q, 0x14, U32(veh, 0x1584));
            SF32(Q, 0x08, phys_sub(lim.v[0], phys_max(F32(WORLD_ZERO4, 0), phys_min(lim.v[0], r.v[0]))));
            SU32(Q, 0x0c, U32(veh, 0x15bc)); SU32(Q, 0x18, veh + 0x1594); SU32(Q, 0x00, S + 0x1f0); SF32(Q, 0x10, 1.0f);
            SU32(Q, 0x1c, veh + 0x15a4);
        }
        WORLD_CALL_SP(4, rx_world_slip_track_mark(Q, veh));
        SU32(veh, 0x1598, U32(veh, 0x1588));
        SU32(veh, 0x159c, U32(veh, 0x158c));
        s2 = U32(veh, 0x16d4);
        if (U32(s2, 0x8890) == 0 || water1) SU32(veh, 0x1588, 0);
        if (U32(s2, 0x89e0) == 0 || water2) SU32(veh, 0x158c, 0);
        SU32(veh, 0x15a0, U32(veh, 0x1590));
        SU32(veh, 0x15a4, U32(veh, 0x1594));
        if (U32(S, 0x10) == 0) SU32(veh, 0x1590, 0);
        if (U32(S, 0x14) == 0) SU32(veh, 0x1594, 0);
    }
    rx_world_vehicle_wheels_contacts(veh, dtp, S);
out:
    phys_security_check_cookie(U32(S, 0x2bc) ^ (S + 0xc));
    world_frame_leave(&f);
}

/* the rider-animation independent rest of 00468300 */
static void track_points(rx_addr water, rx_addr pos, rx_addr prev, rx_addr slots, float scale, rx_addr S, float dtp) {
    WORLD_CALL_SP(0x20, rx_world_tracks_add_contact_points(water, pos, prev, slots, scale, S + 0x150, S + 0x1a0, S + 0x110,
                                                           S + 0x20, dtp));
}
static void rx_world_vehicle_wheels_contacts(rx_addr veh, float dtp, rx_addr S) {
    rx_addr sim = U32(veh, 0x16d4);
    /* ---- grip and terrain layer factors of both contact points ---- */
    float g1, h1, g2, h2;
    if (wheel_on_water(U32(sim, 0x8784))) { g1 = 0.5f; h1 = 0.0f; }
    else {
        WORLD_CALL_SP(4, g1 = rx_world_terrain_grip(S + 0x240, U32(S, 0xc), veh));
        rx_v4 l;
        WORLD_CALL_SP(0, l = rx_world_terrain_layer1_factor(S + 0x240, U32(S, 0xc)));
        h1 = l.v[0];
    }
    SF32(S, 0x14, g1);
    SF32(S, 0x10, h1);
    if (wheel_on_water(U32(U32(veh, 0x16d4), 0x8780))) { g2 = 0.5f; h2 = 0.0f; }
    else {
        WORLD_CALL_SP(4, g2 = rx_world_terrain_grip(S + 0x230, U32(S, 0xc), veh));
        SF32(S, 0x18, g2);
        rx_v4 l;
        WORLD_CALL_SP(0, l = rx_world_terrain_layer1_factor(S + 0x230, U32(S, 0xc)));
        h2 = l.v[0];
    }
    SF32(U32(veh, 0x16d4), 0x88a0, phys_mul(phys_add(g2, F32(S, 0x14)), 0.5f));
    SF32(U32(veh, 0x16d4), 0x88a4, phys_mul(phys_add(h2, F32(S, 0x10)), 0.5f));
    sim = U32(veh, 0x16d4);
    SU32(sim, 0x89f0, world_x87_pass(U32(sim, 0x88a0)));
    SU32(sim, 0x89f4, world_x87_pass(U32(sim, 0x88a4)));
    /* ---- the body contact points: the wheel points moved by the flattened up / forward axes ---- */
    v4_st(veh + 0x1650, v4_ld(S + 0x230)); v4_st(veh + 0x1660, v4_ld(S + 0x230));
    v4_st(veh + 0x1670, v4_ld(S + 0x240)); v4_st(veh + 0x1680, v4_ld(S + 0x240));
    sim = U32(veh, 0x16d4);
    SF32(S, 0x14, phys_mul(F32(sim, 0x88f0), 2.0f));
    SF32(S, 0x10, phys_mul(F32(sim, 0x87a0), 2.0f));
    const rx_v4 cj = v4(-1.0f, -1.0f, -1.0f, 1.0f);
    const rx_v4 q = v4_ld(sim + 0x730), qc = v4_mul(q, cj);
    v4_st(S + 0x220, v4_splat(q.v[0])); v4_st(S + 0x60, v4_splat(q.v[3]));
    v4_st(S + 0x1d0, v4_splat(q.v[1])); v4_st(S + 0x1e0, v4_splat(q.v[2])); v4_st(S + 0x280, q);
    const rx_v4 up = veh_quat_mul(q, veh_quat_mul(v4_and(v4_ld(0x00d615c0), v4_ld(XYZ_MASK)), qc));
    const rx_v4 fw = veh_quat_mul(q, veh_quat_mul(v4_and(v4_ld(0x00d616a0), v4_ld(XYZ_MASK)), qc));
    const rx_v4 fw_flat = v4(fw.v[0], 0.0f, fw.v[2], fw.v[3]), up_flat = v4(up.v[0], 0.0f, up.v[2], up.v[3]);
    const rx_v4 d = v4_mul(unit3(v4_add(fw_flat, up_flat)), v4_splat(1.0f));
    v4_st(veh + 0x1650, v4_add(d, v4_ld(veh + 0x1650)));
    v4_st(veh + 0x1660, v4_sub(v4_ld(veh + 0x1660), d));
    v4_st(veh + 0x1670, v4_add(d, v4_ld(veh + 0x1670)));
    v4_st(veh + 0x1680, v4_sub(v4_ld(veh + 0x1680), d));
    /* ---- the wheel-lift forces (the suspension compressions +0x1640 / +0x1644 and +0x1648 / +0x164c over the rest
     *      lengths, capped at 1; force = +0x16a4 c^2 along the wheel normal) ---- */
    v4_st(U32(veh, 0x16d4) + 0x8a50, v4_ld(WORLD_ZERO4));
    if (U32(veh, 0x1630) == 0 && U32(veh, 0x1634) == 0) { SU32(veh, 0x1614, 0); SU32(veh, 0x160c, 0); }
    else {
        float m = F32(veh, 0x1640) > F32(veh, 0x1644) ? F32(veh, 0x1640) : F32(veh, 0x1644);
        m = phys_div(m, F32(S, 0x14));
        const float c = m > 1.0f ? 1.0f : m;
        const float fz = phys_mul(phys_mul(F32(veh, 0x16a4), c), c);
        SF32(veh, 0x161c, c);
        SU32(veh, 0x1614, U32(veh, 0x160c) == 0);
        SU32(veh, 0x160c, 1);
        v4_st(U32(veh, 0x16d4) + 0x8a50, v4_mul(v4_splat(fz), v4_ld(S + 0x2a0)));
    }
    v4_st(U32(veh, 0x16d4) + 0x8a40, v4_ld(WORLD_ZERO4));
    if (U32(veh, 0x1638) == 0 && U32(veh, 0x163c) == 0) { SU32(veh, 0x1618, 0); SU32(veh, 0x1610, 0); }
    else {
        float m = F32(veh, 0x1648) > F32(veh, 0x164c) ? F32(veh, 0x1648) : F32(veh, 0x164c);
        m = phys_div(m, F32(S, 0x10));
        const float c = m > 1.0f ? 1.0f : m;
        const float fz = phys_mul(phys_mul(F32(veh, 0x16a4), c), c);
        SF32(veh, 0x1620, c);
        SU32(veh, 0x1618, U32(veh, 0x1610) == 0);
        SU32(veh, 0x1610, 1);
        v4_st(U32(veh, 0x16d4) + 0x8a40, v4_mul(v4_splat(fz), v4_ld(S + 0x290)));
    }
    /* ---- the splash probe above the bike's middle: (rotate(q2, +0x8820) + +0x360 + rotate(q2, +0x8970) + +0x2c0) * the
     *      global factor 0xd615d0, raised by the body's up axis ---- */
    sim = U32(veh, 0x16d4);
    {
        const rx_v4 q2 = v4_ld(sim + 0x7c0);
        const rx_v4 p1 = v4_add(veh_rotate(q2, v4_and(v4_ld(sim + 0x8820), v4_ld(XYZ_MASK))), v4_ld(sim + 0x360));
        const rx_v4 p2 = v4_add(veh_rotate(q2, v4_and(v4_ld(sim + 0x8970), v4_ld(XYZ_MASK))), v4_ld(sim + 0x2c0));
        const rx_v4 mid = v4_mul(v4_add(p1, p2), v4_ld(0x00d615d0));
        v4_st(S + 0x1a0, v4(0.0f, 1.0f, 0.0f, 0.0f));
        v4_st(S + 0x210, mid);
        v4_st(veh + 0x1690, v4_add(veh_quat_mul(q, veh_quat_mul(v4_and(v4_ld(S + 0x1a0), v4_ld(XYZ_MASK)), qc)), mid));
        v4_st(S + 0x1f0, v4_ld(veh + 0x1690));
    }
    if ((U32(0, 0x00d72aa8) & 1u) == 0) {
        SU32(0, 0x00d72aa8, U32(0, 0x00d72aa8) | 1u);
        WORLD_CALL_SP(0, (void)rx_water_heights_init());   /* 004242f0 */
#ifdef RX_ORACLE
        WORLD_ORIG(0x008f9fbf, RX_CDECL, 0, 0x00911e90);   /* _atexit */
#else
        WORLD_CALL_SP(4,(void)rx_crt_atexit(0x00911e90));
#endif
    }
    int splash = 0;
    uint32_t found;
    WORLD_CALL_SP(4, found = rx_world_water_set_height(0x00d72aac, S + 0x1f0));
    if (found != 0) {
        const float dy = phys_sub(F32(S, 0x1f4), F32(veh, 0x1694));
        if (all3(cmplt_v(v4_ld(WORLD_ZERO4), v4_splat(dy)))) {
            float c = phys_div(dy, F32(0, 0x009d789c));
            if (c > 1.0f) c = 1.0f;
            const float k = phys_mul(phys_mul(F32(veh, 0x16a8), c), c);
            WORLD_CALL_SP(4,rx_atv_vehicle_add_water_force(veh,k)); /*0045f500: EAX=V,stackfloat RET4; shared base simulation+234*/
            splash = 1;
        }
    }
    WORLD_CALL_SP(0xc, rx_world_water_wake(U32(S, 0xc), veh, veh + 0x1630, dtp));
    SF32(veh, 0x131c, 0.0f);
    if (splash) {
        const rx_v4 p = v4_add(veh_quat_mul(v4_ld(S + 0x280), veh_quat_mul(v4_and(v4_ld(S + 0x1a0), v4_ld(XYZ_MASK)),
                                                                           v4_mul(v4_ld(S + 0x280), cj))), v4_ld(S + 0x210));
        v4_st(S + 0x1f0, p);
        v4_st(S + 0x1a0, p);
        /*004699a0/004699ac push the pending point and a dummy argument;
         * singleton entry is S-12; after caller cleanup the point remains for
         * the following height query. Typed calls keep both original depths. */
        rx_addr set;WORLD_CALL_SP(8,set=rx_atv_water_singleton());
        uint32_t r2;
        WORLD_CALL_SP(4, r2 = rx_world_water_set_height(set, S + 0x1f0));
        if (r2 != 0) {
            const float d = phys_sub(F32(S, 0x1f4), F32(S, 0x1a4));
            SF32(veh, 0x131c, d);
            const float c = phys_div(d, F32(0, 0x009d789c));
            SU32(veh, 0x1310, 1);
            if (c >= 1.0f) SF32(veh, 0x131c, 1.0f);
            else SF32(veh, 0x131c, 0.0f >= c ? 0.0f : c);
        }
    }
    {
        const float v = F32(veh, 0x131c);
        float t;
        if (v > 0.5f) {
            SU32(veh, 0x1310, 1);
            if (U32(veh, 0x1314) == 0 && (F32(veh, 0x1318) > 3.0f || v >= 1.0f)) {
                SU32(veh, 0x1314, 1);
                SU32(U32(veh, 0x1a0), 0x5b568, 0x16);
                SF32(veh, 0x1318, 3.0f);
            }
            t = phys_add(F32(veh, 0x1318), dtp);
        } else {
            t = 0.0f;
            SU32(veh, 0x1310, 0);
        }
        SF32(veh, 0x1318, t);
    }
    sim = U32(veh, 0x16d4);
    if (U32(sim, 0x2e80) != 0) return;
    /* ---- the bike's terrain track contacts (radii table at S + 0x20: 1, 2, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3) ---- */
    {
        static const float radii[12] = {1, 2, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3};
        for (uint32_t i = 0; i < 12; i++) SF32(S, 0x20 + 4 * i, radii[i]);
        WORLD_CALL_SP(0x1c, rx_world_tracks_update_contact_slots(veh + 0x1020, S + 0x150, veh, I32(sim, 0x1318), sim + 0x1bb8,
                                                                 sim + 0x1320, sim + 0x17d0, veh + 0x11b0, S + 0x110));
        if (U32(veh, 0x1320) == 0) {
            const rx_addr G = S + 0x1a0;
            static const int32_t calls[7][4] = {   /* group +0, +4, +8, +0x18 (-1 = unchanged) */
                {3, 8, 0xa, 3}, {1, 9, 0xb, 3}, {2, -1, -1, 1}, {0, -1, -1, 1}, {4, -1, -1, 1}, {7, -1, -1, 1}, {5, 6, -1, 2}};
            for (int c = 0; c < 7; c++) {
                if (calls[c][0] >= 0) SU32(G, 0, (uint32_t)calls[c][0]);
                if (calls[c][1] >= 0) SU32(G, 4, (uint32_t)calls[c][1]);
                if (calls[c][2] >= 0) SU32(G, 8, (uint32_t)calls[c][2]);
                if (calls[c][3] >= 0) SU32(G, 0x18, (uint32_t)calls[c][3]);
                track_points(U32(S, 0xc), U32(veh, 0x16d4) + 0x1320, veh + 0x1020, veh + 0x11b0, 0.150000006f /* 0x9322c8 */,
                             S, dtp);
            }
        }
        sim = U32(veh, 0x16d4);
        for (uint32_t i = 0; i < 0x190; i += 4) SU32(veh + 0x1020, i, U32(sim + 0x1320, i));   /* rep movsd 0x64 */
        crt_memset(sim + 0x1bb8, 0, 0x64);   /* memset (thunk 0x8fa06a) */
    }
    /* ---- the rider's limb track contacts, or (without a wreck class) the rider's strips cut ---- */
    rx_addr ra = U32(veh, 0x1a0);
    if (U32(ra, 0xaf00) != 0) {
        for (uint32_t i = 0; i < 16; i++) SF32(S, 0x20 + 4 * i, 1.0f);
        WORLD_CALL_SP(0x1c, rx_world_tracks_update_contact_slots(veh + 0x1330, S + 0x150, veh, I32(ra, 0x670), ra + 0x674,
                                                                 ra + 0x6c0, ra + 0x7c0, veh + 0x14c0, S + 0x110));
        if (U32(veh, 0x1320) == 0) {
            const rx_addr G = S + 0x1a0;
            static const int32_t calls[6][4] = {
                {2, 6, 0xc, 3}, {1, 7, 0xd, 3}, {5, 3, -1, 2}, {4, 8, 0xa, 3}, {0xb, 0xe, -1, 2}, {9, 0xf, -1, 2}};
            for (int c = 0; c < 6; c++) {
                if (calls[c][0] >= 0) SU32(G, 0, (uint32_t)calls[c][0]);
                if (calls[c][1] >= 0) SU32(G, 4, (uint32_t)calls[c][1]);
                if (calls[c][2] >= 0) SU32(G, 8, (uint32_t)calls[c][2]);
                if (calls[c][3] >= 0) SU32(G, 0x18, (uint32_t)calls[c][3]);
                track_points(U32(S, 0xc), U32(veh, 0x1a0) + 0x6c0, veh + 0x1330, veh + 0x14c0, 0.100000001f /* 0x931d94 */,
                             S, dtp);
            }
        }
        ra = U32(veh, 0x1a0);
        crt_memcpy(veh + 0x1330, ra + 0x6c0, U32(ra, 0x670) << 4);   /* memcpy (thunk 0x8fa070) */
    } else {
        SU32(S, 0x14, 0);
        if (I32(ra, 0x670) > 0) {
            const rx_addr tt = U32(0, 0x00a9577c);
            rx_addr p = veh + 0x14c0;
            SU32(S, 0x10, p);
            for (int32_t i = 0; i < I32(U32(veh, 0x1a0), 0x670); i++, p += 4) {
                const int32_t v = I32(p, 0);
                if (v >= 0) {
                    const int32_t idx = v ^ 0x4000;
                    if (idx < I32(tt, 0xc) && idx >= 0) {
                        WORLD_CALL_SP(0, phys_tracks_cut_strip(tt, idx));   /* TerrainTracks_CutStrip */
                        SU32(U32(tt, 0x24) + (uint32_t)idx * 0xc0u, 0x98, 1);
                        SU32(tt, 0x28, U32(tt, 0x28) - 1);
                    }
                    SU32(p, 0, 0xffffffffu);
                }
                SU32(S, 0x14, (uint32_t)(i + 1));
                SU32(S, 0x10, p + 4);
            }
        }
    }
    ra = U32(veh, 0x1a0);
    crt_memset(ra + 0x674, 0, U32(ra, 0x670) * 4u);   /* memset */
    SU32(veh, 0x1320, 0);
}
RX_FUNCTION(0x00468300, rx_world_vehicle_wheels)
