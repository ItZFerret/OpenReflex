/* race_tag_audio.c -- TagActivityAudioLogic (vtable 0x00942f7c; 0x98 bytes): the audio logic of the Tag party mode. An ActivityAudioLogic
 * (the base vf3 / vf4 are not called) whose vf3 / vf4 add (remove) the handlers of the activity's events "Round Started", "Round Over",
 * "Match Over", "BallDropped", "BallGrabbed" and "Player Wins" when the level's cue map (+0x8c) holds a cue of that name; each handler
 * plays the cue of the same name (005811a0, race_activity_audio.c: its crowd reactions draw the game's random generator).
 * Hand-transliterated from the original (each function's address is in its comment). */
#include "race_tag.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "race_activity.h"
#include "race_extern.h"
#include "world_wreck.h"
#include "activity_cues.h"
#include "crt.h"

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */
#define STR_ROUND_STARTED 0x009421b0u
#define STR_ROUND_OVER    0x009421c0u
#define STR_MATCH_OVER    0x009421ccu
#define STR_BALL_DROPPED  0x00942250u
#define STR_BALL_GRABBED  0x00942244u
#define STR_PLAYER_WINS   0x00941ca8u

/* the cue map (+0x8c, std::multimap<std::string, Cue*>) holds a cue of the name: StringMap_Find (00702ad0) != end (the string at
 * L + 0x1c, the iterator at L + 0x14; L = the frame of vf3 / vf4), as race_section_audio.c */
static int cue_present(rx_addr logic, rx_addr L, rx_addr name) {
    const rx_addr s = L + 0x1c, it = L + 0x14;
    crt_str_ctor_cstr(s, name);
    const rx_addr map = U32(logic, 0x8c);
    const rx_addr end = U32(map, 0x18), proxy = U32(map, 0);
    rx_addr r;
    SU32(L, -4, s);
    SU32(L, -8, it);
    ACT_CALLR(r, 2, rx_world_string_map_find(map, it, s));
    if (U32(r, 0) == 0 || U32(r, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(r, 4) != end;
    crt_str_dtor(s);
#ifndef RX_ORACLE
    if (!found) found = rx_activity_cues_present(logic, name);   /* w-sfx: the level's cues (activity_cues.c) */
#endif
    return found;
}

/* ---- the listener templates ---- */

/* 0058c4b0 (EAX = channel; stdcall logic; RET 4): "Round Started" with 00584df0 (proxy 0x946004: void(Activity*, int, int, int)).
 * Frame: push ecx / ebx / esi / edi. */
void rx_tag_audio_listen_round_started(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058c4b0, 1, 1, 4, channel, STR_ROUND_STARTED, 0x00946004, 0x009cf680, logic, 0x00584df0);
}
RX_FUNCTION(0x0058c4b0, rx_tag_audio_listen_round_started)
/* 0058c570 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(Activity*) event (proxy 0x946024). Frame: push ebx / esi / edi. */
void rx_tag_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058c570, 2 | 4, 2, 3, channel, name, 0x00946024, 0x009cbd24, logic, method);
}
RX_FUNCTION(0x0058c570, rx_tag_audio_listen_activity)
/* 0058c630 (ECX = channel, EAX = name; stdcall logic, method; RET 8): a void(Activity*, BaseDriver*) event (proxy 0x946044) */
void rx_tag_audio_listen_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058c630, 2 | 4, 2, 3, channel, name, 0x00946044, 0x009cda18, logic, method);
}
RX_FUNCTION(0x0058c630, rx_tag_audio_listen_driver)
/* 0058c6f0 (EAX = channel; stdcall logic; RET 4): "Player Wins" with 00583fb0 (proxy 0x946064). Frame: push ecx / ebx / esi / edi. */
void rx_tag_audio_listen_player_wins(rx_addr channel, rx_addr logic) {
    act_subscribe(0x0058c6f0, 1, 1, 4, channel, STR_PLAYER_WINS, 0x00946064, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058c6f0, rx_tag_audio_listen_player_wins)
/* 0058c7b0 / 0058c810 / 0058c870 / 0058c8d0: the listeners leave (frame: sub esp, 0x14; push esi) */
void rx_tag_audio_unlisten_round_started(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_ROUND_STARTED, 0x00946004, 0x009cf680, logic, 0x00584df0);
}
RX_FUNCTION(0x0058c7b0, rx_tag_audio_unlisten_round_started)
void rx_tag_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x00946024, 0x009cbd24, logic, method);
}
RX_FUNCTION(0x0058c810, rx_tag_audio_unlisten_activity)
void rx_tag_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x00946044, 0x009cda18, logic, method);
}
RX_FUNCTION(0x0058c870, rx_tag_audio_unlisten_driver)
void rx_tag_audio_unlisten_player_wins(rx_addr channel, rx_addr logic) {
    act_unlisten(0, channel, STR_PLAYER_WINS, 0x00946064, 0x009cf630, logic, 0x00583fb0);
}
RX_FUNCTION(0x0058c8d0, rx_tag_audio_unlisten_player_wins)

