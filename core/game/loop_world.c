/* loop_world.c -- the world's frame (MXWorld, vtable 0096dd34): MXWorld::vf5 007674b0 and the player slot pass 00766ec0.
 * MXWorld: +0x20014 paused (replay / intro), +0x20018 the session has started, +0x20020 world time (double), +0x20028 the debug
 * toggle's next allowed time (double), +0x20134 the physics world, +0x21da0 the drivers (+0x21da4 array, +0x21da8 count), +0x21dc0
 * debug toggle, +0x21dfc the replay director, +0x21e10 the activity-start sequencer (+0x2210c / +0x22130 active).
 * The game object (00a946b8) + 0x7460: the player list (+0xdfbc players in the session, byte). (bootstrap.md, physics-timing.md) */
#include "loop_world.h"
#include "build_activity_freeride_run.h"
#include "loop_util.h"
#include "loop_extern.h"
#include "loop_cam_entity.h"
#include "loop_platform.h"
#include "build_alloc.h"
#include "world_wreck.h"
#include "race_driver.h"
#include "race_activity.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "rider_cap.h"

/* ---- calls into other subsystems (the core function when registered, else the original) ---- */

/* 006de1f0 / 006def20 (rx_world): the network session clock (00da6ca0) behind its static-init guard 00dcf304: seconds, xmm0 */
static float net_clock(void) {
    race_static_dcf304();   /* 006de1f0 behind its guard 00dcf304, + the atexit of 00913e00 */
    return rx_world_timer_seconds(0x00da6ca0);   /* 006def20 (world owner; registered port) */
}
/* a register-argument original or its core function: one register argument, then stack arguments */
static uint32_t call_reg1(rx_addr fn, int reg, uint32_t value, int n, const uint32_t* args) {
    rx_regs in = loop_regs();
    in.r[reg] = value;
    return loop_ex(fn, in, n, args);
}
/* 00769b80 World_PhysicsUpdate (EAX = physics world, stack dt; rx_world) */
static void physics_update(rx_addr physics, float dt) {
    const uint32_t a[1] = {rx_fb(dt)};
    __typeof__(&rx_world_physics_update) f = LOOP_CORE(0x00769b80, rx_world_physics_update);
    if (f) { const rx_addr s = loop_push(1, a, 0); f(physics, dt); loop_pop(s); } else call_reg1(0x00769b80, LOOP_EAX, physics, 1, a);
}
/* 0055f700 (ESI = the world's drivers, stack network dt; rx_race) */
static void drivers_update(rx_addr drivers, float dt) {
    const uint32_t a[1] = {rx_fb(dt)};
    __typeof__(&rx_activity_manager_update) f = LOOP_CORE(0x0055f700, rx_activity_manager_update);
    if (f) { const rx_addr s = loop_push(1, a, 0); f(drivers, dt); loop_pop(s); return; }
    call_reg1(0x0055f700, LOOP_ESI, drivers, 1, a);
}
/* 00552a10 (EAX = the player list, stack dt; rx_race): the players' updates */
static void players_update(rx_addr list, float dt) {
    const uint32_t a[1] = {rx_fb(dt)};
    __typeof__(&rx_players_update_cameras) f = LOOP_CORE(0x00552a10, rx_players_update_cameras);
    if (f) { const rx_addr s = loop_push(1, a, 0); f(list, dt); loop_pop(s); return; }
    call_reg1(0x00552a10, LOOP_EAX, list, 1, a);
}
static void cdecl0(rx_addr fn) { rx_call_original(fn, RX_CDECL, 0, 0, 0); }
/* cdecl with one float argument */
static void cdecl_f(rx_addr fn, float x) { const uint32_t a[1] = {rx_fb(x)}; rx_call_original(fn, RX_CDECL, 0, 1, a); }
/* 007678b0 ReplayCameras_Update (world, dt; ret 8) */
static void replay_cameras_update(rx_addr world, float dt) {
    const uint32_t a[2] = {world, rx_fb(dt)};
    const rx_addr s = loop_push(2, a, 0);
    rx_replay_cameras_update(world, dt);
    loop_pop(s);
}
/* 00552b90 (EDX = player list, stack index, kind; ret 8; rx_world) -> the player record */
static rx_addr player_at(rx_addr list, uint32_t i, uint32_t kind) {
    const uint32_t a[2] = {i, kind};
    __typeof__(&rx_world_players_find) f = LOOP_CORE(0x00552b90, rx_world_players_find);
    if (!f) return call_reg1(0x00552b90, LOOP_EDX, list, 2, a);
    const rx_addr s = loop_push(2, a, 0);
    const rx_addr r = f(list, i, kind);
    loop_pop(s);
    return r;
}
/* 00552ce0 (ESI = player list, EDI = kind; rx_race) -> how many players of that kind */
static uint32_t player_count(rx_addr list, uint32_t kind) {
    const rx_addr s = loop_push(0, 0, 0);
    const uint32_t n = rx_players_count(list, kind);
    loop_pop(s);
    return n;
}
/* 00767200 PlayerSlot_SpawnStep (EAX = player, stack world; ret 4) -> its state (4 = done) */
static uint32_t spawn_step(rx_addr player, rx_addr world) {
    const uint32_t a[1] = {world};
    uint32_t (*f)(rx_addr, rx_addr) = RX_FN(0x00767200, uint32_t (*)(rx_addr, rx_addr));
    if (!f) return call_reg1(0x00767200, LOOP_EAX, player, 1, a);
    const rx_addr s = loop_push(1, a, 0);
    const uint32_t r = f(player, world);
    loop_pop(s);
    return r;
}

