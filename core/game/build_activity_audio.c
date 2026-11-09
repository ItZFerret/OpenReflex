/* build_activity_audio.c -- construction unit 11: the activity audio logic's start (lead's assignment 2026-10-19: the crowd
 * reactions draw the game's random generator, core/notes/race.md "ActivityAudioLogic"). RaceActivityAudioLogic::vf29 00582ed0 ->
 * ActivityAudioLogic::vf29 00580e70: the logic's activity (+0x50), the audio command callbacks "CrowdReact" / "CrowdSize" in the
 * audio system's command map (00817f70), the cue map (+0x8c: the activity audio manager's [0x00d92f14] +4 / +8 set + 0x94 when the
 * scene has one, else an own empty multimap), the handlers of the state events (00581cf0), the countdown steps, the stunts
 * (005817e0), the wrecks (00581a90), the class's cue handlers (vf3 00582610: one per cue name the map holds) and "Restart".
 * The audio playback itself (the cue's FMOD events) is not here: the handlers run the race unit's ports (race_activity_audio.c). */
#include "build_activity_int.h"
#include "build_rider.h"
#include "build_vehicle_tree.h"
#include "world_wreck.h"
#include "build_activity_teardown.h"
#include "race_challenge_activity.h"
#include "activity_cues.h"

/* ---- the audio system's command map (std::map<const char*, fn, _stricmp-less> at 0x00d69244: head 0x00d6925c, size 0x00d69260;
 * node 0x18: left, parent, right, +0xc name, +0x10 callback, +0x14 colour, +0x15 isnil) ---- */
static int cmd_less(rx_addr a, rx_addr b) { return crt_stricmp(U32(a, 0), U32(b, 0)) < 0; }
static rx_addr cmd_make(rx_addr head, rx_addr where, rx_addr val) {   /* 008185d0 */
    rx_addr n = rx_reflex_aligned_alloc(0x18);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head);
        SU32(n, 0xc, U32(val, 0)); SU32(n, 0x10, U32(val, 4));
        SU8(n, 0x14, 0); SU8(n, 0x15, 0);
    }
    return n;
}
static const vt_tree k_cmd_map = {0x14, 0x1ffffffeu, cmd_less, cmd_make};
/* 00817f70 (EDI = &result {iterator, inserted}; stdcall &pair {name, callback}; RET 4): insert unique */
static void audio_command_add(rx_addr pair) { vt_insert_unique(&k_cmd_map, 0x00d69244, pair); }

/* 0067ec50: the current world (the engine's active scene: the event's +0x5a54 or the front end's +0x20c8) is the UI world */
static uint32_t current_world_is_ui(void) {
    const rx_addr g = U32(0, 0x00a946b8), core = U32(0, 0x00a95844);
    rx_addr w = 0;
    if (g) {
        if (U32(U32(core, 0x20), 0x34) == 1) w = U32(g, 0x5a54);
        else if (U32(U32(core, 0x1c), 0x34) == 1) w = U32(g, 0x20c8);
    }
    return w != 0 && U32(w, 0x20018) != 0;
}

/* the subscriptions of the logic (l) to its activity's channel / the stunts channel: template, conv, pushes, name, vtable, td */
#define ACH(l) U32(U32((l), 0x50), 0x290)
static void sub2(rx_addr tmpl, uint32_t conv, uint32_t pushes, rx_addr ch, rx_addr name, rx_addr vt, rx_addr td, rx_addr l, rx_addr method) {
    ACT_CALL(2, act_subscribe(tmpl, conv, 2, pushes, ch, name, vt, td, l, method));
}

