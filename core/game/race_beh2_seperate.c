/* race_beh2_seperate.c -- BehaviorSeperate (vtable 0x0093f9a0): how an AI rider deals with the other riders around it.
 *
 * Every frame (vf4) it casts three probe rays along its flat velocity (centre / left / right, 00509fc0), then
 *  - pushes its steering (driver +0x60) away from, and its slow-down (driver +0x64) up for, every rider its rays would hit soon
 *    (00532910: time to contact on the side rays, else the time to a crossing of the paths);
 *  - picks the rider ahead it would reach first as its target (00531a70, +0x78) and runs a small state machine on it (+0x7c):
 *    1 = stuck behind it: choose a side to pass on (00531c20), 2 / 3 = passing on the -1 / +1 side: keep passing while there is
 *    room (005322c0), else follow (00532dd0: steer onto its line, brake by +0x64 through the follow time).
 * Layout: +0x04 enabled, +0x08 driver, +0x10 / +0x30 / +0x50 the centre / left / right probe rays (start, end at +0x10), +0x70 /
 * +0x74 the side check's distance / width (0050a8b0), +0x78 target driver, +0x7c state, +0x80 no room to pass, +0x84 contact steering
 * applied this frame, +0x88 held by +0xa0 (its state +0xc == 2), +0x8c hold until (driver clock), +0x90 last decision code (debug:
 * 0 idle, 1 node forbids, 2..6 side choice, 8..0x10 passing, 0x13 / 0x14 follow, 0x15 / 0x16 hold, 0x19 target gone), +0x98 follow
 * until, +0x9c the lap-pacing behaviour (its +0x1c = until when), +0xa0 the behaviour that can hold this one.
 * Tuning (per driver index, race_beh2_extern.h beh2_tune): 0x009e4598 follow / pick-up time, 0x009e4658 hold time, 0x00a04bf8 /
 * 0x00a04b98 contact / crossing look-ahead times, 0x00a04cb8 pacing hold. (notes_beh2.md) */
#include "race_beh2_extern.h"

/* 00533510 (EDI = vehicle, ESI = out centre, arg = out radius; RET 4): the centre of the vehicle's collision bounds in the world
 * (the bounds' centre through the vehicle matrix) and their radius. */
void rx_vehicle_bound_centre(rx_addr vehicle, rx_addr centre, rx_addr radius) {
    RX_FRAME_ENTER(F, 0x30);   /* +0x00 centre (vehicle frame), +0x10 extents, +0x20 radius */
    beh2_shape_bounds(vehicle + 0x94, F, F + 0x10, F + 0x20);
    const rx_addr M = beh2_vehicle_matrix(vehicle);
    const float x = F32(F, 0), y = F32(F, 4), z = F32(F, 8);
    SF32(centre, 0, ((F32(M, 0x20) * z + F32(M, 0x10) * y) + x * F32(M, 0)) + F32(M, 0x30));
    SF32(centre, 4, ((F32(M, 4) * x + F32(M, 0x24) * z) + F32(M, 0x14) * y) + F32(M, 0x34));
    SF32(centre, 8, ((F32(M, 8) * x + F32(M, 0x28) * z) + F32(M, 0x18) * y) + F32(M, 0x38));
    SU32(radius, 0, U32(F, 0x20));
    RX_FRAME_LEAVE(F, 0x30);
}
RX_FUNCTION(0x00533510, rx_vehicle_bound_centre)

/* 00533070 (args: behaviour, other driver, max time, max move; RET 0x10) -> xmm0: the time until this rider reaches the other one,
 * or 0. The other's bounds centre (height 0) must lie within 0.8 class widths of the left or the right probe ray; the time is the
 * distance to the closest point of that ray over this rider's speed (+0x150) and counts only if it is below max_time and the other,
 * moving on at its speed (+0x150) along its flat velocity for that time, moves less than max_move. */
