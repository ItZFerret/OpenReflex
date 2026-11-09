/* race_beh2_misc.c -- BehaviorSuckRacing (vtable 0x0093fcc0), the spline scans of BehaviorOverrideSpline (0x0093f928) and the
 * shared Behavior::vf7 (enable).
 *
 * BehaviorSuckRacing ("suck racing": the AI lets the humans catch up). Only for tuning set 3 (player +0x8f4) and activity kinds
 * 0, 1, 2, 6, 0x11, 0x12, 0x13. Layout: +0x04 enabled, +0x08 driver, +0x0c mode (0 hold back, 1 / 2 two other modes (005352c0 /
 * 00535380: ported; reached by the SX / Terracross worlds), 3 decide), +0x10 the mode's step, +0x14, +0x18 next decision (driver clock), +0x1c, +0x20.
 * Decide (every 5 s): when a human is placed behind this rider (or the global 0x00ae1b90 is set) draw r = rand % 150 (the game's
 * LCG 0x9cb860) and with an odd r below (place + riders / 2) / riders * 100 pick a mode at random (kind 2 / 0x11: 0 or 1, kind 6:
 * 1, else 0 or 2). Hold back (mode 0): disable the driver's behaviour 1 for 4 s, then enable it and reset (mode 3).
 * BehaviorOverrideSpline: +0x08 driver, +0x0c..+0x30 remembered splines (10), +0x34 their count, +0x38 the current spline, +0x44
 * its side, +0x5c the navigate behaviour (its side +0xc). A spline (track +0x130[i], +0x1d8 of them): +0x00 its first node, +0x14
 * busy, +0x18 + 4 * (4 * class + kind) per vehicle flags (1 usable, 2 remember), +0x118 bit 1 (a spline). (notes_beh2.md) */
#include "race_beh2_extern.h"
#include "rider_cap.h"

/* 0052dc40 (vf7 of 28 classes: ECX = object, arg; RET 4) -> EAX = arg: object +0x04 = arg (a behaviour's enabled flag). */
uint32_t rx_behavior_set_enabled(rx_addr self, uint32_t enabled) {
    SU32(self, 4, enabled);
    return enabled;
}
RX_FUNCTION(0x0052dc40, rx_behavior_set_enabled)

/* 00534fe0 BehaviorSuckRacing::vf5: the type id 0x17. */
uint32_t rx_behavior_suck_racing_type(rx_addr self) { (void)self; return 0x17; }
RX_FUNCTION(0x00534fe0, rx_behavior_suck_racing_type)

/* 00534fa0 BehaviorSuckRacing::vf6 (ECX = behaviour) -> EAX 0: reset to deciding in 5 s. */
uint32_t rx_behavior_suck_racing_reset(rx_addr beh) {
    const rx_addr d = U32(beh, 8);
    SF32(beh, 0x14, 0.0f);
    SF32(beh, 0x18, F32(d, 0x130) + 5.0f /* 0x935890 */);
    SF32(beh, 0x1c, 0.0f);
    SU32(beh, 0xc, 3);
    SU32(beh, 0x10, 0);
    SU32(beh, 0x20, 0);
    return 0;
}
RX_FUNCTION(0x00534fa0, rx_behavior_suck_racing_reset)

/* 00535480 (ESI = behaviour): mode 0, hold back: step 0 disables behaviour 1 (0053baa0) for 4 s, step 1 enables it again when the
 * time is up, step 2 resets (vf6). */
