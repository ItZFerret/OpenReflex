/* build_activity_pre.c -- Activity::vf37 005589c0 (ECX = the activity; the old state, RET 4): the Pre state's enter: the world's physics bodies back to their start
 * transforms (00770d50), the controls enabled (0055a1e0), the world's camera marked (+0x21bec = 0), the activity's countdown clock +0x2a8 (1.0 for the free ride,
 * else 0.0), the session's timer event queued with the session's seconds + 1.0 (006e4a90 -> 00707320: "NetworkTimerEvents" {1, seconds}; its handler sets +0x2a8),
 * the activity's entities' start states (00557fe0), the scene's view records reset (+0xfe4), the audio 3D scale 1.0 (00805200), the damage behaviours' targets
 * released (0047bb40 / 0047bce0) and the rules popup question (005a4bd0).
 * Not offline paths (abort naming them): the networked session's timing (00585b20 ...), the replay recorder's frame (006df530), a session result (+0xfe8:
 * 005584f0), the series events (kinds 0x1f .. 0x21: the series manager). UI / audio boundary (core/plan/QUEUE.md "chain"): the rules popup (005a4bd0 / 005a4ec0: the
 * UI property "RulesType" and the popup event; owned API successor preserves selection/hold through the explicit Rules host) and the audio 3D settings (00805200: FMOD). */
#include "career_activity.h"
#include "rider_cap.h"
#include "world_veh_damage.h"
#ifndef RX_ORACLE
#include "career_rules.h"
#endif
#include "fx.h"
#include "build_activity_int.h"
#include "build_activity.h"
#include "build_activity_pre.h"
#include "build_activity_freeride_run.h"
#include "build_alloc.h"
#include "build_rider_util.h"
#include "world_veh.h"
#include "world_wreck.h"
#include "phys_body.h"
#include "phys_coll_misc.h"
#include "race_activity.h"

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* 00770d50 (ECX = the physics world): each body (+0x19d8 + 4 k, count +0x19d4; last to first) back to its start transform: +0x80 = -1, +0x84 = 0, +0x40 .. +0x5f =
 * +0x60 .. +0x7f, the shape's (+0x34) vf30 (+0x78) of (+0x40, +0x50), +0xd0 = +0xd1, +0xb4 = +0xb8, its broadphase box (00842b000 for its index in the body
 * records +0x1994); then the count 0 and the records vector (+0xcc00) erased to its end */
void rx_physics_world_reset_bodies(rx_addr P) {
    ACT_FRAME_EBP(L, 16, 0x14, 3);
    const int32_t n = I32(P, 0x19d4);
    if (n - 1 > -1) {
        const rx_addr rec = P + 0x1994;
        rx_addr it = P + 0x19d8 + 4u * (uint32_t)(n - 1);
        for (int32_t left = n; left != 0; left--, it -= 4) {
            const rx_addr b = U32(it, 0);
            SU32(b, 0x80, 0xffffffffu); SU32(b, 0x84, 0);
            rx_st4(b + 0x40, rx_ld4(b + 0x60));
            rx_st4(b + 0x50, rx_ld4(b + 0x70));
            { const uint32_t a[2] = {b + 0x40, b + 0x50}; act_vcall(U32(b, 0x34), 0x78, 2, a); }
            SU8(b, 0xd0, U8(b, 0xd1));
            SU32(b, 0xb4, U32(b, 0xb8));
            if (((int32_t)(U32(rec, 0x10) - U32(rec, 0xc)) / 0xe0) == 0) crt_invalid_parameter_noinfo();
            const int32_t idx = (int32_t)(b - U32(rec, 0xc)) / 0xe0;
            ACT_CALL(0, phys_body_update_grid(rec, idx));               /* 0042b000 (ESI = the records, ECX = the index) */
        }
    }
    const rx_addr v = P + 0xcc00;
    SU32(P, 0x19d4, 0);
    const uint32_t last = U32(v, 0x10);
    SU32(v, 0x18, 0);
    if (U32(v, 0xc) > last) crt_invalid_parameter_noinfo();
    const uint32_t first = U32(v, 0xc);
    if (first > U32(v, 0x10)) crt_invalid_parameter_noinfo();
    ACT_CALL(5, (void)phys_vector_erase(v, L + 0x18, first, last));    /* 007cf450 (ESI = the vector) */
    ACT_END(L);
}
RX_FUNCTION(0x00770d50, rx_physics_world_reset_bodies)

/* 00707320 (stdcall channel, kind, float value; RET 0xc): under the channel's lock (+0x28) an event record {+0 / +4 the event named "NetworkTimerEvents" found in the
 * channel's events (+8: StringMap_Find's iterator), +8 an 8-byte payload {kind, value} (_aligned_malloc(8, 16) under the allocation lock), +0xc 8} appended to the
 * channel's queue (+0x40) */