/* ---- MXWorld ---- */

/* 00767690 (ESI = world): the debug key map 00d5bee8 (KeyMap::vf2(7, 0)) toggles +0x21dc0, at most once a second of world time;
 * not with the developer keys off (009b716d). */
void rx_world_debug_toggle(rx_addr world) {
    if (U8(0, 0x009b716d) != 0) return;
    if (loop_tc2(0x008115f0, 0x00d5bee8, 7, 0) == 0) return;
    const double t = F64(world, 0x20020);
    if (!(t > F64(world, 0x20028))) return;
    const uint32_t was = U32(world, 0x21dc0);
    SF64(world, 0x20028, t + 1.0 /* 0x977638 */);
    SU32(world, 0x21dc0, was == 0);
}
RX_FUNCTION(0x00767690, rx_world_debug_toggle)

/* the player slot checks of 00766ec0: a player that still has to be spawned (a reset point / grid slot not given yet, or no
 * vehicle / driver) */
static int needs_spawn(rx_addr world, rx_addr p) {
    if (U32(p, 0x10) != 0 && U32(world, 0x20018) != 0) return I32(p, 0x1c4) > -1 || I32(p, 0x1c0) > -1;
    return I32(p, 0x1c4) > -1 || I32(p, 0x1c0) > -1 || U32(p, 0x1ac) == 0 || U32(p, 0x1b4) == 0;
}
static void driver_joined(rx_addr player, rx_addr driver) {   /* 0055b610 Activity_OnPlayerJoined (ECX = player, stack driver) */
#ifdef RX_ORACLE
    const uint32_t a[1] = {driver};
    rx_call_original(0x0055b610, RX_THISCALL, player, 1, a);
#else
    loop_tc1(0x0055b610, player, driver);
#endif
    if (U32(driver, 0x21c) == 5) SU32(driver, 0x10, 2);
}

/* 00766ec0 (world; ret 4): the session's player slots. Before the session starts (+0x20018 == 0): every player (list kind 3) still
 * spawning runs PlayerSlot_SpawnStep (stop the pass unless it is done, state 4); a spawned player not yet bound (+0x1bc != 0x3d)
 * leaves its old activity slot and joins the driver of its grid slot (0055f630); players without vehicle or driver mark their
 * driver (+0x10 = 1). Started: spawning players step (twice for a pending respawn, +0x10 == 1), a player without an activity
 * (+0x94) gets state 0x3e and joins the first driver. Then every driver with a request (+0x10): 2 = reset (slots 17, 15), 1 = clear. */
