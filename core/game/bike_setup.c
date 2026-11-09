/* bike_setup.c -- the motocross simulation's ground placement (RavenSimulationMotocross::vf20 0043d1f0): where a reset puts the
 * bike. (RavenSimulationMotocross::vf2 0044a650 and 0045f600 are ported in build_vehicle_place.c.) Motocross only.
 *
 * The SSE idioms are the bike unit's (bike_math.h: "normalize or NaN", the horizontal sums, the quaternion product with the sign
 * tables 0093a1f0.., the rotation q (v) q*); .data / .bss vectors are read from game memory: 0x009cb4b0 the up axis,
 * 0x00d616a0 / 0x00d61680 two unit axes, 0x009cc37c / 0x009cd9b4 tunables. */
#include "bike_setup.h"
#include "bike_math.h"
#include "loop_util.h"
#include "loop_vehicle.h"
#include "world_veh.h"
#include "phys_terrain.h"
#include "map_scale.h"
#include "../src/fmt_modtrack.h"

/* x87 `fld m32; fstp m32`: a NaN keeps its payload, an SNaN comes out quiet (loop_cam.h cam_x87_bits) */
static uint32_t bs_x87(uint32_t b) {
    return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) ? (b | 0x00400000u) : b;
}
static float bs_sqrt_x87(rx_addr slot) { return phys_sqrt(rx_bf(bs_x87(U32(slot, 0)))); }   /* SqrtF of a stack slot (fld / fstp) */

/* 00402dc0 (EDX = the function-local static 0x009e30d4, first use initializes it): the current terrain */
static rx_addr bs_terrain(rx_addr ret) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const rx_addr s = loop_push(0, 0, ret); const rx_addr r = rx_world_find_by_controller(0x009e30d4); loop_pop(s);
    return r;
}
/* 007c5aa0 TerrainHeightAt (stdcall terrain, point in / out; RET 8) */
static uint32_t bs_height_at(rx_addr terrain, rx_addr point, rx_addr ret) {
    const uint32_t a[2] = {terrain, point};
    uint32_t edx; const rx_addr s = loop_push(2, a, ret); const uint32_t r = phys_terrain_height_at(terrain, point, &edx); loop_pop(s);
    return r;
}
/* 00401630 Vec3_Cross (ECX = c, EAX = a; cdecl out) */
static void bs_cross(rx_addr out, rx_addr c, rx_addr a, rx_addr ret) {
    const uint32_t x[1] = {out};
    const rx_addr s = loop_push(1, x, ret); rx_world_vec3_cross(out, c, a); loop_pop(s);
}

/* The orientation quaternion of the rows X (m00 = x0, X.y / X.z in memory at xy / xz), Y, Z (the larger of the four diagonal
 * combinations, as the code inlines it; the SqrtF arguments are the stack slots the candidates were stored in): out x y z w */