void rx_session_queue_timer_event(rx_addr ch, uint32_t kind, float value) {
    ACT_FRAME_EBP(L, 8, 0x2c, 3);
    rr_os_cs_enter(ch + 0x28);
    const rx_addr rec = rx_reflex_aligned_alloc(0x10);
    if (rec) { SU32(rec, 0, 0); SU32(rec, 4, 0); }
    SU32(L, 0xc, rec);
    const rx_addr str = L + 0x1c, out = L + 0x14;
    crt_str_ctor_cstr(str, 0x00941d1c /* "NetworkTimerEvents" */);
    rx_addr it;
    ACT_CALLR(it, 2, rx_world_string_map_find(ch + 8, out, str));
    const uint32_t i0 = U32(it, 0), i1 = U32(it, 4);
    crt_str_dtor(str);
    SU32(rec, 0, i0); SU32(rec, 4, i1);
    const int locked = U8(0, 0x00da67d8) != 0;
    if (locked) rr_os_cs_enter(0x00d61704);
    const rx_addr pl = rx_crt_aligned_malloc(8, 0x10);
    if (locked) rr_os_cs_leave(0x00d61704);
    SU32(rec, 8, pl); SU32(rec, 0xc, 8);
    SU32(pl, 0, kind); SF32(pl, 4, value);
    ACT_CALL(1, rx_event_handler_list_push_back(ch + 0x40, rec));      /* 005b9ff0 */
    rr_os_cs_leave(ch + 0x28);
    ACT_END(L);
}
RX_FUNCTION(0x00707320, rx_session_queue_timer_event)

/* 006e4a90 (ESI = the session [0xda6cb0]; stack float seconds): offline (+0x1c4c == 0, or the mode +0x1b0 == 1) the timer event {1, seconds} queued on the session's
 * channel [0xda6ca0]; a networked session sends it through the Quazal host (not offline) */
void rx_session_pre_timer(rx_addr s, float seconds) {
    ACT_FRAME_EBP(L, 8, 0x14, 1);
    if (U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1) act_unreached(0x006e4ab4, "the networked session's timer event");
    session_static();
    ACT_CALL(3, rx_session_queue_timer_event(U32(0, 0x00da6ca0), 1, seconds));
    ACT_END(L);
}
RX_FUNCTION(0x006e4a90, rx_session_pre_timer)

/* 00559ac0: each view (+0x19b8 + 4 k, count +0x19d0) of the engine's current scene: its input record (+0x70): +0 = 0, +0x40 = 0, +0x30 / +0x34 / +0x38 = 0, +0x3c = 1.0,
 * +0x1c = 1000000.0 (0x49742400), +0x14 = 1, +0x20 = 0, +0x18 = 0, +0x28 = 0, +0x2c = 1.0 */
void rx_scene_views_reset(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    rx_addr sc = U32(ctx, 0x1c + 4 * U32(ctx, 4));
    for (uint32_t k = 0; k < U32(sc, 0x19d0); k++) {
        const rx_addr r = U32(U32(sc, 0x19b8 + 4 * k), 0x70);
        SU32(r, 0, 0); SU32(r, 0x40, 0); SU32(r, 0x30, 0); SU32(r, 0x34, 0); SU32(r, 0x38, 0); SU32(r, 0x3c, 0x3f800000u);
        SU32(r, 0x1c, 0x49742400u); SU32(r, 0x14, 1); SU32(r, 0x20, 0); SU32(r, 0x18, 0); SU32(r, 0x28, 0); SU32(r, 0x2c, 0x3f800000u);
        sc = U32(ctx, 0x1c + 4 * U32(ctx, 4));
    }
}
RX_FUNCTION(0x00559ac0, rx_scene_views_reset)

/* 0047bce0 (EAX = the damage behaviour manager 0x00db1410): the twelve behaviours (+0x10 + 0x410 k) with a target (+0x3fc) released (0047a880) */
void rx_damage_manager_release_targets(rx_addr m) {
    ACT_FRAME(L, 0, 2);
    for (uint32_t k = 0; k < rx_dmg_n(); k++) {   /* (w-rc-core) + the side entries made */
        const rx_addr b = rx_dmg_entry(m, k);
        if (U32(b, 0x3fc) != 0) ACT_CALL(1, rx_world_damage_reset_parts(b));   /* 0047a880 */
    }
    ACT_END(L);
}
RX_FUNCTION(0x0047bce0, rx_damage_manager_release_targets)

