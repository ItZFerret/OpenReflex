/* race_math.c -- small geometry helpers of the race / AI code (driver and behaviour support, 0x00508xxx-0x0050bxxx).
 * (core/notes/race.md) */
#include "race.h"
#include "race_extern.h"
#include "build_activity.h"

/* 005084f0 (EAX = vehicle, ECX = vector, in place): the vector into the vehicle's frame: v' = conj(q) * v * q with the vehicle's
 * orientation quaternion q (+0xe0 x y z w). Every product and sum in double, each half rounded to float, as the original. */
void rx_race_to_vehicle_frame(rx_addr vehicle, rx_addr v) {
    const float qx = F32(vehicle, 0xe0), qy = F32(vehicle, 0xe4), qz = F32(vehicle, 0xe8), qw = F32(vehicle, 0xec);
    const float vx = F32(v, 0), vy = F32(v, 4), vz = F32(v, 8);
    const float cx = -0.0f /* 0x9774a8 */ - qx, cy = -0.0f - qy, cz = -0.0f - qz;
    const double Z = 0.0 /* 0x977548 */;
    /* t = conj(q) * (v, 0) */
    float tw = (float)((((double)qw * Z - (double)cx * vx) - (double)vy * cy) - (double)vz * cz);
    float tx = (float)((((double)qw * vx + (double)cx * Z) + (double)vz * cy) - (double)vy * cz);
    float ty = (float)((((double)qw * vy + (double)cy * Z) + (double)vx * cz) - (double)cx * vz);
    float tz = (float)((((double)qw * vz + (double)cz * Z) + (double)cx * vy) - (double)vx * cy);
    /* v' = t * q (vector part) */
    SF32(v, 0, (float)((((double)qw * tx + (double)qx * tw) + (double)qz * ty) - (double)qy * tz));
    SF32(v, 4, (float)((((double)qy * tw + (double)qw * ty) + (double)qx * tz) - (double)qz * tx));
    SF32(v, 8, (float)((((double)qz * tw + (double)qw * tz) + (double)qy * tx) - (double)qx * ty));
}
RX_FUNCTION(0x005084f0, rx_race_to_vehicle_frame)

/* 005089d0 (ECX = record; returns it in EAX): a pose record reset: +0x00 the identity rotation (16 bytes at 0x9cb550),
 * +0x10 the zero vector (0xa95d50), +0x20 / +0x21 two flags cleared. */
rx_addr rx_race_pose_reset(rx_addr rec) {
    for (uint32_t i = 0; i < 16; i += 4) SU32(rec, i, U32(0, 0x9cb550 + i));
    for (uint32_t i = 0; i < 12; i += 4) SU32(rec, 0x10 + i, U32(0, 0xa95d50 + i));
    SU8(rec, 0x20, 0);
    SU8(rec, 0x21, 0);
    return rec;
}
RX_FUNCTION(0x005089d0, rx_race_pose_reset)

/* 0050b850 (ECX = point, EDX = segment end b, args: segment start a, radius; ESI = out (or 0); cdecl) -> EAX: does the point
 * project onto the segment a -> b (0 <= t <= 1) closer than radius? Then the closest point goes to out. */
int rx_race_point_near_segment(rx_addr p, rx_addr a, rx_addr b, float radius, rx_addr out) {
    const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8), px = F32(p, 0), py = F32(p, 4), pz = F32(p, 8);
    const float dx = F32(b, 0) - ax, dy = F32(b, 4) - ay, dz = F32(b, 8) - az;
    float t = rx_divs(((py - ay) * dy + (px - ax) * dx) + (pz - az) * dz, (dx * dx + dy * dy) + dz * dz);
    if (0.0f > t || t > 1.0f /* 0x931f24 */) return 0;
    const float cx = ax + dx * t, cy = F32(a, 4) + dy * t, cz = F32(a, 8) + dz * t;
    const float ex = px - cx, ey = py - cy, ez = pz - cz;
    if (!(radius * radius > (ey * ey + ex * ex) + ez * ez)) return 0;
    if (out != 0) { SF32(out, 0, cx); SF32(out, 4, cy); SF32(out, 8, cz); }
    return 1;
}
RX_FUNCTION(0x0050b850, rx_race_point_near_segment)

