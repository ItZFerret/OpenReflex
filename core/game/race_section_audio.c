/* race_section_audio.c -- SectionRaceActivityAudioLogic (vtable 0x00942d9c; 0xa4 bytes): the audio logic of the Section Race. It is a
 * RaceActivityAudioLogic whose vf3 / vf4 (the cue handlers) add "SectionWinner" and "SectionLost" (void(SectionRaceDriver*, int), proxy
 * vtable 0x00945ee4) when the level's cue map (+0x8c) holds a cue of that name. The handlers 005830f0 / 00583110 play the cue
 * (005811a0, race_activity_audio.c: the cue's crowd reactions draw the game's random generator, so they are ported, not left to the
 * audio engine). Hand-transliterated from the original (the address of each function is in its comment). */
#include "race_section.h"
#include "build_activity_int.h"
#include "build_activity_teardown.h"
#include "race_activity.h"
#include "race_extern.h"
#include "world_wreck.h"
#include "activity_cues.h"

#define ACH(l) U32(U32((l), 0x50), 0x290)   /* the logic's activity's channel */

/* the cue map (+0x8c, std::multimap<std::string, Cue*>) holds a cue of the name: StringMap_Find (00702ad0) != end (the string at
 * L + 0x1c, the iterator at L + 0x14; L = the frame of vf3 / vf4) */
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

/* 0058ba90 (EAX = name, ECX = channel; stdcall logic, method; RET 8): the logic listens to a void(SectionRaceDriver*, int) event with the
 * method (proxy 0x00945ee4). Frame: push ebx / esi / edi. */
void rx_section_audio_listen(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_subscribe(0x0058ba90, 2 | 4, 2, 3, channel, name, 0x00945ee4, 0x009cdad8, logic, method);
}
RX_FUNCTION(0x0058ba90, rx_section_audio_listen)

/* 0058bb50 (EAX = name, ECX = channel; stdcall logic, method; RET 8): the listener of 0058ba90 leaves. Frame: sub esp, 0x14; push esi. */
void rx_section_audio_unlisten(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method) {
    act_unlisten(0, channel, name, 0x00945ee4, 0x009cdad8, logic, method);
}
RX_FUNCTION(0x0058bb50, rx_section_audio_unlisten)

/* 00582ef0 slot 3 (ECX = logic): RaceActivityAudioLogic::vf3, then the cue handlers of the section race. Frame: push ebp; and esp, -8;
 * sub esp, 0x28; push ebx / ebp / esi / edi. */
void rx_section_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    ACT_CALL(0, rx_race_audio_logic_vf3(logic));
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, 0x0093ef5c /* "SectionWinner" */)) {
            SU32(L, -4, 0x005830f0);
            SU32(L, -8, logic);
            ACT_CALL(2, rx_section_audio_listen(ACH(logic), 0x0093ef5c, logic, 0x005830f0));
        }
        if (cue_present(logic, L, 0x0093ef6c /* "SectionLost" */)) {
            SU32(L, -4, 0x00583110);
            SU32(L, -8, logic);
            ACT_CALL(2, rx_section_audio_listen(ACH(logic), 0x0093ef6c, logic, 0x00583110));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00582ef0, rx_section_audio_vf3)

/* 00582ff0 slot 4 (ECX = logic): RaceActivityAudioLogic::vf4, then the section cue handlers leave. Same frame as vf3. */
void rx_section_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(L, 8, 0x28, 4);
    ACT_CALL(0, rx_race_audio_logic_vf4(logic));
    if (U32(logic, 0x8c) != 0) {
        if (cue_present(logic, L, 0x0093ef5c)) {
            SU32(L, -4, 0x005830f0);
            SU32(L, -8, logic);
            ACT_CALL(2, rx_section_audio_unlisten(ACH(logic), 0x0093ef5c, logic, 0x005830f0));
        }
        if (cue_present(logic, L, 0x0093ef6c)) {
            SU32(L, -4, 0x00583110);
            SU32(L, -8, logic);
            ACT_CALL(2, rx_section_audio_unlisten(ACH(logic), 0x0093ef6c, logic, 0x00583110));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00582ff0, rx_section_audio_vf4)

/* 005830f0 / 00583110 (thiscall logic; driver, section; RET 8): the "SectionWinner" / "SectionLost" handlers: the cue of that name for
 * the driver's player (005811a0). */
void rx_section_audio_winner(rx_addr logic, rx_addr driver, int32_t section) {
    (void)section;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x0093ef5c /* "SectionWinner" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x005830f0, rx_section_audio_winner)
void rx_section_audio_lost(rx_addr logic, rx_addr driver, int32_t section) {
    (void)section;
    RACE_CORE_CALL(4, rx_audio_cue(logic, 0x0093ef6c /* "SectionLost" */, U32(driver, 0x74), 0));
}
RX_FUNCTION(0x00583110, rx_section_audio_lost)

