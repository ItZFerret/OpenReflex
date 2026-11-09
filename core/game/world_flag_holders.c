/* world_flag_holders.c -- the lap board's flag holders: ScoreTowerFlagHolder (+0x350 the activity's channel, +0x354 the activity, +0x358 / +0x35c / +0x368 / +0x36c the state
 * values and the two flag textures, +0x360 hidden, +0x364 the flag state, +0x370 the flag person's model instance) and the event templates they subscribe with.
 * "First Human Joined Activity" 006c18e0 hides the tower in a mode 3 activity; "Activity Init" 006c1910 (kind != 8) takes the activity and its channel and subscribes
 * "TwoLapsLeft" -> 006c1b30, "OneLapLeft" -> 005a4150 (a shared empty handler), "EndOfRace" -> 006c1b60; "Activity UnInit" 006c1990 unsubscribes them. */
#include "build_activity_util.h"
#include "world_flag_holders.h"
#include "world_misc.h"
#include "race_activity_misc.h"
#include "build_activity_int.h"
#include "race.h"
#include "phys_vec.h"

/* 006c45f0 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc): void(Activity*, BaseDriver*) handler (EventHandlerProxy vtable 0x009604f4, signature
 * 0x009cda18) */
void rx_event_subscribe_activity_driver(rx_addr ch, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    rx_event_register_proxy(ch, name, 0x009604f4, 0x009cda18, entity, cb, arg);
}
RX_FUNCTION(0x006c45f0, rx_event_subscribe_activity_driver)

/* the listener {vtable, 0, the signature's name (type_info of td), entity, callback, argument} leaves the channel of the name (00592ef0); frame: sub esp, 0x1c; push esi; the key at
 * +8, the find's arguments pushed first (name, then channel) */
