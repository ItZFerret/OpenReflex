/* fx_tutorial_manager.c -- the MotoSkills TutorialManager (MXUIWorld + 0x22180, 0xc0 bytes): its construction, events, per-frame update, the six
 * lessons' checks, the lesson start / outcome / retry, the voice and the profile's TutorialsCompleted. _re/notes/tutorials.md (the flow, the Lua
 * pages); names _re/names/98_tutorials.tsv. Every function is a hand transliteration of the disassembly (`disasm.py <addr>`).
 *
 * The manager (ESI / ECX = m):
 *   +0x00 not running (1)        +0x04 the lesson 0..5        +0x08 0 idle, 1 waits for the input action 7, 2 runs
 *   +0x0c / +0x10 / +0x14 the player count / vehicle / "in the UI world" are valid (events SetValidPlayerCount / SetValidVehicle / InUIWorld)
 *   +0x18 the lesson's boundary exit does not count       +0x1c a lesson-objects pass is pending for the lesson +0x20 (ShowLessonObjects)
 *   +0x24 the local player's vehicle      +0x30 / +0x40 / +0x50 the lesson's start: position, two directions (the TUT0<n>_Start trigger)
 *   +0x60 the manager may run (+0 == 0 and the three valid words)   +0x64 passed   +0x68 / +0x6c bad / good jumps   +0x70 wrecked
 *   +0x74 left the lesson's area   +0x78 show the PreTutorialPopup at the start   +0x7c the lesson's step   +0x80 the landing was a crash
 *   +0x84 the wreck timer   +0x88 the step's 2 s timer   +0x8c / +0x90 the lean timers   +0x94 judge the jump   +0x98 above / away from the
 *   ground   +0x9c knocked off balance   +0xa0 .. +0xac the four lean directions   +0xb0 the lean-in's other axis   +0xb4 a popup froze the rider
 * Calls into the UI and the localized voice go through fx_tutorial_ui.h (oracle: the originals; product: the record the host reads). The
 * localized sound list's own stop (0080c880) and the sound engine's pause (00805000) are the core's / the audio boundary. */
#include "fx_tutorial.h"
#include "fx_tutorial_ui.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_activity_freeride_run.h"
#include "build_boundary_audio.h"
#include "build_tables.h"
#include "build_asset.h"
#include "race_activity.h"
#include "race.h"
#include "world_misc.h"
#include "world_veh.h"
#include "world_board_handlers.h"
#include "world_wreck.h"
#include "phys_terrain.h"
#include "phys_crt.h"
#include "loop_player.h"
#include "loop_util.h"
#include "anim.h"
#include "anim_scenario.h"
#include "crt.h"
#include "highlight_progress.h"
#include "garage_test_drive.h"
#include "build_ui_world.h"
#include "race.h"
#include <string.h>

#define TUT_CHANNEL      0x00a946a8u   /* g_TutorialManagerChannel */
#define TUT_AUDIO_DB     0x00a946acu   /* g_TutorialAudioDatabase ("AudioLocalizedTutorial") */
#define TUT_AUDIO_PKG    0x00a946b0u   /* g_TutorialAudioPackage ("Audio") */
#define TUT_AUDIO_LOADED 0x00a946b4u
#define TUT_LESSON_NAMES 0x009dd284u   /* Reflex_FourDirections, Reflex_HoldTight, Reflex_LeanIn, TakeOffAndLanding, WreckAvoidance, FreestyleTricks */
#define TUT_VOICE_KINDS  0x009dd29cu   /* Intro, BoundryExceeded, Completed, Fail, Wrecked */
#define S_MANAGER        0x0096cfa8u   /* "TutorialManager" */
#define S_MAPPINGS       0x0096cfb8u   /* "TutorialAudioMappings" */
#define S_DELIM          0x0094f118u   /* "_" */
#define S_EMPTY          0x009416d4u   /* "" */
#define CTX              0x00a95844u
#define GAME             0x00a946b8u

/* ---- small helpers ---------------------------------------------------------------------------------------------------------------- */

/* the world of the active scene (inlined everywhere): the event scene's (context + 0x20 +0x34 == 1) the real world G + 0x5a54, the UI
 * scene's (context + 0x1c) the UI world G + 0x20c8, else 0 */
static rx_addr tut_active_world(void) {
    const rx_addr ctx = U32(0, CTX), g = U32(0, GAME);
    if (U32(U32(ctx, 0x20), 0x34) == 1) return U32(g, 0x5a54);
    if (U32(U32(ctx, 0x1c), 0x34) == 1) return U32(g, 0x20c8);
    return 0;
}
/* the original's `push 0; push 0; (EAX = world + 0x21da0, EDI = 8) call 0055f630; call 00559e60`: the world's FreeRide activity (kind 8,
 * area 0) and its first human driver (the activity is passed on in EAX even when it is 0) */
static rx_addr tut_find_activity(rx_addr world) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 4);
    rx_addr a;
    ACT_CALLR(a, 1, rx_activity_find_by_kind_area(world + 0x21da0, 8, 0));
    rx_set_sp(sp);
    return a;
}
static rx_addr tut_driver_of(rx_addr act) {
    rx_addr d;
    ACT_CALLR(d, 1, rx_activity_human_driver(act, 0));
    return d;
}
static rx_addr tut_local_driver(rx_addr world) { return tut_driver_of(tut_find_activity(world)); }

/* a KeyMap's value (vtable +0x10 (7, pad); x87 st0) */
static float tut_keymap_value(rx_addr km, uint32_t pad) {
    const rx_addr fn = U32(U32(km, 0), 0x10);
    float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, float (*)(rx_addr, uint32_t, uint32_t));
    if (f) { float v; const uint32_t a[2] = {7, pad}; const rx_addr s = loop_push(2, a, 0); v = f(km, 7, pad); loop_pop(s); return v; }
    rx_regs in, out; memset(&in, 0, sizeof in); memset(&out, 0, sizeof out); in.r[1] = km;
    const uint32_t a[2] = {7, pad};
    rx_call_original_ex(fn, &in, &out, 2, a);
    return out.st0;
}
/* a KeyMap's bindings of a pad on / off (vtable +0x24 (pad, on)) */
static void tut_keymap_enable(rx_addr km, uint32_t pad, uint32_t on) { (void)loop_tc2(loop_vslot(km, 0x24), km, pad, on); }
static rx_addr tut_input_block(uint32_t pad) { return U32(0, 0x009c52c4u + 4u * pad); }
/* the rider lean's two axes (00548fc0 / 00549050 maps, input 7) */
static float tut_lean_x(uint32_t pad) {   /* 00548580 (EAX = pad): the first map's value (the second read and dropped) */
    const rx_addr a = rx_world_pad_wreck_steer_map(pad), b = rx_world_pad_wreck_lean_map(pad);
    const float v = tut_keymap_value(a, pad);
    (void)tut_keymap_value(b, pad);
    return v;
}
static float tut_lean_y(uint32_t pad) {   /* 00548540 (EAX = pad): -(the second map's value) */
    const rx_addr a = rx_world_pad_wreck_steer_map(pad), b = rx_world_pad_wreck_lean_map(pad);
    (void)tut_keymap_value(a, pad);
    return -tut_keymap_value(b, pad);
}

/* The eleven input groups of a player on / off, as MXPlayer's own setters (loop_player.c): each re-reads the pad (+0x1f0) and skips a
 * negative one; a replay's hold (+0x11c8) only marks the group's bit in +0x11cc. */
static void tut_groups(rx_addr P, uint32_t on) {
#define TUT_GROUP(bit, stmt) do { const int32_t s_ = I32(P, 0x1f0); if (s_ >= 0) { const uint32_t pad = (uint32_t)s_; (void)pad; \
        if (U32(P, 0x11c8) != 0) SU32(P, 0x11cc, on ? (U32(P, 0x11cc) | (bit)) : (U32(P, 0x11cc) & ~(uint32_t)(bit))); else { stmt; } } } while (0)
    TUT_GROUP(1, { tut_keymap_enable(rx_world_pad_steer_map(pad), pad, on); tut_keymap_enable(rx_world_pad_lean_map(pad), pad, on); });
    TUT_GROUP(2, tut_keymap_enable(tut_input_block(pad) + 0x348, pad, on));
    TUT_GROUP(4, ACT_CALL(0, rx_player_slot_group2(pad, on)));
    TUT_GROUP(8, { tut_keymap_enable(rx_world_pad_wreck_steer_map(pad), pad, on); tut_keymap_enable(rx_world_pad_wreck_lean_map(pad), pad, on); });
    TUT_GROUP(0x10, tut_keymap_enable(tut_input_block(pad) + 0x9ec, pad, on));
    TUT_GROUP(0x20, ACT_CALL(0, rx_player_slot_group5(pad, on)));
    TUT_GROUP(0x40, tut_keymap_enable(tut_input_block(pad) + 0x127c, pad, on));
    TUT_GROUP(0x80, tut_keymap_enable(tut_input_block(pad) + 0x167c, pad, on));
    TUT_GROUP(0x100, tut_keymap_enable(tut_input_block(pad) + 0x748, pad, on));
    TUT_GROUP(0x200, tut_keymap_enable(tut_input_block(pad) + 0x147c, pad, on));
    TUT_GROUP(0x400, tut_keymap_enable(tut_input_block(pad) + 0xf34, pad, on));
