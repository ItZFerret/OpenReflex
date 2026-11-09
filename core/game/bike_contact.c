/* bike_contact.c -- slot 13 of the motocross simulation (00444b90 RavenSimulationMotocross::vf13): the contacts of the wheels and
 * of the body with the terrain and the objects, the position corrections that push the bodies out of what they penetrate, the
 * rolling orientation of the wheels and the resting pin.
 *
 * Wheel contact record W (0x150 bytes; rear S+8790 (body 3), front S+88e0 (body 2)): +00 body index, +10 wheel extent vec4,
 * +40 contact normal, +50 / +60 the two contact points whose difference is the penetration, +fc hit flag, +100 / +108 contact
 * state flags. Bodies: stride 0xa0 from S, position +f0, orientation +730 (bike.md). */
#include "bike_contact.h"
#include "bike_curve.h"
#include "bike_slot13.h"
#include "phys_coll_terrain.h"
#include "phys_quat.h"
#include "world_veh.h"
#include "loop_jobs.h"
#ifdef RX_CONTACT_FRAME_OBSERVERS
#include "contact_frames.h"
#define CONTACT_ALL3(mask) rx_contact_frames_all3(mask)
#define CONTACT_MOVED(body) rx_contact_frames_body_moved(body)
#define CONTACT_AVERAGE_BEGIN() rx_contact_frames_average_begin()
#else
#define CONTACT_ALL3(mask) bike_all3(mask)
#define CONTACT_MOVED(body) ((void)0)
#define CONTACT_AVERAGE_BEGIN() ((void)0)
#endif

#define G_NUMERATOR   0x00d61650u   /* vec4 1.0 in all lanes at run time (the "one" added to counts) */
#define G_ZERO        0x00d61670u   /* zero vec4 */
#define G_UP          0x00d616a0u   /* the bike's up axis (vec4) */
#define G_CORR_SCALE  0x009cd970u   /* float: share of the penetration that is corrected (0.9) */

/* ==== 00431ee0 / 0043cfe0 : the position correction of one contacting wheel ===================================================== */
/* Common tail. KP = the correction (scale * penetration along the contact normal); up = the bike's up axis in the world.
 *  - if the correction points against the world up axis (G . KP < 0) it is reported as it is (negated) and the body stays;
 *  - else if the contact is flagged (W+108) or the correction points against the bike's up (up . KP < 0) it is reported as KP,
 *    the body stays;
 *  - else the part along the bike's up axis, s * up, is moved into the body position and only the rest is reported.
 * Returns the reported vec4 (the original stores it through its stack argument). Adds the body part to the wheel body's position. */
static rx_v4 wheel_correction_tail(rx_addr W, rx_addr S, rx_v4 up, rx_v4 KP) {
    const rx_v4 zero = rx_ld4(G_ZERO);
    const rx_v4 s4 = v4_splat(bike_hsum_012(v4_mul(KP, up)));
    const float g = bike_hsum_012(v4_mul(KP, rx_ld4(G_UP)));
    rx_v4 out, move;
    if (CONTACT_ALL3(v4_cmplt(v4_splat(g), zero))) {
        out = v4_sub(v4_zero(), KP); move = zero;
    } else if (I32(W, 0x108) != 0 || CONTACT_ALL3(v4_cmplt(s4, zero))) {
        out = KP; move = zero;
    } else {
        move = v4_mul(s4, up); out = v4_sub(KP, move);
    }
    const rx_addr B = S + (uint32_t)(I32(W, 0) * 0xa0) + 0xf0;
    rx_st4(B, v4_add(rx_ld4(B), move));
    CONTACT_MOVED(B);
    return out;
}

/* the bike's up axis in the world: q * (up * conj q) with the root orientation q = S+730 */
static rx_v4 wheel_world_up(rx_addr S) { return bike_rotate(rx_ld4(S + 0x730), rx_ld4(G_UP)); }

/* 00431ee0 BikeWheel_ContactPositionCorrection (ECX = W, EDX = S, stack out vec4; ret 4; EAX = out): penetration D = W+50 - W+60.
 * With the contact normal n = W+40 and its y: y >= 0.6 -> the whole D is corrected; y <= 0.4 -> only the part along n; in between
 * a blend of the two. */
