/* anim_scenario_fmx.c -- the Freestyle intro scenario FMXIntroScenario (vtable 0x0094410c; the activity's +0xff080): the intro
 * cinematic of a Freestyle event (step 1 the start line with the crew chiefs, 4 / 5 the hero shot of each human rider, then the
 * event), the FMX twin of RaceIntroScenario (anim_scenario_race_intro.c, whose shared members and helpers it reuses). The scenario
 * object: +0x14 the activity, +0x151b4 the step, +0x151c0 the human driver shown, +0x151c4. The level cast is the world's +0x20150
 * (+0x40 the activity, +0x16ec / +0x16f0 the two crew chief cameras, +0x1730 the hero camera); world +0x21d50 the race camera.
 *
 * The finish and results scenarios (FMXFinishScenario 0x0094414c, FMXResultsScenario 0x0094418c) are in anim_scenario_finish.c.
 * Adapters in core/oracle/race_freestyle.cpp (group rx_anim). */
#include "anim_scenario_fmx.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim_scenario_race_intro.h"
#include "race.h"
#include "race_extern.h"
#include "race_activity.h"
#include "race_driver.h"
#include "world_node.h"
#include "build_activity_util.h"

/* a virtual call at the current stack depth */
static uint32_t vcall0(rx_addr obj, uint32_t slot) { return race_thiscall(race_vfn(obj, slot), obj, 0, 0, 0, 0, 0); }
static uint32_t vcall1(rx_addr obj, uint32_t slot, uint32_t a) { return race_thiscall(race_vfn(obj, slot), obj, 1, a, 0, 0, 0); }

/* `rep movsd` of 0x10 dwords */
static void copy64(rx_addr dst, rx_addr src) {
    for (uint32_t o = 0; o < 0x40; o += 4) SU32(dst, o, U32(src, o));
}

/* the player of the activity's n-th human driver (00559e60 with EAX = the activity, stack n) */
static rx_addr human_player(rx_addr act, int32_t n) {
    rx_addr d;
    RACE_CORE_CALL(1, d = rx_activity_human_driver(act, n));
    return U32(d, 0x74);
}
/* the number of human drivers of player id 1 (00559e20 with EDX = the activity, EDI = 1) */
static int32_t human_count(rx_addr act) {
    uint32_t n;
    RACE_CORE_CALL(0, n = rx_activity_count_humans_of(act, 1));
    return (int32_t)n;
}

/* 0074da40 (ECX = the player; stack k; RET 4) -> EAX: the rider clip of the intro step k + 1 by the vehicle's class (+0x1ac ->
 * +0x194: 0 bike, 1 ATV, else a vehicle): 0, 1, 5: "RiderStartLineBike" / "...ATV" / "RiderStartLineVehicle"; 3: "RiderStartLineBike" /
 * "...ATV" / "RiderHeroVehicle"; 4: "RiderStartLineVehicle"; 2 returns EAX as it came (the callers clear it: 0). */
rx_addr rx_fmx_rider_clip(rx_addr player, uint32_t k) {
    const uint32_t kind = U32(U32(player, 0x1ac), 0x194);
    switch (k) {
    case 0: case 1: case 5:
        if (kind == 0) return 0x0096cbd0;                         /* "RiderStartLineBike" */
        if (kind == 1) return 0x0096cbe4;                         /* "RiderStartLineATV" */
        return 0x0096ccc4;                                        /* "RiderStartLineVehicle" */
    case 3:
        if (kind == 0) return 0x0096cbd0;
        if (kind == 1) return 0x0096cbe4;
        return 0x0096cbf8;                                        /* "RiderHeroVehicle" */
    case 4:
        return 0x0096ccc4;
    default:
        return 0;
    }
}
RX_FUNCTION(0x0074da40, rx_fmx_rider_clip)

/* 0074d7a0 (ECX = step, EDX = the player) -> EAX: the crew chief clip of the step by the vehicle's class: step 1 "MidResCrewChief
 * StartLineBike" / "...ATV" / "...Vehicle", step 4 "HiResCrewChiefHeroBike" / "...ATV" / "...Vehicle", else 0 */
rx_addr rx_fmx_chief_clip(uint32_t step, rx_addr player) {
    const uint32_t kind = U32(U32(player, 0x1ac), 0x194);
    if (step == 1) {
        if (kind == 0) return 0x0096cb34;
        if (kind == 1) return 0x0096cb54;
        return 0x0096cb70;
    }
    if (step == 4) {
        if (kind == 0) return 0x0096cb90;
        if (kind == 1) return 0x0096ccdc;
        return 0x0096cba8;
    }
    return 0;
}
RX_FUNCTION(0x0074d7a0, rx_fmx_chief_clip)

