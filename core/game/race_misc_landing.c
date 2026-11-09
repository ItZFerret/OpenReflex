/* race_misc_landing.c -- the landing prediction the AI's air behaviours use (FUN_00461980): where and when a vehicle in the air
 * comes down. The flight is ballistic (gravity 32.2 units/s^2, no drag) from a point offset from the vehicle's position by its
 * orientation; the fall time to the level's floor height (terrain +0xdc) bounds the search; the path is cut into 15 segments that
 * are ray-cast against the physics bodies, then against the terrain. The original is SSE code on 4-lane vectors: the lanes are
 * reproduced exactly (the vectors the callees and the outputs see), with the x86 NaN rules (rx_x86.h). */
#include "race.h"
#include "race_extern.h"
#include "race_misc.h"
#include "world_veh.h"
#include "phys_terrain.h"
#include "loop_vehicle.h"

typedef struct { float v[4]; } lane4;

static lane4 ld4(rx_addr a) { lane4 r; for (int i = 0; i < 4; i++) r.v[i] = F32(a, 4 * i); return r; }
static void st4(rx_addr a, lane4 x) { for (int i = 0; i < 4; i++) SF32(a, 4 * i, x.v[i]); }
static lane4 bc(float x) { lane4 r = {{x, x, x, x}}; return r; }
static lane4 add4(lane4 a, lane4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_adds(a.v[i], b.v[i]); return a; }
static lane4 sub4(lane4 a, lane4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_subs(a.v[i], b.v[i]); return a; }
static lane4 mul4(lane4 a, lane4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_muls(a.v[i], b.v[i]); return a; }
static lane4 div4(lane4 a, lane4 b) { for (int i = 0; i < 4; i++) a.v[i] = rx_divs(a.v[i], b.v[i]); return a; }
static lane4 sqrt4(lane4 a) { for (int i = 0; i < 4; i++) a.v[i] = rx_sqrts(a.v[i]); return a; }
/* shufps a, b, imm */
static lane4 shuf(lane4 a, lane4 b, unsigned imm) {
    lane4 r = {{a.v[imm & 3], a.v[(imm >> 2) & 3], b.v[(imm >> 4) & 3], b.v[(imm >> 6) & 3]}};
    return r;
}
static lane4 c4(rx_addr a) { return ld4(a); }   /* a 16-byte constant (from memory: .rdata or .data) */
/* cmpleps / cmpltps on lanes 0..2 all true (the original sums the three masks and compares with -3) */
static int all3_le(lane4 a, lane4 b) { return a.v[0] <= b.v[0] && a.v[1] <= b.v[1] && a.v[2] <= b.v[2]; }
static int all3_lt(lane4 a, lane4 b) { return a.v[0] < b.v[0] && a.v[1] < b.v[1] && a.v[2] < b.v[2]; }

/* The quaternion product the original builds from shuffles and sign vectors (0x93a1f0 (1, -1, 1, -1), 0x93a210 (-1, 1, 1, -1),
 * 0x93a200 (1, 1, -1, -1)): q (x) p with the lanes summed as ((rev(swap(rev p)) * q2) * s210 + (q1 * swap(rev p)) * s200) +
 * (q3 * p + (q0 * rev p) * s1f0). */
static lane4 quat_mul(lane4 q, lane4 p) {
    const lane4 rp = shuf(p, p, 0x1b);
    const lane4 sp = shuf(rp, rp, 0xb1);
    const lane4 tp = shuf(sp, sp, 0x1b);
    const lane4 x4 = mul4(mul4(bc(q.v[0]), rp), c4(0x0093a1f0));
    const lane4 x0 = add4(mul4(mul4(tp, bc(q.v[2])), c4(0x0093a210)), mul4(mul4(bc(q.v[1]), sp), c4(0x0093a200)));
    return add4(x0, add4(mul4(bc(q.v[3]), p), x4));
}

/* The horizontal length of v (lanes 0 + 2, then + lane 1; the original masks y to 0 before) */
static float hlen2(lane4 v) { return rx_adds(rx_adds(v.v[0], v.v[2]), v.v[1]); }

/* PhysicsWorld_RayCastBodies 0085d360 (cdecl world, rays, n, hits, mask 4): the core function in the oracle too (the oracle ran
 * the original until w-residue2: the core's checked-iterator advance 00842fa0 skipped the proxy reads that map the boot's ConVar
 * page 0x1002b000, known-differences 2). */
static void raycast_bodies(rx_addr world, rx_addr rays, int32_t n, rx_addr hits) {
    RACE_CORE_CALL(5, rx_physics_world_raycast_bodies(world, rays, n, hits, 4));
}

