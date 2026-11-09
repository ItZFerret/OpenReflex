/* anim_scenario_race_intro.c -- the race intro scenario (RaceIntroScenario 0x00943394; vf0 .. vf13 below), the steps of the intro cinematic: 1 the start-line
 * overview, 2 / 3 the hero shot of a human's vehicle, 4 / 5 the crew chief's shot of the next human (bike 4, vehicle 5), 6 the board, then the race. The scenario object
 * (+0x14 the activity, +0x20 the script, +0x151b0 started, +0x151b4 the step, +0x151c0 the human driver index, +0x151c4) drives the level cast (world +0x20150 = SM: +0x40 the
 * activity, +0x50 + 0x30 i the cast entries, +0x16ec the cameras, +0x1730 the active camera; world +0x203e0 the board, +0x21d50 the race camera matrix).
 * Series events (the career's event kinds; the manager 0x9e2188 behind the static guard 0x9e275c) choose other first steps. */
#include "vehicle_audio.h"
#include "crt_atexit.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim_scenario_race_intro.h"
#include "build_activity_util.h"
#include "build_series_init.h"
#include "race.h"
#include "race_activity.h"
#include "race_driver.h"
#include "race_extern.h"
#include "race_misc.h"
#include "anim.h"
#include "anim_ik.h"
#include "build_asset.h"
#include "world_wreck.h"
#include "fx.h"
#include "phys_terrain.h"
#include "rx_vcall.h"
#include "world_rider.h"
#include "world_util.h"
#include "world_veh.h"
#include "world_node.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "music_player.h"
#include "rider_cap.h"

/* A direct call at the original call site: `n` stack arguments and the (zero) return-address slot below the caller's ESP, so the
 * callee's frame lands where the original's does. */
static rx_addr rs_site(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}
/* `rep movsd` of 0x10 dwords between game addresses. */
static void rs_copy64(rx_addr dst, rx_addr src) {
    for (uint32_t k = 0; k < 16; k++) SU32(dst, 4u * k, U32(src, 4u * k));
}

static void series_static(void) {
    if ((U8(0, 0x009e275c) & 1) == 0) {
        SU32(0, 0x009e275c, U32(0, 0x009e275c) | 1);
        rx_series_manager_construct();
        RX_ATEXIT(0x00911da0);       /* _atexit */
    }
}

/* 00749550 (EDX = scenario): every driver's vehicle simulation (+0x1ac -> +0x234) +0x54 = 1 */
void rx_scenario_sim_flag(rx_addr sc) {
    const rx_addr act = U32(sc, 0x14);
    for (int32_t i = 0; i < I32(act, 0x26c); i++)
        SU32(U32(U32(U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74), 0x1ac), 0x234), 0x54, 1);
}
RX_FUNCTION(0x00749550, rx_scenario_sim_flag)

/* 0074bb90 PodiumRiders_ClearMud (stdcall SM; RET 4): the mud (+0x3b0 .. +0x3bc) of every player's rider entity of the cast (entries at +0x50 + 0x30 k, k < the player count) cleared */
void rx_podium_riders_clear_mud(rx_addr sm) {
    const int32_t n = U8(U32(0, 0x00a946b8), 0x1541c);
    for (int32_t k = 0; k < n; k++) {
        const rx_addr e = rx_cast_u32(sm, (uint32_t)k, 0);   /* (w-rc-core) entries 12..31 in side storage */
        if (e == 0) continue;
        SF32(e, 0x3b0, 0.0f); SF32(e, 0x3b4, 0.0f); SF32(e, 0x3b8, 0.0f); SF32(e, 0x3bc, 0.0f);
    }
}
RX_FUNCTION(0x0074bb90, rx_podium_riders_clear_mud)

/* 0074d6c0 (EAX = the scenario): the player of the human driver number +0x151c0, or 0 when there are fewer humans (00559e20 counts with the id 1) */
rx_addr rx_race_intro_human_player(rx_addr sc) {
    const uint32_t n = U32(sc, 0x151c0);
    const rx_addr act = U32(sc, 0x14);
    if ((int32_t)n < (int32_t)rx_activity_count_humans_of(act, 1)) return U32(rx_activity_human_driver(act, (int32_t)n), 0x74);
    return 0;
}
RX_FUNCTION(0x0074d6c0, rx_race_intro_human_player)

/* 00749750 (ECX = the player): the crew chief step of the vehicle's class (+0x194 above 1: a vehicle 5, else a bike 4) */
uint32_t rx_race_intro_class_step(rx_addr p) { return U32(U32(p, 0x1ac), 0x194) > 1 ? 5 : 4; }
RX_FUNCTION(0x00749750, rx_race_intro_class_step)

/* 00749680 (ECX = k, no bound check: 0 .. 7): the career event's step: 3 -> 2, 4 -> 4, 6 -> 0, 7 -> 1, else 5 */
uint32_t rx_race_intro_career_step(uint32_t k) {
    switch (k) { case 3: return 2; case 4: return 4; case 6: return 0; case 7: return 1; default: return 5; }
}
RX_FUNCTION(0x00749680, rx_race_intro_career_step)

/* 00749c00 (EDI = the scenario) -> EAX: the first step: 1, 2 for the series event kinds 3 / 7 .. (the series manager +0x10 / +0xc), and for the kind 0xc or the modes 8 / 5: the human
 * driver's crew chief step (5, or 4 for a bike / ATV class) */
uint32_t rx_race_intro_series_step(rx_addr sc) {
    uint32_t step = 1;
    series_static();
    if (U32(0, 0x009e2318) == 2 || U32(0, 0x009e2318) == 1) {
        series_static();
        const rx_addr s = U32(0, 0x009e2314);
        uint32_t v = U32(s, 0xc);
        if (v == 2) v = U32(s, 0x10);
        v -= 3;
        if (v <= 4 && (v == 1 || v == 4)) step = 2;
    } else {
        series_static();
        if (U32(0, 0x009e2318) == 1) { /* unreachable: handled above */ }
    }
    const rx_addr act = U32(sc, 0x14);
    if (U32(act, 0x23c) == 0xc || U32(act, 0x238) == 8 || U32(act, 0x238) == 5) {
        const rx_addr d = rx_activity_human_driver(act, (int32_t)U32(sc, 0x151c0));
        step = 5;
        if (!(U32(U32(U32(d, 0x74), 0x1ac), 0x194) > 1)) step = 4;
    }
    return step;
}
RX_FUNCTION(0x00749c00, rx_race_intro_series_step)

/* ---- 0074b5a0 ScenarioCast_Pause (EAX = the cast; no RET): every cast entry's entity (+0x50 + 0x30 k, the 12 entries) gets
 * state +0x378 = 1. */
void rx_scenario_cast_pause(rx_addr sm) {
    for (uint32_t k = 0; k < RX_RC_MAX; k++) {   /* (w-rc-core) entries 12..31 in side storage */
        const rx_addr e = rx_cast_u32(sm, k, 0);
        if (e != 0) SU32(e, 0x378, 1);
    }
}
RX_FUNCTION(0x0074b5a0, rx_scenario_cast_pause)

/* ---- 0055eb20 / 0055eb30 BaseScenario::vf9 / vf10: the step == 7 / == 0xb predicates. ---- */
uint32_t rx_scenario_step_is_7(rx_addr sc) { return I32(sc, 0x151b4) == 7 ? 1u : 0u; }
RX_FUNCTION(0x0055eb20, rx_scenario_step_is_7)
uint32_t rx_scenario_step_is_b(rx_addr sc) { return I32(sc, 0x151b4) == 0xb ? 1u : 0u; }
RX_FUNCTION(0x0055eb30, rx_scenario_step_is_b)