void rx_suck_racing_hold(rx_addr beh) {
    const uint32_t step = U32(beh, 0x10);
    if (step == 0) {
        const rx_addr b = rx_driver_behavior_1(U32(beh, 8));
        if (b != 0) beh2_behavior_enable(b, 0);
        SF32(beh, 0x18, F32(U32(beh, 8), 0x130) + 4.0f /* 0x9322c0 */);
        SU32(beh, 0x10, 1);
    } else if (step == 1) {
        const rx_addr d = U32(beh, 8);
        if (F32(d, 0x130) > F32(beh, 0x18)) {
            SU32(beh, 0x10, 2);
            const rx_addr b = rx_driver_behavior_1(d);
            if (b != 0) beh2_behavior_enable(b, 1);
        }
    } else if (step == 2) {
        beh2_behavior_reset(beh);
    }
}
RX_FUNCTION(0x00535480, rx_suck_racing_hold)

/* The position of a driver in the activity's standings (+0x2d0[], +0x26c entries), -1 if not there. */
static int32_t standing_of(rx_addr act, rx_addr driver, int32_t n) {
    for (int32_t i = 0; i < n; i++) if (rx_stand(act, (uint32_t)i) == driver) return i;
    return -1;
}

/* 00535090 (arg = behaviour; RET 4): mode 3, decide (see the file comment). */
void rx_suck_racing_decide(rx_addr beh) {
    rx_addr d = U32(beh, 8);
    if (!(F32(d, 0x130) > F32(beh, 0x18))) return;
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    const int32_t place = standing_of(act, d, I32(act, 0x26c));
    const rx_addr act2 = U32(U32(d, 0x74), 0x94);
    const int32_t riders = I32(act2, 0x26c);
    const float friders = (float)riders;
    int32_t humans = 0;
    for (int32_t i = 0; i < riders; i++)
        if (U32(U32(U32(U32(act2, 0x268), 4u * (uint32_t)i), 0x74), 0x10) == 0) humans++;
    int32_t j = 0;
    for (; j < humans; j++) {
        const int32_t n = I32(act, 0x26c);
        const rx_addr h = beh2_activity_driver_of_kind(act, 0, (uint32_t)j);
        if (standing_of(act, h, n) > place) break;
    }
    if (j == humans && U32(0, 0x00ae1b90) == 0) return;
    const int32_t r = (int32_t)race_rand() % 150;
    const float chance = ((friders * 0.5f /* 0x9322cc */ + (float)place) / friders) * 100.0f /* 0x93005c */;
    if (chance > (float)r && (r & 1) != 0) {
        const uint32_t kind = beh2_activity_kind_inline(U32(U32(U32(beh, 8), 0x74), 0x94));
        uint32_t modes[2] = {0, 2};
        int32_t count = 2;
        if (kind == 2 || kind == 0x11) modes[1] = 1;
        else if (kind == 6) { modes[0] = 1; count = 1; }
        const int32_t k = (int32_t)race_rand() % count;
        SU32(beh, 0x10, 0);
        SU32(beh, 0xc, modes[k]);
    }
    d = U32(beh, 8);
    SF32(beh, 0x18, F32(d, 0x130) + 5.0f /* 0x935890 */);
}
RX_FUNCTION(0x00535090, rx_suck_racing_decide)

/* 00526c60 (EDI = a behaviour of the driver; xmm0 = the result, EDX as the original leaves it): the distance left along the run of
 * nodes the vehicle is on: the vehicle's position projected on the driver's segment (+0x18 -> +0x1c, the fraction clamped to 0..1; NaN
 * counts as 1) gives the distance from the segment's start (SqrtF); the node's length (+0x4f0) minus it, plus the lengths of the
 * following nodes while their flags for this vehicle class (+0x68 + 4 * (vehicle +0x198 + 4 * +0x194)) have bit 0x200, over the
 * next sections while the section allows it (+0x10). */
