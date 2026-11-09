/* race_activity_stub.c -- MXActivityRaceStub's remaining virtuals of the event world's race (activity object +0x234 = the world, +0xff090 = the intro scenario,
 * +0x114244 / +0x129414 / +0x13e600 / +0x13e644 / +0x13e648 scenario and countdown fields):
 *   vf17 005797b0 (the end of the activity: replay cameras reset, the recorded-clip activity cleared), vf18 00566d00 (the restart: the series manager of a series
 *   event, the scenario state, the kind's setup 00566da0, the Activity::vf18 tail), vf37 00568140 (Pre enter), vf25 005681e0 (the Pre frame: the intro scenario).
 * The kind setup00566da0 uses the complete typed0055a7e0 track-row builder for its original finite race-kind map; National /
 * Supercross races (kinds0/1) do not enter this setup branch. */
#include "crt_atexit.h"
#include "race.h"
#include "crt.h"
#include "phys_vec.h"
#include "fx.h"
#include "build_activity.h"
#include "race_extern.h"
#include "career_activity.h"
#include "race_activity.h"
#include "race_activity_stub.h"
#include "race_activity_ui.h"
#include "gateless_race_start.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_activity_freeride_run.h"
#include "build_activity_pre.h"
#include "build_series_init.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "anim_scenario_misc.h"
#include "loop_world.h"
#include "world_misc.h"
#include "crt.h"
#include "phys_vec.h"
#include "fx.h"
#include "world_rider.h"
#include "world_wreck.h"
#include "loop_player.h"
#include "../src/rx_os.h"
#include <string.h>

/* 005797b0 MXActivityRaceStub::vf17 (ECX = activity): Activity::vf17, the world's +0x21de0 cleared, the replay cameras updated with 0.033 s (0x977818), and when this is
 * the clip recorder's activity (0xa942b8) its state / activity cleared. */
void rx_race_stub_vf17(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);                                  /* push esi */
    RACE_CORE_CALL(0, rx_activity_vf17(act));
    const rx_addr w = U32(act, 0x234);
    SU32(w, 0x21de0, 0);
    RACE_CORE_CALL(2, rx_replay_cameras_update(w, 0.032999999821186066f /* 0x977818 */));
    if (act == U32(0, 0x00a942b8)) { SU32(0, 0x00a942b4, 0); SU32(0, 0x00a942b8, 0); }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005797b0, rx_race_stub_vf17)

/* 0056a120 (stdcall the level name; RET 4) -> EAX: the first three characters of the name formatted "%c%c%c" and compared with "NAT" -> 0, "SX_" -> 1, "PX_" -> 0x13,
 * "TX_" -> 0x11, else 0x3d. */
uint32_t rx_race_track_kind(rx_addr name) {
    const rx_addr E0 = rx_sp();                                 /* the entry ESP: sub esp, 0xc; push ebx / esi / edi; the arguments of the format below it */
    const rx_addr buf = E0 - 0xc, args = E0 - 0x24;
    SU32(args, 0, (uint32_t)(int32_t)(int8_t)U8(name, 0));
    SU32(args, 4, (uint32_t)(int32_t)(int8_t)U8(name, 1));
    SU32(args, 8, (uint32_t)(int32_t)(int8_t)U8(name, 2));
    rx_set_sp(E0 - 0x28);
    rx_format_buffer(buf, 4, 0x009420e4 /* "%c%c%c" */, args);
    rx_set_sp(E0);
    static const struct { rx_addr str; uint32_t kind; } k[3] = { {0x009420ec /* "NAT" */, 0}, {0x009420f0 /* "SX_" */, 1}, {0x009420f4 /* "PX_" */, 0x13} };
    for (int i = 0; i < 3; i++) {
        rx_set_sp(E0 - 0x28 + 0x10);                            /* the stricmp's two pushes and return address */
        const int32_t r = crt_stricmp(buf, k[i].str);
        rx_set_sp(E0);
        if (r == 0) return k[i].kind;
    }
    rx_set_sp(E0 - 0x28 + 0x10);
    const int32_t r = crt_stricmp(buf, 0x009420f8 /* "TX_" */);
    rx_set_sp(E0);
    return r == 0 ? 0x3d : 0x11;
}
RX_FUNCTION(0x0056a120, rx_race_track_kind)

/* 00566da0 (ESI = the activity, EDI = 0; plain `push ebp; and esp, -8`): by the kind (+0x23c; kind 0xc is first mapped by the level name, 0056a120): kinds 2, 3, 0x11,
 * 0x12 (table 0x566e3c -> 0x566dd8) and 0x2e, 0x2f, 0x32, 0x34, 0x35 (-> case A) / 0x13 (case B): a session state (+0x238) other than 0 / 2 with the area not yet set (+0x884 = 0)
 * runs 0055a7e0 (the starting grid by the track, with 5 / 0xc); every other kind does nothing. */