/* 00461980 (ECX = time out, EDX = vehicle; args: point out, normal out (16 bytes each); RET 8) -> EAX: 1 when the landing is on
 * something found, 0 when nothing is hit within the flight (the last point of the path, the up vector 0xd616a0). The start is the
 * vehicle position (vf30) plus (0, s, 0) rotated by the orientation (vf36; s = -2 on bikes, -1 with +0x194, from .data); the
 * velocity is vf37. With no fall time (the discriminant (y - floor) * 64.4 + vy^2 not above the .data threshold 0xd61670, or the
 * time (sqrt + vy) / 32.2 not above it) the point is the terrain under the start (FUN_007c5c90) at time 0. Nearly no horizontal
 * speed (< 0.01): a vertical ray to the floor height (PhysicsWorld_RayCastBodies, else the terrain under it) and the fall time to
 * that height. Otherwise 16 points of the parabola at fall time / 15 steps make 15 segments: the first body hit (point, normal),
 * else the first terrain hit (TerrainSegmentQuery ECX = start, EAX = end, ESI = the terrain); the time is the horizontal distance
 * from the start over the horizontal speed. The terrain is FUN_00402dc0 (EDX = 0x9e30d4; its static 0x9e30dc behind 0x9e30e0).
 * Frame: and esp, -16; sub 0x6f4; 3 pushes (velocity +0xb0, position +0x90, orientation +0xf0, segments +0x140, path +0x320,
 * hits +0x420 / +0x110). */