/* 0050b990 (EDX = p0, EAX = p1, arg = q0, ESI = q1, EDI = out (or 0); cdecl) -> EAX: do the segments p0 -> p1 and q0 -> q1 cross
 * in the ground plane (x / z), strictly inside both (0 < s < 1, 0 < t < 1)? The crossing point (y = 0) goes to out; it is placed
 * on q0 -> q1 with p's parameter s, as the original does. */
int rx_race_segments_cross_xz(rx_addr p0, rx_addr p1, rx_addr q0, rx_addr q1, rx_addr out) {
    const float pdx = F32(p1, 0) - F32(p0, 0), qdx = F32(q1, 0) - F32(q0, 0);
    const float den = (F32(p1, 8) - F32(p0, 8)) * qdx - (F32(q1, 8) - F32(q0, 8)) * pdx;
    if (den == 0.0f /* 0x92e12c */) return 0;
    const float rz = F32(q0, 8) - F32(p0, 8), rx = F32(q0, 0) - F32(p0, 0);
    const float inv = 1.0f /* 0x931f24 */ / den;
    const float s = (rz * pdx - (F32(p1, 8) - F32(p0, 8)) * rx) * inv;
    const float t = (rz * qdx - (F32(q1, 8) - F32(q0, 8)) * rx) * inv;
    if (!(s > 0.0f) || !(1.0f > s) || !(t > 0.0f) || !(1.0f > t)) return 0;
    if (out != 0) {
        SF32(out, 4, 0.0f);
        SF32(out, 0, qdx * s + F32(q0, 0));
        SF32(out, 8, (F32(q1, 8) - F32(q0, 8)) * s + F32(q0, 8));
    }
    return 1;
}
RX_FUNCTION(0x0050b990, rx_race_segments_cross_xz)

/* 0050a650 (EAX = node, EDI = out position, args: out direction, distance; RET 8): the track ahead of a node, averaged: the mean
 * of the node positions and of their directions (+0x50) over the nodes within `distance` along the line (at least the node
 * itself); the result is the mean position moved 15 units along the normalized mean direction. Both are 0 when distance <= 0. */
void race_look_ahead(rx_addr node, float distance, float pos[3], float dir[3]) {
    pos[0] = pos[1] = pos[2] = 0.0f;
    dir[0] = dir[1] = dir[2] = 0.0f;
    float prev_x = F32(node, 0), prev_y = F32(node, 4), prev_z = F32(node, 8);
    float count = 0.0f;
    if (!(distance > 0.0f)) return;
    do {
        if (node == 0) break;
        pos[0] = F32(node, 0) + pos[0];
        pos[1] = pos[1] + F32(node, 4);
        pos[2] = pos[2] + F32(node, 8);
        dir[0] = F32(node, 0x50) + dir[0];
        dir[1] = F32(node, 0x54) + dir[1];
        dir[2] = F32(node, 0x58) + dir[2];
        const float x = F32(node, 0), y = F32(node, 4), z = F32(node, 8);
        const float dx = x - prev_x, dy = y - prev_y, dz = z - prev_z;
        distance = distance - sqrtf((dy * dy + dx * dx) + dz * dz);
        prev_x = x; prev_y = y; prev_z = z;
        node = U32(node, 0x64);
        count = count + 1.0f /* 0x931f24 */;
    } while (distance > 0.0f);
    if (count == 0.0f) return;
    pos[0] = pos[0] / count;
    pos[2] = pos[2] / count;
    pos[1] = pos[1] / count;
    const float len2 = (dir[1] * dir[1] + dir[0] * dir[0]) + dir[2] * dir[2];
    if (len2 > 0.0f) {
        const float inv = 1.0f / sqrtf(len2);
        dir[0] = dir[0] * inv; dir[1] = dir[1] * inv; dir[2] = dir[2] * inv;
    }
    const float k = 15.0f /* 0x9322d0 */;
    pos[0] = pos[0] + dir[0] * k;
    pos[1] = pos[1] + dir[1] * k;
    pos[2] = pos[2] + dir[2] * k;
}
void rx_track_look_ahead(rx_addr node, rx_addr out_pos, rx_addr out_dir, float distance) {
    float pos[3], dir[3];
    race_look_ahead(node, distance, pos, dir);
    for (int i = 0; i < 3; i++) { SF32(out_pos, 4 * i, pos[i]); SF32(out_dir, 4 * i, dir[i]); }
}
RX_FUNCTION(0x0050a650, rx_track_look_ahead)