/* 0074d800 (EDI = the player, ESI = the crew chief entity; stdcall scenario, clip; RET 8): the chief stands at the player's vehicle,
 * upright (its world matrix (vf15) with the up row (0, 1, 0), the other rows' y = 0) and 0.25 m higher, given to the chief (vf16); the
 * clip played (0055e360) and the shirt of the vehicle's team (006bd760). Frame: push ebp; and esp, -16; sub 0x40 (the matrix at +0). */
void rx_fmx_place_chief(rx_addr player, rx_addr chief, rx_addr sc, rx_addr clip) {
    RACE_ORIG_FRAME_ENTER(F, 0x40, 0);
    vcall1(U32(player, 0x1ac), 15, F);                            /* the vehicle's world matrix */
    SF32(F, 0x10, 0.0f);
    SF32(F, 0x18, 0.0f);
    SF32(F, 4, 0.0f);
    SF32(F, 0x24, 0.0f);
    const float y = F32(F, 0x34) + 0.25f;                         /* 0x92ff84 */
    SF32(F, 0x14, 1.0f);
    SF32(F, 0x34, y);
    vcall1(chief, 16, F);
    RACE_CORE_CALL(2, rx_activity_setup_object(clip, U32(sc, 0x14), chief));
    RACE_CORE_CALL(1, rx_crew_chief_set_shirt(U32(player, 0x1ac), chief));
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0074d800, rx_fmx_place_chief)

/* 0074d890 (stdcall scenario, step; RET 8): the crew chiefs of the step: the cast's cameras reset (0074b480); the first human's chief
 * (camera +0x16ec; at step 4 the hero camera +0x1730 when the shown human +0x151c0 is the first) and, with two humans, the second's
 * (+0x16f0, or the hero camera at step 4 when he is shown), each placed with the step's clip (0074d7a0 / 0074d800). Frame: push ecx /
 * ebx / ebp / esi / edi (the first player in the scenario argument's slot, a 0 at +0x10). */
