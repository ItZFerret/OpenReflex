/* race_activity_misc.c -- small race / activity / scene helpers that were still called as originals (census, miscport).
 * Each function is a hand transliteration of the original (address in its comment); register conventions as the callers use them. */
#include "phys_studio.h"
#include "race_activity_misc.h"
#include "records_leave_highlights.h"
#include "race_extern.h"
#include "crt.h"
#include "phys_coll_misc.h"
#include "loop_platform.h"
#include "rx_vcall.h"
#include "build_vehicle_tree.h"
#include "build_vehicle.h"
#include "build_tables.h"
#include "build_alloc.h"
#include "race.h"
#include "anim.h"
#include "anim_scenario.h"
#include "world_misc.h"
#include "world_rider.h"
#include "loop_player.h"
#include "loop_world.h"
#include "race_activity.h"
#include "race_activity_ui.h"
#include "race_challenge.h"
#include "build_activity.h"
#include "rider_cap.h"

/* 004fb0e0 (EAX = vector {+0 owner proxy, +0xc first, +0x10 last}; plain ret) -> ECX: vector::clear() = erase(first, last) of the
 * checked 4-byte-item vector (the iterator range checks call _invalid_parameter_noinfo). The out iterator is the frame's 8 bytes at
 * F + 0x10 (its first word also serves as the owner copy pushed as the third argument). ECX is what 007cf450 leaves: with
 * nothing to erase the vector's owner pointer, else the moved item count * 4 (0: the range ends at the vector's end). */
