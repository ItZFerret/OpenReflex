/* race_section_driver.c -- SectionRaceDriver (vtable 0x0093fd14; 0x1170 bytes): the RaceDriver of the Section Race, hand-transliterated
 * from the original (the address of each function is in its comment).
 *
 * The driver adds to RaceDriver (0x1130 bytes):
 *   +0x1134  the sections it holds the best time of (the standings key of MXActivitySectionRace::vf43)
 *   +0x1138  the section count (the activity's +0x13e650)
 *   +0x113c  the section the driver is in (-1 none)
 *   +0x1140  float[sections]  the running time in each section (reset to 0 on entering the section)
 *   +0x1144  float[sections]  its best time in each section (FLT_MAX = none)
 *   +0x1168  how often it took a section's best time (the standings tie break)
 * It listens to "VehicleEnteredSection" on the trigger registry [0xa89bc8] (the SectionRaceZone volumes post it with the vehicle, the
 * section index and 1 on entering / 0 on leaving): 00513080 times the driver's pass through the section. "SectionWinner" /
 * "SectionLost" (void(SectionRaceDriver*, int)) tell the HUD and the audio who took / lost a section's best time. */
#include "race_section.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "build_activity_freeride_run.h"
#include "race_activity_teardown.h"
#include "race_driver.h"
#include "race_driver_reset.h"
#include "race_extern.h"
#include "race.h"
#include "fx.h"

#define FLT_MAX_BITS 0x7f7fffffu   /* [0x00977494] */

void rx_waypoint_network_vf14(rx_addr driver);   /* NetworkDriver::vf14 0050ec30 (waypoint_driver_network.c) */

static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}

/* 00512db0 (ESI = driver, EAX = player): RaceDriver's constructor (0050ed80), the section fields, vtable 0x0093fd14, +0xee8 = 4 (the
 * driver kind), no section yet. No frame. */
rx_addr rx_section_driver_construct(rx_addr d, rx_addr player) {
    ACT_CALL(0, (void)rx_race_driver_construct(d, player));
    SU32(d, 0x1134, 0);
    SU32(d, 0x1138, 0);
    SU32(d, 0x1140, 0);
    SU32(d, 0x1144, 0);
    SU32(d, 0x1168, 0);
    SU32(d, 0x116c, 0);
    SU32(d, 0, 0x0093fd14);
    SU32(d, 0xee8, 4);
    SU32(d, 0x113c, 0xffffffffu);
    return d;
}
RX_FUNCTION(0x00512db0, rx_section_driver_construct)

/* 0053ace0 (stdcall driver; RET 4): the driver listens to "VehicleEnteredSection" (the trigger registry [0xa89bc8]; proxy 0x00940350:
 * void(RavenVehicle*, int, int)) with 00513080. Frame: push ebx / esi. */
void rx_section_driver_listen(rx_addr d) {
    act_subscribe(0x0053ace0, 0, 1, 2, U32(0, 0x00a89bc8), 0x00936160 /* "VehicleEnteredSection" */, 0x00940350, 0x009cbbdc, d, 0x00513080);
}
RX_FUNCTION(0x0053ace0, rx_section_driver_listen)

/* 0053ada0 (stdcall driver; RET 4): the listener of 0053ace0 leaves. Frame: sub esp, 0x14; push esi. */
void rx_section_driver_unlisten(rx_addr d) {
    act_unlisten(0, U32(0, 0x00a89bc8), 0x00936160, 0x00940350, 0x009cbbdc, d, 0x00513080);
}
RX_FUNCTION(0x0053ada0, rx_section_driver_unlisten)

/* 00512e00 slot 1 (ECX = driver): the section count of the activity, the two time arrays (all FLT_MAX), the listener of the sections,
 * then a jump to RaceDriver::vf1. Frame: push esi. */
void rx_section_driver_vf1(rx_addr d) {
    ACT_FRAME(F, 0, 1);
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    SU32(d, 0x1134, 0);
    SU32(d, 0x1138, U32(act, 0x13e650));
    /* the allocation sizes are 4 * count, -1 on overflow (the compiler's new[]) */
    uint64_t bytes = (uint64_t)U32(d, 0x1138) * 4;
    rx_addr running, best;
    ACT_CALLR(running, 1, rx_reflex_aligned_alloc(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes));
    SU32(d, 0x1140, running);
    bytes = (uint64_t)U32(d, 0x1138) * 4;
    ACT_CALLR(best, 1, rx_reflex_aligned_alloc(bytes >> 32 ? 0xffffffffu : (uint32_t)bytes));
    SU32(d, 0x1144, best);
    for (int32_t i = 0; i < I32(d, 0x1138); i++) {
        SU32(U32(d, 0x1140), 4u * (uint32_t)i, FLT_MAX_BITS);
        SU32(U32(d, 0x1144), 4u * (uint32_t)i, FLT_MAX_BITS);
    }
    ACT_CALL(1, rx_section_driver_listen(d));
    ACT_END(F);
    rx_race_driver_vf1(d);
}
RX_FUNCTION(0x00512e00, rx_section_driver_vf1)