void rx_race_stub_kind_setup(rx_addr act) {
    RACE_FRAME_EBP(G, 8, 0, 0);
    uint32_t kind = U32(act, 0x23c);
    if (kind == 0xc) { RACE_CORE_CALL(1, kind = rx_race_track_kind(U32(act, 0x234) + 0x20030)); }
    const uint32_t idx = kind - 2u;
    if (idx <= 0x33) {
        static const uint8_t tab[0x34] = {0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 2, 2, 0, 2, 0, 0};
        const uint32_t c = tab[idx];
        if (c != 2) {
            const uint32_t st = U32(act, 0x238);
            if (st != 0 && st != 2 && U32(act, 0x884) == 0) {
                const rx_addr sp = rx_sp(), entry = sp - 20;
                const uint32_t count = c == 1 ? 12 : 5;
                SU32(entry, 0, c == 1 ? 0x00566e2c : 0x00566e00);
                SU32(entry, 4, act); SU32(entry, 8, act + 0x8a0);
                SU32(entry, 12, count); SU32(entry, 16, 0); /* vf18 retains EDI=0 */
                rx_set_sp(entry);
                rx_gateless_race_start(act, act + 0x8a0, count, 0);
                if (rx_sp() != entry) act_unreached(0x00566e27, "gate-less parent changed entered guestSP");
                rx_set_sp(sp);
            }
        }
    }
    RACE_FRAME_END(G);
}
RX_FUNCTION(0x00566da0, rx_race_stub_kind_setup)

/* 0056a0b0 (stdcall activity; RET 4; `sub esp, 0x194; push esi`): the world's scene entities of the type 0x107b (at most 100: the array at E0 - 0x190, the overflow flag at
 * E0 - 0x194, E0 = the entry ESP) get +0x94 = +0xa8 = 0 */
void rx_race_clear_props(rx_addr act) {
    const rx_addr E0 = rx_sp(), arr = E0 - 0x190, flag = E0 - 0x194;
    rx_set_sp(E0 - 0x198);
    for (rx_addr a = arr; a < E0; a += 4) SU32(a, 0, 0);         /* memset(E0 - 0x18c, 0, 0x18c) and the word below it */
    const rx_addr scene = U32(U32(act, 0x234), 0x20138);
    int32_t n;
    RACE_CORE_CALL(4, n = (int32_t)rx_scene_manager_collect_typed(scene, 0x107b, arr, 0x64, flag));
    for (int32_t i = 0; i < n; i++) {
        const rx_addr o = U32(arr, 4u * (uint32_t)i);
        SU32(o, 0x94, 0); SU32(o, 0xa8, 0);
    }
    rx_set_sp(E0);
}
RX_FUNCTION(0x0056a0b0, rx_race_clear_props)

/* 00566d00 MXActivityRaceStub::vf18 (ECX = activity; `push esi` ... `push edi`; EDI = 0): a series event (kind 0x1f) constructs the series manager on first use (guard bit 0 of
 * 0x9e275c, _atexit) and queries it (005b6a30 on 0x9e2188); then +0x114244 (the intro scenario's state) = 0xf, +0x129414 = 0, the kind setup, session state 5: +0xff088 = the
 * driver count, +0x13e644 / +0x13e648 = 0, session state 8: 0056a0b0; the tail is Activity::vf18. */
void rx_race_stub_vf18(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);                                  /* push esi */
    if (U32(act, 0x23c) == 0x1f) {
        if ((U8(0, 0x009e275c) & 1) == 0) {
            SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
            RACE_CORE_CALL(0, (void)rx_series_manager_construct());
            RX_ATEXIT(0x00911da0);   /* _atexit */
        }
        RACE_CORE_CALL(1, (void)rx_career_current_event_type(0x009e2188));   /* 005b6a30 (stdcall manager; RET 4) */
    }
    rx_set_sp(F - 4);                                           /* push edi */
    SU32(act, 0x114244, 0xf);
    SU32(act, 0x129414, 0);
    RACE_CORE_CALL(0, rx_race_stub_kind_setup(act));
    rx_set_sp(F);                                               /* pop edi */
    const uint32_t st = U32(act, 0x238);
    if (st == 5) SU32(act, 0xff088, U32(act, 0x26c));
    SU32(act, 0x13e644, 0);
    SU32(act, 0x13e648, 0);
    if (st == 8) RACE_CORE_CALL(1, rx_race_clear_props(act));
    RACE_FRAME_END(F);                                          /* pop esi; the tail jump keeps the entry ESP */
    rx_activity_vf18(act);
}
RX_FUNCTION(0x00566d00, rx_race_stub_vf18)

/* 0055e6e0 (EAX = activity, ECX = level): the track music flag 0x9c3edc = the level when the activity has no audio logic (+0x234 = 0) or the logic's +0x22148 is set, else 0 */
void rx_activity_set_track_level(rx_addr act, uint32_t level) {
    const rx_addr audio = U32(act, 0x234);
    if (audio == 0) { SU32(0, 0x009c3edc, level); return; }
    SU32(0, 0x009c3edc, U32(audio, 0x22148) != 0 ? level : 0);
}
RX_FUNCTION(0x0055e6e0, rx_activity_set_track_level)