uint32_t rx_vector_clear(rx_addr vec) {
    RACE_FRAME_EBP(F, 8, 0xc, 3);                 /* push ebp; and esp, -8; sub esp, 0xc; push ebx / esi / edi */
    const rx_addr out = F + 0x10;
    const rx_addr last = U32(vec, 0x10);          /* ebx */
    if (U32(vec, 0xc) > last) crt_invalid_parameter_noinfo();
    const rx_addr first = U32(vec, 0xc);          /* edi */
    SU32(out, 0, U32(vec, 0));
    if (first > U32(vec, 0x10)) crt_invalid_parameter_noinfo();
    const uint32_t owner = U32(out, 0);
    SU32(F, -4, last); SU32(F, -8, owner); SU32(F, -12, first); SU32(F, -16, U32(vec, 0)); SU32(F, -20, out);
    SU32(F, -24, 0x004fb124);
    RACE_CORE_CALL(5, phys_vector_erase(vec, out, first, last));
    const uint32_t ecx = first == last ? U32(vec, 0) : 0;
    RACE_FRAME_END(F);
    return ecx;
}
RX_FUNCTION(0x004fb0e0, rx_vector_clear)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* 005dfc90 (cdecl: language name or 0; plain ret) -> EAX: the UI's language table lookup (shared by every UI text and the race
 * code: the profile name only fills when it exists). Under the table's critical section 0xaf695c: a name that is null or "" is
 * the "Default" language (0x9c4f18, the same function again); else the current locale's suffix (the entry [0xaf6950] +
 * 0x1c * [0xa94074]: a std::string, inline below 16 chars) forms "<name>.<suffix>" (0x93a024 "%s.%s", 0078fd60) and the list
 * 0xaf6888 of language handles is searched by _stricmp against each language's name (virtual 8 of the object the handle points
 * at); then the bare name ("%s" 0x9419e4). Returns the handle (the list's element) or 0. The list helpers 005e3ee0 (count)
 * and 005e3ff0 (checked element address) are inlined. ECX / EDX are not modelled (UI callers only read EAX).
 * Frame: sub esp, 0x108 (the buffer at +0, the cookie at +0x104); push ebx / ebp / esi, and edi for the table path. */
static void ui_invalid_parameter(void) { crt_invalid_parameter_noinfo(); }
static uint32_t ui_lang_count(void) { return (uint32_t)((int32_t)(U32(0, 0x00af6888 + 0x10) - U32(0, 0x00af6888 + 0xc)) >> 2); }
static rx_addr ui_lang_at(uint32_t i) {
    if (!(i < ui_lang_count())) ui_invalid_parameter();
    return U32(0, 0x00af6888 + 0xc) + 4u * i;
}
/* the search of the list for the name in buf: the handle, or 0 */
static rx_addr ui_lang_search(rx_addr buf) {
    if (ui_lang_count() == 0) return 0;
    for (uint32_t i = 0;;) {
        const rx_addr handle = U32(ui_lang_at(i), 0);
        const rx_addr obj = U32(handle, 0);
        const uint32_t name = rx_vcall(obj, 8, 0, NULL);
        if (crt_stricmp(buf, name) == 0) return U32(ui_lang_at(i), 0);
        i++;
        if (!(i < ui_lang_count())) return 0;
    }
}
rx_addr rx_ui_language_find(rx_addr name) {
    const rx_addr sp0 = rx_sp(), E0 = sp0 - 0x108, buf = E0;
    rx_set_sp(E0);
    SU32(E0, 0x104, U32(0, 0x009b45e4) ^ E0);
    loop_enter_cs(0x00af695c);
    rx_addr result = 0;
    if (name == 0 || crt_stricmp(name, 0x00935d6c /* "" */) == 0) {
        const rx_addr sp1 = rx_sp() - 12;                       /* push ebx / ebp / esi */
        SU32(sp1, -4, 0x009c4f18 /* "Default" */);
        rx_set_sp(sp1 - 8);
        SU32(sp1 - 8, 0, 0x005dfe2d);
        result = rx_ui_language_find(0x009c4f18);
        rx_set_sp(E0);
        loop_leave_cs(0x00af695c);
    } else {
        const uint32_t idx = U32(0, 0x00a94074);
        const int32_t bytes = (int32_t)(U32(0, 0x00af6954) - U32(0, 0x00af6950));
        if (!(idx < (uint32_t)(bytes / 0x1c))) ui_invalid_parameter();
        const rx_addr entry = U32(0, 0x00af6950) + 0x1c * idx;
        const rx_addr suffix = U32(entry, 0x18) < 0x10 ? entry + 4 : U32(entry, 4);
        const rx_addr sp1 = E0 - 16;                            /* push ebx / ebp / esi / edi */
        SU32(sp1, -4, suffix); SU32(sp1, -8, name); SU32(sp1, -12, 0x0093a024 /* "%s.%s" */); SU32(sp1, -16, 0x005dfd40);
        rx_set_sp(sp1 - 16);
        rx_format_buffer(buf, 0x104, 0x0093a024, sp1 - 8);
        rx_set_sp(sp1);
        result = ui_lang_search(buf);
        if (result == 0) {
            SU32(sp1, -4, name); SU32(sp1, -8, 0x009419e4 /* "%s" */); SU32(sp1, -12, 0x005dfd9f);
            rx_set_sp(sp1 - 12);
            rx_format_buffer(buf, 0x104, 0x009419e4, sp1 - 4);
            rx_set_sp(sp1);
            result = ui_lang_search(buf);
        }
        rx_set_sp(E0);
        loop_leave_cs(0x00af695c);
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x005dfc90, rx_ui_language_find)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* 00592ef0 (EAX = event channel; stdcall key ptr; RET 4): the listener whose vf4 (0x10) accepts the key leaves the channel's listener
 * list (the std::list at channel + 8: +0 container proxy, +0x14 head): under the critical section +0x24, the first match is erased
 * (00705640 with EDI = the list) and released: vf5 (0x14) while a dispatch is running (the busy count +0x3c >= 1: the object
 * outlives the call), else deleted (vf0(1)). The checked iterators' tests call _invalid_parameter_noinfo.
 * Frame: push ebp; and esp, -8; sub esp, 0x14; push ebx / esi / edi (+0x14 the critical section, +0x10 the list, +0x18 the proxy
 * copy, which the erase's out iterator reuses, +0x1c the head copy). */
void rx_channel_remove_listener(rx_addr ch, rx_addr key) {
    RACE_FRAME_EBP(F, 8, 0x14, 3);
    const rx_addr cs = ch + 0x24, list = ch + 8;
    SU32(F, 0x14, cs);
    loop_enter_cs(cs);
    const int32_t busy = I32(ch, 0x3c);                         /* lock cmpxchg [edx], eax | 0: the value, unchanged */
    const rx_addr proxy = U32(ch, 8);                           /* edi */
    SU32(F, 0x10, list);
    const rx_addr head = U32(list, 0x14);
    rx_addr node = U32(head, 0);                                /* ebx */
    SU32(F, 0x1c, head); SU32(F, 0x18, proxy);
    for (;;) {
        if (proxy == 0 || proxy != U32(F, 0x18)) crt_invalid_parameter_noinfo();
        if (node == U32(F, 0x1c)) break;
        rx_addr cont;
        if (proxy != 0) cont = U32(proxy, 0); else { crt_invalid_parameter_noinfo(); cont = 0; }
        if (node == U32(cont, 0x14)) crt_invalid_parameter_noinfo();
        const rx_addr lst = U32(node, 8);
        const uint32_t kp = key;
        const uint32_t match = rx_vcall(lst, 0x10, 1, &kp) & 0xff;
        if (match != 0) {
            RACE_CORE_CALL(3, rx_pending_list_erase(list, F + 0x18, proxy, node));
            if (busy >= 1) rx_vcall(lst, 0x14, 0, NULL);
            else { const uint32_t one = 1; rx_vcall(lst, 0, 1, &one); }
            break;
        }
        if (proxy != 0) cont = U32(proxy, 0); else { crt_invalid_parameter_noinfo(); cont = 0; }
        if (node == U32(cont, 0x14)) crt_invalid_parameter_noinfo();
        node = U32(node, 0);
    }
    loop_leave_cs(U32(F, 0x14));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00592ef0, rx_channel_remove_listener)

/* 006cde10 (stdcall hook; RET 4): the stunt scoring listener of "VehicleEntered" (0x935e84) leaves the vehicle trigger channel
 * (the registry 0xa89bc8's channel 00593220): the key {matcher vtable 0x960b64, 0, the payload type's name (0x9cbb7c, type_info
 * node 0x9e1a7c), the hook, the handler 0x6cc230} on the frame at E, E = the aligned frame - 0x14. */
void rx_stunt_scoring_unlisten(rx_addr hook) {
    RACE_FRAME_EBP(F, 8, 0x14, 1);                              /* push esi */
    const rx_addr E = F + 4;
    rx_addr ch;
    SU32(F, -4, 0x00935e84); SU32(F, -8, U32(0, 0x00a89bc8));
    RACE_CORE_CALL(2, ch = rx_world_registry_find(U32(0, 0x00a89bc8), 0x00935e84));
    SU32(E, 4, 0);
    SU32(E, 0, 0x00960b64);
    SU32(E, 0xc, hook);
    SU32(E, 0x10, 0x006cc230);
    const rx_addr name = crt_type_info_name(0x009cbb7c, 0x009e1a7c);
    SU32(E, 8, name);
    RACE_CORE_CALL(1, rx_channel_remove_listener(ch, E));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x006cde10, rx_stunt_scoring_unlisten)

/* 00587630 (ESI = manager, ECX = player, EDX = driver, stack activity; RET 4): the "PlayerLeft" event (0x941bb8) with the payload
 * void(Activity*, MXPlayer*, BaseDriver*) (type_info 0x9cf5d4), raised like 0053a6d0 (race_event.c). */
void rx_event_activity_player_driver(rx_addr manager, rx_addr activity, rx_addr player, rx_addr driver) {
    uint32_t p[3] = {activity, player, driver};
    race_raise_event(manager, 0x00941bb8, 0x009cf5d4, p, 3);
}
RX_FUNCTION(0x00587630, rx_event_activity_player_driver)

/* 0054db00 (EAX = player; plain ret): the player's session hooks released: the object at +0x9a8 told (vf29, 0x74) and cleared; a
 * rider (+0x1b4) has its stunt animation reset (0061c5f0: EAX = the rider) and the record at rider +0x5b54c cleared (+0x1c = 0,
 * the floats +0x14 / +0x24 / +0x28 = 0); a local human (+4 != 0, +0x10 == 0) with the scoring listener +0xc70 (006cde10) drops it. */
void rx_player_release_session(rx_addr player) {
    RACE_FRAME_PLAIN(F, 0, 1);                                  /* push esi */
    if (U32(player, 0x9a8) != 0) { rx_vcall(U32(player, 0x9a8), 0x74, 0, NULL); }
    const rx_addr rider = U32(player, 0x1b4);
    SU32(player, 0x9a8, 0);
    if (rider != 0) {
        RACE_CORE_CALL(0, rx_rider_animation_reset_stunt(rider));
        const rx_addr rec = U32(player, 0x1b4) + 0x5b54c;
        SU32(rec, 0x1c, 0);
        SF32(rec, 0x14, 0.0f); SF32(rec, 0x24, 0.0f); SF32(rec, 0x28, 0.0f);
    }
    if (U32(player, 4) != 0 && U32(player, 0x10) == 0) {
        const rx_addr hook = player + 0xc70;
        if (U32(hook, 0) != 0) {
            RACE_CORE_CALL(1, rx_stunt_scoring_unlisten(hook));
            SU32(hook, 0, 0);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0054db00, rx_player_release_session)

/* A controller's mapping block `off` (vf9 of the embedded key map, (pad, 1)): a network player (+0x11c8) instead keeps the wanted
 * state as bit `bit` of +0x11cc. */
static void left_block(rx_addr player, uint32_t bit, uint32_t off) {
    const int32_t pad = I32(player, 0x1f0);
    if (pad < 0) return;
    if (U32(player, 0x11c8) != 0) { SU32(player, 0x11cc, U32(player, 0x11cc) | bit); return; }
    const rx_addr obj = U32(0, 0x009c52c4 + 4u * (uint32_t)pad) + off;
    const uint32_t a[2] = {(uint32_t)pad, 1};
    rx_vcall(obj, 0x24, 2, a);
}
/* the pad getters' device (00548f20 / 00548f70 with the pad in EAX, 00548fc0 / 00549050 in ESI): vf9 (pad, 1) of it */
static void left_pad_device(rx_addr dev, int32_t pad) { const uint32_t a[2] = {(uint32_t)pad, 1}; rx_vcall(dev, 0x24, 2, a); }

/* 0055c1d0 Activity_OnPlayerLeft (stdcall: activity, driver; RET 8): the driver's player leaves the activity: its pad's key maps
 * (the mapping getters of 00548f20 / 70 / c0 / 9050, the control blocks 0x9c52c4[pad] + 0x348 / 0x9ec / 0x127c / 0x167c / 0x748 /
 * 0x147c / 0xf34 and the two player groups 00548bf0 / 00548c40) are re-enabled (a network player: its bits 1 .. 0x400 of +0x11cc
 * set instead); the vehicle's controller (+0x1ac -> +0x234, vf22) told; the rider's fade (006237a0 sel 5 = 0.25 s when its state
 * +0xaee0 is 1) and motion outputs (006215e0) reset, +0xaee0 = 0; the session hooks released (0054db00); the player's entity
 * (+0x9a4: removed from its scene 007d3040, vf9, deleted); the driver told (vf2); the driver's bookkeeping (+0x144 -> +0x28 count,
 * the player's +0x1dc = -1, the activity's slot +0x304[driver +0x148] = 0); the activity's last human told (vf51) and MXRecords_Leave
 * for a local human; the "PlayerLeft" events (00587630 on the activity's manager, 0053a6d0 on the global one); the player's
 * +0x94 = 0, +0x9c = the activity's +0x23c; the driver leaves the lists +0x268 (count +0x26c) and +0x27c (+0x280) and, for a
 * player not in a network game (+0x908 == 0), the 12-slot player table +0x2d0 is rebuilt from the list; the driver deleted (vf0(1)).
 * Frame: push ecx / ebx / ebp / esi / edi. */
void rx_activity_on_player_left(rx_addr act, rx_addr drv) {
    RACE_FRAME_PLAIN(F, 0, 5);
    const rx_addr player = U32(drv, 0x74);                      /* ebx */
    const int32_t ctl = I32(player, 0x1f0);                     /* esi */
    if (ctl >= 0) {
        if (U32(player, 0x11c8) == 0) {
            rx_addr dev;
            RACE_CORE_CALL(0, dev = rx_world_pad_steer_map((uint32_t)ctl)); left_pad_device(dev, ctl);
            RACE_CORE_CALL(0, dev = rx_world_pad_lean_map((uint32_t)ctl)); left_pad_device(dev, ctl);
        } else SU32(player, 0x11cc, U32(player, 0x11cc) | 1);
        left_block(player, 2, 0x348);
        if (I32(player, 0x1f0) >= 0) {
            if (U32(player, 0x11c8) == 0) { RACE_CORE_CALL(0, rx_player_slot_group2((uint32_t)I32(player, 0x1f0), 1)); }
            else SU32(player, 0x11cc, U32(player, 0x11cc) | 4);
        }
        if (I32(player, 0x1f0) >= 0) {
            if (U32(player, 0x11c8) == 0) {
                rx_addr dev;
                RACE_CORE_CALL(0, dev = rx_world_pad_wreck_steer_map((uint32_t)ctl)); left_pad_device(dev, ctl);
                RACE_CORE_CALL(0, dev = rx_world_pad_wreck_lean_map((uint32_t)ctl)); left_pad_device(dev, ctl);
            } else SU32(player, 0x11cc, U32(player, 0x11cc) | 8);
        }
        left_block(player, 0x10, 0x9ec);
        if (I32(player, 0x1f0) >= 0) {
            if (U32(player, 0x11c8) == 0) { RACE_CORE_CALL(0, rx_player_slot_group5((uint32_t)ctl, 1)); }
            else SU32(player, 0x11cc, U32(player, 0x11cc) | 0x20);
        }
        left_block(player, 0x40, 0x127c);
        left_block(player, 0x80, 0x167c);
        left_block(player, 0x100, 0x748);
        left_block(player, 0x200, 0x147c);
        left_block(player, 0x400, 0xf34);
    }
    if (U32(player, 0x1ac) != 0) rx_vcall(U32(U32(player, 0x1ac), 0x234), 0x58, 0, NULL);
    const rx_addr rider = U32(U32(drv, 0x74), 0x1b4);
    if (rider != 0 && U32(rider, 0xaf08) == 0) {
        if (U32(rider, 0xaee0) == 1) RACE_CORE_CALL(1, rx_world_rider_chains_fade_in(rider, 5, 0, 0.25f /* 0x92ff84 */));
        RACE_CORE_CALL(0, rx_rider_animation_set_motion_outputs(rider));
        SU32(rider, 0xaee0, 0);
    }
    RACE_CORE_CALL(0, rx_player_release_session(U32(drv, 0x74)));
    const rx_addr ent = U32(player, 0x9a4);
    if (ent != 0) {
        RACE_CORE_CALL(1, rx_scene_manager_remove_entity(ent, U32(ent, 0x10)));
        rx_vcall(ent, 0x24, 0, NULL);
        if (U32(player, 0x9a4) != 0) { const uint32_t one = 1; rx_vcall(U32(player, 0x9a4), 0, 1, &one); }
        SU32(player, 0x9a4, 0);
    }
    rx_vcall(drv, 8, 0, NULL);
    SU32(U32(drv, 0x144), 0x28, U32(U32(drv, 0x144), 0x28) - 1);
    SU32(U32(drv, 0x74), 0x1dc, 0xffffffffu);
    rx_raceidx_set(act, U32(drv, 0x148), 0);   /* (w-rc-core) indices 12..31 in side storage */
    uint32_t humans;
    RACE_CORE_CALL(0, humans = rx_activity_count_humans_of(act, 1));
    if (humans == 1 && U32(U32(drv, 0x74), 4) != 0 && U32(U32(drv, 0x74), 0x10) == 0) rx_vcall(act, 0xcc, 0, NULL);
    {
        const rx_addr p = U32(drv, 0x74);
        if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && U32(act, 0x238) != 4 && U32(0, 0x00a946c0) != 0) {
            RACE_CORE_CALL(2, rx_mxrecords_leave(U32(0, 0x00a946c0), U32(p, 0x1f0)));
        }
    }
    RACE_CORE_CALL(1, rx_event_activity_player_driver(U32(act, 0x290), act, U32(drv, 0x74), drv));
    RACE_CORE_CALL(1, rx_event_activity_driver(U32(0, 0x00a93f74), 0x00941bb8, act, drv));
    SU32(U32(drv, 0x74), 0x94, 0);
    SU32(U32(drv, 0x74), 0x9c, U32(act, 0x23c));
    for (int part = 0; part < 2; part++) {                      /* the driver leaves the lists +0x268 / +0x27c */
        const uint32_t list = part == 0 ? 0x268 : 0x27c, cnt = part == 0 ? 0x26c : 0x280;
        const int32_t n = I32(act, cnt);
        int32_t i = 0;
        if (n > 0) {
            const rx_addr d = U32(act, list);
            for (; i < n; i++) if (U32(d, 4u * (uint32_t)i) == drv) break;
            if (i < n && i >= 0) { SU32(d, 4u * (uint32_t)i, U32(d, 4u * (uint32_t)n - 4u)); SU32(act, cnt, U32(act, cnt) - 1); }
        }
    }
    if (U32(U32(drv, 0x74), 0x908) == 0) {
        uint32_t i = 0;
        int more = 1;
        if (I32(act, 0x26c) > 0) {
            do {
                rx_stand_set(act, i, U32(U32(act, 0x268), 4 * i));   /* (w-rc-core) standings 12..31 in side storage */
                i++;
            } while ((int32_t)i < I32(act, 0x26c));
            if (i >= 0xc) more = 0;
        }
        if (more) for (uint32_t k = 0; k < 0xc - i; k++) SU32(act, 0x2d0 + 4 * (i + k), 0);
        for (uint32_t k = i > RX_RC_ORIG ? i : RX_RC_ORIG; k < RX_RC_MAX; k++) rx_stand_set(act, k, 0);   /* (w-rc-core) the side entries past the drivers */
    }
    { const uint32_t one = 1; rx_vcall(drv, 0, 1, &one); }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055c1d0, rx_activity_on_player_left)


/* ---------------------------------------------------------------------------------------------------------------------------- */
/* The activity's state-name map (std::map<int, std::string> at activity + 0x240; node: +0xc the key, +0x10 the string, colour
 * +0x2c, isnil +0x2d; max_size 0x7fffffe): _Buynode 00586b60 (the node 0x30 bytes, ReflexAlignedAlloc; links head / where / head,
 * the pair copied: the key and the string copy-constructed), _Insert 005864d0, insert(value) 005862f0 and insert(hint, value)
 * 00585e90, over the generic tree code (build_vehicle_tree.c). The out iterators are {container proxy (the map's +0), node}. */
static int imap_less(rx_addr a, rx_addr b) { return I32(a, 0) < I32(b, 0); }
static rx_addr imap_make(rx_addr head, rx_addr where, rx_addr val) {
    const rx_addr n = rx_reflex_aligned_alloc(0x30);
    if (n) {
        SU32(n, 0, head); SU32(n, 4, where); SU32(n, 8, head);
        SU32(n, 0xc, U32(val, 0));
        crt_str_ctor_copy(n + 0x10, val + 4);
        SU8(n, 0x2c, 0); SU8(n, 0x2d, 0);
    }
    return n;
}
static const vt_tree k_state_map = {0x2c, 0x7fffffeu, imap_less, imap_make};

/* 005864d0 (EDI = map, EAX = where; stdcall out, add_left (a byte), value*; RET 0xc) -> EAX = out: _Insert: the new node linked
 * under `where`, the tree rebalanced; out = {proxy, node}. */
rx_addr rx_state_map_insert_at(rx_addr map, rx_addr where, rx_addr out, uint32_t add_left, rx_addr val) {
    const rx_addr node = vt_insert_at(&k_state_map, map, where, add_left & 0xff, val);
    SU32(out, 4, node);
    SU32(out, 0, U32(map, 0));
    return out;
}
RX_FUNCTION(0x005864d0, rx_state_map_insert_at)

/* 005862f0 (stdcall map, out, value*; RET 0xc) -> EAX = out {proxy, node, inserted}: insert(value): the descent by key, the
 * predecessor test; an existing key returns its node with inserted = 0. */
rx_addr rx_state_map_insert_unique(rx_addr map, rx_addr out, rx_addr val) {
    const uint32_t size0 = U32(map, 0x1c);
    const rx_addr node = vt_insert_unique(&k_state_map, map, val);
    const uint32_t inserted = U32(map, 0x1c) != size0;
    SU32(out, 4, node);
    SU32(out, 0, U32(map, 0));
    SU8(out, 8, (uint8_t)inserted);
    return out;
}
RX_FUNCTION(0x005862f0, rx_state_map_insert_unique)

/* 00585e90 (stdcall map, out, value*, hint container, hint node; RET 0x14) -> EAX = out {proxy, node}: insert(hint, value): an empty
 * map inserts at the head; the hint at the leftmost / the end / between neighbours (the checked iterators call
 * _invalid_parameter_noinfo when the hint's container is not the map's) tries the neighbouring position and otherwise falls back to
 * insert(value). */
rx_addr rx_state_map_insert_hint(rx_addr map, rx_addr out, rx_addr val, rx_addr hint_cont, rx_addr hint_node) {
    if (U32(map, 0x1c) != 0) {
        const rx_addr head = U32(map, 0x18);
        if (hint_cont == 0 || hint_cont != U32(map, 0)) crt_invalid_parameter_noinfo();
        if (hint_node != U32(head, 0)) {
            if (hint_cont == 0 || hint_cont != U32(map, 0)) crt_invalid_parameter_noinfo();
        }
    }
    const rx_addr node = vt_insert_hint(&k_state_map, map, val, hint_node);
    SU32(out, 0, U32(map, 0));
    SU32(out, 4, node);
    return out;
}
RX_FUNCTION(0x00585e90, rx_state_map_insert_hint)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* Player List table access as the game's code inlines it (the table 0xae0e08: +0x10 / +0x14 its column vector, +4 the vector's
 * checked-iterator proxy; a column's +0x40 its data vector {+0xc first, +0x10 last}). Every access runs the checked iterators'
 * tests (_invalid_parameter_noinfo) and reads the cell between EnterCriticalSection / LeaveCriticalSection of 0xae1c44. */
#define PLAYER_LIST 0x00ae0e08u
#define TBL_CS 0x00ae1c44u
static void tbl_invalid(void) { crt_invalid_parameter_noinfo(); }
/* the cell `row` of column `name`: Table_ColumnIndex, the column vector's bound, the data vector's bound; the address of the cell */
static rx_addr tbl_cell_addr(rx_addr table, rx_addr name, uint32_t row) {
    const uint32_t ci = rx_tbl_column_index(table, name);
    if (ci >= (uint32_t)((int32_t)(U32(table, 0x14) - U32(table, 0x10)) >> 2)) tbl_invalid();
    const rx_addr col = U32(U32(table, 0x10), 4u * ci);
    loop_enter_cs(TBL_CS);
    const rx_addr vec = U32(col, 0x40);
    if (row >= (uint32_t)((int32_t)(U32(vec, 0x10) - U32(vec, 0xc)) >> 2)) tbl_invalid();
    const rx_addr cell = U32(vec, 0xc) + 4u * row;
    loop_leave_cs(TBL_CS);
    return cell;
}
/* the inlined row count of a table (EAX = its first column: 004eb830) behind the iterator checks; 0 without columns */
static uint32_t tbl_rows_checked(rx_addr table) {
    const rx_addr begin = U32(table, 0x10), end = U32(table, 0x14);
    if (((int32_t)(end - begin) >> 2) == 0) return 0;
    if (begin > end) tbl_invalid();
    rx_addr owner;
    if (U32(table, 4) == 0) { tbl_invalid(); owner = 0; } else owner = U32(U32(table, 4), 0);
    if (!(begin < U32(owner, 0x10))) tbl_invalid();
    return rx_tbl_column_rows(U32(begin, 0));
}

/* 004de700 (ESI = row; plain ret): the Player List row's "Disconnected" cell (0x93dee8) set to 1 (the table's change counter +0x68
 * counts a changed value), and the table's dirty flag 0xae0e94 set. */
void rx_player_row_set_disconnected(uint32_t row) {
    RACE_FRAME_PLAIN(F, 8, 1);                                   /* sub esp, 8; push edi (the value at +4) */
    SU32(F, 4, 1);
    const uint32_t ci = rx_tbl_column_index(PLAYER_LIST, 0x0093dee8 /* "Disconnected" */);
    if (U32(rx_tbl_cell_word(PLAYER_LIST, ci, row), 0) != 1) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
    if (ci >= (uint32_t)((int32_t)(U32(PLAYER_LIST, 0x14) - U32(PLAYER_LIST, 0x10)) >> 2)) tbl_invalid();
    rx_tbl_set_int(U32(U32(PLAYER_LIST, 0x10), 4u * ci), row, F + 4);
    SU32(0, 0x00ae0e94, 1);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x004de700, rx_player_row_set_disconnected)

/* 00553a50 (stdcall players = the players block (activity manager's), RET 4): the pending disconnects: for each of the +0xe00c
 * unique ids at +0xdfdc, the first Player List row with that UniqueId is marked Disconnected (004de700) when it is not a local
 * player (PlayerLocal == 0) or is a human (PlayerType == 1); then the count +0xe00c is cleared.
 * Frame: sub esp, 8; push ebx / ebp / esi / edi (the list pointer at +0x10, the index at +0x14). */
void rx_players_mark_disconnected(rx_addr players) {
    RACE_FRAME_PLAIN(F, 8, 4);
    SU32(F, 0x14, 0);
    if (U32(players, 0xe00c) == 0) { SU32(players, 0xe00c, 0); RACE_FRAME_END(F); return; }
    SU32(F, 0x10, players + 0xdfdc);
    uint32_t k = 0;
    do {
        uint32_t row = 0;
        for (;;) {
            if (((int32_t)(U32(PLAYER_LIST, 0x14) - U32(PLAYER_LIST, 0x10)) >> 2) == 0) break;
            if (!(row < tbl_rows_checked(PLAYER_LIST))) break;
            const uint32_t uid = U32(tbl_cell_addr(PLAYER_LIST, 0x0093df0c /* "UniqueId" */, row), 0);
            if (uid == U32(U32(F, 0x10), 0)) {
                const uint32_t local = U32(tbl_cell_addr(PLAYER_LIST, 0x0093de90 /* "PlayerLocal" */, row), 0);
                int mark = local == 0;
                if (!mark) mark = U32(tbl_cell_addr(PLAYER_LIST, 0x00936ef4 /* "PlayerType" */, row), 0) == 1;
                if (mark) { RACE_CORE_CALL(0, rx_player_row_set_disconnected(row)); break; }
            }
            row++;
        }
        SU32(F, 0x10, U32(F, 0x10) + 4);
        k++;
        SU32(F, 0x14, k);
    } while (k < U32(players, 0xe00c));
    SU32(players, 0xe00c, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00553a50, rx_players_mark_disconnected)

/* the cell of a table's column by name at a row: the lookups of the inlined tbl_cell_addr, the value read after the lock is left */
static uint32_t tbl_cell_value(rx_addr table, rx_addr name, uint32_t row) { return U32(tbl_cell_addr(table, name, row), 0); }
/* The rows of `table` (from the last to the first) whose int column `name` equals `value` are erased (004ece50): the loops over the
 * network vehicle / rider tables. */
static void tbl_erase_matching(rx_addr table, rx_addr name, uint32_t value) {
    int32_t row = (int32_t)tbl_rows_checked(table) - 1;
    for (; row >= 0; row--) {
        if (tbl_cell_value(table, name, (uint32_t)row) == value) {
            RACE_CORE_CALL(2, rx_tbl_erase_row(table, (uint32_t)row));
        }
    }
}
/* the Vehicle List / Rider List row of the player's id and, when its owner is a network client (5), the network proxy rows of that
 * owner's id in the two network tables */
static void left_remove_network(rx_addr list_table, rx_addr id_name, uint32_t id, rx_addr owner_name, rx_addr owner_id_name,
                                rx_addr net_name, rx_addr t1, rx_addr t2, int rider) {
    uint32_t key = id;
    const uint32_t ci = rx_tbl_column_index(list_table, id_name);
    if (ci >= (uint32_t)((int32_t)(U32(list_table, 0x14) - U32(list_table, 0x10)) >> 2)) tbl_invalid();
    const rx_addr ref = rx_tbl_row(U32(U32(list_table, 0x10), 4u * ci), (int32_t)key);   /* 00418ec0 */
    const rx_addr t = U32(ref, 8);
    if (t == 0 || U32(ref, 0) == 0xffffffffu) return;
    const uint32_t row = U32(ref, 0);
    if (tbl_cell_value(t, owner_name, row) != 5) return;
    uint32_t value;
    if (!rider) {
        value = U32(rx_tbl_cell_word(t, rx_tbl_column_index(t, owner_id_name), row), 0);   /* OwnerVehicleId (0041aaf0) */
    } else {
        value = tbl_cell_value(t, owner_id_name, row);                                      /* OwnerRiderId */
    }
    tbl_erase_matching(t1, net_name, value);
    tbl_erase_matching(t2, net_name, value);
}

/* 00553c60 (stdcall players = the players block, RET 4): the pending removals of players (the unique ids at +0xe010, count +0xe040):
 * for each id, the Player List rows (last to first) with that UniqueId and that are not local or are human (PlayerType 1) are
 * handled once: the Vehicle List row of the player's VehicleId and, when its owner is a network client (VehicleOwner 5), the
 * network vehicle rows (NetworkVehicleId == the owner's OwnerVehicleId) of the two tables 0xae0550 / 0xae0898 erased; the same for
 * the RiderId (Rider List 0xae19b0, RiderOwner, OwnerRiderId, NetworkRiderId of 0xae1a28 / 0xae0f10); then the player's row erased
 * (004ece50). Afterwards +0xe040 = 0 and the game's activity area (+0x15b54): set up now (0075c430, ESI = game) when it exists,
 * else +0xe044 = 1 (the deferred set-up of 00552930).
 * Frame: sub esp, 0x10; push ebx / ebp / esi / edi. */
void rx_players_remove_left(rx_addr players) {
    RACE_FRAME_PLAIN(F, 0x10, 4);
    SU32(F, 0x14, 0);
    if (U32(players, 0xe040) != 0) {
        uint32_t k = 0;
        do {
            int32_t row = (int32_t)tbl_rows_checked(PLAYER_LIST) - 1;
            for (; row >= 0; row--) {
                if (tbl_cell_value(PLAYER_LIST, 0x0093df0c /* "UniqueId" */, (uint32_t)row) != rx_seat_leave(players, k)) continue;   /* (w-rc-core) 12.. in side storage */
                if (tbl_cell_value(PLAYER_LIST, 0x0093de90 /* "PlayerLocal" */, (uint32_t)row) != 0 &&
                    tbl_cell_value(PLAYER_LIST, 0x00936ef4 /* "PlayerType" */, (uint32_t)row) != 1) continue;
                SU32(F, 0x14, (uint32_t)row);
                const uint32_t vid = tbl_cell_value(PLAYER_LIST, 0x00936cb4 /* "VehicleId" */, (uint32_t)row);
                left_remove_network(0x00ae05c8, 0x00936cb4, vid, 0x0093df84 /* "VehicleOwner" */, 0x0093df94 /* "OwnerVehicleId" */,
                                    0x0093e288 /* "NetworkVehicleId" */, 0x00ae0550, 0x00ae0898, 0);
                const uint32_t rid = tbl_cell_value(PLAYER_LIST, 0x0093dec8 /* "RiderId" */, (uint32_t)row);
                left_remove_network(0x00ae19b0, 0x0093dec8, rid, 0x0093dfa4 /* "RiderOwner" */, 0x0093dfb0 /* "OwnerRiderId" */,
                                    0x0093e23c /* "NetworkRiderId" */, 0x00ae1a28, 0x00ae0f10, 1);
                RACE_CORE_CALL(2, rx_tbl_erase_row(PLAYER_LIST, (uint32_t)row));
                break;
            }
            k++;
        } while (k < U32(players, 0xe040));
    }
    const rx_addr game = U32(0, 0x00a946b8);
    SU32(players, 0xe040, 0);
    if (U32(game, 0x15b54) == 0) SU32(players, 0xe044, 1);
    else { RACE_CORE_CALL(0, rx_game_setup_activity_area(game)); }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00553c60, rx_players_remove_left)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* 00552c60 (EAX = a player, ESI = the players block (game + 0x7460); plain ret): the player's slot released: its index = (player -
 * block) / 0x12a0 (the compiler's multiply by 0x0dbeb61f); when the slot is in use (flag byte +0xdf80 + index) the index leaves the
 * 12-entry order list at +0xdf8c (the last entry, count +0xdfbc, moving into its place and set to -1; nothing moves when the index
 * is not in it), the flag is cleared, the player object is destroyed (vf0 with 0: no delete) and the count drops by one. */
void rx_player_slot_free(rx_addr players, rx_addr player) {
    const int32_t diff = (int32_t)(player - players);
    const int64_t prod = (int64_t)diff * 0x0dbeb61f;
    int32_t hi = (int32_t)(prod >> 32) >> 8;
    uint32_t idx = (uint32_t)hi + ((uint32_t)hi >> 31);
    if (rx_seat_of(players, player) >= (int32_t)RX_RC_ORIG) idx = (uint32_t)rx_seat_of(players, player);   /* (w-rc-core) a side seat */
    if (rx_seat_used(players, idx) != 0) {
        uint32_t k = 0;
        for (; k < RX_RC_MAX; k++) if (rx_seat_order(players, k) == idx) break;   /* (w-rc-core) 0xc in the original */
        if (k < RX_RC_MAX) {
            const uint32_t count = U8(players, 0xdfbc);
            rx_seat_set_order(players, k, rx_seat_order(players, count - 1));
            rx_seat_set_order(players, U8(players, 0xdfbc) - 1, 0xffffffffu);
        }
        rx_seat_set_used(players, idx, 0);
        const rx_addr obj = rx_seat(players, idx);
        const uint32_t zero = 0;
        rx_vcall(obj, 0, 1, &zero);
        SU8(players, 0xdfbc, U8(players, 0xdfbc) - 1);
    }
}
RX_FUNCTION(0x00552c60, rx_player_slot_free)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* The player attribute table 0xae0b08 (columns PlayerUIDHigh / PlayerUIDLow / Attribute / Value; the rows the online metrics upload
 * reads, 00753c20): rows built from criteria like every table helper (00419a70 criterion, 004175e0 push, the inlined destructor). */
static void crit_int_push(rx_addr vec, rx_addr crit, rx_addr name, uint32_t value, rx_addr value_slot) {
    SU32(value_slot, 0, value);
    rx_tbl_crit_init_int(crit, name, 0, value);                  /* 00419a70 (ESI = crit, EAX = op, ECX = &value; name) */
    rx_tbl_crit_push(vec, crit);                                 /* 004175e0 (EDI = vec) */
    rx_tbl_crit_destroy(crit);
}
/* 004e21d0 (stdcall a0, a1; RET 8): the rows of the player (PlayerUIDHigh = a1, PlayerUIDLow = a0: the original reads its second
 * argument first) deleted (004ecf00 on the table 0xae0b08). Frame: sub esp, 0xd0; push ebx / esi / edi (the criteria vector at
 * +0x10, the value slot +0xc, the criterion +0x28). */
void rx_player_attrs_delete(uint32_t a0, uint32_t a1) {
    RACE_FRAME_PLAIN(F, 0xd0, 3);
    const rx_addr vec = F + 0x10, crit = F + 0x28;
    rx_tbl_crit_vec_init(vec);
    crit_int_push(vec, crit, 0x0093e150 /* "PlayerUIDHigh" */, a1, F + 0xc);
    crit_int_push(vec, crit, 0x0093e160 /* "PlayerUIDLow" */, a0, F + 0xc);
#ifdef RX_ORACLE
    /* rx_tbl_delete_rows (004ecf00) stops at the row-set paths the reference boot never executed (004ed07e matching rows, 004ebf9a an
     * empty intersection): the oracle verifies this function against the original with the original delete in both runs; the product
     * calls the core's */
    { const uint32_t da[2] = {0x00ae0b08, vec}; rx_call_original(0x004ecf00, RX_STDCALL, 0, 2, da); }
#else
    RACE_CORE_CALL(2, rx_tbl_delete_rows(0x00ae0b08, vec));      /* 004ecf00 */
#endif
    rx_tbl_crit_vec_destroy(vec);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x004e21d0, rx_player_attrs_delete)

/* 004e2340 (stdcall a0, a1, attribute, value; RET 0x10) -> the new row's index (rows - 1): a row with the four columns
 * (PlayerUIDHigh = a1, PlayerUIDLow = a0, Attribute 0x93e2ac, Value 0x93e2b8) added to the table 0xae0b08 (004ec950, EAX = the
 * table). Frame: sub esp, 0xe0; push ebx / esi / edi (the added row's reference at +0xdc). */
uint32_t rx_player_attr_add(uint32_t a0, uint32_t a1, uint32_t attribute, uint32_t value) {
    RACE_FRAME_PLAIN(F, 0xe0, 3);
    const rx_addr vec = F + 0x10, crit = F + 0x28, out = F + 0xdc;
    rx_tbl_crit_vec_init(vec);
    crit_int_push(vec, crit, 0x0093e150 /* "PlayerUIDHigh" */, a1, F + 0xc);
    crit_int_push(vec, crit, 0x0093e160 /* "PlayerUIDLow" */, a0, F + 0xc);
    crit_int_push(vec, crit, 0x0093e2ac /* "Attribute" */, attribute, F + 0xc);
    crit_int_push(vec, crit, 0x0093e2b8 /* "Value" */, value, F + 0xc);
    RACE_CORE_CALL(2, rx_tbl_add_row(0x00ae0b08, out, vec));     /* 004ec950 (EAX = the table; the row reference, the criteria) */
    const uint32_t rows = rx_tbl_row_count(0x00ae0b08);          /* 004021d0 */
    rx_tbl_crit_vec_destroy(vec);
    RACE_FRAME_END(F);
    return rows - 1;
}
RX_FUNCTION(0x004e2340, rx_player_attr_add)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* MXRecords (the activity's per-player record rows: records + 0x18 * slot {+0 the profile's PlayerRecords table, +4 the working row,
 * +8 the stored free-ride row, +0xc EventTypeLocationId, +0x10 VehicleTypeId, +0x14 holeshots}; Join / Reset: build_activity_records.c) */

/* the record columns of MXRecords_CopyRow 00763600 (name, float): every one copied from the source row into the destination row */
typedef struct { uint32_t name; int is_float; } copy_col;
static const copy_col k_copy_cols[] = {
    {0x00957c1c, 1}, {0x0096d990, 0}, {0x0096d99c, 0}, {0x00957c04, 0}, {0x0096d9a8, 0}, {0x0096d9b8, 0},
    {0x0096d9cc, 0}, {0x0096d9e0, 0}, {0x0096d9f0, 0}, {0x0096da08, 0}, {0x0096da1c, 0}, {0x0096da2c, 0},
    {0x0096da40, 0}, {0x0096da54, 0}, {0x0096da6c, 0}, {0x0096da80, 0}, {0x00957c28, 1}, {0x0096da98, 0},
    {0x009626b0, 0}, {0x0096daa4, 0}, {0x0096dab0, 0}, {0x0096dabc, 0}, {0x0096dacc, 0}, {0x0096dadc, 0}};
/* 00763600 MXRecords_CopyRow (stdcall &destination ref, &source ref; refs {+0 row, +4 set, +8 table}; RET 8): the 24 record columns
 * (TotalTime, TotalLaps, ... TopSpeed; the two float columns through 005063c0, the rest Table_SetCellInt) */
void rx_records_copy_row(rx_addr dst, rx_addr src) {
    RACE_FRAME_PLAIN(F, 0, 5);
    for (size_t k = 0; k < sizeof k_copy_cols / sizeof k_copy_cols[0]; k++) {
        const rx_addr st = U32(src, 8);
        const uint32_t c = rx_tbl_column_index(st, k_copy_cols[k].name);
        const rx_addr cell = rx_tbl_cell_word(st, c, U32(src, 0));
        if (k_copy_cols[k].is_float) RACE_CORE_CALL(2, rx_tbl_set_cell_float(U32(dst, 8), k_copy_cols[k].name, U32(dst, 0), cell));
        else RACE_CORE_CALL(2, rx_tbl_set_cell_int(U32(dst, 8), k_copy_cols[k].name, U32(dst, 0), cell));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00763600, rx_records_copy_row)

/* 00764560 MXRecords_Leave (stdcall records, slot; RET 8): the slot's working row becomes the stored row of its location and vehicle
 * type (EventTypeLocationId = slot +0xc, VehicleTypeId = +0x10): the stored row (found by the criteria in the profile's table
 * slot +0, or added with the working row's VehicleTypeId) takes the working row's record columns (MXRecords_CopyRow), the working
 * row is erased (004ece50) and the highlights are checked (007647f0); the slot's rows / ids are reset to -1, its holeshots to 0.
 * 007647f0 only scans the profile's record rows (read-only, into scratch arrays) and reports highlight progress to the profile /
 * Steam (MXHighlightTracking_AddProgress 006d7980): candidate typed closure runs the original-derived scan.
 * Frame: push ebp; and esp, -8; sub esp, 0x104; push ebx / esi / edi: S = A - 0x110: +0xc the found row reference, +0x18 the working
 * row reference {row, 0, table}, +0x24 / +0x3c the criteria vectors, +0x54 the added row's reference, +0x60 the criterion. */
void rx_mxrecords_leave(rx_addr records, uint32_t slot) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~7u, S = A - 0x104 - 12;
    rx_set_sp(S);
    /* PHYSICS STUDIO (port, phys_studio.h): an event that rode a config at any point (a live edit included) commits no records: the
     * working row is dropped as a MAP SCALE event never has one (its rows are deleted by the next join) */
    if (slot != 0xffffffffu && rx_ps_records_off()) SU32(records + slot * 0x18, 4, 0xffffffffu);
    if (slot != 0xffffffffu) {
        const rx_addr rs = records + slot * 0x18;                 /* ebx */
        const rx_addr out = S + 0xc, work = S + 0x18, vec1 = S + 0x24, vec2 = S + 0x3c, add = S + 0x54, crit = S + 0x60;
        if (U32(rs, 4) != 0xffffffffu) {
            SU32(work, 0, U32(rs, 4)); SU32(work, 4, 0); SU32(work, 8, U32(rs, 0));
            RACE_CORE_CALL(0, rx_list_init(vec1));                /* 005442c0 (ESI = the vector) */
            crit_int_push(vec1, crit, 0x0093e228 /* "EventTypeLocationId" */, U32(rs, 0xc), rs + 0xc);
            crit_int_push(vec1, crit, 0x00936ce4 /* "VehicleTypeId" */, U32(rs, 0x10), rs + 0x10);
            SU32(out, 0, 0xffffffffu); SU32(out, 4, 0); SU32(out, 8, 0);
            RACE_CORE_CALL(3, rx_tbl_find_first(U32(rs, 0), vec1, out));          /* 004ed410 */
            if (U32(out, 8) != 0 && U32(out, 0) != 0xffffffffu) {
                RACE_CORE_CALL(2, rx_records_copy_row(out, work));
            } else {
                RACE_CORE_CALL(0, rx_list_init(vec2));
                crit_int_push(vec2, crit, 0x0093e228, U32(rs, 0xc), rs + 0xc);
                const rx_addr t = U32(work, 8);
                const uint32_t ci = rx_tbl_column_index(t, 0x00936ce4);
                const rx_addr cell = rx_tbl_cell_word(t, ci, U32(work, 0));
                crit_int_push(vec2, crit, 0x00936ce4, U32(cell, 0), cell);
                RACE_CORE_CALL(2, rx_tbl_add_row(U32(rs, 0), add, vec2));        /* 004ec950 (EAX = the table) */
                SU32(out, 0, U32(add, 0)); SU32(out, 4, U32(add, 4)); SU32(out, 8, U32(add, 8));
                RACE_CORE_CALL(2, rx_records_copy_row(out, work));
                rx_tbl_crit_vec_destroy(vec2);
            }
            RACE_CORE_CALL(2, rx_tbl_erase_row(U32(work, 8), U32(work, 0)));      /* 004ece50 */
            RACE_CORE_CALL(2,rx_records_leave_highlights(records,slot));
            rx_tbl_crit_vec_destroy(vec1);
        }
        SU32(rs, 4, 0xffffffffu); SU32(rs, 8, 0xffffffffu); SU32(rs, 0xc, 0xffffffffu); SU32(rs, 0x14, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00764560, rx_mxrecords_leave)

/* ---------------------------------------------------------------------------------------------------------------------------- */
/* 00585330 (EDI = the free ride's discovery tracker {+0 count, +4 the entities (each +0x140 found flag, +0xa2 byte, +0x70; vf17 =
 * position), +0x84 the number found}; stdcall activity, player; RET 8): for a player with a vehicle (+0x1ac): every discovery
 * entity not yet found whose position is within the squared distance 0x9ca32c of the vehicle, or within 0x9ca330 of the point
 * 3.0 (0x92fe48) above it (the y difference measured from there), is found: the flags set (+0x140 = 1, +0xa2 = 0, +0x70 = 0), the
 * found count +0x84 incremented and the medal of the new count among the thirds of the total (0: below a third .. 2: the last, 3:
 * past it) awarded with 0054e670 (player, 7, medal, (float)count), the activity's "HitDiscoveryFound" event (0x942898: 0058d7d0 with
 * ECX = found, EDX = total) raised on its manager (+0x290) and the collectable recorded (0054d590 (player, index)). 0054e670 (race_challenge.c) /
 * 0054d590 (race_activity_ui.c) are the profile's medal / collectable tables (UI properties).
 * Frame: and esp, -16; sub esp, 0x28; push ebx / esi (the vehicle position at +0x10, the entity's at +0x20, the index +8, the list
 * pointer +0xc). */
void rx_free_ride_discoveries(rx_addr tr, rx_addr act, rx_addr player) {
    RACE_FRAME_EBP(F, 16, 0x28, 2);
    const rx_addr veh = U32(player, 0x1ac);
    if (veh != 0) {
        const rx_addr pos = F + 0x10, pos2 = F + 0x20;
        { const uint32_t a[1] = {pos}; (void)rx_vcall(veh, 0x44, 1, a); }
        SU32(F, 8, 0);
        if (I32(tr, 0) > 0) {
            rx_addr p = tr + 4;
            SU32(F, 0xc, p);
            int32_t i = 0;
            do {
                const rx_addr e = U32(p, 0);
                if (U32(e, 0x140) == 0) {
                    { const uint32_t a[1] = {pos2}; (void)rx_vcall(e, 0x44, 1, a); }
                    const float py = F32(F, 0x14), qy = F32(F, 0x24);
                    float dz = F32(F, 0x18); dz = dz - F32(F, 0x28);
                    float dx = F32(F, 0x10); dx = dx - F32(F, 0x20);
                    float dy = py - qy;
                    float dy3 = py + F32(0, 0x0092fe48) - qy;
                    const float d1 = (dz * dz + dy * dy) + dx * dx;
                    int hit = F32(0, 0x009ca32c) > d1;
                    if (!hit) {
                        const float d2 = (dz * dz + dy3 * dy3) + dx * dx;
                        hit = F32(0, 0x009ca330) > d2;
                    }
                    if (hit) {
                        SU32(e, 0x140, 1); SU8(e, 0xa2, 0); SU32(e, 0x70, 0);
                        const int32_t n = I32(tr, 0), c = I32(tr, 0x84);
                        const int32_t third = (int32_t)(((int64_t)n * 0x55555556ll) >> 32);
                        const int32_t t3 = third + (int32_t)((uint32_t)third >> 31);
                        SU32(tr, 0x84, (uint32_t)(c + 1));
                        const int32_t c1 = c + 1;
                        uint32_t medal;
                        if (c < t3 && t3 <= c1) medal = 0;
                        else if (c < t3 * 2 && t3 * 2 <= c1) medal = 1;
                        else if (c < n && n <= c1) medal = 2;
                        else medal = 3;
                        const float fc = (float)c1;
                        RACE_CORE_CALL(4, rx_challenge_award(player, 7, medal, rx_fb(fc)));       /* 0054e670 (race_challenge.c) */
                        RACE_CORE_CALL(2, rx_event_player_2int(U32(act, 0x290), 0x00942898 /* "HitDiscoveryFound" */, player, U32(tr, 0x84), U32(tr, 0)));
                        RACE_CORE_CALL(2, rx_free_ride_collectable_found(player, (uint32_t)I32(F, 8)));   /* 0054d590 (race_activity_ui.c) */
                    }
                }
                p += 4; SU32(F, 0xc, p);
                i++; SU32(F, 8, (uint32_t)i);
            } while (i < I32(tr, 0));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00585330, rx_free_ride_discoveries)