/* ---- 0074dd20 FMX / RaceIntroScenario::vf12: two calls of the shared empty virtual 007e3be0 (the second with +0x151c0 on the
 * stack, dead): no state. ---- */
void rx_intro_scenario_vf12(rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    { const rx_addr s = rs_site(0, NULL); rx_world_shared_vf_empty(); rx_set_sp(s); }
    { const uint32_t a[1] = {U32(sc, 0x151c0)};
      const rx_addr s = rs_site(1, a); rx_world_shared_vf_empty(); rx_set_sp(s); }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0074dd20, rx_intro_scenario_vf12)

/* ---- 007498a0 / 00749ba0 the crew chief / rider clip names (ECX = the player, stack index; RET 4): a tail jump through the
 * table at 0x7498f8 / 0x749be8 (indices 0..5; 0, 1, 2, 5 by the vehicle class, 3 the hero pair, 4 the vehicle's hero clip). ---- */
rx_addr rx_race_intro_chief_clip(rx_addr player, uint32_t index) {
    const uint32_t k = U32(U32(player, 0x1ac), 0x194);
    if (index <= 2 || index == 5) {
        if (k == 0) return 0x0096cb34;                        /* "MidResCrewChiefStartLineBike" */
        if (k == 1) return 0x0096cb54;                        /* "MidResCrewChiefStartLineATV" */
        return 0x0096cb70;                                    /* "MidResCrewChiefStartLineVehicle" */
    }
    if (index == 4) return 0x0096cba8;                        /* "HiResCrewChiefHeroVehicle" */
    return k <= 1 ? 0x0096cb90 : 0x0096cba8;                  /* "HiResCrewChiefHeroBike" / Vehicle */
}
RX_FUNCTION(0x007498a0, rx_race_intro_chief_clip)
rx_addr rx_race_intro_rider_clip(rx_addr player, uint32_t index) {
    const uint32_t k = U32(U32(player, 0x1ac), 0x194);
    if (index == 4) return 0x0096cbf8;                        /* "RiderHeroVehicle" */
    if (k == 0) return 0x0096cbd0;                            /* "RiderStartLineBike" */
    if (k == 1) return 0x0096cbe4;                            /* "RiderStartLineATV" */
    return 0x0096cbf8;                                        /* "RiderHeroVehicle" */
}
RX_FUNCTION(0x00749ba0, rx_race_intro_rider_clip)

/* ---- 00749fb0 RaceIntroScenario::vf5: 1, or 0 at step 6 when the "LoopScenario" registry flag is off. ---- */
uint32_t rx_race_intro_vf5(rx_addr sc) {
    if (I32(0, 0x00d61048) == 0 && I32(sc, 0x151b4) == 6) return 0;
    return 1;
}
RX_FUNCTION(0x00749fb0, rx_race_intro_vf5)

/* ---- 00749780 RaceIntroScenario::vf4 (skip to the last step): 1 (the step stays 6 when it is, or when the event kind is below
 * 2), else the step becomes 0 and it returns 0. ---- */
uint32_t rx_race_intro_vf4(rx_addr sc) {
    if (I32(sc, 0x151b4) != 6) {
        if (U32(U32(sc, 0x14), 0x23c) <= 1) { SU32(sc, 0x151b4, 6); return 1; }
        SU32(sc, 0x151b4, 0);
        return 0;
    }
    return 1;
}
RX_FUNCTION(0x00749780, rx_race_intro_vf4)

/* ---- 007497b0 RaceIntroScenario::vf3 (the next step): 1. Step 1 advances to 2 (3 for the event kind 0x11); 2 / 3 and 4 / 5
 * step the human index (4 / 5 to the next human, or 6 when none is left) and take the class step of that human's vehicle; step 6
 * sets the board's state +0x484 = 4 and ends (step 0, returns 0). ---- */
uint32_t rx_race_intro_vf3(rx_addr sc) {
    const rx_addr entry0 = rx_sp();
    if (I32(sc, 0x151b4) == I32(0, 0x00d610a0)) return 1;
    const rx_addr E = entry0 - 12u;                            /* push esi; push ebx; push ebp */
    rx_set_sp(E);
    const rx_addr act = U32(sc, 0x14);
    uint32_t index = 0;
    switch (I32(sc, 0x151b4)) {
    case 1:
        SU32(sc, 0x151b4, (U32(act, 0x23c) == 0x11 ? 1u : 0u) + 2u);
        rx_set_sp(entry0);
        return 1;
    case 2: case 3:
        index = 0;
        break;
    case 4: case 5: {
        uint32_t humans = 0;
        { const rx_addr s = rs_site(0, NULL); humans = rx_activity_count_humans_of(act, 1); rx_set_sp(s); }
        SU32(sc, 0x151c0, U32(sc, 0x151c0) + 1u);
        index = U32(sc, 0x151c0);
        if ((int32_t)index >= (int32_t)humans) {
            SU32(sc, 0x151b4, 6);
            rx_set_sp(entry0);
            return 1;
        }
        break;
    }
    case 6: {
        const rx_addr board = U32(U32(act, 0x234), 0x203e0);
        SU32(sc, 0x151b4, 0);
        if (board != 0) SU32(board, 0x484, 4);
        rx_set_sp(entry0);
        return 0;
    }
    default:
        rx_set_sp(entry0);
        return 1;
    }
    switch (I32(sc, 0x151b4)) {
    case 4: case 5: case 2: case 3: break;
    default: break;
    }
    {
        uint32_t d = 0, step = 0;
        const uint32_t a[1] = {index};
        const rx_addr s = rs_site(1, a);
        d = rx_activity_human_driver(act, (int32_t)index);
        rx_set_sp(s);
        { const rx_addr s2 = rs_site(0, NULL); step = rx_race_intro_class_step(U32(d, 0x74)); rx_set_sp(s2); }
        SU32(sc, 0x151b4, step);
    }
    rx_set_sp(entry0);
    return 1;
}
RX_FUNCTION(0x007497b0, rx_race_intro_vf3)

/* ---- 00749fd0 RaceIntroScenario::vf13 (the previous step): +0x151c0 = 0; over step 1: 2 / 3 -> 1, 4 / 5 -> 2, 6 -> 4 (5 for a
 * vehicle class above 1) with the human index reset; 1 when it moved, else 0. ---- */
uint32_t rx_race_intro_vf13(rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    uint32_t r = 0;
    SU32(sc, 0x151c0, 0);
    if (I32(sc, 0x151b4) > 1) {
        switch (I32(sc, 0x151b4)) {
        case 2: case 3:
            SU32(sc, 0x151b4, 1);
            r = 1;
            break;
        case 4: case 5:
            SU32(sc, 0x151c0, 0);
            SU32(sc, 0x151b4, 2);
            r = 1;
            break;
        case 6: {
            rx_addr d = 0;
            { const uint32_t a[1] = {0}; const rx_addr s = rs_site(1, a);
              d = rx_activity_human_driver(U32(sc, 0x14), 0); rx_set_sp(s); }
            SU32(sc, 0x151b4, (U32(U32(U32(d, 0x74), 0x1ac), 0x194) > 1 ? 1u : 0u) + 4u);
            SU32(sc, 0x151c0, 0);
            r = 1;
            break;
        }
        default: break;
        }
    }
    rx_set_sp(entry0);
    return r;
}
RX_FUNCTION(0x00749fd0, rx_race_intro_vf13)

