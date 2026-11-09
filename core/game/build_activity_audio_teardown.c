/* build_activity_audio_teardown.c -- the activity audio logic's teardown (ActivityAudioLogic::vf30 00580f80, RaceActivityAudioLogic
 * ::vf30 005825f0 / vf4 005829b0): the mirror of build_activity_audio.c's vf29 / vf3 subscriptions. Activity::vf3 calls vf30 through
 * the activity's +0xfd8; every handler the logic subscribed leaves its event (the handler-unsubscription templates 0058b010 ...,
 * act_unlisten in build_activity_teardown.c). The audio command callbacks vf29 added stay (the original does not remove them). */
#include "build_activity_teardown.h"
#include "build_activity_int.h"
#include "world_wreck.h"
#include "rx_vcall.h"
#include "activity_cues.h"

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */

/* ---- the templates (frame: aligned = push ebp; and esp, -8; sub esp, 0x14; push esi, else sub esp, 0x14; push esi) ---- */
/* 0058b010 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(Activity*, int) handler (proxy 0x945da4) */
void rx_audio_unlisten_state(rx_addr channel, rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, channel, name, 0x00945da4, 0x009cf5b4, self, method);
}
RX_FUNCTION(0x0058b010, rx_audio_unlisten_state)
/* 0058ad70 (ECX = name; stdcall logic, method; RET 8): a stunt handler on the stunts channel [0xa94304] (proxy 0x945d44) */
void rx_audio_unlisten_stunt(rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, U32(0, 0x00a94304), name, 0x00945d44, 0x009cbf38, self, method);
}
RX_FUNCTION(0x0058ad70, rx_audio_unlisten_stunt)
/* 0058add0 (ECX = name; stdcall logic, method; RET 8): a physics-stunt handler on the stunts channel (proxy 0x945d64) */
void rx_audio_unlisten_physics_stunt(rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, U32(0, 0x00a94304), name, 0x00945d64, 0x009ce768, self, method);
}
RX_FUNCTION(0x0058add0, rx_audio_unlisten_physics_stunt)
/* 0058aef0 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(RavenVehicle*) wreck handler (proxy 0x945d84) */
void rx_audio_unlisten_wreck(rx_addr channel, rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(0, channel, name, 0x00945d84, 0x009cdb7c, self, method);
}
RX_FUNCTION(0x0058aef0, rx_audio_unlisten_wreck)
/* 0058b850 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(Activity*, BaseDriver*) cue handler (proxy 0x945e24) */
void rx_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(0, channel, name, 0x00945e24, 0x009cda18, self, method);
}
RX_FUNCTION(0x0058b850, rx_audio_unlisten_driver)
/* 0058b370 (EAX = channel; stdcall logic; RET 4): "Restart" -> 00582d70 (proxy 0x945e04) */
void rx_audio_unlisten_restart(rx_addr channel, rx_addr self) {
    act_unlisten(1, channel, 0x00941ba0, 0x00945e04, 0x009cbd24, self, 0x00582d70);
}
RX_FUNCTION(0x0058b370, rx_audio_unlisten_restart)
/* 0058b8b0 / 0058b910 / 0058b970 / 0058b9d0 / 0058ba30 (EAX = channel; stdcall logic; RET 4): the race cue handlers of vf3 */
void rx_audio_unlisten_best_lap(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x0093ef0c /* "Best Lap" */, 0x00945e44, 0x009cda88, self, 0x00582e00);
}
RX_FUNCTION(0x0058b8b0, rx_audio_unlisten_best_lap)
void rx_audio_unlisten_lapped(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x00941c5c /* "VehicleGettingLapped" */, 0x00945e64, 0x009cdb7c, self, 0x00582e20);
}
RX_FUNCTION(0x0058b910, rx_audio_unlisten_lapped)
void rx_audio_unlisten_player_wins(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x00941ca8 /* "Player Wins" */, 0x00945e84, 0x009cf630, self, 0x00583fb0);
}
RX_FUNCTION(0x0058b970, rx_audio_unlisten_player_wins)
void rx_audio_unlisten_eliminated(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x0093ef48 /* "Player Eliminated" */, 0x00945ea4, 0x009cdaa8, self, 0x00583a70);
}
RX_FUNCTION(0x0058b9d0, rx_audio_unlisten_eliminated)
void rx_audio_unlisten_timed_section(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x00941710 /* "TimedSectionEntered" */, 0x00945ec4, 0x009ce550, self, 0x00582e60);
}
RX_FUNCTION(0x0058ba30, rx_audio_unlisten_timed_section)

/* a 2-argument template called at the original's depth: the arguments pushed (logic, method) */
static void unl2(void (*fn)(rx_addr, rx_addr, rx_addr, rx_addr), rx_addr channel, rx_addr name, rx_addr l, rx_addr method) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, method); SU32(sp, -8, l);
    ACT_CALL(2, fn(channel, name, l, method));
}