/* 0050a5f0 (args: node, out position (x, z), distance; ESI = out direction (x, z); RET 0xc): rx_track_look_ahead flattened to
 * the ground plane. */
void rx_track_look_ahead_xz(rx_addr node, rx_addr out_pos, float distance, rx_addr out_dir) {
    float pos[3], dir[3];
    race_look_ahead(node, distance, pos, dir);
    SF32(out_pos, 0, pos[0]); SF32(out_pos, 4, pos[2]);
    SF32(out_dir, 0, dir[0]); SF32(out_dir, 4, dir[2]);
}
RX_FUNCTION(0x0050a5f0, rx_track_look_ahead_xz)

/* The game's own random generator (inlined in several functions): seed 0x9cb860 = seed * 0x41c64e6d + 0x3039, calls 0x9cb864;
 * result bits 16..30. */
uint32_t race_rand(void) {
    SU32(0, 0x9cb864, U32(0, 0x9cb864) + 1);
    uint32_t seed = U32(0, 0x9cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x9cb860, seed);
    return (seed >> 16) & 0x7fff;
}

/* 0050a360 (args: driver, candidate sections, count, prefer flag; RET 0x10) -> EAX: the section a driver takes at a fork.
 * Each candidate has per vehicle class / kind flags (+0x18 + 4 * k, k = vehicle +0x198 + 4 * vehicle class +0x194) and a speed
 * (+0x98 + 4 * k). The candidates flagged 1 and 0x20 (with the prefer flag: 0x20 or 2) are drawn among at random; without any,
 * each candidate in turn: flag 2 = taken with probability 1/2 (immediately), 0x200 / 0x400 = taken when the driver's navigate
 * behaviour's side (+0xc) is -1 / 1, 8 = when the vehicle's speed (+0x150) is less than 5 below the candidate's, 4 = when the
 * candidate's speed is higher, 0x10 = when lower (the last one that qualifies). *slot = the original's argument slot 1, which
 * holds the last qualifying candidate (0 on the random path). */
rx_addr rx_race_choose_section(rx_addr driver, rx_addr list, uint32_t count, uint32_t prefer, rx_addr* slot) {
    rx_addr vehicle = U32(U32(driver, 0x74), 0x1ac);
    rx_addr picked[32];   /* the original's stack array holds 6 */
    uint32_t n = 0;
    rx_addr chosen = 0;
    *slot = 0;
    const uint32_t want = prefer != 0 ? 0x22 : 0x20;
    if (count != 0) {
        const uint32_t k = U32(vehicle, 0x198) + 4 * U32(vehicle, 0x194);
        for (uint32_t i = 0; i < count; i++) {
            rx_addr c = U32(list, 4 * i);
            uint32_t f = U32(c, 0x18 + 4 * k);
            if ((f & 1) && (f & want)) picked[n++] = c;
        }
    }
    if (n != 0) return picked[race_rand() % n];
    for (uint32_t i = 0; i < count; i++) {
        const uint32_t k = U32(vehicle, 0x198) + 4 * U32(vehicle, 0x194);
        rx_addr c = U32(list, 4 * i);
        uint32_t f = U32(c, 0x18 + 4 * k);
        if (f & 2) {
            if (race_rand() & 1) return U32(list, 4 * i);
        } else if (f & 0x200) {
            if (I32(rx_driver_navigate_behavior(driver), 0xc) == -1) chosen = U32(list, 4 * i);
        } else if (f & 0x400) {
            if (I32(rx_driver_navigate_behavior(driver), 0xc) == 1) chosen = U32(list, 4 * i);
        } else if (f & 8) {
            if (5.0f /* 0x935890 */ > F32(c, 0x98 + 4 * k) - F32(vehicle, 0x150)) chosen = c;
        } else if (f & 4) {
            if (F32(c, 0x98 + 4 * k) > F32(vehicle, 0x150)) chosen = c;
        } else if (f & 0x10) {
            if (F32(vehicle, 0x150) > F32(c, 0x98 + 4 * k)) chosen = c;
        }
        *slot = chosen;
    }
    return chosen;
}
RX_FUNCTION(0x0050a360, rx_race_choose_section)

/* 0050aa70 (EAX = driver, ECX = node) -> EAX: is the driver's vehicle on the track (second edge pair) within the track's search
 * window ahead of the node (track +0x1b8 by vehicle class), following into the next sections? */