/* 00512eb0 slot 2 (ECX = driver): the release. Under the engine's profiler scope (edi) a networked driver's player state is cleared
 * (as RaceDriver::vf2), the two time arrays are freed, the section listener leaves and RaceDriver::vf2 runs.
 * Frame: push ebp; and esp, -8; push esi / edi. */
void rx_section_driver_vf2(rx_addr d) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    session_static();
    const rx_addr scope = race_profile_begin();
    const rx_addr net = U32(d, 0xef0);
    if (net != 0 && (U8(net, 0x20) & 1) != 0) {
        uint32_t valid;
        ACT_CALLR(valid, 0, rx_net_ref_valid(net) & 0xff);
        if (valid != 0 && U32(U32(U32(d, 0xef0), 0x1d4), 0) != 0) ACT_CALL(0, rx_net_player_state_clear(U32(U32(U32(d, 0xef0), 0x1d4), 0)));
    }
    if (U32(d, 0x1140) != 0) {
        ACT_CALL(1, rx_reflex_aligned_free(U32(d, 0x1140)));
        SU32(d, 0x1140, 0);
    }
    if (U32(d, 0x1144) != 0) {
        ACT_CALL(1, rx_reflex_aligned_free(U32(d, 0x1144)));
        SU32(d, 0x1144, 0);
    }
    ACT_CALL(1, rx_section_driver_unlisten(d));
    ACT_CALL(0, rx_race_driver_vf2(d));
    race_profile_end(scope);
    ACT_END(F);
}
RX_FUNCTION(0x00512eb0, rx_section_driver_vf2)

/* 00512fc0 slot 3 (ECX = driver): the reset. RaceDriver::vf3, then no section, every running and best time FLT_MAX, no section won,
 * no leader mark. Frame: push esi. */
void rx_section_driver_vf3(rx_addr d) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_race_driver_vf3(d));
    SU32(d, 0x113c, 0xffffffffu);
    for (int32_t i = 0; i < I32(d, 0x1138); i++) {
        SU32(U32(d, 0x1140), 4u * (uint32_t)i, FLT_MAX_BITS);
        SU32(U32(d, 0x1144), 4u * (uint32_t)i, FLT_MAX_BITS);
    }
    SU32(d, 0x1168, 0);
    SU32(d, 0x1134, 0);
    ACT_END(F);
}
RX_FUNCTION(0x00512fc0, rx_section_driver_vf3)

/* 00513020 slot 7 (ECX = driver; dt; RET 4): the update. A remote driver (the player's +0x908) updates as a NetworkDriver; else as a
 * RaceDriver, and the running time of the section the driver is in grows by dt.
 * Frame: push ebp; and esp, -8; push ecx / esi; the dt is loaded and stored through x87 for the callee. */
void rx_section_driver_vf7(rx_addr d, float dt) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    uint32_t bits = rx_fb(dt);
    if ((bits & 0x7f800000u) == 0x7f800000u && (bits & 0x7fffffu)) bits |= 0x400000u;   /* fld / fstp quiets a signalling NaN */
    const float loaded = rx_bf(bits);
    if (U32(U32(d, 0x74), 0x908) != 0) {
        ACT_CALL(1, rx_network_driver_update(d, loaded));
        ACT_END(F);
        return;
    }
    ACT_CALL(1, rx_race_driver_update(d, loaded));
    if (I32(d, 0x113c) > -1) {
        const rx_addr cell = U32(d, 0x1140) + 4u * U32(d, 0x113c);
        SF32(cell, 0, rx_adds(F32(cell, 0), dt));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00513020, rx_section_driver_vf7)

/* 005133f0 (ECX = driver; stdcall out index, out value; RET 8): the driver's best section time: *index = -1 and *value = -1.0 (the
 * float at 0x9cd9e8) without a time array, else the lowest time of the array (the first one on ties; FLT_MAX and index -1 when the
 * driver has none). The original scans four times at once; the order of the comparisons is the sequential one. */
void rx_section_driver_best(rx_addr d, rx_addr out_index, rx_addr out_value) {
    const uint32_t minus_one = U32(0, 0x009cd9e8);
    const rx_addr times = U32(d, 0x1144);
    SU32(out_index, 0, 0xffffffffu);
    SU32(out_value, 0, minus_one);
    if (times == 0) return;
    float best = rx_bf(FLT_MAX_BITS);
    int32_t index = -1;
    for (int32_t i = 0; i < I32(d, 0x1138); i++) {
        const float t = F32(times, 4u * (uint32_t)i);
        if (best > t) {   /* comiss; jbe */
            best = t;
            index = i;
        }
    }
    SF32(out_value, 0, best);
    SU32(out_index, 0, (uint32_t)index);
}
RX_FUNCTION(0x005133f0, rx_section_driver_best)

