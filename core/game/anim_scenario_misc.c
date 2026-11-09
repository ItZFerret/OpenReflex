/* anim_scenario_misc.c -- the FreeRide scenario's podium setup (miscport, census): 0074ada0 binds the activity to the podium
 * (scenario object at world + 0x20150), creates the podium riders (0074ba40), collects the scene's podium / crew entities by class
 * and dresses the crew chiefs (0074c6d0 -> CrewChief_SetShirt 006bd760); 0074b180 is its release. Frames and registers as the
 * original; calls of other units' ports through their headers. */
#include "anim.h"
#include "anim_extern.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "race_activity.h"
#include "build_activity_int.h"
#include "build_rider.h"
#include "build_vehicle.h"
#include "fx_mud_bind.h"
#include "world_rider.h"
#include "rx_vcall.h"
#include "rider_cap.h"

/* 0068f300 (ECX = map object (unused), stack node; RET 4): frees a std::map subtree: the right subtrees recursively, the nodes
 * along the left spine (ReflexAlignedFree); stops at the nil node (+0x15 != 0). Frame: push ebx / esi / edi. */
void rx_tree_free_subtree(rx_addr map, rx_addr node) {
    const rx_addr sp0 = rx_sp();
    rx_addr edi = node, esi = node;
    if (U8(edi, 0x15) != 0) return;
    do {
        rx_set_sp(sp0 - 0x10);                                   /* push ebx / esi / edi, push the argument */
        SU32(sp0 - 0x10, 0, U32(esi, 8));
        rx_set_sp(sp0 - 0x14);
        SU32(sp0 - 0x14, 0, 0x0068f31c);
        rx_tree_free_subtree(map, U32(esi, 8));
        esi = U32(esi, 0);
        rx_set_sp(sp0 - 0x14);
        SU32(sp0 - 0x14, 0, edi);
        SU32(sp0 - 0x18, 0, 0x0068f324);
        rx_set_sp(sp0 - 0x18);
        rx_reflex_aligned_free(edi);
        rx_set_sp(sp0 - 0xc);
        edi = esi;
    } while (U8(esi, 0x15) == 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0068f300, rx_tree_free_subtree)

/* 006bd760 CrewChief_SetShirt (EAX = a driver's character: its +0x18c the id, stack chief; RET 4): puts the "CrewChief_ShirtColor_
 * Default" texture -> the shirt texture of the player whose id (+4) is the character's +0x18c (the game's list +0x154d8, count
 * +0x154dc) into the chief's texture override map (+0x374: head +0x18, size +0x1c; the old nodes freed by 0068f300 and the map
 * emptied). The textures are resolved by the engine's asset manager (virtual 0x6c: name, 'text', 'ure', 1) and marked in use
 * (+0x4c) while the map entry is written. A missing player leaves edi = 0 (the original reads its +0x94 at address 0x94). */
void rx_crew_chief_set_shirt(rx_addr driver, rx_addr chief) {
    const rx_addr sp0 = rx_sp();
    const rx_addr game = U32(0, 0x00a946b8);
    const int32_t count = I32(game, 0x154dc);
    rx_addr entry = 0;
    {
        int32_t i = 0;
        rx_addr p = U32(game, 0x154d8);
        if (count > 0) {
            for (;;) {
                const rx_addr e = U32(p, 0);
                if (e != 0 && U32(e, 4) == U32(driver, 0x18c)) { entry = e; break; }
                i++; p += 4;
                if (!(i < count)) break;
            }
        }
    }
    const rx_addr map = chief + 0x374;
    rx_set_sp(sp0 - 0x14);                                       /* push edi / ebp / esi / ebx, the argument */
    SU32(sp0 - 0x14, 0, U32(U32(chief, 0x38c), 4));
    rx_set_sp(sp0 - 0x18);
    rx_tree_free_subtree(map, U32(U32(chief, 0x38c), 4));
    rx_set_sp(sp0 - 0x10);
    rx_addr head = U32(map, 0x18);
    SU32(head, 4, head);
    head = U32(map, 0x18);
    SU32(map, 0x1c, 0);
    SU32(head, 0, head);
    head = U32(map, 0x18);
    SU32(head, 8, head);
    if (U32(entry, 0x94) != 0) {
        const rx_addr mgr = U32(U32(0, 0x00a95844), 8);
        const uint32_t a[4] = {0x00940464 /* "CrewChief_ShirtColor_Default" */, 0x74786574, 0x657275, 1};
        const rx_addr tex = rx_vcall(mgr, 0x6c, 4, a);
        SU32(tex, 0x4c, 1);
        const rx_addr data = U32(tex, 0x44);
        if (data != 0) {
            const rx_addr shirt = U32(entry, 0x94);
            SU32(shirt, 0x4c, 1);
            const rx_addr value = U32(shirt, 0x44);
            if (value != 0) {
                const rx_addr key = sp0 + 4;                     /* the key is built in the argument's slot */
                SU32(key, 0, U32(U32(data, 0x10), 4u * U32(data, 0x18)));
                rx_addr cell;
                rx_set_sp(sp0 - 0x20);                           /* push the key's address and the map */
                SU32(sp0 - 0x20, 0, map); SU32(sp0 - 0x1c, 0, key); SU32(sp0 - 0x24, 0, 0x006bd82d);
                rx_set_sp(sp0 - 0x24);
                cell = rx_uint_map_index(map, key);
                rx_set_sp(sp0 - 0x10);
                SU32(cell, 0, value);
                SU32(U32(entry, 0x94), 0x4c, 0);
            }
            SU32(tex, 0x4c, 0);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006bd760, rx_crew_chief_set_shirt)

/* 0074b180 (EAX = podium object; stack flag; RET 4): the podium released: the engine's frame object waited (0084ea00 with EDI =
 * [[0xa95844] + 0x10]), a bound activity's sequences finished (0055e2b0), the scenario's cast released (0074bd50), +0x44 = 0; with
 * the flag set the podium's lists are cleared too (+0x172c, +0x744, the 4-dword table +0x16ec (0x40 bytes), +0x40, +0x290 and
 * the 0x4b0 bytes from +0x294). Frame: push ecx / ebx / esi / edi. */
void rx_podium_release(rx_addr podium, uint32_t clear) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);
    const rx_addr edi = U32(U32(0, 0x00a95844), 0x10);
    rx_set_sp(sp0 - 0x14);
    rx_engine_frame_mark(edi);
    rx_set_sp(sp0 - 0x10);
    if (U32(podium, 0x40) != 0) {
        rx_set_sp(sp0 - 0x18);
        SU32(sp0 - 0x14, 0, U32(podium, 0x40));
        rx_activity_finish_sequences(U32(podium, 0x40));
        rx_set_sp(sp0 - 0x10);
    }
    rx_set_sp(sp0 - 0x18);
    SU32(sp0 - 0x14, 0, podium);
    rx_scenario_cast_release(podium);
    rx_set_sp(sp0 - 0x10);
    SU32(podium, 0x44, 0);
    if (clear != 0) {
        const rx_addr tab = podium + 0x16ec;
        SU32(podium, 0x172c, 0);
        SU32(podium, 0x744, 0);
        crt_memset(tab, 0, 0x40);
        SU32(podium, 0x40, 0);
        SU32(podium, 0x290, 0);
        crt_memset(podium + 0x294, 0, 0x4b0);
        SU32(tab, 0, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074b180, rx_podium_release)

/* 0074c6d0 (EDI = podium; plain ret): dresses the crew chiefs (the podium's list +0x16ec, count +0x172c): the first min(n, count)
 * chiefs get the shirt of the activity's driver table entry (+0x2d0 + 4 i, n = the activity's +0x26c), the rest cycle through the
 * table (i mod n); empty entries are skipped. Frame: push ebp; and esp, -8; sub esp, 8; push ebx / esi. */
void rx_podium_set_shirts(rx_addr podium) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4) & ~7u;
    rx_set_sp(A - 0x10);
    const rx_addr act = U32(podium, 0x40);
    const int32_t n = I32(act, 0x26c);
    if (n == 0) { rx_set_sp(sp0); return; }
    int32_t i = 0;
    if (n > 0) {
        uint32_t off = 0x2d0;
        rx_addr p = podium + 0x16ec;
        for (;;) {
            if (i >= I32(podium, 0x172c)) { rx_set_sp(sp0); return; }
            const rx_addr chief = U32(p, 0);
            if (chief != 0) {
                const rx_addr drv = U32(U32(podium, 0x40) + off, 0);
                const rx_addr veh = U32(U32(drv, 0x74), 0x1ac);
                rx_set_sp(A - 0x18); SU32(A - 0x18, 0, chief);
                rx_crew_chief_set_shirt(veh, chief);
                rx_set_sp(A - 0x10);
            }
            off += 4; i++; p += 4;
            if (!(i < n)) break;
        }
    }
    if (i >= I32(podium, 0x172c)) { rx_set_sp(sp0); return; }
    rx_addr p = podium + 4u * (uint32_t)i + 0x16ec;
    do {
        const int32_t slot = i % n;
        const rx_addr drv = U32(U32(podium, 0x40) + 0x2d0, 4u * (uint32_t)slot);
        if (drv != 0) {
            const rx_addr chief = U32(p, 0);
            if (chief != 0) {
                const rx_addr veh = U32(U32(drv, 0x74), 0x1ac);
                rx_set_sp(A - 0x18); SU32(A - 0x18, 0, chief);
                rx_crew_chief_set_shirt(veh, chief);
                rx_set_sp(A - 0x10);
            }
        }
        i++; p += 4;
    } while (i < I32(podium, 0x172c));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074c6d0, rx_podium_set_shirts)

/* 0074ba40 PodiumRiders_Create (stack podium; RET 4): for each of the game's podium places (byte +0x1541c): the player of the
 * places's index among the human players (00552b90 on the player list 0x7460, kind 3), a rider character for it
 * (0053eab0: the game's +0x15504, the activity's world, the player's rider +0x5b51c), its number plate / body textures
 * ("0x941724" / "0x941738" -> the player's +0xb24 / +0xb28) in the character's override map (+0x380), the character shown
 * (+0x3a0 = 0, flags +0xb0 |= 1) and placed at the podium marker (world +0x2214c: vec4 at +0x90 -> character +0x3c0). The
 * records of 0x30 bytes from podium + 0x50: +0 the character, +4 the player. Frame: push ebp; and esp, -16; sub esp, 0x24; push
 * ebx / esi / edi. */
void rx_podium_riders_create(rx_addr podium) {
    const rx_addr sp0 = rx_sp();
    const rx_addr A = (sp0 - 4) & ~15u;
    const rx_addr B = A - 0x24 - 8;                             /* after push ebx / esi */
    const rx_addr S = B - 4;                                    /* after push edi */
    rx_set_sp(S);
    const int32_t count = (int32_t)U8(U32(0, 0x00a946b8), 0x1541c);
    const rx_addr marker = U32(U32(U32(podium, 0x40), 0x234), 0x2214c);
    const uint32_t vx = U32(marker, 0x90), vy = U32(marker, 0x94), vz = U32(marker, 0x98), vw = U32(marker, 0x9c);
    SU32(B, 0x1c, vx); SU32(B, 0x20, vy); SU32(B, 0x24, vz); SU32(B, 0x28, vw);
    SU32(B, 0x18, (uint32_t)count);
    rx_addr rec = podium + 0x50;
    for (int32_t i = 0; i < count; i++) {
        if (i >= (int32_t)RX_RC_ORIG) rec = rx_cast_rec(podium, (uint32_t)i);   /* (w-rc-core) records 12..31 in side storage */
        const rx_addr game = U32(0, 0x00a946b8);
        rx_set_sp(S - 8);
        SU32(S - 8, 0, (uint32_t)i); SU32(S - 4, 0, 3); SU32(S - 12, 0, 0x0074bac3);
        rx_set_sp(S - 12);
        const rx_addr player = rx_world_players_find(game + 0x7460, (uint32_t)i, 3);
        rx_set_sp(S);
        SU32(rec, 4, player);
        const rx_addr rider = U32(player, 0x1b4);
        const uint32_t id = U32(rider, 0x5b51c);
        const rx_addr world = U32(U32(podium, 0x40), 0x234);
        const rx_addr mgr = U32(0, 0x00a946b8) + 0x15504;
        rx_set_sp(S - 0xc);
        SU32(S - 0xc, 0, mgr); SU32(S - 8, 0, world); SU32(S - 4, 0, id); SU32(S - 0x10, 0, 0x0074baf2);
        rx_set_sp(S - 0x10);
#ifdef RX_ORACLE
        /* the rider spawn's own dead-stack differences (known-differences.md 1, rx_build_rider's RiderSpawnCharacter) are not this function's:
         * the oracle runs the original spawn, the product the core one */
        const uint32_t spawn_args[3] = {mgr, world, id};
        const rx_addr chr = rx_call_original(0x0053eab0, RX_STDCALL, 0, 3, spawn_args);
#else
        const rx_addr chr = rx_rider_spawn_character(mgr, world, id);
#endif
        rx_set_sp(S);
        SU32(rec, 0, chr);
        const rx_addr map = chr + 0x380;
        rx_set_sp(S - 0xc);
        rx_model_override_texture(map, 0x00941724, U32(player, 0xb24));
        rx_model_override_texture(map, 0x00941738, U32(player, 0xb28));
        rx_set_sp(S);
        SU32(chr, 0x3a0, 0);
        SU16(chr, 0xb0, (uint16_t)(U16(chr, 0xb0) | 1));
        const rx_addr dst = chr + 0x3c0;
        SU32(dst, 0, U32(B, 0x1c)); SU32(dst, 4, U32(B, 0x20)); SU32(dst, 8, U32(B, 0x24)); SU32(dst, 0xc, U32(B, 0x28));
        rec += 0x30;
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074ba40, rx_podium_riders_create)

/* 0074ada0 (EAX = activity, ECX = the podium object): binds the activity to the podium (+0x40, +0x44 = 1; a previously bound one is
 * released first: 0074b180 with the flag set), with the free ride's drivers table (0xd61048 set) the activity's podium drivers
 * (+0x2d0 / +0x2d4 / +0x2d8) default to the first / second (> 1 podium place) / third (> 2) of its driver list (+0x268), the
 * riders created (0074ba40), the scene (world + 0x20138) queried by class (007d2920): 0x103a -> +0x290 (1), 0x104f -> +0x294
 * (300; the count in +0x744), 0x101a -> +0x748 (1000; +0x16e8), 0x103b (16 on the stack: the chiefs with +0x354 == 0 go into the
 * list +0x16ec, one with it set into +0x1730; the shirts dressed by 0074c6d0), and the 12 pairs from +0x1734 cleared.
 * Frame: sub esp, 0x4c; push ebx / ebp / esi / edi. */
void rx_podium_setup(rx_addr act, rx_addr podium) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = sp0 - 0x4c - 16;
    rx_set_sp(S);
    SU32(podium, 0x44, 1);
    if (U32(podium, 0x40) != 0) {
        SU32(S, -4, 1); SU32(S, -8, 0x0074adc2);
        rx_set_sp(S - 8);
        rx_podium_release(podium, 1);
        rx_set_sp(S);
    }
    SU32(podium, 0x40, act);
    if (U32(0, 0x00d61048) != 0) {
        if (U32(act, 0x2d0) == 0) SU32(act, 0x2d0, U32(U32(act, 0x268), 0));
        const rx_addr game = U32(0, 0x00a946b8);
        if (U32(act, 0x2d4) == 0 && U8(game, 0x1541c) > 1) SU32(act, 0x2d4, U32(U32(act, 0x268), 4));
        if (U32(act, 0x2d8) == 0 && U8(game, 0x1541c) > 2) SU32(act, 0x2d8, U32(U32(act, 0x268), 8));
    }
    SU32(S, -4, podium); SU32(S, -8, 0x0074ae28);
    rx_set_sp(S - 8);
    rx_podium_riders_create(podium);
    rx_set_sp(S);
    const rx_addr scratch = S + 0x14, list = S + 0x18;
    {
        rx_addr scene = U32(U32(act, 0x234), 0x20138);
        rx_set_sp(S - 0x14); SU32(S, -4, 0); SU32(S, -8, 1); SU32(S, -0xc, podium + 0x290); SU32(S, -0x10, 0x103a);
        SU32(S, -0x14, 0x0074ae4c);
        (void)rx_scene_manager_collect_typed(scene, 0x103a, podium + 0x290, 1, 0);
        rx_set_sp(S);
        scene = U32(U32(act, 0x234), 0x20138);
        rx_set_sp(S - 0x14);
        const uint32_t n1 = rx_scene_manager_collect_typed(scene, 0x104f, podium + 0x294, 300, scratch);
        rx_set_sp(S);
        SU32(podium, 0x744, n1);
        scene = U32(U32(act, 0x234), 0x20138);
        rx_set_sp(S - 0x14);
        const uint32_t n2 = rx_scene_manager_collect_typed(scene, 0x101a, podium + 0x748, 1000, scratch);
        rx_set_sp(S);
        SU32(podium, 0x16e8, n2);
    }
    crt_memset(podium + 0x16ec, 0, 0x40);
    SU32(podium, 0x1730, 0);
    crt_memset(list, 0, 0x40);
    SU32(podium, 0x172c, 0);
    rx_addr scene = U32(U32(act, 0x234), 0x20138);
    rx_set_sp(S - 0x14);
    const int32_t n = (int32_t)rx_scene_manager_collect_typed(scene, 0x103b, list, 0x10, 0);
    rx_set_sp(S);
    if (n != 0) {
        for (int32_t i = 0; i < n; i++) {
            const rx_addr chief = U32(list, 4u * (uint32_t)i);
            if (U32(chief, 0x354) == 0) {
                const uint32_t k = U32(podium, 0x172c);
                SU32(podium, 0x16ec + 4 * k, chief);
                SU32(podium, 0x172c, U32(podium, 0x172c) + 1);
            } else SU32(podium, 0x1730, chief);
        }
        SU32(S, -4, 0x0074af32);
        rx_set_sp(S - 4);
        rx_podium_set_shirts(podium);
        rx_set_sp(S);
    }
    for (uint32_t k = 0; k < 12; k++) { SU32(podium, 0x1734 + 8 * k, 0); SU32(podium, 0x1738 + 8 * k, 0); }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0074ada0, rx_podium_setup)
