/* fx_tutorial.c -- TutorialManager_ShowLessonObjects 00751c40 (stdcall: manager, lesson; RET 8). Hand transliteration from the disassembly
 * run in the oracle (the tutorial boot with the lesson driver, w-tutorials: 73 calls, 0 mismatches; the strtok cursor is the CRT's own,
 * crt_strtok, so the thread data's _token matches the original's).
 *
 * The original's TutorialManager_Construct 0074fd10 ends with ResetLesson(1) and ShowLessonObjects(manager, -1) ("every TUT0n object
 * hidden"), and TutorialManager_Update 007500c0 calls it again with the current lesson when a lesson change is pending. This
 * function is the only part of the TutorialManager that touches the scene: it walks the UI world scene's (engine context +0x1c)
 * entities of three classes and sets, by the object's name:
 *    type 4    (LWModelRSData)            +0xa2 = shown
 *    type 0x1078 (ScrollingTextureRSData) +0xa2 = shown
 *    type 0x102a (PlaceableParticleEffect) controller (entity +0x90) vf5(shown)   <- the emitter's active gate
 * A name is "TUT" + a number (the number parsed from the token's last character): shown only when that number is lesson + 1;
 * every other name (a non-TUT prefix) stays shown. For lesson 2 the object also needs "Trajectory_<step>" with <step> == manager
 * +0x7c + 1 (the models / scrolling textures only).
 *
 * This is what keeps the 24 TUT placeable effects of the UI scene inert in the original (their controllers' +0x2c / emitter +0x1c
 * stay 0). The manager (fx_tutorial_manager.c) calls it from its construction (MXUIWorld::vf1, build_ui_world.c), its events and its update.
 *
 * Frame: the original is `sub esp, 0x1e0` (no EBP) with the /GS cookie at [esp + 0x1dc], the query array at [esp + 0x28] (0xc4
 * bytes) and the 0x103-byte name buffer at [esp + 0xf0]; the four callee-saved registers are pushed below the frame. The locals
 * below are this frame's array / name / prefix slots; the cookie is not written (nothing reads it). */
#include "fx_tutorial.h"
#include "scene_fx_entities.h"
#include "anim.h"
#include "crt.h"
#include <string.h>

#define TUT_PREFIX  0x0096d2acu   /* "TUT" (s_TUT_ObjectPrefix; then "Trajectory" at 0x0096d2b0) */
#define TUT_TRAJ    0x0096d2b0u
#define TUT_DELIM   0x0094f118u   /* "_" */

/* msvcr90 strtok (crt_strtok: the cursor in the thread's data, as the original's calls leave it) */
static rx_addr tok_next(rx_addr* state, rx_addr delim) {
    const rx_addr t = crt_strtok(*state, delim);
    *state = 0;
    return t;
}
/* the leading 3 characters of the token into pfx (strncpy(pfx, token, 3): zero padded; pfx[3] = 0) */
static void prefix3(rx_addr pfx, rx_addr token) {
    int done = 0;
    for (uint32_t k = 0; k < 3; k++) {
        const uint8_t c = done ? 0 : U8(token, k);
        if (!c) done = 1;
        SU8(pfx, k, c);
    }
    SU8(pfx, 3, 0);
}
/* strcmp(pfx, "TUT"): 0 when equal (the inlined two-byte compare of the original) */
static int prefix_is_tut(rx_addr pfx) {
    for (uint32_t k = 0;; k++) {
        const uint8_t a = U8(pfx, k), b = U8(TUT_PREFIX, k);
        if (a != b) return a < b ? -1 : 1;
        if (!a) return 0;
    }
}
/* strcmp(token, <game string at addr>) */
static int str_eq_game(rx_addr a, rx_addr b) {
    for (uint32_t k = 0;; k++) {
        const uint8_t x = U8(a, k), y = U8(b, k);
        if (x != y) return 0;
        if (!x) return 1;
    }
}
/* the token's last character parsed as a number (atoi(token + strlen(token) - 1)) */
static int32_t last_number(rx_addr token) {
    uint32_t n = 0;
    while (U8(token, n)) n++;
    return n ? crt_atoi(token + (n - 1)) : 0;
}
/* the object's name (entity +0x18) copied into the frame's 0x103-byte buffer and cut at its first '_'; returns the first token */
static rx_addr first_token(rx_addr namebuf, rx_addr e, rx_addr* state) {
    crt_strncpy(namebuf, U32(e, 0x18), 0x103);
    SU8(namebuf, 0x103, 0);
    *state = namebuf;
    return tok_next(state, TUT_DELIM);
}
/* the lesson-2 "Trajectory_<step>" condition: the next two tokens ("Trajectory", then the step number). The original writes the
 * caller's flag only when the second token is "Trajectory" (it starts true); returns 1 when it did, 0 when the token differs. */