/* 0053aea0 (ESI = manager, EDX = event name, ECX = value; stdcall driver; RET 4): the event void(SectionRaceDriver*, int) raised on the
 * activity's channel: payload {driver, value}. Frame: sub esp, 0xc; push edi (the /GS cookie at E - 4, the payload at E - 0xc). */
void rx_section_event_driver_int(rx_addr manager, rx_addr name, rx_addr driver, int32_t value) {
    ACT_FRAME(F, 0xc, 1);
    const rx_addr payload = F + 4;   /* E - 0xc */
    SU32(payload, 0, driver);
    SU32(payload, 4, (uint32_t)value);
    act_raise(manager, name, 0x009cdad8, payload, 8);
    ACT_END(F);
}
RX_FUNCTION(0x0053aea0, rx_section_event_driver_int)

/* 00513080 (thiscall driver; vehicle, section, entering; RET 0xc): the "VehicleEnteredSection" handler. It counts for the driver whose
 * vehicle (the player's +0x1ac) it is, while the race runs (+0x21c == 2), unless the driver is off the track or finished: the first
 * rule (type 1: +0x50) or the off-track penalty rule (type 0xb: +0x10) must be set (default set) and the finish rule (type 0: +0xc)
 * clear. Entering a section sets the driver's current section and its running time there to 0. Leaving the section it is in:
 * a networked session sends the time (ONLINE: not offline); else when the time beats the driver's best in the section the best
 * takes it and, if it also beats the section's best (the activity's +0x13e654), the driver gets a leader mark (+0x1168) and
 * "SectionWinner" is raised for it and "SectionLost" for the previous leader; the section is left (-1).
 * Frame: push ebp; and esp, -8; sub esp, 0xc; push ebx / esi / edi; the default flag at F + 0xc, 0 at F + 0x10. */
void rx_section_driver_entered(rx_addr d, rx_addr vehicle, int32_t section, int32_t entering) {
    ACT_FRAME_EBP(F, 8, 0xc, 3);
    SU32(F, 0xc, 1);   /* the default "counts" flag */
    SU32(F, 0x10, 0);
    rx_addr rule_advance, rule_penalty, rule_finish;
    ACT_CALLR(rule_advance, 1, rx_driver_rule_1(d));
    ACT_CALLR(rule_penalty, 1, rx_driver_offtrack_penalty_rule(d));
    ACT_CALLR(rule_finish, 1, rx_driver_rule_0(d));
    uint32_t counts = 1;
    if (rule_advance != 0) counts = U32(rule_advance, 0x50);
    else if (rule_penalty != 0) counts = U32(rule_penalty, 0x10);
    const uint32_t finished = rule_finish != 0 ? U32(rule_finish, 0xc) : U32(F, 0x10);
    if (counts == 0 || finished != 0) { ACT_END(F); return; }
    const rx_addr player = U32(d, 0x74);
    if (U32(player, 0x1ac) != vehicle) { ACT_END(F); return; }
    const rx_addr act = U32(player, 0x94);
    if (U32(act, 0x21c) != 2) { ACT_END(F); return; }
    if (entering != 0) {
        SU32(d, 0x113c, (uint32_t)section);
        SF32(U32(d, 0x1140), 4u * (uint32_t)section, 0.0f);
        ACT_END(F);
        return;
    }
    if (I32(d, 0x113c) == -1 || section != I32(d, 0x113c)) { ACT_END(F); return; }
    {   /* the session manager (006de1c0) and its session: a networked session with the host flag off sends the time */
        rx_addr mgr;
        ACT_CALLR(mgr, 0, rx_fx_session_manager());
        uint32_t networked;
        ACT_CALLR(networked, 0, rx_session_is_networked(U32(mgr, 0x10)));
        if (networked != 0) {
            ACT_CALLR(mgr, 0, rx_fx_session_manager());
            if (U32(U32(mgr, 0x10), 0x1c3c) == 0) act_unreached(0x00513179, "ONLINE: a networked section race's section time (NetSectionRace update)");
        }
    }
    const int32_t at = I32(d, 0x113c);
    const float time = F32(U32(d, 0x1140), 4u * (uint32_t)at);
    const rx_addr best_cell = U32(d, 0x1144) + 4u * (uint32_t)at;
    if (F32(best_cell, 0) > time) {   /* comiss; jbe */
        SF32(best_cell, 0, time);
        const rx_addr act2 = U32(U32(d, 0x74), 0x94);
        const int32_t idx = I32(d, 0x113c);
        const float section_best = F32(U32(act2, 0x13e654), 4u * (uint32_t)idx);
        const float run_time = F32(U32(d, 0x1140), 4u * (uint32_t)idx);
        if (section_best > run_time) {   /* comiss; jbe */
            const rx_addr leader = U32(U32(act2, 0x13e658), 4u * (uint32_t)idx);
            SU32(d, 0x1168, U32(d, 0x1168) + 1);
            const rx_addr manager = U32(U32(U32(d, 0x74), 0x94), 0x290);
            ACT_CALL(1, rx_section_event_driver_int(manager, 0x0093ef5c /* "SectionWinner" */, d, idx));
            if (leader != 0 && leader != d) {
                const rx_addr manager2 = U32(U32(U32(d, 0x74), 0x94), 0x290);
                ACT_CALL(1, rx_section_event_driver_int(manager2, 0x0093ef6c /* "SectionLost" */, leader, I32(d, 0x113c)));
            }
        }
    }
    SU32(d, 0x113c, 0xffffffffu);
    ACT_END(F);
}
RX_FUNCTION(0x00513080, rx_section_driver_entered)