/* 00581e20 (ESI = logic): the twelve "<state> Entered / Exited" handlers of 00581cf0 leave (0058b010) */
void rx_audio_unlisten_states(rx_addr l) {
    static const uint32_t k[12][2] = {
        {0x009427a4, 0x00592a60}, {0x009427b0, 0x00592ac0}, {0x009427bc, 0x00592b30}, {0x009427d0, 0x00592b80},
        {0x009427e4, 0x00592a70}, {0x009427f8, 0x00592ad0}, {0x0094280c, 0x00592b40}, {0x00942820, 0x00592b90},
        {0x00942830, 0x00592a80}, {0x00942840, 0x00592ae0}, {0x0094284c, 0x00592b50}, {0x00942860, 0x00592ba0}};
    for (int i = 0; i < 12; i++) unl2(rx_audio_unlisten_state, ACH(l), k[i][0], l, k[i][1]);
}
RX_FUNCTION(0x00581e20, rx_audio_unlisten_states)

/* 00581b10 (ESI = logic): the five wreck handlers of 00581a90 leave (0058aef0). Frame: push ebp; and esp, -8. */
void rx_audio_unlisten_wrecks(rx_addr l) {
    static const uint32_t k[5][2] = {{0x0093ee38, 0x005c3300}, {0x00941278, 0x00592bc0}, {0x00941260, 0x00592ab0},
                                     {0x009416fc, 0x00592b20}, {0x009416e8, 0x005c3310}};
    ACT_FRAME_EBP(F, 8, 0, 0);
    for (int i = 0; i < 5; i++) unl2(rx_audio_unlisten_wreck, ACH(l), k[i][0], l, k[i][1]);
    ACT_END(F);
}
RX_FUNCTION(0x00581b10, rx_audio_unlisten_wrecks)

/* ActivityAudioLogic::vf30 00580f80 (thiscall): +0x58 = 0; the state handlers (00581e20), "Countdown Step" / "FinishCountdown Step"
 * (0058b010), the stunt handlers ("StuntBegin" / "StuntCompleted" 0058ad70, "PhysicsStuntBegin" / "PhysicsStuntEnd" 0058add0) and
 * the wreck handlers (00581b10) leave; then a jump to the class's vf4 (its cue handlers). Frame: push esi (popped before the jump). */
void rx_audio_logic_vf30(rx_addr l) {
    ACT_FRAME(F, 0, 1);
    SU32(l, 0x58, 0);
    ACT_CALL(0, rx_audio_unlisten_states(l));
    unl2(rx_audio_unlisten_state, ACH(l), 0x00941bdc /* "Countdown Step" */, l, 0x00592a90);
    unl2(rx_audio_unlisten_state, ACH(l), 0x00941bec /* "FinishCountdown Step" */, l, 0x00592af0);
    {
        const rx_addr sp = rx_sp();
        SU32(sp, -4, 0x00592b00); SU32(sp, -8, l);
        ACT_CALL(2, rx_audio_unlisten_stunt(0x0095ba28 /* "StuntBegin" */, l, 0x00592b00));
        SU32(sp, -4, 0x00592aa0); SU32(sp, -8, l);
        ACT_CALL(2, rx_audio_unlisten_stunt(0x00942784 /* "StuntCompleted" */, l, 0x00592aa0));
        SU32(sp, -4, 0x00592b60); SU32(sp, -8, l);
        ACT_CALL(2, rx_audio_unlisten_physics_stunt(0x009607a0 /* "PhysicsStuntBegin" */, l, 0x00592b60));
        SU32(sp, -4, 0x00592b10); SU32(sp, -8, l);
        ACT_CALL(2, rx_audio_unlisten_physics_stunt(0x00942794 /* "PhysicsStuntEnd" */, l, 0x00592b10));
    }
    ACT_CALL(0, rx_audio_unlisten_wrecks(l));
    ACT_END(F);
    /* mov ecx, esi; pop esi; jmp [vtable + 0x10]: vf4 entered at this function's entry ESP */
    const rx_addr target = U32(U32(l, 0), 0x10);
    void (*vf4)(rx_addr) = (void (*)(rx_addr))rx_fn(target);
    if (vf4) vf4(l);
    else rx_call_original(target, RX_THISCALL, l, 0, NULL);
}
RX_FUNCTION(0x00580f80, rx_audio_logic_vf30)

/* the cue map (+0x8c, std::multimap<std::string, Cue*>) holds a cue of the name: StringMap_Find (00702ad0) != end (the string at
 * L + 0x1c, the iterator at L + 0x14) */
