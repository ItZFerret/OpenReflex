/* loop_input_suspend.c -- Player_SetInputSuspended 0054f540 and its callers in the UI manager (loop_input_suspend.h; w-tutcore).
 * Transliterated from the disassembly. Every call is made at the original's stack depth with its call site's return address (the callees
 * are KeyMap virtuals, the pad-map selectors and the input groups' setters: none keeps a stack address). */
#include "loop_input_suspend.h"
#include "loop_util.h"
#include "loop_player.h"
#include "loop_input.h"
#include "world_misc.h"
#include "build_activity_freeride_run.h"
#include "build_activity_util.h"
#include <string.h>
#include "rider_cap.h"

#define SUS_BLOCKS 0x009c52c4u   /* g_ControllerKeyMapBlocks: the per-controller input block */
static rx_addr sus_block(uint32_t pad) { return U32(0, SUS_BLOCKS + 4u * pad); }

/* a KeyMap virtual at the original's depth: vf10 (+0x28: the pad's bindings enabled?; stack pad; RET 4) / vf9 (+0x24: enable; stack pad,
 * on; RET 8) */
static uint32_t km_enabled(rx_addr km, uint32_t pad, rx_addr ret) {
    const rx_addr fn = loop_vslot(km, 0x28);
    uint32_t (*f)(rx_addr, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t));
    const uint32_t a[1] = {pad};
    if (!f) return rx_call_original(fn, RX_THISCALL, km, 1, a);
    const rx_addr s = loop_push(1, a, ret);
    const uint32_t r = f(km, pad);
    loop_pop(s);
    return r;
}
static void km_enable(rx_addr km, uint32_t pad, uint32_t on, rx_addr ret) {
    const rx_addr fn = loop_vslot(km, 0x24);
    uint32_t (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(fn, uint32_t (*)(rx_addr, uint32_t, uint32_t));
    const uint32_t a[2] = {pad, on};
    if (!f) { (void)rx_call_original(fn, RX_THISCALL, km, 2, a); return; }
    const rx_addr s = loop_push(2, a, ret);
    (void)f(km, pad, on);
    loop_pop(s);
}
/* the pad-map selectors (world_misc.c: 00548f20 / 00548f70 EAX = pad, 00548fc0 / 00549050 ESI = pad), a call with its return address */
static rx_addr pad_map(rx_addr (*sel)(uint32_t), uint32_t pad, rx_addr ret) {
    const rx_addr s = loop_push(0, 0, ret);
    const rx_addr r = sel(pad);
    loop_pop(s);
    return r;
}
/* a call of a register-convention core function with its return address */
#define SUS_AT(ret, stmt) do { const rx_addr s_ = loop_push(0, 0, (ret)); stmt; loop_pop(s_); } while (0)

/* ---- the groups' "all enabled" queries ---------------------------------------------------------------------------------------- */

/* 00548d20 (ESI = pad) -> EAX: the steer map (00548f20) and the lean map (00548f70) both report the pad's bindings enabled (vf10) */
uint32_t rx_player_group0_enabled(uint32_t pad) {
    if (km_enabled(pad_map(rx_world_pad_steer_map, pad, 0x00548d27), pad, 0x00548d31) == 0) return 0;
    if (km_enabled(pad_map(rx_world_pad_lean_map, pad, 0x00548d3c), pad, 0x00548d46) == 0) return 0;
    return 1;
}
RX_FUNCTION(0x00548d20, rx_player_group0_enabled)

/* 00548d60 (EAX = pad; push esi) -> EAX: the wreck steer map (00548fc0) and the wreck lean map (00549050) both enabled */
uint32_t rx_player_group3_enabled(uint32_t pad) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                                  /* push esi */
    uint32_t r = 0;
    if (km_enabled(pad_map(rx_world_pad_wreck_steer_map, pad, 0x00548d68), pad, 0x00548d72) != 0 &&
        km_enabled(pad_map(rx_world_pad_wreck_lean_map, pad, 0x00548d7b), pad, 0x00548d85) != 0) r = 1;
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00548d60, rx_player_group3_enabled)