float rx_driver_run_distance(rx_addr beh, uint32_t* edx_out) {
    const rx_addr d = U32(beh, 8), n1 = U32(d, 0x18), n2 = U32(d, 0x1c), veh = U32(U32(d, 0x74), 0x1ac);
    const float x4 = F32(n1, 0), z5 = F32(n1, 8);
    const float ex = rx_subs(F32(n2, 0), x4), ez = rx_subs(F32(n2, 8), z5);
    float t = rx_divs(rx_adds(rx_muls(rx_subs(F32(veh, 0xc8), z5), ez), rx_muls(rx_subs(F32(veh, 0xc0), x4), ex)),
                      rx_adds(rx_muls(ez, ez), rx_muls(ex, ex)));
    if (1.0f /* 0x931f24 */ > t) { if (0.0f > t) t = 0.0f; }
    else t = 1.0f;
    const float px = rx_subs(rx_adds(rx_muls(ex, t), x4), F32(n1, 0));
    const float pz = rx_subs(rx_adds(rx_muls(ez, t), z5), F32(n1, 8));
    const float dy = rx_subs(-0.0f /* 0x9774a8 */, F32(n1, 4));
    const float sq = rx_sqrts(rx_adds(rx_adds(rx_muls(pz, pz), rx_muls(dy, dy)), rx_muls(px, px)));   /* SqrtF 0042c230 */
    float rem = rx_subs(F32(n1, 0x4f0), sq);
    rx_addr node = n2, sec = U32(d, 0x14);
    const uint32_t off = 4u * (U32(veh, 0x198) + 4u * U32(veh, 0x194)) + 0x68u;
    if (edx_out) *edx_out = node == 0 ? n1 : off;
    if (node == 0) return rem;
    for (;;) {
        if ((U32(node, off) & 0x200u) == 0) return rem;
        rem = rx_adds(F32(node, 0x4f0), rem);
        node = U32(node, 0x64);
        if (node != 0) continue;
        if (U32(sec, 0x10) == 0) return rem;
        sec = U32(U32(sec, 8), 0);
        node = U32(sec, 0);
        if (node == 0) return rem;
    }
}
RX_FUNCTION(0x00526c60, rx_driver_run_distance)

static void suck_racing_reset_virtual(rx_addr b) {   /* vf6 (slot 6, +0x18): 00534fa0 */
    typedef uint32_t (*fn_t)(rx_addr);
    const rx_addr fn = race_vfn(b, 6);
    fn_t f = RX_FN(fn, fn_t);
    if (f) (void)f(b); else race_thiscall(fn, b, 0, 0, 0, 0, 0);
}

/* 005352c0 (ESI = behaviour; no result): mode 1 of BehaviorSuckRacing. Step 0: the next decision time (+0x18) = the driver's clock
 * (+0x130) + 1 + 0.5 x a random fraction (the game's LCG), and (kind 6 only: the driver must have its type 0x14 behaviour, its
 * +0xa4 set) step 1; step 1: until +0x18 passes the clock the driver is held (+0x20 = 1), then step 2; step 2: the reset (vf6). */
void rx_suck_racing_mode1(rx_addr b) {
    const rx_addr d = U32(b, 8);
    switch (U32(b, 0x10)) {
    case 0: {
        const uint32_t r = race_rand();
        SF32(b, 0x18, rx_adds(rx_adds(rx_muls(rx_muls((float)r, 3.0518509e-05f /* 0x92e054 */), 0.5f /* 0x9322cc */), F32(d, 0x130)), 1.0f));
        const uint32_t kind = beh2_activity_ui_kind(U32(U32(d, 0x74), 0x94));
        if (kind == 6) {
            rx_addr n;
            RACE_CORE_CALL(1, n = rx_driver_behavior_14(U32(b, 8)));
            if (n == 0) return;
            if (U32(n, 0xa4) == 0) return;
        }
        SU32(b, 0x10, 1);
        return;
    }
    case 1:
        if (F32(b, 0x18) > F32(d, 0x130)) { SU32(b, 0x20, 1); return; }
        SU32(b, 0x10, 2);
        return;
    case 2:
        RACE_CORE_CALL(0, suck_racing_reset_virtual(b));
        return;
    default:
        return;
    }
}
RX_FUNCTION(0x005352c0, rx_suck_racing_mode1)