int rx_race_on_track_ahead(rx_addr driver, rx_addr node) {
    rx_addr vehicle = U32(U32(driver, 0x74), 0x1ac);
    race_track_query q = {vehicle + 0xc0, U32(driver, 0x14), node, 0, F32(U32(driver, 0x10), 0x1b8 + 4 * U32(vehicle, 0x194)), 0.0f, 0, 1};
    rx_addr sec, nd;
    return race_track_find(&q, 1, &sec, &nd) != 0;
}
RX_FUNCTION(0x0050aa70, rx_race_on_track_ahead)

/* 0050b150 (ECX = a, EAX = b, ESI = direction; all (x, y) pairs) -> xmm0: 1 - clamp(dot(normalize(a - b), direction), 0, 0.5). */
float rx_race_heading_factor(rx_addr a, rx_addr b, rx_addr dir) {
    float dx = F32(a, 0) - F32(b, 0), dy = F32(a, 4) - F32(b, 4);
    const float l2 = dx * dx + dy * dy;
    if (l2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
        dx = dx * inv;
        dy = inv * dy;
    }
    float dot = rx_adds(rx_muls(F32(dir, 4), dy), rx_muls(F32(dir, 0), dx));
    if (!(dot > 0.5f /* 0x9322cc */) && 0.0f > dot) return 1.0f - 0.0f;
    if (dot > 0.5f) dot = 0.5f;
    return rx_subs(1.0f, dot);
}
RX_FUNCTION(0x0050b150, rx_race_heading_factor)

/* Vehicle vf43 (slot 0x2b; RavenVehicleMotocross 00466ae0 = bike +0x8d98): nonzero while the bike is in the air (the behaviours
 * treat it as airborne). Vehicle vf38 (slot 0x26; 00466960 = the body's angular velocity, bike +0x740) into a 16-byte vector. */
typedef uint32_t (*race_vehicle_query_fn)(rx_addr vehicle);
typedef void (*race_vehicle_vec_fn)(rx_addr vehicle, rx_addr out);
static uint32_t vehicle_airborne(rx_addr vehicle) {
    rx_addr fn = race_vfn(vehicle, 0x2b);
    race_vehicle_query_fn f = RX_FN(fn, race_vehicle_query_fn);
    return f ? f(vehicle) : race_thiscall(fn, vehicle, 0, 0, 0, 0, 0);
}
static void vehicle_vector_26(rx_addr vehicle, rx_addr out) {
    rx_addr fn = race_vfn(vehicle, 0x26);
    race_vehicle_vec_fn f = RX_FN(fn, race_vehicle_vec_fn);
    if (f) f(vehicle, out); else race_thiscall(fn, vehicle, 1, out, 0, 0, 0);
}

/* 0050b220 (ESI = driver, EDI = controls): throttle back when the vehicle turns fast: with the vehicle's vf38 vector's y
 * (yaw rate) beyond a threshold by the driver's skill (+0x32c: 0 -> 0.5, 2 -> 0.4, otherwise 0.6; skill 1: never) the throttle
 * (+0x00) is scaled down linearly (to 0.1 at 1.5) and +0x1c by 0.9. Not while the bike is in the air. */
void rx_race_limit_throttle_in_turns(rx_addr driver, rx_addr controls) {
    rx_addr vehicle = U32(U32(driver, 0x74), 0x1ac);
    if (vehicle_airborne(vehicle) != 0) return;
    float limit = 0.6f /* 0x931da8 */;
    switch (U32(driver, 0x32c)) {
    case 0: limit = 0.5f /* 0x9322cc */; break;
    case 1: return;
    case 2: limit = 0.4f /* 0x935860 */; break;
    default: break;
    }
    RACE_FRAME16_ENTER(F, 0x10);
    vehicle_vector_26(U32(U32(driver, 0x74), 0x1ac), F);
    const float rate = fabsf(F32(F, 4));
    RACE_FRAME16_LEAVE(F);
    if (!(rate > limit)) return;
    const float slope = -0.9f /* 0x977744 */ / (1.5f /* 0x9322a4 */ - limit);
    const float factor = (1.0f - slope * limit) + rate * slope;
    SF32(controls, 0, factor * F32(controls, 0));
    SF32(controls, 0x1c, F32(controls, 0x1c) * F32(0, 0x9cd970) /* 0.9, .data */);
}
RX_FUNCTION(0x0050b220, rx_race_limit_throttle_in_turns)