/* 00548da0 (ESI = pad) -> EAX: the input block's KeyMaps +0x148, +0x1134, +0x11d8 (00548bf0's group) all enabled */
uint32_t rx_player_group2_enabled(uint32_t pad) {
    static const uint32_t off[3] = {0x148, 0x1134, 0x11d8};
    static const rx_addr ret[3] = {0x00548db5, 0x00548dce, 0x00548de7};
    for (int i = 0; i < 3; i++) if (km_enabled(sus_block(pad) + off[i], pad, ret[i]) == 0) return 0;
    return 1;
}
RX_FUNCTION(0x00548da0, rx_player_group2_enabled)

/* 00548e00 (ESI = pad) -> EAX: the ten KeyMaps of 00548c40's group (+0x33a0 .. +0x4630) all enabled */
uint32_t rx_player_group5_enabled(uint32_t pad) {
    static const uint32_t off[10] = {0x33a0, 0x358c, 0x378c, 0x398c, 0x3b8c, 0x3d8c, 0x3f8c, 0x418c, 0x438c, 0x4630};
    static const rx_addr ret[10] = {0x00548e15, 0x00548e32, 0x00548e4f, 0x00548e6c, 0x00548e89, 0x00548ea6, 0x00548ebf, 0x00548ed8,
                                    0x00548ef1, 0x00548f0a};
    for (int i = 0; i < 10; i++) if (km_enabled(sus_block(pad) + off[i], pad, ret[i]) == 0) return 0;
    return 1;
}
RX_FUNCTION(0x00548e00, rx_player_group5_enabled)

/* ---- the pad's vibration ------------------------------------------------------------------------------------------------------ */

/* 00815170 XenonInputManager::vf45 (thiscall device, controller, value; RET 0xc) -> EAX: a connected pad's (+0x2a0) vibration pause word
 * (+0x2bc) = value, 1; else 0 (only device 1, the pads) */
uint32_t rx_input_set_vibration_paused(rx_addr im, uint32_t device, uint32_t ctl, uint32_t paused) {
    if (device != 1) return 0;
    const rx_addr pad = im + ctl * 0x2dcu;
    if (U32(pad, 0x2a0) == 0) return 0;
    SU32(pad, 0x2bc, paused);
    return 1;
}
RX_FUNCTION(0x00815170, rx_input_set_vibration_paused)

/* the engine context's input manager (XenonCore vf2: context + 0x34), then its vf48 (+0xc0: stop the vibration; stack device, controller;
 * RET 8) / vf45 (+0xb4; stack device, controller, value; RET 0xc) at the original's depth */
static rx_addr sus_input_manager(rx_addr ret) {
    const rx_addr ctx = loop_ctx();
    return loop_tc0r(loop_vslot(ctx, 8), ctx, ret);
}
static void sus_im_call(rx_addr im, uint32_t off, int n, const uint32_t* a, rx_addr ret) {
    const rx_addr fn = loop_vslot(im, off);
    void* f = rx_fn(fn);
    if (!f) { (void)rx_call_original(fn, RX_THISCALL, im, n, a); return; }
    const rx_addr s = loop_push(n, a, ret);
    if (n == 2) ((uint32_t (*)(rx_addr, uint32_t, uint32_t))f)(im, a[0], a[1]);
    else ((uint32_t (*)(rx_addr, uint32_t, uint32_t, uint32_t))f)(im, a[0], a[1], a[2]);
    loop_pop(s);
}

/* ---- Player_SetInputSuspended ------------------------------------------------------------------------------------------------- */

/* the eleven groups, as the players' setters keep them (loop_player.c pl_groups_on; the bit is the group's in +0x11cc):
 *   1 the pad maps 00548f20 / 00548f70, 2 the block's +0x348, 4 00548bf0's three, 8 the wreck maps 00548fc0 / 00549050, 0x10 +0x9ec,
 *   0x20 00548c40's ten, 0x40 +0x127c, 0x80 +0x167c, 0x100 +0x748, 0x200 +0x147c, 0x400 +0xf34 */

/* one group switched on / off as the function's own inline setters do: the pad re-read (skipped when negative); while +0x11c8 is set only
 * the bit changes (on: set, off: cleared) */