static void unsubscribe_key(rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr entity, uint32_t cb, uint32_t arg) {
    ACT_FRAME(L, 0x1c, 1);
    const rx_addr K = L + 8;
    SU32(L, -4, name); SU32(L, -8, registry);
    rx_addr ch;
    ACT_CALLR(ch, 2, rx_world_registry_find(registry, name));
    SU32(K, 0xc, entity);
    SU32(K, 4, 0);
    SU32(K, 0, vtable);
    SU32(K, 0x10, cb); SU32(K, 0x14, arg);
    SU32(K, 8, crt_type_info_name(td, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(ch, K));
    ACT_END(L);
}
void rx_event_unsubscribe_activity_driver(rx_addr ch, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_key(ch, name, 0x009604f4, 0x009cda18, entity, cb, arg);
}
RX_FUNCTION(0x006c46b0, rx_event_unsubscribe_activity_driver)
void rx_event_unsubscribe_global(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_key(U32(0, 0x00a93f74), name, 0x009604d4, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c4580, rx_event_unsubscribe_global)

/* 006c18e0 (thiscall, stack activity; RET 4): a mode 3 activity (+0x238): the tower hidden (+0x360 = 1, +0xa2 = 0) */
void rx_score_tower_first_human(rx_addr self, rx_addr act) {
    if (U32(act, 0x238) == 3) { SU32(self, 0x360, 1); SU8(self, 0xa2, 0); }
}
RX_FUNCTION(0x006c18e0, rx_score_tower_first_human)

/* 006c1910 (thiscall, stack activity; RET 4) */
void rx_score_tower_activity_init(rx_addr self, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    if (U32(act, 0x23c) != 8) {
        SU32(self, 0x354, act);
        const rx_addr ch = U32(act, 0x290);
        SU32(self, 0x350, ch);
        { const rx_addr sp = rx_sp(); SU32(sp, -12, self); SU32(sp, -8, 0x005a4150); SU32(sp, -4, 0); }
        ACT_CALL(3, rx_event_subscribe_activity_driver(ch, 0x00941c74 /* "TwoLapsLeft" */, self, 0x005a4150, 0));
        { const rx_addr sp = rx_sp(); SU32(sp, -12, self); SU32(sp, -8, 0x006c1b30); SU32(sp, -4, 0); }
        ACT_CALL(3, rx_event_subscribe_activity_driver(U32(self, 0x350), 0x00941c80 /* "OneLapLeft" */, self, 0x006c1b30, 0));
        { const rx_addr sp = rx_sp(); SU32(sp, -12, self); SU32(sp, -8, 0x006c1b60); SU32(sp, -4, 0); }
        ACT_CALL(3, rx_event_subscribe_activity_driver(U32(self, 0x350), 0x00941c9c /* "EndOfRace" */, self, 0x006c1b60, 0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c1910, rx_score_tower_activity_init)

/* 006c1990 (thiscall, stack activity; RET 4) */
void rx_score_tower_activity_uninit(rx_addr self, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    if (U32(self, 0x354) == act) {
        ACT_CALL(3, rx_event_unsubscribe_activity_driver(U32(self, 0x350), 0x00941c74, self, 0x005a4150, 0));
        ACT_CALL(3, rx_event_unsubscribe_activity_driver(U32(self, 0x350), 0x00941c80, self, 0x006c1b30, 0));
        ACT_CALL(3, rx_event_unsubscribe_activity_driver(U32(self, 0x350), 0x00941c9c, self, 0x006c1b60, 0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c1990, rx_score_tower_activity_uninit)

/* 006c1a90 (EAX = the tower, ECX = the state; a jump table 0x006c1b18: 0, 1, 2, 2): the tower's animation channel (+0x16c = +0x170 = 0, +0x174 = the clip: +0x358 for state 0,
 * +0x35c for the others, +0x184 = 0.0); states 1 / 2 / 3 then set the flag person's texture (006c1b80: 0 for state 1, 1 for 2 / 3) */
void rx_score_tower_apply_state(rx_addr t, uint32_t state) {
    SU32(t, 0x16c, 0); SU32(t, 0x170, 0);
    SU32(t, 0x174, U32(t, state == 0 ? 0x358 : 0x35c));
    SF32(t, 0x184, 0.0f);
    if (state != 0) {
        const rx_addr sp = rx_sp();
        SU32(sp, -8, t); SU32(sp, -4, state == 1 ? 0 : 1);
        ACT_CALL(2, rx_score_tower_set_flag_texture(t, state == 1 ? 0 : 1));
    }
}
RX_FUNCTION(0x006c1a90, rx_score_tower_apply_state)

/* 006c1b30 (thiscall, stack activity, driver; RET 8): flag state 2 unless it is 1 or 2 already */
void rx_score_tower_two_laps(rx_addr t, rx_addr act, rx_addr drv) {
    (void)act; (void)drv;
    const uint32_t s = U32(t, 0x364);
    if (s != 2 && s != 1) { SU32(t, 0x364, 2); rx_score_tower_apply_state(t, 2); }
}
RX_FUNCTION(0x006c1b30, rx_score_tower_two_laps)

/* 006c1b60 (thiscall, stack activity, driver; RET 8): flag state 1 unless it is 1 already */
void rx_score_tower_end_of_race(rx_addr t, rx_addr act, rx_addr drv) {
    (void)act; (void)drv;
    if (U32(t, 0x364) != 1) { SU32(t, 0x364, 1); rx_score_tower_apply_state(t, 1); }
}
RX_FUNCTION(0x006c1b60, rx_score_tower_end_of_race)

/* 006c1b80 (stdcall tower, which; RET 8): the flag person's model instance (+0x370; its asset +0x44) busy (+0x4c = 1); each node (0x1c bytes from +0x130, count +0x118) with its
 * children (16 bytes from node +0x10): the child whose name (vf2) is "FlagPersonFlag" (0x0095f3b4) is busy while its asset (+0x44) with a texture slot (+0x18) takes the
 * tower's texture for `which` (0: +0x368, 1: +0x36c) at +0x1c; a node continues with the next node after a match; +0x4c = 0 at the end */
static void set_flag_texture(rx_addr t, uint32_t which, uint32_t inst_off, uint32_t tex0_off, uint32_t tex1_off) {
    ACT_FRAME(L, 0xc, 4);
    const rx_addr inst = U32(t, inst_off);
    const rx_addr model = U32(inst, 0x44);
    SU32(inst, 0x4c, 1);
    SU32(L, 0x18, 0);
    if (U32(model, 0x118) == 0) { SU32(U32(t, inst_off), 0x4c, 0); ACT_END(L); return; }
    SU32(L, 0x10, 0);
    for (uint32_t n = 0; n < U32(model, 0x118); n++) {
        SU32(L, 0x18, n);
        const uint32_t ncount = (uint32_t)(((int32_t)(U32(model, 0x134) - U32(model, 0x130))) / 0x1c);
        if (n >= ncount) crt_invalid_parameter_noinfo();
        const rx_addr node = U32(model, 0x130) + 0x1c * n;
        const uint32_t ccount = (uint32_t)(((int32_t)(U32(node, 0x14) - U32(node, 0x10))) >> 4);
        SU32(L, 0x10, 0x1cu * n);
        for (uint32_t c = 0; c < ccount; c++) {
            SU32(L, 0x14, 0x10u * c);
            if (n >= (uint32_t)(((int32_t)(U32(model, 0x134) - U32(model, 0x130))) / 0x1c)) crt_invalid_parameter_noinfo();
            const rx_addr nn = U32(model, 0x130) + 0x1cu * n;
            if (c >= (uint32_t)(((int32_t)(U32(nn, 0x14) - U32(nn, 0x10))) >> 4)) crt_invalid_parameter_noinfo();
            const rx_addr child = U32(U32(nn, 0x10) + 0x10u * c, 0xc);
            const rx_addr name = act_vcall(child, 8, 0, 0);
            if (crt_stricmp(name, 0x0095f3b4 /* "FlagPersonFlag" */) == 0) {
                const rx_addr asset = U32(child, 0x44);
                SU32(child, 0x4c, 1);
                if (asset != 0 && U32(asset, 0x18) != 0) {
                    uint32_t tex = 0;
                    int ok = 1;
                    if (which == 0) tex = U32(t, tex0_off); else if (which == 1) tex = U32(t, tex1_off); else ok = 0;
                    if (ok && tex != 0) SU32(asset, 0x1c, tex);
                }
                SU32(child, 0x4c, 0);
                break;
            }
        }
    }
    SU32(U32(t, inst_off), 0x4c, 0);
    ACT_END(L);
}
void rx_score_tower_set_flag_texture(rx_addr t, uint32_t which) { set_flag_texture(t, which, 0x370, 0x368, 0x36c); }
RX_FUNCTION(0x006c1b80, rx_score_tower_set_flag_texture)

/* ---- TrackSideFlagHolder (+0x350 the channel, +0x354 the activity, +0x35c the clip, +0x364 {section, node, distance^2} of the nearest track node, +0x370 the flag state,
 * +0x374 the reach (the activity's +0xa6c + +0xa70), +0x378 / +0x37c the windows, +0x380 / +0x384 the flag textures, +0x388 the flag person's model instance) ---- */

/* 006c4ab0 / 006c4b70 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc): void(Activity*, RavenVehicle*) handler (vtable 0x00960574, signature 0x009cdb7c) */
void rx_event_subscribe_activity_vehicle(rx_addr ch, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    rx_event_register_proxy(ch, name, 0x00960574, 0x009cdb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4ab0, rx_event_subscribe_activity_vehicle)
void rx_event_unsubscribe_activity_vehicle(rx_addr ch, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_key(ch, name, 0x00960574, 0x009cdb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4b70, rx_event_unsubscribe_activity_vehicle)

/* 006c30f0 (stdcall holder, which; RET 8): as 006c1b80 for the holder's model (+0x388) and textures (+0x380 / +0x384) */
void rx_track_flag_set_flag_texture(rx_addr t, uint32_t which) { set_flag_texture(t, which, 0x388, 0x380, 0x384); }
RX_FUNCTION(0x006c30f0, rx_track_flag_set_flag_texture)

/* 006c2d90 (thiscall, stack activity; RET 4): "Activity Init" of a kind != 8 activity: +0x354 = the activity, +0x350 = its channel; "VehicleWrecked" -> 006c2f60 and
 * "VehicleGettingLapped" -> 006c3020 subscribed; the holder's nearest track node (00538440 over the activity's track block +0x890, the holder's position and the direction
 * row +0x20 of its node's matrix, second edge pair) into +0x364; +0x374 = +0xa70 + +0xa6c */
void rx_track_flag_activity_init(rx_addr self, rx_addr act) {
    ACT_FRAME_EBP(L, 16, 0x24, 3);
    if (U32(act, 0x23c) != 8) {
        SU32(self, 0x354, act);
        const rx_addr ch = U32(act, 0x290);
        SU32(self, 0x350, ch);
        { const rx_addr sp = rx_sp(); SU32(sp, -12, self); SU32(sp, -8, 0x006c2f60); SU32(sp, -4, 0); }
        ACT_CALL(3, rx_event_subscribe_activity_vehicle(ch, 0x0093ee38 /* "VehicleWrecked" */, self, 0x006c2f60, 0));
        { const rx_addr sp = rx_sp(); SU32(sp, -12, self); SU32(sp, -8, 0x006c3020); SU32(sp, -4, 0); }
        ACT_CALL(3, rx_event_subscribe_activity_vehicle(U32(self, 0x350), 0x00941c5c /* "VehicleGettingLapped" */, self, 0x006c3020, 0));
        const rx_addr pos = L + 0x20, dirv = L + 0x10;
        { const uint32_t a[1] = {pos}; act_vcall(self, 0x44, 1, a); }
        const rx_addr r = act_vcall(self, 0x70, 0, 0);
        const rx_addr n = U32(r, 0);
        rx_st4(dirv, rx_ld4(U32(U32(n, 0), 0x18) + ((uint32_t)U8(n, 0xb) << 6) + 0x20));
        { const rx_addr sp = rx_sp(); SU32(sp, -12, act + 0x890); SU32(sp, -8, dirv); SU32(sp, -4, 1); }
        ACT_CALL(3, rx_track_nearest_node_all(act + 0x890, dirv, 1, pos, self + 0x364));
        SF32(self, 0x374, phys_add(F32(act, 0xa70), F32(act, 0xa6c)));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c2d90, rx_track_flag_activity_init)

/* 006c2e60 (thiscall, stack activity; RET 4): the two listeners removed when it is the holder's activity */
void rx_track_flag_activity_uninit(rx_addr self, rx_addr act) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    if (U32(self, 0x354) == act) {
        ACT_CALL(3, rx_event_unsubscribe_activity_vehicle(U32(self, 0x350), 0x0093ee38, self, 0x006c2f60, 0));
        ACT_CALL(3, rx_event_unsubscribe_activity_vehicle(U32(self, 0x350), 0x00941c5c, self, 0x006c3020, 0));
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c2e60, rx_track_flag_activity_uninit)

/* the vehicle's position through its vf17 (+0x44), the nearest track node within the holder's reach (005385f0: stack track block, out {+0x? the section, the node, distance^2}, the
 * reach, 0, 0; EAX = the position) and the node (out + 4) or 0 */
static rx_addr nearest_in_reach(rx_addr self, rx_addr act, rx_addr veh, rx_addr L) {
    const rx_addr out = L + 4, pos = L + 0x10;
    { const uint32_t a[1] = {pos}; act_vcall(veh, 0x44, 1, a); }
    const float reach = F32(self, 0x374);
    { const rx_addr sp = rx_sp(); SU32(sp, -20, act + 0x890); SU32(sp, -16, out); SU32(sp, -12, rx_fb(reach)); SU32(sp, -8, 0); SU32(sp, -4, 0); }
    ACT_CALL(5, rx_track_nearest_in_radius(pos, act + 0x890, out, reach, 0, 0));
    return U32(L, 8);
}

/* 006c2f60 (thiscall, stack activity, vehicle; RET 8): "VehicleWrecked": a wreck behind the holder's node within its window (+0x378): the clip +0x35c plays, flag state 1,
 * texture 1 */
void rx_track_flag_vehicle_wrecked(rx_addr self, rx_addr act, rx_addr veh) {
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    const rx_addr node = nearest_in_reach(self, act, veh, L);
    if (node != 0) {
        const float a = F32(node, 0x4f4), b = F32(U32(self, 0x368), 0x4f4);
        if (b > a && a > phys_sub(b, F32(self, 0x378))) {
            SU32(self, 0x16c, 0); SU32(self, 0x170, 0); SU32(self, 0x174, U32(self, 0x35c)); SF32(self, 0x184, 0.0f);
            SU32(self, 0x370, 1);
            { const rx_addr sp = rx_sp(); SU32(sp, -8, self); SU32(sp, -4, 1); }
            ACT_CALL(2, rx_track_flag_set_flag_texture(self, 1));
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c2f60, rx_track_flag_vehicle_wrecked)

/* 006c3020 (thiscall, stack activity, vehicle; RET 8): "VehicleGettingLapped": unless the flag is state 1: a vehicle ahead of the node within the reach window (+0x37c): flag
 * state 2, texture 0 */
void rx_track_flag_vehicle_lapped(rx_addr self, rx_addr act, rx_addr veh) {
    ACT_FRAME_EBP(L, 16, 0x1c, 1);
    if (U32(self, 0x370) != 1) {
        const rx_addr node = nearest_in_reach(self, act, veh, L);
        if (node != 0) {
            const float a = F32(node, 0x4f4), b = F32(U32(self, 0x368), 0x4f4);
            if (a > b && phys_add(F32(self, 0x37c), b) > a) {
                SU32(self, 0x16c, 0); SU32(self, 0x170, 0); SU32(self, 0x174, U32(self, 0x35c)); SF32(self, 0x184, 0.0f);
                SU32(self, 0x370, 2);
                { const rx_addr sp = rx_sp(); SU32(sp, -8, self); SU32(sp, -4, 0); }
                ACT_CALL(2, rx_track_flag_set_flag_texture(self, 0));
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c3020, rx_track_flag_vehicle_lapped)