/* ---- vf3 / vf4 ---- */

/* 00583ab0 slot 3 (ECX = logic): the cue handlers listen. Frame: push ebp; and esp, -8; sub esp, 0x28; push ebx / ebp / esi / edi. */
void rx_tag_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_tag_audio_listen_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_tag_audio_listen_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_tag_audio_listen_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_BALL_DROPPED)) ACT_CALL(2, rx_tag_audio_listen_driver(ACH(logic), STR_BALL_DROPPED, logic, 0x00583fe0));
        if (cue_present(logic, L, STR_BALL_GRABBED)) ACT_CALL(2, rx_tag_audio_listen_driver(ACH(logic), STR_BALL_GRABBED, logic, 0x00584000));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_tag_audio_listen_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00583ab0, rx_tag_audio_vf3)

/* 00583d30 slot 4 (ECX = logic): the cue handlers leave. Same frame as vf3. */
void rx_tag_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, STR_ROUND_STARTED)) ACT_CALL(1, rx_tag_audio_unlisten_round_started(ACH(logic), logic));
        if (cue_present(logic, L, STR_ROUND_OVER)) ACT_CALL(2, rx_tag_audio_unlisten_activity(ACH(logic), STR_ROUND_OVER, logic, 0x00583a30));
        if (cue_present(logic, L, STR_MATCH_OVER)) ACT_CALL(2, rx_tag_audio_unlisten_activity(ACH(logic), STR_MATCH_OVER, logic, 0x00583a50));
        if (cue_present(logic, L, STR_BALL_DROPPED)) ACT_CALL(2, rx_tag_audio_unlisten_driver(ACH(logic), STR_BALL_DROPPED, logic, 0x00583fe0));
        if (cue_present(logic, L, STR_BALL_GRABBED)) ACT_CALL(2, rx_tag_audio_unlisten_driver(ACH(logic), STR_BALL_GRABBED, logic, 0x00584000));
        if (cue_present(logic, L, STR_PLAYER_WINS)) ACT_CALL(1, rx_tag_audio_unlisten_player_wins(ACH(logic), logic));
    }
    ACT_END(L);
}
RX_FUNCTION(0x00583d30, rx_tag_audio_vf4)

/* ---- the cue handlers (thiscall logic): the cue of the event's name (005811a0: logic, name, a player, 0) ---- */

/* 00584df0 (activity, round, best, rounds; RET 0x10): "Round Started" with the ADDRESS of the round argument where a player goes (the
 * original's lea of its own argument slot) */
void rx_tag_audio_round_started(rx_addr logic, rx_addr act, uint32_t round, uint32_t best, uint32_t rounds) {
    (void)act; (void)round; (void)best; (void)rounds;
    const rx_addr slot = rx_sp() + 8;   /* the entry ESP + 8: the round argument */
    RACE_CORE_CALL(4, rx_audio_cue(logic, STR_ROUND_STARTED, slot, 0));
}
RX_FUNCTION(0x00584df0, rx_tag_audio_round_started)
/* 00583a30 / 00583a50 (activity; RET 4): "Round Over" / "Match Over", no player */
void rx_tag_audio_round_over(rx_addr logic, rx_addr act) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_ROUND_OVER, 0, 0)); }
RX_FUNCTION(0x00583a30, rx_tag_audio_round_over)
void rx_tag_audio_match_over(rx_addr logic, rx_addr act) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_MATCH_OVER, 0, 0)); }
RX_FUNCTION(0x00583a50, rx_tag_audio_match_over)
/* 00583fe0 / 00584000 (activity, driver; RET 8): "BallDropped" / "BallGrabbed" for the driver's player */
void rx_tag_audio_ball_dropped(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_BALL_DROPPED, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00583fe0, rx_tag_audio_ball_dropped)
void rx_tag_audio_ball_grabbed(rx_addr logic, rx_addr act, rx_addr driver) { (void)act; RACE_CORE_CALL(4, rx_audio_cue(logic, STR_BALL_GRABBED, U32(driver, 0x74), 0)); }
RX_FUNCTION(0x00584000, rx_tag_audio_ball_grabbed)
/* 00583fb0 ("Player Wins") is RaceActivityAudioLogic's handler, shared: rx_race_audio_player_wins (race_activity_audio.c). */