/* 00535380 (ESI = behaviour; no result): mode 2 of BehaviorSuckRacing. Step 0: a random threshold +0x14 (a fraction of the game's
 * LCG) and step 1; step 1: the driver's type 0x12 behaviour (none: wait ends at once, +0x1c = 0, step 2); in its mode 2 the
 * fraction of the run left (00526c60 / its length +0x20) below 1 - the threshold: wait 3 s more (+0x1c = clock + 3) and step 2;
 * step 2: after +0x1c the reset (vf6). Whenever +0x1c is still ahead of the clock the driver is held (+0x20 = 1). Frame: push edi. */
void rx_suck_racing_mode2(rx_addr b) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const rx_addr d = U32(b, 8);
    switch (U32(b, 0x10)) {
    case 0: {
        const uint32_t r = race_rand();
        SF32(b, 0x14, rx_muls((float)r, 3.0518509e-05f /* 0x92e054 */));
        SU32(b, 0x10, 1);
        break;
    }
    case 1: {
        rx_addr n;
        RACE_CORE_CALL(1, n = rx_driver_behavior_12(d));
        if (n == 0) { SF32(b, 0x1c, 0.0f); SU32(b, 0x10, 2); break; }
        if (U32(n, 0xc) != 2) break;
        float dist;
        RACE_CORE_CALL(0, dist = rx_driver_run_distance(n, 0));
        const float left = rx_subs(1.0f, rx_divs(dist, F32(n, 0x20)));
        if (left > F32(b, 0x14)) {
            SF32(b, 0x1c, rx_adds(F32(U32(b, 8), 0x130), 3.0f /* 0x92fe48 */));
            SU32(b, 0x10, 2);
        }
        break;
    }
    case 2:
        if (F32(d, 0x130) > F32(b, 0x1c)) RACE_CORE_CALL(0, suck_racing_reset_virtual(b));
        break;
    default:
        break;
    }
    if (F32(b, 0x1c) > F32(U32(b, 8), 0x130)) SU32(b, 0x20, 1);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00535380, rx_suck_racing_mode2)

/* 00534ff0 BehaviorSuckRacing::vf4 (ECX = behaviour; RET 4, unused) -> EAX 0: run the current mode. */
uint32_t rx_behavior_suck_racing_update(rx_addr beh, float dt) {
    (void)dt;   /* the vf4 argument (unused here) */
    if (U32(beh, 4) == 0) return 0;
    const rx_addr pl = U32(U32(beh, 8), 0x74);
    if (U32(pl, 0x8f4) != 3) return 0;
    const uint32_t kind = beh2_activity_ui_kind(U32(pl, 0x94));
    if (kind > 0x13 || !(kind <= 2 || kind == 6 || kind >= 0x11)) return 0;
    switch (U32(beh, 0xc)) {
    case 0: rx_suck_racing_hold(beh); break;
    case 1: RACE_CORE_CALL(0, rx_suck_racing_mode1(beh)); break;   /* ESI = behaviour */
    case 2: RACE_CORE_CALL(0, rx_suck_racing_mode2(beh)); break;
    case 3: rx_suck_racing_decide(beh); break;
    }
    return 0;
}
RX_FUNCTION(0x00534ff0, rx_behavior_suck_racing_update)

/* The vehicle flags of a spline for this vehicle (class +0x194, kind +0x198). */
static uint32_t spline_flags(rx_addr spline, rx_addr vehicle) {
    return U32(spline, 0x18 + 4u * (U32(vehicle, 0x198) + 4u * U32(vehicle, 0x194)));
}
/* The position of a spline in the behaviour's remembered list (flag 2 only), -1 if not there. */
static int32_t remembered_at(rx_addr beh, rx_addr spline, uint32_t flags) {
    if ((flags & 2) == 0) return -1;
    for (uint32_t j = 0; j < U32(beh, 0x34); j++) if (U32(beh, 0xc + 4 * j) == spline) return (int32_t)j;
    return -1;
}
/* The end of both scans: 0050a360 picks one of the candidates (preferring flag 2 in activity kind 3 for tuning set 0); a pick
 * clears the remembered list and takes the navigate behaviour's side. Returns the pick (0 = none). */