/* 0050b0f0 (xmm1 = x, args: a, b, c, base; RET 0x10) -> xmm0: c / base^x - a / base^b (pow in double, as the CRT computes it). */
float rx_race_pow_difference(float x, float a, float b, float c, float base) {
    const float t1 = c / (float)race_pow((double)base, (double)x);
    const float t2 = a / (float)race_pow((double)base, (double)b);
    return t1 - t2;
}
RX_FUNCTION(0x0050b0f0, rx_race_pow_difference)

/* RavenVehicle::vf15 (slot 15, 0x00430220): copies the vehicle's 4x4 world matrix (+0xf0) to out. */
typedef void (*race_vehicle_matrix_fn)(rx_addr vehicle, rx_addr out);
static void vehicle_matrix(rx_addr vehicle, rx_addr out) {
    rx_addr fn = race_vfn(vehicle, 15);
    race_vehicle_matrix_fn f = RX_FN(fn, race_vehicle_matrix_fn);
    if (f) f(vehicle, out); else race_thiscall(fn, vehicle, 1, out, 0, 0, 0);
}

/* 0050a8b0 (ESI = driver, args: side (-1 behind / 1 ahead), distance, width; RET 0xc) -> EAX: is another racer of the activity
 * within `distance` behind (side -1) or ahead (side 1) of the driver's vehicle and within `width` of its line? Ahead / behind
 * along the vehicle's heading (its matrix's first row, flat, normalized); sideways along the vehicle's +0x130 / +0x138 axis. */
int rx_race_rider_near(rx_addr driver, int side, float distance, float width) {
    rx_addr vehicle = U32(U32(driver, 0x74), 0x1ac);
    RACE_FRAME16_ENTER(F, 0x40);
    vehicle_matrix(vehicle, F);
    float hx = F32(F, 0), hz = F32(F, 8);
    RACE_FRAME16_LEAVE(F);
    vehicle = U32(U32(driver, 0x74), 0x1ac);
    const float ox = F32(vehicle, 0xc0), oz = F32(vehicle, 0xc8);
    const float len2 = hx * hx + hz * hz;
    if (len2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        hz = inv * hz;
        hx = inv * hx;
    }
    rx_addr act = U32(U32(driver, 0x74), 0x94);
    const int32_t n = I32(act, 0x26c);
    rx_addr list = U32(act, 0x268);
    for (int32_t i = 0; i < n; i++, list += 4) {
        rx_addr other = U32(list, 0);
        if (other == driver) continue;
        rx_addr ov = U32(U32(other, 0x74), 0x1ac), mv = U32(U32(driver, 0x74), 0x1ac);
        const float dx = F32(ov, 0xc0) - ox, dz = F32(ov, 0xc8) - oz;
        const float lateral = fabsf(F32(mv, 0x138) * dz + dx * F32(mv, 0x130));
        if (!(width > lateral)) continue;
        const float along = dz * hz + dx * hx;
        if (side == -1) {
            if (along > -0.0f /* 0x9774a8 */ - distance && 0.0f > along) return 1;
        } else if (side == 1) {
            if (distance > along && along > 0.0f /* 0x92e12c */) return 1;
        }
    }
    return 0;
}
RX_FUNCTION(0x0050a8b0, rx_race_rider_near)

/* RavenVehicle::vf14 (slot 14): a pointer to the vehicle's world matrix (rows x, y, z, translation). */
typedef rx_addr (*race_vehicle_matrix_ptr_fn)(rx_addr vehicle);
static rx_addr vehicle_matrix_ptr(rx_addr vehicle) {
    rx_addr fn = race_vfn(vehicle, 14);
    race_vehicle_matrix_ptr_fn f = RX_FN(fn, race_vehicle_matrix_ptr_fn);
    return f ? f(vehicle) : race_thiscall(fn, vehicle, 0, 0, 0, 0, 0);
}

/* 00509fc0 (args: driver, out centre ray; EDI = out left ray, ESI = out right ray; RET 8): three probe rays from the back of the
 * driver's vehicle along its flat velocity: start at the back (half the class length behind, table 0x93582c by vehicle class
 * +0x194) in the middle and half the class width (table 0x93580c) to either side along the matrix's x row, at height 0; end 1000
 * units further along the normalized velocity (+0x160 / +0x168). Ray: +0x00 start, +0x10 end. */