/* the original's `push arg...; call`: the reserved argument slots written (a port that reads its float from its argument slot needs it) */
static rx_addr stub_site(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* 00621570 (ECX = the rider animation object of a player): +0x564 -> byte 1, +0x568 -> word = +0x5b4, +0x58c -> the clamp of +0x588 / +0x584 (min(+0x588, 0) when 0 >= it,
 * else 0; then +0x584 unless it is below that), then 006235e0 (EAX = 5; stack 0.25 (0x92ff84), 0) */
void rx_rider_anim_fade_default(rx_addr ra) {
    RACE_FRAME_PLAIN(F, 0, 0);
    rx_addr p = U32(ra, 0x564);
    if (p) SU8(p, 0, 1);
    p = U32(ra, 0x568);
    if (p) SU16(p, 0, U16(ra, 0x5b4));
    float v = F32(ra, 0x588);
    const float v584 = F32(ra, 0x584);
    if (!(0.0f >= v)) v = 0.0f;
    if (!(v584 < v)) v = v584;
    p = U32(ra, 0x58c);
    if (p) SF32(p, 0, v);
    { const uint32_t a[2] = {rx_fb(0.25f /* 0x92ff84 */), 0}; const rx_addr s = stub_site(2, a);
      rx_world_rider_chains_fade_out(ra, 5, 0.25f /* 0x92ff84 */, 0); rx_set_sp(s); }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00621570, rx_rider_anim_fade_default)

/* 0055a390 (stdcall activity; RET 4; push ebx / ebp / esi / edi): every driver's player's rider animation object (+0x74 -> +0x1b4) that is not locked (+0xaf08 = 0): in
 * state 1 (+0xaee0) the chains fade in (006237a0: EAX = 5, ESI = 0, 0.25), then its default fade 00621570, and its state = 1 */
void rx_race_drivers_fade(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 4);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr ra = U32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74), 0x1b4);
        if (ra == 0 || U32(ra, 0xaf08) != 0) continue;
        if (U32(ra, 0xaee0) == 1) { const uint32_t a[1] = {rx_fb(0.25f /* 0x92ff84 */)}; const rx_addr s = stub_site(1, a);
                                      rx_world_rider_chains_fade_in(ra, 5, 0, 0.25f /* 0x92ff84 */); rx_set_sp(s); }
        RACE_CORE_CALL(0, rx_rider_anim_fade_default(ra));
        SU32(ra, 0xaee0, 1);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055a390, rx_race_drivers_fade)

/* 00568140 MXActivityRaceStub::vf37 (ECX = activity, stack the old state; RET 4; the MXActivityRace vtable's slot 37 is the thunk 00565030 = jmp 00568140): Activity::vf37; session
 * state 5: the lap count (+0xff070) = drivers - 1; the podium / intro setup 0074ada0 (EAX = activity, ECX = the world's +0x20150 block); with no scene lock (world +0x20018 = 0),
 * 0xd60ff0 set, 0xae1b90 clear and no area flag (+0xfe8 = 0): every local vehicle's vf49 (0055d240), the music level +0xff080 = 0x32 and 0055e6e0, the countdown +0x13e600 = 2;
 * a UI flag (+0x1068) raises the hub message through the UI manager (UI boundary); finally 0055a390. */
void rx_race_stub_vf37(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);                                  /* push esi */
    RACE_CORE_CALL(1, rx_activity_vf37(act));
    if (U32(act, 0x238) == 5) SU32(act, 0xff070, U32(act, 0x26c) - 1);
    RACE_CORE_CALL(0, rx_podium_setup(act, U32(act, 0x234) + 0x20150));
    if (U32(U32(act, 0x234), 0x20018) == 0 && U32(0, 0x00d60ff0) != 0 && U32(0, 0x00ae1b90) == 0 && U32(act, 0xfe8) == 0) {
        RACE_CORE_CALL(0, rx_local_vehicles_vf49());
        SU32(act, 0xff080, 0x32);
        RACE_CORE_CALL(0, rx_activity_set_track_level(act, 0x32));
        SU32(act, 0x13e600, 2);
    }
    if (U32(act, 0x1068) != 0) {
#ifdef RX_ORACLE   /* UI boundary (core/plan/QUEUE.md "chain"): 0059b860 sets the UI properties of the hub routing and raises "GotoHub" on the UI: the product has no UI */
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(0, 0x00a93fcc);
        rx_call_original_ex(0x0059b860, &in, 0, 0, 0);
#endif
    }
    RACE_CORE_CALL(1, rx_race_drivers_fade(act));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568140, rx_race_stub_vf37)
/* 00565030 (MXActivityRace slot 37): `jmp 00568140` */
void rx_race_stub_vf37_slot(rx_addr act) { rx_race_stub_vf37(act); }
RX_FUNCTION(0x00565030, rx_race_stub_vf37_slot)