static int trajectory_step(rx_addr* state, rx_addr mgr, int* show) {
    const rx_addr t2 = tok_next(state, TUT_DELIM);
    if (!t2 || !str_eq_game(t2, TUT_TRAJ)) return 0;
    const rx_addr t3 = tok_next(state, TUT_DELIM);
    *show = t3 && crt_atoi(t3) == I32(mgr, 0x7c) + 1;
    return 1;
}
/* the controller (entity +0x90) vf5(shown): the particle effect's active gate (006968e0: +0x2c = on; off -> on sets +0x30) */
static void controller_enable(rx_addr e, uint32_t on) {
    const rx_addr fn = U32(U32(e + 0x90, 0), 0x14);
    void (*f)(rx_addr, uint32_t) = RX_FN(fn, void (*)(rx_addr, uint32_t));
    if (f) f(e + 0x90, on);
    else { const uint32_t a[1] = {on}; rx_call_original(fn, RX_THISCALL, e + 0x90, 1, a); }
}

void rx_tutorial_show_lesson_objects(rx_addr mgr, int32_t lesson) {
    RX_FRAME_ENTER(F, 0x1e0);
    const rx_addr ctx = U32(0, 0x00a95844);
    const rx_addr scene = ctx ? U32(ctx, 0x1c) : 0;
    const rx_addr list = F + 0x10, namebuf = F + 0xd8, pfx = F + 0x08;
    for (uint32_t i = 0; i < 0xc4; i++) SU8(list, i, 0);

    /* types 4 and 0x1078: the entity's draw flag (+0xa2) by its name */
    static const uint32_t shown_types[2] = {4, 0x1078};
    for (int t = 0; t < 2; t++) {
        for (uint32_t i = 0; i < 0xc4; i++) SU8(list, i, 0);
        const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, shown_types[t], list, 0x32, 0);
        for (int32_t i = 0; i < n; i++) {
            const rx_addr e = U32(list, 4u * (uint32_t)i);
            rx_addr state = 0;
            const rx_addr tok = first_token(namebuf, e, &state);
            prefix3(pfx, tok);
            if (prefix_is_tut(pfx) == 0) {
                int show = 1;
                if (last_number(tok) == lesson + 1) {
                    if (lesson == 2) {   /* the trajectory step of lesson 3 */
                        int s = show;
                        if (trajectory_step(&state, mgr, &s)) show = s;
                    }
                } else show = 0;
                SU8(e, 0xa2, (uint8_t)show);
            }
        }
    }

    /* type 0x102a: the placeable effect's controller enabled by its name */
    for (uint32_t i = 0; i < 0xc4; i++) SU8(list, i, 0);
    const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, 0x102a, list, 0x32, 0);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = U32(list, 4u * (uint32_t)i);
        rx_addr state = 0;
        const rx_addr tok = first_token(namebuf, e, &state);
        prefix3(pfx, tok);
        int show = 1;
        if (prefix_is_tut(pfx) == 0) show = last_number(tok) == lesson + 1;
        if (e) controller_enable(e, (uint32_t)show);
    }
    RX_FRAME_LEAVE(F, 0x1e0);
}
RX_FUNCTION(0x00751c40, rx_tutorial_show_lesson_objects)