/* ---- 0074eaf0 RaceIntro / WaypointIntroScenario::vf6: every driver's player entity (+0x1b4) hides (flags +0xa2 = 1); then the
 * riders unpause (00749590 with 0: the cast's states cleared). ---- */
void rx_intro_scenario_vf6(rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push esi; push edi */
    rx_set_sp(E);
    {
        const rx_addr world = U32(U32(sc, 0x14), 0x234);
        const rx_addr a = U32(world, 0x20190);
        const rx_addr sm = world + 0x20150;
        const int32_t n = I32(a, 0x26c);
        for (int32_t i = 0; i < n; i++) {
            const rx_addr d = U32(U32(sm, 0x40), 0x268);
            const rx_addr p = U32(U32(d, 4u * (uint32_t)i), 0x74);
            SU8(U32(p, 0x1b4), 0xa2, 1);
        }
    }
    { const rx_addr s = rs_site(0, NULL); rx_scenario_set_riders_paused(sc, 0); rx_set_sp(s); }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0074eaf0, rx_intro_scenario_vf6)

/* ---- 00749d00 RaceIntroScenario::vf0: +0x151c4 = 0; every driver's player entity shows (+0xa2 = 0); the podium riders' mud
 * cleared; +0x151c0 = 0; the sim flags set; the riders paused; the session static (once); then the first step: the series step
 * (00749c00) when the session is offline / the first player and the activity has no 0x1068 flag, else 6 for an event kind below
 * 2 / 0. Returns the step. ---- */