/* 0055eb40 BaseScenario::vf11 (thiscall): the scenario is running: +0x151b4 neither 0 nor 0xf */
uint32_t rx_scenario_is_active(rx_addr sc) {
    const uint32_t s = U32(sc, 0x151b4);
    return s != 0 && s != 0xf;
}
RX_FUNCTION(0x0055eb40, rx_scenario_is_active)

/* 005681e0 MXActivityRaceStub::vf25 (ECX = activity, stack dt; RET 4): the Pre frame (not while the three counters 0xa81ad0 / 0xa81ad8 / 0xa81ac8 add up above 0, nor with +0xff068
 * set): when the activity is not in an intro (vf11) the intro scenario's state (+0x114244) = 0, +0xff084 = 1, the scenes' cameras reset (0074b480), the world's +0x203e0 hidden
 * and its +0x70 = 0, the cast stopped (0074b450) and its states cleared (0074b4f0) -- all of it inside the `intro == 0` branch (00568220 jne 0056827d);
 * +0xff084 = 1 with the area flag (+0xfe8) or 0xae1b90; the countdown +0x13e600 decremented, and when it ran out
 * and the scenario (vf7 at +0x1c on +0xff090) is done: with an intro running the screen set-up 00747810 and, if it returned non-zero, the UI hub message (UI boundary), +0xff084 = 1;
 * Activity::vf25; and while the scenario is not done (vf7) its update 007473f0. */