static rx_addr choose_candidate(rx_addr beh, rx_addr list, uint32_t found) {
    const uint32_t kind = beh2_activity_kind_inline(U32(U32(U32(beh, 8), 0x74), 0x94));
    const uint32_t prefer = (kind == 3 && U32(U32(U32(beh, 8), 0x74), 0x8f4) == 0) ? 1u : 0u;
    rx_addr slot;
    const rx_addr chosen = rx_race_choose_section(U32(beh, 8), list, found, prefer, &slot);
    if (chosen != 0) {
        SU32(beh, 0x34, 0);
        SU32(beh, 0x44, U32(U32(beh, 0x5c), 0xc));
    }
    return chosen;
}

/* 0052a380 (ESI = behaviour, arg = range; RET 4) -> EAX: the spline to take (0 = none; the caller stores it at +0x38). The splines starting within `range` of the track 100 ahead (0050a650) and
 * pointing the same way (dot > 0.707), not busy, not remembered, on the navigate behaviour's side of the track (the side of the
 * nearest track node, 005381f0) are the candidates (10 at most; flag 2 ones are remembered); remembered splines out of range are
 * forgotten. */
rx_addr rx_override_spline_scan(rx_addr beh, float range) {
    RX_FRAME_ENTER(F, 0x60);   /* +0x00 ahead position, +0x10 direction, +0x20 candidates [10], +0x48 start, +0x50 nearest */
    for (uint32_t i = 0; i < 10; i++) SU32(F, 0x20 + 4 * i, 0);
    uint32_t found = 0;
    rx_track_look_ahead(U32(U32(beh, 8), 0x1c), F, F + 0x10, 100.0f /* 0x93005c */);
    const rx_addr v = beh2_vehicle(U32(beh, 8));
    for (uint32_t i = 0; i < U32(U32(U32(beh, 8), 0x10), 0x1d8); i++) {
        const rx_addr d = U32(beh, 8);
        const rx_addr spl = U32(U32(U32(d, 0x10), 0x130), 4 * i);
        if ((U8(spl, 0x118) & 2) == 0) continue;
        const uint32_t fl = spline_flags(spl, v);
        if ((fl & 1) == 0) continue;
        const int32_t at = remembered_at(beh, spl, fl);
        const rx_addr p = U32(spl, 0);
        const float pz = F32(p, 8), px = F32(p, 0);
        const float dz = pz - F32(F, 8), dx = px - F32(F, 0);
        const float r2 = range * range;
        if (r2 > dz * dz + dx * dx) {
            if (U32(spl, 0x14) != 0) continue;
            if (!(F32(p, 0x58) * F32(F, 0x18) + F32(p, 0x50) * F32(F, 0x10) > F32(0, 0x009cd99c) /* 0.707 */)) continue;
            if (at > -1) continue;
            SU32(F, 0x48, U32(d, 0x14));
            SU32(F, 0x4c, U32(d, 0x1c));
            float budget = r2 + 100.0f /* 0x93005c */;
            rx_track_nearest_node(F + 0x48, p, F + 0x50, &budget);
            const rx_addr nn = U32(F, 0x54);
            float ex = px - F32(nn, 0), ez = pz - F32(nn, 8);
            const float l2 = ex * ex + ez * ez;
            if (l2 != 0.0f /* 0x92e12c */) {
                const float inv = 1.0f /* 0x931f24 */ / sqrtf(l2);
                ex = inv * ex;
                ez = inv * ez;
            }
            const float side = F32(nn, 0x50) * ez - F32(nn, 0x58) * ex;
            const float sign = side >= 0.0f ? 1.0f /* 0x931f24 */ : F32(0, 0x009cd9e8) /* -1 */;
            if (rx_cvtt(-0.0f /* 0x9774a8 */ - sign) != I32(U32(beh, 0x5c), 0xc)) continue;
            SU32(F, 0x20 + 4 * found, spl);
            found++;
            if ((fl & 2) != 0 && U32(beh, 0x34) < 10) {
                SU32(beh, 0xc + 4 * U32(beh, 0x34), spl);
                SU32(beh, 0x34, U32(beh, 0x34) + 1);
            }
            if (found == 10) break;
        } else if (at > -1) {
            const uint32_t n = U32(beh, 0x34);
            if ((uint32_t)at != n - 1) SU32(beh, 0xc + 4 * (uint32_t)at, U32(beh, 8 + 4 * n));
            SU32(beh, 0x34, n - 1);
        }
    }
    const rx_addr chosen = choose_candidate(beh, F + 0x20, found);
    RX_FRAME_LEAVE(F, 0x60);
    return chosen;
}
RX_FUNCTION(0x0052a380, rx_override_spline_scan)