static void sus_block_set(rx_addr P, uint32_t off, uint32_t bit, uint32_t on, rx_addr ret) {
    const int32_t pad = I32(P, 0x1f0);
    if (pad < 0) return;
    if (U32(P, 0x11c8) != 0) { SU32(P, 0x11cc, on ? (U32(P, 0x11cc) | bit) : (U32(P, 0x11cc) & ~bit)); return; }
    km_enable(sus_block((uint32_t)pad) + off, (uint32_t)pad, on, ret);
}
static void sus_maps_set(rx_addr P, rx_addr (*a)(uint32_t), rx_addr (*b)(uint32_t), uint32_t bit, uint32_t on, const rx_addr ret[4]) {
    const int32_t pad = I32(P, 0x1f0);
    if (pad < 0) return;
    if (U32(P, 0x11c8) != 0) { SU32(P, 0x11cc, on ? (U32(P, 0x11cc) | bit) : (U32(P, 0x11cc) & ~bit)); return; }
    km_enable(pad_map(a, (uint32_t)pad, ret[0]), (uint32_t)pad, on, ret[1]);
    km_enable(pad_map(b, (uint32_t)pad, ret[2]), (uint32_t)pad, on, ret[3]);
}
static void sus_group_set(rx_addr P, void (*g)(uint32_t, uint32_t), uint32_t bit, uint32_t on, rx_addr ret) {
    const int32_t pad = I32(P, 0x1f0);
    if (pad < 0) return;
    if (U32(P, 0x11c8) != 0) { SU32(P, 0x11cc, on ? (U32(P, 0x11cc) | bit) : (U32(P, 0x11cc) & ~bit)); return; }
    SUS_AT(ret, g((uint32_t)pad, on));
}

/* 0054f540 Player_SetInputSuspended (stdcall player, suspended; RET 8). Nothing when +0x11c8 already equals `suspended`.
 * Suspend: +0x11cc = the groups enabled now (each group's query: 00548d20, the block's +0x348 vf10, 00548da0, 00548d60, +0x9ec, +0x167c,
 * 00548e00, +0x127c, +0x748, +0x147c, +0xf34 -> bits 1, 2, 4, 8, 0x10, 0x80, 0x20, 0x40, 0x100, 0x200, 0x400); with a pad (+0x1f0 not
 * negative) every group off (in the order 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400); the pad's vibration stopped
 * (XenonInputManager::vf48( 1, pad )) and paused (vf45( 1, pad, 0 )); +0x11c8 = suspended.
 * Release: +0x11c8 = 0; every group whose bit +0x11cc holds on again (order 1, 2, 4, 8, 0x10, 0x20, 0x400, 0x40, 0x80, 0x100, 0x200); the
 * vibration on (vf45( 1, pad, 1 )). Frame: push ebx; then push esi / edi. */
