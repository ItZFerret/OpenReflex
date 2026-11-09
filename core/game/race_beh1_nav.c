/* race_beh1_nav.c -- BehaviorNavigateTrack (vtable 0x93f8b0): the AI's racing line, and BehaviorOverrideSpline (0x93f928): the
 * authored lines (splines) that replace it in places.
 * NavigateTrack each frame looks ahead along the track by the tuning's look-ahead distance (0x9e4298), shifts that point sideways
 * toward the racing-line side it picked (+0x0c: -1 / 0 / 1, the offset growing from the centre line to the tuning's fraction of the
 * edge distance 0xa048f8) and steers the driver at it (target +0x20 / +0x24); far off the line (heading . track direction below
 * -0.75) it turns hard (+-1 into the driver's steering +0x60) toward the side the track position (00509e30) says, and otherwise adds
 * a steering push toward the target (pow-difference gain of the tuning 0xa049b8, scaled by the heading factor 0050b150). Three random
 * numbers per frame (+0x40..+0x48) feed the line variations. Behaviour: +0x0c line side, +0x10, +0x14 / +0x18 the side's offset
 * state / its node, +0x20 target, +0x28 hard-turn state (-1 / 0 / 1), +0x2c / +0x30 / +0x38 linked behaviours, +0x3c, +0x4c "on the
 * line" (0 when the heading agrees with the track better than the tuning's 0x9e4418 / 0x9e3908 limit).
 * OverrideSpline: +0x38 the spline (+0x00 first node, +0x08 next splines, +0x10 their count, +0x18 + 4 k per-class flags: 1 usable,
 * 0x40 steer along it, 0x80 clear the driver's line flags), +0x3c / +0x40 the current node / next node, +0x44 the line side it wants,
 * +0x48 / +0x4c the look-ahead point, +0x50 / +0x54 the flags 0x40 / 0x80, +0x58 state (0 waiting for a spline ahead, 1 entering,
 * 2 following), +0x5c the NavigateTrack behaviour, +0x60 / +0x64 other behaviours (paused through vf7 while following).
 * (core/notes/race.md "AI behaviours") */
#include "race_beh1_extern.h"
#include "race_beh2.h"

/* ---- the other behaviour cluster (race_beh2_*.c) ---- */
static uint32_t b1_beh2_534470(rx_addr obj) { return rx_behavior_take_line_ready(obj); }
static rx_addr b1_spline_ahead(rx_addr b, float dist) { return rx_override_spline_scan(b, dist); }
static rx_addr b1_spline_reselect(rx_addr b) { return rx_override_spline_scan_start(b); }
/* behaviour vf7 (0x1c, thiscall, one dword argument): pause (1) / resume (0) */
typedef void (*b1_void_u32_fn)(rx_addr self, uint32_t a);
static void b1_behavior_vf7(rx_addr obj, uint32_t a) {
    rx_addr fn = race_vfn(obj, 7);
    b1_void_u32_fn f = RX_FN(fn, b1_void_u32_fn);
    if (f) f(obj, a); else B1_ORIG(fn, RX_THISCALL, obj, a);
}

/* ------------------------------------------------------------------------------------------------- BehaviorNavigateTrack */
/* 00528d50 (EDI = behaviour, args: when_airborne, force; RET 8): pick the racing-line side (+0x0c). Off (no change) when disabled
 * unless forced. A side forced by the driver (+0x310: -1 / 1) wins unless a linked line (+0x30, +0x7c 2 / 3) is active; else a
 * positive 00534470 of +0x2c takes the side of the driver's +0x314 (-1 -> -1, else 1); else (airborne only with +0x38's +0x4c or
 * when_airborne) the side the rider is on: across fraction < 0.5 - tuning (0x9e4238) -> -1, > 0.5 + tuning -> 1. */
void rx_behavior_navigate_pick_side(rx_addr b, uint32_t when_airborne, uint32_t force) {
    if (U32(b, 4) == 0 && force == 0) return;
    uint32_t linked = 0, r = 0;
    rx_addr c = U32(b, 0x30);
    if (c != 0 && (U32(c, 0x7c) == 2 || U32(c, 0x7c) == 3)) linked = 1;
    if (U32(b, 0x2c) != 0) r = b1_beh2_534470(U32(b, 0x2c));
    rx_addr drv = b1_driver(b);
    int32_t forced = I32(drv, 0x310);
    if (forced != 0 && linked == 0) {
        if (forced == -1) SU32(b, 0xc, 0xffffffffu);
        else if (forced == 1) SU32(b, 0xc, 1);
        return;
    }
    if (r != 0 && linked == 0) {
        SU32(b, 0xc, I32(drv, 0x314) != -1 ? 1u : 0xffffffffu);
        return;
    }
    if (b1_vehicle_airborne(b1_vehicle(drv)) != 0) {
        rx_addr o = U32(b, 0x38);
        if (!(o != 0 && U32(o, 0x4c) != 0) && when_airborne == 0) return;
    }
    drv = b1_driver(b);
    float across = rx_race_track_lateral(drv, b1_vehicle(drv) + 0xc0);
    float k = b1_tuning(0x009e4238, drv);
    if (0.5f - k > across) { SU32(b, 0xc, 0xffffffffu); return; }
    if (across > k + 0.5f) SU32(b, 0xc, 1);
}
RX_FUNCTION(0x00528d50, rx_behavior_navigate_pick_side)

/* 00528940 (EDI = behaviour, EDX = node, stack side; RET 4) -> xmm0: the lateral offset of the line on `side` at the node: the node
 * centre's distance to that edge (+0x10 for -1, +0x20 for 1) x the tuning fraction (0xa048f8), signed (+ toward -1's edge... the
 * sign is side + 2 / -1). 0 while the section is flagged 0x10, while switching sides (+0x14 != side: reset, remember the node +0x18)
 * and at the node where the switch happened. */