void rx_race_probe_rays(rx_addr driver, rx_addr centre, rx_addr left, rx_addr right) {
    rx_addr v = U32(U32(driver, 0x74), 0x1ac);
    const float px = F32(v, 0xc0), pz = F32(v, 0xc8);
    float dx = F32(v, 0x160), dy = 0.0f, dz = F32(v, 0x168);
    const float len2 = (dx * dx + 0.0f * 0.0f) + dz * dz;
    if (len2 > 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        dx = inv * dx;
        dy = inv * 0.0f /* 0x92e12c */;
        dz = dz * inv;
    }
    v = U32(U32(driver, 0x74), 0x1ac);
    const uint32_t cls = U32(v, 0x194);
    const float half_len = F32(0, 0x93582c + 4 * cls) * 0.5f /* 0x9322cc */;
    const float half_w = F32(0, 0x93580c + 4 * cls) * 0.5f;
    rx_addr M = vehicle_matrix_ptr(v);
    const float m0 = F32(M, 0), m2 = F32(M, 8), m4 = F32(M, 0x10), m6 = F32(M, 0x18), m8 = F32(M, 0x20), m10 = F32(M, 0x28);
    const float nw = -0.0f /* 0x9774a8 */ - half_w, nl = -0.0f - half_len;
    const float A = m4 * 0.0f, C = m6 * 0.0f, B8 = m8 * nl, B10 = m10 * nl;
    /* rear points in the matrix frame: (side, 0, -half length) */
    const float lx = ((m0 * nw + A) + B8) + px, lz = pz + ((m2 * nw + C) + B10);
    const float rx = ((m0 * half_w + A) + B8) + px, rz = pz + ((m2 * half_w + C) + B10);
    const float cx = ((m0 * 0.0f + A) + B8) + px, cz = pz + ((m2 * 0.0f + C) + B10);
    const float y0 = 0.0f + 0.0f;
    const float reach = F32(0, 0x9cf5b0);   /* 1000.0, .data */
    const float ex = dx * reach, ey = dy * reach, ez = dz * reach;
    SF32(centre, 4, y0); SF32(centre, 0, cx); SF32(centre, 8, cz);
    SF32(centre, 0x18, ez + cz); SF32(centre, 0x10, ex + cx); SF32(centre, 0x14, ey + y0);
    SF32(left, 0, lx); SF32(left, 4, y0); SF32(left, 8, lz);
    SF32(left, 0x14, ey + y0); SF32(left, 0x10, ex + lx); SF32(left, 0x18, ez + lz);
    SF32(right, 0, rx); SF32(right, 4, y0); SF32(right, 8, rz);
    SF32(right, 0x10, ex + rx); SF32(right, 0x14, ey + y0); SF32(right, 0x18, ez + rz);
}
RX_FUNCTION(0x00509fc0, rx_race_probe_rays)

/* 0053fa30 (thiscall, RET 4): a vehicle part package's state. */
static uint32_t part_package_pending(rx_addr part, uint32_t flag) { uint32_t r; RACE_CORE_CALL(1, r = rx_part_packages_ready(part, flag)); return r; }