typedef struct { rx_addr x1, x2, y0, y2, z0, z1; } bs_rows;   /* the addresses of the off-diagonal entries read */
static void bs_rows_to_quat(float x0, float m11, float m22, bs_rows r, rx_addr S, uint32_t sw, uint32_t sx, uint32_t sy, uint32_t sz,
                            rx_addr q) {
    const float t0 = phys_add(x0, 1.0f), t1 = phys_sub(1.0f, x0), k = 0.25f /* 0x92ff84 */;
    const float qx = phys_mul(phys_sub(phys_sub(t0, m11), m22), k);
    const float qy = phys_mul(phys_sub(phys_add(t1, m11), m22), k);
    const float qw = phys_mul(phys_add(phys_add(t0, m11), m22), k);
    const float qz = phys_mul(phys_add(phys_sub(t1, m11), m22), k);
    SF32(S, sw, qw); SF32(S, sx, qx); SF32(S, sy, qy); SF32(S, sz, qz);
    const float X1 = F32(r.x1, 0), X2 = F32(r.x2, 0), Y0 = F32(r.y0, 0), Y2 = F32(r.y2, 0), Z0 = F32(r.z0, 0), Z1 = F32(r.z1, 0);
    float m = qx > qy ? qx : qy;
    if (!(m > qz)) m = qz;
    if (!(m > qw)) m = qw;
    if (m == qw) {
        const float s = bs_sqrt_x87(S + sw), d = phys_div(k, s);
        SF32(q, 0xc, s);
        SF32(q, 0, phys_mul(phys_sub(Y2, Z1), d));
        SF32(q, 4, phys_mul(phys_sub(Z0, X2), d));
        SF32(q, 8, phys_mul(phys_sub(X1, Y0), d));
        return;
    }
    float m2 = qy > qz ? qy : qz;
    if (!(m2 > qx)) m2 = qx;
    if (m2 == qx) {
        const float s = bs_sqrt_x87(S + sx), d = phys_div(k, s);
        SF32(q, 0, s);
        SF32(q, 0xc, phys_mul(phys_sub(Y2, Z1), d));
        SF32(q, 4, phys_mul(phys_add(X1, Y0), d));
        SF32(q, 8, phys_mul(phys_add(Z0, X2), d));
    } else if (qy > qz) {
        const float s = bs_sqrt_x87(S + sy), d = phys_div(k, s);
        SF32(q, 4, s);
        SF32(q, 0xc, phys_mul(phys_sub(Z0, X2), d));
        SF32(q, 0, phys_mul(phys_add(X1, Y0), d));
        SF32(q, 8, phys_mul(phys_add(Z1, Y2), d));
    } else {
        const float s = bs_sqrt_x87(S + sz), d = phys_div(k, s);
        SF32(q, 8, s);
        SF32(q, 0xc, phys_mul(phys_sub(X1, Y0), d));
        SF32(q, 0, phys_mul(phys_add(Z0, X2), d));
        SF32(q, 4, phys_mul(phys_add(Z1, Y2), d));
    }
}

/* The two wheel probes after a placement: from the simulation's two contact points (S +0x230 / +0x2d0 less the axis 0x00d616a0
 * times S +0x88f0 / +0x87a0) down by D (S0 + 0xa0), PhysicsWorld_RayCastBodies on the active world's physics (game +0x5a54 or
 * +0x20c8 by the active view set, +0x20134), then per probe the terrain height under the ray end (TerrainHeightAt), raised to a
 * body hit above it, or the body hit alone: the ground points at S0 + 0xd0 / + 0xe0, their found flags at S0 + 0x60 / + 0x64.
 * Returns whether both were found. */