float rx_seperate_contact_time(rx_addr beh, rx_addr other, float max_time, float max_move) {
    rx_addr v = beh2_vehicle(U32(beh, 8));
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    rx_addr ov = beh2_vehicle(other);
    float dx = F32(ov, 0x160), dy = 0.0f, dz = F32(ov, 0x168);
    const float len2 = (dx * dx + 0.0f * 0.0f) + dz * dz;
    if (len2 > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        dx = inv * dx;
        dy = inv * 0.0f;
        dz = dz * inv;
    }
    RX_FRAME_ENTER(F, 0x30);   /* +0x00 the other's centre, +0x10 the closest point of the ray, +0x20 radius (unused) */
    rx_vehicle_bound_centre(beh2_vehicle(other), F, F + 0x20);
    ov = beh2_vehicle(other);
    const float width = beh2_class_width(ov) * F32(0, 0x009cc8a8) /* 0.8 */;
    SF32(F, 4, 0.0f);
    int hit = rx_race_point_near_segment(F, beh + 0x30, beh + 0x40, width, F + 0x10);
    if (!hit) hit = rx_race_point_near_segment(F, beh + 0x50, beh + 0x60, width, F + 0x10);
    float result = 0.0f;
    if (hit) {
        const float px = F32(F, 0x10), py = F32(F, 0x14), pz = F32(F, 0x18);
        const float ey = 0.0f - py, ex = mx - px, ez = mz - pz;
        const float t = sqrtf((ey * ey + ex * ex) + ez * ez) / F32(beh2_vehicle(U32(beh, 8)), 0x150);
        if (max_time > t) {
            const float k = F32(ov, 0x150) * t;
            const float ox = (dx * k + px) - px, oy = (dy * k + py) - py, oz = (dz * k + pz) - pz;
            if (max_move > sqrtf((oy * oy + ox * ox) + oz * oz)) result = t;
        }
    }
    RX_FRAME_LEAVE(F, 0x30);
    return result;
}
RX_FUNCTION(0x00533070, rx_seperate_contact_time)

/* 005332f0 (ECX = other driver, args: behaviour, max time; RET 8) -> xmm0: where the other rider's path (from one class length
 * behind it, 1000 units along its velocity) crosses this rider's centre probe ray, the time this rider needs to get there, if the
 * other gets there first and the time is below max_time; else 0. */
float rx_seperate_crossing_time(rx_addr other, rx_addr beh, float max_time) {
    rx_addr v = beh2_vehicle(U32(beh, 8));
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    rx_addr ov = beh2_vehicle(other);
    const float ox = F32(ov, 0xc0), oz = F32(ov, 0xc8), vx = F32(ov, 0x160), vz = F32(ov, 0x168);
    ov = beh2_vehicle(other);
    const float len = beh2_class_length(ov);
    RX_FRAME_ENTER(F, 0x30);   /* +0x00 / +0x10 the other's path, +0x20 the crossing */
    const float ax = ox - vx * len, ay = 0.0f - len * 0.0f, az = oz - vz * len;
    const float far = F32(0, 0x009cf5b0) /* 1000 */;
    SF32(F, 0, ax); SF32(F, 4, ay); SF32(F, 8, az);
    SF32(F, 0x10, vx * far + ax); SF32(F, 0x14, 0.0f * far + ay); SF32(F, 0x18, vz * far + az);
    float result = 0.0f;
    if (rx_race_segments_cross_xz(F, F + 0x10, beh + 0x10, beh + 0x20, F + 0x20)) {
        const float hx = F32(F, 0x20), hz = F32(F, 0x28);
        const float dy = F32(F, 0x24) - 0.0f;
        const float dxm = hx - mx, dzm = hz - mz, dxo = hx - ox, dzo = hz - oz;
        const float t_me = sqrtf((dy * dy + dxm * dxm) + dzm * dzm) / F32(beh2_vehicle(U32(beh, 8)), 0x150);
        const float t_other = sqrtf((dy * dy + dxo * dxo) + dzo * dzo) / F32(ov, 0x150);
        if (t_me > t_other && max_time > t_me) result = t_me;
    }
    RX_FRAME_LEAVE(F, 0x30);
    return result;
}
RX_FUNCTION(0x005332f0, rx_seperate_crossing_time)

/* 00532910 (EDI = behaviour): contact avoidance against every other rider of the activity that has a vehicle, is not out
 * (player +0x11a0) and is ahead (z >= 0 in this rider's frame). The urgency f = 1 - time / look-ahead (look-ahead times x2 on
 * tracks flagged +0x118 bit 2 for row 2) of the contact time (side rays, max move = the other's class length) or, without one,
 * the crossing time; a contact with a human only counts off those tracks. While the vehicle's flag 43 is set and the player's
 * +0x10bc is above 0.24 the urgency steers (driver +0x60) away from the side the other is on (its point, slot 17, in this
 * vehicle's matrix frame) and is used up (+0x84 = 1); what is left, scaled down by the lateral offset to the target (x 0.2), goes
 * x 2.5 into the slow-down (driver +0x64). */