float rx_behavior_navigate_line_offset(rx_addr b, rx_addr node, int32_t side) {
    rx_addr drv = b1_driver(b);
    if (U8(U32(drv, 0x14), 0x118) & 0x10) return 0.0f;
    int32_t cur = I32(b, 0x14);
    if (side != cur && cur != 0) {
        SU32(b, 0x14, 0);
        SU32(b, 0x18, node);
        return 0.0f;
    }
    if (U32(b, 0x18) == node) { SU32(b, 0x14, (uint32_t)side); return 0.0f; }
    float w = 0.0f;
    rx_addr edge;
    int32_t k;
    if (side == -1) { w = b1_tuning(0x00a048f8, drv); edge = node + 0x10; k = side + 2; }
    else if (side == 1) { w = b1_tuning(0x00a048f8, drv); edge = node + 0x20; k = -1; }
    else { edge = rx_fb(w); k = (int32_t)rx_fb(w); }      /* the original loads both from the zeroed local */
    SU32(b, 0x14, (uint32_t)side);
    float dx = F32(node, 0) - F32(edge, 0);
    float dz = F32(node, 8) - F32(edge, 8);
    float dy = F32(node, 4) - F32(edge, 4);
    float d = sqrtf((dx * dx + dz * dz) + dy * dy);
    return d * (float)k * w;
}
RX_FUNCTION(0x00528940, rx_behavior_navigate_line_offset)

/* 00527360 (EAX = behaviour): follow the racing line (see the file comment). */
void rx_behavior_navigate_follow(rx_addr b) {
    rx_addr drv = b1_driver(b), veh = b1_vehicle(drv);
    const float px = F32(veh, 0xc0), pz = F32(veh, 0xc8);
    float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
    float len2 = hx * hx + hz * hz;
    if (len2 != 0.0f) {
        float inv = 1.0f / sqrtf(len2);
        hx = inv * hx;
        hz = hz * inv;
    }
    SU32(b, 0x10, 0);
    SU32(b, 0x3c, 1);
    for (uint32_t i = 0; i < 3; i++)
        SF32(b, 0x40 + 4 * i, (float)((double)(float)(int32_t)race_rand() * 3.0517578125e-05 /* 0x9776a0 */));
    rx_behavior_navigate_pick_side(b, 1, 0);
    drv = b1_driver(b);
    RX_FRAME_ENTER(F, 0x40);                    /* F + 0x00 look pos (x, z), + 0x08 look dir (x, z), + 0x10 start (section,
                                                 * node), + 0x18 nearest (section, node, d2), + 0x28 query position */
    rx_track_look_ahead_xz(U32(drv, 0x1c), F, b1_tuning(0x009e4298, drv), F + 8);
    drv = b1_driver(b);
    const float lx = F32(F, 0), lz = F32(F, 4), dx = F32(F, 8), dz = F32(F, 0xc);
    SU32(F, 0x14, U32(drv, 0x1c));
    SU32(F, 0x10, U32(drv, 0x14));
    SF32(F, 0x28, lx);
    SF32(F, 0x2c, 0.0f);
    SF32(F, 0x30, lz);
    float budget = b1_data(0x009c4750) /* 200 */;
    rx_track_nearest_node(F + 0x10, F + 0x28, F + 0x18, &budget);
    rx_addr nearest = U32(F, 0x1c);
    RX_FRAME_LEAVE(F, 0x40);
    /* the track's sideways direction (dir x up) */
    float z0 = 0.0f * 0.0f;
    float sx = z0 - dz;
    float sy = dz * 0.0f - dx * 0.0f;
    float sz = dx - z0;
    float s2 = (sx * sx + sy * sy) + sz * sz;
    if (s2 > 0.0f) {
        float inv = 1.0f / sqrtf(s2);
        sx = inv * sx;
        sz = sz * inv;
    }
    float o = rx_behavior_navigate_line_offset(b, nearest, I32(b, 0xc));
    SF32(b, 0x20, o * sx + lx);
    SF32(b, 0x24, o * sz + lz);
    float c = hz * dz + hx * dx;
    SU32(b, 0x4c, 1);
    if (!((U32(b, 0x2c) != 0 && b1_beh2_534470(U32(b, 0x2c)) != 0) || U32(b1_driver(b), 0x310) != 0)) {
        drv = b1_driver(b);
        rx_addr limit = (U8(U32(drv, 0x14), 0x118) & 4) ? 0x009e4418 : 0x009e3908;
        if (c > b1_tuning(limit, drv)) {        /* (then 007e3be0, an empty debug hook) */
            SU32(b, 0x4c, 0);
            return;
        }
    }
    int32_t state = I32(b, 0x28);
    if (state == -1 || state == 1) {
        float s = state == -1 ? b1_data(0x009cd9e8) /* -1 */ : 1.0f;
        SF32(b1_driver(b), 0x60, s);
        if (c > b1_data(0x009cd9b4) /* -0.35 */) SU32(b, 0x28, 0);
        return;
    }
    if (state != 0) return;
    drv = b1_driver(b);
    if (b1_data(0x009cd9b0) /* -0.75 */ > c) {
        if (rx_race_track_lateral(drv, b1_vehicle(drv) + 0xc0) > 0.5f /* 0x9322cc */) {
            SF32(drv, 0x60, 1.0f);
            SU32(b, 0x28, 1);
        } else {
            SF32(drv, 0x60, b1_data(0x009cd9e8));
            SU32(b, 0x28, 0xffffffffu);
        }
        return;
    }
    rx_addr row = 0x00a049b8u + 16u * b1_tuning_index(drv);
    float k = rx_race_pow_difference(F32(row, 0xc), F32(row, 0), F32(row, 4), F32(row, 8), 1.0f);
    float tx = px - F32(b, 0x20), tz = pz - F32(b, 0x24);
    float t2 = tx * tx + tz * tz;
    if (t2 != 0.0f) {
        float inv = 1.0f / sqrtf(t2);
        tx = inv * tx;
        tz = inv * tz;
    }
    RX_FRAME_ENTER(G, 0x30);                    /* G + 0x00 push (vehicle frame), + 0x10 position (x, z), + 0x18 heading */
    SF32(G, 0, tx * k);
    SF32(G, 4, 0.0f);
    SF32(G, 8, tz * k);
    rx_race_to_vehicle_frame(b1_vehicle(drv), G);
    SF32(G, 0x10, px); SF32(G, 0x14, pz);
    SF32(G, 0x18, hx); SF32(G, 0x1c, hz);
    float h = rx_race_heading_factor(b + 0x20, G + 0x10, G + 0x18);
    float push = F32(G, 0);
    RX_FRAME_LEAVE(G, 0x30);
    drv = b1_driver(b);
    SF32(drv, 0x60, F32(drv, 0x60) + h * push);
    SU32(b, 0x28, 0);
}
RX_FUNCTION(0x00527360, rx_behavior_navigate_follow)

