/* race_driver_ai.c -- the AI rider's per-frame decision (00510010): rules, the behaviours in priority order (the first that
 * claims the frame wins: +0x2ec its index), then the steering / throttle / brake / lean shaping that turns the behaviours' steering
 * accumulator (+0x60) and slow-down factor (+0x64) into the control outputs (+0x3c0 ..), the track-node reactions, the start
 * boost by place and the catch-up (lap pacing). (core/notes/race.md "AI decision") */
#include "race.h"
#include "race_extern.h"
#include "race_driver.h"
#include "race_activity.h"
#include "loop_vehicle.h"

static uint32_t airborne(rx_addr v) {                      /* vehicle vf43 (00466ae0: bike +0x8d98) */
    typedef uint32_t (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(v, 43);
    fn_t f = RX_FN(fn, fn_t);
    return f ? f(v) : race_thiscall(fn, v, 0, 0, 0, 0, 0);
}
static rx_addr vehicle_matrix_ptr14(rx_addr v) {            /* vehicle vf14 (00430240: &+0xf0) */
    typedef rx_addr (*fn_t)(rx_addr);
    rx_addr fn = race_vfn(v, 14);
    fn_t f = RX_FN(fn, fn_t);
    return f ? f(v) : race_thiscall(fn, v, 0, 0, 0, 0, 0);
}
static void rule_update(rx_addr r, float dt) {               /* rule vf5 (dt) */
    typedef void (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(r, 5);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(r, dt)); else race_thiscall(fn, r, 1, rx_fb(dt), 0, 0, 0);
}
static uint32_t behavior_update(rx_addr b, float dt) {       /* behaviour vf4 (dt) -> claims the frame */
    typedef uint32_t (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(b, 4);
    fn_t f = RX_FN(fn, fn_t);
    uint32_t r;
    if (f) RACE_CORE_CALL(1, r = f(b, dt)); else r = race_thiscall(fn, b, 1, rx_fb(dt), 0, 0, 0);
    return r;
}
static void behavior_debug_draw(rx_addr b) { race_thiscall(race_vfn(b, 8), b, 0, 0, 0, 0, 0); }   /* behaviour vf8 */
static float axis(float x) { if (x >= 1.0f) return 1.0f; if (F32(0, 0x9cd9e8) >= x) return F32(0, 0x9cd9e8); return x; }
static float unit(float x) { if (x >= 1.0f) return 1.0f; if (0.0f >= x) return 0.0f; return x; }
static uint32_t skill_index(rx_addr d) {                    /* the AI skill tables' index: tier + 4 * (3 * +0x328 + +0x32c) */
    return U32(U32(d, 0x74), 0x8f4) + 4 * (3 * U32(d, 0x328) + U32(d, 0x32c));
}
static void fix_progress_release(rx_addr fp) {               /* BehaviorFixForwardProgress: stop its recovery */
    SU32(fp, 0x18, 0); SU32(fp, 0x54, 0); SU32(fp, 0x58, 0);
    SF32(fp, 0x20, 0.0f);
    SF32(U32(fp, 8), 0x3cc, 0.0f);
}

/* One smoothed control channel of 0050b330: the output slot (controls + slot) eases toward its new value over the driver's time
 * constant (+tau): rate = min(tau, dt) / tau, value = (new - previous) x rate + previous; the rate goes to tau + 4, the value to
 * the previous slot (tau - 4) and the controls. The two pedal channels with a dead zone drop a value below 0x9cd950 to 0. */
static void ai_smooth_channel(rx_addr d, uint32_t tau_off, rx_addr slot, float dt, int dead_zone) {
    const float tau = F32(d, tau_off);
    float x = tau > dt ? dt : tau;
    const float inv = rx_divs(1.0f /* 0x931f24 */, tau);
    const float prev = F32(d, tau_off - 4);
    float v = rx_subs(F32(slot, 0), prev);
    x = rx_muls(x, inv);
    v = rx_adds(rx_muls(v, x), prev);
    SF32(d, tau_off + 4, x);
    SF32(d, tau_off - 4, v);
    SF32(slot, 0, v);
    if (dead_zone && v != 0.0f && F32(0, 0x9cd950) /* 9e-5 */ > v) {
        SF32(slot, 0, 0.0f);
        SF32(d, tau_off - 4, 0.0f);
    }
}

/* 0050b330 (ECX = driver, EDX = the control outputs (+0x3c0), xmm2 = dt; no result): the AI's control outputs eased by their time
 * constants (driver +0x338 .. +0x3b4, 0x14 apart: throttle, front brake, rear brake, lean fore / aft, lean side, ...). Not run for
 * the personality that takes the raw controls (+0x32c == 1). */
void rx_ai_control_smooth(rx_addr d, rx_addr c, float dt) {
    ai_smooth_channel(d, 0x338, c + 0x00, dt, 0);
    ai_smooth_channel(d, 0x34c, c + 0x04, dt, 1);
    ai_smooth_channel(d, 0x360, c + 0x08, dt, 1);
    ai_smooth_channel(d, 0x374, c + 0x1c, dt, 0);
    ai_smooth_channel(d, 0x388, c + 0x20, dt, 0);
    ai_smooth_channel(d, 0x39c, c + 0x14, dt, 0);
    ai_smooth_channel(d, 0x3b0, c + 0x18, dt, 0);
}
RX_FUNCTION(0x0050b330, rx_ai_control_smooth)

/* 00790100 (EAX = out buffer, xmm0 = seconds, EDI = with hundredths; cdecl, no stack arguments; no useful result): a lap time as
 * text, "MM:SS" or "MM:SS.cc" (the negative with a leading "-"); no time (0 or FLT_MAX) "--:--.--", above 5999.99 "99:99(.99)",
 * below -5999.99 "-99:99(.99)" (8 characters, the strncpy of the constant, then a 0). The hundredths are the time x 100 rounded
 * away from zero (x + copysign(0.5, x), truncated: the x87 add at 53 bits, _ftol2_sse). */
void rx_lap_time_text(rx_addr out, float t, uint32_t with_hundredths) {
    RACE_FRAME_PLAIN(F, 8, 3);
    rx_addr src = 0;
    if (t == 0.0f || t == rx_bf(0x7f7fffffu) /* 0x977494 */) src = 0x0096f32c;                     /* "--:--.--" */
    else if (t > 5999.99f /* 0x977634 */) src = with_hundredths ? 0x0096f338 : 0x0096f344;     /* "99:99.99" / "99:99" */
    else if (-5999.99f /* 0x977630 */ > t) src = with_hundredths ? 0x0096f34c : 0x0096f358;     /* "-99:99.99" / "-99:99" */
    if (src != 0) {
        RACE_CORE_CALL(3, crt_strncpy(out, src, 8));
        SU8(out, 8, 0);
        RACE_FRAME_END(F);
        return;
    }
    const float x = rx_muls(t, 100.0f /* 0x93005c */);
    int32_t n = rx_cvtt(copysign(0.5 /* 0x9774e8 */, (double)x) + (double)x);
    n = rx_cvtt((double)(float)n);                                                             /* cvtsi2ss, cvttss2si */
    if (n < 0) { SU8(out, 0, '-'); out++; n = -n; }
    const int32_t c = n / 100;
    SU8(out, 0, '0' + n / 60000);
    SU8(out, 1, '0' + n / 6000 % 10);
    SU8(out, 2, ':');
    SU8(out, 3, '0' + c % 60 / 10);
    SU8(out, 4, '0' + c % 10);
    if (with_hundredths == 0) { SU8(out, 5, 0); RACE_FRAME_END(F); return; }
    SU8(out, 5, '.');
    SU8(out, 6, '0' + (n - c * 100) / 10);
    SU8(out, 7, '0' + (n % 10));
    SU8(out, 8, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00790100, rx_lap_time_text)

/* 00509940 (ESI = driver; no result): the debug view of an AI rider's tuning (developer flag 0xae1b90 and a player with +0x1cc):
 * the tuning set names, the 17 parameter rows of the personality (the range of the row 3 through 0051dd70) and the two rings of 10
 * recent samples are all printed through the empty hook 007e3be0, so the only calls with an effect are the range query's (its out
 * pair is on the stack). Frame: sub esp, 0x10; push ebx, ebp, edi (the range out at +0x10). */
void rx_ai_debug_overlay(rx_addr d) {
    rx_addr player = U32(d, 0x74);
    if (U32(player, 0x1cc) == 0 || U32(0, 0xae1b90) == 0) return;
    RACE_FRAME_PLAIN(F, 0x10, 3);
    for (uint32_t i = 0; i < 0x11; i++) {
        if (U32(0, 0x00a89dc0 + (U32(d, 0xee8) * 0x11 + i) * 0xbc) != 0 && i == 3) RACE_CORE_CALL(1, rx_ai_param_range_dd70(d, F + 0x10));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00509940, rx_ai_debug_overlay)

/* 00510010 (args: driver, dt; RET 8): see the file comment. The AI personality / difficulty enter through the skill tables
 * (0x9e4838 heading tolerance, 0x9e43b8 steering bias, 0x9e4478 lean gain, all by skill_index) and the AI parameters. */
void rx_ai_race_update(rx_addr d, float dt) {
    RACE_ORIG_FRAME_ENTER(SF, 0x194, 3);   /* the behaviours' callees keep stack addresses; +0x30 the predicted point */
    SF32(d, 0x60, 0.0f); SF32(d, 0x64, 0.0f); SF32(d, 0x68, 0.0f);
    SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f); SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
    SF32(d, 0x3dc, 0.0f); SF32(d, 0x3e0, 0.0f); SF32(d, 0x3d4, 0.0f); SF32(d, 0x3d8, 0.0f);
    SU32(d, 0x3d0, 0);
    if (U32(d, 0xeec) == 0 || U32(0, 0xae1ce8) == 0) { RACE_ORIG_FRAME_LEAVE(SF); return; }
    const int dbg = U32(0, 0xae1b90) != 0;
#define P() U32(d, 0x74)
#define V() U32(U32(d, 0x74), 0x1ac)
    if (U32(V(), 0x280) != 0) rx_ai_controls_off(d);
    SU32(d, 0xf18, U32(d, 0xf14));
    SU32(d, 0xf14, airborne(V()));
    SF32(d, 0x3c0, 1.0f);
    SF32(d, 0x64, 0.0f);
    for (uint32_t i = 0; i < U32(d, 0xee4); i++) rule_update(U32(d, 0xa0 + 4 * i), dt);
    /* (debug hook 007e3be0 is an empty function) */
    SU32(d, 0x2ec, 0xffffffffu);
    uint32_t i = 0;
    for (; i < U32(d, 0xee0); i++) {
        /* the original formats "Behavior %d" (the type, vf5) into a stack buffer here for a debug view: no effect */
        const uint32_t claimed = behavior_update(U32(d, 0xe8 + 4 * i), dt);
        if (U32(P(), 0x1cc) != 0 && U32(0, 0xae1b90) != 0) behavior_debug_draw(U32(d, 0xe8 + 4 * i));
        if (claimed) { SU32(d, 0x2ec, i); break; }
    }
    for (uint32_t j = i + 1; j < U32(d, 0xee0); j++)
        if (U32(P(), 0x1cc) != 0 && U32(0, 0xae1b90) != 0) behavior_debug_draw(U32(d, 0xe8 + 4 * j));
    rx_addr fp = rx_driver_fix_progress_behavior(d);
    int heading_ok = 0;                 /* the original's [esp + 0x0c] */
    float speed_cap = 0.0f;             /* [esp + 0x10] */
    uint32_t on_track = 1;              /* [esp + 0x18] */
    rx_addr pen = rx_driver_offtrack_penalty_rule(d);
    int off = 0;
    if (pen != 0) {
        on_track = U32(pen, 0x10);
        SF32(d, 0x10c8, F32(pen, 0x1c));
        if (on_track == 0) off = 1;
    }
    rx_addr v = V();
    if (!off && U32(v, 0x280) != 0) off = 1;
    if (!off && F32(P(), 0x116c) > 0.0f) off = 1;
    if (!off) {
        if (U32(fp, 0x18) != 1) {
            rx_addr n = U32(d, 0x18);
            const float h = (F32(v, 0x138) * F32(n, 0x58) + F32(v, 0x134) * F32(n, 0x54)) + F32(v, 0x130) * F32(n, 0x50);
            if (h > 0.0f && F32(0, 0x9e4838 + 4 * skill_index(d)) > h) heading_ok = 1;
            float nx = F32(v, 0x160), ny = 0.0f, nz = F32(v, 0x168);
            const float l2 = nz * nz + nx * nx;
            if (l2 > 0.0f) {
                const float inv = 1.0f / sqrtf(l2);
                nx = inv * F32(v, 0x160); ny = inv * 0.0f; nz = nz * inv;
            }
            if ((F32(n, 0x58) * nz + F32(n, 0x54) * ny) + nx * F32(n, 0x50) > 0.0f) {
                const float m = (F32(n, 0x58) * (F32(v, 0xc8) - F32(d, 0x88)) + F32(n, 0x54) * (F32(v, 0xc4) - F32(d, 0x84)))
                                + F32(n, 0x50) * (F32(v, 0xc0) - F32(d, 0x80));
                if (m > 0.0f) { SF32(d, 0x80, F32(v, 0xc0)); SF32(d, 0x84, F32(v, 0xc4)); SF32(d, 0x88, F32(v, 0xc8)); }
            }
        }
    } else {
        const float arrow = F32(d, 0x10d0);
        if (!(arrow == 0.0f) && F32(d, 0x130) > arrow) {
            rx_addr p2 = rx_driver_offtrack_penalty_rule(d);
            if (p2 != 0) SU8(p2, 0xd8, 0);
            if (U32(V(), 0x280) == 0) {
                rx_event_driver_int(U32(U32(P(), 0x94), 0x290), 0x0093ef24 /* "Off Track" */, d, on_track);
                rx_event_driver(U32(U32(P(), 0x94), 0x290), 0x0093ef30 /* "Show Arrow" */, d);
                SF32(d, 0x10d0, 0.0f);
            }
        }
        v = V();
        float nx = F32(v, 0x160), ny = F32(v, 0x164), nz = F32(v, 0x168);
        const float l2 = (ny * ny + nx * nx) + nz * nz;
        if (l2 > 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            nx = inv * nx; nz = nz * inv; ny = ny * inv;
        }
        if ((F32(v, 0x138) * nz + F32(v, 0x134) * ny) + nx * F32(v, 0x130) > 0.0f) speed_cap = F32(0, 0x9d78a4);   /* 58.67 */
    }
    /* steering: the behaviours' accumulator, clamped */
    float s = F32(d, 0x60);
    if (F32(0, 0x9cd9e8) > s) s = F32(0, 0x9cd9e8);
    else if (s > 1.0f) s = 1.0f;
    SF32(d, 0x3dc, s);
    v = V();
    const int atv = U32(v, 0x194) == 1;
    if (F32(0, 0x9d3bfc) /* 0.05 */ > F32(P(), 0x10bc) || airborne(v) == 0) {
        const float limit = atv ? F32(0, 0x9d78a0) /* 0.65 */ : 0.5f /* 0x9322cc */;
        const float st = F32(d, 0x3dc), a = fabsf(st);
        const float held = a > limit ? F32(0, 0xd92ee4) + dt : 0.0f;   /* a global hard-steer timer (0xd92ee4) */
        v = V();
        const uint32_t k = U32(v, 0x198) + 4 * U32(v, 0x194);
        const float w = F32(U32(d, 0x18), 0x468 + 4 * k);
        SF32(0, 0xd92ee4, held);
        if (!(w == 0.0f)) {
            SF32(d, 0x3d4, w * 0.01f /* 0x9322ac */ * st);
        } else if (held > (atv ? 0.25f /* 0x92ff84 */ : 0.2f /* 0x92e388 */)) {
            if (15.0f /* 0x9322d0 */ > F32(V(), 0x150) && a == 1.0f) SF32(d, 0x3d4, st);
            else SF32(d, 0x3d4, (atv ? 0.75f /* 0x935aa8 */ : 0.6f /* 0x931da8 */) * st);
        }
    }
    /* braking from the behaviours' slow-down factor */
    if (airborne(V()) == 0 || 0.5f > F32(P(), 0x10bc)) {
        const float b = F32(d, 0x64);
        if (!(b == 0.0f)) {
            if (b >= 0.5f) { SF32(d, 0x3c4, b); SF32(d, 0x3c8, b); }
            else if (!(b < 0.2f /* 0x92e388 */) && b == b) { SF32(d, 0x3c4, b * 0.5f); SF32(d, 0x3c8, b * 0.5f); }
            SF32(d, 0x3c0, 0.0f);
        }
    }
    RACE_CORE_CALL(2, rx_ai_track_node_reactions(d, dt));
    int side;
    RACE_CORE_CALL(1, side = rx_driver_predict_side(d, SF + 0x30, ((((SF - 8)) - 4) & ~15u) - 0x48 - 8));
    rx_addr al = rx_driver_air_leveling_behavior(d);
    /* the lean bias */
    int lean_done = 0, skip_rest = 0;
    if (F32(d, 0x1100) > F32(d, 0x130)) {
        const float bias = -0.0f /* 0x9774a8 */ - F32(0, 0x9e43b8 + 4 * skill_index(d));
        SF32(d, 0x318, bias);
        rx_addr p = P();
        const uint32_t row = U32(d, 0xee8) * 0xc7cu;
        const float g = I32(p, 0x8fc) != -1 ? F32(0, row + 8 * U32(p, 0x8fc) + 0xa89ee0)
                                            : F32(0, row + 8 * (U32(p, 0x8f4) * 3 + U32(p, 0x8f8)) + 0xa89e80);
        SF32(d, 0x318, g * bias * 0.01f /* 0x9322ac */);
        if (side == 0) { SF32(d, 0x1100, 0.0f); SF32(d, 0x318, 0.0f); }
        if (!(F32(d, 0x3c4) == 0.0f)) { SF32(d, 0x1100, 0.0f); SF32(d, 0x318, 0.0f); }
        lean_done = U32(d, 0x32c) == 2;   /* skill 2: no lean */
    } else {
        const float s320 = F32(d, 0x320);
        if (!(s320 == 0.0f) && on_track != 0) {
            if (U32(V(), 0x280) == 0 && U32(d, 0x32c) != 2) { SF32(d, 0x3e0, s320); SF32(d, 0x3d8, s320); }
            lean_done = 1;
        } else {
            const float s318 = F32(d, 0x318);
            lean_done = 1;
            if (!(s318 == 0.0f) && on_track != 0 && (0.1f /* 0x931d94 */ > fabsf(F32(d, 0x3dc)) || U32(d, 0x32c) != 2)) {
                v = V();
                if (F32(v, 0x150) > F32(0, 0x9cd954) /* 44 */ && U32(v, 0x280) == 0) {
                    if (airborne(v) == 0) lean_done = 0;
                    else if (al == 0) skip_rest = 1;
                    else if (F32(0, 0x9cd958) /* 0.85 */ > F32(al, 0x54)) lean_done = 0;
                }
            }
        }
    }
    if (!skip_rest && !lean_done) { SF32(d, 0x3e0, F32(d, 0x318)); SF32(d, 0x3d8, F32(d, 0x318)); }
    /* in the air: follow the AirLeveling behaviour */
    if (!skip_rest && al != 0) {
        if (U32(d, 0x32c) == 2) {
            if (airborne(V()) != 0) {
                const float t = F32(d, 0x3c0);
                const float x = F32(d, 0x3d8) - F32(al, 0x40);
                SF32(d, 0x3d8, x);
                if (!(t == 1.0f)) {
                    if (0.0f > x) { SF32(d, 0x3c4, x * -0.125f /* 0x9776f4 */); SF32(d, 0x3c8, x * -0.125f); }
                    else if (x > 0.0f) { SF32(d, 0x3c0, x * 0.5f); SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f); }
                }
            }
        } else if (airborne(V()) != 0) {
            v = V();
            if (F32(al, 0x58) > F32(d, 0x130)) {
                const float c = (F32(al, 0x28) * F32(v, 0x138) + F32(al, 0x24) * F32(v, 0x134)) + F32(al, 0x20) * F32(v, 0x130);
                if (F32(0, 0x9cd95c) /* -0.2 */ > c) {
                    SF32(d, 0x3e0, F32(0, 0x9cd9e8)); SF32(d, 0x3d8, F32(0, 0x9cd9e8));
                    SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
                    SF32(d, 0x3c0, 1.0f);
                }
            }
            v = V();
            if (0.0f > F32(v, 0x164) || 1.0f > F32(al, 0x50)) {
                SF32(d, 0x3d8, F32(al, 0x40) + F32(d, 0x3d8));
                SF32(d, 0x3dc, F32(al, 0x44) + F32(d, 0x3dc));
                if (F32(P(), 0x10bc) > 0.5f) {
                    const float x = F32(d, 0x3d8);
                    if (x >= 0.75f /* 0x935aa8 */) {
                        SF32(d, 0x3c4, x * 0.25f /* 0x92ff84 */);
                        SF32(d, 0x3c8, F32(d, 0x3d8) * 0.25f);
                        SF32(d, 0x3c0, 0.0f);
                    } else if (F32(0, 0x9cd9b0) /* -0.75 */ >= x) {
                        const float y = F32(d, 0x3d8) * 0.25f;
                        SF32(d, 0x3c4, 0.0f); SF32(d, 0x3c8, 0.0f);
                        SF32(d, 0x3c0, fabsf(y));
                    }
                }
            } else {
                SF32(d, 0x3dc, F32(al, 0x44) + F32(d, 0x3dc));
                const float q = (F32(v, 0x148) + F32(v, 0x140)) * 0.0f + F32(v, 0x144);
                if (F32(0, 0x9cd960) /* 0.45 */ > q) { SF32(d, 0x3c4, 0.5f); SF32(d, 0x3c8, 0.5f); }
            }
        } else if (U32(d, 0x32c) != 2) {
            float a = fabsf(F32(d, 0x3dc));
            if (!(1.0f > a)) a = 1.0f;
            const float w = F32(0, 0x9e4478 + 4 * skill_index(d));
            SF32(d, 0x3e0, w * a + F32(d, 0x3e0));
            SF32(d, 0x3d8, F32(0, 0x9e4478 + 4 * skill_index(d)) * a + F32(d, 0x3d8));
        }
    }
    rx_driver_update_highlight(d);
    v = V();
    if ((U32(v, 0x194) == 1 || U32(v, 0x194) == 0) && airborne(v) == 0 && U32(d, 0xf18) != 0 &&
        F32(P(), 0x10bc) > 0.3f /* 0x931da4 */)
        (void)race_rand();   /* a landing draws a random number (unused) */
    /* a rider beside: no steering into it, brake instead (unless it is on the predicted side) */
    const int toward = 0.0f > F32(d, 0x3dc) ? -1 : 1;
    int near;
    RACE_CORE_CALL(3, near = rx_race_rider_near(d, toward, 7.0f /* 0x930004 */, 5.0f /* 0x935890 */));
    if (near) {
        if (side != I32(U32(d, 0x1110), 0xc)) {
            const float b = fabsf(F32(d, 0x3dc)) * 2.0f /* 0x92e330 */ + F32(d, 0x3c8);
            SF32(d, 0x3c8, b); SF32(d, 0x3c4, b);
        }
        SF32(d, 0x3dc, 0.0f); SF32(d, 0x3d4, 0.0f);
    }
    v = V();
    if (U32(v, 0x194) == 7 && airborne(v) != 0) {
        rx_addr M = vehicle_matrix_ptr14(V());
        const float q = (F32(M, 8) + F32(M, 0)) * 0.0f + F32(M, 4);
        if (0.5f > q) SF32(d, 0x3dc, q);
    }
    /* throttle caps: off-track speed cap, heading tolerance */
    if (fp != 0 && U32(fp, 0x18) != 1) {
        int capped = 0;
        if (!(speed_cap == 0.0f)) {
            if (airborne(V()) == 0) {
                if (F32(V(), 0x150) > speed_cap) SF32(d, 0x3c0, 0.0f);
                else if (F32(d, 0x64) == 0.0f) { SF32(d, 0x3c0, F32(0, 0x9cc8a8) /* 0.8 */); SF32(d, 0x3c8, 0.0f); SF32(d, 0x3c4, 0.0f); }
                capped = 1;
            }
        }
        if (!capped) {
            const float lim = heading_ok == 0 ? 1.0f : 0.5f;
            const float t = F32(d, 0x3c0);
            float r = t >= lim ? lim : t;
            if (!(0.0f < r) && r == r) r = 0.0f;
            SF32(d, 0x3c0, r);
        }
    }
    /* catch-up: the lap pacing's throttle */
    rx_addr lp = rx_driver_lap_pacing_behavior(d);
    if (lp != 0 && on_track != 0 && F32(d, 0x64) == 0.0f) {
        const float g = F32(lp, 0x18);
        if (!(g == 1.0f) && airborne(V()) == 0) {
            v = V();
            if (U32(v, 0x194) == 1 || U32(v, 0x194) == 0) SF32(d, 0x3c0, g);
            else if (F32(v, 0x150) > F32(0, 0x9cd964) /* 73.3 */) {
                SF32(d, 0x3c0, g); SF32(d, 0x3c8, F32(0, 0x9d3bfc)); SF32(d, 0x3c4, F32(0, 0x9d3bfc));
            }
        }
    }
    /* the stuck recovery ends once riding along the track again */
    if (fp != 0 && !(F32(d, 0x3c4) == 0.0f)) {
        v = V();
        float hx = F32(v, 0x130), hy = 0.0f, hz = F32(v, 0x138);
        const float l2 = hx * hx + hz * hz;
        if (l2 > 0.0f) {
            const float inv = 1.0f / sqrtf(l2);
            hx = inv * F32(v, 0x130); hy = inv * 0.0f; hz = hz * inv;
        }
        rx_addr n = U32(d, 0x1c);
        if ((F32(n, 0x58) * hz + F32(n, 0x54) * hy) + F32(n, 0x50) * hx > 0.5f && F32(d, 0x130) > F32(fp, 0x20) &&
            2.0f > fabsf(F32(V(), 0x150)))
            fix_progress_release(fp);
    }
    rx_race_limit_throttle_in_turns(d, d + 0x3c0);
    if ((U8(U32(d, 0x14), 0x118) & 4) == 0 && F32(U32(P(), 0x94), 0x294) > 10.0f /* 0x92fd88 */) {
        rx_addr sr = rx_driver_suck_racing_behavior(d);
        if (sr != 0 && U32(sr, 0x20) != 0) { SF32(d, 0x3dc, 0.0f); SF32(d, 0x3d4, 0.0f); }
        rx_addr lp2 = rx_driver_lap_pacing_behavior(d);
        if (lp2 != 0 && U32(lp2, 0x2c) == 0) {
            /* RavenVehicle_SetTuning (the pacing's tuning): EDI = the vehicle, ESI = the tuning, the default on the stack */
            RACE_CORE_CALL(1, rx_vehicle_set_tuning(U32(U32(U32(lp2, 8), 0x74), 0x1ac), lp2 + 0x40, lp2 + 0x40));
            SU32(lp2, 0x2c, 1);
        }
    }
    /* the start: throttle by place for the first seconds (more for the riders behind) */
    const int sx = U32(U32(P(), 0x94), 0x23c) == 0x11;
    const float lo = sx ? F32(0, 0xd92ee8) : 1.5f /* 0x9322a4 */, hi = sx ? F32(0, 0x9cd968) /* 13 */ : 5.0f /* 0x935890 */;
    const float rc = F32(d, 0x134);
    if (rc > lo && F32(V(), 0x150) > F32(0, 0x9cd96c) /* 34.69 */ && ((U8(U32(d, 0x14), 0x118) & 4) != 0 || hi > rc)) {
        if (fp != 0) fix_progress_release(fp);
        double u = (double)(int32_t)(12 - U32(d, 0x78));
        if (u < 0.0) u += 4294967296.0;                                 /* x87 at 53 bits = double arithmetic here */
        u = u * (double)F32(0, 0x9cd970) /* 0.9 */;
        u = u / (double)I32(U32(U32(d, 0x74), 0x94), 0x26c);
        u = u + (double)0.1f /* 0x931d94 */;
        u = u * (double)F32(d, 0x3c0);
        SF32(d, 0x3c0, (float)u);
    }
    if (U32(d, 0x32c) == 2) SF32(d, 0x3d8, 0.0f);
    SF32(d, 0x3dc, axis(F32(d, 0x3dc)));
    SF32(d, 0x3e0, axis(F32(d, 0x3e0)));
    SF32(d, 0x3d4, axis(F32(d, 0x3d4)));
    SF32(d, 0x3d8, axis(F32(d, 0x3d8)));
    SF32(d, 0x3c0, unit(F32(d, 0x3c0)));
    SF32(d, 0x3c4, unit(F32(d, 0x3c4)));
    SF32(d, 0x3c8, unit(F32(d, 0x3c8)));
    if (U32(d, 0x32c) != 1) {
        RACE_CORE_CALL(0, rx_ai_control_smooth(d, d + 0x3c0, dt));
    }
    if (U32(P(), 0x1cc) != 0 && U32(0, 0xae1b90) != 0) {
        RACE_CORE_CALL(0, rx_free_ride_debug_draw(d));   /* ESI = the driver */
    }
    if (U32(P(), 0x1cc) != 0) {
        /* the debug view's lap times: formatted into a stack buffer (FUN_00790100) for the empty print hook 007e3be0 */
        for (int32_t lap = 0; lap < I32(d, 0xf1c); lap++) {
            RACE_CORE_CALL(0, rx_lap_time_text(SF + 0x90, F32(d, 0xf24 + 4 * (uint32_t)lap), 1));
        }
    }
    (void)dbg;
#undef P
#undef V
    RACE_ORIG_FRAME_LEAVE(SF);
}
RX_FUNCTION(0x00510010, rx_ai_race_update)