#undef TUT_GROUP
}

/* FUN_0078fd60 (ESI = size, EDI = buffer; cdecl format, one argument) */
static void tut_format(rx_addr buf, rx_addr fmt, uint32_t arg, int has_arg) {
    const rx_addr sp = rx_sp();
    SU32(sp, -8, fmt); SU32(sp, -4, arg);
    rx_set_sp(sp - 12);
    rx_format_buffer(buf, 0x104, fmt, sp - 4);
    rx_set_sp(sp);
    (void)has_arg;
}
#define tut_strtok crt_strtok
/* the trigger's / entity's name (+0x18) into the 0x103-byte frame buffer (strncpy, then the last byte 0), its first token */
static rx_addr tut_second_token(rx_addr buf, rx_addr name) {
    crt_strncpy(buf, name, 0x103);
    SU8(buf, 0x103, 0);
    (void)tut_strtok(buf, S_DELIM);
    return tut_strtok(0, S_DELIM);
}

/* 0074fcd0 (cdecl name) -> the lesson whose voice name (0x009dd284) the name is (_stricmp), 6 when none */
int32_t rx_tutorial_lesson_of_name(rx_addr name) {
    int32_t i = 0;
    for (; i < 6; i++) if (crt_stricmp(name, U32(0, TUT_LESSON_NAMES + 4u * (uint32_t)i)) == 0) return i;
    return 6;
}
RX_FUNCTION(0x0074fcd0, rx_tutorial_lesson_of_name)

/* ---- the voice -------------------------------------------------------------------------------------------------------------------- */

/* the engine database's (context + 8) handle of "TutorialAudioMappings" ('bxml'), its XML reader (+0x44 - 0x460) with the handle's +0x4c set */
static rx_addr tut_mappings(void) {
    const rx_addr db = U32(U32(0, CTX), 8);
    const uint32_t a[4] = {S_MAPPINGS, 0x6c6d7862 /* 'bxml' */, 0, 0};
    const rx_addr h = rx_call_original(U32(U32(db, 0), 0x6c), RX_THISCALL, db, 4, a);
    if (!h) return 0;
    SU32(h, 0x4c, 1);
    return U32(h, 0x44) ? U32(h, 0x44) - 0x460 : 0;
}
static uint32_t xr(rx_addr x, uint32_t off, int n, const uint32_t* a) { return rx_call_original(U32(U32(x, 0), off), RX_THISCALL, x, n, a); }

/* 00752a40 TutorialManager_ReadAudioSettings: unless both are set, the mappings' Settings / DataBase and Settings / Package texts */
void rx_tutorial_read_audio_settings(void) {
    const rx_addr d = U32(0, TUT_AUDIO_DB), p = U32(0, TUT_AUDIO_PKG);
    if (d && U8(d, 0) && p && U8(p, 0)) return;
    const rx_addr x = tut_mappings();
    if (!x) return;
    const rx_addr root = xr(x, 4, 0, 0);
    uint32_t a[2] = {root, 0x0095bd58 /* "Settings" */};
    const rx_addr settings = xr(x, 0x1c, 2, a);
    if (!settings) return;
    a[0] = settings; a[1] = 0x0095baa8;   /* "DataBase" */
    rx_addr e = xr(x, 0x1c, 2, a);
    if (e) { const uint32_t b[1] = {e}; SU32(0, TUT_AUDIO_DB, xr(x, 0xe0, 1, b)); }
    a[0] = settings; a[1] = 0x0093702c;   /* "Package" */
    e = xr(x, 0x1c, 2, a);
    if (e) { const uint32_t b[1] = {e}; SU32(0, TUT_AUDIO_PKG, xr(x, 0xe0, 1, b)); }
}
RX_FUNCTION(0x00752a40, rx_tutorial_read_audio_settings)

/* the localized sound list's stop of the manager's requests (0080c880 "TutorialManager"), when the list exists ([0x00a957d4]) */
static void tut_stop_voice(void) {
#ifndef RX_ORACLE
    rx_tutorial_ui_note_stop();
#endif
    if (U32(0, 0x00a957d4) != 0) ACT_CALL(1, rx_loc_sound_list_stop_owner(S_MANAGER));
}

/* 007527c0 TutorialManager_PlayLessonVoice (cdecl lesson, kind): the manager's requests stopped; the mappings' "Tutorial" elements whose
 * Name (0x0093a3bc) is the lesson's (_stricmp), its child of the kind's name, a non-empty text = the event path: UILocSound_Play (the database
 * / package of the settings, the path, priority 9, owner "TutorialManager"); the element iterator released. */
void rx_tutorial_play_lesson_voice(int32_t lesson, int32_t kind) {
    RX_FRAME_ENTER(F, 0xc + 0x10);
    tut_stop_voice();
    const rx_addr x = tut_mappings();
    if (x) {
        const rx_addr it = F + 0x14;
        const rx_addr root = xr(x, 4, 0, 0);
        uint32_t a[3] = {root, 0x0096d3a8 /* "Tutorial" */, it};
        rx_addr e = xr(x, 0x20, 3, a);
        while (e) {
            uint32_t b[2] = {e, 0x0093a3bc /* "Name" */};
            const rx_addr at = xr(x, 0xe8, 2, b);
            rx_addr nm = 0;
            if (at) { const uint32_t c[1] = {at}; nm = xr(x, 0x110, 1, c); }
            if (crt_stricmp(nm, U32(0, TUT_LESSON_NAMES + 4u * (uint32_t)lesson)) == 0) {
                const uint32_t c[2] = {e, U32(0, TUT_VOICE_KINDS + 4u * (uint32_t)kind)};
                const rx_addr k = xr(x, 0x1c, 2, c);
                if (k) {
                    const uint32_t t1[1] = {k};
                    const rx_addr path = xr(x, 0xe0, 1, t1);
                    if (U8(path, 0)) {
                        ACT_CALL(0, rx_tutorial_read_audio_settings());
                        rx_tutorial_ui_voice(U32(0, TUT_AUDIO_DB), U32(0, TUT_AUDIO_PKG), path, S_MANAGER);
                        break;
                    }
                }
            }
            const uint32_t n[1] = {U32(it, 0)};
            e = xr(x, 0x28, 1, n);
        }
        const uint32_t r[1] = {U32(it, 0)};
        (void)xr(x, 0x24, 1, r);
    }
    RX_FRAME_LEAVE(F, 0x1c);
}
RX_FUNCTION(0x007527c0, rx_tutorial_play_lesson_voice)

/* 00752b10 TutorialManager_LoadAudio / 00752b60 _UnloadAudio: the settings, then the package's load request (0041b4b0 on [0x00d72a30]) /
 * release (0041b600). AUDIO BOUNDARY (product): the localized voice package is the host's (it plays the voice from the recorded path);
 * the loaded flag is the original's. */
void rx_tutorial_load_audio(void) {
    if (U32(0, TUT_AUDIO_LOADED) != 0) return;
    ACT_CALL(0, rx_tutorial_read_audio_settings());
    const rx_addr d = U32(0, TUT_AUDIO_DB), p = U32(0, TUT_AUDIO_PKG);
    if (!d || !U8(d, 0) || !p || !U8(p, 0)) return;
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = p; in.r[2] = d;
    const uint32_t a[1] = {U32(0, 0x00d72a30)};
    rx_call_original_ex(0x0041b4b0, &in, &o, 1, a);
#endif
    SU32(0, TUT_AUDIO_LOADED, 1);
}
RX_FUNCTION(0x00752b10, rx_tutorial_load_audio)
void rx_tutorial_unload_audio(void) {
    if (U32(0, TUT_AUDIO_LOADED) == 0) return;
    ACT_CALL(0, rx_tutorial_read_audio_settings());
    const rx_addr d = U32(0, TUT_AUDIO_DB), p = U32(0, TUT_AUDIO_PKG);
    if (!d || !U8(d, 0) || !p || !U8(p, 0)) return;
#ifdef RX_ORACLE
    rx_regs in, o; memset(&in, 0, sizeof in); in.r[1] = p; in.r[7] = d;
    const uint32_t a[1] = {U32(0, 0x00d72a30)};
    rx_call_original_ex(0x0041b600, &in, &o, 1, a);
#endif
    SU32(0, TUT_AUDIO_LOADED, 0);
}
RX_FUNCTION(0x00752b60, rx_tutorial_unload_audio)

/* ---- the profile ------------------------------------------------------------------------------------------------------------------ */