/* 005132e0 slot 17 (ECX = driver): the driver leaves the race: every section it led loses its leader (the time FLT_MAX, no leader) and
 * "SectionLost"-style notice void(SectionRaceDriver*, int) {0, section} under "SectionWinner"'s name (the HUD clears the leader),
 * then vf3 (the reset); a networked driver whose network object (+0xb2c of the player) is a remote one (flag 8 or without the 0x138
 * byte) is cleared (lap -1 / best FLT_MAX when it is not finished, distance 0); then NetworkDriver::vf14.
 * Frame: sub esp, 0x10; push ebp / esi / edi (+ ebx while sections are walked). */
void rx_section_driver_vf17(rx_addr d) {
    ACT_FRAME(F, 0x10, 3);
    const rx_addr act = U32(U32(d, 0x74), 0x94);
    SU32(F, 0xc, d);   /* [esp + 0xc] (the cookie's slot) holds the driver */
    if (I32(act, 0x13e650) > 0) {
        const rx_addr F4 = F - 4;   /* push ebx */
        rx_set_sp(F4);
        for (int32_t s = 0; s < I32(act, 0x13e650); s++) {
            if (U32(U32(act, 0x13e658), 4u * (uint32_t)s) != d) continue;
            SU32(U32(act, 0x13e654), 4u * (uint32_t)s, FLT_MAX_BITS);
            SU32(U32(act, 0x13e658), 4u * (uint32_t)s, 0);
            const rx_addr manager = U32(act, 0x290);
            const rx_addr payload = F4 + 0x14;
            SU32(payload, 0, 0);
            SU32(payload, 4, (uint32_t)s);
            act_raise(manager, 0x0093ef5c /* "SectionWinner" */, 0x009cdad8, payload, 8);
        }
        rx_set_sp(F);   /* pop ebx */
    }
    act_vcall(d, 0xc, 0, 0);   /* vf3 */
    const rx_addr net = U32(U32(d, 0x74), 0xb2c);
    if (net != 0 && ((U8(net, 0x128) & 8) != 0 || U8(net, 0x138) == 0)) {
        if (U32(d, 0x70) == 0) {
            SU32(d, 0xf1c, 0xffffffffu);
            SU32(d, 0x10c4, FLT_MAX_BITS);
        }
        SF32(d, 0x10cc, 0.0f);
    }
    ACT_CALL(0, rx_waypoint_network_vf14(d));
    ACT_END(F);
}
RX_FUNCTION(0x005132e0, rx_section_driver_vf17)


/* 005125c0 slot 12 (ECX = driver) -> EAX: the driver's best lap (+0x1118) takes the activity's best (+0x10c4 of the activity at +0x24)
 * when that is lower; 1 when it changed. */
uint32_t rx_race_driver_vf12(rx_addr d) {
    const float best = F32(U32(d, 0x24), 0x10c4);
    if (best < F32(d, 0x1118)) {
        SF32(d, 0x1118, best);
        return 1;
    }
    return 0;
}
RX_FUNCTION(0x005125c0, rx_race_driver_vf12)

/* 005125f0 slot 13 (ECX = driver) -> st0: a ghost's (player kind 2) best lap +0x1118, else the driver's own +0x10c4. */
float rx_race_driver_vf13(rx_addr d) {
    if (U32(U32(d, 0x74), 0x10) == 2) return F32(d, 0x1118);
    return F32(d, 0x10c4);
}
RX_FUNCTION(0x005125f0, rx_race_driver_vf13)