static int cue_present(rx_addr l, rx_addr L, rx_addr name) {
    const rx_addr s = L + 0x1c, it = L + 0x14;
    crt_str_ctor_cstr(s, name);
    const rx_addr m = U32(l, 0x8c);
    const rx_addr end = U32(m, 0x18), proxy = U32(m, 0);
    rx_addr r;
    SU32(L, -4, s); SU32(L, -8, it);
    ACT_CALLR(r, 2, rx_world_string_map_find(m, it, s));
    if (U32(r, 0) == 0 || U32(r, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(r, 4) != end;
    crt_str_dtor(s);
#ifndef RX_ORACLE
    if (!found) found = rx_activity_cues_present(l, name);   /* w-sfx: the level's cues (activity_cues.c): the handlers vf3 subscribed leave */
#endif
    return found;
}
/* RaceActivityAudioLogic::vf4 005829b0 (thiscall): the cue handlers vf3 (00582610) subscribed leave, one per cue the map holds:
 * "EndOfRace" / "TwoLapsLeft" / "OneLapLeft" / "Holeshot" (0058b850), "Best Lap" 0058b8b0, "VehicleGettingLapped" 0058b910,
 * "Player Wins" 0058b970, "Player Eliminated" 0058b9d0, "TimedSectionEntered" 0058ba30.
 * Frame: push ebp; and esp, -8; sub esp, 0x28; push ebx / ebp / esi / edi. */
void rx_race_audio_logic_vf4(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        if (cue_present(l, L, 0x00941c9c)) unl2(rx_audio_unlisten_driver, ACH(l), 0x00941c9c /* "EndOfRace" */, l, 0x00582da0);
        if (cue_present(l, L, 0x00941c74)) unl2(rx_audio_unlisten_driver, ACH(l), 0x00941c74 /* "TwoLapsLeft" */, l, 0x00582dc0);
        if (cue_present(l, L, 0x00941c80)) unl2(rx_audio_unlisten_driver, ACH(l), 0x00941c80 /* "OneLapLeft" */, l, 0x00582de0);
        if (cue_present(l, L, 0x0093ef0c)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_best_lap(ACH(l), l)); }
        if (cue_present(l, L, 0x00941c5c)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_lapped(ACH(l), l)); }
        if (cue_present(l, L, 0x00941ca8)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_player_wins(ACH(l), l)); }
        if (cue_present(l, L, 0x0093ef48)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_eliminated(ACH(l), l)); }
        if (cue_present(l, L, 0x00942044)) unl2(rx_audio_unlisten_driver, ACH(l), 0x00942044 /* "Holeshot" */, l, 0x00582e40);
        if (cue_present(l, L, 0x00941710)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_timed_section(ACH(l), l)); }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005829b0, rx_race_audio_logic_vf4)

/* 0058b130 (EAX = channel; stdcall logic; RET 4): the "ChallengeAwardAchieved" handler 00592bb0 (proxy 0x945dc4, signature 0x9ce508) that
 * FreeRideActivityAudioLogic::vf3 (0058b070) subscribed leaves the event */
void rx_audio_unlisten_challenge_award(rx_addr channel, rx_addr self) {
    act_unlisten(0, channel, 0x00942870 /* "ChallengeAwardAchieved" */, 0x00945dc4, 0x009ce508, self, 0x00592bb0);
}
RX_FUNCTION(0x0058b130, rx_audio_unlisten_challenge_award)

/* FreeRideActivityAudioLogic::vf4 005823b0 (thiscall): the cue handler vf3 subscribed leaves when the cue map holds "ChallengeAwardAchieved"
 * (0058b130). Frame: push ebp; and esp, -8; sub esp, 0x28; push ebx / ebp / esi / edi. */
void rx_freeride_audio_logic_vf4(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        if (cue_present(l, L, 0x00942870)) { SU32(L, -4, l); ACT_CALL(1, rx_audio_unlisten_challenge_award(ACH(l), l)); }
    }
    ACT_END(L);
}
RX_FUNCTION(0x005823b0, rx_freeride_audio_logic_vf4)

/* FreeRideActivityAudioLogic::vf30 00582260: a jump to ActivityAudioLogic::vf30 (00580f80) */
void rx_freeride_audio_logic_vf30(rx_addr l) { rx_audio_logic_vf30(l); }
RX_FUNCTION(0x00582260, rx_freeride_audio_logic_vf30)

/* RaceActivityAudioLogic::vf30 005825f0 (thiscall): the base vf30, then "Restart" leaves (0058b370) when the logic has an activity.
 * Frame: push esi. */
void rx_race_audio_logic_vf30(rx_addr l) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_audio_logic_vf30(l));
    if (U32(l, 0x50) != 0) {
        SU32(F, -4, l);
        ACT_CALL(1, rx_audio_unlisten_restart(U32(U32(l, 0x50), 0x290), l));
    }
    ACT_END(F);
}
RX_FUNCTION(0x005825f0, rx_race_audio_logic_vf30)