/* 004e8460 PlayerRider_RaiseTutorialsCompleted (EAX = the controller; stdcall lesson; RET 4): the controller's PlayerRider table (the per-device
 * database 0x00ae10f0 + 0x1f4 * controller), row 0's TutorialsCompleted raised to lesson + 1 (Table_SetCellInt: the save's revision) */
void rx_tutorial_raise_completed(uint32_t controller, int32_t lesson) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);                                                /* push esi / edi */
    rx_addr t;
    ACT_CALLR(t, 1, rx_tbl_find(0x00ae10f0u + controller * 0x1f4u, 0x0093e53c /* "PlayerRider" */));
    uint32_t col;
    ACT_CALLR(col, 1, rx_tbl_column_index(t, 0x0093e5d4 /* "TutorialsCompleted" */));
    rx_addr cell;
    ACT_CALLR(cell, 1, rx_tbl_cell_word(t, col, 0));
    const int32_t v = lesson + 1;
    if (I32(cell, 0) < v) {
        SU32(sp0, 4, (uint32_t)v);                                     /* the argument slot holds the value */
        ACT_CALL(2, rx_tbl_set_cell_int(t, 0x0093e5d4, 0, sp0 + 4));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004e8460, rx_tutorial_raise_completed)

/* 007532a0 (EDX = the profile manager [0x00d92f10]): its four profiles' "achievement shown" flags: a signed-in profile (+0x3c[i]) whose
 * byte +0x148 + 0x1138 i is 0 gets it set and +0x45ac + 0x89c i cleared */
void rx_tutorial_profiles_mark(rx_addr pm) {
    for (uint32_t i = 0; i < 4; i++) {
        if (U32(pm, 0x3c + 4 * i) != 0 && U8(pm, 0x148 + 0x1138 * i) == 0) {
            SU8(pm, 0x148 + 0x1138 * i, 1);
            SU32(pm, 0x45ac + 0x89c * i, 0);
        }
    }
}
RX_FUNCTION(0x007532a0, rx_tutorial_profiles_mark)

/* the highlight tracking's profile persistence (006d7980): the original in the oracle (highlight_progress.c needs a Steam provider there) */
static void tut_highlight(uint32_t controller, uint32_t index) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {U32(0, 0x00d92edc), controller, index};
    (void)rx_call_original(0x006d7980, RX_STDCALL, 0, 3, a);
#else
    ACT_CALL(3, (void)rx_highlight_add_progress(U32(0, 0x00d92edc), controller, index));
#endif
}

/* ---- reset / start / outcome / retry ------------------------------------------------------------------------------------------------ */

/* 00751740 TutorialManager_ResetLesson (EAX = m; stdcall first; RET 4) */
void rx_tutorial_reset_lesson(uint32_t first, rx_addr m) {
    SU32(m, 8, 0); SU32(m, 0x64, 0); SU32(m, 0x70, 0); SU32(m, 0x74, 0);
    SF32(m, 0x84, 0.0f); SU32(m, 0x94, 0); SF32(m, 0x88, 2.0f /* 0x92e330 */); SU32(m, 0x18, 0); SU32(m, 0xb0, 0);
    SU32(m, 0xa0, 0); SU32(m, 0xa4, 0); SU32(m, 0xa8, 0); SU32(m, 0xac, 0); SU32(m, 0x98, 0); SU32(m, 0x9c, 0);
    SF32(m, 0x90, 0.0f); SF32(m, 0x8c, 0.0f); SU32(m, 0x1c, 0); SU32(m, 0x20, 0xffffffffu); SU32(m, 0x80, 0);
    if (first) { SU32(m, 0x68, 0); SU32(m, 0x7c, 0); SU32(m, 0x6c, 0); SU32(m, 0x78, 1); }
    SU32(m, 0x1c, 1);
    SU32(m, 0x20, U32(m, 4));
}
RX_FUNCTION(0x00751740, rx_tutorial_reset_lesson)

/* 007517e0 TutorialManager_StartLesson (stdcall m; RET 4): +8 = 2; DYN_TUTORIAL_DESCRIPTION = STR_TUTORIAL_<n>_DESCRIPTION; at the lesson's
 * first start (+0x78) "ShowPreTutorialPopup" and the Intro voice; UI CurrentTutorial = n; DYN_TUTORIAL_ACTIVATION = the step's
 * STR_TUTORIAL_<n>_HUD_DESCRIPTION[_<step>] (lessons 3 / 4 / 6 per step; an out-of-range step keeps the buffer), DYN_TUTORIAL_HUD_LABEL =
 * STR_TUTORIAL_<n>_HUD_LABEL; the start's height on the UI scene's terrain; the two start directions normalized; +0x1c / +0x20 the lesson's
 * objects pending; the player (vehicle +0x190) reset to the start + 2 x the second direction (005501f0); the sounds resumed. Frame: push ebp;
 * and esp, -16; sub esp, 0x134 (the text at +0x30, the start at +0x20, the cookie at +0x130); push ebx / esi / edi. */
void rx_tutorial_start_lesson(rx_addr m) {
    ACT_FRAME_EBP(L, 16, 0x134, 3);
    const rx_addr base = L + 0xc;                                      /* the original's ESP after `sub esp, 0x134` */
    SU32(base, 0x130, U32(0, 0x009b45e4) ^ base);                      /* the /GS cookie */
    const rx_addr text = base + 0x30, pos = base + 0x20;
    SU32(m, 8, 2);
    tut_format(text, 0x0096d0c0 /* "STR_TUTORIAL_%d_DESCRIPTION" */, U32(m, 4) + 1, 1);
    rx_tutorial_ui_dynamic_text(0x0096d0dc /* "DYN_TUTORIAL_DESCRIPTION" */, text);
    if (U32(m, 0x78) == 1) {
        rx_tutorial_ui_raise(0x0096d0f8 /* "ShowPreTutorialPopup" */);
        ACT_CALL(2, rx_tutorial_play_lesson_voice(I32(m, 4), 0));
        SU32(m, 0x78, 0);
    }
    rx_tutorial_ui_set_int(0x0096d110 /* "CurrentTutorial" */, U32(m, 4) + 1);
    rx_addr src = 0;
    const uint32_t lesson = U32(m, 4), step = U32(m, 0x7c);
    if (lesson == 3) src = step == 0 ? 0x0096d120 : step == 1 ? 0x0096d144 : step == 2 ? 0x0096d168 : 0;
    else if (lesson == 5) src = step == 0 ? 0x0096d18c : step == 1 ? 0x0096d1b0 : step == 2 ? 0x0096d1d4 : 0;
    else if (lesson == 2) src = step == 0 ? 0x0096d1f8 : step == 1 ? 0x0096d21c : 0;
    else tut_format(text, 0x0096d240 /* "STR_TUTORIAL_%d_HUD_DESCRIPTION" */, lesson + 1, 1);
    if (src) { crt_strncpy(text, src, 0x103); SU8(text, 0x103, 0); }
    rx_tutorial_ui_dynamic_text(0x0096d260 /* "DYN_TUTORIAL_ACTIVATION" */, text);
    tut_format(text, 0x0096d278 /* "STR_TUTORIAL_%d_HUD_LABEL" */, U32(m, 4) + 1, 1);
    rx_tutorial_ui_dynamic_text(0x0096d294 /* "DYN_TUTORIAL_HUD_LABEL" */, text);
    SU32(pos, 0, U32(m, 0x30)); SU32(pos, 4, U32(m, 0x34)); SU32(pos, 8, U32(m, 0x38));
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    rx_addr terrain = 0;
    {   /* the terrain of the UI scene (the inlined 0042d960 over the list 0x009e30d4) */
        const rx_addr scene = U32(U32(0, CTX), 0x1c);
        for (uint32_t i = 0; i < U32(0, 0x009e30dc); i++) {
            const rx_addr t = U32(0, 0x009e30d4 + 4 * i);
            if (t && scene == U32(t, 0x44a8)) { terrain = t; break; }
        }
    }
    { uint32_t edx; ACT_CALL(1, (void)phys_terrain_base_height_at(terrain, pos, &edx)); }
    {
        const float x = F32(m, 0x40), y = F32(m, 0x44), z = F32(m, 0x48);
        const float l2 = x * x + y * y + z * z;
        if (l2 > 0.0f) {
            float s; ACT_CALLR(s, 1, phys_sqrtf(l2));
            const float r = 1.0f / s;
            SF32(m, 0x40, x * r); SF32(m, 0x44, r * F32(m, 0x44)); SF32(m, 0x48, r * F32(m, 0x48));
        }
    }
    {
        const float x = F32(m, 0x50), y = F32(m, 0x54), z = F32(m, 0x58);
        const float l2 = x * x + y * y + z * z;
        if (l2 > 0.0f) {
            float s; ACT_CALLR(s, 1, phys_sqrtf(l2));
            const float r = 1.0f / s;
            SF32(m, 0x50, x * r); SF32(m, 0x54, F32(m, 0x54) * r); SF32(m, 0x58, F32(m, 0x58) * r);
        }
    }
    SU32(m, 0x20, U32(m, 4));
    SU32(m, 0x1c, 1);
    SF32(pos, 0, F32(m, 0x50) * 2.0f + F32(pos, 0));
    SF32(pos, 4, F32(m, 0x54) * 2.0f + F32(pos, 4));
    SF32(pos, 8, F32(m, 0x58) * 2.0f + F32(pos, 8));
    ACT_CALL(1, rx_player_reset_to(U32(U32(m, 0x24), 0x190), pos, m + 0x40));
    ACT_CALL(2, rx_audio_pause_all(0, 0));
    ACT_END(L);
}
RX_FUNCTION(0x007517e0, rx_tutorial_start_lesson)