rx_v4 bike_wheel_position_correction(rx_addr W, rx_addr S) {
    const rx_v4 up = wheel_world_up(S);
    const rx_v4 D = v4_sub(rx_ld4(W + 0x50), rx_ld4(W + 0x60));
    const rx_v4 n = rx_ld4(W + 0x40);
    const rx_v4 ny = v4_lane(n, 1);
    rx_v4 P = D;
    if (CONTACT_ALL3(v4_cmplt(ny, v4_splat(0.6f /* 00931da8 */)))) {
        if (CONTACT_ALL3(v4_cmplt(ny, v4_splat(0.4f /* 00935860 */)))) {
            P = v4_mul(v4_splat(bike_hsum_012(v4_mul(n, D))), n);
        } else {
            const rx_v4 proj = v4_mul(v4_splat(bike_hsum_012(v4_mul(n, D))), n);
            const rx_v4 t = v4_div(v4_sub(ny, v4_splat(0.6f)), v4_sub(v4_splat(0.4f), v4_splat(0.6f)));
            P = v4_add(v4_mul(v4_sub(proj, D), t), D);
        }
    }
    const rx_v4 KP = v4_mul(v4_splat(F32(0, G_CORR_SCALE)), P);
    return wheel_correction_tail(W, S, up, KP);
}
RX_FUNCTION(0x00431ee0, bike_wheel_position_correction)

/* 0043cfe0 BikeWheel_ContactPositionCorrectionRiderOff (ECX = W, EDX = S, stack out; ret 4; EAX = out): the same with the rider off
 * the bike: always only the part of D along the contact normal. */
rx_v4 bike_wheel_position_correction_rider_off(rx_addr W, rx_addr S) {
    const rx_v4 up = wheel_world_up(S);
    const rx_v4 n = rx_ld4(W + 0x40);
    const rx_v4 D = v4_sub(rx_ld4(W + 0x50), rx_ld4(W + 0x60));
    const rx_v4 KP = v4_mul(v4_mul(v4_splat(bike_hsum_012(v4_mul(D, n))), n), v4_splat(F32(0, G_CORR_SCALE)));
    return wheel_correction_tail(W, S, up, KP);
}
RX_FUNCTION(0x0043cfe0, bike_wheel_position_correction_rider_off)

/* ==== 00438180 BodyProbe_ApplyAverageCorrection ============================================================================== */
/* ECX = S: the corrections of the touching body probes (S+17d0 + 16k, flags S+1af0 + 4k, count S+1318) and of the body contacts
 * (S+21c0 + 16k, count S+20f8) are averaged (sum / number of them, "number" counted in a vec4 as N = [00d61650] each) and every
 * body (count S+ec) is translated by the average. */
void bike_body_probe_apply_average_correction(rx_addr S) {
    const rx_v4 N = rx_ld4(G_NUMERATOR);
    rx_v4 sum = v4_splat(F32(0, 0x0092e12c)), cnt = sum;   /* 0 */
    for (int32_t k = 0, n = I32(S, 0x1318); k < n; k++) {
        if (U32(S, 0x1af0 + 4u * (uint32_t)k) == 0) continue;
        sum = v4_add(rx_ld4(S + 0x17d0 + 16u * (uint32_t)k), sum);
        cnt = v4_add(N, cnt);
    }
    for (int32_t k = 0, n = I32(S, 0x20f8); k < n; k++) {
        sum = v4_add(rx_ld4(S + 0x21c0 + 16u * (uint32_t)k), sum);
        cnt = v4_add(N, cnt);
    }
    if (!CONTACT_ALL3(v4_cmplt(rx_ld4(G_ZERO), cnt))) return;
    const rx_v4 avg = v4_div(sum, cnt);
    CONTACT_AVERAGE_BEGIN();
    for (int32_t i = 0; i < I32(S, 0xec); i++) {
        const rx_addr B = S + 0xa0u * (uint32_t)i + 0xf0;
        rx_st4(B, v4_add(rx_ld4(B), avg));
        CONTACT_MOVED(B);
    }
}
RX_FUNCTION(0x00438180, bike_body_probe_apply_average_correction)

/* ==== 004456d0 BikeWheel_ApplyContactCorrection ============================================================================== */
/* EDX = S (ret 0): the position corrections of the touching wheels (front S+88e0 when S+89dc, rear S+8790 when S+888c; the rider-off
 * variant when S+2374) are averaged and every body is translated by the average; then 438180 does the same for body contacts. */
void bike_wheel_apply_contact_correction(rx_addr S) {
    const rx_v4 N = rx_ld4(G_NUMERATOR);
    rx_v4 front = v4_splat(F32(0, 0x0092e12c)), rear = front, cnt = front;
    if (I32(S, 0x89dc) != 0) {
        front = U32(S, 0x2374) ? bike_wheel_position_correction_rider_off(S + 0x88e0, S) : bike_wheel_position_correction(S + 0x88e0, S);
        cnt = v4_add(cnt, N);
    }
    if (I32(S, 0x888c) != 0) {
        rear = U32(S, 0x2374) ? bike_wheel_position_correction_rider_off(S + 0x8790, S) : bike_wheel_position_correction(S + 0x8790, S);
        cnt = v4_add(cnt, N);
    }
    if (bike_all3(v4_cmplt(rx_ld4(G_ZERO), cnt))) {
        const rx_v4 avg = v4_div(v4_add(rear, front), cnt);
        for (int32_t i = 0; i < I32(S, 0xec); i++) {
            const rx_addr B = S + 0xa0u * (uint32_t)i + 0xf0;
            rx_st4(B, v4_add(avg, rx_ld4(B)));
        }
    }
    bike_body_probe_apply_average_correction(S);
}
RX_FUNCTION(0x004456d0, bike_wheel_apply_contact_correction)