void rx_fmx_place_chiefs(rx_addr sc, uint32_t step) {
    const rx_addr entry = rx_sp();
    RACE_FRAME_PLAIN(F, 0, 5);
    const rx_addr sm = U32(U32(sc, 0x14), 0x234) + 0x20150;
    RACE_CORE_CALL(0, rx_scenario_anims_reset_cameras(sm));
    const rx_addr act = U32(sc, 0x14);
    const rx_addr p0 = human_player(act, 0);
    SU32(entry, 4, p0);
    rx_addr cam0 = U32(sm, 0x16ec);
    if (step == 4) {
        SU32(F, 0x10, 0);
        rx_addr shown = 0;
        if (I32(sc, 0x151c0) < human_count(act)) shown = human_player(act, I32(sc, 0x151c0));
        if (shown == p0) cam0 = U32(sm, 0x1730);
    }
    rx_addr clip;
    RACE_CORE_CALL(0, clip = rx_fmx_chief_clip(step, p0));
    RACE_CORE_CALL(2, rx_fmx_place_chief(p0, cam0, sc, clip));
    if (human_count(U32(sc, 0x14)) > 1) {
        const rx_addr p1 = human_player(U32(sc, 0x14), 1);
        rx_addr cam1 = U32(sm, 0x16f0);
        if (step == 4) {
            rx_addr hero;
            RACE_CORE_CALL(0, hero = rx_race_intro_human_player(sc));
            if (hero == p1) cam1 = U32(sm, 0x1730);
        }
        RACE_CORE_CALL(0, clip = rx_fmx_chief_clip(step, p1));
        RACE_CORE_CALL(2, rx_fmx_place_chief(p1, cam1, sc, clip));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0074d890, rx_fmx_place_chiefs)

/* 0074d980 (stdcall scenario, step; RET 8): the riders of the step: the first human's cast entity placed with its rider clip
 * (0074da40 with step - 1 when it is 0 .. 5, else none; 0074be10), the second human's likewise (avoiding [0xa942ec]). Frame: push
 * ebx / ebp / esi / edi (the clip in the step argument's slot). */
void rx_fmx_place_riders(rx_addr sc, uint32_t step) {
    const rx_addr entry = rx_sp();
    RACE_FRAME_PLAIN(F, 0, 4);
    const rx_addr act = U32(sc, 0x14);
    const rx_addr p0 = human_player(act, 0);
    const uint32_t k = step - 1;
    rx_addr clip0 = 0;
    if (k <= 5) RACE_CORE_CALL(1, clip0 = rx_fmx_rider_clip(p0, k));
    SU32(entry, 8, clip0);
    const rx_addr sm = U32(act, 0x234) + 0x20150;
    rx_addr entry0;
    RACE_CORE_CALL(1, entry0 = rx_scenario_cast_find(sm, p0));
    RACE_CORE_CALL(4, rx_scenario_cast_place_entity(sm, 0, U32(entry0, 0), 0, U32(entry, 8), 0));
    const rx_addr act2 = U32(sc, 0x14);
    if (human_count(act2) > 1) {
        const rx_addr p1 = human_player(act2, 1);
        const rx_addr sm2 = U32(act2, 0x234) + 0x20150;
        rx_addr entry1;
        RACE_CORE_CALL(1, entry1 = rx_scenario_cast_find(sm2, p1));
        const rx_addr ent1 = U32(entry1, 0);
        rx_addr clip1 = 0;
        if (k <= 5) RACE_CORE_CALL(1, clip1 = rx_fmx_rider_clip(p1, k));
        RACE_CORE_CALL(4, rx_scenario_cast_place_entity(sm2, U32(0, 0x00a942ec), ent1, 0, clip1, 0));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0074d980, rx_fmx_place_riders)

/* 0074c5e0 (ESI = the level cast): every local human's rider entity (+0x1b4) shown (+0xa2 = 1) */
void rx_cast_show_local_riders(rx_addr sm) {
    const int32_t n = I32(U32(sm, 0x40), 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr p = U32(U32(U32(U32(sm, 0x40), 0x268), 4 * (uint32_t)i), 0x74);
        if (U32(p, 4) != 0 && U32(p, 0x10) == 0) SU8(U32(p, 0x1b4), 0xa2, 1);
    }
}
RX_FUNCTION(0x0074c5e0, rx_cast_show_local_riders)

/* 0074dab0 FMXIntroScenario::vf0 (ECX = scenario): the start: +0x151c4 = 0; every human's rider entity hidden (+0xa2 = 0); the podium
 * riders' mud cleared (0074bb90); the simulations flagged (00749550); the riders paused (00749590 with 1); +0x151c0 = 0; the first
 * human's vehicle attached to its cast entity's animations (0074b080 / 0074af60); the first step 1 offline (session +0x1c4c / +0x1b0)
 * without the UI flag +0x1068, else 0. Frame: push ebx / esi (edi, ebp inside). */
void rx_fmx_intro_vf0(rx_addr sc) {
    const rx_addr entry = rx_sp();
    rx_set_sp(entry - 12);                                        /* push ebx; push esi; push edi */
    SU32(sc, 0x151c4, 0);
    const rx_addr world = U32(U32(sc, 0x14), 0x234);
    const rx_addr sm = world + 0x20150;
    const int32_t n = I32(U32(world, 0x20190), 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr p = U32(U32(U32(U32(sm, 0x40), 0x268), 4 * (uint32_t)i), 0x74);
        if (U32(p, 0x10) == 0) SU8(U32(p, 0x1b4), 0xa2, 0);
    }
    rx_set_sp(entry - 16);                                        /* push ebp */
    RACE_CORE_CALL(1, rx_podium_riders_clear_mud(U32(U32(sc, 0x14), 0x234) + 0x20150));
    RACE_CORE_CALL(0, rx_scenario_sim_flag(sc));
    RACE_CORE_CALL(0, rx_scenario_set_riders_paused(sc, 1));
    const rx_addr act = U32(sc, 0x14);
    SU32(sc, 0x151c0, 0);
    const rx_addr p0 = human_player(act, 0);
    const rx_addr cast = U32(act, 0x234) + 0x20150;
    rx_addr e;
    RACE_CORE_CALL(1, e = rx_scenario_cast_find(cast, p0));
    RACE_CORE_CALL(1, rx_scenario_anims_attach(U32(p0, 0x1ac), cast, U32(e, 0)));
    rx_set_sp(entry - 12);
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    const rx_addr session = U32(0, 0x00da6cb0);
    const int offline = session == 0 || U32(session, 0x1c4c) == 0 || U32(session, 0x1b0) == 1;
    if (offline && U32(U32(sc, 0x14), 0x1068) == 0) SU32(sc, 0x151b4, 1);
    else SU32(sc, 0x151b4, 0);
    rx_set_sp(entry);
}
RX_FUNCTION(0x0074dab0, rx_fmx_intro_vf0)

/* 0074dbc0 FMXIntroScenario::vf1 (ECX = scenario; step; RET 4): begin a step: the riders (0074d980) and the crew chiefs (0074d890);
 * step 4 (the hero shot): the shown human's cast entity's world matrix (vf14) copied to the level cast and, through its hierarchy's
 * (vf28) "MXRider_Chest" node (the world matrix's translation replaced by the chest's, 007db890), to the race camera (world
 * +0x21d50). Frame: push ebp; and esp, -16; sub 0x54; push ebx / esi / edi (the scenario at +0x18, the matrix / node at +0x1c, the
 * chest matrix at +0x20). */
void rx_fmx_intro_vf1(rx_addr sc, uint32_t step) {
    RACE_ORIG_FRAME_ENTER(F, 0x54, 3);
    SU32(F, 0x18, sc);
    RACE_CORE_CALL(2, rx_fmx_place_riders(sc, step));
    RACE_CORE_CALL(2, rx_fmx_place_chiefs(sc, step));
    if (step == 4) {
        const rx_addr act = U32(sc, 0x14);
        const rx_addr p = human_player(act, I32(sc, 0x151c0));
        rx_addr e;
        RACE_CORE_CALL(1, e = rx_scenario_cast_find(U32(act, 0x234) + 0x20150, p));
        const rx_addr ent = U32(e, 0);
        const rx_addr m = vcall0(ent, 14);
        copy64(U32(U32(sc, 0x14), 0x234) + 0x20150, m);
        SU32(F, 0x1c, m);
        const rx_addr h = vcall0(ent, 28);
        uint32_t has;
        RACE_CORE_CALL(1, has = rx_hierarchy_has_node(h, 0x009581fc /* "MXRider_Chest" */));
        rx_addr src;
        if (has != 0) {
            rx_addr rec;
            RACE_CORE_CALL(1, rec = rx_hierarchy_find_node(h, 0x009581fc));
            SU32(F, 0x1c, rec);
            const rx_addr m2 = vcall0(ent, 14);
            copy64(F + 0x20, m2);
            RACE_CORE_CALL(1, rx_world_node_evaluate_world(rec, 0));
            const rx_addr wm = (uint32_t)U8(rec, 0xb) * 0x40u + U32(U32(rec, 0), 0x1c);
            SU32(F, 0x50, U32(wm, 0x30));
            SU32(F, 0x54, U32(wm, 0x34));
            SU32(F, 0x58, U32(wm, 0x38));
            src = F + 0x20;
        } else {
            src = U32(F, 0x1c);
        }
        copy64(U32(U32(U32(F, 0x18), 0x14), 0x234) + 0x21d50, src);
    }
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0074dbc0, rx_fmx_intro_vf1)

/* 0074d700 FMXIntroScenario::vf3 (ECX = scenario) -> EAX: the next step: the debug hold step [0xd610a0] stays (1); step 1 -> 4 with the
 * first human (+0x151c0 = 0); step 4: the next human (+0x151c0 + 1) or, after the last, step 0 (returns 0); his step is 5 for a
 * vehicle class above 1, else 4. Other steps stay (1). Frame: push esi (ebx / ebp / edi). */
uint32_t rx_fmx_intro_vf3(rx_addr sc) {
    const rx_addr entry = rx_sp();
    const uint32_t step = U32(sc, 0x151b4);
    if (step == U32(0, 0x00d610a0)) return 1;
    rx_set_sp(entry - 16);                                        /* push esi; push ebx; push ebp; push edi */
    uint32_t r = 1;
    if (step == 1) {
        SU32(sc, 0x151b4, 4);
        SU32(sc, 0x151c0, 0);
    } else if (step == 4) {
        const rx_addr act = U32(sc, 0x14);
        const int32_t n = human_count(act);
        SU32(sc, 0x151c0, U32(sc, 0x151c0) + 1);
        if (I32(sc, 0x151c0) >= n) {
            SU32(sc, 0x151b4, 0);
            r = 0;
        } else {
            const rx_addr p = human_player(act, I32(sc, 0x151c0));
            SU32(sc, 0x151b4, U32(U32(p, 0x1ac), 0x194) > 1 ? 5u : 4u);
        }
    }
    rx_set_sp(entry);
    return r;
}
RX_FUNCTION(0x0074d700, rx_fmx_intro_vf3)

/* 0074dcf0 FMXIntroScenario::vf6 (ECX = scenario): the end: the local riders shown (0074c5e0), the riders unpaused (00749590 with 0).
 * Frame: push esi / edi. */
void rx_fmx_intro_vf6(rx_addr sc) {
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(0, rx_cast_show_local_riders(U32(U32(sc, 0x14), 0x234) + 0x20150));
    RACE_CORE_CALL(0, rx_scenario_set_riders_paused(sc, 0));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0074dcf0, rx_fmx_intro_vf6)