static int bs_probe(rx_addr sim, rx_addr S, rx_addr terrain, rx_v4 D, rx_addr ret_ray, rx_addr ret_h) {
    const rx_v4 Y = rx_ld4(0x00d616a0);
    const rx_v4 W0 = v4_sub(rx_ld4(sim + 0x230), v4_mul(Y, rx_ld4(sim + 0x88f0)));
    const rx_v4 W1 = v4_sub(rx_ld4(sim + 0x2d0), v4_mul(rx_ld4(sim + 0x87a0), Y));
    rx_st4(S + 0xf0, W0); rx_st4(S + 0x110, W1);
    rx_st4(S + 0x100, v4_sub(W0, D)); rx_st4(S + 0x120, v4_sub(W1, D));
    rx_st4(S + 0xa0, D);
    const rx_addr ctx = U32(0, 0x00a95844);
    rx_addr level = 0;
    if (U32(U32(ctx, 0x20), 0x34) == 1) level = U32(U32(0, 0x00a946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) level = U32(U32(0, 0x00a946b8), 0x20c8);
    const rx_addr world = U32(level, 0x20134);
    {
        const uint32_t a[5] = {world, S + 0xf0, 2, S + 0x140, 0};
        const rx_addr s = loop_push(5, a, ret_ray);
        rx_physics_world_raycast_bodies(world, S + 0xf0, 2, S + 0x140, 0);
        loop_pop(s);
    }
    for (uint32_t k = 0; k < 2; k++) {
        const rx_addr G = S + 0xd0 + 0x10 * k, hit = S + 0x140 + 0x30 * k;
        rx_st4(G, rx_ld4(S + 0x100 + 0x20 * k));
        if (bs_height_at(terrain, G, ret_h) != 0) {
            SU32(S, 0x60 + 4 * k, 1);
            if (U8(hit, 0) != 0 && F32(G, 4) < F32(hit, 0x14)) rx_st4(G, rx_ld4(hit + 0x10));
        } else if (U8(hit, 0) != 0) {
            rx_st4(G, rx_ld4(hit + 0x10));
            SU32(S, 0x60 + 4 * k, 1);
        } else {
            SU32(S, 0x60 + 4 * k, 0);
        }
        /* Imported riding decks are static triangle meshes, not raycast bodies.
         * Keep both placement probes on the authored route's surface layer. */
        if (fmt_modtrack_racing() && U32(S, 0x60 + 4 * k)) {
            SF32(G, 4, fmt_modtrack_ride_height(F32(G, 0), F32(G, 8),
                F32(S, 0xb4) - 3.5f, F32(G, 4), rx_ms_scale()));
        }
    }
    return U32(S, 0x60) != 0 && U32(S, 0x64) != 0;
}

/* 0043d1f0 RavenSimulationMotocross::vf20 (thiscall S; stack position, direction, out position, out forward, out up; RET 0x14):
 * the ground placement for a reset. The position is lifted onto the terrain (TerrainHeightAt) and 3.5 above it, the direction
 * flattened (y = 0) and normalized into out forward; the orientation from the rows (up x forward, up 0x009cb4b0, forward) places the
 * simulation (vf2). Both wheel probes must find ground: the line between the two ground points gives a new forward and up (the
 * axis 0x00d616a0 less its own projection, normalized), the simulation is placed again, probed again, lowered by the shorter
 * probe (at most the .data 0x009cc37c) along 0x00d616a0 and raised by 0x009cd9b4: out position, out forward / up = the two axes
 * 0x00d61680 / 0x00d616a0 rotated by the final orientation. Frame: push ebp; and esp, -16; sub esp, 0x1a4 (cookie at +0x1a0);
 * push ebx / esi / edi: +0xb0 the position, +0xc0 the orientation, +0xd0 / +0xe0 the ground points, +0xf0.. the two rays,
 * +0x140 the hits, +0x50 / +0x60 / +0x70 / +0x80 / +0x90 vectors and the second quaternion. */
void rx_sim_moto_ground_place(rx_addr sim, rx_addr pos, rx_addr dir, rx_addr out_pos, rx_addr out_fwd, rx_addr out_up) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, B = A - 0x1a4u, S = B - 0xcu;
    SU32(B, 0x1a0, U32(0, 0x009b45e4) ^ B);
    rx_set_sp(S);
    SU32(S, 0x3c, sim); SU32(S, 0x30, out_pos); SU32(S, 0x34, out_fwd); SU32(S, 0x38, out_up);
    const rx_addr terrain = bs_terrain(0x0043d24f);
    SU32(S, 0x2c, terrain);
    {   /* the position on the ground (not below it), 3.5 up */
        const rx_v4 p0 = rx_ld4(pos);
        rx_st4(S + 0xb0, p0); rx_st4(S + 0xd0, p0);
        bs_height_at(terrain, S + 0xd0, 0x0043d274);
        const rx_v4 g = rx_ld4(S + 0xd0);
        rx_v4 p = rx_ld4(S + 0xb0);
        if (p.v[1] < g.v[1]) p = g;
        SU32(out_fwd, 0, bs_x87(U32(dir, 0)));
        SU32(out_fwd, 4, bs_x87(U32(dir, 4)));
        SU32(out_fwd, 8, bs_x87(U32(dir, 8)));
        rx_st4(S + 0xb0, v4_add(v4(0.0f, 3.5f /* 0x977da8 */, 0.0f, 0.0f), p));
    }
    rx_st4(S + 0x40, v4_zero());
    {   /* the flat forward */
        const float x = F32(out_fwd, 0), z = F32(out_fwd, 8);
        const float l2 = phys_add(phys_add(phys_mul(x, x), phys_mul(0.0f, 0.0f)), phys_mul(z, z));
        SF32(out_fwd, 4, 0.0f);
        SF32(S, 0x28, x); SF32(S, 0x20, z); SF32(S, 0x10, l2);
        if (l2 > 0.0f) {
            const float inv = phys_div(1.0f, bs_sqrt_x87(S + 0x10));
            SF32(out_fwd, 0, phys_mul(inv, x));
            SF32(out_fwd, 4, phys_mul(inv, 0.0f /* 0x92e12c */));
            SF32(out_fwd, 8, phys_mul(inv, z));
        }
    }
    {   /* side = up x forward; the first orientation */
        bs_cross(S + 0x60, 0x009cb4b0, out_fwd, 0x0043d38a);
        const float cx = F32(S, 0x60), cy = F32(S, 0x64), cz = F32(S, 0x68);
        const float l2 = phys_add(phys_add(phys_mul(cy, cy), phys_mul(cx, cx)), phys_mul(cz, cz));
        SF32(S, 0x20, l2);
        float nx = cx;
        if (l2 > 0.0f) {
            const float inv = phys_div(1.0f, bs_sqrt_x87(S + 0x20));
            nx = phys_mul(inv, cx);
            SF32(S, 0x64, phys_mul(inv, cy));
            SF32(S, 0x68, phys_mul(inv, cz));
        }
        const bs_rows r = {S + 0x64, S + 0x68, 0x009cb4b0, 0x009cb4b8, out_fwd, out_fwd + 4};
        bs_rows_to_quat(nx, F32(0, 0x009cb4b4), F32(out_fwd, 8), r, S, 0x20, 0x28, 0x10, 0x24, S + 0x50);
        rx_st4(S + 0xc0, rx_ld4(S + 0x50));
        loop_tc2(loop_vslot(sim, 8), sim, S + 0xb0, S + 0xc0);          /* vf2: place the simulation */
    }
    const rx_v4 D = v4(0.0f, 20.0f /* 0x935aac */, 0.0f, 0.0f);
    if (bs_probe(sim, S, terrain, D, 0x0043d763, 0x0043d796)) {
        /* the slope between the two ground points: forward n, up np (the axis less its projection), side np x n */
        const rx_v4 n = bike_normalize(v4_sub(rx_ld4(S + 0xd0), rx_ld4(S + 0xe0)));
        const rx_v4 Y = rx_ld4(0x00d616a0);
        const float d = bike_dot3(n, Y);
        const rx_v4 np = bike_normalize(v4_sub(Y, v4_mul(v4_splat(d), Y)));
        rx_st4(S + 0x70, np); rx_st4(S + 0x90, n);
        bs_cross(S + 0x50, S + 0x70, S + 0x90, 0x0043d916);
        const float cx = F32(S, 0x50), cy = F32(S, 0x54), cz = F32(S, 0x58);
        const float l2 = phys_add(phys_add(phys_mul(cx, cx), phys_mul(cy, cy)), phys_mul(cz, cz));
        SF32(S, 0x24, l2);
        float nx = cx;
        if (l2 > 0.0f) {
            const float inv = phys_div(1.0f, bs_sqrt_x87(S + 0x24));
            nx = phys_mul(inv, cx);
            SF32(S, 0x54, phys_mul(inv, cy));
            SF32(S, 0x58, phys_mul(inv, cz));
        }
        const bs_rows r = {S + 0x54, S + 0x58, S + 0x70, S + 0x78, S + 0x90, S + 0x94};
        bs_rows_to_quat(nx, F32(S, 0x74), F32(S, 0x98), r, S, 0x24, 0x20, 0x28, 0x10, S + 0x80);
        rx_st4(S + 0xc0, rx_ld4(S + 0x80));
        loop_tc2(loop_vslot(sim, 8), sim, S + 0xb0, S + 0xc0);
        if (bs_probe(sim, S, terrain, rx_ld4(S + 0xa0), 0x0043dd2f, 0x0043dd65)) {
            /* lowered by the shorter probe (at most 0x009cc37c), raised by 0x009cd9b4 */
            float m = F32(0, 0x009cc37c);
            for (uint32_t k = 0; k < 2; k++) {
                const rx_v4 dd = v4_sub(rx_ld4(S + 0xd0 + 0x10 * k), rx_ld4(S + 0xf0 + 0x20 * k));
                const float dist = phys_sqrt(bike_hsum_021(v4_mul(dd, dd)));
                if (dist < m) m = dist;
            }
            const rx_v4 np2 = v4_add(v4(0.0f, F32(0, 0x009cd9b4), 0.0f, 0.0f), v4_sub(rx_ld4(S + 0xb0), v4_mul(v4_splat(m), rx_ld4(0x00d616a0))));
            rx_st4(S + 0xb0, np2);
            const rx_v4 q = rx_ld4(S + 0xc0);
            rx_st4(out_pos, np2);
            rx_st4(out_fwd, bike_rotate(q, rx_ld4(0x00d61680)));
            rx_st4(out_up, bike_rotate(q, rx_ld4(0x00d616a0)));
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0043d1f0, rx_sim_moto_ground_place)