void rx_player_set_input_suspended(rx_addr P, uint32_t on) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);                                                  /* push ebx */
    if (U32(P, 0x11c8) == on) { rx_set_sp(sp0); return; }
    rx_set_sp(sp0 - 12);                                                 /* push esi / edi */
    if (on != 0) {
#define SUS_OR(v, bit) SU32(P, 0x11cc, U32(P, 0x11cc) | ((v) != 0 ? (bit) : 0u))
        SU32(P, 0x11cc, 0);
        uint32_t v;
        SUS_AT(0x0054f574, v = rx_player_group0_enabled(U32(P, 0x1f0)));                            SUS_OR(v, 1u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x348, U32(P, 0x1f0), 0x0054f59f);                SUS_OR(v, 2u);
        SUS_AT(0x0054f5b7, v = rx_player_group2_enabled(U32(P, 0x1f0)));                            SUS_OR(v, 4u);
        SUS_AT(0x0054f5cf, v = rx_player_group3_enabled(U32(P, 0x1f0)));                            SUS_OR(v, 8u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x9ec, U32(P, 0x1f0), 0x0054f5fb);                SUS_OR(v, 0x10u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x167c, U32(P, 0x1f0), 0x0054f627);               SUS_OR(v, 0x80u);
        SUS_AT(0x0054f641, v = rx_player_group5_enabled(U32(P, 0x1f0)));                            SUS_OR(v, 0x20u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x127c, U32(P, 0x1f0), 0x0054f66d);               SUS_OR(v, 0x40u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x748, U32(P, 0x1f0), 0x0054f699);                SUS_OR(v, 0x100u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0x147c, U32(P, 0x1f0), 0x0054f6c7);               SUS_OR(v, 0x200u);
        v = km_enabled(sus_block(U32(P, 0x1f0)) + 0xf34, U32(P, 0x1f0), 0x0054f6f5);                SUS_OR(v, 0x400u);
#undef SUS_OR
        if (I32(P, 0x1f0) >= 0) {
            static const rx_addr r0[4] = {0x0054f733, 0x0054f73f, 0x0054f746, 0x0054f752};
            static const rx_addr r3[4] = {0x0054f7cd, 0x0054f7d9, 0x0054f7de, 0x0054f7ea};
            if (U32(P, 0x11c8) != 0) SU32(P, 0x11cc, U32(P, 0x11cc) & ~1u);   /* (the pad was read once for group 0) */
            else {
                const uint32_t pad = U32(P, 0x1f0);
                km_enable(pad_map(rx_world_pad_steer_map, pad, r0[0]), pad, 0, r0[1]);
                km_enable(pad_map(rx_world_pad_lean_map, pad, r0[2]), pad, 0, r0[3]);
            }
            sus_block_set(P, 0x348, 2u, 0, 0x0054f789);
            sus_group_set(P, rx_player_slot_group2, 4u, 0, 0x0054f7ac);
            sus_maps_set(P, rx_world_pad_wreck_steer_map, rx_world_pad_wreck_lean_map, 8u, 0, r3);
            sus_block_set(P, 0x9ec, 0x10u, 0, 0x0054f821);
            sus_group_set(P, rx_player_slot_group5, 0x20u, 0, 0x0054f844);
            sus_block_set(P, 0x127c, 0x40u, 0, 0x0054f87b);
            sus_block_set(P, 0x167c, 0x80u, 0, 0x0054f8b5);
            sus_block_set(P, 0x748, 0x100u, 0, 0x0054f8ef);
            sus_block_set(P, 0x147c, 0x200u, 0, 0x0054f929);
            sus_block_set(P, 0xf34, 0x400u, 0, 0x0054f963);
        }
        {
            rx_addr im = sus_input_manager(0x0054f976);
            const uint32_t a[2] = {1, U32(P, 0x1f0)};
            sus_im_call(im, 0xc0, 2, a, 0x0054f985);                     /* vf48: the vibration stopped */
            im = sus_input_manager(0x0054f998);
            const uint32_t b[3] = {1, U32(P, 0x1f0), 0};
            sus_im_call(im, 0xb4, 3, b, 0x0054f9a9);                     /* vf45( 1, pad, 0 ) */
        }
        SU32(P, 0x11c8, on);
        rx_set_sp(sp0);
        return;
    }
    SU32(P, 0x11c8, 0);
    if ((U32(P, 0x11cc) & 1u) != 0 && I32(P, 0x1f0) >= 0) {
        const uint32_t pad = U32(P, 0x1f0);
        km_enable(pad_map(rx_world_pad_steer_map, pad, 0x0054f9dd), pad, 1, 0x0054f9e9);
        km_enable(pad_map(rx_world_pad_lean_map, pad, 0x0054f9f0), pad, 1, 0x0054f9fc);
    }
    if (U32(P, 0x11cc) & 2u) sus_block_set(P, 0x348, 2u, 1, 0x0054fa40);
    if (U32(P, 0x11cc) & 4u) sus_group_set(P, rx_player_slot_group2, 4u, 1, 0x0054fa72);
    if (U32(P, 0x11cc) & 8u) {
        static const rx_addr r3[4] = {0x0054fa9f, 0x0054faab, 0x0054fab0, 0x0054fabc};
        sus_maps_set(P, rx_world_pad_wreck_steer_map, rx_world_pad_wreck_lean_map, 8u, 1, r3);
    }
    if (U32(P, 0x11cc) & 0x10u) sus_block_set(P, 0x9ec, 0x10u, 1, 0x0054fb00);
    if (U32(P, 0x11cc) & 0x20u) sus_group_set(P, rx_player_slot_group5, 0x20u, 1, 0x0054fb32);
    if (U32(P, 0x11cc) & 0x400u) sus_block_set(P, 0xf34, 0x400u, 1, 0x0054fb7c);
    if (U32(P, 0x11cc) & 0x40u) sus_block_set(P, 0x127c, 0x40u, 1, 0x0054fbc0);
    if (U32(P, 0x11cc) & 0x80u) sus_block_set(P, 0x167c, 0x80u, 1, 0x0054fc06);
    if (U32(P, 0x11cc) & 0x100u) sus_block_set(P, 0x748, 0x100u, 1, 0x0054fc50);
    if (U32(P, 0x11cc) & 0x200u) sus_block_set(P, 0x147c, 0x200u, 1, 0x0054fc9a);
    {
        const rx_addr im = sus_input_manager(0x0054fcad);
        const uint32_t b[3] = {1, U32(P, 0x1f0), 1};
        sus_im_call(im, 0xb4, 3, b, 0x0054fcbe);                         /* vf45( 1, pad, 1 ) */
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0054f540, rx_player_set_input_suspended)