void rx_seperate_avoid_riders(rx_addr beh) {
    rx_addr driver = U32(beh, 8);
    SU32(beh, 0x84, 0);
    float mult = 1.0f /* 0x931f24 */;
    if ((U8(U32(driver, 0x14), 0x118) & 4) != 0 && U32(driver, 0x32c) == 2) mult = 2.0f /* 0x92e330 */;
    const int32_t n = I32(U32(U32(driver, 0x74), 0x94), 0x26c);
    RX_FRAME_ENTER(F, 0x30);   /* +0x00 relative position, +0x10 the other's point, +0x20 flat right vector */
    for (int32_t i = 0; i < n; i++) {
        driver = U32(beh, 8);
        const rx_addr other = U32(U32(U32(U32(driver, 0x74), 0x94), 0x268), 4u * (uint32_t)i);
        if (driver == other) continue;
        const rx_addr op = U32(other, 0x74), ov = U32(op, 0x1ac);
        if (ov == 0 || U32(op, 0x11a0) != 0) continue;
        const rx_addr mv = beh2_vehicle(driver);
        SF32(F, 0, F32(ov, 0xc0) - F32(mv, 0xc0));
        SF32(F, 4, F32(ov, 0xc4) - F32(mv, 0xc4));
        SF32(F, 8, F32(ov, 0xc8) - F32(mv, 0xc8));
        rx_race_to_vehicle_frame(mv, F);
        if (0.0f > F32(F, 8)) continue;
        const float move = beh2_class_length(beh2_vehicle(other));
        float range = beh2_tune_f(0x00a04bf8, U32(beh, 8)) * mult;
        float t = rx_seperate_contact_time(beh, other, range, move);
        float f = 0.0f;
        int apply;
        if (t == 0.0f) {
            range = beh2_tune_f(0x00a04b98, U32(beh, 8)) * mult;
            t = rx_seperate_crossing_time(other, beh, range);
            apply = !(t == 0.0f);
        } else {
            apply = U32(U32(other, 0x74), 0x10) != 0 || (U8(U32(U32(beh, 8), 0x14), 0x118) & 4) == 0;
        }
        if (apply) {
            f = 1.0f /* 0x931f24 */ - t / range;
            if (!(f == 0.0f) && beh2_vehicle_flag43(beh2_vehicle(U32(beh, 8))) != 0) {
                const rx_addr pl = U32(U32(beh, 8), 0x74);
                if (F32(pl, 0x10bc) > F32(0, 0x009cd9ec) /* 0.24 */) {
                    const rx_addr M = beh2_vehicle_matrix(U32(pl, 0x1ac));
                    beh2_vehicle_point(beh2_vehicle(other), F + 0x10);
                    const float px = F32(F, 0x10), py = F32(F, 0x14), pz = F32(F, 0x18);
                    const float lx = ((pz * F32(M, 8) + py * F32(M, 4)) + F32(M, 0) * px) + F32(M, 0xc);
                    const rx_addr d = U32(beh, 8);
                    SU32(beh, 0x84, 1);
                    if (0.0f > lx) SF32(d, 0x60, F32(d, 0x60) + f);
                    else SF32(d, 0x60, F32(d, 0x60) - f);
                    f = 0.0f;
                }
            }
        }
        const rx_addr target = U32(beh, 0x78);
        if (target == other && (U8(U32(U32(beh, 8), 0x14), 0x118) & 4) == 0) {
            const rx_addr v = beh2_vehicle(U32(beh, 8)), tv = beh2_vehicle(target);
            const float mx = F32(v, 0xc0), mz = F32(v, 0xc8), tx = F32(tv, 0xc0), tz = F32(tv, 0xc8);
            beh2_flat_dir(beh2_vehicle_matrix(v), F + 0x20);
            const float lat = F32(F, 0x24) * (tz - mz) + (tx - mx) * F32(F, 0x20);
            float c = 1.0f /* 0x931f24 */ - fabsf(lat) * 0.2f /* 0x92e388 */;
            if (0.0f > c) c = 0.0f;
            f = c * f;
        }
        const rx_addr d = U32(beh, 8);
        SF32(d, 0x64, f * F32(0, 0x009d789c) /* 2.5 */ + F32(d, 0x64));
    }
    RX_FRAME_LEAVE(F, 0x30);
}
RX_FUNCTION(0x00532910, rx_seperate_avoid_riders)