/* 0053ffb0 (arg = vehicle parts; RET 4) -> EAX: is any of the 17 part slots (+0x50) still loading (0053fa30 with the +0xc0 flag)? */
int rx_race_parts_loading(rx_addr parts) {
    for (uint32_t i = 0; i < 0x11; i++) {
        rx_addr part = U32(parts, 0x50 + 4 * i);
        if (part != 0 && part_package_pending(part, U32(parts, 0xc0)) != 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x0053ffb0, rx_race_parts_loading)

/* Projection parameter of p onto the segment a -> b in the ground plane, clamped to [0, 1] (a NaN gives 1). */
static float clamped_t(float ax, float az, float ex, float ez, float px, float pz) {
    float t = rx_divs((pz - az) * ez + (px - ax) * ex, ez * ez + ex * ex);
    if (1.0f /* 0x931f24 */ > t && 0.0f > t) t = 0.0f;
    else if (!(1.0f > t)) t = 1.0f;
    return t;
}

/* 00509e30 (ECX = driver, EDX = position) -> xmm0: where the position is across the track in the driver's segment (+0x18 ->
 * +0x1c): 0 at the left edge (+0x10), 1 at the right edge (+0x20), from the nearest points of both edges (ground plane). */
float rx_race_track_lateral(rx_addr driver, rx_addr pos) {
    rx_addr a = U32(driver, 0x18), b = U32(driver, 0x1c);
    const float px = F32(pos, 0), pz = F32(pos, 8);
    const float lax = F32(a, 0x10), laz = F32(a, 0x18), lex = F32(b, 0x10) - lax, lez = F32(b, 0x18) - laz;
    const float t = clamped_t(lax, laz, lex, lez, px, pz);
    const float lx = rx_adds(rx_muls(lex, t), lax), lz = rx_adds(rx_muls(lez, t), laz);
    const float rax = F32(a, 0x20), raz = F32(a, 0x28), rex = F32(b, 0x20) - rax, rez = F32(b, 0x28) - raz;
    const float u = clamped_t(rax, raz, rex, rez, px, pz);
    const float wx = rx_subs(rx_adds(rx_muls(rex, u), rax), lx), wz = rx_subs(rx_adds(rx_muls(rez, u), raz), lz);
    return rx_divs(rx_adds(rx_muls(wz, rx_subs(pz, lz)), rx_muls(wx, rx_subs(px, lx))), rx_adds(rx_muls(wz, wz), rx_muls(wx, wx)));
}
RX_FUNCTION(0x00509e30, rx_race_track_lateral)

/* 0050a570 (EAX = vector, ESI = out (x, z)): the vector's ground-plane direction, normalized (left as is when zero). */
void rx_race_flat_direction(rx_addr v, rx_addr out) {
    const float x = F32(v, 0), z = F32(v, 8);
    SF32(out, 0, x);
    SF32(out, 4, z);
    const float l2 = x * x + z * z;
    if (l2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
        SF32(out, 0, x * inv);
        SF32(out, 4, inv * z);
    }
}
RX_FUNCTION(0x0050a570, rx_race_flat_direction)

/* 0050ae20 (ESI = driver, arg = dt; RET 4) -> EAX: the AI's lean-to-recover sequence (driver +0x2f8 state, +0x2fc side, +0x304
 * end time, +0x308 timer, output +0x3d4 lean in -1..1, +0x3dc). The vehicle's flat heading (its matrix's x row) against the vector
 * at vehicle +0x140 gives `dot`; state 1 leans to the side until the heading turns past 0.766 (cos 40 deg), gives up at the end
 * time, and when the bike leaves the ground (vf43) holds on for 0.1 s (state 2, to 0.866) and then eases (state 3: lean = dot * 0.6
 * against the side) until it lands. Returns whether the sequence still runs.
 * With a zero heading the original reads three floats of its own stack it never wrote; the core reads the same addresses
 * (exact when called through the adapter, the original's frame being where the adapter's caller put it). */
int rx_race_lean_recover(rx_addr d, float dt, rx_addr orig_frame) {
    rx_addr v = U32(U32(d, 0x74), 0x1ac);
    RACE_FRAME16_ENTER(F, 0x40);
    vehicle_matrix(v, F);
    const float mx = F32(F, 0), mz = F32(F, 8);
    RACE_FRAME16_LEAVE(F);
    float hx, hy, hz;
    const float l2 = mx * mx + mz * mz;
    if (l2 > 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
        hx = inv * mx; hz = inv * mz; hy = inv * 0.0f;
    } else {
        hz = F32(orig_frame, 0x18); hy = F32(orig_frame, 0x14); hx = F32(orig_frame, 0x10);
    }
    v = U32(U32(d, 0x74), 0x1ac);
    const float dot = (F32(v, 0x148) * hz + F32(v, 0x144) * hy) + F32(v, 0x140) * hx;
    float lean;
    switch (U32(d, 0x2f8)) {
    case 0:
        SF32(d, 0x3dc, 0.0f);
        SF32(d, 0x3d4, 0.0f);
        return U32(d, 0x2f8) != 0;
    case 1:
        if (vehicle_airborne(U32(U32(d, 0x74), 0x1ac)) != 0) {
            SU32(d, 0x2f8, 2);
            SF32(d, 0x308, 0.1f /* 0x931d94 */);
        } else if (F32(d, 0x130) > F32(d, 0x304)) {
            SU32(d, 0x2f8, 0);
            SF32(d, 0x308, 0.0f);
            SF32(d, 0x3d4, 0.0f);
            return U32(d, 0x2f8) != 0;
        }
        if (U32(d, 0x2fc) != 0) lean = 0.766f /* 0x9779e0 */ > dot ? 1.0f : 0.0f;
        else lean = dot > -0.766f /* 0x9779dc */ ? F32(0, 0x9cd9e8) /* -1 */ : 0.0f;
        break;
    case 2: {
        const float t = F32(d, 0x308) - dt;
        SF32(d, 0x308, t);
        if (0.0f > t) SU32(d, 0x2f8, 3);
        if (U32(d, 0x2fc) != 0) lean = 0.866f /* 0x9779d8 */ > dot ? 1.0f : 0.0f;
        else lean = dot > -0.866f /* 0x9779d4 */ ? F32(0, 0x9cd9e8) : 0.0f;
        break;
    }
    case 3:
        SF32(d, 0x308, F32(d, 0x308) - dt);
        if (vehicle_airborne(U32(U32(d, 0x74), 0x1ac)) == 0) { SU32(d, 0x2f8, 0); SF32(d, 0x308, 0.0f); }
        lean = U32(d, 0x2fc) != 0 ? dot * 0.6f /* 0x931da8 */ : dot * -0.6f /* 0x9774dc */;
        break;
    default:
        return U32(d, 0x2f8) != 0;
    }
    SF32(d, 0x3d4, lean);
    return U32(d, 0x2f8) != 0;
}
RX_FUNCTION(0x0050ae20, rx_race_lean_recover)

/* fld / fstp of a float: the bits, a signalling NaN quieted */
static uint32_t rm_fcopy(uint32_t bits) {
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x007fffffu) != 0) bits |= 0x00400000u;
    return bits;
}
/* 00407430 (ECX = out, ESI = point P, EAX = segment start A, cdecl segment end B; the caller pops) -> xmm0 = distance: the point of
 * the segment A -> B nearest to P (an end point when P projects outside it) goes to out; the distance is SqrtF (sqrtss) of the squared
 * distance, computed the way each case of the original does (|PA|^2, |PB|^2 = |PA|^2 - 2 dot + |BA|^2, or with the projection t:
 * |PA|^2 - (t dot) 2 + (t t) |BA|^2). The y / z of an end point are copied through the x87 (a signalling NaN comes out quiet). Frame:
 * push ebp; and esp, -16; sub esp, 0x20; push ecx. */