uint32_t rx_landing_predict(rx_addr time_out, rx_addr veh, rx_addr point_out, rx_addr normal_out) {
    RACE_FRAME_EBP(F, 16, 0x6f4, 3);
    uint32_t r = 1;
    lane4 v15 = bc(15.0f);
    st4(F + 0xa0, v15);
    const float s = U32(veh, 0x194) == 0 ? F32(0, 0x9d7b30) /* -2 */ : F32(0, 0x9cd9e8) /* -1 */;
    SF32(F, 0x24, s);
    if ((U8(0, 0x9e30e0) & 1) == 0) { SU32(0, 0x9e30e0, U32(0, 0x9e30e0) | 1); SU32(0, 0x9e30dc, 0); }
    rx_addr terrain;
    RACE_CORE_CALL(0, terrain = rx_world_find_by_controller(0x009e30d4));   /* EDX = 0x9e30d4 */
    SU32(F, 0x28, terrain);
    const float floor_y = F32(terrain, 0xdc);
    SF32(F, 0x2c, floor_y);
    race_thiscall(race_vfn(veh, 37), veh, 1, F + 0xb0, 0, 0, 0);
    race_thiscall(race_vfn(veh, 30), veh, 1, F + 0x90, 0, 0, 0);
    race_thiscall(race_vfn(veh, 36), veh, 1, F + 0xf0, 0, 0, 0);
    const lane4 q = ld4(F + 0xf0);
    const lane4 zero = bc(0.0f);
    /* the offset (0, s, 0, 0) rotated: q (x) (a (x) conj q) */
    const lane4 a = {{0.0f, s, 0.0f, 0.0f}};   /* (0, s, 0, 0) masked by 0x92db80 (the w lane cleared) */
    const lane4 qc = mul4(q, c4(0x0093a220));
    const lane4 r1 = shuf(qc, qc, 0x1b);
    const lane4 s1 = shuf(r1, r1, 0xb1);
    const lane4 t1 = shuf(s1, s1, 0x1b);
    const lane4 p = add4(add4(mul4(mul4(t1, bc(a.v[2])), c4(0x0093a210)), mul4(mul4(bc(a.v[1]), s1), c4(0x0093a200))),
                         add4(mul4(bc(a.v[3]), qc), mul4(mul4(bc(a.v[0]), r1), c4(0x0093a1f0))));
    const lane4 vel = ld4(F + 0xb0);
    const lane4 start = add4(quat_mul(q, p), ld4(F + 0x90));
    const lane4 mask_xz = {{1.0f, 0.0f, 1.0f, 1.0f}};
    st4(F + 0x30, mask_xz);
    const lane4 hvel = mul4(mask_xz, vel);
    st4(F + 0x80, hvel);
    const lane4 g = {{0.0f, 32.2f /* 0x977878 */, 0.0f, 0.0f}};
    st4(F + 0xc0, g);
    const lane4 vy = bc(vel.v[1]);
    const lane4 sy = bc(start.v[1]);
    st4(F + 0x100, sy);
    const lane4 g2 = mul4(bc(2.0f), bc(32.2f));
    const lane4 vy2 = mul4(vy, vy);
    st4(F + 0x40, vy2);
    st4(F + 0x50, g2);
    const lane4 disc = add4(mul4(sub4(sy, bc(floor_y)), g2), vy2);
    st4(F + 0x90, start);
    st4(F + 0xe0, vy);
    st4(F + 0x60, bc(32.2f));
    lane4 tl = zero;
    int no_fall = all3_le(disc, c4(0x00d61670));
    if (!no_fall) {
        tl = div4(add4(sqrt4(disc), vy), bc(32.2f));
        no_fall = all3_le(tl, c4(0x00d61670));
    }
    if (no_fall) {
        SF32(time_out, 0, 0.0f);
        st4(point_out, start);
        RACE_CORE_CALL(2, (void)phys_terrain_base_height_normal(normal_out, terrain, point_out));   /* ECX = the normal */
        goto done;
    }
    const lane4 hv2 = mul4(hvel, hvel);
    st4(F + 0x80, hv2);
    const float speed2 = hlen2(hv2);
    const float hspeed = rx_sqrts(speed2);
    if (hspeed < 0.01f /* 0x9322ac */) {
        /* falling straight down: one ray from the start to the floor height */
        lane4 end = {{0.0f, floor_y, 0.0f, 0.0f}};
        end = add4(end, mul4(mask_xz, start));
        st4(F + 0xc0, start);
        st4(F + 0xd0, end);
        st4(F + 0xa0, zero);
        v15 = zero;
        raycast_bodies(U32(U32(veh, 0x188), 0x20134), F + 0xc0, 1, F + 0x110);   /* PhysicsWorld_RayCastBodies */
        lane4 hit;
        if (U8(F, 0x110) != 0) {
            hit = ld4(F + 0x120);
            st4(point_out, hit);
            st4(normal_out, ld4(F + 0x130));
        } else {
            st4(F + 0x80, start);
            RACE_CORE_CALL(2, (void)phys_terrain_base_height_normal(normal_out, terrain, F + 0x80));
            hit = ld4(F + 0x80);
            st4(point_out, hit);
        }
        const lane4 k0 = c4(0x00d61670);
        const lane4 d2 = add4(mul4(sub4(sy, bc(hit.v[1])), ld4(F + 0x50)), ld4(F + 0x40));
        if (!all3_lt(k0, d2)) { SF32(time_out, 0, v15.v[0]); goto done; }
        const lane4 t2 = div4(add4(sqrt4(d2), ld4(F + 0xe0)), ld4(F + 0x60));
        SF32(time_out, 0, all3_lt(t2, k0) ? k0.v[0] : t2.v[0]);
        goto done;
    }
    {
        /* the parabola: 16 points at fall time / 15 steps, then 15 segments */
        const lane4 dt = div4(tl, v15);
        const lane4 hg = mul4(bc(0.5f), g);
        lane4 t = zero;
        for (uint32_t i = 0; i < 16; i++) {
            st4(F + 0x320 + 0x10 * i, sub4(add4(mul4(t, vel), start), mul4(mul4(hg, t), t)));
            t = add4(t, dt);
        }
        for (uint32_t i = 0; i < 15; i++) {
            st4(F + 0x140 + 0x20 * i, ld4(F + 0x320 + 0x10 * i));
            st4(F + 0x150 + 0x20 * i, ld4(F + 0x330 + 0x10 * i));
        }
        raycast_bodies(U32(U32(veh, 0x188), 0x20134), F + 0x140, 0xf, F + 0x420);
        const float speed = rx_sqrts(hlen2(ld4(F + 0x80)));
        for (uint32_t i = 0; i < 15; i++) {
            if (U8(F + 0x420 + 0x30 * i, 0) == 0) continue;
            const lane4 hit = ld4(F + 0x430 + 0x30 * i);
            lane4 d = mul4(sub4(hit, ld4(F + 0x90)), ld4(F + 0x30));
            d = mul4(d, d);
            SF32(time_out, 0, rx_divs(rx_sqrts(hlen2(d)), speed));
            st4(point_out, hit);
            st4(normal_out, ld4(F + 0x440 + 0x30 * i));
            goto done;
        }
        for (uint32_t i = 0; i < 15; i++) {
            const rx_addr seg = F + 0x140 + 0x20 * i;
            uint32_t qr[2] = {seg, 0}, hit_found;                            /* ECX = the start, EDX = 0 */
            RACE_CORE_CALL(2, hit_found = phys_terrain_segment_query(terrain, seg, seg + 0x10, point_out, normal_out, qr));   /* TerrainSegmentQuery */
            if (hit_found != 0) {
                lane4 d = mul4(sub4(ld4(point_out), ld4(F + 0x90)), ld4(F + 0x30));
                d = mul4(d, d);
                SF32(time_out, 0, rx_divs(rx_sqrts(hlen2(d)), rx_sqrts(hlen2(ld4(F + 0x80)))));
                goto done;
            }
        }
        const lane4 last = ld4(F + 0x310);
        st4(point_out, last);
        st4(normal_out, c4(0x00d616a0));
        lane4 d = mul4(sub4(last, ld4(F + 0x90)), ld4(F + 0x30));
        d = mul4(d, d);
        SF32(time_out, 0, rx_divs(rx_sqrts(hlen2(d)), rx_sqrts(hlen2(ld4(F + 0x80)))));
        r = 0;
    }
done:
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x00461980, rx_landing_predict)