void rx_race_stub_vf25(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 4, 3);                                 /* push ebp; and esp, -8; push ecx; push ebx / esi / edi */
    const int32_t sum = I32(0, 0x00a81ad8) + I32(0, 0x00a81ad0) + I32(0, 0x00a81ac8);
    if (sum > 0) { RACE_FRAME_END(F); return; }
    if (U32(act, 0xff068) != 0) { RACE_FRAME_END(F); return; }
    uint32_t intro;
    intro = race_thiscall(race_vfn(act, 11), act, 0, 0, 0, 0, 0);
    const rx_addr w = U32(act, 0x234), sc = act + 0xff090;
    if (intro == 0) {
        SU32(act, 0x114244, 0);
        SU32(act, 0xff084, 1);
        RACE_CORE_CALL(0, rx_scenario_anims_reset_cameras(w + 0x20150));
        const rx_addr o = U32(U32(act, 0x234), 0x203e0);
        if (o != 0) { SU8(o, 0xa2, 0); SU32(o, 0x70, 0); }
        RACE_CORE_CALL(0, rx_scenario_anims_stop_actors(U32(act, 0x234) + 0x20150));
        /* 00568278 call 0074b4f0 is inside the same `intro == 0` block (00568220 jne 0056827d skips it): only when the activity
         * is not in an intro. Outside it the core stopped the intro cast every Pre frame: the intro riders' RiderCharacter
         * +0x70 / +0xa2 (playing / visible) were cleared (0055f700 / 0084ed20 Verify), where the original keeps them. */
        RACE_CORE_CALL(0, rx_scenario_cast_stop_all(U32(act, 0x234) + 0x20150));
    }
    if (U32(act, 0xfe8) != 0 || U32(0, 0x00ae1b90) != 0) SU32(act, 0xff084, 1);
    const int32_t cd = I32(act, 0x13e600);
    SU32(act, 0x13e600, (uint32_t)(cd - 1));
    if (!(cd > 0)) {
        if (race_thiscall(race_vfn(sc, 7), sc, 0, 0, 0, 0, 0) != 0) {
            if (race_thiscall(race_vfn(act, 11), act, 0, 0, 0, 0, 0) != 0) {
                uint32_t started = 0;
                RACE_CORE_CALL(0, started = rx_scenario_start(sc));      /* the screen set-up (ESI = the scenario) -> EAX */
                if (started != 0) {
#ifdef RX_ORACLE   /* UI boundary: UIProperty_SetString("UIRoutingMessage", "GotoEventIntroPage") and UIManager_QueueUIEvent("GotoHub", 1) */
                    uint32_t a1[1] = {0x00941f7c};
                    rx_regs ui; memset(&ui, 0, sizeof ui); ui.r[6] = U32(0, 0x00a93fcc); ui.r[7] = 0x0094cc74;   /* ESI = the UI manager (005682d7 loads it before the call; UIProperty_SetString reads it) */
                    rx_call_original_ex(0x005dbf50, &ui, 0, 1, a1);
                    uint32_t a2[2] = {0x009424ec, 1};
                    rx_call_original(0x005db670, RX_THISCALL, U32(0, 0x00a93fcc), 2, a2);
#endif
                }
            }
            SU32(act, 0xff084, 1);
        }
    }
    RACE_CORE_CALL(1, rx_activity_pre(act, dt));
    if (race_thiscall(race_vfn(sc, 7), sc, 0, 0, 0, 0, 0) == 0) RACE_CORE_CALL(2, rx_scenario_intro_update(sc, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005681e0, rx_race_stub_vf25)

/* 0055eb60 MXActivityRaceStub::vf48 (thiscall): the intro flag +0xff084 */
uint32_t rx_race_stub_vf48(rx_addr act) { return U32(act, 0xff084); }
RX_FUNCTION(0x0055eb60, rx_race_stub_vf48)

/* 0055cda0 (ECX = activity; stack kind, float; RET 8): the network timer event handler: kind 1: the activity's timer +0x2a8 = the value; kind 3: +0x294 = the value when it is above
 * the current one or the current one is more than 2.0 (0x92e330) above it */
void rx_activity_net_timer_event(rx_addr act, uint32_t kind, float v) {
    if (kind == 1) { SF32(act, 0x2a8, v); return; }
    if (kind != 3) return;
    float cur = F32(act, 0x294);
    if (v > cur || rx_subs(cur, v) > 2.0f /* 0x92e330 */) SF32(act, 0x294, v);
}
RX_FUNCTION(0x0055cda0, rx_activity_net_timer_event)

/* 00559b60 (no arguments): every view of the current context's player list (the engine context [0xa95844]: +4 the current slot, +0x1c + 4 * slot the context; +0x19d0 the count,
 * +0x19b8 the views, each with a parameter block at +0x70) gets its default parameters: +0 = +0x40 = 0, +0x30 .. +0x38 = 0.0, +0x3c = 1.0, +0x1c = 1.5 (0x9322a4), +0x14 = 1,
 * +0x20 = 0.0, +0x18 = 2, +0x28 = +0x2c = 0.0 */
void rx_game_reset_view_params(void) {
    RACE_FRAME_EBP(F, 16, 0xc, 1);
    const rx_addr ctx = U32(0, 0x00a95844);
    rx_addr c = U32(ctx, 0x1c + 4 * U32(ctx, 4));
    for (uint32_t i = 0; i < U32(c, 0x19d0); i++) {
        const rx_addr q = U32(U32(c, 0x19b8 + 4 * i), 0x70);
        SU32(q, 0, 0); SU32(q, 0x40, 0);
        SF32(q, 0x30, 0.0f); SF32(q, 0x34, 0.0f); SF32(q, 0x38, 0.0f);
        SF32(q, 0x3c, 1.0f); SF32(q, 0x1c, 1.5f /* 0x9322a4 */);
        SU32(q, 0x14, 1); SF32(q, 0x20, 0.0f); SU32(q, 0x18, 2); SF32(q, 0x28, 0.0f); SF32(q, 0x2c, 0.0f);
        c = U32(ctx, 0x1c + 4 * U32(ctx, 4));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559b60, rx_game_reset_view_params)

/* The localized text of a finishing driver's message ("Player Wins" 00587d00 carries it to the HUD). The text goes to the UI only (the event's subscribers are HUD
 * components; no gameplay code reads the buffer): the oracle runs the original localization lookups (005dfc90 / 005e0270 / WideCharToMultiByte / the formats) so a Verify sees
 * the same memory, the product has no localization database and leaves an empty string. Buffers as the original's frame: the UTF-8 string at E0 - 0x208 (0x100 bytes), the
 * message at E0 - 0x108 (0x104 bytes), E0 = the entry ESP. */
static rx_addr lap_message(rx_addr E0, rx_addr act, rx_addr drv, int winner, int online) {
    const rx_addr text = E0 - 0x108;
    SU8(text, 0, 0);
#ifdef RX_ORACLE
    const rx_addr utf8 = E0 - 0x208;
    const rx_addr player = U32(drv, 0x74);
    static const rx_addr ids[4] = {0x00941fb4, 0x00941fc4, 0x00941fb4, 0x00942078};
    const rx_addr id = winner ? ids[online ? 0 : 1] : ids[online ? 2 : 3];
    uint32_t zero = 0;
    const rx_addr mgr = rx_call_original(0x005dfc90, RX_CDECL, 0, 1, &zero);
    rx_addr entry;
    RACE_CORE_CALL(0, entry = rx_ui_language_lookup(id, mgr));   /* 005e0270 (EAX = the string id, EDI = the language) */
    { uint32_t a[8] = {0xfde9, 0, U32(entry, 0x10), 0xffffffffu, utf8, 0x100, 0, 0}; rx_os_import(0x009160a8, 8, a); }
    if (winner) {
        const rx_addr fmt = online ? 0x00942070 : 0x00941fd0;
        rx_set_sp(E0 - 0x230); SU32(E0 - 0x230, 4, player + 0x54); SU32(E0 - 0x230, 8, utf8);
        rx_format_buffer(text, 0x104, fmt, E0 - 0x230 + 4);
        rx_set_sp(E0);
    } else if (online) {
        rx_set_sp(E0 - 0x22c); SU32(E0 - 0x22c, 4, utf8);
        rx_format_buffer(text, 0x104, 0x009419e4, E0 - 0x22c + 4);
        rx_set_sp(E0);
    } else {
        const rx_addr ord = E0 - 0x210;
        SU8(E0 - 0x109, 0, 0);
        { uint32_t a[1] = {U32(act, 0x300)}; rx_regs oi; memset(&oi, 0, sizeof oi); oi.r[1] = 8; oi.r[7] = ord; rx_regs oo; memset(&oo, 0, sizeof oo); rx_call_original_ex(0x005dc7a0, &oi, &oo, 1, a); }
        rx_set_sp(E0 - 0x230); SU32(E0 - 0x230, 4, utf8); SU32(E0 - 0x230, 8, ord);
        rx_format_buffer(text, 0x104, 0x00941fd0, E0 - 0x230 + 4);
        rx_set_sp(E0);
    }
#else
    (void)act; (void)drv; (void)winner; (void)online;
#endif
    return text;
}

/* the session's presence of a controller (PCSession::vf12 006e9e00: thiscall [0x00da6cb0]; controller, key, value): online sessions only -- not core (build_activity_grid.c) */
static void lap_presence(uint32_t ctrl, uint32_t key, uint32_t value) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    const rx_addr s = U32(0, 0x00da6cb0);
    const uint32_t a[3] = {ctrl, key, value};
    act_ui_call(U32(U32(s, 0), 0x30), RX_THISCALL, s, 3, a);
}

/* 005676e0 (ECX = activity; stack driver, lap, flag, laps; RET 0x10; the "Lap Completed" handler of the activity modes 1 / 6 / 7 / 8, subscribed by build_activity_start.c): a driver
 * finishing the race (its lap = the race's laps): the standings sorted, the finisher count +0x300 + 1 (at most the driver count): the first finisher raises "Player Wins" with its
 * message (text: UI), online sessions clear a negative +0x2a4; the player's vehicle / rider settle (0054b720) and, without the area flag (+0xfe8), the activity's vf53 (3) runs; later
 * finishers raise the same event with their place. Then for every driver: a recording clip candidate (type 9 finished / 7), and for a local human the session presence
 * 0x10 = min(lap + 1, laps) and 0x11 = laps. */
void rx_activity_lap_completed(rx_addr A, rx_addr drv, int32_t lap, uint32_t flag, int32_t laps) {
    (void)flag;
    const rx_addr E0 = rx_sp();
    RACE_FRAME_PLAIN(F, 0x214, 4);
    uint32_t finished = 0;
    const uint32_t mode = U32(A, 0x238);
    if (mode == 1 || (mode - 6u) <= 2u) {
        if (U32(drv, 0xf1c) == U32(A, 0xff070)) {
            finished = 1;
            RACE_CORE_CALL(0, rx_activity_sort_standings(A));
            int32_t n = I32(A, 0x300) + 1;
            if (!(n < I32(A, 0x26c))) n = I32(A, 0x26c);
            SU32(A, 0x300, (uint32_t)n);
            if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
            const rx_addr sess = U32(0, 0x00da6cb0);
            const int online = U32(sess, 0x1c4c) != 0 && U32(sess, 0x1b0) != 1;
            const rx_addr mgr = U32(A, 0x290);
            if (n == 1) {
                const rx_addr text = lap_message(E0, A, drv, 1, online);
                rx_event_player_wins(mgr, A, drv, text, 0);
                if (online && F32(A, 0x2a4) < 0.0f) SF32(A, 0x2a4, 0.0f);
                rx_player_set_fade_hold(U32(drv, 0x74));
                if (U32(A, 0xfe8) == 0) race_thiscall(race_vfn(A, 53), A, 1, 3, 0, 0, 0);
            } else {
                const rx_addr text = lap_message(E0, A, drv, 0, online);
                rx_event_player_wins(mgr, U32(U32(drv, 0x74), 0x94), drv, text, 0);
            }
            rx_player_set_fade_hold(U32(drv, 0x74));
        }
        const rx_addr P = U32(drv, 0x74);
        if (U32(0, 0x00a942b4) == 1 && U32(P, 4) != 0 && U32(P, 0x10) == 0)
            rx_world_clip_recorder_candidate_of_type(finished ? 9 : 7, drv);
        if (U32(P, 4) != 0 && U32(P, 0x10) == 0) {
            int32_t v = lap + 1;
            v = v < 1 ? 1 : (v > laps ? laps : v);
            lap_presence(U32(P, 0x1f0), 0x10, (uint32_t)v);
            lap_presence(U32(P, 0x1f0), 0x11, (uint32_t)laps);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005676e0, rx_activity_lap_completed)

/* 005798f0 MXActivityRaceStub::vf31 (and the shared slot 31; ECX = activity, arg: the old state; RET 4): the level's frame marked
 * (0084ea00), its current activity's scripted sequences run to their end (0055e2b0), the level cast released (0074bd50) with its
 * +0x44 cleared, the UI's hub routing (0059b860: UI BOUNDARY, oracle original / product nothing) and every local vehicle's vf49
 * (0055d240). */
void rx_race_stub_vf31(rx_addr act) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push esi; push edi */
    rx_set_sp(E);
    const rx_addr ctx = U32(0, 0x00a95844);
    const rx_addr sm = U32(act, 0x234) + 0x20150;
    RACE_CORE_CALL(0, rx_engine_frame_mark(U32(ctx, 0x10)));
    const rx_addr cur = U32(sm, 0x40);
    if (cur != 0) RACE_CORE_CALL(1, rx_activity_finish_sequences(cur));
    RACE_CORE_CALL(1, rx_scenario_cast_release(sm));
    SU32(sm, 0x44, 0);
#ifdef RX_ORACLE   /* UI boundary: 0059b860 sets the hub routing's UI properties and raises "GotoHub" */
    { rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(0, 0xa93fcc);
      rx_call_original_ex(0x0059b860, &in, 0, 0, 0); }
#endif
    RACE_CORE_CALL(0, rx_local_vehicles_vf49());
    rx_set_sp(entry0);
}
RX_FUNCTION(0x005798f0, rx_race_stub_vf31)

/* 005695b0 (ECX = the activity; stack: activity; RET 4): every driver's vehicle pose (vehicle vf 0x90 into a 0x40-byte frame
 * buffer, the simulation's vf 0x54 on the same); then, for each driver, the controller's key map groups 2 / 4 / 8 and the map
 * +0x748 enabled (mask bits 2 / 4 / 8 / 0x100 set in +0x11cc when the controller is in the "reversed" state +0x11c8). */
void rx_race_stub_reset_vehicle_maps(rx_addr act) {
    RACE_FRAME_EBP(F, 16, 0x34, 3);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
        const rx_addr p = U32(d, 0x74), veh = U32(p, 0x1ac);
        (void)race_thiscall(race_vfn(veh, 36 /* +0x90 */), veh, 1, F + 0x20, 0, 0, 0);
        (void)race_thiscall(race_vfn(U32(veh, 0x234), 21 /* +0x54 */), U32(veh, 0x234), 1, F + 0x20, 0, 0, 0);
        const int32_t j = I32(p, 0x1f0);
        if (j < 0) continue;
        if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) | 2u); }
        else { const rx_addr m = U32(0, 0x009c52c4 + 4 * (uint32_t)j) + 0x348;
               (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)j, 1, 0, 0); }
        if (j < 0) continue;
        if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) | 4u); }
        else RACE_CORE_CALL(0, rx_player_slot_group2((uint32_t)j, 1));
        if (j < 0) continue;
        if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) | 8u); }
        else {
            rx_addr m = 0;
            RACE_CORE_CALL(0, m = rx_world_pad_wreck_steer_map((uint32_t)j));
            (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)j, 1, 0, 0);
            RACE_CORE_CALL(0, m = rx_world_pad_wreck_lean_map((uint32_t)j));
            (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)j, 1, 0, 0);
        }
        if (j < 0) continue;
        if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) | 0x100u); }
        else { const rx_addr m = U32(0, 0x009c52c4 + 4 * (uint32_t)j) + 0x748;
               (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)j, 1, 0, 0); }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005695b0, rx_race_stub_reset_vehicle_maps)