void rx_world_update_players(rx_addr world) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    rx_set_sp(A - 0x18);
    const rx_addr list = U32(0, 0x00a946b8) + 0x7460;
    const uint32_t n = U8(list, 0xdfbc);
    if (U32(world, 0x20018) == 0) {
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr p = player_at(list, i, 3);
            if (needs_spawn(world, p) && spawn_step(p, world) != 4) { rx_set_sp(sp0); return; }
            if (U32(p, 0x1bc) == 0x3d) continue;
            const rx_addr act = U32(p, 0x94);
            if (act != 0) {
                /* 0055a050 is a register-argument function (ECX = the group, ESI = the player at 00766f84): its port takes (group, rider). The
                 * typed pointer loop_tc0 builds, `uint32_t (*)(rx_addr)`, left the rider (x1) to whatever the register held (found by the
                 * extended sigcheck: a typed call of a literal address with fewer arguments than the port takes) */
                const uint32_t slot = (uint32_t)rx_world_group_rider_index(act, p);
                const rx_addr table = U32(p, 0x908) != 0 ? U32(act, 0x27c) : U32(act, 0x268);
                const uint32_t a[2] = {act, U32(table, 4 * slot)};
                { const rx_addr s = loop_push(2, a, 0x00766fad); rx_activity_on_player_left(a[0], a[1]); loop_pop(s); }   /* Activity_OnPlayerLeft 0055c1d0 */
            }
            const uint32_t a[1] = {(uint32_t)(int32_t)(int8_t)U8(p, 0x1c8)};
            rx_addr driver;
            {   /* 0055f630 (EAX = the activity manager, EDI = the player's grid kind +0x1bc, stack the area): the core's port, else the original with EDI set */
                __typeof__(&rx_activity_find_by_kind_area) f = LOOP_CORE(0x0055f630, rx_activity_find_by_kind_area);
                if (f) { const rx_addr s = loop_push(1, a, 0); driver = f(world + 0x21da0, U32(p, 0x1bc), a[0]); loop_pop(s); }
                else { rx_regs in = loop_regs(); in.r[LOOP_EAX] = world + 0x21da0; in.r[LOOP_EDI] = U32(p, 0x1bc); driver = loop_ex(0x0055f630, in, 1, a); }
            }
            if (U32(p, 0x1ac) != 0 && U32(p, 0x1b4) != 0) {
                driver_joined(p, driver);
                SU32(p, 0x1bc, 0x3d);
            }
        }
        for (uint32_t i = 0; i < player_count(list, 3); i++) {
            const rx_addr p = player_at(list, i, 3);
            if (U32(p, 0x1bc) == 0x3d || (U32(p, 0x1ac) != 0 && U32(p, 0x1b4) != 0)) continue;
            rx_addr driver = 0;
            for (int32_t k = 0; k < I32(world, 0x21da8); k++) {
                const rx_addr d = U32(U32(world, 0x21da4), 4u * (uint32_t)k);
                if (U32(p, 0x1bc) == U32(d, 0x23c) && (int32_t)(int8_t)U8(p, 0x1c8) == I32(d, 0x220)) { driver = d; break; }
            }
            SU32(driver, 0x10, 1);
        }
    } else {
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr p = player_at(list, i, 3);
            if (needs_spawn(world, p)) {
                if (U32(p, 0x10) == 1) {
                    spawn_step(p, world);
                    spawn_step(p, world);
                } else if (spawn_step(p, world) != 4) {
                    rx_set_sp(sp0);
                    return;
                }
            }
            if (U32(p, 0x94) != 0) continue;
            SU32(p, 0x9c, 0x3e);
            if (U32(p, 0x1ac) != 0 && U32(p, 0x1b4) != 0) driver_joined(p, U32(U32(world, 0x21da4), 0));
        }
    }
    const uint32_t drivers = U32(world, 0x21da8);
    for (uint32_t k = 0; k < drivers; k++) {
        const rx_addr d = U32(U32(world, 0x21da4), 4 * k);
        const uint32_t request = U32(d, 0x10);
        if (request == 2) {
            loop_tc0(loop_vslot(d, 0x44), d);
            loop_tc0(loop_vslot(d, 0x3c), d);
        }
        if (request == 2 || request == 1) SU32(d, 0x10, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00766ec0, rx_world_update_players)

/* ---- the pending event handlers (005933a0, 00705640) ----
 * A channel (EventSystem::Channel 0x5c: +0x28 its critical section, +0x40 a std::list {+0x40 the container proxy, +0x54 head,
 * +0x58 size}) queues pending dispatch records: a 0x10-byte value {+0 the event map's iterator container proxy, +4 the map node,
 * +8 the payload (_aligned_malloc), +0xc the payload size}. The record's map node holds the event's name at +0xc (a std::string:
 * +0x18 capacity, +4 the heap pointer or +4 the inline buffer). */

/* 00705640 (EDI = the list, stdcall out iterator, container proxy, node; RET 0xc; EAX = out): erase the node (unlink it from the
 * doubly linked list, free it with ReflexAlignedFree 0078fc50, size-- unless it is the head) and return {container, node->next}.
 * The checked iterators' tests call _invalid_parameter_noinfo (the CRT handler). Frame: sub esp, 8; push ebx / ebp / esi. */
rx_addr rx_pending_list_erase(rx_addr list, rx_addr out, rx_addr cont, rx_addr node) {
    if (cont == 0) crt_invalid_parameter_noinfo();
    const rx_addr l = U32(cont, 0);
    if (node == U32(l, 0x14)) crt_invalid_parameter_noinfo();
    const rx_addr next = U32(node, 0);
    if (node != U32(list, 0x14)) {
        const rx_addr prev = U32(node, 4);
        SU32(prev, 0, next);
        SU32(next, 4, prev);
        { const rx_addr s = loop_push(1, (uint32_t[]){node}, 0x0070569f);
          rx_reflex_aligned_free(node);
          loop_pop(s); }
        SU32(list, 0x18, U32(list, 0x18) - 1);
    }
    SU32(out, 0, U32(list, 0));
    SU32(out, 4, next);
    return out;
}
RX_FUNCTION(0x00705640, rx_pending_list_erase)

/* the map-iterator check of the dispatch loop (two inlined dereferences of the pending record's iterator) */
static void pending_iter_check(rx_addr rec) {
    if (U32(rec, 0) == 0) crt_invalid_parameter_noinfo();
    const rx_addr map = U32(U32(rec, 0), 0);
    if (U32(rec, 4) == U32(map, 0x18)) crt_invalid_parameter_noinfo();
}

/* 005933a0 (stdcall channel; RET 4): drain up to 0x19 pending dispatch records from the channel's list under its critical section
 * (+0x28), then, outside it, dispatch each (004020c0 with the record's event name, payload and size), free the payload (msvcr90
 * _aligned_free under the allocator lock 0xd61704 while 0xda67d8) and the record (ReflexAlignedFree 0078fc50).
 * Frame: push ebp; and esp, -8; sub esp, 0x7c; push ebx / esi / edi: the count at -0x7c, the out iterator at -0x70, the 25-record
 * snapshot at -0x68 (no /GS cookie in this one). */
void rx_pending_handlers_run(rx_addr ch) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~7u;
    rx_set_sp(A - 0x88u);
    if (U32(ch, 0x58) == 0) { rx_set_sp(sp0); return; }
    const rx_addr out = A - 0x70u, snap = A - 0x68u;
    loop_enter_cs(ch + 0x28);
    const rx_addr cont = U32(ch, 0x40), head = U32(ch, 0x54);
    rx_addr node = U32(head, 0);
    uint32_t n = 0;
    while (node != head && n < 0x19) {
        if (cont == 0) crt_invalid_parameter_noinfo();
        if (node == U32(U32(cont, 0), 0x14)) crt_invalid_parameter_noinfo();
        SU32(snap, 4u * n, U32(node, 8));
        { const uint32_t a[3] = {out, cont, node};
          const rx_addr s = loop_push(3, a, 0x00593432);
          rx_pending_list_erase(ch + 0x40u, out, cont, node);
          loop_pop(s); }
        node = U32(out, 4);
        n++;
    }
    loop_leave_cs(ch + 0x28);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr rec = U32(snap, 4u * i);
        pending_iter_check(rec);
        pending_iter_check(rec);
        const rx_addr str = U32(rec, 4) + 0xcu;
        const rx_addr name = U32(str, 0x18) >= 0x10u ? U32(str, 4) : str + 4u;
        const uint32_t payload = U32(rec, 8), size = U32(rec, 0xc);
        { const uint32_t a[2] = {payload, size};
          const rx_addr s = loop_push(2, a, 0x005934c2);
          rx_world_event_dispatch(ch, name, payload, size);
          loop_pop(s); }
        if (payload != 0) {
            if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
            crt_aligned_free(payload);
            if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
        }
        { const rx_addr s = loop_push(1, (uint32_t[]){rec}, 0x00593501);
          rx_reflex_aligned_free(rec);
          loop_pop(s); }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x005933a0, rx_pending_handlers_run)