/* 00581cf0 (ESI = logic): the twelve "<state> Entered / Exited" handlers (0058af50: void(Activity*, int)) */
static void audio_state_handlers(rx_addr l) {
    static const uint32_t k[12][2] = {
        {0x009427a4, 0x00592a60}, {0x009427b0, 0x00592ac0}, {0x009427bc, 0x00592b30}, {0x009427d0, 0x00592b80},
        {0x009427e4, 0x00592a70}, {0x009427f8, 0x00592ad0}, {0x0094280c, 0x00592b40}, {0x00942820, 0x00592b90},
        {0x00942830, 0x00592a80}, {0x00942840, 0x00592ae0}, {0x0094284c, 0x00592b50}, {0x00942860, 0x00592ba0}};
    ACT_FRAME(L, 0, 1);
    for (int i = 0; i < 12; i++) sub2(0x0058af50, 2 | 4, 3, ACH(l), k[i][0], 0x00945da4, 0x009cf5b4, l, k[i][1]);
    ACT_END(L);
}
/* 005817e0 (ESI = logic): "StuntBegin" / "StuntCompleted" (0058abf0) and "PhysicsStuntBegin" / "PhysicsStuntEnd" (0058acb0) on
 * the stunts channel [0x00a94304] */
static void audio_stunt_handlers(rx_addr l) {
    ACT_FRAME(L, 0, 1);
    const rx_addr st = U32(0, 0x00a94304);
    sub2(0x0058abf0, 8, 3, st, 0x0095ba28, 0x00945d44, 0x009cbf38, l, 0x00592b00);
    sub2(0x0058abf0, 8, 3, U32(0, 0x00a94304), 0x00942784, 0x00945d44, 0x009cbf38, l, 0x00592aa0);
    sub2(0x0058acb0, 8, 3, U32(0, 0x00a94304), 0x009607a0, 0x00945d64, 0x009ce768, l, 0x00592b60);
    sub2(0x0058acb0, 8, 3, U32(0, 0x00a94304), 0x00942794, 0x00945d64, 0x009ce768, l, 0x00592b10);
    (void)st;
    ACT_END(L);
}
/* 00581a90 (ESI = logic): the wreck handlers (0058ae30: void(RavenVehicle*)) */
static void audio_wreck_handlers(rx_addr l) {
    static const uint32_t k[5][2] = {{0x0093ee38, 0x005c3300}, {0x00941278, 0x00592bc0}, {0x00941260, 0x00592ab0},
                                     {0x009416fc, 0x00592b20}, {0x009416e8, 0x005c3310}};
    ACT_FRAME(L, 0, 1);
    for (int i = 0; i < 5; i++) sub2(0x0058ae30, 2 | 4, 3, ACH(l), k[i][0], 0x00945d84, 0x009cdb7c, l, k[i][1]);
    ACT_END(L);
}

/* ActivityAudioLogic::vf29 00580e70 (thiscall logic; activity; RET 4) */
void rx_audio_logic_vf29(rx_addr l, rx_addr A) {
    ACT_FRAME_EBP(L, 8, 0x14, 3);
    SU32(l, 0x50, A);
    SU32(l, 0x58, 0);
    {   /* the command callbacks (pair {name, fn} on the frame, EDI = the result) */
        const rx_addr pair = L + 0xc;
        SU32(pair, 0, 0x00942754); SU32(pair, 4, 0x006744f0);   /* "CrowdReact" */
        ACT_CALL(1, audio_command_add(pair));
        SU32(pair, 0, 0x00942760); SU32(pair, 4, 0x00675340);   /* "CrowdSize" */
        ACT_CALL(1, audio_command_add(pair));
    }
    uint32_t ui;
    ACT_CALLR(ui, 0, current_world_is_ui());
    const rx_addr mgr = U32(0, 0x00d92f14);
    rx_addr set = ui ? U32(mgr, 4) : U32(mgr, 8);
    SU32(l, 0x54, set);
    rx_addr cues;
    if (set) { cues = set + 0x94; SU32(l, 0x90, 0); }
    else {
        rx_addr b;
        ACT_CALLR(b, 1, rx_reflex_aligned_alloc(0x20));
        cues = 0;
        if (b) ACT_CALLR(cues, 0, rx_rider_map30_init(b));                  /* 0063b790 (ESI = block) */
        SU32(l, 0x90, 1);
    }
    SU32(l, 0x8c, cues);
#ifndef RX_ORACLE
    rx_activity_cues_bind(l);   /* w-sfx: the level's cues (the product has no activity audio manager: activity_cues.c) */
#endif
    ACT_CALL(0, audio_state_handlers(l));
    sub2(0x0058af50, 2 | 4, 3, ACH(l), 0x00941bdc /* "Countdown Step" */, 0x00945da4, 0x009cf5b4, l, 0x00592a90);
    sub2(0x0058af50, 2 | 4, 3, ACH(l), 0x00941bec /* "FinishCountdown Step" */, 0x00945da4, 0x009cf5b4, l, 0x00592af0);
    ACT_CALL(0, audio_stunt_handlers(l));
    ACT_CALL(0, audio_wreck_handlers(l));
    act_vcall(l, 0xc, 0, 0);                                                  /* vf3: the class's cue handlers */
    ACT_END(L);
}
RX_FUNCTION(0x00580e70, rx_audio_logic_vf29)