void rx_race_intro_vf0(rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ebx; push esi; push edi */
    rx_set_sp(E);
    SU32(sc, 0x151c4, 0);
    {
        const rx_addr world = U32(U32(sc, 0x14), 0x234);
        const rx_addr a = U32(world, 0x20190);
        const rx_addr sm = world + 0x20150;
        const int32_t n = I32(a, 0x26c);
        for (int32_t i = 0; i < n; i++) {
            const rx_addr d = U32(U32(sm, 0x40), 0x268);
            const rx_addr p = U32(U32(d, 4u * (uint32_t)i), 0x74);
            SU8(U32(p, 0x1b4), 0xa2, 0);
        }
        { const uint32_t a1[1] = {sm}; const rx_addr s = rs_site(1, a1); rx_podium_riders_clear_mud(sm); rx_set_sp(s); }
    }
    SU32(sc, 0x151c0, 0);
    { const rx_addr s = rs_site(0, NULL); rx_scenario_sim_flag(sc); rx_set_sp(s); }
    { const rx_addr s = rs_site(0, NULL); rx_scenario_set_riders_paused(sc, 1); rx_set_sp(s); }
    if ((U8(0, 0x00dcf304) & 1) == 0) {
        SU32(0, 0x00dcf304, U32(0, 0x00dcf304) | 1u);
        { const rx_addr s = rs_site(0, NULL); (void)rx_stats_static_construct(); rx_set_sp(s); }
        { RX_ATEXIT(0x00913e00); }   /* _atexit */
    }
    {
        const rx_addr sess = U32(0, 0x00da6cb0);
        const rx_addr act = U32(sc, 0x14);
        if (((U32(sess, 0x1c4c) == 0) || (U32(sess, 0x1b0) == 1)) && U32(act, 0x1068) == 0) {
            const rx_addr s = rs_site(0, NULL);
            const uint32_t step = rx_race_intro_series_step(sc);
            rx_set_sp(s);
            SU32(sc, 0x151b4, step);
        } else {
            SU32(sc, 0x151b4, U32(act, 0x23c) <= 1 ? 6u : 0u);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00749d00, rx_race_intro_vf0)

/* ---- 00749600 (EAX = the scenario, stack k; RET 4): the event cards: card i (the six names at 0x9dd254) shows its node
 * (+0xc bit 4) when bit i of `k` is set, in the CardGirl entity's hierarchy (+0x210) of the level cast. ---- */
static const uint32_t k_card_names[6] = {0x0096c5e8u, 0x0096c5d8u, 0x0096c5c8u, 0x0096c5b8u, 0x0096c5a8u, 0x0096c594u};

void rx_race_intro_card_mask(rx_addr sc, uint32_t k) {
    const rx_addr entry0 = rx_sp(), E8 = entry0 - 8u;          /* push esi; push edi */
    rx_set_sp(E8);
    rx_addr ent = 0;
    { const rx_addr s = rs_site(0, NULL);
      ent = rx_scenario_anims_find_entity(U32(U32(sc, 0x14), 0x234) + 0x20150, 0x0096c5f8 /* "CardGirl" */);
      rx_set_sp(s); }
    const uint32_t mask = 1u << (k & 0x1f);
    if (ent != 0) {
        const rx_addr E16 = entry0 - 16u;                      /* push ebx; push ebp */
        rx_set_sp(E16);
        const rx_addr h = ent + 0x210;                         /* the CardGirl's hierarchy */
        uint32_t bit = 1;
        for (uint32_t i = 0; i < 6; i++) {
            const rx_addr name = k_card_names[i];
            uint32_t has = 0;
            { const uint32_t a[1] = {name}; const rx_addr s = rs_site(1, a);
              has = rx_hierarchy_has_node(h, name); rx_set_sp(s); }
            if (has != 0) {
                rx_addr rec = 0;
                { const uint32_t a[1] = {name}; const rx_addr s = rs_site(1, a);
                  rec = rx_hierarchy_find_node(h, name); rx_set_sp(s); }
                const uint8_t b = U8(rec, 0xc);
                const uint8_t cl = (uint8_t)(((mask & bit) == 0 ? 1u : 0u) << 4);
                SU8(rec, 0xc, (uint8_t)(b ^ ((uint8_t)(cl ^ b) & 0x10)));
            }
            bit = (bit << 1) | (bit >> 31);
        }
        rx_set_sp(E8);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00749600, rx_race_intro_card_mask)

/* ---- 007496d0 (EAX = step, stack scenario; RET 4): steps 1..3 show the event card of the series event's kind (career step
 * 00749680) when the series manager's event is kind 1/2, else card 5. ---- */
void rx_race_intro_card_flags(uint32_t step, rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 8u;           /* push ecx; push esi */
    rx_set_sp(E);
    if ((int32_t)step <= 0 || (int32_t)step > 3) { rx_set_sp(entry0); return; }
    uint32_t v = 5;
    series_static();
    if (U32(0, 0x009e2318) == 2 || U32(rx_fx_series_manager(), 0x190) == 1) {
        const rx_addr sm = rx_fx_series_manager();
        uint32_t c = U32(U32(sm, 0x18c), 0xc);
        if (c == 2) c = U32(U32(sm, 0x18c), 0x10);
        if (c <= 7) v = rx_race_intro_career_step(c);
    }
    { const uint32_t a[1] = {v}; const rx_addr s = rs_site(1, a); rx_race_intro_card_mask(sc, v); rx_set_sp(s); }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x007496d0, rx_race_intro_card_flags)

/* ---- 00749b10 (EAX = step, stack scenario; RET 4): for steps 1..6 every player's scenario entity (the cast entry of the player
 * at [0xa946b8]+0x7460, kind 3) plays the rider clip 00749ba0(1) at time 0.05 (0074be10, then vf 0x4c). ---- */
void rx_race_intro_place_riders(uint32_t step, rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 16u;          /* push ebx; push ebp; push esi; push edi */
    rx_set_sp(E);
    if ((int32_t)step <= 0 || (int32_t)step > 6) { rx_set_sp(entry0); return; }
    const rx_addr sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    for (uint32_t i = 0; i < U8(U32(0, 0x00a946b8), 0x1541c); i++) {
        rx_addr p = 0, clip = 0, ent = 0;
        { const uint32_t a[2] = {i, 3}; const rx_addr s = rs_site(2, a);
          p = rx_world_players_find(U32(0, 0x00a946b8) + 0x7460, i, 3); rx_set_sp(s); }
        { const uint32_t a[1] = {1}; const rx_addr s = rs_site(1, a);
          clip = rx_race_intro_rider_clip(p, 1); rx_set_sp(s); }
        { const uint32_t a[1] = {p}; const rx_addr s = rs_site(1, a);
          ent = U32(rx_scenario_cast_find(sm, p), 0); rx_set_sp(s); }
        { const uint32_t a[4] = {ent, 0, clip, 0}; const rx_addr s = rs_site(4, a);
          rx_scenario_cast_place_entity(sm, 0, a[0], a[1], a[2], a[3]); rx_set_sp(s); }
        { const uint32_t a1 = rx_fb(0.05f /* 0x9d3bfc */); const rx_addr s = rs_site(1, &a1);
          world_vcall_f(ent, 0x4c, 0.05f); rx_set_sp(s); }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00749b10, rx_race_intro_place_riders)

/* ---- 00749910 (stack scenario, step; RET 8): the crew chief shots (steps 1..6): the cameras reset; needs at least as many
 * cameras as drivers; for steps 4 / 5 the hero human's camera, else the i-th camera. Each driver's chief entity takes the
 * driver's upright vehicle pose (the y column of its matrix +0x3c set to (0, 1, 0); for a vehicle the ground height of the
 * position, +0 y), plays the chief clip 007498a0(step - 1) through 0055e360. ---- */
void rx_race_intro_place_chiefs(rx_addr sc, uint32_t step) {
    const rx_addr entry0 = rx_sp();
    const rx_addr E = ((entry0 - 4u) & ~15u) - 0x64u - 12u;    /* push ebp; and esp,-16; sub esp,0x64; push ebx, esi, edi */
    rx_set_sp(E);
    const rx_addr act = U32(sc, 0x14);
    const int32_t n = I32(act, 0x26c);
    const rx_addr world = U32(act, 0x234), sm = world + 0x20150;
    const int chief = (step == 4 || step == 5);
    if ((int32_t)step <= 0 || (int32_t)step > 6) { rx_set_sp(entry0); return; }
    SU32(E, 0x1c, sm); SU32(E, 0x18, (uint32_t)n); SU32(E, 0x14, 0);      /* the original's locals (the dead stack too) */
    { const rx_addr s = rs_site(0, NULL); rx_scenario_anims_reset_cameras(sm); rx_set_sp(s); }
    if (I32(sm, 0x172c) < n) { rx_set_sp(entry0); return; }
    rx_addr human = 0;
    if (chief) { const rx_addr s = rs_site(0, NULL); human = rx_race_intro_human_player(sc); rx_set_sp(s); }
    SU32(E, 0x14, human);
    if (n <= 0) { rx_set_sp(entry0); return; }
    const rx_addr camtab = sm + 0x16ec;
    SU32(E, 0x10, camtab);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr cell = camtab + 4u * (uint32_t)i;      /* [esp+0x10], advanced by 4 at the end of each turn */
        SU32(E, 0x10, cell);
        const rx_addr p = U32(U32(U32(act, 0x268), 4u * (uint32_t)i), 0x74);
        const rx_addr cam = (p == human) ? U32(sm, 0x1730) : U32(cell, 0);
        if (cam == 0) { SU32(E, 0x10, cell + 4u); continue; }
        const rx_addr veh = U32(p, 0x1ac);
        { const uint32_t a[1] = {E + 0x30}; (void)rx_vcall(veh, 0x3c, 1, a); }      /* the vehicle's world matrix */
        SF32(E, 0x40, 0.0f); SF32(E, 0x44, 1.0f); SF32(E, 0x48, 0.0f);
        SF32(E, 0x34, 0.0f); SF32(E, 0x54, 0.0f);
        float y;
        if (U32(veh, 0x194) <= 1) {
            y = F32(E, 0x64);
        } else {
            SF32(E, 0x20, F32(E, 0x60)); SF32(E, 0x24, F32(E, 0x64)); SF32(E, 0x28, F32(E, 0x68));
            if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1u); SU32(0, 0x009e30dc, 0); }
            rx_addr w = 0;
            { const rx_addr s = rs_site(0, NULL); w = rx_world_find_by_controller(0x009e30d4); rx_set_sp(s); }
            if (w != 0) { const uint32_t a[1] = {E + 0x20}; const rx_addr s = rs_site(1, a);
                          uint32_t edx = 0;
                          (void)phys_terrain_base_height_at(w, E + 0x20, &edx); rx_set_sp(s); }
            SF32(E, 0x60, F32(E, 0x20)); SF32(E, 0x68, F32(E, 0x28));
            y = F32(E, 0x24);
        }
        /* `push ecx` (the matrix address) at 00749aaf lowers ESP by 4 before the store: 00749ab6's `movss
         * [esp+0x68], xmm1` writes the frame's +0x64, i.e. the pose's y, not its z. */
        SF32(E, 0x64, phys_add(F32(0, 0x009cb14c), y));
        { const uint32_t a[1] = {E + 0x30}; (void)rx_vcall(cam, 0x40, 1, a); }       /* the camera takes the pose */
        { const uint32_t a[1] = {cam}; const rx_addr s = rs_site(1, a);
          rx_crew_chief_set_shirt(veh, cam); rx_set_sp(s); }
        rx_addr clip = 0;
        if (step - 1 <= 5) {
            const uint32_t idx = step - 1;
            const uint32_t a[1] = {idx};
            const rx_addr s = rs_site(1, a);
            clip = rx_race_intro_chief_clip(p, idx);
            rx_set_sp(s);
        }
        { const uint32_t a[2] = {act, cam}; const rx_addr s = rs_site(2, a);
          rx_activity_setup_object(clip, act, cam); rx_set_sp(s); }
        SU32(E, 0x10, cell + 4u);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00749910, rx_race_intro_place_chiefs)

/* ---- 00749df0 RaceIntroScenario::vf1 (ECX = the scenario, stack step; RET 4): begin a step. The event cards (007496d0) and
 * the cast placement (00749910); the board's move state +0x484 = 2 for step 1, 3 and its setup (006b7fa0) for step 6; the riders
 * placed (00749b10); over step 3: steps 4 / 5 copy the hero actor's matrix (vf 0x38) to the level cast and, for the chest node
 * ("MXRider_Chest" in the actor's hierarchy vf 0x70), the chest's world translation, to the race camera matrix
 * (world +0x21d50); step 6 copies the board's matrix both places. ---- */
void rx_race_intro_vf1(rx_addr sc, uint32_t step) {
    const rx_addr entry0 = rx_sp();
    const rx_addr E = ((entry0 - 4u) & ~15u) - 0x54u - 12u;    /* push ebp; and esp,-16; sub esp,0x54; push ebx, esi, edi */
    rx_set_sp(E);
    { const uint32_t a[1] = {sc}; const rx_addr s = rs_site(1, a); rx_race_intro_card_flags(step, sc); rx_set_sp(s); }
    { const uint32_t a[2] = {sc, step}; const rx_addr s = rs_site(2, a);
      rx_race_intro_place_chiefs(sc, step); rx_set_sp(s); }
    {
        const rx_addr act = U32(sc, 0x14), world = U32(act, 0x234);
        const rx_addr board = U32(world, 0x203e0);
        if (board != 0) {
            if (step == 1) {
                SU32(board, 0x484, 2);
            } else if (step == 6) {
                SU32(board, 0x484, 3);
                { const rx_addr s = rs_site(0, NULL); rx_board_setup(board); rx_set_sp(s); }
            }
        }
        { const uint32_t a[1] = {sc}; const rx_addr s = rs_site(1, a);
          rx_race_intro_place_riders(step, sc); rx_set_sp(s); }
        if ((int32_t)step < 4) { rx_set_sp(entry0); return; }
        rx_addr src = 0;                                        /* the matrix copied to world + 0x21d50 */
        if ((int32_t)step <= 5) {
            rx_addr d = 0;
            { const uint32_t a[1] = {U32(sc, 0x151c0)};
              const rx_addr s = rs_site(1, a); d = rx_activity_human_driver(act, (int32_t)U32(sc, 0x151c0)); rx_set_sp(s); }
            const rx_addr p = U32(d, 0x74);
            const rx_addr ent = U32(rx_scenario_cast_find(world + 0x20150, p), 0);
            { const rx_addr m = rx_vcall(ent, 0x38, 0, NULL);     /* the actor's world matrix */
              rs_copy64(world + 0x20150, m);
              const rx_addr h = rx_vcall(ent, 0x70, 0, NULL);     /* its hierarchy */
              uint32_t has = 0;
              { const uint32_t a[1] = {0x009581fc}; const rx_addr s = rs_site(1, a);
                has = rx_hierarchy_has_node(h, 0x009581fc /* "MXRider_Chest" */); rx_set_sp(s); }
              if (has != 0) {
                  rx_addr rec = 0;
                  { const uint32_t a[1] = {0x009581fc}; const rx_addr s = rs_site(1, a);
                    rec = rx_hierarchy_find_node(h, 0x009581fc); rx_set_sp(s); }
                  const rx_addr m2 = rx_vcall(ent, 0x38, 0, NULL);
                  rs_copy64(E + 0x20, m2);
                  { const uint32_t a[1] = {0}; const rx_addr s = rs_site(1, a);
                    rx_world_node_evaluate_world(rec, 0); rx_set_sp(s); }
                  const rx_addr e = U8(rec, 0xb) * 0x40u + U32(U32(rec, 0), 0x1c);   /* movzx byte [rec+0xb] */
                  SU32(E, 0x50, U32(e, 0x30)); SU32(E, 0x54, U32(e, 0x34)); SU32(E, 0x58, U32(e, 0x38));
                  src = E + 0x20;
              } else {
                  src = m;
              }
            }
            rs_copy64(world + 0x21d50, src);
        } else {                                                /* step 6 */
            const rx_addr board = U32(world, 0x203e0);
            if (board == 0) { rx_set_sp(entry0); return; }
            const rx_addr m = rx_vcall(board, 0x38, 0, NULL);
            rs_copy64(world + 0x20150, m);
            rs_copy64(world + 0x21d50, m);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x00749df0, rx_race_intro_vf1)

/* ---- 00747810 Scenario_Start (ESI = the scenario; no RET) -> EAX: +0x151b0 (1 = started) --------------------------------------
 * +0x151b0 = 1, the clock +0x10 = 0, +0x151b8 = 0, the page wipe's duration +0x151bc from the UI property "PageWipeInDuration"
 * (005dc170: EAX = the name, [esp+4] the manager, [esp+8] the float*; a PRODUCT BOUNDARY, no UI store). vf0 picks the first
 * step; with one vf1 begins it and the intro script loads (00747750): when it does, the viewports (00757660) and the started
 * flag. Otherwise vf6 and the end: the level cast's actors / cameras / cast stopped, the 0x21884 block cleared, the board reset,
 * the pose overrides cleared; 0. */
static void intro_ui_property_float(rx_addr key, rx_addr out) {
#ifdef RX_ORACLE
    rx_regs r; memset(&r, 0, sizeof r);
    r.r[0] = key;                                              /* EAX: the property's name */
    const uint32_t a[2] = {U32(0, 0x00a93fcc), out};
    rx_call_original_ex(0x005dc170, &r, 0, 2, a);
#else
    (void)key; (void)out;   /* PRODUCT BOUNDARY: the UI property store is absent (build_ui_props.c); the property is not found and
                             * the output stays unwritten, as the original's store does for a property nobody set. Nothing reads
                             * the page wipe duration (no reader of +0x151bc in the exe). */
#endif
}
uint32_t rx_scenario_start(rx_addr sc) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ecx; push ebx; push edi */
    rx_set_sp(E);
    SU32(sc, 0x151b0, 1);
    SF32(sc, 0x10, 0.0f);
    SU32(sc, 0x151b8, 0);
    { const uint32_t a[2] = {U32(0, 0x00a93fcc), sc + 0x151bc}; const rx_addr s = rs_site(2, a);
      intro_ui_property_float(0x0096c674 /* "PageWipeInDuration" */, sc + 0x151bc); rx_set_sp(s); }
    (void)rx_vcall(sc, 0, 0, NULL);                             /* vf0 */
    if (U32(sc, 0x151b4) != 0) {
        const uint32_t step = U32(sc, 0x151b4);
        (void)rx_vcall(sc, 4, 1, &step);                        /* vf1(step) */
        uint32_t loaded = 0;
        { const rx_addr s = rs_site(0, NULL); loaded = rx_scenario_load_intro_script(sc, step); rx_set_sp(s); }
        if (loaded != 0) {
            SF32(sc, 0x151a8, 0.0f);
            SU32(sc, 0x151ac, 0);
#ifdef RX_ORACLE
            /* 00757660 (MXGame viewports): render setup, a PRODUCT BOUNDARY as in rx_scenario_intro_update. */
            { const uint32_t a[2] = {U32(0, 0x00a946b8), 1}; const rx_addr s = rs_site(2, a);
              rx_call_original(0x00757660, RX_STDCALL, 0, 2, a); rx_set_sp(s); }
#endif
            const uint32_t r = U32(sc, 0x151b0);
            rx_set_sp(entry0);
            return r;
        }
    }
    (void)rx_vcall(sc, 0x18, 0, NULL);                          /* vf6 */
    SU32(sc, 0x151b0, 0);
    SU32(sc, 0x151b4, 0);
    {
        const rx_addr world = U32(U32(sc, 0x14), 0x234);
        const rx_addr sm = world + 0x20150;
        { const rx_addr s = rs_site(0, NULL); rx_scenario_anims_stop_actors(sm); rx_set_sp(s); }
        { const rx_addr s = rs_site(0, NULL); rx_scenario_anims_reset_cameras(sm); rx_set_sp(s); }
        { const rx_addr s = rs_site(0, NULL); rx_scenario_cast_stop_all(sm); rx_set_sp(s); }
        for (uint32_t k = 0; k < 12; k++) { SU32(world, 0x21884 + 8u * k, 0); SU32(world, 0x21888 + 8u * k, 0); }
        const rx_addr board = U32(world, 0x203e0);
        if (board != 0) { SU8(board, 0xa2, 0); SU32(board, 0x70, 0); }
        { const rx_addr s = rs_site(0, NULL); rx_pose_overrides_clear(); rx_set_sp(s); }
    }
    const uint32_t r = U32(sc, 0x151b0);
    rx_set_sp(entry0);
    return r;
}
RX_FUNCTION(0x00747810, rx_scenario_start)

/* ---- The scenario script's event handlers (table 0x9c5120, called by rx_scenario_script_run with ECX = the activity, stack:
 *      event, +0x1518c flag; RET 8). The intro scripts (NAT_CO_AF_*: Intro / StartLine / HeroBike / 30SecGirl) use the types
 *      0 CameraAnimation, 1 SetScenarioOrigin, 6 ScenarioEnd, 11 FullScreenEffect, 18 DisableEntity, 19 PlaceEntity,
 *      21 SetNodeVisibility and 22 PlayTimeline. ---- */

/* 0063e7c0 (EDI = the camera path, stack the clip's name; RET 4): the path's label +0xdc = the name (0x40 bytes), its clip
 * (+0x11c) = the "anim" asset, its time +0x120 = the asset's +0x4c; the asset held (+0x4c = 1) while read. */
void rx_camera_path_set_clip(rx_addr e, rx_addr name) {
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(e, 0xdc + k, 0);
    for (uint32_t k = 0;; k++) { SU8(e, 0xdc + k, U8(name, k)); if (U8(name, k) == 0) break; }
    const rx_addr mgr = U32(U32(0, 0x00a95844), 8);
    rx_addr h = 0;
    { const uint32_t a[1] = {name}; const rx_addr s = rs_site(1, a);
      h = rx_database_vf_handle(mgr, name, 0x6d696e61 /* 'anim' */, 0, 1); rx_set_sp(s); }
    SU32(e, 0x11c, h);
    if (h != 0) {
        SU32(h, 0x4c, 1);
        SU32(e, 0x120, U32(U32(h, 0x44), 0x4c));
        SU32(U32(e, 0x11c), 0x4c, 0);
    }
}
RX_FUNCTION(0x0063e7c0, rx_camera_path_set_clip)

/* 0063d780 (EBX = the scenario camera, stack: the camera object, the clip's name; RET 8): the camera object's +0x3ac label =
 * "Scenario_Cam", then its path (+0x310) takes the clip (0063e7c0); the camera's +0x440 = 1. */
void rx_scenario_camera_setup(rx_addr camera, rx_addr name) {
    const rx_addr entry0 = rx_sp();
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(camera, 0x3ac + k, 0);
    for (uint32_t k = 0;; k++) { SU8(camera, 0x3ac + k, U8(0x00941e44 + k, 0)); if (U8(0x00941e44 + k, 0) == 0) break; }
    { const uint32_t a[1] = {name}; const rx_addr s = rs_site(1, a);
      rx_camera_path_set_clip(camera + 0x310, name); rx_set_sp(s); }
    SU32(camera, 0x440, 1);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0063d780, rx_scenario_camera_setup)

/* 0055d290 CameraAnimation (ECX = the activity, stack: event, flag; RET 8): the event's AnimName (+0x48) through the animation
 * alias table (006b6a30) and, as an "anim" asset, into the scenario camera's path (world +0x218f0: the camera object +0x3ac /
 * its path +0x310, 0063d780). */
void rx_scenario_event_camera(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ebx; push esi; push edi */
    rx_set_sp(E);
    const rx_addr camera = U32(act, 0x234) + 0x218f0;
    rx_addr name = 0;
    { const uint32_t a[1] = {ev + 0x48}; const rx_addr s = rs_site(1, a);
      name = rx_anim_alias_pick(0, 0, ev + 0x48); rx_set_sp(s); }
    if (name == 0) name = ev + 0x48;
    rx_addr h = 0;
    { const uint32_t a[4] = {name, 0x6d696e61 /* 'anim' */, 0, 1};
      const rx_addr s = rs_site(4, a);
      h = rx_database_vf_handle(U32(U32(0, 0x00a95844), 8), name, 0x6d696e61, 0, 1);
      rx_set_sp(s); }
    if (h != 0) {
        SU32(h, 0x4c, 1);
        if (U32(h, 0x44) != 0) { const uint32_t a[2] = {name, camera}; const rx_addr s = rs_site(2, a);
                                 rx_scenario_camera_setup(camera, name); rx_set_sp(s); }
        SU32(h, 0x4c, 0);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055d290, rx_scenario_event_camera)

/* 0055d300 SetScenarioOrigin (ECX = the activity, stack: event, flag; RET 8): the event's ModelName (+8) as a prop or any entity
 * of the level cast; its matrix (vf 0x38) to the race camera matrix (world +0x21d50) and to the cast (world +0x20150). */
void rx_scenario_event_origin(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push ebx */
    rx_set_sp(E);
    const rx_addr world = U32(act, 0x234), sm = world + 0x20150;
    rx_addr e = 0;
    { const uint32_t a[1] = {ev + 8}; const rx_addr s = rs_site(1, a);
      e = rx_scenario_anims_find_prop(sm, ev + 8); rx_set_sp(s); }
    if (e == 0) { const uint32_t a[1] = {ev + 8}; const rx_addr s = rs_site(1, a);
                  e = rx_scenario_anims_find_entity(sm, ev + 8); rx_set_sp(s); }
    if (e == 0) { rx_set_sp(entry0); return; }
    const rx_addr m = rx_vcall(e, 0x38, 0, NULL);
    rs_copy64(world + 0x21d50, m);
    const rx_addr m2 = rx_vcall(e, 0x38, 0, NULL);
    rs_copy64(sm, m2);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055d300, rx_scenario_event_origin)

/* 0055ddc0 DisableEntity (ECX = the activity, stack: event, flag; RET 8): the entity of ModelName (+8): its playing flag +0x70
 * = 0, then by its type (vf 0x58): hidden (+0xa2 = 0) for the types 4 / 0x103a / 0x103b / 0x104f / 0x1050 / 0x1079 / 0x1081;
 * for 0x1059 its +0x790 object stops (+0x794 = 1, vf 0x10's +0x1c = 0). */
void rx_scenario_event_disable(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ebx; push esi; push edi */
    rx_set_sp(E);
    const rx_addr sm = U32(act, 0x234) + 0x20150;
    rx_addr e = 0;
    { const uint32_t a[1] = {ev + 8}; const rx_addr s = rs_site(1, a);
      e = rx_scenario_anims_find_entity(sm, ev + 8); rx_set_sp(s); }
    if (e == 0) { rx_set_sp(entry0); return; }
    SU32(e, 0x70, 0);
    const uint32_t type = rx_vcall(e, 0x58, 0, NULL);
    if (type == 0x1059) {
        const rx_addr ai = U32(e, 0x790);
        SU32(e, 0x794, 1);
        if (ai != 0) {
            const rx_addr v = rx_vcall(ai, 0x10, 0, NULL);
            if (v != 0) { SU32(v, 0x1c, 0); SU8(e, 0xa2, 0); }
        }
    } else if (type == 4 || type == 0x103a || type == 0x103b || type == 0x104f || type == 0x1050 || type == 0x1079 ||
               type == 0x1081) {
        SU8(e, 0xa2, 0);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055ddc0, rx_scenario_event_disable)

/* 0055e0e0 PlayTimeline (ECX = the activity, stack: event, flag; RET 8): the activity's scripted sequence of TimelineName
 * (+8): 0055e100(activity, name, 0). */
void rx_scenario_event_timeline(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp();
    const uint32_t a[2] = {ev + 8, 0};
    const rx_addr s = rs_site(2, a);
    (void)rx_activity_start_sequence(act, ev + 8, 0);
    rx_set_sp(s);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055e0e0, rx_scenario_event_timeline)

/* ---- the intro's FX / crowd handlers -------------------------------------------------------------------------------------- */

/* 006924a0 (EAX = the triggered particle effect, xmm0 = the delay): both delays +0xb0 / +0x2a0 = the delay; +0x2a4 = the
 * effect's start time (its data record [+0xb8] * 0x1f0 + [[+0xb4]+0x44]+0x10, +0xc4 + +0xc8) + the delay; the asset +0xb4 held;
 * with a positive delay the effect's creator ([+0x90], vf 0x14) enabled (1). */
void rx_fx_triggered_effect_start(float delay, rx_addr e) {
    const rx_addr entry0 = rx_sp();
    const rx_addr E = entry0 - 8u;                             /* push esi; (the stack argument) */
    rx_set_sp(E);
    SF32(e, 0xb0, delay);
    SF32(e, 0x2a0, delay);
    const rx_addr rec = U32(e, 0xb4), data = U32(rec, 0x44);
    const rx_addr r = U32(data, 0x10) + U32(e, 0xb8) * 0x1f0u;
    SU32(rec, 0x4c, 1);
    SF32(e, 0x2a4, phys_add(phys_add(F32(r, 0xc8), F32(r, 0xc4)), delay));
    SU32(U32(e, 0xb4), 0x4c, 0);
    if (F32(e, 0x2a0) > 0.0f /* 0x92e12c */) {
        const uint32_t one = 1;
        (void)rx_vcall(e + 0x90, 0x14, 1, &one);   /* lea ecx, [eax + 0x90]: the controller object, not its vtable */
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x006924a0, rx_fx_triggered_effect_start)

/* 006744f0 (cdecl: name; RET 0) -> 1: the crowd reaction of the name: the 10 reaction names (0x931efc: "StartGate",
 * "EndRace", "4thTo3rd" .. "Wreck") by _stricmp; the index is the crowd event (00674540, world_wreck_misc.c). 1 always. */
uint32_t rx_world_crowd_event_name(rx_addr name) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ecx; push ebx; push esi; push edi */
    rx_set_sp(E);
    for (uint32_t i = 0; i < 10; i++)
        if (crt_stricmp(name, U32(0, 0x00931efc + 4u * i)) == 0) { rx_world_crowd_event((int32_t)i); break; }
    rx_set_sp(entry0);
    return 1;
}
RX_FUNCTION(0x006744f0, rx_world_crowd_event_name)

/* 0055d7b0 CrowdReaction (ECX = the activity (unused), stack: event, flag; RET 8): the event's CrowdEventName (+8), when not
 * empty, as a crowd reaction (006744f0). */
void rx_scenario_event_crowd(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)act; (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    const rx_addr name = ev + 8;
    if (name != 0 && U8(name, 0) != 0) {
        const uint32_t a[1] = {name}; const rx_addr s = rs_site(1, a);
        (void)rx_world_crowd_event_name(name);
        rx_set_sp(s);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055d7b0, rx_scenario_event_crowd)

/* the delay of a trigger event: ms (+0x110) * 0.0333333 s, at least 0.01 s (0x9322ac). */
static float trigger_delay(rx_addr ev) {
    const float t = phys_mul((float)I32(ev, 0x110), 0.033333335f /* 0x977470 */);
    return t > 0.009999999776482582f ? t : 0.009999999776482582f;
}

/* 0055d7e0 ParticleEffectTrigger (ECX = the activity, stack: event, flag; RET 8): the level's entity of type 0x102f named
 * Emitter (+8) starts its triggered particle effect (006924a0, delay); else the active activity's entity of type 0x102a named the
 * same plays (+0x70 = 1). */
void rx_scenario_event_particle(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 12u;          /* push ebx; push esi; push edi */
    rx_set_sp(E);
    const rx_addr world = U32(act, 0x234);
    rx_addr e = 0;
    { const uint32_t a[3] = {U32(world, 0x20138), 0x102f, ev + 8}; const rx_addr s = rs_site(3, a);
      e = rx_scene_manager_find_typed(a[0], a[1], a[2]); rx_set_sp(s); }
    if (e != 0) {
        const float d = trigger_delay(ev);
        const uint32_t a[1] = {rx_fb(d)}; const rx_addr s = rs_site(1, a);
        rx_fx_triggered_effect_start(d, e);
        rx_set_sp(s);
    } else {
        const rx_addr act2 = U32(world, 0x20190);
        if (act2 != 0) {
            rx_addr e2 = 0;
            { const uint32_t a[3] = {U32(U32(act2, 0x234), 0x20138), 0x102a, ev + 8}; const rx_addr s = rs_site(3, a);
              e2 = rx_scene_manager_find_typed(a[0], a[1], a[2]); rx_set_sp(s); }
            if (e2 != 0) SU32(e2, 0x70, 1);
        }
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055d7e0, rx_scenario_event_particle)

/* 0055d950 FireworkEffectTrigger (ECX = the activity, stack: event, flag; RET 8): the level's entity of type 0x102e named
 * Emitter (+8) starts its fireworks effect (006922c0, delay). */
void rx_scenario_event_firework(rx_addr act, rx_addr ev, uint32_t flag) {
    (void)flag;
    const rx_addr entry0 = rx_sp(), E = entry0 - 4u;           /* push esi */
    rx_set_sp(E);
    const rx_addr world = U32(act, 0x234);
    rx_addr e = 0;
    { const uint32_t a[3] = {U32(world, 0x20138), 0x102e, ev + 8}; const rx_addr s = rs_site(3, a);
      e = rx_scene_manager_find_typed(a[0], a[1], a[2]); rx_set_sp(s); }
    if (e != 0) {
        const float d = trigger_delay(ev);
        const uint32_t a[1] = {rx_fb(d)}; const rx_addr s = rs_site(1, a);
        rx_fx_fireworks_start(d, e);
        rx_set_sp(s);
    }
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0055d950, rx_scenario_event_firework)

#ifndef RX_ORACLE
/* 0055d610 SoundEffect (ECX = the activity, stack: event, flag; RET 8): the event's "Sound" name (+8), its "{...}" form through
 * 0078fde0 or the "Scenarios/All/<name>" form, played as a UI sound (Activity_UiKind 00556280 -> 0067edc0 -> the audio command).
 * AUDIO BOUNDARY (core/plan/QUEUE.md "chain round 3"): the whole effect is the sound engine's queue (0067edc0 -> Scenario_AudioCommand),
 * nothing gameplay reads it (no game state written: the name buffers are the handler's own frame); the oracle runs the original.
 * The product (w-frscene) keeps, OUTSIDE guest memory, what the original hands the sound engine, for the renderer's sound player
 * (rx_session_scenario_sounds): the position - the flag object's matrix (its vf14, a getter) when there is one, else the event's +0xd0 -
 * passed only when the flag object exists or the position is not (0, 0, 0) (0055d66d .. 0055d68f: else a 2D sound); a name not starting
 * with "{" (0055d69e _strnicmp) as it is when it holds a "/" (0078fcf0 with "/"), else "Scenarios/All/<name>" (0078fd60); empty names
 * play nothing (0055d77d). The "{...}" form (a string id 0078fde0 resolves, played through the UI kind 0067edc0) is not recorded: no
 * shipped scenario script uses it. The sound engine (0067ead0 "ScenarioSoundEffectEvent") resolves [FORMAT] / [LOC] / [PLATFORM]. */
#define RX_SCENARIO_SOUNDS 64
static struct { char name[128]; float pos[3]; int has_pos; } g_scenario_sounds[RX_SCENARIO_SOUNDS];
static uint32_t g_scenario_sound_head, g_scenario_sound_count;
void scenario_event_sound(rx_addr act, rx_addr ev, uint32_t flag) {
    rx_addr src = ev + 0xd0;
    if (flag) src = rx_vcall(flag, 0x38, 0, NULL);
    const float x = F32(src, 0x30), y = F32(src, 0x34), z = F32(src, 0x38);
    const int has_pos = flag != 0 || !(x == 0.0f && y == 0.0f && z == 0.0f);
    const rx_addr name = ev + 8;
    rx_vehicle_audio_scenario_sound(name);   /* w-audio: the "{...}" sound-layer commands (EnableStartLineState ...) as cues (game/vehicle_audio.c; reads only) */
    {   /* w-sfx: 0067edc0 -> Scenario_AudioCommand's music commands on the player's state (game/music_player.c): the activity kind (Activity_UiKind,
         * 0x3e -> 8) for StartScenarioMusic (00677750: its draw of the game's LCG), StopScenarioMusic -> 00677600 */
        char cmd[40]; uint32_t k = 0;
        if (U8(name, 0) == '{') { for (; k < sizeof cmd - 1; k++) { const char c = (char)U8(name, k + 1); if (!c || c == '}' || c == '|' || c == ' ') break; cmd[k] = c; } }
        cmd[k] = 0;
        if (!strcasecmp(cmd, "StartScenarioMusic")) { uint32_t kind = rx_activity_ui_kind(act); if (kind == 0x3e) kind = 8; rx_music_start_score(kind); }
        else if (!strcasecmp(cmd, "StopScenarioMusic")) rx_music_fade_out();
    }
    if (U8(name, 0) == '{') return;
    char raw[0x41]; uint32_t n = 0;
    while (n < 0x40 && U8(name, n)) { raw[n] = (char)U8(name, n); n++; }
    raw[n] = 0;
    if (!n) return;
    char out[128];
    if (strchr(raw, '/')) snprintf(out, sizeof out, "%s", raw); else snprintf(out, sizeof out, "Scenarios/All/%s", raw);
    const uint32_t slot = (g_scenario_sound_head + g_scenario_sound_count) % RX_SCENARIO_SOUNDS;
    if (g_scenario_sound_count == RX_SCENARIO_SOUNDS) g_scenario_sound_head = (g_scenario_sound_head + 1) % RX_SCENARIO_SOUNDS;
    else g_scenario_sound_count++;
    snprintf(g_scenario_sounds[slot].name, sizeof g_scenario_sounds[slot].name, "%s", out);
    g_scenario_sounds[slot].pos[0] = x; g_scenario_sounds[slot].pos[1] = y; g_scenario_sounds[slot].pos[2] = z;
    g_scenario_sounds[slot].has_pos = has_pos;
}
/* the recorded scenario sounds, oldest first (removed from the record); returns how many were written */
int rx_scenario_sounds_take(char (*names)[128], float (*pos)[3], int32_t* has_pos, int max) {
    int n = 0;
    while (g_scenario_sound_count && n < max) {
        const uint32_t k = g_scenario_sound_head;
        memcpy(names[n], g_scenario_sounds[k].name, 128);
        pos[n][0] = g_scenario_sounds[k].pos[0]; pos[n][1] = g_scenario_sounds[k].pos[1]; pos[n][2] = g_scenario_sounds[k].pos[2];
        has_pos[n] = g_scenario_sounds[k].has_pos;
        g_scenario_sound_head = (k + 1) % RX_SCENARIO_SOUNDS; g_scenario_sound_count--; n++;
    }
    return n;
}
void rx_scenario_sounds_clear(void) { g_scenario_sound_head = g_scenario_sound_count = 0; }
RX_FUNCTION(0x0055d610, scenario_event_sound)
#endif

/* 0074d170 FreerideIntroScenario::vf0: hide the live riders, attach each
 * podium actor to its vehicle, pause the cast and choose the first intro step. */
void rx_freeride_intro_vf0(rx_addr sc) {
    const rx_addr sp0=rx_sp();rx_set_sp(sp0-0x18);
    SU32(sc,0x151c0,0);
    const rx_addr activity=U32(sc,0x14),world=U32(activity,0x234),cast=world+0x20150;
    const rx_addr live=U32(world,0x20190);
    for(int32_t i=0;i<I32(live,0x26c);i++) {
        const rx_addr player=U32(U32(U32(live,0x268),4*(uint32_t)i),0x74);
        SU8(U32(player,0x1b4),0xa2,0);
    }
    SU32(sc,0x151c4,0);
    for(int32_t i=0;i<I32(activity,0x26c);i++) {
        const rx_addr player=U32(U32(U32(activity,0x268),4*(uint32_t)i),0x74);
        rx_addr entry=0;
        for(uint32_t k=0;k<RX_RC_MAX;k++) {   /* (w-rc-core) entries 12..31 in side storage */
            const rx_addr candidate=rx_cast_rec_ro(cast,k);
            if(!candidate||!U32(candidate,4))break;
            if(U32(candidate,4)==player){entry=candidate;break;}
        }
        const uint32_t arg[1]={U32(entry,0)};const rx_addr site=rs_site(1,arg);
        rx_scenario_anims_attach(U32(player,0x1ac),cast,arg[0]);rx_set_sp(site);
    }
    {const rx_addr site=rs_site(0,NULL);rx_scenario_sim_flag(sc);rx_set_sp(site);}
    {const rx_addr site=rs_site(0,NULL);rx_scenario_set_riders_paused(sc,1);rx_set_sp(site);}
    if(!(U8(0,0x00dcf304)&1)) {
        SU32(0,0x00dcf304,U32(0,0x00dcf304)|1);
        {const rx_addr site=rs_site(0,NULL);(void)rx_stats_static_construct();rx_set_sp(site);}
        RX_ATEXIT(0x00913e00);
    }
    const rx_addr session=U32(0,0x00da6cb0);
    SU32(sc,0x151b4,U32(session,0x1c4c)!=0&&U32(session,0x1b0)!=1?0:1);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074d170, rx_freeride_intro_vf0)

/* 0074d060 FreerideIntroScenario::vf4: first overview can advance immediately. */
uint32_t rx_freeride_intro_advance(rx_addr sc) {
    if(U32(sc,0x151b4)==1) (void)rx_vcall(sc,0xc,0,NULL);
    return 1;
}
RX_FUNCTION(0x0074d060, rx_freeride_intro_advance)
/* 0074d440 FreerideIntroScenario::vf5: rider closeups (4,5,13) cannot skip. */
uint32_t rx_freeride_intro_can_skip(rx_addr sc) {
    const int32_t step=I32(sc,0x151b4);
    return !(step>3&&(step<6||step==13));
}
RX_FUNCTION(0x0074d440, rx_freeride_intro_can_skip)