/* 00531a70 (EDI = behaviour): the target (+0x78) = the rider ahead (z >= 0 in this rider's frame, height difference below 6) this
 * rider would reach first (smallest contact time with the follow time 0x009e4598 and 2.5 class lengths of move); a new target
 * starts state 1 (+0x7c). The old target stays when no rider qualifies. */
void rx_seperate_pick_target(rx_addr beh) {
    rx_addr driver = U32(beh, 8);
    const rx_addr old = U32(beh, 0x78);
    float best = rx_bf(0x7f7fffff) /* 0x977494 */;
    const int32_t n = I32(U32(U32(driver, 0x74), 0x94), 0x26c);
    RX_FRAME_ENTER(F, 0x10);
    for (int32_t i = 0; i < n; i++) {
        driver = U32(beh, 8);
        const rx_addr pl = U32(driver, 0x74);
        const rx_addr other = U32(U32(U32(pl, 0x94), 0x268), 4u * (uint32_t)i);
        if (driver == other) continue;
        const rx_addr op = U32(other, 0x74), ov = U32(op, 0x1ac);
        if (ov == 0 || U32(op, 0x11a0) != 0) continue;
        const rx_addr mv = U32(pl, 0x1ac);
        if (fabsf(F32(mv, 0xc4) - F32(ov, 0xc4)) >= 6.0f /* 0x92fe4c */) continue;
        SF32(F, 0, F32(ov, 0xc0) - F32(mv, 0xc0));
        SF32(F, 4, F32(ov, 0xc4) - F32(mv, 0xc4));
        SF32(F, 8, F32(ov, 0xc8) - F32(mv, 0xc8));
        rx_race_to_vehicle_frame(mv, F);
        if (0.0f > F32(F, 8)) continue;
        const float range = beh2_tune_f(0x009e4598, U32(beh, 8));
        const float move = beh2_class_length(beh2_vehicle(other)) * F32(0, 0x009d789c) /* 2.5 */;
        const float t = rx_seperate_contact_time(beh, other, range, move);
        if (!(t == 0.0f) && best > t) {
            best = t;
            SU32(beh, 0x78, other);
        }
    }
    RX_FRAME_LEAVE(F, 0x10);
    if (U32(beh, 0x78) != old) SU32(beh, 0x7c, 1);
}
RX_FUNCTION(0x00531a70, rx_seperate_pick_target)

/* 00532fd0 (ECX = behaviour): the hold by the behaviour at +0xa0: its state 2 holds now (+0x88 = 1, the hold time cleared), its
 * state 0 starts a hold of 0x009e4658 seconds unless one is set. */
void rx_seperate_check_hold(rx_addr beh) {
    const rx_addr h = U32(beh, 0xa0);
    SU32(beh, 0x88, 0);
    if (h == 0) return;
    const uint32_t st = U32(h, 0xc);
    if (st == 2) {
        SU32(beh, 0x88, 1);
        SU32(beh, 0x90, 0x15);
        SF32(beh, 0x8c, 0.0f);
    } else if (st == 0 && F32(beh, 0x8c) == 0.0f) {
        const rx_addr d = U32(beh, 8);
        SF32(beh, 0x8c, beh2_tune_f(0x009e4658, d) + F32(d, 0x130));
        SU32(beh, 0x90, 0x16);
    }
}
RX_FUNCTION(0x00532fd0, rx_seperate_check_hold)

/* 005321f0 (ESI = behaviour) -> EAX: is the target out of the way (no contact within the follow time + 0.05)? Then the pacing
 * behaviour (+0x9c) holds for 0x00a04cb8 seconds and this behaviour resets (vf6): 1. */
int rx_seperate_target_clear(rx_addr beh) {
    const rx_addr target = U32(beh, 0x78);
    const float range = beh2_tune_f(0x009e4598, U32(beh, 8)) + F32(0, 0x009d3bfc) /* 0.05 */;
    const float move = beh2_class_length(beh2_vehicle(target)) * F32(0, 0x009d789c) /* 2.5 */;
    const float t = rx_seperate_contact_time(beh, target, range, move);
    if (!(t == 0.0f)) return 0;
    const rx_addr pacing = U32(beh, 0x9c);
    if (pacing != 0) SF32(pacing, 0x1c, beh2_tune_f(0x00a04cb8, U32(beh, 8)) + F32(U32(pacing, 8), 0x130));
    SU32(beh, 0x90, 0x19);
    beh2_behavior_reset(beh);
    return 1;
}
RX_FUNCTION(0x005321f0, rx_seperate_target_clear)