/* ---- the replay camera directors (007678b0) ---- */

/* a director virtual with no stack arguments whose result is in st(0) (the core's port as a C float, else the original) */
static float director_value(rx_addr d) {
    const rx_addr fn = loop_vslot(d, 0x94);
    float (*f)(rx_addr) = RX_FN(fn, float (*)(rx_addr));
    if (f) { const rx_addr s = loop_push(0, 0, 0); const float r = f(d); loop_pop(s); return r; }
    rx_regs in = loop_regs(), out;
    in.r[LOOP_ECX] = d;
    rx_call_original_ex(fn, &in, &out, 0, 0);
    return out.st0;
}

/* 007678b0 ReplayCameras_Update (world, dt; ret 8): the replay camera directors and the replay camera controller.
 *  - the developer key 0xd5e270 (KeyMap::vf1(7, 0), only with the developer keys on 0x9b716d) toggles world +0x21de5;
 *  - an active +0x21de5 picks the first player (list +0xdf8c slots, player +0x10 == 0) and its vehicle (+0x1ac); the controller
 *    +0x21de0 is used instead when set;
 *  - the vehicle/controller's player (+0x190) and its +0x21c flag: while it is set nothing is selected; else the selection timer
 *    +0x21e00 += dt and, past 0.5 s, the director with the smallest value (+0x21dd8 list, +0x310 next; the director's slot 0x94)
 *    becomes the current one (+0x21dfc): the old one's vf29 (slot 0x74), the new one's vf28(0) (slot 0x70) and vf38 (slot 0x98),
 *    the timer zeroed. When it is already current and has +0x2f4 clear, vf28(0) / vf38 run again;
 *  - the current director is copied into the scene's first view (0063dba0), and, with world +0x21de4, the selected player's camera
 *    (+0x974 + 4 * +0x988) too;
 *  - with 0xa9434f every director's slot 0x9c runs.
 * Frame: push ebp; and esp, -16; sub esp, 0x14; push ebx / esi / edi (locals at -0x10 / -0xc / -8, the cookie at -4). */