float rx_closest_point_on_segment(rx_addr out, rx_addr p, rx_addr a, rx_addr b) {
    const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8);
    const float py = rx_subs(F32(p, 4), ay), bz = rx_subs(F32(b, 8), az), by = rx_subs(F32(b, 4), ay);
    const float pz = rx_subs(F32(p, 8), az), px = rx_subs(F32(p, 0), ax), bx = rx_subs(F32(b, 0), ax);
    const float dot = rx_adds(rx_adds(rx_muls(px, bx), rx_muls(py, by)), rx_muls(pz, bz));
    const float pa2 = rx_adds(rx_adds(rx_muls(py, py), rx_muls(pz, pz)), rx_muls(px, px));
    if (0.0f >= dot) {                                        /* before A (a NaN dot never gets here: comiss + jb) */
        SU32(out, 0, U32(a, 0)); SU32(out, 4, rm_fcopy(U32(a, 4))); SU32(out, 8, rm_fcopy(U32(a, 8)));
        return rx_sqrts(pa2);
    }
    const float ba2 = rx_adds(rx_adds(rx_muls(bx, bx), rx_muls(by, by)), rx_muls(bz, bz));
    if (dot >= ba2) {                                         /* beyond B */
        SU32(out, 0, U32(b, 0)); SU32(out, 4, rm_fcopy(U32(b, 4))); SU32(out, 8, rm_fcopy(U32(b, 8)));
        return rx_sqrts(rx_adds(rx_subs(pa2, rx_muls(dot, 2.0f /* 0x92e330 */)), ba2));
    }
    const float t = rx_divs(dot, ba2);                        /* on the segment */
    const float x = rx_adds(ax, rx_muls(bx, t)), y = rx_adds(ay, rx_muls(by, t)), z = rx_adds(az, rx_muls(bz, t));
    SU32(out, 4, rx_fb(y)); SU32(out, 0, rx_fb(x)); SU32(out, 8, rx_fb(z));
    return rx_sqrts(rx_adds(rx_subs(pa2, rx_muls(rx_muls(t, dot), 2.0f)), rx_muls(rx_muls(t, t), ba2)));
}
RX_FUNCTION(0x00407430, rx_closest_point_on_segment)