/* The lane geometry of 00531c20 / 005322c0: the closest points of the left edge, the right edge and the centre line of the track
 * segment (driver +0x18 -> +0x1c) to this rider, and the room on the -1 side (room_a) and on the +1 side (room_b) for the lane the
 * driver is in (+0x310: -1 left of centre, 0 middle, 1 right). The original leaves both uninitialised for other lanes. */
static void clamp_project(float ax, float az, float bx, float bz, float px, float pz, float* ox, float* oz) {
    const float dx = bx - ax, dz = bz - az;
    float u = ((px - ax) * dx + (pz - az) * dz) / (dx * dx + dz * dz);
    if (1.0f /* 0x931f24 */ > u) { if (0.0f > u) u = 0.0f; }
    else u = 1.0f;
    *ox = dx * u + ax;
    *oz = dz * u + az;
}
static void lane_room(rx_addr driver, float mx, float mz, float* room_a, float* room_b) {
    const rx_addr a = U32(driver, 0x18), b = U32(driver, 0x1c);
    float lx, lz, rx, rz, cx, cz;
    clamp_project(F32(a, 0x10), F32(a, 0x18), F32(b, 0x10), F32(b, 0x18), mx, mz, &lx, &lz);
    clamp_project(F32(a, 0x20), F32(a, 0x28), F32(b, 0x20), F32(b, 0x28), mx, mz, &rx, &rz);
    clamp_project(F32(a, 0), F32(a, 8), F32(b, 0), F32(b, 8), mx, mz, &cx, &cz);
    const int32_t lane = I32(driver, 0x310);
    *room_a = 0.0f; *room_b = 0.0f;
    if (lane == -1) {
        *room_a = sqrtf((lz - mz) * (lz - mz) + (lx - mx) * (lx - mx));
        *room_b = sqrtf((cz - mz) * (cz - mz) + (cx - mx) * (cx - mx));
    } else if (lane == 0) {
        *room_a = sqrtf((lz - mz) * (lz - mz) + (lx - mx) * (lx - mx));
        *room_b = sqrtf((rz - mz) * (rz - mz) + (rx - mx) * (rx - mx));
    } else if (lane == 1) {
        const float rc = sqrtf((rz - cz) * (rz - cz) + (rx - cx) * (rx - cx));
        const float cm = sqrtf((cz - mz) * (cz - mz) + (cx - mx) * (cx - mx));
        *room_a = rc - cm;
        *room_b = sqrtf((rz - mz) * (rz - mz) + (rx - mx) * (rx - mx));
    }
}
/* The target's lateral offset from this rider along the target's flat right vector (matrix row 0, normalized) and the room
 * needed to pass it: half both class widths + 6 - |offset|. */
static void target_offset(rx_addr driver, rx_addr target, float mx, float mz, float* lat, float* need) {
    const float tx = F32(beh2_vehicle(target), 0xc0), tz = F32(beh2_vehicle(target), 0xc8);
    const float hw_me = beh2_class_width(beh2_vehicle(driver)) * 0.5f /* 0x9322cc */;
    const rx_addr tv = beh2_vehicle(target);
    const float hw_t = beh2_class_width(tv) * 0.5f;
    const rx_addr M = beh2_vehicle_matrix(tv);
    float rx = F32(M, 0), rz = F32(M, 8);
    const float len2 = rz * rz + rx * rx;
    if (len2 != 0.0f) {
        const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
        rx = inv * rx;
        rz = rz * inv;
    }
    *lat = (mz - tz) * rz + (mx - tx) * rx;
    *need = ((hw_t + hw_me) + 6.0f /* 0x92fe4c */) - fabsf(*lat);
}

/* 00531c20 (EDI = behaviour): stuck behind the target (state 1): pass on the side away from it (+1 when it is to the right) if
 * that side has the room and no rider (0050a8b0 with +0x70 / +0x74), else on the other side if that has room for the offset too;
 * state 3 / 2 for the +1 / -1 side. */