/* ---- the UI manager's side ---------------------------------------------------------------------------------------------------- */

/* FUN_00552c10 (ESI = the players game + 0x7460, EDI = controller) -> EAX: the player of the controller (the active slots' records
 * +0xdf8c[0 .. +0xdfbc), their +0x1f0), else 0 */
static rx_addr sus_player_of(uint32_t controller) {
    const rx_addr list = U32(0, 0x00a946b8) + 0x7460;
    const uint32_t n = U8(list, 0xdfbc);
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t rec = rx_seat_order(list, i);
        if (U32(rx_seat(list, rec), 0x1f0) == controller) return rx_seat(list, rec);
    }
    return 0;
}

#ifdef RX_ORACLE
uint32_t rx_ui_get_suspend_input(rx_addr ui, rx_addr out) {
    rx_regs in, o;
    memset(&in, 0, sizeof in);
    in.r[0] = 0x0094c2a4;                                                /* "SuspendPlayerInput" */
    const uint32_t a[2] = {ui, out};
    rx_call_original_ex(0x005dc0c0, &in, &o, 2, a);
    return o.r[0];
}
#else
static uint32_t g_suspend;                                               /* the property (the host's pages set it) */
uint32_t rx_ui_get_suspend_input(rx_addr ui, rx_addr out) { (void)ui; SU32(out, 0, g_suspend); return 1; }
uint32_t rx_ui_suspend_input(void) { return g_suspend; }
void rx_ui_suspend_input_reset(void) { g_suspend = 0; }
/* The host's pages run between frames, where the original's run in the UI manager's update (UIManager::vf19's page graph update), before
 * MXUIManager::vf44 in the same tick: a value set there takes effect in that tick, before the next world update. So: the change callback
 * (005a3b00) at once, then vf44's loop (the rest of that tick). */
void rx_ui_suspend_input_set(rx_addr ui, uint32_t value) {
    const uint32_t was = g_suspend;
    g_suspend = value;
    if (was != value) {                                                  /* UIProperty_SetInt's change callbacks: 005a3b00 */
        ACT_CALL(1, rx_ui_on_suspend_input_changed(ui, 0));
    }
    rx_ui_suspend_input_frame();
}
void rx_ui_suspend_input_frame(void) {
    if (g_suspend == 0 || U32(0, 0x00a946b8) == 0) return;
    for (uint32_t c = 0; c < 4; c++) {                                   /* 00598df4 .. 00598e22 */
        uint32_t local;
        ACT_CALLR(local, 1, rx_player_controller_present(c));            /* 004dd3d0 */
        if (local == 0) continue;
        const rx_addr P = sus_player_of(c);
        if (P) ACT_CALL(2, rx_player_set_input_suspended(P, 1));
    }
}
#endif

/* 005a3b00 MXUIManager_OnSuspendPlayerInputChanged (ECX = ui; stack the event's arguments, unused; RET 4): the property read (0 when absent),
 * and when it is 0 every local controller's player (004dd3d0, 00552c10) released: Player_SetInputSuspended( player, 0 ). Frame: push ecx (the
 * value); push ebx / esi / edi. */
void rx_ui_on_suspend_input_changed(rx_addr ui, rx_addr args) {
    (void)args;
    ACT_FRAME(L, 4, 3);
    const rx_addr value = L + 0xc;
    SU32(value, 0, 0);
    ACT_CALL(2, (void)rx_ui_get_suspend_input(ui, value));
    const uint32_t v = U32(value, 0);
    for (uint32_t c = 0; c < 4; c++) {
        uint32_t local;
        ACT_CALLR(local, 1, rx_player_controller_present(c));
        if (local == 0 || v != 0) continue;
        rx_addr P;
        ACT_CALLR(P, 0, sus_player_of(c));
        if (P) ACT_CALL(2, rx_player_set_input_suspended(P, v));
    }
    ACT_END(L);
}
RX_FUNCTION(0x005a3b00, rx_ui_on_suspend_input_changed)