/* ==== calls out of this file (world / collision code: the owners' core functions) ============================================ */

/* 00402dc0 (world): EDX = registry (entries +0, count +8); returns in EAX the entry whose id is the current scene's, or 0. */
static rx_addr contact_current_terrain(rx_addr registry) {
    rx_addr r;
    PHYS_CALL_SP(0, r = rx_world_find_by_controller(registry));
    return r;
}
/* 007c6160 TerrainTriangles (world): ECX = &position vec4; stack terrain, triangle buffer, &count, capacity, &extent vec4; ret 0x14.
 * Writes the triangles around the position into the buffer (48 bytes each) and their number into *count. */
static void contact_terrain_triangles(rx_addr terrain, rx_addr buffer, rx_addr count, uint32_t capacity, rx_addr extent, rx_addr position) {
    PHYS_CALL_SP(0x14, phys_terrain_triangles(position, terrain, buffer, count, (int32_t)capacity, extent));
}
/* 0042db20 (collision): ECX = &body position, EDX = &contact normal; stack W, &contact point, &body orientation, &root orientation;
 * ret 0x10. Builds the wheel's contact record W from the contact. */
static void contact_build_wheel_record(rx_addr W, rx_addr position, rx_addr normal, rx_addr point, rx_addr body_q, rx_addr root_q) {
    PHYS_CALL_SP(0x10, phys_probe_frame_setup(position, normal, W, point, body_q, root_q));
}
#define TRIANGLE_BUFFER  0x00ad4c30u   /* up to 400 terrain triangles of 48 bytes (global) */
#define SCENE_REGISTRY   0x009e30d4u   /* the terrain registry (entries, count at +8 = 0x009e30dc) */

/* ==== 00444a40 BikeWheel_QueryTerrainContact ================================================================================== */
/* ESI = W, stack S (ret 4): the terrain contact of one wheel. The wheel's body (W+0) position P and orientation q; the terrain
 * triangles within a box of side 1.2 around P (007c6160, count -> S+8788); each is tested against the wheel shape W+10 (438260)
 * and on a hit the contact record W is built (42db20). W+fc = hit flag. The locals are passed by address to the callees and live in a
 * frame laid out like the original's: +10 box extent, then the contact normal; +20 query position (then the contact point);
 * +30 body orientation; +40 body position; +50 root orientation. */
void bike_wheel_query_terrain_contact(rx_addr W, rx_addr S) {
    const rx_addr body = S + (uint32_t)(I32(W, 0) * 0xa0);
    RX_FRAME_ENTER(Fr, 0x80);
    const rx_addr F = (Fr + 15) & ~15u;
    v4_st(F + 0x50, rx_ld4(S + 0x730));
    v4_st(F + 0x30, rx_ld4(body + 0x730));
    v4_st(F + 0x10, v4_splat(1.2f /* 00930064 */));
    const rx_v4 P = rx_ld4(body + 0xf0);
    v4_st(F + 0x40, P);
    v4_st(F + 0x20, v4_add(v4_splat(F32(0, 0x0092e12c)), P));
    if (!(U8(0, 0x009e30e0) & 1)) {              /* function-local static: the registry's count */
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    /* the triangle buffer is one global for every MX simulation: a parallel simulation job's own copy (loop_jobs.h rx_job_scratch) */
    const rx_addr tri = rx_job_scratch(TRIANGLE_BUFFER, 400 * 48);
    contact_terrain_triangles(contact_current_terrain(SCENE_REGISTRY), tri, S + 0x8788, 400, F + 0x10, F + 0x20);
    const uint32_t count = U32(S, 0x8788);
    rx_job_scratch_used(TRIANGLE_BUFFER, count * 48u);
    if (count == 0) {
        SU32(W, 0xfc, 0);
    } else {
        const uint32_t hit = bike_wheel_triangle_contact(F + 0x20, F + 0x10, F + 0x30, F + 0x40, W + 0x10, tri,
                                                          (int32_t)count, NULL);
        SU32(W, 0xfc, hit);
        if (hit) contact_build_wheel_record(W, F + 0x40, F + 0x10, F + 0x20, F + 0x30, F + 0x50);
    }
    RX_FRAME_LEAVE(Fr, 0x80);
}
RX_FUNCTION(0x00444a40, bike_wheel_query_terrain_contact)