/* 00752740 TutorialManager_OnRetry (ECX = m; "RetryTutorial", and the next step of lessons 3 / 4): the manager's voice stopped, the lesson
 * reset (from the start when the step is 0), started again, "ShowTutorialHUD". Frame: sub esp, 8; push ebx / esi / edi. */
void rx_tutorial_on_retry(rx_addr m) {
    ACT_FRAME(L, 8, 3);
    tut_stop_voice();
    ACT_CALL(1, rx_tutorial_reset_lesson(U32(m, 0x7c) == 0, m));
    ACT_CALL(1, rx_tutorial_start_lesson(m));
    rx_tutorial_ui_raise(0x0096d398 /* "ShowTutorialHUD" */);
    ACT_END(L);
}
RX_FUNCTION(0x00752740, rx_tutorial_on_retry)

/* 007520a0 TutorialManager_Outcome (stdcall m; RET 4). A pass of lesson 3's first step (step 1 next) and of lesson 4's jumps while fewer than
 * 2 bad / 3 steps / 2 good: the next step (OnRetry); lesson 4 with 2 bad jumps: a fail. A pass: PlayerRider.TutorialsCompleted (004e8460),
 * DYN_TUTORIAL_COMPLETION, the Completed voice, "ShowPostTutorialPopup", the next lesson (after 3: highlight 0x1e; after 6: lesson 0, the profiles
 * marked (007532a0), highlight 0x1f). A fail: STR_TUTORIAL_FAILED_BOUNDARY (+0x74, voice BoundryExceeded) / _<n>_FAILED_WRECK (+0x70, Wrecked) /
 * _<n>_FAILED (Fail) into DYN_TUTORIAL_FAILED, "ShowFailedTutorialPopup". Both: +0x1c = 1 / +0x20 = -1 (every lesson object hidden next frame),
 * the player's +0x1ec cleared, the sounds paused (Sound_PauseAll(1, 0)). Frame: push ebp; and esp, -8; sub esp, 0x11c; push ebx / esi / edi. */
void rx_tutorial_outcome(rx_addr m) {
    ACT_FRAME_EBP(L, 8, 0x11c, 3);
    const rx_addr base = L + 0xc;
    SU32(base, 0x118, U32(0, 0x009b45e4) ^ base);
    const rx_addr text = base + 0x18;
    if (U32(m, 0x64) != 0) {
        const uint32_t lesson = U32(m, 4);
        if (lesson == 2) {
            if (U32(m, 0x7c) == 0) { SU32(m, 0x7c, 1); ACT_CALL(0, rx_tutorial_on_retry(m)); ACT_END(L); return; }
        } else if (lesson == 3) {
            if (I32(m, 0x68) > 1) { SU32(m, 0x7c, 0); SU32(m, 0x64, 0); }
            else if (I32(m, 0x7c) < 3) {
                if (I32(m, 0x6c) >= 2) { ACT_END(L); return; }
                ACT_CALL(0, rx_tutorial_on_retry(m));
                ACT_END(L);
                return;
            }
        }
    }
    const rx_addr d = tut_local_driver(tut_active_world());
    const rx_addr P = U32(d, 0x74);
    SU32(base, 0x14, P);
    if (U32(m, 0x64) == 0) {
        if (U32(m, 4) == 3) {
            if (I32(m, 0x68) > 1) SU32(m, 0x7c, 0);
            if (I32(m, 0x7c) - I32(m, 0x6c) >= 2) SU32(m, 0x7c, 0);
        }
        if (U32(m, 0x74) != 0) {
            tut_format(text, 0x0096d2bc /* "STR_TUTORIAL_FAILED_BOUNDARY" */, 0, 0);
            ACT_CALL(2, rx_tutorial_play_lesson_voice(I32(m, 4), 1));
        } else if (U32(m, 0x70) != 0) {
            tut_format(text, 0x0096d2dc /* "STR_TUTORIAL_%d_FAILED_WRECK" */, U32(m, 4) + 1, 1);
            ACT_CALL(2, rx_tutorial_play_lesson_voice(I32(m, 4), 4));
        } else {
            tut_format(text, 0x0096d2fc /* "STR_TUTORIAL_%d_FAILED" */, U32(m, 4) + 1, 1);
            ACT_CALL(2, rx_tutorial_play_lesson_voice(I32(m, 4), 3));
        }
        rx_tutorial_ui_dynamic_text(0x0096d314 /* "DYN_TUTORIAL_FAILED" */, text);
        rx_tutorial_ui_raise(0x0096d328 /* "ShowFailedTutorialPopup" */);
    } else {
        ACT_CALL(1, rx_tutorial_raise_completed(U32(P, 0x1f0), I32(m, 4)));
        tut_format(text, 0x0096d340 /* "STR_TUTORIAL_%d_COMPLETION" */, U32(m, 4) + 1, 1);
        ACT_CALL(2, rx_tutorial_play_lesson_voice(I32(m, 4), 2));
        rx_tutorial_ui_dynamic_text(0x0096d35c /* "DYN_TUTORIAL_COMPLETION" */, text);
        rx_tutorial_ui_raise(0x0096d374 /* "ShowPostTutorialPopup" */);
        switch (U32(m, 4)) {
        case 0: SU32(m, 4, 1); break;
        case 1: SU32(m, 4, 2); break;
        case 2: SU32(m, 4, 3); tut_highlight(U32(P, 0x1f0), 0x1e); break;
        case 3: SU32(m, 4, 4); break;
        case 4: SU32(m, 4, 5); break;
        case 5:
            SU32(m, 4, 0);
            rx_tutorial_profiles_mark(U32(0, 0x00d92f10));
            tut_highlight(U32(P, 0x1f0), 0x1f);
            break;
        default: break;
        }
    }
    SU32(m, 0x1c, 1);
    SU32(m, 0x20, 0xffffffffu);
    SU32(P, 0x1ec, 0);
    ACT_CALL(2, rx_audio_pause_all(1, 0));
    ACT_END(L);
}
RX_FUNCTION(0x007520a0, rx_tutorial_outcome)

/* ---- the checks ----------------------------------------------------------------------------------------------------------------------- */

static int tut_paused(void) { return U32(tut_active_world(), 0x20014) != 0; }

/* 00750a70 (stdcall m; RET 4): the vehicle's landing failed: the world runs, the vehicle (+0x24) crashed (+0x280) and the local player's
 * reset timer (+0x116c) and wreck timer (+0xb38) are not positive */
uint32_t rx_tutorial_landing_failed(rx_addr m) {
    if (tut_paused()) return 0;
    if (U32(U32(m, 0x24), 0x280) == 0) return 0;
    const rx_addr P = U32(tut_local_driver(tut_active_world()), 0x74);
    return (!(F32(P, 0x116c) > 0.0f) && !(F32(P, 0xb38) > 0.0f)) ? 1u : 0u;
}
RX_FUNCTION(0x00750a70, rx_tutorial_landing_failed)

/* 00750b30 TutorialManager_CheckWreck (ESI = m; stdcall dt; RET 4): the 2 s wait after a wreck (+0x84) runs out -> a fail (Outcome); a new
 * wreck (the vehicle crashed, the player's reset / wreck timers not positive) starts it and marks +0x70 */
void rx_tutorial_check_wreck(float dt, rx_addr m) {
    if (tut_paused()) return;
    const float w = F32(m, 0x84);
    if (w > 0.0f) {
        const float r = w - dt;
        SF32(m, 0x84, r);
        if (!(0.0f < r)) { SU32(m, 0x64, 0); ACT_CALL(0, rx_tutorial_outcome(m)); }
        return;
    }
    if (U32(U32(m, 0x24), 0x280) == 0) return;
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 8);
    rx_addr world;
    ACT_CALLR(world, 0, rx_world_active_sound_table(U32(0, GAME)));
    rx_set_sp(sp);
    const rx_addr P = U32(tut_local_driver(world), 0x74);
    if (!(F32(P, 0x116c) > 0.0f) && !(F32(P, 0xb38) > 0.0f)) {
        SF32(m, 0x84, 2.0f /* 0x92e330 */);
        SU32(m, 0x70, 1);
    }
}
RX_FUNCTION(0x00750b30, rx_tutorial_check_wreck)