void rx_seperate_choose_side(rx_addr beh) {
    const rx_addr driver = U32(beh, 8), target = U32(beh, 0x78);
    const rx_addr v = beh2_vehicle(driver);
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    float room_a, room_b, lat, need;
    lane_room(driver, mx, mz, &room_a, &room_b);
    target_offset(driver, target, mx, mz, &lat, &need);
    const float alat = fabsf(lat);
    if (lat > 0.0f) {
        if (room_b > need && !rx_race_rider_near(U32(beh, 8), 1, F32(beh, 0x70), F32(beh, 0x74))) {
            SU32(beh, 0x7c, 3); SU32(beh, 0x90, 3); return;
        }
        if (room_a > alat + need && !rx_race_rider_near(U32(beh, 8), -1, F32(beh, 0x70), F32(beh, 0x74))) {
            SU32(beh, 0x7c, 2); SU32(beh, 0x90, 5); return;
        }
        SU32(beh, 0x90, 6);
        return;
    }
    if (lat != lat) return;
    if (room_a > need && !rx_race_rider_near(U32(beh, 8), -1, F32(beh, 0x70), F32(beh, 0x74))) {
        SU32(beh, 0x7c, 2); SU32(beh, 0x90, 2); return;
    }
    if (room_b > alat + need && !rx_race_rider_near(U32(beh, 8), 1, F32(beh, 0x70), F32(beh, 0x74))) {
        SU32(beh, 0x7c, 3); SU32(beh, 0x90, 4); return;
    }
    SU32(beh, 0x90, 6);
}
RX_FUNCTION(0x00531c20, rx_seperate_choose_side)

/* 005322c0 (EDI = behaviour) -> EAX: passing (state 2 / 3): 1 = go on passing on this side (room and no rider there), or the side
 * was switched (state 3 <-> 2, 1); 0 = no room on either side (+0x80 = 1): follow the target instead. */
int rx_seperate_keep_passing(rx_addr beh) {
    const rx_addr driver = U32(beh, 8), target = U32(beh, 0x78);
    SU32(beh, 0x80, 0);
    const rx_addr v = beh2_vehicle(driver);
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    float room_a, room_b, lat, need;
    lane_room(driver, mx, mz, &room_a, &room_b);
    target_offset(driver, target, mx, mz, &lat, &need);
    const float alat = fabsf(lat);
    const uint32_t st = U32(beh, 0x7c);
    if (st == 3) {
        int ok = 1;
        if (rx_race_rider_near(U32(beh, 8), 1, F32(beh, 0x70), F32(beh, 0x74))) { ok = 0; SU32(beh, 0x90, 8); }
        if (need > room_b) { ok = 0; SU32(beh, 0x90, 0xb); }
        if (1.0f /* 0x931f24 */ > need && 0.0f > lat) SU32(beh, 0x90, 0xd);
        else if (ok) return 1;
        if (room_a > alat + need && !rx_race_rider_near(U32(beh, 8), -1, F32(beh, 0x70), F32(beh, 0x74))) {
            SU32(beh, 0x90, 0xf); SU32(beh, 0x7c, 2);
            return 1;
        }
        SU32(beh, 0x80, 1);
        return 0;
    }
    if (st == 2) {
        int ok = 1;
        if (rx_race_rider_near(U32(beh, 8), -1, F32(beh, 0x70), F32(beh, 0x74))) { ok = 0; SU32(beh, 0x90, 0xa); }
        if (need > room_a) { ok = 0; SU32(beh, 0x90, 0xc); }
        if (1.0f > need && lat > 0.0f) SU32(beh, 0x90, 0xe);
        else if (ok) return 1;
        if (room_b > alat + need && !rx_race_rider_near(U32(beh, 8), 1, F32(beh, 0x70), F32(beh, 0x74))) {
            SU32(beh, 0x90, 0x10); SU32(beh, 0x7c, 3);
            return 1;
        }
        SU32(beh, 0x80, 1);
        return 0;
    }
    return 1;
}
RX_FUNCTION(0x005322c0, rx_seperate_keep_passing)

/* 00532dd0 (EAX = behaviour): follow the target: above 14.67 (vehicle +0x164) or within the follow hold (+0x98, 0.75 s, renewed
 * while fast) nothing; with the vehicle's flag 43 nothing; else steer (driver +0x60) toward the target's line, by the contact time
 * over the follow time, scaled down by the lateral offset (x 1/7); the sign by the passing side (state 2: -). */