void rx_replay_cameras_update(rx_addr world, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu;
    rx_set_sp(A - 0x20u);
    const rx_addr L18 = A - 8u;
    SU32(A, -4u, U32(0, 0x009b45e4) ^ A);                          /* the security cookie */
    rx_addr ent = U32(world, 0x21de0);                             /* the replay camera controller */
    if (U8(0, 0x009b716d) == 0) {
        if (loop_tc2(0x00811420, 0x00d5e270, 7, 0) != 0) {
            const uint8_t on = U8(world, 0x21de5) == 0;
            SU8(world, 0x21de5, on);
        }
    }
    if (ent == 0) {
        if (U8(world, 0x21de5) != 0) {
            const rx_addr list = U32(0, 0x00a946b8) + 0x7460u;
            rx_addr p = 0;
            if (U32(list, 0xdf8c) != 0xffffffffu) {
                const uint32_t n = U8(list, 0xdfbc);
                for (uint32_t i = 0; i < n; i++) {
                    const rx_addr q = rx_seat_active(list, i);
                    if (U32(q, 0x10) != 0) continue;
                    p = q;
                    break;
                }
            }
            ent = p != 0 ? U32(p, 0x1ac) : U32(0, 0x1ac);
        }
        if (ent == 0) {
            const rx_addr cur = U32(world, 0x21dfc);
            if (cur != 0) loop_tc0(loop_vslot(cur, 0x74), cur);
            SU32(world, 0x21dfc, 0);
            SF32(world, 0x21e00, F32(0, 0x00977d64));
            goto tail;
        }
    }
    ent = U32(ent, 0x190);                                         /* the player the camera belongs to */
    SU32(L18, 0, ent);
    if (ent != 0 && U8(ent, 0x21c) != 0) goto tail;
    {
        const float t = F32(world, 0x21e00) + dt;
        SF32(world, 0x21e00, t);
        if (t > F32(0, 0x009322cc)) {                              /* comiss; jbe: not above 0.5 (or unordered) */
            rx_addr best = 0;
            float bestv = F32(0, 0x00977494);                      /* FLT_MAX */
            for (rx_addr d = U32(world, 0x21dd8); d != 0; d = U32(d, 0x310)) {
                const float v = director_value(d);
                if (bestv > v) { bestv = v; best = d; }            /* fcomip; jbe: keep the current best */
            }
            const rx_addr cur = U32(world, 0x21dfc);
            if (cur != best) {
                if (cur != 0) loop_tc0(loop_vslot(cur, 0x74), cur);
                SU32(world, 0x21dfc, best);
                if (best != 0) {
                    loop_tc1(loop_vslot(best, 0x70), best, 0);
                    const rx_addr c2 = U32(world, 0x21dfc);
                    loop_tc0(loop_vslot(c2, 0x98), c2);
                }
                SF32(world, 0x21e00, 0.0f);
            } else if (cur != 0 && U32(cur, 0x2f4) == 0) {
                loop_tc1(loop_vslot(cur, 0x70), cur, 0);
                const rx_addr c2 = U32(world, 0x21dfc);
                loop_tc0(loop_vslot(c2, 0x98), c2);
            }
        }
    }
    {
        const rx_addr cur = U32(world, 0x21dfc);
        if (cur != 0 && U32(0, 0x00d61334) == 0) {
            const rx_addr scene = U32(world, 0x20138);
            const uint32_t a[2] = {cur, U32(scene, 0x19b8)};
            const rx_addr s = loop_push(2, a, 0x00767acb);
            rx_cam_entity_to_view(cur, U32(scene, 0x19b8));
            loop_pop(s);
        }
    }
    if (U8(world, 0x21de4) != 0) {
        const rx_addr p = U32(L18, 0);
        if (p != 0) {
            const rx_addr scene = U32(world, 0x20138);
            const uint32_t idx = U32(p, 0x988);
            const rx_addr cam = U32(p, 0x974 + 4u * idx);                 /* the player's camera of index +0x988 */
            const uint32_t a[2] = {cam, U32(scene, 0x19b8)};
            const rx_addr s = loop_push(2, a, 0x00767afc);
            rx_cam_entity_to_view(cam, U32(scene, 0x19b8));
            loop_pop(s);
        }
    }
tail:
    if (U8(0, 0x00a9434f) != 0) {
        for (rx_addr d = U32(world, 0x21dd8); d != 0; d = U32(d, 0x310))
            loop_tc0(loop_vslot(d, 0x9c), d);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007678b0, rx_replay_cameras_update)


/* 007674b0 MXWorld::vf5 (dt; ret 4) -> 1: the world's frame. The network clock's step since the last frame (>= 0) for the
 * drivers. Paused (+0x20014): only the replay cameras and the replay director (slot 19) run, and the clock bookkeeping skips
 * (00a89bf0 = 1). Else: world time += dt, the player slots, the physics (unless the engine is paused: timing +0x0c), the rider
 * animation jobs (kicked and waited for), the drivers (0055f700), the players (00552a10), the debug toggle, the activity-start
 * sequencer, the replay cameras and the world's sound zones (00674e90). */
uint32_t rx_world_update(rx_addr world, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u;
    rx_set_sp(A - 0x10);
    if ((U32(0, 0x00dcf748) & 1) == 0) {
        SU32(0, 0x00dcf748, U32(0, 0x00dcf748) | 1);
        SF32(0, 0x00dcf744, net_clock());
    }
    float step = net_clock() - F32(0, 0x00dcf744);
    if (!(step > 0.0f)) step = 0.0f;
    const float now = net_clock();
    SF32(0, 0x00dcf744, now);
    if (U32(world, 0x20014) != 0) {
        if (U32(world, 0x21dfc) != 0) {
            replay_cameras_update(world, dt);
            const rx_addr director = U32(world, 0x21dfc);
            if (director != 0) loop_tc_f(loop_vslot(director, 0x4c), director, dt);
        }
        SU32(0, 0x00a89bf0, 1);
        rx_set_sp(sp0);
        return 1;
    }
    SF64(world, 0x20020, (double)dt + F64(world, 0x20020));
    rx_world_update_players(world);
    if (U32(loop_timing(), 0xc) == 0) physics_update(U32(world, 0x20134), dt);
    else SU32(0, 0x00a89bf0, 1);
    cdecl0(0x0061d060);                                       /* RiderAnimation_KickJobs (rx_anim) */
    cdecl0(0x0061d130);                                       /* RiderAnimation_WaitJobs */
    drivers_update(world + 0x21da0, step);
    players_update(U32(0, 0x00a946b8) + 0x7460, dt);
    rx_world_debug_toggle(world);
    if (U32(world, 0x22130) != 0 && U32(world, 0x2210c) != 0) loop_tc0(0x005399e0, world + 0x21e10);
    replay_cameras_update(world, dt);
    cdecl_f(0x00674e90, dt);                                  /* the crowd audio's frame update (the product runs the port, world_crowd_audio.c) */
    rx_set_sp(sp0);
    return 1;
}
RX_FUNCTION(0x007674b0, rx_world_update)