/* RaceActivityAudioLogic::vf29 00582ed0 (thiscall logic; activity; RET 4): the base vf29, "Restart" (0058b2b0) -> 00582d70 */
void rx_race_audio_logic_vf29(rx_addr l, rx_addr A) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(1, rx_audio_logic_vf29(l, A));
    ACT_CALL(1, act_subscribe(0x0058b2b0, 1, 1, 2, ACH(l), 0x00941ba0 /* "Restart" */, 0x00945e04, 0x009cbd24, l, 0x00582d70));
    ACT_END(L);
}
RX_FUNCTION(0x00582ed0, rx_race_audio_logic_vf29)

/* the cue map (std::multimap<std::string, Cue*>) holds a cue of the name: StringMap_Find (00702ad0) != end */
static int cue_present(rx_addr l, rx_addr L, rx_addr name) {
    const rx_addr s = L + 0x1c, it = L + 0x14;
    crt_str_ctor_cstr(s, name);
    const rx_addr m = U32(l, 0x8c);
    const rx_addr end = U32(m, 0x18), proxy = U32(m, 0);
    rx_addr r;
    ACT_CALLR(r, 2, rx_world_string_map_find(m, it, s));
    if (U32(r, 0) == 0 || U32(r, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(r, 4) != end;
    crt_str_dtor(s);
#ifndef RX_ORACLE
    if (!found) found = rx_activity_cues_present(l, name);   /* w-sfx: the level's cues (activity_cues.c) */
#endif
    return found;
}
/* RaceActivityAudioLogic::vf3 00582610 (ECX = logic): a handler per cue the map holds (EndOfRace / TwoLapsLeft / OneLapLeft /
 * Holeshot: 0058b3d0 void(Activity*, BaseDriver*); Best Lap 0058b490, VehicleGettingLapped 0058b550, Player Wins 0058b610, Player
 * Eliminated 0058b6d0, TimedSectionEntered 0058b790) */
void rx_race_audio_logic_vf3(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        if (cue_present(l, L, 0x00941c9c)) sub2(0x0058b3d0, 2 | 4, 3, ACH(l), 0x00941c9c /* "EndOfRace" */, 0x00945e24, 0x009cda18, l, 0x00582da0);
        if (cue_present(l, L, 0x00941c74)) sub2(0x0058b3d0, 2 | 4, 3, ACH(l), 0x00941c74 /* "TwoLapsLeft" */, 0x00945e24, 0x009cda18, l, 0x00582dc0);
        if (cue_present(l, L, 0x00941c80)) sub2(0x0058b3d0, 2 | 4, 3, ACH(l), 0x00941c80 /* "OneLapLeft" */, 0x00945e24, 0x009cda18, l, 0x00582de0);
        if (cue_present(l, L, 0x0093ef0c)) ACT_CALL(1, act_subscribe(0x0058b490, 1, 1, 4, ACH(l), 0x0093ef0c /* "Best Lap" */, 0x00945e44, 0x009cda88, l, 0x00582e00));
        if (cue_present(l, L, 0x00941c5c)) ACT_CALL(1, act_subscribe(0x0058b550, 1, 1, 4, ACH(l), 0x00941c5c /* "VehicleGettingLapped" */, 0x00945e64, 0x009cdb7c, l, 0x00582e20));
        if (cue_present(l, L, 0x00941ca8)) ACT_CALL(1, act_subscribe(0x0058b610, 1, 1, 4, ACH(l), 0x00941ca8 /* "Player Wins" */, 0x00945e84, 0x009cf630, l, 0x00583fb0));
        if (cue_present(l, L, 0x0093ef48)) ACT_CALL(1, act_subscribe(0x0058b6d0, 1, 1, 4, ACH(l), 0x0093ef48 /* "Player Eliminated" */, 0x00945ea4, 0x009cdaa8, l, 0x00583a70));
        if (cue_present(l, L, 0x00942044)) sub2(0x0058b3d0, 2 | 4, 3, ACH(l), 0x00942044 /* "Holeshot" */, 0x00945e24, 0x009cda18, l, 0x00582e40);
        if (cue_present(l, L, 0x00941710)) ACT_CALL(1, act_subscribe(0x0058b790, 1, 1, 4, ACH(l), 0x00941710 /* "TimedSectionEntered" */, 0x00945ec4, 0x009ce550, l, 0x00582e60));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00582610, rx_race_audio_logic_vf3)

/* FreeRideActivityAudioLogic::vf3 00582330 (ECX = logic): the "ChallengeAwardAchieved" cue's handler (0058b070: void(MXPlayer*, MXChallengeType, MXChallengeAwardType),
 * proxy vtable 0x00945dc4, signature 0x009ce508) on the activity channel when the cue map holds that cue; the callback 00592bb0 plays the cue */
void rx_free_ride_audio_logic_vf3(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        if (cue_present(l, L, 0x00942870 /* "ChallengeAwardAchieved" */))
            ACT_CALL(1, act_subscribe(0x0058b070, 1, 1, 4, ACH(l), 0x00942870, 0x00945dc4, 0x009ce508, l, 0x00592bb0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00582330, rx_free_ride_audio_logic_vf3)

/* ChallengeActivityAudioLogic (the challenge activities' logic, vtable 0x009431fc; 00584e50): its cue handlers on the activity channel
 * for the cues the map holds: "ChallengeAward" (template 0058d470: proxy vtable 0x009461c4, signature 0x009cf774, the callback
 * 00585150), "TargetPracticeFound" (0058d530: 0x009461e4, 0x009cf7a4, 005851a0), "HitDiscoveryFound" (0058d5f0: 0x00946204, 0x009cbe2c,
 * 005851d0); the callbacks play the cue (race_activity_audio.c). vf3 00584ed0 subscribes them, vf4 00585010 removes them (the
 * unsubscription templates 0058d6b0 / 0058d710 / 0058d770, unaligned 0x14-byte key frames). */
static const struct { rx_addr name, sub, unsub, vtable, td, method; } k_challenge_cues[3] = {
    {0x009424f4 /* "ChallengeAward" */, 0x0058d470, 0x0058d6b0, 0x009461c4, 0x009cf774, 0x00585150},
    {0x009419b0 /* "TargetPracticeFound" */, 0x0058d530, 0x0058d710, 0x009461e4, 0x009cf7a4, 0x005851a0},
    {0x00942898 /* "HitDiscoveryFound" */, 0x0058d5f0, 0x0058d770, 0x00946204, 0x009cbe2c, 0x005851d0},
};
void rx_challenge_audio_logic_vf3(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        for (int k = 0; k < 3; k++) {
            if (cue_present(l, L, k_challenge_cues[k].name))
                ACT_CALL(1, act_subscribe(k_challenge_cues[k].sub, 1, 1, 4, ACH(l), k_challenge_cues[k].name, k_challenge_cues[k].vtable,
                                          k_challenge_cues[k].td, l, k_challenge_cues[k].method));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00584ed0, rx_challenge_audio_logic_vf3)
void rx_challenge_audio_logic_vf4(rx_addr l) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(l, 0x8c) != 0) {
        for (int k = 0; k < 3; k++) {
            if (cue_present(l, L, k_challenge_cues[k].name))
                ACT_CALL(1, act_unlisten(0, ACH(l), k_challenge_cues[k].name, k_challenge_cues[k].vtable, k_challenge_cues[k].td, l,
                                         k_challenge_cues[k].method));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00585010, rx_challenge_audio_logic_vf4)