/* 007515b0 TutorialManager_CheckRiderReflex (stdcall m; RET 4): lesson 1. The lean's back / forward axis y = -(input 7 of the second map),
 * the side axis x (00548580): y > 0.8 marks back (+0xa8), y < -0.8 forward (+0xa0), else x > 0.8 right (+0xac), x < -0.8 left (+0xa4); all four
 * -> a pass. */
void rx_tutorial_check_rider_reflex(rx_addr m) {
    if (tut_paused()) return;
    const rx_addr d = tut_local_driver(tut_active_world());
    const uint32_t pad = U32(U32(d, 0x74), 0x1f0);
    const float y = tut_lean_y(pad);
    const float x = tut_lean_x(U32(U32(d, 0x74), 0x1f0));
    if (y > 0.8f /* 0x9cc8a8 */) SU32(m, 0xa8, 1);
    else if (-0.8f /* 0x9776e4 */ > y) SU32(m, 0xa0, 1);
    else if (x > 0.8f) SU32(m, 0xac, 1);
    else if (-0.8f > x) SU32(m, 0xa4, 1);
    if (U32(m, 0xa0) && U32(m, 0xa8) && U32(m, 0xac) && U32(m, 0xa4)) {
        SU32(m, 0x64, 1);
        ACT_CALL(0, rx_tutorial_outcome(m));
    }
}
RX_FUNCTION(0x007515b0, rx_tutorial_check_rider_reflex)

/* 00750c00 TutorialManager_CheckHoldTight (stdcall m, dt; RET 8): lesson 2. The time leaned back (y > 0.8) summed in +0x90; 2 s -> passed
 * (judged at the Finish trigger) */
void rx_tutorial_check_hold_tight(rx_addr m, float dt) {
    if (tut_paused()) return;
    const rx_addr d = tut_local_driver(tut_active_world());
    if (tut_lean_y(U32(U32(d, 0x74), 0x1f0)) > 0.8f) SF32(m, 0x90, F32(m, 0x90) + dt);
    if (F32(m, 0x90) >= 2.0f) SU32(m, 0x64, 1);
}
RX_FUNCTION(0x00750c00, rx_tutorial_check_hold_tight)

/* 00750d00 TutorialManager_CheckLeanIn (stdcall m, dt; RET 8): lesson 3. The lean's side axis below -0.8 sums +0x8c; the steer map's input 7
 * below -0.8 marks +0xb0; step 0 passes, step 1 needs 1 s and the mark (else not passed). */
void rx_tutorial_check_lean_in(rx_addr m, float dt) {
    if (tut_paused()) return;
    const rx_addr d = tut_local_driver(tut_active_world());
    const float x = tut_lean_x(U32(U32(d, 0x74), 0x1f0));
    const uint32_t pad = U32(U32(d, 0x74), 0x1f0);
    const rx_addr a = rx_world_pad_steer_map(pad), b = rx_world_pad_lean_map(pad);
    const float s = tut_keymap_value(a, pad);
    (void)tut_keymap_value(b, pad);
    if (-0.8f > x) SF32(m, 0x8c, F32(m, 0x8c) + dt);
    if (-0.8f > s) SU32(m, 0xb0, 1);
    const uint32_t step = U32(m, 0x7c);
    if (step == 0) { SU32(m, 0x64, 1); return; }
    if (step != 1) return;
    if (F32(m, 0x8c) >= 1.0f && U32(m, 0xb0) != 0) SU32(m, 0x64, 1);
    else SU32(m, 0x64, 0);
}
RX_FUNCTION(0x00750d00, rx_tutorial_check_lean_in)

/* the vehicle's position (vf 0x44 into out) and its height above the UI scene's terrain point (the copy's y from 007c5950 through 0042d960's
 * terrain): |pos - ground|. The frame of the three checks below: push ebp; and esp, -16; sub esp, 0x28; push ebx / edi (pos at +0x10, the
 * ground point at +0x20). */
static float tut_height(rx_addr m, rx_addr base) {
    const rx_addr pos = base + 0x10, gp = base + 0x20;
    { const uint32_t a[1] = {pos}; (void)loop_tc1(loop_vslot(U32(m, 0x24), 0x44), U32(m, 0x24), a[0]); }
    SU32(gp, 0, U32(pos, 0)); SU32(gp, 4, U32(pos, 4)); SU32(gp, 8, U32(pos, 8));
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    rx_addr t;
    ACT_CALLR(t, 1, rx_terrain_for_scene(0x009e30d4, U32(U32(0, CTX), 0x1c)));
    { uint32_t edx; ACT_CALL(1, (void)phys_terrain_base_height_at(t, gp, &edx)); }
    const float dz = F32(pos, 8) - F32(gp, 8), dy = F32(pos, 4) - F32(gp, 4), dx = F32(pos, 0) - F32(gp, 0);
    const float l2 = (dz * dz + dy * dy) + dx * dx;
    float r; ACT_CALLR(r, 1, phys_sqrtf(l2));
    return r;
}

/* 00750e60 TutorialManager_CheckTakeOffAndLanding (ESI = m; stdcall dt; RET 4): lesson 4. While no lesson-objects pass is pending: the landing
 * verdict once (00750a70), the lean back past 0.8 marks +0xa8, 8 ft above the ground marks +0x98; in the air or crashed: +0x18 (the boundary does
 * not count) and, back on the ground or crashed, the jump is judged (+0x94) after the 2 s timer: a crash = a bad jump (+0x68, +0x70), a lean back
 * = a good one (+0x6c, passed), else bad; the step on (+0x7c) and the Outcome. */