/* ---------------------------------------------------------------------------------- NavigateTrack modes 1 / 2 (the monitor corner)
 * BehaviorNavigateTrack's +0x34 object is the driver's BehaviorMonitorCorner (0053b960 -> the behaviour with vf5 id 0x12): mode 0
 * is the plain racing line (00527360), mode 1 (+0xc = 1: the monitor found a corner, 00526a30) is 005278e0, mode 2 (+0xc = 2: the
 * monitor watches the corner, 00526b00) is 00527fc0. Both steer at a target built from the monitor's corner nodes: the first node
 * (+0x14), its direction (+0x1c, +-1) and its length (+0x20). (core/notes/race.md "AI behaviours") */

/* 00528bd0 (EAX = the mode object's +0x1c (the corner direction), EDX = the nearest node, ESI = behaviour, stack: side, random;
 * RET 8) -> xmm0: the lateral offset of the corner line at `node`, in mode 1. The edge of the node the corner turns away from
 * (+0x10 for the -1 side, +0x20 for the +1 side; the side the behaviour already has takes it) is at the tuning distance
 * (0x9e3f98 = same side, 0x9e42f8 = other side) plus the random (+0x40) scaled by the rider's ability class
 * (player +0x8f4 <= 3; +0x8f8 0/1/2 -> 0x9322c8 / 0x935860 / 0x9c4b4c, else 0), signed by the edge. Unreachable: with a
 * direction that is neither -1 nor 1 the original reads its own uninitialised frame slot [esp+0xc] as the tuning value, the edge
 * pointer and the sign (the monitor's direction is only ever +-1, 005380f0; the port uses 0 like 00528940's zeroed local). */
float rx_behavior_navigate_corner_offset(rx_addr b, rx_addr node, int32_t cur, int32_t side, float rnd) {
    const uint32_t same = (uint32_t)(side == cur);
    float t;
    rx_addr edge;
    int32_t sign;
    if (cur == -1) {
        if (same) { t = b1_tuning(0x009e3f98, b1_driver(b)); edge = node + 0x10; sign = 1; }
        else      { t = b1_tuning(0x009e42f8, b1_driver(b)); edge = node + 0x20; sign = -1; }
    } else if (cur == 1) {
        if (same) { t = b1_tuning(0x009e3f98, b1_driver(b)); edge = node + 0x20; sign = -1; }
        else      { t = b1_tuning(0x009e42f8, b1_driver(b)); edge = node + 0x10; sign = 1; }
    } else {
        const uint32_t dead = 0;                    /* [esp + 0xc] (unreachable, see above) */
        t = rx_bf(dead); edge = dead; sign = (int32_t)dead;
    }
    float r = 0.0f;
    {
        const rx_addr pl = b1_player(b1_driver(b));      /* [esi + 8] -> +0x74 */
        if (U32(pl, 0x8f4) <= 3u) {
            switch (U32(pl, 0x8f8)) {
            case 0: r = rnd * b1_data(0x009322c8); break;
            case 1: r = rnd * b1_data(0x00935860); break;
            case 2: r = rnd * b1_data(0x009c4b4c); break;
            default: break;
            }
        }
    }
    const float stored = same ? r + t : t - r;
    const float dx = F32(node, 0) - F32(edge, 0);
    const float dz = F32(node, 8) - F32(edge, 8);
    const float dy = F32(node, 4) - F32(edge, 4);
    return (sqrtf((dx * dx + dz * dz) + dy * dy) * (float)sign) * stored;
}
RX_FUNCTION(0x00528bd0, rx_behavior_navigate_corner_offset)

/* 00528a70 (EAX = the side the caller passes as EAX, EDX = the nearest node, ESI = behaviour, stack: side, random; RET 8) -> xmm0:
 * mode 2's version of the same offset: one table (0x9e3288) for both sides, the random (+0x48) scaled by 0x9322c8 / 0x92ff84 /
 * 0x935860 for the ability classes 0 / 1 / 2, and the edge chosen by the EAX side (the call sites pass the behaviour's picked side
 * in EAX and the corner's direction on the stack in the corner branch, the reverse in the non-corner branch). Same dead else path
 * as 00528bd0 (the corner direction is +-1). */
float rx_behavior_navigate_apex_offset(rx_addr b, rx_addr node, int32_t cur, int32_t side, float rnd) {
    const uint32_t same = (uint32_t)(side == cur);
    float t;
    rx_addr edge;
    int32_t sign;
    if (cur == -1) {
        t = b1_tuning(0x009e3288, b1_driver(b));
        if (same) { edge = node + 0x20; sign = -1; }
        else      { edge = node + 0x10; sign = 1; }
    } else if (cur == 1) {
        t = b1_tuning(0x009e3288, b1_driver(b));
        if (same) { edge = node + 0x10; sign = 1; }
        else      { edge = node + 0x20; sign = -1; }
    } else {
        const uint32_t dead = 0;                    /* [esp + 0xc] (unreachable, see 00528bd0) */
        t = rx_bf(dead); edge = dead; sign = (int32_t)dead;
    }
    float r = 0.0f;
    {
        const rx_addr pl = b1_player(b1_driver(b));
        if (U32(pl, 0x8f4) <= 3u) {
            switch (U32(pl, 0x8f8)) {
            case 0: r = rnd * b1_data(0x009322c8); break;
            case 1: r = rnd * b1_data(0x0092ff84); break;
            case 2: r = rnd * b1_data(0x00935860); break;
            default: break;
            }
        }
    }
    const float stored = same ? r + t : t - r;
    const float dx = F32(node, 0) - F32(edge, 0);
    const float dz = F32(node, 8) - F32(edge, 8);
    const float dy = F32(node, 4) - F32(edge, 4);
    return (sqrtf((dx * dx + dz * dz) + dy * dy) * (float)sign) * stored;
}
RX_FUNCTION(0x00528a70, rx_behavior_navigate_apex_offset)

