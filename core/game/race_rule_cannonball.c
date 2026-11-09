/* race_rule_cannonball.c -- RuleCannonball (vtable 0x0093fe90, 0x20 bytes: +4 the driver, +8 the world's cannonball specs, +0xc, +0x10, +0x14): the free
 * ride activity's driver rule (MXActivityFreeRide::vf47 00560c20). */
#include "build_activity_util.h"
#include "race_rule_cannonball.h"
#include "../src/rx_os.h"
#include <string.h>

/* 00519ef0 RuleCannonball::vf1 (thiscall driver; RET 4): +4 = the driver, +8 = the current world's +0x22130 (the engine's active scene picks the event world
 * [+0x20] or the UI world [+0x1c], else address 0), +0x10 = +0x14 = 0 */
void rx_rule_cannonball_vf1(rx_addr r, rx_addr driver) {
    const rx_addr ctx = U32(0, 0x00a95844), game = U32(0, 0x00a946b8);
    SU32(r, 4, driver);
    rx_addr w;
    if (U32(U32(ctx, 0x20), 0x34) == 1) w = U32(game, 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) w = U32(game, 0x20c8);
    else w = 0;
    SU32(r, 8, U32(w, 0x22130));
    SU32(r, 0x10, 0);
    SU32(r, 0x14, 0);
}
RX_FUNCTION(0x00519ef0, rx_rule_cannonball_vf1)

/* 00519f50 RuleCannonball::vf2 (thiscall; the rule's stop): the sound instance of the rule (+0xc: an FMOD event; set only by the audio code) stopped: on the main thread (the id
 * [0x00d67d1c]) directly (FUN_0080ae10: ESI = the instance), else posted to the audio thread (FUN_0080c280 (1, the instance, 0, 0)); +0xc = 0.
 * PRODUCT BOUNDARY (core/plan/QUEUE.md "chain"): the FMOD stop calls are audio only (the oracle runs the originals; the product's +0xc is never set: nothing plays) */
void rx_rule_cannonball_vf2(rx_addr r) {
    const rx_addr snd = U32(r, 0xc);
    if (snd == 0) return;
    ACT_FRAME(L, 0, 2);
    const uint32_t main_thread = U32(0, 0x00d67d1c);
    uint32_t tid;
    { ACT_FRAME(R, 0, 1); tid = RX_OS0(0x0091607c); ACT_END(R); }      /* push ebx (00519f5b) .. pop ebx (00519f6a) around GetCurrentThreadId */
    if (main_thread == tid) {
        rx_regs in, out; memset(&in, 0, sizeof in); memset(&out, 0, sizeof out); in.r[6] = snd;
        act_ui_call_ex(0x0080ae10, &in, &out, 0, 0);
    } else {
        const uint32_t a[4] = {1, snd, 0, 0};
        act_ui_call(0x0080c280, RX_CDECL, 0, 4, a);
    }
    SU32(r, 0xc, 0);
    ACT_END(L);
}
RX_FUNCTION(0x00519f50, rx_rule_cannonball_vf2)