/* 00568340 MXActivityRaceStub::vf38 (ECX = the activity, stack: the old state; RET 4): the base Countdown enter (Activity::vf38);
 * for the event kinds 0 / 1 the vehicles' maps reset (005695b0); in the session state 6 every driver's simulation +0x54 = 1 and its
 * controller's 11 key map groups disabled (0x348, 0x9ec, 0x127c, 0x167c, 0x748, 0x147c, 0xf34, the steer / lean maps and 00548bf0 /
 * 00548c40; mask bits 1, 2, 4 .. 0x400 cleared in +0x11cc when +0x11c8 is set). */
void rx_race_stub_vf38(rx_addr act, uint32_t old_state) {
    (void)old_state;                                          /* the port of 00558f70 takes no argument */
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push ebp; push (the argument) */
    rx_set_sp(E);
    RACE_CORE_CALL(1, rx_activity_countdown_enter(act));
    const uint32_t kind = U32(act, 0x23c);
    if (kind == 0 || kind == 1) RACE_CORE_CALL(1, rx_race_stub_reset_vehicle_maps(act));
    if (I32(act, 0x238) == 6) {
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr d = U32(U32(act, 0x268), 4u * (uint32_t)i);
            const rx_addr p = U32(d, 0x74);
            SU32(U32(U32(p, 0x1ac), 0x234), 0x54, 1);
            const int32_t j = I32(p, 0x1f0);
            if (j < 0) continue;
            /* the 11 groups, in the original's order (map offset, mask bit, the map getters and the two slot groups) */
            static const uint32_t k_off[7] = {0x348, 0x9ec, 0x127c, 0x167c, 0x748, 0x147c, 0xf34};
            static const uint32_t k_bit[7] = {2, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400};
            int32_t jj = j;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~1u); }
            else {
                rx_addr m = 0;
                RACE_CORE_CALL(0, m = rx_world_pad_steer_map((uint32_t)jj));
                (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0);
                RACE_CORE_CALL(0, m = rx_world_pad_lean_map((uint32_t)jj));
                (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0);
            }
            if (jj < 0) continue;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~k_bit[0]); }
            else { const rx_addr m = U32(0, 0x009c52c4 + 4 * (uint32_t)jj) + k_off[0];
                   (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0); }
            if (jj < 0) continue;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~4u); }
            else RACE_CORE_CALL(0, rx_player_slot_group2((uint32_t)jj, 0));
            if (jj < 0) continue;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~8u); }
            else {
                rx_addr m = 0;
                RACE_CORE_CALL(0, m = rx_world_pad_wreck_steer_map((uint32_t)jj));
                (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0);
                RACE_CORE_CALL(0, m = rx_world_pad_wreck_lean_map((uint32_t)jj));
                (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0);
            }
            if (jj < 0) continue;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~0x10u); }
            else { const rx_addr m = U32(0, 0x009c52c4 + 4 * (uint32_t)jj) + k_off[1];
                   (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0); }
            if (jj < 0) continue;
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~0x20u); }
            else RACE_CORE_CALL(0, rx_player_slot_group5((uint32_t)jj, 0));
            for (uint32_t g = 2; g < 7; g++) {
                if (jj < 0) break;
                if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) & ~k_bit[g]); continue; }
                const rx_addr m = U32(0, 0x009c52c4 + 4 * (uint32_t)jj) + k_off[g];
                (void)race_thiscall(race_vfn(m, 9 /* +0x24 */), m, 2, (uint32_t)jj, 0, 0, 0);
            }
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00568340, rx_race_stub_vf38)