/* 005278e0 (stack behaviour; RET 4): mode 1, the corner line. The same shape as 00527360 (the rider's normalized flat heading
 * and position, the side pick - inlined here without the airborne gate, its vf43 result discarded -, the 3D look-ahead along the
 * corner's first node, the nearest node, the sideways direction of that node) but the target is the nearest node moved by
 * 00528bd0 / 00528940 (the node's 0x400 flag picks), and the +0x28 hard-turn state machine of 00527360 follows on the heading
 * . target dot. The offset random is +0x40. */
void rx_behavior_navigate_line(rx_addr b) {
    RACE_FRAME_EBP(F, 16, 0x64, 3);             /* push ebp; and esp, -16; sub esp, 0x64; push ebx / esi / edi */
    rx_addr drv = b1_driver(b);
    rx_addr veh = b1_vehicle(drv);
    SF32(F, 0x28, F32(veh, 0xc0));              /* vehicle x */
    SF32(F, 0x2c, F32(veh, 0xc8));              /* vehicle z */
    SF32(F, 0x24, F32(veh, 0x138));             /* heading z */
    SF32(F, 0x20, F32(veh, 0x130));             /* heading x */
    {
        const float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
        const float len2 = hz * hz + hx * hx;
        SF32(F, 0x18, len2);
        if (len2 != 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
            SF32(F, 0x20, inv * hx);
            SF32(F, 0x24, hz * inv);
        }
    }
    if (U32(b, 4) != 0) {                       /* 00528d50's body, inlined without the airborne gate */
        uint32_t linked = 0, r = 0;
        const rx_addr c = U32(b, 0x30);
        if (c != 0 && (U32(c, 0x7c) == 2 || U32(c, 0x7c) == 3)) linked = 1;
        if (U32(b, 0x2c) != 0) r = rx_behavior_take_line_ready(U32(b, 0x2c));
        const int32_t forced = I32(drv, 0x310);
        if (forced != 0 && linked == 0) {
            if (forced == -1) SU32(b, 0xc, 0xffffffffu);
            else if (forced == 1) SU32(b, 0xc, 1);
        } else if (r != 0 && linked == 0) {
            SU32(b, 0xc, I32(drv, 0x314) != -1 ? 1u : 0xffffffffu);
        } else {
            (void)b1_vehicle_airborne(veh);     /* called, result discarded (the 00527360 inline has no gate) */
            drv = b1_driver(b);
            veh = b1_vehicle(drv);
            const float across = rx_race_track_lateral(drv, veh + 0xc0);
            const float k = b1_tuning(0x009e4238, drv);
            if (0.5f - k > across) SU32(b, 0xc, 0xffffffffu);
            else if (across > k + 0.5f) SU32(b, 0xc, 1);
        }
    }
    {
        const rx_addr pt = U32(U32(b, 0x34), 0x14);          /* the corner's first node */
        drv = b1_driver(b);
        veh = b1_vehicle(drv);
        const rx_addr start = U32(drv, 0x18), next = U32(drv, 0x1c);
        const float sx = F32(start, 0), sz = F32(start, 8);
        const float dz = F32(next, 8) - sz, dx = F32(next, 0) - sx;
        const float t = ((F32(veh, 0xc8) - sz) * dz + (F32(veh, 0xc0) - sx) * dx) / (dz * dz + dx * dx);
        const float px = dx * t + sx, pz = dz * t + sz;
        const float ptx = F32(pt, 0), ptz = F32(pt, 8);
        const float ddx = ptx - px, ddz = ptz - pz;
        const float dist = sqrtf(ddz * ddz + ddx * ddx);
        float lx = ptx, lz = ptz;
        SF32(F, 0x30, ptx); SF32(F, 0x34, ptz);
        if (b1_tuning(0x00a04958, drv) > dist) {
            rx_track_look_ahead(pt, F + 0x60, F + 0x40, b1_tuning(0x009e35e8, drv));
            lx = F32(F, 0x60); lz = F32(F, 0x68);
        }
        SU32(F, 0x30, U32(drv, 0x14));                      /* the nearest-node start (section, node) */
        SU32(F, 0x34, pt);
        SF32(F, 0x50, lx); SF32(F, 0x54, 0.0f); SF32(F, 0x58, lz);
        { float budget = b1_data(0x009c4750); rx_track_nearest_node(F + 0x30, F + 0x50, F + 0x40, &budget); }
        const rx_addr nearest = U32(F, 0x44);
        const float nx = F32(nearest, 0), nz = F32(nearest, 8);
        SF32(F, 0x60, nx); SF32(F, 0x68, nz);
        /* the node direction's sideways vector (dir x up): (-dirz, dirx), the third lane the original's 0 * dir differences */
        const float y0 = F32(nearest, 0x54) * 0.0f;
        const float sxx = y0 - F32(nearest, 0x58);
        const float szz = F32(nearest, 0x50) - y0;
        const float s3 = F32(nearest, 0x58) * 0.0f - F32(nearest, 0x50) * 0.0f;
        const float s2 = (s3 * s3 + sxx * sxx) + szz * szz;
        float sx2 = sxx, sz2 = szz;
        SF32(F, 0x30, sxx); SF32(F, 0x38, szz); SF32(F, 0x40, sxx); SF32(F, 0x48, szz); SF32(F, 0x1c, s2);
        if (s2 > 0.0f) {
            const float inv = 1.0f / sqrtf(s2);
            sx2 = inv * sxx;
            sz2 = inv * szz;
            SF32(F, 0x40, sx2); SF32(F, 0x48, sz2);
        }
        const uint32_t koff = (U32(veh, 0x198) + 4u * U32(veh, 0x194)) * 4u + 0x68u;
        float tx, tz;
        if ((U32(pt, koff) & 0x400u) == 0) {
            const float o = rx_behavior_navigate_corner_offset(b, nearest, I32(U32(b, 0x34), 0x1c), I32(b, 0xc), F32(b, 0x40));
            tx = sx2 * o;
            tz = sz2 * o;
        } else {
            const float o = rx_behavior_navigate_line_offset(b, nearest, I32(b, 0xc));
            tx = o * sx2;
            tz = sz2 * o;
        }
        tx = tx + nx;                           /* 527d30 */
        tz = tz + nz;
        /* (then rtti_shared_vf_007e3be0, an empty debug hook) */
        SF32(b, 0x20, tx);
        const float ddx2 = tx - F32(F, 0x28), ddz2 = tz - F32(F, 0x2c);
        SF32(b, 0x24, tz);
        const float d2 = ddz2 * ddz2 + ddx2 * ddx2;
        SF32(F, 0x30, ddx2); SF32(F, 0x34, ddz2); SF32(F, 0x1c, d2);
        float nx2 = ddx2, nz2 = ddz2;
        if (d2 != 0.0f) {
            const float inv = 1.0f / sqrtf(d2);
            nx2 = ddx2 * inv;
            nz2 = ddz2 * inv;
        }
        const float dot = nx2 * F32(F, 0x20) + nz2 * F32(F, 0x24);
        const int32_t state = I32(b, 0x28);
        if (state == -1 || state == 1) {
            SF32(b1_driver(b), 0x60, state == -1 ? b1_data(0x009cd9e8) : 1.0f /* 0x931f24 */);
            if (dot > b1_data(0x009cd9b4)) SU32(b, 0x28, 0);
            RACE_FRAME_END(F);
            return;
        }
        if (state != 0) { RACE_FRAME_END(F); return; }
        drv = b1_driver(b);
        if (b1_data(0x009cd9b0) > dot) {        /* hard turn toward the side the track position says */
            if (rx_race_track_lateral(drv, b1_vehicle(drv) + 0xc0) > 0.5f /* 0x9322cc */) {
                SF32(drv, 0x60, 1.0f);
                SU32(b, 0x28, 1);
            } else {
                SF32(drv, 0x60, b1_data(0x009cd9e8));
                SU32(b, 0x28, 0xffffffffu);
            }
            RACE_FRAME_END(F);
            return;
        }
        {
            const rx_addr row = 0x00a049b8u + 16u * b1_tuning_index(drv);
            const float k = rx_race_pow_difference(F32(row, 0xc), F32(row, 0), F32(row, 4), F32(row, 8), 1.0f /* 0x977638 */);
            const float ax = F32(F, 0x28) - F32(b, 0x20);
            const float az = F32(F, 0x2c) - F32(b, 0x24);
            const float a2 = ax * ax + az * az;
            SF32(F, 0x18, k);
            SF32(F, 0x30, ax); SF32(F, 0x34, az); SF32(F, 0x1c, a2);
            float nx3 = ax, nz3 = az;
            if (a2 != 0.0f) {
                const float inv = 1.0f / sqrtf(a2);
                nx3 = inv * ax;
                nz3 = inv * az;
            }
            SF32(F, 0x40, nx3 * k);
            SF32(F, 0x44, 0.0f);
            SF32(F, 0x48, nz3 * k);
            rx_race_to_vehicle_frame(b1_vehicle(drv), F + 0x40);
            const float hf = rx_race_heading_factor(b + 0x20, F + 0x28, F + 0x20);
            drv = b1_driver(b);
            SF32(drv, 0x60, F32(drv, 0x60) + hf * F32(F, 0x40));
            SU32(b, 0x28, 0);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005278e0, rx_behavior_navigate_line)

/* 00527fc0 (stack behaviour; RET 4): mode 2, the corner approach. The full 00528d50 side pick (with its airborne gate), the side
 * lock (+0x10: refreshed from +0xc while the monitor's run is far from over - 00526c60 / the monitor +0x20 - or at low speed,
 * 0x9cd9b8 / 0x935aa8), the 3D look-ahead from the driver's next node (tuning 0x9e4538), the nearest node: a corner node (0x200)
 * takes the sideways offset of 00528bd0 / 00528940 from that node's own direction (the driver's previous node's 0x400 flag picks;
 * the offset random +0x44 and the locked side +0x10); a plain node takes the flat look-ahead (0x9e45f8), and with the previous
 * node's 0x400 flag clear the offset of 00528a70 (random +0x48, the picked side +0xc in EAX) from the look-ahead direction.
 * The same +0x28 hard-turn state machine; after the steering push, |heading . target| (0x931da8) sets the driver's +0x68:
 * +-1 when it is small, else scaled by 0x977af4 / 0x977af8 by the steer's sign. */
void rx_behavior_navigate_apex(rx_addr b) {
    RACE_FRAME_EBP(F, 16, 0x84, 3);             /* push ebp; and esp, -16; sub esp, 0x84; push ebx / esi / edi */
    rx_addr drv = b1_driver(b);
    rx_addr veh = b1_vehicle(drv);
    SF32(F, 0x38, F32(veh, 0xc0));
    SF32(F, 0x3c, F32(veh, 0xc8));
    SF32(F, 0x34, F32(veh, 0x138));
    SF32(F, 0x30, F32(veh, 0x130));
    {
        const float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
        const float len2 = hz * hz + hx * hx;
        SF32(F, 0x18, len2);
        if (len2 != 0.0f) {
            const float inv = 1.0f /* 0x931f24 */ / sqrtf(len2);
            SF32(F, 0x30, inv * hx);
            SF32(F, 0x34, hz * inv);
        }
    }
    rx_behavior_navigate_pick_side(b, 0, 0);    /* the full 00528d50 body, when_airborne = 0 */
    if (U32(b, 0x10) == 0) {
        SU32(b, 0x10, U32(b, 0xc));
    } else {
        drv = b1_driver(b);
        veh = b1_vehicle(drv);
        const float speed_thresh = b1_data(0x009cd9b8);
        int lock = (speed_thresh > F32(veh, 0x150)) ? 1 : 0;
        if (!lock) {
            const rx_addr m = U32(b, 0x34);
            const float rem = rx_driver_run_distance(m, 0);
            const float frac = rem / F32(m, 0x20);
            const float left = 1.0f - frac;
            lock = (left > b1_data(0x00935aa8)) ? 1 : 0;
        }
        if (lock) SU32(b, 0x10, U32(b, 0xc));
    }
    drv = b1_driver(b);
    rx_track_look_ahead(U32(drv, 0x1c), F + 0x80, F + 0x70, b1_tuning(0x009e4538, drv));
    const float lx = F32(F, 0x80), lz = F32(F, 0x88);
    SU32(F, 0x40, U32(drv, 0x14));
    SU32(F, 0x44, U32(drv, 0x1c));
    SF32(F, 0x60, lx); SF32(F, 0x64, 0.0f); SF32(F, 0x68, lz);
    { float budget = b1_data(0x009c4750); rx_track_nearest_node(F + 0x40, F + 0x60, F + 0x50, &budget); }
    const rx_addr nearest = U32(F, 0x54);
    veh = b1_vehicle(drv);
    const uint32_t koff = (U32(veh, 0x198) + 4u * U32(veh, 0x194)) * 4u + 0x68u;
    float tx, tz;
    if ((U32(nearest, koff) & 0x200u) != 0) {   /* the nearest node is in the corner */
        const float y0 = F32(nearest, 0x54) * 0.0f;
        const float sxx = y0 - F32(nearest, 0x58);
        const float szz = F32(nearest, 0x50) - y0;
        const float s3 = F32(nearest, 0x58) * 0.0f - F32(nearest, 0x50) * 0.0f;
        const float s2 = (s3 * s3 + sxx * sxx) + szz * szz;
        float sx = sxx, sz = szz;
        SF32(F, 0x20, sxx); SF32(F, 0x28, szz); SF32(F, 0x40, sxx); SF32(F, 0x48, szz);
        if (s2 > 0.0f) {
            const float inv = 1.0f / sqrtf(s2);
            sx = inv * sxx;
            sz = szz * inv;
            SF32(F, 0x40, sx); SF32(F, 0x48, sz);
        }
        const int32_t side = I32(b, 0x10);
        float o;
        if ((U32(U32(drv, 0x18), koff) & 0x400u) == 0)
            o = rx_behavior_navigate_corner_offset(b, nearest, I32(U32(b, 0x34), 0x1c), side, F32(b, 0x44));
        else
            o = rx_behavior_navigate_line_offset(b, nearest, side);
        tx = sx * o + F32(nearest, 0);
        tz = sz * o + F32(nearest, 8);
    } else {
        rx_track_look_ahead_xz(U32(b1_driver(b), 0x1c), F + 0x20, b1_tuning(0x009e45f8, b1_driver(b)), F + 0x18);
        const float fx = F32(F, 0x20), fz = F32(F, 0x24);
        SU32(F, 0x50, U32(drv, 0x14));
        SU32(F, 0x54, U32(drv, 0x1c));
        SF32(F, 0x70, fx); SF32(F, 0x74, 0.0f); SF32(F, 0x78, fz);
        { float budget = b1_data(0x009c4750); rx_track_nearest_node(F + 0x50, F + 0x70, F + 0x40, &budget); }
        if ((U32(U32(drv, 0x18), koff) & 0x400u) != 0) {
            tx = fx;
            tz = fz;
        } else {
            const float y0 = 0.0f * 0.0f;
            const float sxx = y0 - F32(F, 0x1c);
            const float szz = F32(F, 0x18) - y0;
            const float s3 = F32(F, 0x1c) * 0.0f - F32(F, 0x18) * 0.0f;
            const float s2 = (s3 * s3 + sxx * sxx) + szz * szz;
            float sx = sxx, sz = szz;
            SF32(F, 0x50, sxx); SF32(F, 0x58, szz); SF32(F, 0x88, szz); SF32(F, 0x80, sxx);
            if (s2 > 0.0f) {
                const float inv = 1.0f / sqrtf(s2);
                sx = inv * sxx;
                sz = szz * inv;
                SF32(F, 0x50, sx); SF32(F, 0x58, sz);
            }
            const float o = rx_behavior_navigate_apex_offset(b, U32(F, 0x44), I32(b, 0xc), I32(U32(b, 0x34), 0x1c), F32(b, 0x48));
            tx = o * sx + fx;
            tz = o * sz + fz;
        }
    }
    SF32(F, 0x50, tx);
    SF32(F, 0x58, tz);
    /* (then rtti_shared_vf_007e3be0, an empty debug hook) */
    SF32(b, 0x20, tx);
    const float dx = tx - F32(F, 0x38), dz = tz - F32(F, 0x3c);
    SF32(b, 0x24, tz);
    const float d2 = dz * dz + dx * dx;
    SF32(F, 0x20, dx); SF32(F, 0x24, dz); SF32(F, 0x18, d2);
    float nx = dx, nz = dz;
    if (d2 != 0.0f) {
        const float inv = 1.0f / sqrtf(d2);
        nx = dx * inv;
        nz = dz * inv;
    }
    const float dot = nz * F32(F, 0x34) + nx * F32(F, 0x30);
    const int32_t state = I32(b, 0x28);
    if (state == -1 || state == 1) {
        SF32(b1_driver(b), 0x60, state == -1 ? b1_data(0x009cd9e8) : 1.0f /* 0x931f24 */);
        if (dot > b1_data(0x009cd9b4)) SU32(b, 0x28, 0);
        RACE_FRAME_END(F);
        return;
    }
    if (state != 0) { RACE_FRAME_END(F); return; }
    drv = b1_driver(b);
    if (b1_data(0x009cd9b0) > dot) {
        if (rx_race_track_lateral(drv, b1_vehicle(drv) + 0xc0) > 0.5f /* 0x9322cc */) {
            SF32(drv, 0x60, 1.0f);
            SU32(b, 0x28, 1);
        } else {
            SF32(drv, 0x60, b1_data(0x009cd9e8));
            SU32(b, 0x28, 0xffffffffu);
        }
        RACE_FRAME_END(F);
        return;
    }
    {
        const rx_addr row = 0x00a049b8u + 16u * b1_tuning_index(drv);
        const float k = rx_race_pow_difference(F32(row, 0xc), F32(row, 0), F32(row, 4), F32(row, 8), 1.0f /* 0x977638 */);
        const float ax = F32(F, 0x38) - F32(b, 0x20);       /* vehicle - target (the other way from mode 1) */
        const float az = F32(F, 0x3c) - F32(b, 0x24);
        const float a2 = ax * ax + az * az;
        SF32(F, 0x20, k);
        SF32(F, 0x40, ax); SF32(F, 0x44, az); SF32(F, 0x18, a2);
        float nx3 = ax, nz3 = az;
        if (a2 != 0.0f) {
            const float inv = 1.0f / sqrtf(a2);
            nx3 = inv * ax;
            nz3 = inv * az;
        }
        SF32(F, 0x40, nx3 * k);
        SF32(F, 0x44, 0.0f);
        SF32(F, 0x48, nz3 * k);
        rx_race_to_vehicle_frame(b1_vehicle(drv), F + 0x40);
        const float hf = rx_race_heading_factor(b + 0x20, F + 0x38, F + 0x30);
        drv = b1_driver(b);
        SF32(drv, 0x60, F32(drv, 0x60) + hf * F32(F, 0x40));
        /* |heading . unit(target)|: the driver's +0x68 line value */
        const float t2 = tx * tx + tz * tz;
        SF32(F, 0x20, t2);
        float nx4 = tx, nz4 = tz;
        if (t2 != 0.0f) {
            const float inv = 1.0f / sqrtf(t2);
            nx4 = inv * tx;
            nz4 = inv * tz;
        }
        const float dot2 = nz4 * F32(F, 0x34) + nx4 * F32(F, 0x30);
        const float ad = fabsf(dot2);                       /* 00401000 Float_Abs */
        if (b1_data(0x00931da8) > ad) {
            if (F32(drv, 0x60) > 0.0f) SF32(drv, 0x68, 1.0f);
            else SF32(drv, 0x68, b1_data(0x009cd9e8));
        } else {
            if (F32(drv, 0x60) > 0.0f) SF32(drv, 0x68, ad * b1_data(0x00977af8));
            else SF32(drv, 0x68, ad * b1_data(0x00977af4));
        }
        SU32(b, 0x28, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00527fc0, rx_behavior_navigate_apex)

/* ------------------------------------------------------------------------------------------------ BehaviorOverrideSpline */
/* The driver's AI control channels +0x2f0 .. +0x324: off (0050ac80, inlined) when a spline with flag 0x80 is taken, on (0050acc0,
 * inlined at the spline's end) afterwards. */
static void b1_clear_line_flags(rx_addr drv) { rx_ai_controls_off(drv); }
/* The class / kind flags of a spline for the rider's vehicle. */
static uint32_t b1_spline_flags(rx_addr b, rx_addr sp) {
    rx_addr veh = b1_vehicle(b1_driver(b));
    return U32(sp, (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0x18u);
}
static void b1_take_spline(rx_addr b, rx_addr sp) {
    SU32(b, 0x38, sp);
    SU32(b, 0x3c, U32(sp, 0));
    SU32(b, 0x40, U32(U32(sp, 0), 0x64));
}

/* 00529c40 (EAX = behaviour) -> 0: waiting: when no race-line rule override is active (rule type 0 +0x0c) and the rider is on the
 * track, look for a spline within the tuning's distance (0x9e3c08) ahead; found: state 1 (entering) at its first node. */
uint32_t rx_behavior_override_spline_wait(rx_addr b) {
    rx_addr r0 = rx_driver_rule_0(b1_driver(b));
    rx_addr r1 = rx_driver_rule_1(b1_driver(b));
    rx_addr rb = rx_driver_offtrack_penalty_rule(b1_driver(b));
    uint32_t active = r0 != 0 ? U32(r0, 0xc) : 0, on = 1;
    if (r1 != 0) on = U32(r1, 0x50);
    else if (rb != 0) on = U32(rb, 0x10);
    if (active != 0 || on == 0) return 0;
    rx_addr sp = b1_spline_ahead(b, b1_tuning(0x009e3c08, b1_driver(b)));
    SU32(b, 0x38, sp);
    if (sp == 0) return 0;
    SU32(b, 0x58, 1);
    SU32(b, 0x3c, U32(sp, 0));
    SU32(b, 0x40, U32(U32(sp, 0), 0x64));
    return 0;
}
RX_FUNCTION(0x00529c40, rx_behavior_override_spline_wait)

/* 00529ce0 (EAX = behaviour) -> 0: entering: once the rider is past the spline's first node (along its direction +0x50): when it
 * is more than 25 (driver +0x328 == 0) / 12 sideways from it, give up (reset); else follow (state 2), with the spline's flags 0x40 /
 * 0x80, the driver's line flags cleared and, for a steering spline, the other behaviours paused. Not yet past: 0052a6d0 may pick
 * another spline. */
uint32_t rx_behavior_override_spline_enter(rx_addr b) {
    rx_addr drv = b1_driver(b), n = U32(b, 0x3c), veh = b1_vehicle(drv);
    float d = (F32(n, 0x58) * (F32(veh, 0xc8) - F32(n, 8)) + F32(n, 0x50) * (F32(veh, 0xc0) - F32(n, 0))) + F32(n, 0x54) * 0.0f;
    if (d > 0.0f) {
        veh = b1_vehicle(drv);
        float lat = (F32(n, 8) - F32(veh, 0xc8)) * F32(n, 0x50) + (F32(n, 0) - F32(veh, 0xc0)) * F32(n, 0x58);
        float lim = U32(drv, 0x328) == 0 ? 25.0f /* 0x92e134 */ : 12.0f /* 0x97785c */;
        if (fabsf(lat) > lim) { b1_behavior_reset(b); return 0; }
        b1_clear_line_flags(drv);
        SU32(b, 0x58, 2);
        rx_addr sp = U32(b, 0x38);
        SU32(b, 0x50, b1_spline_flags(b, sp) & 0x40);
        SU32(b, 0x54, b1_spline_flags(b, sp) & 0x80);
        if (U32(b, 0x50) != 0) {
            if (U32(b, 0x60) != 0) b1_behavior_vf7(U32(b, 0x60), 0);
            if (U32(b, 0x64) != 0) b1_behavior_vf7(U32(b, 0x64), 0);
            b1_behavior_vf7(U32(b, 0x5c), 0);
        }
        return 0;
    }
    rx_addr sp = b1_spline_reselect(b);
    if (sp != 0) b1_take_spline(b, sp);
    return 0;
}
RX_FUNCTION(0x00529ce0, rx_behavior_override_spline_enter)

/* 00529eb0 (EAX = behaviour) -> 0: following: advance along the spline's nodes as the rider passes them; at its end continue on
 * one of the next splines usable by the vehicle (up to 10, chosen by 0050a360), or end the override (the line side re-picked, the
 * driver's line flags set, reset). While the NavigateTrack side differs from the spline's (+0x44) on a non-steering spline: 0050acc0
 * and reset. A steering spline adds a steering push toward its look-ahead point (tuning 0x9e3c08 / 0x9e3c68 gains). */
uint32_t rx_behavior_override_spline_follow(rx_addr b) {
    rx_addr r1 = rx_driver_rule_1(b1_driver(b));
    rx_addr rb = rx_driver_offtrack_penalty_rule(b1_driver(b));
    uint32_t on = 1;
    if (r1 != 0) on = U32(r1, 0x50);
    else if (rb != 0) on = U32(rb, 0x10);
    rx_addr n = U32(b, 0x40);
    if (U32(n, 0x64) != 0 || on != 0) {
        rx_addr drv = b1_driver(b), veh = b1_vehicle(drv);
        float d = (F32(n, 0x58) * (F32(veh, 0xc8) - F32(n, 8)) + F32(n, 0x54) * 0.0f) + F32(n, 0x50) * (F32(veh, 0xc0) - F32(n, 0));
        if (d > 0.0f) {
            SU32(b, 0x3c, n);
            SU32(b, 0x40, U32(n, 0x64));
            if (U32(b, 0x54) != 0) b1_clear_line_flags(drv);
        }
    }
    if (U32(b, 0x40) == 0) {
        rx_addr sp = U32(b, 0x38);
        if (U32(sp, 0x10) != 0) {
            RX_FRAME_ENTER(F, 0x28);
            for (uint32_t i = 0; i < 10; i++) SU32(F, 4 * i, 0);
            uint32_t cnt = 0;
            if (I32(sp, 0x10) > 0) {
                rx_addr veh = b1_vehicle(b1_driver(b));
                uint32_t off = (U32(veh, 0x198) + U32(veh, 0x194) * 4u) * 4u + 0x18u;
                rx_addr list = U32(sp, 8);
                for (int32_t i = 0;;) {
                    rx_addr e = U32(list, 4 * (uint32_t)i);
                    if (U8(e + off, 0) & 1) SU32(F, 4 * cnt++, e);
                    if (cnt == 10) break;
                    i++;
                    if (!(i < I32(U32(b, 0x38), 0x10))) break;
                }
                if (cnt != 0) {
                    rx_addr slot;
                    rx_addr r = rx_race_choose_section(b1_driver(b), F, cnt, 0, &slot);
                    if (r != 0) {
                        b1_take_spline(b, r);
                        SU32(b, 0x50, b1_spline_flags(b, r) & 0x40);
                        SU32(b, 0x54, b1_spline_flags(b, r) & 0x80);
                    }
                }
            }
            RX_FRAME_LEAVE(F, 0x28);
        }
        if (U32(b, 0x40) == 0) {
            rx_behavior_navigate_pick_side(U32(b, 0x5c), 1, 1);
            rx_ai_controls_on(b1_driver(b));     /* (inlined) */
            b1_behavior_reset(b);
            return 0;
        }
    }
    rx_behavior_navigate_pick_side(U32(b, 0x5c), 1, 1);
    if (U32(b, 0x44) != U32(U32(b, 0x5c), 0xc)) {
        if (U32(b, 0x50) == 0) {
            rx_ai_controls_on(b1_driver(b));
            b1_behavior_reset(b);
            return 0;
        }
    } else if (U32(b, 0x50) == 0) return 0;
    /* steer along the spline */
    rx_addr drv = b1_driver(b), veh = b1_vehicle(drv);
    const float px = F32(veh, 0xc0), pz = F32(veh, 0xc8);
    float hx = F32(veh, 0x130), hz = F32(veh, 0x138);
    float len2 = hx * hx + hz * hz;
    if (len2 != 0.0f) {
        float inv = 1.0f / sqrtf(len2);
        hx = inv * hx;
        hz = hz * inv;
    }
    RX_FRAME_ENTER(F, 0x40);                    /* F + 0x00 look pos, + 0x10 look dir, + 0x20 push, + 0x30 / 0x38 points */
    rx_track_look_ahead(U32(b, 0x40), F, F + 0x10, b1_tuning(0x009e3c08, drv));
    const float lx = F32(F, 0), lz = F32(F, 8);
    SF32(b, 0x48, lx);
    SF32(b, 0x4c, lz);
    drv = b1_driver(b);
    rx_addr row = 0x009e3c68u + 16u * b1_tuning_index(drv);
    float k = F32(row, 8) / (float)race_pow(1.0 /* 0x977638 */, (double)F32(row, 0xc));
    k = k - F32(row, 0) / (float)race_pow(1.0, (double)F32(row, 4));
    float tx = px - lx, tz = pz - lz;
    float t2 = tx * tx + tz * tz;
    if (t2 != 0.0f) {
        float inv = 1.0f / sqrtf(t2);
        tx = inv * tx;
        tz = inv * tz;
    }
    SF32(F, 0x20, tx * k);
    SF32(F, 0x24, 0.0f);
    SF32(F, 0x28, tz * k);
    rx_race_to_vehicle_frame(b1_vehicle(drv), F + 0x20);
    SF32(F, 0x30, lx); SF32(F, 0x34, lz);
    SF32(F, 0x38, px); SF32(F, 0x3c, pz);
    RX_FRAME_ENTER(H, 0x10);
    SF32(H, 0, hx); SF32(H, 4, hz);
    float h = rx_race_heading_factor(F + 0x30, F + 0x38, H);
    RX_FRAME_LEAVE(H, 0x10);
    float push = F32(F, 0x20);
    RX_FRAME_LEAVE(F, 0x40);
    drv = b1_driver(b);
    SF32(drv, 0x60, h * push + F32(drv, 0x60));
    return 0;
}
RX_FUNCTION(0x00529eb0, rx_behavior_override_spline_follow)
