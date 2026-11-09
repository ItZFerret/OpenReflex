/* build_activity_frame.c -- construction unit 11: small engine / scene helpers on the path from a set-up event to the running race
 * whose owners (loop core, scene) have finished (lead's assignment, 2026-10-19; core/notes/construction.md section 16):
 *   0084ea00 the engine's frame mark (the job pool idle, the task thread's handshake), 0081f290 the controllers' timed rumble
 *   effects, 007d20b0 a scene's active flag and its views' audio listeners (007d2030 / 00804da0 / 00794d10). */
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "loop_engine.h"
#include "loop_platform.h"
#include <string.h>

/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* FUN_0084ea00 (EDI = the engine's frame object [core + 0x10]): the job pool (0x00d94ae0, its function-local static under bit 0 of
 * 0x00d94ad8: FUN_00881a70, built at startup) waited idle (JobPool_WaitIdle 00881e20, ESI = pool); off the engine's thread
 * (+0xcc) the task thread's event (+0xdc) is waited for, then signalled (SetEvent; GetLastError on failure). */
void rx_engine_frame_mark(rx_addr e) {
    ACT_FRAME(L, 0, 1);
    if (!(U8(0, 0x00d94ad8) & 1)) act_unreached(0x0084ea19, "the job pool's static constructor (00881a70) not run");
    ACT_CALL(0, rx_jobpool_wait_idle(0x00d94ae0));
    if (loop_get_current_thread_id() != U32(e, 0xcc)) loop_wait_event(U32(e, 0xdc));
    if (loop_get_current_thread_id() != U32(e, 0xcc)) {
        if (loop_set_event(U32(e, 0xdc)) == 0) loop_get_last_error();
    }
    ACT_END(L);
}
RX_FUNCTION(0x0084ea00, rx_engine_frame_mark)

/* FUN_0081f290: the controllers' timed rumble effects. With the effect manager [0x00d611d0] the 0x30-byte effect records
 * (0x00d611b4, count 0x00d611b8: 0081f3d0) and the 0x20-byte motor requests (0x00d611a0, count 0x00d611a4: 0081f310) are applied;
 * then both counts are cleared. */
void rx_rumble_frame(void) {
    ACT_FRAME(L, 0, 5);
    if (U32(0, 0x00d611d0) != 0 && (I32(0, 0x00d611b8) > 0 || I32(0, 0x00d611a4) > 0))
        act_unreached(0x0081f2a9, "controller rumble effects (0081f3d0 / 0081f310: input)");
    SU32(0, 0x00d611a4, 0);
    SU32(0, 0x00d611b8, 0);
    ACT_END(L);
}
RX_FUNCTION(0x0081f290, rx_rumble_frame)

/* ---- a scene's active flag (007d20b0) ---- */

/* FUN_007d2030 (ECX = view): the view takes an audio listener unless it is named "Jumbotron" or "Shared Viewport" (+0x18) */
static uint32_t view_has_listener(rx_addr v) {
    if (crt_strcmp(v + 0x18, 0x00960c18 /* "Jumbotron" */) == 0) return 0;
    if (crt_strcmp(v + 0x18, 0x0096d6ac /* "Shared Viewport" */) == 0) return 0;
    return 1;
}
/* FUN_00804da0: the first free audio listener of the four 0x80-byte records at 0x00d67d20 (+0 in use), taken; or 0 */
static rx_addr audio_listener_alloc(void) {
    rx_addr r = 0;
    for (uint32_t k = 0, p = 0x00d67d20; k < 0x200 && r == 0; k += 0x80, p += 0x80)
        if (U32(p, 0) == 0) { SU32(p, 0, 1); r = p; }
    return r;
}
/* FUN_00794d10 (EDI = view): the view's listener (+0x80) from its camera (+0): the camera's position (vf0 into a frame vector;
 * the previous one kept at +0x60 when +0x70 is set), the camera's forward (+0x1d0) at +0x20 and up (+0x1c0) at +0x30 */
static void audio_listener_update(rx_addr v) {
#ifndef RX_ORACLE
    /* PRODUCT BOUNDARY: the view's camera (XenonCore::vf10 00843aa0, a 0x1e0 camera object) and the audio listener are the
     * camera / audio units' (no renderer or audio engine in the product). The viewport count and records stay (scene_viewports.c). */
    (void)v;
    return;
#else
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    if (U32(v, 0x80) != 0) {
        const rx_addr cam = U32(v, 0);
        const rx_addr pos = L + 0x10;
        act_vcall(cam, 0, 1, (const uint32_t[]){pos});
        const rx_addr l = U32(v, 0x80);
        const uint32_t f0 = U32(cam, 0x1d0), f1 = U32(cam, 0x1d4), f2 = U32(cam, 0x1d8);
        const uint32_t u0 = U32(cam, 0x1c0), u1 = U32(cam, 0x1c4), u2 = U32(cam, 0x1c8);
        if (U32(l, 0x70) != 0) {
            SU32(l, 0x60, x87_copy(U32(l, 0x10))); SU32(l, 0x64, x87_copy(U32(l, 0x14))); SU32(l, 0x68, x87_copy(U32(l, 0x18)));
        }
        SU32(l, 0x10, U32(pos, 0)); SU32(l, 0x14, U32(pos, 4)); SU32(l, 0x18, U32(pos, 8));
        const rx_addr l2 = U32(v, 0x80);
        SU32(l2, 0x20, f0); SU32(l2, 0x24, f1); SU32(l2, 0x28, f2);
        SU32(l2, 0x30, u0); SU32(l2, 0x34, u1); SU32(l2, 0x38, u2);
    }
    ACT_END(L);
#endif
}
/* FUN_007d20b0 (EAX = active; stdcall scene; RET 4): the scene's +0x34 and +0 = active; every view (+0x19b8, count +0x19d0):
 * active: a listener taken (when the view has none and may have one) and updated from its camera; inactive: its listener
 * released (in use 0, the vectors zero) */
void rx_scene_set_active(uint32_t active, rx_addr s) {
    ACT_FRAME_EBP(L, 8, 4, 3);
    SU32(s, 0x34, active);
    SU32(s, 0, active);
    for (uint32_t k = 0; k < U32(s, 0x19d0); k++) {
        const rx_addr v = U32(s, 0x19b8 + 4 * k);
        if (U32(s, 0x34) != 0) {
            if (view_has_listener(v)) {
                if (U32(v, 0x80) == 0) SU32(v, 0x80, audio_listener_alloc());
                ACT_CALL(0, audio_listener_update(U32(s, 0x19b8 + 4 * k)));
            }
        } else {
            const rx_addr l = U32(v, 0x80);
            if (l) {
                SU32(l, 0, 0);
                static const uint32_t offs[12] = {0x60, 0x64, 0x68, 0x10, 0x14, 0x18, 0x20, 0x24, 0x28, 0x30, 0x34, 0x38};
                for (int i = 0; i < 12; i++) SF32(l, offs[i], 0.0f);
                SU32(v, 0x80, 0);
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x007d20b0, rx_scene_set_active)