void rx_seperate_follow(rx_addr beh) {
    const rx_addr driver = U32(beh, 8), target = U32(beh, 0x78);
    rx_addr v = beh2_vehicle(driver);
    const float mx = F32(v, 0xc0), mz = F32(v, 0xc8);
    const rx_addr tv = beh2_vehicle(target);
    const float tx = F32(tv, 0xc0), tz = F32(tv, 0xc8);
    v = beh2_vehicle(driver);
    if (F32(v, 0x164) > F32(0, 0x009cd9f0) /* 14.6666 */) {
        SF32(beh, 0x98, F32(driver, 0x130) + 0.75f /* 0x935aa8 */);
        SU32(beh, 0x90, 0x13);
        return;
    }
    if (F32(beh, 0x98) > F32(driver, 0x130)) { SU32(beh, 0x90, 0x13); return; }
    if (beh2_vehicle_flag43(v) != 0) { SU32(beh, 0x90, 0x14); return; }
    const float range = beh2_tune_f(0x009e4598, U32(beh, 8));
    float scale = 0.0f;
    const float move = beh2_class_length(beh2_vehicle(U32(beh, 0x78))) * F32(0, 0x009d789c) /* 2.5 */;
    const float t = rx_seperate_contact_time(beh, U32(beh, 0x78), range, move);
    if (!(t == 0.0f)) scale = t / range;
    RX_FRAME_ENTER(F, 0x10);
    beh2_flat_dir(beh2_vehicle_matrix(beh2_vehicle(U32(beh, 8))), F);
    float c = 1.0f /* 0x931f24 */ - fabsf(F32(F, 4) * (tz - mz) + (tx - mx) * F32(F, 0)) * 0.14285715f /* 0x977af0 */;
    RX_FRAME_LEAVE(F, 0x10);
    if (0.0f > c) c = 0.0f;
    const rx_addr d = U32(beh, 8);
    if (U32(beh, 0x7c) == 2) SF32(d, 0x60, F32(d, 0x60) - c * scale);
    else SF32(d, 0x60, F32(d, 0x60) + c * scale);
}
RX_FUNCTION(0x00532dd0, rx_seperate_follow)

/* 005318f0 BehaviorSeperate::vf6 (ECX = behaviour) -> EAX 0: reset (no target, idle, no follow hold, no contact steering). */
uint32_t rx_behavior_seperate_reset(rx_addr beh) {
    SU32(beh, 0x78, 0);
    SU32(beh, 0x7c, 0);
    SF32(beh, 0x98, 0.0f);
    SU32(beh, 0x84, 0);
    return 0;
}
RX_FUNCTION(0x005318f0, rx_behavior_seperate_reset)

/* 00531940 BehaviorSeperate::vf4 (ECX = behaviour; RET 4, the argument is unused) -> EAX 0: the frame (see the file comment).
 * A track node flagged 0x2000 for this vehicle class (prev node +0x68 + 4 * (4 * class + kind)) cancels everything (reset, the
 * pacing hold cleared). */
uint32_t rx_behavior_seperate_update(rx_addr beh, float dt) {
    (void)dt;   /* the vf4 argument (unused here) */
    if (U32(beh, 4) == 0) return 0;
    rx_race_probe_rays(U32(beh, 8), beh + 0x10, beh + 0x30, beh + 0x50);
    rx_seperate_avoid_riders(beh);
    rx_addr driver = U32(beh, 8);
    const rx_addr v = beh2_vehicle(driver);
    if ((U32(U32(driver, 0x18), 0x68 + 4u * (U32(v, 0x198) + 4u * U32(v, 0x194))) & 0x2000) != 0) {
        const rx_addr pacing = U32(beh, 0x9c);
        if (pacing != 0) SF32(pacing, 0x1c, 0.0f);
        beh2_behavior_reset(beh);
        SU32(beh, 0x90, 1);
        return 0;
    }
    rx_seperate_check_hold(beh);
    driver = U32(beh, 8);
    if (F32(beh, 0x8c) > F32(driver, 0x130) || U32(beh, 0x88) != 0) return 0;
    rx_seperate_pick_target(beh);
    switch (U32(beh, 0x7c)) {
    case 0:
        SU32(beh, 0x90, 0);
        break;
    case 1:
        if (!rx_seperate_target_clear(beh)) rx_seperate_choose_side(beh);
        break;
    case 2:
    case 3:
        if (!rx_seperate_target_clear(beh) && rx_seperate_keep_passing(beh)) rx_seperate_follow(beh);
        break;
    }
    return 0;
}
RX_FUNCTION(0x00531940, rx_behavior_seperate_update)