/* 0052a6d0 (EDI = behaviour) -> EAX: the same at the start of a spline: candidates are the splines starting within 15 of the rider or
 * of the current spline's start projected onto the rider's heading (0050deb0), pointing the same way, not busy, not remembered. */
rx_addr rx_override_spline_scan_start(rx_addr beh) {
    RX_FRAME_ENTER(F, 0x50);   /* +0x00 offset, +0x10 projection, +0x20 candidates [10] */
    for (uint32_t i = 0; i < 10; i++) SU32(F, 0x20 + 4 * i, 0);
    uint32_t found = 0;
    const rx_addr d = U32(beh, 8);
    const rx_addr v = U32(U32(d, 0x74), 0x1ac);
    const rx_addr p0 = U32(U32(beh, 0x38), 0);
    SF32(F, 0, F32(p0, 0) - F32(v, 0xc0));
    SF32(F, 4, F32(p0, 4) - F32(v, 0xc4));
    SF32(F, 8, F32(p0, 8) - F32(v, 0xc8));
    const rx_addr pr = beh2_project_on(v + 0x130, F, F + 0x10);
    const float qz = F32(v, 0xc8) + F32(pr, 8), qx = F32(pr, 0) + F32(v, 0xc0);
    const rx_addr list = U32(U32(d, 0x10), 0x130);
    for (uint32_t i = 0; i < U32(U32(U32(beh, 8), 0x10), 0x1d8); i++) {
        const rx_addr spl = U32(list, 4 * i);
        if ((U8(spl, 0x118) & 2) == 0) continue;
        const uint32_t fl = spline_flags(spl, v);
        if ((fl & 1) == 0) continue;
        const int32_t at = remembered_at(beh, spl, fl);
        const rx_addr p = U32(spl, 0);
        const float r = F32(0, 0x009d78c0) /* 225 */;
        const float az = F32(p, 8) - qz, ax = F32(p, 0) - qx;
        if (!(r > az * az + ax * ax)) {
            const float bz = F32(p, 8) - F32(v, 0xc8), bx = F32(p, 0) - F32(v, 0xc0);
            if (!(r > bz * bz + bx * bx)) continue;
        }
        if (U32(spl, 0x14) != 0) continue;
        if (!(F32(v, 0x138) * F32(p, 0x58) + F32(v, 0x130) * F32(p, 0x50) > F32(0, 0x009cd99c) /* 0.707 */)) continue;
        if (at > -1) continue;
        SU32(F, 0x20 + 4 * found, spl);
        found++;
        if (found == 10) break;
    }
    const rx_addr chosen = choose_candidate(beh, F + 0x20, found);
    RX_FRAME_LEAVE(F, 0x50);
    return chosen;
}
RX_FUNCTION(0x0052a6d0, rx_override_spline_scan_start)