/* 00565040 MXActivityRace::vf38 (ECX = the activity, stack: the old state; RET 4): the Countdown enter: the start board's
 * countdown time +0x4f4 = rand() * (1/32768) * 6 + 3 s; the stub's vf38 (00568340); the event countdown scale +0xff078 = 4.0,
 * +0xff080 = 100; the session's display minimum 0x9c3edc = 100 (0 when the world's +0x22148 is set); by the session state
 * (+0x238): 0 / 2 / 3: the countdown clock +0x29c = 0; 4: nothing; 1 / 5 .. 8: +0x29c = +0x2b0 and, offline for the race event
 * kinds, with a board the saved game's display minimum 0x9cf5b0 (1000). */
void rx_race_vf38(rx_addr act, uint32_t old_state) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push esi; push edi */
    rx_set_sp(E);
    const rx_addr board = U32(act, 0x2cc);
    if (board != 0) {
        const float r = (float)crt_rand();
        const float drawn = phys_add(phys_mul(phys_mul(r, 3.0518509447574615e-05f /* 0x92e054 */), 6.0f /* 0x92fe4c */), 3.0f /* 0x92fe48 */);
        SF32(board, 0x4f4, drawn);
    }
    RACE_CORE_CALL(1, rx_race_stub_vf38(act, old_state));
    SF32(act, 0xff078, 4.0f /* 0x9322c0 */);
    SU32(act, 0xff080, 100);
    const rx_addr world = U32(act, 0x234);
    SU32(0, 0x009c3edc, (world == 0 || U32(world, 0x22148) != 0) ? 100u : 0u);
    switch (I32(act, 0x238)) {
    case 0: case 2: case 3:
        SF32(act, 0x29c, 0.0f);
        rx_set_sp(entry0);
        return;
    case 1: case 5: case 6: case 7: case 8: {
        SF32(act, 0x29c, F32(act, 0x2b0));
        rx_addr m = 0;
        RACE_CORE_CALL(0, m = rx_fx_session_manager());       /* 006de1c0 */
        const rx_addr s = U32(m, 0x10);
        if (U32(s, 0x1c4c) == 0 || U32(s, 0x1b0) == 1) {
            const uint32_t kind = U32(act, 0x23c);
            static const uint32_t kinds[17] = {0, 0x23, 0x24, 0x25, 1, 0x13, 0x27, 0x28, 0x29, 7, 0x2b, 0x2c, 0x2d, 4, 0x37, 0x38, 0x39};
            if (U32(act, 0x2cc) != 0)
                for (uint32_t k = 0; k < 17; k++)
                    if (kind == kinds[k]) { SF32(act, 0x29c, F32(0, 0x009cf5b0)); break; }
        }
        rx_set_sp(entry0);
        return;
    }
    default:
        rx_set_sp(entry0);
        return;
    }
}
RX_FUNCTION(0x00565040, rx_race_vf38)

/* Isolated waypoint composition: expose existing presentation helpers without
 * duplicating their localization/network boundary or altering race logic. */
#include "waypoint_lap_support.h"
rx_addr rx_waypoint_lap_message(rx_addr entry, rx_addr activity, rx_addr driver, int winner, int online) {
    const rx_addr saved = rx_sp();
    rx_addr message = lap_message(entry, activity, driver, winner, online);
    rx_set_sp(saved); return message;
}
void rx_waypoint_lap_presence(uint32_t controller, uint32_t key, uint32_t value) {
    lap_presence(controller, key, value);
}