void rx_tutorial_check_take_off_and_landing(float dt, rx_addr m) {
    ACT_FRAME_EBP(L, 16, 0x28, 2);
    const rx_addr base = L + 8;
    if (tut_paused() || U32(m, 0x1c) != 0) { ACT_END(L); return; }
    const float h = tut_height(m, base);
    const rx_addr d = tut_local_driver(tut_active_world());
    if (U32(m, 0x80) == 0) { uint32_t f; ACT_CALLR(f, 1, rx_tutorial_landing_failed(m)); SU32(m, 0x80, f); }
    const float y = tut_lean_y(U32(U32(d, 0x74), 0x1f0));
    (void)tut_lean_x(U32(U32(d, 0x74), 0x1f0));
    if (y > 0.8f) SU32(m, 0xa8, 1);
    if (h > 8.0f /* 0x9322b8 */) SU32(m, 0x98, 1);
    if (U32(m, 0x98) != 0 || U32(m, 0x80) != 0) {
        SU32(m, 0x18, 1);
        const uint32_t air = loop_tc0(loop_vslot(U32(m, 0x24), 0xac), U32(m, 0x24));
        if (air == 0 || U32(m, 0x80) != 0) SU32(m, 0x94, 1);
    }
    if (U32(m, 0x94) != 0) {
        const float r = F32(m, 0x88) - dt;
        SF32(m, 0x88, r);
        if (!(0.0f < r)) {
            if (U32(m, 0x80) != 0) {
                SU32(m, 0x68, U32(m, 0x68) + 1);
                SU32(m, 0x70, 1);
            } else if (U32(m, 0xa8) != 0) {
                SU32(m, 0x6c, U32(m, 0x6c) + 1);
                SU32(m, 0x7c, U32(m, 0x7c) + 1);
                SU32(m, 0x64, 1);
                ACT_CALL(1, rx_tutorial_outcome(m));
                ACT_END(L);
                return;
            }
            SU32(m, 0x7c, U32(m, 0x7c) + 1);
            SU32(m, 0x64, 0);
            ACT_CALL(1, rx_tutorial_outcome(m));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00750e60, rx_tutorial_check_take_off_and_landing)

/* 006381c0 (EAX = the rider's balance request, rider + 0x5b54c; ECX = 0..11): the request's kind (+0x1c: 0xc / 0xb / 0xf / 0x10) and
 * direction (+0x20: 0 / 1 / 2) of the jump table 0x0063827c */
void rx_tutorial_knock_request(rx_addr req, uint32_t index) {
    static const uint8_t kind[4] = {0xc, 0xb, 0xf, 0x10};
    SU32(req, 0x1c, kind[index / 3]);
    SU32(req, 0x20, index % 3);
}

/* 007510c0 TutorialManager_CheckWreckAvoidance (ESI = m; stdcall dt; RET 4): lesson 5. 4 ft above the ground marks +0x98; then on the ground
 * and not crashed, the player's reset timer not positive: knocked once (the game LCG 0x009cb860: one of four balance requests on the rider),
 * then staying up (wreck timer not positive) for the 2 s timer -> a pass. */
void rx_tutorial_check_wreck_avoidance(float dt, rx_addr m) {
    ACT_FRAME_EBP(L, 16, 0x28, 2);
    const rx_addr base = L + 8;
    if (tut_paused()) { ACT_END(L); return; }
    const float h = tut_height(m, base);
    if (h > 4.0f /* 0x9322c0 */) SU32(m, 0x98, 1);
    if (U32(m, 0x98) == 0) { ACT_END(L); return; }
    const uint32_t air = loop_tc0(loop_vslot(U32(m, 0x24), 0xac), U32(m, 0x24));
    if (air != 0 || U32(U32(m, 0x24), 0x280) != 0) { ACT_END(L); return; }
    const rx_addr sp = rx_sp();
    rx_set_sp(sp - 8);
    rx_addr world;
    ACT_CALLR(world, 0, rx_world_active_sound_table(U32(0, GAME)));
    rx_set_sp(sp);
    const rx_addr d = tut_local_driver(world);
    const rx_addr P = U32(d, 0x74);
    if (F32(P, 0x116c) > 0.0f) { ACT_END(L); return; }
    if (U32(m, 0x9c) != 0) {
        if (!(F32(P, 0xb38) > 0.0f)) {
            const float r = F32(m, 0x88) - dt;
            SF32(m, 0x88, r);
            if (!(0.0f < r)) { SU32(m, 0x64, 1); ACT_CALL(1, rx_tutorial_outcome(m)); }
            ACT_END(L);
            return;
        }
        ACT_END(L);
        return;
    }
    SU32(0, 0x009cb864, U32(0, 0x009cb864) + 1);
    const uint32_t seed = U32(0, 0x009cb860) * 0x41c64e6du + 0x3039u;
    SU32(0, 0x009cb860, seed);
    const int32_t r = (int32_t)((seed >> 16) & 0x7fffu) % 4;
    static const uint8_t index[4] = {8, 0xb, 2, 5};
    rx_tutorial_knock_request(U32(U32(d, 0x74), 0x1b4) + 0x5b54c, index[r]);
    SU32(m, 0x9c, 1);
    ACT_END(L);
}
RX_FUNCTION(0x007510c0, rx_tutorial_check_wreck_avoidance)

/* 00751320 TutorialManager_CheckFreestyleTricks (ESI = m; stdcall dt; RET 4): lesson 6. 8 ft above the ground: the player's trick (+0xb74 its
 * name, while +0xbc0 / +0xb74) against the step's (Shaolin_air, doublecancan_L_air, Deadbody_air: _stricmp) -> passed; +0x98. Back on the ground,
 * the 2 s timer (+0x18 set): a passed third step -> the Outcome; else the step's flags cleared and the lesson started again at its start
 * (the step on after a pass). */
void rx_tutorial_check_freestyle_tricks(float dt, rx_addr m) {
    ACT_FRAME_EBP(L, 16, 0x28, 2);
    const rx_addr base = L + 8;
    if (tut_paused()) { ACT_END(L); return; }
    const rx_addr d = tut_local_driver(tut_active_world());
    const float h = tut_height(m, base);
    if (h > 8.0f) {
        const rx_addr P = U32(d, 0x74);
        const rx_addr name = U32(P, 0xb74);
        if ((U32(P, 0xbc0) != 0 || name != 0) && name != 0) {
            const uint32_t step = U32(m, 0x7c);
            if (step == 0) {
                if (crt_stricmp(name, 0x0096d090 /* "Shaolin_air" */) == 0) SU32(m, 0x64, 1);
            } else if (step == 1 || step == 2) {
                if (crt_stricmp(name, step == 1 ? 0x0096d09c /* "doublecancan_L_air" */ : 0x0096d0b0 /* "Deadbody_air" */) == 0) SU32(m, 0x64, 1);
            }
        }
        SU32(m, 0x98, 1);
    }
    if (U32(m, 0x98) != 0) {
        const uint32_t air = loop_tc0(loop_vslot(U32(m, 0x24), 0xac), U32(m, 0x24));
        if (air == 0) {
            const float r = F32(m, 0x88) - dt;
            SU32(m, 0x18, 1);
            SF32(m, 0x88, r);
            if (!(0.0f < r)) {
                if (U32(m, 0x64) != 0) {
                    SU32(m, 0x7c, U32(m, 0x7c) + 1);
                    if (I32(m, 0x7c) >= 3) { SU32(m, 0x64, 1); ACT_CALL(1, rx_tutorial_outcome(m)); ACT_END(L); return; }
                }
                SF32(m, 0x84, 0.0f); SF32(m, 0x88, 2.0f);
                SU32(m, 0xa0, 0); SU32(m, 0xa4, 0); SU32(m, 0xa8, 0); SU32(m, 0xac, 0); SU32(m, 0x98, 0); SU32(m, 0x9c, 0);
                SF32(m, 0x90, 0.0f); SF32(m, 0x8c, 0.0f); SU32(m, 0x64, 0);
                ACT_CALL(1, rx_tutorial_start_lesson(m));
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00751320, rx_tutorial_check_freestyle_tricks)

/* ---- the update ----------------------------------------------------------------------------------------------------------------------- */

/* 007500c0 TutorialManager_Update (stdcall m, dt; RET 8), from MXUIWorld::vf5. While the manager runs (+0 == 0): with the local player of
 * the world's FreeRide: a pending lesson-objects pass once the player's reset is over (+0x116c not positive); a popup up (00743120): the
 * player's input groups off, the vehicle held (sim +0x54 = 1), +0xb4 = 1, done; else the groups on. +0xb4 cleared. Not valid (+0x60 == 0):
 * the groups off, the vehicle held. State 1: input action 7 (block +0x25dc vf1) starts the lesson; state 2: the lesson's check (with the
 * wreck check, except lesson 4). Frame: push ebp; and esp, -8; push ecx; push ebx / esi / edi. */
void rx_tutorial_update(rx_addr m, float dt) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    if (U32(m, 0) != 0) { ACT_END(L); return; }
    const rx_addr world = tut_active_world();
    rx_addr d = 0;
    rx_addr act;
    {
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 4);
        ACT_CALLR(act, 1, rx_activity_find_by_kind_area(world + 0x21da0, 8, 0));
        if (act) {
            ACT_CALLR(d, 1, rx_activity_human_driver(act, 0));
        }
        rx_set_sp(sp);
    }
    SU32(L, 0xc, d);
    if (act && d && U32(d, 0x74)) {
        const rx_addr P = U32(d, 0x74);
        if (U32(m, 0x1c) != 0 && !(F32(P, 0x116c) > 0.0f)) {
            ACT_CALL(2, rx_tutorial_show_lesson_objects(m, I32(m, 0x20)));
            SU32(m, 0x1c, 0);
        }
        uint32_t popup;
        ACT_CALLR(popup, 0, rx_tutorial_ui_popup_active());
        if (popup) {
            tut_groups(P, 0);
            SU32(U32(U32(P, 0x1ac), 0x234), 0x54, 1);
            SU32(m, 0xb4, 1);
            ACT_END(L);
            return;
        }
        if (I32(P, 0x1f0) >= 0) tut_groups(P, 1);
    }
    if (U32(m, 0xb4) != 0) SU32(m, 0xb4, 0);
    if (U32(m, 0x60) == 0) {
        if (d && U32(d, 0x74)) {
            const rx_addr P = U32(d, 0x74);
            tut_groups(P, 0);
            SU32(U32(U32(P, 0x1ac), 0x234), 0x54, 1);
        }
        ACT_END(L);
        return;
    }
    const uint32_t state = U32(m, 8);
    if (state == 1) {
        if (d) {
            const uint32_t pad = U32(U32(d, 0x74), 0x1f0);
            const rx_addr km = tut_input_block(pad) + 0x25dc;
            const uint32_t v = loop_tc2(loop_vslot(km, 4), km, 7, pad);
            if (v != 0) ACT_CALL(1, rx_tutorial_start_lesson(m));
        }
    } else if (state == 2) {
        switch (U32(m, 4)) {
        case 0: ACT_CALL(1, rx_tutorial_check_wreck(dt, m)); ACT_CALL(1, rx_tutorial_check_rider_reflex(m)); break;
        case 1: ACT_CALL(1, rx_tutorial_check_wreck(dt, m)); ACT_CALL(2, rx_tutorial_check_hold_tight(m, dt)); break;
        case 2: ACT_CALL(1, rx_tutorial_check_wreck(dt, m)); ACT_CALL(2, rx_tutorial_check_lean_in(m, dt)); break;
        case 3: ACT_CALL(1, rx_tutorial_check_take_off_and_landing(dt, m)); break;
        case 4: ACT_CALL(1, rx_tutorial_check_wreck(dt, m)); ACT_CALL(1, rx_tutorial_check_wreck_avoidance(dt, m)); break;
        case 5: ACT_CALL(1, rx_tutorial_check_wreck(dt, m)); ACT_CALL(1, rx_tutorial_check_freestyle_tricks(dt, m)); break;
        default: break;
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x007500c0, rx_tutorial_update)

/* ---- the events ----------------------------------------------------------------------------------------------------------------------- */

/* the valid word (+0x60): the manager runs (+0 == 0) and the player count (+0xc), vehicle (+0x10) and UI world (+0x14) words are set */
static void tut_validate(rx_addr m) {
    SU32(m, 0x60, 1);
    if (U32(m, 0) != 0) SU32(m, 0x60, 0);
    if (U32(m, 0x10) == 0) SU32(m, 0x60, 0);
    if (U32(m, 0xc) == 0) SU32(m, 0x60, 0);
    if (U32(m, 0x14) == 0) SU32(m, 0x60, 0);
}

/* 0074fea0 (ESI = m; stdcall lesson; RET 4): the lesson on (+4, +0 = 0), reset from its start; the UI scene's trigger (0x1028) named
 * "TUT0<n>_Start_<lesson name>" for it (unless a lesson already runs: +8 == 2) gives the start: its position (vf 0x44 into +0x30) and its
 * matrix's z / y rows (+0x40 / +0x50); the local player's vehicle (+0x24) from the world's FreeRide; the lesson started; the valid word.
 * Frame: sub esp, 0x1d4 (the count at +4, the list at +8 (0xc4), the name at +0xd0, the cookie at +0x1d0); push ebp (ebx / edi in the loop). */
void rx_tutorial_enable_lesson(int32_t lesson, rx_addr m) {
    ACT_FRAME(L, 0x1d4, 1);
    SU32(L, 0x1d4, U32(0, 0x009b45e4) ^ (L + 4));
    SU32(m, 4, (uint32_t)lesson);
    SU32(m, 0, 0);
    ACT_CALL(1, rx_tutorial_reset_lesson(1, m));
    const rx_addr list = L + 8, name = L + 0xd0;
    for (uint32_t i = 0; i < 0xc8; i++) SU8(list, i, 0);
    int32_t n;
    ACT_CALLR(n, 4, (int32_t)rx_scene_manager_collect_typed(U32(U32(0, CTX), 0x1c), 0x1028, list, 0x32, 0));
    if (n <= 0) { ACT_END(L); return; }
    rx_set_sp(rx_sp() - 8);                                            /* push ebx / edi */
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = U32(list, 4u * (uint32_t)i);
        const rx_addr t2 = tut_second_token(name, U32(e, 0x18));
        if (!t2 || crt_stricmp(t2, 0x00941b94 /* "Start" */) != 0) continue;
        if (U32(m, 8) == 2) break;
        const rx_addr rest = tut_strtok(0, S_EMPTY);
        int32_t k;
        ACT_CALLR(k, 1, rx_tutorial_lesson_of_name(rest));
        if (k != I32(m, 4)) continue;
        if (!e) break;
        { const uint32_t a[1] = {m + 0x30}; (void)loop_tc1(loop_vslot(e, 0x44), e, a[0]); }
        const rx_addr mx = loop_tc0(loop_vslot(e, 0x38), e);
        SU32(m, 0x40, U32(mx, 0x20)); SU32(m, 0x44, U32(mx, 0x24)); SU32(m, 0x48, U32(mx, 0x28));
        SU32(m, 0x50, U32(mx, 0x10)); SU32(m, 0x54, U32(mx, 0x14)); SU32(m, 0x58, U32(mx, 0x18));
        const rx_addr w = tut_active_world();
        rx_addr fr = 0;
        for (int32_t j = 0; j < I32(w, 0x21da8); j++) {
            const rx_addr a = U32(U32(w, 0x21da4), 4u * (uint32_t)j);
            if (U32(a, 0x23c) == 8 && U32(a, 0x220) == 0) { fr = a; break; }
        }
        const rx_addr d = tut_driver_of(fr);
        SU32(m, 0x24, U32(U32(d, 0x74), 0x1ac));
        ACT_CALL(1, rx_tutorial_start_lesson(m));
        tut_validate(m);
        break;
    }
    ACT_END(L);
}
RX_FUNCTION(0x0074fea0, rx_tutorial_enable_lesson)

/* 00752630 the "EnableTutorialManager" handler (ECX = m; stdcall lesson; RET 4): the lesson on (0074fea0), every lesson object hidden */
void rx_tutorial_on_enable(rx_addr m, int32_t lesson) {
    rx_set_sp(rx_sp() - 4);                                            /* push esi */
    ACT_CALL(1, rx_tutorial_enable_lesson(lesson, m));
    ACT_CALL(2, rx_tutorial_show_lesson_objects(m, -1));
    rx_set_sp(rx_sp() + 4);
}
RX_FUNCTION(0x00752630, rx_tutorial_on_enable)

/* 00752650 the "DisableTutorialManager" handler (ECX = m): not running, not valid; every lesson object hidden */
void rx_tutorial_on_disable(rx_addr m) {
    SU32(m, 0, 1);
    SU32(m, 0x60, 0);
    ACT_CALL(2, rx_tutorial_show_lesson_objects(m, -1));
}
RX_FUNCTION(0x00752650, rx_tutorial_on_disable)

/* 00752680 / 007526c0 / 00752700: "SetValidPlayerCount" (+0xc), "SetValidVehicle" (+0x10), "InUIWorld" (+0x14) (ECX = m; stdcall value; RET 4),
 * then the valid word */
void rx_tutorial_on_valid_player_count(rx_addr m, uint32_t v) { SU32(m, 0xc, v); tut_validate(m); }
RX_FUNCTION(0x00752680, rx_tutorial_on_valid_player_count)
void rx_tutorial_on_valid_vehicle(rx_addr m, uint32_t v) { SU32(m, 0x10, v); tut_validate(m); }
RX_FUNCTION(0x007526c0, rx_tutorial_on_valid_vehicle)
void rx_tutorial_on_in_ui_world(rx_addr m, uint32_t v) { SU32(m, 0x14, v); tut_validate(m); }
RX_FUNCTION(0x00752700, rx_tutorial_on_in_ui_world)

/* the lesson triggers' name: "TUT0<n>" then the kind, then the lesson's name (the rest): the lesson number, or -1 when the kind differs */
static int32_t tut_trigger_lesson(rx_addr buf, rx_addr trigger, rx_addr kind) {
    const rx_addr t2 = tut_second_token(buf, U32(trigger, 0x18));
    if (!t2 || crt_stricmp(t2, kind) != 0) return -2;
    return 0;
}

/* 007523f0 the trigger registry's "VehicleEntered" handler (ECX = m; stdcall vehicle, trigger; RET 8): in the UI world (+0x20018), the manager
 * valid, a vehicle and a trigger: a running lesson's "TUT0<n>_Finish_<lesson>" -> the Outcome. Frame: sub esp, 0x108; push esi / edi. */
void rx_tutorial_on_vehicle_entered(rx_addr m, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME(L, 0x108, 2);
    const rx_addr base = L + 8;
    SU32(base, 0x104, U32(0, 0x009b45e4) ^ base);
    const rx_addr w = tut_active_world();
    if (U32(w, 0x20018) != 0 && U32(m, 0x60) != 0 && vehicle != 0 && trigger != 0) {
        if (tut_trigger_lesson(base, trigger, 0x00941eac /* "Finish" */) == 0 && U32(m, 8) == 2) {
            const rx_addr rest = tut_strtok(0, S_EMPTY);
            int32_t k;
            ACT_CALLR(k, 1, rx_tutorial_lesson_of_name(rest));
            if (k == I32(m, 4)) ACT_CALL(1, rx_tutorial_outcome(m));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x007523f0, rx_tutorial_on_vehicle_entered)

/* 00752500 the "VehicleExited" handler: a running lesson's "TUT0<n>_Boundary_<lesson>" left while the boundary counts (+0x18 == 0) and no popup
 * is up -> left the area (+0x74), not passed, the Outcome. Frame: sub esp, 0x108; push esi (edi in the test). */
void rx_tutorial_on_vehicle_exited(rx_addr m, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME(L, 0x108, 1);
    const rx_addr base = L + 4;
    SU32(base, 0x104, U32(0, 0x009b45e4) ^ base);
    const rx_addr w = tut_active_world();
    if (U32(w, 0x20018) != 0 && U32(m, 0x60) != 0 && vehicle != 0 && trigger != 0) {
        rx_set_sp(rx_sp() - 4);                                        /* push edi */
        if (tut_trigger_lesson(base, trigger, 0x0096d38c /* "Boundary" */) == 0 && U32(m, 0x18) == 0) {
            const rx_addr rest = tut_strtok(0, S_EMPTY);
            int32_t k;
            ACT_CALLR(k, 1, rx_tutorial_lesson_of_name(rest));
            if (k == I32(m, 4) && U32(m, 8) == 2) {
                uint32_t popup;
                ACT_CALLR(popup, 0, rx_tutorial_ui_popup_active());
                if (popup == 0) { SU32(m, 0x74, 1); SU32(m, 0x64, 0); ACT_CALL(1, rx_tutorial_outcome(m)); }
            }
        }
        rx_set_sp(rx_sp() + 4);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00752500, rx_tutorial_on_vehicle_exited)

/* ---- the construction ----------------------------------------------------------------------------------------------------------------- */

/* the three handler templates 00752bb0 (void (int)), 00752c70 (void ()) on the manager's channel and 00752d30 (void (RavenVehicle*, const
 * MXTrigger*)) on the trigger registry [0x00a89bc8] (ECX = the event's name; stdcall object, method; RET 8): the proxy {vtable, 0, the
 * signature's name, object, method} appended to the event's handler list when the signature matches */
static void tut_subscribe(rx_addr tmpl, rx_addr ch, rx_addr name, rx_addr vtable, rx_addr td, rx_addr m, rx_addr method) {
    ACT_CALL(2, act_subscribe(tmpl, 8, 2, 2, ch, name, vtable, td, m, method));
}

/* 0074fd10 TutorialManager_Construct (ESI = m; EAX = m): the "TutorialManager" channel (a 0x5c-byte EventSystem::Channel registered under the
 * name: 0055ee60's construction), its events (void (int): EnableTutorialManager, SetValidPlayerCount, SetValidVehicle, InUIWorld; void ():
 * DisableTutorialManager, RetryTutorial), the manager's handlers on them and on the trigger registry's VehicleEntered / VehicleExited; the
 * words; ResetLesson(1); every lesson object hidden. Frame: push ecx; push ebx / edi. */
rx_addr rx_tutorial_construct(rx_addr m) {
    ACT_FRAME(L, 4, 2);
    rx_addr ch;
    ACT_CALLR(ch, 0, rx_event_channel_create(S_MANAGER));
    SU32(0, TUT_CHANNEL, ch);
    act_event_type(0x005c0cd0, U32(0, TUT_CHANNEL), 0x0096c1c0 /* "EnableTutorialManager" */);
    act_event_type(0x00586ca0, U32(0, TUT_CHANNEL), 0x0094c394 /* "DisableTutorialManager" */);
    act_event_type(0x005c0cd0, U32(0, TUT_CHANNEL), 0x0094cca8 /* "SetValidPlayerCount" */);
    act_event_type(0x005c0cd0, U32(0, TUT_CHANNEL), 0x0096d064 /* "SetValidVehicle" */);
    act_event_type(0x005c0cd0, U32(0, TUT_CHANNEL), 0x0096d074 /* "InUIWorld" */);
    act_event_type(0x00586ca0, U32(0, TUT_CHANNEL), 0x0096d080 /* "RetryTutorial" */);
    tut_subscribe(0x00752bb0, U32(0, TUT_CHANNEL), 0x0096c1c0, 0x0096d3c8, 0x009d24d0, m, 0x00752630);
    tut_subscribe(0x00752c70, U32(0, TUT_CHANNEL), 0x0094c394, 0x0096d3e8, 0x009cf3f0, m, 0x00752650);
    tut_subscribe(0x00752bb0, U32(0, TUT_CHANNEL), 0x0094cca8, 0x0096d3c8, 0x009d24d0, m, 0x00752680);
    tut_subscribe(0x00752bb0, U32(0, TUT_CHANNEL), 0x0096d064, 0x0096d3c8, 0x009d24d0, m, 0x007526c0);
    tut_subscribe(0x00752bb0, U32(0, TUT_CHANNEL), 0x0096d074, 0x0096d3c8, 0x009d24d0, m, 0x00752700);
    tut_subscribe(0x00752c70, U32(0, TUT_CHANNEL), 0x0096d080, 0x0096d3e8, 0x009cf3f0, m, 0x00752740);
    tut_subscribe(0x00752d30, U32(0, 0x00a89bc8), 0x00935e84 /* "VehicleEntered" */, 0x0096d408, 0x009cbb7c, m, 0x007523f0);
    tut_subscribe(0x00752d30, U32(0, 0x00a89bc8), 0x00935e94 /* "VehicleExited" */, 0x0096d408, 0x009cbb7c, m, 0x00752500);
    SU32(m, 0x60, 1); SU32(m, 0xc, 1); SU32(m, 0x10, 1); SU32(m, 0x14, 1); SU32(m, 0, 1);
    SU32(m, 4, 0); SU32(m, 8, 0); SU32(m, 0x24, 0); SU32(m, 0xb4, 0);
    ACT_CALL(1, rx_tutorial_reset_lesson(1, m));
    ACT_CALL(2, rx_tutorial_show_lesson_objects(m, -1));
    ACT_END(L);
    return m;
}
RX_FUNCTION(0x0074fd10, rx_tutorial_construct)

/* 007410f0 App_EnableTutorial (EAX = the lesson): the "EnableTutorialManager" event with it */
void rx_tutorial_app_enable(int32_t lesson) {
    ACT_FRAME(L, 0, 3);                                                /* push ecx; push esi; push edi */
    SU32(L, 8, (uint32_t)lesson);                                      /* the payload: the "push ecx" slot */
    crt_type_info_name(0x009d24d0, 0x009e1a7c);                        /* typeid(void (*)(int)).name() */
    ACT_CALL(2, rx_world_event_dispatch(U32(0, TUT_CHANNEL), 0x0096c1c0 /* "EnableTutorialManager" */, L + 8, 4));
    ACT_END(L);
}
RX_FUNCTION(0x007410f0, rx_tutorial_app_enable)
/* a void () event on the manager's channel (payload 0, size 0, as App.DisableTutorial 00729f90 / EnableTutorialMode raise it) */
static void tut_raise_void(rx_addr name) {
    uint32_t node = 0x009e1a7c;
    crt_type_info_name(0x009cf3f0, node);
    ACT_CALL(2, rx_world_event_dispatch(U32(0, TUT_CHANNEL), name, 0, 0));
}
void rx_tutorial_app_disable(void) { tut_raise_void(0x0094c394 /* "DisableTutorialManager" */); }
void rx_tutorial_app_retry(void) { tut_raise_void(0x0096d080 /* "RetryTutorial" */); }

/* 00599820 MXUIManager_EnableTutorialMode (ECX = ui; stdcall enable; RET 4): 0 -> 1 turns the UI world's tutorial mode on (00765820); ui + 0x298
 * and the UI's TutorialMode; the players idle when off / active when on (005992f0); off: "DisableTutorialManager", the voice package released;
 * on: the voice package, the loading page's route to the hub (0059b760: UI), each local human row's rich presence (controller, 9, 2) and
 * (controller, 0, 7) (the online session [0x00da6cb0]: ONLINE BOUNDARY, as garage_test_drive.c). Frame: push ebp; and esp, -8; push ecx;
 * push ebx / esi / edi. */
void rx_ui_enable_tutorial_mode(rx_addr ui, uint32_t enable) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    const uint32_t was = U32(ui, 0x298);
    if (!(was == 1 && enable == 0) && was == 0 && enable == 1) ACT_CALL(0, rx_mxuiworld_tutorial_mode_on(U32(U32(0, GAME), 0x20c8)));
    SU32(ui, 0x298, enable);
    rx_tutorial_ui_set_int(0x0094c384 /* "TutorialMode" */, enable);
    ACT_CALL(1, rx_ui_set_players_inactive(U32(ui, 0x298) == 0, ui));
    if (enable == 0) {
        tut_raise_void(0x0094c394 /* "DisableTutorialManager" */);
        ACT_CALL(0, rx_tutorial_unload_audio());
        ACT_END(L);
        return;
    }
    ACT_CALL(0, rx_tutorial_load_audio());
#ifdef RX_ORACLE
    { rx_regs in, o; memset(&in, 0, sizeof in); in.r[0] = ui; rx_call_original_ex(0x0059b760, &in, &o, 0, 0); }   /* UI BOUNDARY */
#endif
    for (uint32_t r = 0;; r++) {
        const rx_addr t = ACT_PLAYER_LIST;
        if (((U32(t, 0x14) - U32(t, 0x10)) >> 2) == 0) break;
        if (r >= rx_tbl_column_rows(U32(U32(t, 0x10), 0))) break;
        const uint32_t type = act_cell(t, 0x00936ef4 /* "PlayerType" */, r), loc = act_cell(t, 0x0093de90 /* "PlayerLocal" */, r);
        if (type != 0 || loc == 0) continue;
        const uint32_t controller = act_cell(t, 0x0093ded0 /* "Controller" */, r);
        rx_ui_presence(controller, 9, 2);
        rx_ui_presence(controller, 0, 7);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00599820, rx_ui_enable_tutorial_mode)

/* 007424a0 (EDX = paused): App.PauseActiveWorld's body (the Lua binding 0072e010): the engine timing's pause word (context + 0x10 +0xc)
 * and the active world's own (+0x20014). TutorialPauseMenu:Start pauses the UI world under the lesson menu, :End resumes it. */
void rx_app_pause_active_world(uint32_t paused) {
    const rx_addr w = tut_active_world();
    SU32(U32(U32(0, CTX), 0x10), 0xc, paused);
    if (U32(w, 0x20014) != paused) SU32(w, 0x20014, paused);
}
RX_FUNCTION(0x007424a0, rx_app_pause_active_world)