/* Activity::vf37 005589c0 (ECX = the activity) */
void rx_activity_vf37(rx_addr A) {
    ACT_FRAME(L, 0x2c, 3);                                              /* sub esp, 0x2c; push ebx / ebp / edi (esi is pushed at 00558a78, popped after 006e4a90) */
    const rx_addr w = U32(A, 0x234);
    ACT_CALL(0, rx_physics_world_reset_bodies(U32(w, 0x20134)));        /* 00770d50 */
    ACT_CALL(1, rx_activity_enable_controls(0, A));                     /* 0055a1e0 (EAX = 0) */
    if (w + 0x218f0 != 0) SU8(w, 0x21bec, 0);
    SF32(A, 0x2a8, U32(A, 0x23c) == 8 ? F32(0, 0x00931f24) : 0.0f);
    session_static();
    float seconds;
    ACT_CALLR(seconds, 0, rx_world_timer_seconds(0x00da6ca0));          /* 006def20 */
    session_static();
    const rx_addr s = U32(0, 0x00da6cb0);
    if (U32(s, 0x1c4c) != 0 && U32(s, 0x1b0) != 1) act_unreached(0x00558a91, "the networked session's players (00585b20)");
    seconds = seconds + F32(0, 0x00931f24);
    session_static();
    { ACT_FRAME(E, 0, 1); ACT_CALL(1, rx_session_pre_timer(s, seconds)); ACT_END(E); }   /* 006e4a90 (ESI = the session; esi pushed) */
    session_static();
    if (U32(U32(0, 0x00da6cb0), 0x1c48) != 0 && U32(A, 0x238) == 1) act_unreached(0x00558b9d, "the replay recorder's frame (006df530)");
    ACT_CALL(1, rx_activity_entities_start(A));                         /* 00557fe0 */
    if (U32(A, 0xfe8) != 0) act_unreached(0x00558bd0, "a session result's entities (005584f0)");
    const uint32_t kind = U32(A, 0x23c);
    SU32(A, 0x230, 0);
    if (kind == 0x1f || kind == 0x20 || kind == 0x21) { rx_addr manager; ACT_CALLR(manager, 0, rx_fx_series_manager()); ACT_CALL(1, (void)rx_career_current_event_type(manager)); }
    if (U32(A, 0xfe4) != 0) ACT_CALL(0, rx_scene_views_reset());        /* 00559ac0 */
    SU32(0, 0x00a957d8, 0);
    { const uint32_t a[1] = {0x3f800000u}; act_ui_call(0x00805200, RX_CDECL, 0, 1, a); }   /* the audio 3D scale 1.0: FMOD */
    SU32(0, 0x00a941b8, U32(w, 0x20018) == 0);
    rx_addr mgr;
    ACT_CALLR(mgr, 0, rx_world_damage_manager_instance());              /* 0047bb40 */
    ACT_CALL(0, rx_damage_manager_release_targets(mgr));                /* 0047bce0 */
    if (U32(A, 0xfe8) != 0) SF32(A, 0x2a4, F32(A, 0x2ac) + F32(0, 0x00931f24));
    SU32(A, 0xff068, 0);
    {   /* the rules popup (005a4bd0: UI properties; EDX = the UI manager [0xa93fcc]) */
        rx_regs in, out; memset(&in, 0, sizeof in); in.r[1] = A; in.r[2] = U32(0, 0x00a93fcc);
        memset(&out, 0, sizeof out);
#ifndef RX_ORACLE
        /* Only an actually configured career host replaces the V8 UI boundary.
         * Unconfigured exhibition retains that exact existing boundary; this
         * does not claim newly implemented exhibition Rules presentation. */
        if (rx_owned_rules_available()) out.r[0] = rx_owned_rules_select(A);
        else
#endif
        act_ui_call_ex(0x005a4bd0, &in, &out, 0, 0);
        const uint32_t popup = out.r[0] & 0xff;
        if (popup && U32(A, 0x1068) == 0) {
            const rx_addr d0 = U32(A, 0x268) ? U32(U32(A, 0x268), 0) : 0;
            if (d0) {
                const uint32_t t = U32(U32(d0, 0x74), 0x9c);
                if (t != 0x16 && t != 0x1b && t != 0x1d) {
                    SU32(A, 0xff068, 1);
                    memset(&in, 0, sizeof in); in.r[0] = U32(0, 0x00a93fcc); in.r[1] = A;
#ifndef RX_ORACLE
                    if (rx_owned_rules_available()) rx_owned_rules_show(A);
                    else
#endif
                    act_ui_call_ex(0x005a4ec0, &in, &out, 0, 0);
                }
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005589c0, rx_activity_vf37)
