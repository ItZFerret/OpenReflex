/* world_scene_handlers.c -- event handlers of the event world's scene entities (PopUpBale / PopDownBale "VehicleExited") and of the rider clip recorder's
 * stunt listeners (ClipRecorder_Init 006aa660 registers 006ab290 / 006ab380 / 006ab410 / 006ab4a0 on the global stunt channel [0x00a94304] with the object
 * 0x00d32360; they are thiscall members that never read `this`). The bales' online replication calls (006f7910 / 006f7a10: the network session's bale state
 * through the Quazal reference handle [0x00d60d54], which is the null handle offline) are the network boundary: the oracle runs the original, the product
 * does nothing (nothing in the simulation reads the replicated record; see core/plan/QUEUE.md "handlers"). */
#include "build_activity_util.h"
#include "world_scene_handlers.h"
#include "phys_vec.h"
#include "world.h"
#include "world_veh.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "world_flag_holders.h"
#include "race_activity_misc.h"
#include "race_activity.h"
#include "fx.h"
#include "fx.h"
#include "build_activity_freeride_run.h"

/* 006f7910 / 006f7a10 (cdecl byte; the network replication of a bale's state): the network boundary (act_ui_call: oracle original, product nothing) */
static void net_bale_sync(rx_addr fn, uint32_t value) {
    const uint32_t a[1] = {value};
    act_ui_call(fn, RX_CDECL, 0, 1, a);
}

/* 006c14a0 (thiscall bale, stack vehicle, trigger; RET 8): PopUpBale's "VehicleExited" (the event's trigger: +0xc0 its name). While the bale is not up (+0x170 == 0) and
 * the trigger is the bale's own (+0x178: its trigger name, _stricmp equal): the vehicle's activity (vehicle +0x190 -> +0x94) of a mode that pops the bale up (the jump
 * table 0x006c1574 over mode - 1: modes 1 3 5 6 8 yes, 2 only for a driver whose +0x10 is 0, 4 and 7 and others no) and not a free ride (+0x23c != 8): the bale is
 * up (+0x174 = 0, +0x170 = 1, +0xa2 = 1); in a live race ([0xa942b4] == 1) the clip recorder's timestamp [0xa942d0] takes the race time when it is still 0.0; the
 * network replication 006f7910(1). Frame: push ecx, esi, edi. */
void rx_popup_bale_vehicle_exited(rx_addr self, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME(L, 0, 3);
    if (U32(self, 0x170) == 0) {
        const int32_t same = crt_stricmp(self + 0x178, trigger + 0xc0);
        if (same == 0) {
            const rx_addr checker = U32(vehicle, 0x190);
            const rx_addr act = U32(checker, 0x94);
            if (act != 0) {
                const uint32_t idx = U32(act, 0x238) - 1u;
                uint32_t pops = 0;
                if (idx <= 7) {
                    switch (idx) {
                    case 0: case 2: case 4: case 5: case 7: pops = 1; break;
                    case 1: pops = U32(checker, 0x10) == 0; break;
                    default: break;   /* modes 4 and 7: 0 */
                    }
                }
                if (U32(act, 0x23c) != 8 && pops != 0) {
                    const int live = U32(0, 0x00a942b4) == 1;
                    SU32(self, 0x174, 0);
                    SU32(self, 0x170, 1);
                    SU8(self, 0xa2, 1);
                    if (live && 0.0f == F32(0, 0x00a942d0)) SU32(0, 0x00a942d0, U32(U32(0, 0x00a942b8), 0x294));
                    net_bale_sync(0x006f7910, 1);
                }
            }
        }
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c14a0, rx_popup_bale_vehicle_exited)

/* 006c0b60 (thiscall bale, stack vehicle, trigger; RET 8): PopDownBale's "VehicleExited": the vehicle's activity (vehicle +0x190 -> +0x94) is the bale's own (+0x170) in mode 2
 * (+0x21c, the player state), the bale not down yet (+0x174 == 0) and the trigger its own (+0x17c, _stricmp): +0x178 = 0, +0x174 = 1 (down); the body of the collision part
 * (+0x158 -> +4 the world, the keyed body) leaves the active kinematic list (0076a8b0) and its +0xb0 is cleared; the bale's position through vf17 (the local's w word is the
 * original's stack leftover; the result is then replaced by the up pose +0x1d0, a dead local); in a live race the clip recorder's [0xa942cc] takes the race time when it
 * is still 0.0; the network replication 006f7a10(1). Frame: push ebp; and esp, -16; sub esp, 0x18; push esi, edi. */
void rx_popdown_bale_vehicle_exited(rx_addr self, rx_addr vehicle, rx_addr trigger) {
    ACT_FRAME_EBP(L, 16, 0x18, 2);
    const rx_addr act = U32(U32(vehicle, 0x190), 0x94);
    if (act == U32(self, 0x170) && U32(act, 0x21c) == 2 && U32(self, 0x174) == 0 && crt_stricmp(self + 0x17c, trigger + 0xc0) == 0) {
        SU32(self, 0x178, 0);
        SU32(self, 0x174, 1);
        if (U32(self, 0x158) != 0) {
            const rx_addr world = U32(U32(self, 0x158), 4);
            rx_addr body;
            ACT_CALLR(body, 0, rx_world_physics_handle_body(U32(self, 0x158)));
            ACT_CALL(1, rx_world_kinematic_deactivate(body, world));          /* 0076a8b0 (EDX = body; stack world) */
            ACT_CALLR(body, 0, rx_world_physics_handle_body(U32(self, 0x158)));
            SU8(body, 0xb0, 0);
        }
        { const uint32_t a[1] = {L + 0x10}; act_vcall(self, 0x44, 1, a); }       /* vf17 */
        SF32(L, 0x14, phys_sub(F32(L, 0x14), 5.0f /* 0x935890 */));
        for (uint32_t k = 0; k < 16; k += 4) SU32(L, 0x10 + k, U32(self, 0x1d0 + k));
        if (U32(0, 0x00a942b4) == 1 && 0.0f == F32(0, 0x00a942cc)) SU32(0, 0x00a942cc, U32(U32(0, 0x00a942b8), 0x294));
        net_bale_sync(0x006f7a10, 1);
    }
    ACT_END(L);
}
RX_FUNCTION(0x006c0b60, rx_popdown_bale_vehicle_exited)

/* ---- the rider clip recorder's stunt listeners: one event into the clip ring (0x00d2f160 + (cursor << 7), cursor [0xa942c8], 100 records of 0x80: +0 the race time, +4 the
 * event type, +8 the player's driver (the player's activity +0x94 index through 0055a050 into the race's racing list +0x268, or the waiting list +0x27c for a replay player
 * +0x908), +0x60 the stunt descriptor) while a race is being recorded ([0xa942b4] == 1) ---- */
static void clip_ring_event(uint32_t type, rx_addr driver, rx_addr stunt) {
    const uint32_t i = U32(0, 0x00a942c8);
    const rx_addr e = 0x00d2f160 + (i << 7);
    SU32(e, 4, type);
    SU32(e, 0, U32(U32(0, 0x00a942b8), 0x294));
    SU32(e, 8, driver);
    SU32(e, 0x60, stunt);
    SU32(0, 0x00a942c8, (uint32_t)((int32_t)(i + 1) % 100));
}
static rx_addr clip_driver_of(rx_addr player) {
    const rx_addr act = U32(player, 0x94);
    int32_t idx;
    ACT_CALLR(idx, 0, rx_world_group_rider_index(act, player));                    /* 0055a050 (ECX = the activity, ESI = the player) */
    return U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4u * (uint32_t)idx);
}

/* 006ab290 (thiscall 0x00d32360, stack player, stunt; RET 8): "StuntBegin": type 0; a player in the race (+4 set, +0x10 clear) with recording on (0xd5a9a4) also asks for a
 * type 4 clip candidate from 2.0 s before to 5.8 s after the race time (006aacf0). Frame: push ecx, esi, edi */
void rx_clip_event_stunt_begin(rx_addr self, rx_addr player, rx_addr stunt) {
    (void)self;
    ACT_FRAME(L, 0, 3);
    if (U32(0, 0x00a942b4) == 1) {
        const rx_addr driver = clip_driver_of(player);
        if (U32(player, 4) != 0 && U32(player, 0x10) == 0 && U32(0, 0x00d5a9a4) != 0) {
            const float now = F32(U32(0, 0x00a942b8), 0x294);
            const float end = phys_add(now, 5.8f /* 0x9c40cc */), start = phys_sub(now, 2.0f /* 0x9c40c8 */);
            ACT_CALL(2, rx_world_clip_recorder_candidate(4, driver, start, end));
        }
        clip_ring_event(0, driver, stunt);
    }
    ACT_END(L);
}
RX_FUNCTION(0x006ab290, rx_clip_event_stunt_begin)

/* 006ab380 (RET 8): "StuntHoldBegin": type 1. Frame: push esi, edi */
void rx_clip_event_stunt_hold_begin(rx_addr self, rx_addr player, rx_addr stunt) {
    (void)self;
    if (U32(0, 0x00a942b4) == 1) {
        ACT_FRAME(L, 0, 2);
        clip_ring_event(1, clip_driver_of(player), stunt);
        ACT_END(L);
    }
}
RX_FUNCTION(0x006ab380, rx_clip_event_stunt_hold_begin)

/* 006ab410 (RET 8): "StuntHoldEnd": type 2 */
void rx_clip_event_stunt_hold_end(rx_addr self, rx_addr player, rx_addr stunt) {
    (void)self;
    if (U32(0, 0x00a942b4) == 1) {
        ACT_FRAME(L, 0, 2);
        clip_ring_event(2, clip_driver_of(player), stunt);
        ACT_END(L);
    }
}
RX_FUNCTION(0x006ab410, rx_clip_event_stunt_hold_end)

/* 006ab4a0 (RET 8): "StuntCompleted": type 3 */
void rx_clip_event_stunt_completed(rx_addr self, rx_addr player, rx_addr stunt) {
    (void)self;
    if (U32(0, 0x00a942b4) == 1) {
        ACT_FRAME(L, 0, 2);
        clip_ring_event(3, clip_driver_of(player), stunt);
        ACT_END(L);
    }
}
RX_FUNCTION(0x006ab4a0, rx_clip_event_stunt_completed)

/* ---- unloading: the entities' vf9 (the scene removes the entity: ModelEntity::vf9, then the handlers leave their channels) and the listener-removal templates they use.
 * A listener {vtable, 0, the signature's type name, entity, callback, argument} is found in its channel's event (00593220) and removed (00592ef0); frame: sub esp, 0x1c;
 * push esi; the event find's two arguments (registry, name) pushed first. The same body as world_flag_holders.c's 006c46b0 with other proxy types. ---- */
static void unsubscribe_listener(rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr entity, uint32_t cb, uint32_t arg) {
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

/* 006c4450 / 006c4330 (EAX = channel; stack entity, callback, argument; RET 0xc): the "VehicleExited" listener of a PopUpBale (proxy 0x009604b4) / PopDownBale (0x00960494),
 * signature void(RavenVehicle*, const MXTrigger*) (0x009cbb7c); the unregistration partner of build_scene_props.c's 006c4390 / 006c4270 */
void rx_event_unregister_vehicle_exited_up(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_listener(channel, 0x00935e94, 0x009604b4, 0x009cbb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4450, rx_event_unregister_vehicle_exited_up)
void rx_event_unregister_vehicle_exited_down(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_listener(channel, 0x00935e94, 0x00960494, 0x009cbb7c, entity, cb, arg);
}
RX_FUNCTION(0x006c4330, rx_event_unregister_vehicle_exited_down)

/* 006c3c70 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc): a void(Activity*) listener (proxy 0x009603d4, 0x009cbd24) leaves its channel's event */
void rx_event_unregister_activity(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_listener(channel, name, 0x009603d4, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c3c70, rx_event_unregister_activity)

/* 006c4a40 (ECX = name; the activity manager's channel [0x00a93f74]; stack entity, callback, argument; RET 0xc): a global void(Activity*) listener (proxy 0x00960554) leaves */
void rx_event_unregister_global_c(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_listener(U32(0, 0x00a93f74), name, 0x00960554, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c4a40, rx_event_unregister_global_c)

/* 006c3930 (stack entity, callback, argument; RET 0xc): the "First Human Joined Activity" listener (proxy 0x00960394, the partner of 006c3860) leaves the activity manager's channel */
void rx_event_unregister_first_human(rx_addr entity, uint32_t cb, uint32_t arg) {
    unsubscribe_listener(U32(0, 0x00a93f74), 0x00941df4, 0x00960394, 0x009cbd24, entity, cb, arg);
}
RX_FUNCTION(0x006c3930, rx_event_unregister_first_human)

/* ModelEntity::vf9 00401ea0 (fx_rdb.c; thiscall, ECX = the entity), entered at the original's ESP. In the oracle the original runs: the port keeps the callee-saved
 * registers in the fx unit's register model (fx_util.h), which only the fx adapters bind (same as rx_character_entity_release, anim_character.c). */
static void model_entity_vf9(rx_addr self) {
#ifdef RX_ORACLE
    rx_call_original(0x00401ea0, RX_THISCALL, self, 0, 0);
#else
    ACT_CALL(0, rx_fx_model_entity_vf9(self));
#endif
}

/* 006c0f50 / 006c0730 (thiscall; vf9): PopUpBale / PopDownBale unload: ModelEntity::vf9, then the "VehicleExited" listener (channel +0x1e4, handler 006c14a0 / 006c0b60) leaves.
 * Frame: push ebp; and esp, -8; push ecx; push esi */
void rx_popup_bale_vf9(rx_addr self) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    model_entity_vf9(self);                                                  /* ModelEntity::vf9 (fx_rdb.c) */
    ACT_CALL(3, rx_event_unregister_vehicle_exited_up(U32(self, 0x1e4), self, 0x006c14a0, 0));
    ACT_END(L);
}
RX_FUNCTION(0x006c0f50, rx_popup_bale_vf9)
void rx_popdown_bale_vf9(rx_addr self) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    model_entity_vf9(self);
    ACT_CALL(3, rx_event_unregister_vehicle_exited_down(U32(self, 0x1e4), self, 0x006c0b60, 0));
    ACT_END(L);
}
RX_FUNCTION(0x006c0730, rx_popdown_bale_vf9)

/* 006c1890 (ESI = the tower): ScoreTowerFlagHolder's three global listeners leave (006c4580: "First Human Joined Activity" 006c18e0, "Activity Init" 006c1910,
 * "Activity UnInit" 006c1990). Frame: push ebp; and esp, -8 */
void rx_score_tower_unlisten(rx_addr self) {
    ACT_FRAME_EBP(L, 8, 0, 0);
    ACT_CALL(3, rx_event_unsubscribe_global(0x00941df4, self, 0x006c18e0, 0));
    ACT_CALL(3, rx_event_unsubscribe_global(0x00935ea4, self, 0x006c1910, 0));
    ACT_CALL(3, rx_event_unsubscribe_global(0x00941d90, self, 0x006c1990, 0));
    ACT_END(L);
}
RX_FUNCTION(0x006c1890, rx_score_tower_unlisten)

/* 006c1830 (thiscall; vf9): ScoreTowerFlagHolder unload: ModelEntity::vf9, then 006c1890 (ESI = this). Frame: push esi */
void rx_score_tower_vf9(rx_addr self) {
    ACT_FRAME(L, 0, 1);
    model_entity_vf9(self);
    ACT_CALL(0, rx_score_tower_unlisten(self));
    ACT_END(L);
}
RX_FUNCTION(0x006c1830, rx_score_tower_vf9)

/* 006c2d40 (ESI = the holder): TrackSideFlagHolder's three global listeners leave (006c4a40: 006c18e0, 006c2d90, 006c2e60) */
void rx_track_flag_unlisten(rx_addr self) {
    ACT_FRAME_EBP(L, 8, 0, 0);
    ACT_CALL(3, rx_event_unregister_global_c(0x00941df4, self, 0x006c18e0, 0));
    ACT_CALL(3, rx_event_unregister_global_c(0x00935ea4, self, 0x006c2d90, 0));
    ACT_CALL(3, rx_event_unregister_global_c(0x00941d90, self, 0x006c2e60, 0));
    ACT_END(L);
}
RX_FUNCTION(0x006c2d40, rx_track_flag_unlisten)

/* 006c2ce0 (thiscall; vf9): TrackSideFlagHolder unload */
void rx_track_flag_vf9(rx_addr self) {
    ACT_FRAME(L, 0, 1);
    model_entity_vf9(self);
    ACT_CALL(0, rx_track_flag_unlisten(self));
    ACT_END(L);
}
RX_FUNCTION(0x006c2ce0, rx_track_flag_vf9)

/* 006b7b60 (thiscall; vf9): ThirtySecondBoard unload: ModelEntity::vf9, then its "First Human Joined Activity" listener (006b7b90) leaves. Frame: push ebp; and esp, -8; push ecx; push esi */
void rx_thirty_second_board_vf9(rx_addr self) {
    ACT_FRAME_EBP(L, 8, 0, 2);
    model_entity_vf9(self);
    ACT_CALL(3, rx_event_unregister_first_human(self, 0x006b7b90, 0));
    ACT_END(L);
}
RX_FUNCTION(0x006b7b60, rx_thirty_second_board_vf9)

/* ---- the profile statistics (DataMiner, the save-game / records object 0x007557c0's `rec`; record layout: 0x1138-byte player blocks at +0x50 + player * 0x1138 ... and 0x89c-byte
 * "dirty" blocks at +0x4530): the activity's "End" handler ---- */

/* 005411f0 (ECX = the table object, EDI = the key; -> EAX): the record (+8) of the first entry (+0x28 pointers, count +0x2c; an entry's +4 the key) with the key, else 0 */
rx_addr rx_stat_table_find(rx_addr table, uint32_t key) {
    const int32_t n = I32(table, 0x2c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr e = U32(U32(table, 0x28), 4u * (uint32_t)i);
        if (e != 0 && U32(e, 4) == key) return U32(e, 8);
    }
    return 0;
}
RX_FUNCTION(0x005411f0, rx_stat_table_find)

/* 0050de10 (EAX = a FreestyleDriver; -> xmm0): the largest of its player's five trick scores: +0xc84 / +0xc94 / +0xc88 / +0xc8c / +0xc90 (the player = driver +0x74) when it has a
 * player, else five values of the driver's own block +0xf18 .. +0xf28; each step keeps the first when it is greater (comiss / ja), else takes the next */
float rx_freestyle_driver_best_score(rx_addr drv) {
    const rx_addr p = U32(drv, 0x74);
    if (p != 0) {
        float m = F32(p, 0xc84);
        m = rx_maxss(m, F32(p, 0xc94));
        m = rx_maxss(m, F32(p, 0xc88));
        m = rx_maxss(m, F32(p, 0xc8c));
        return rx_maxss(m, F32(p, 0xc90));
    }
    float m = F32(drv, 0xf18);
    m = rx_maxss(m, F32(drv, 0xf1c));
    m = rx_maxss(m, F32(drv, 0xf20));
    m = rx_maxss(m, F32(drv, 0xf24));
    return rx_maxss(m, F32(drv, 0xf28));
}
RX_FUNCTION(0x0050de10, rx_freestyle_driver_best_score)

/* 007538c0 (thiscall rec, stack activity; RET 4): "End" of the activity the records object follows (+0x1c; the start handler 00753760 sets it): for each of the activity's four
 * human drivers (00559e60) whose player (driver +0x74) has a counted slot (rec +0x3c + slot * 4, the slot = player +0x1f0): the bike's distance accumulator (+0x154, miles = * 1/5280
 * as an int) is added to the player's block (+0x100) and cleared; the player's two counters (+0x11d0 / +0x11d4) are added to +0x108 / +0x110 and cleared; a FreestyleDriver's best trick
 * score (0050de10) replaces the block's (+0x118, with its player +0x120); for a free ride the activity-kind counters (+0x50 + (kind + 0x33 + slot * 0x227) * 8, kind = rec +0x67a0) and
 * the event-type counter (005411f0) count up; for the other activities (a driver with +0x70) the same two counters count, then the win / race counters: offline +0xe0 (+0xd8 for the
 * session's own driver, rec - the activity's +0x2d0), networked +0xf0 (+0x150 when the session flag +0x1c48 is set, +0xe8 for the own driver). Frame: sub esp, 0xc; push ebp, esi
 * (+ ebx, edi inside the loop) */
void rx_dataminer_activity_end(rx_addr rec, rx_addr act) {
    ACT_FRAME(L, 0xc, 2);
    if (U32(rec, 0x1c) != 0 && act != 0) {
        if ((int32_t)U32(rec, 0x67a0) >= 0) {
            const rx_addr own = U32(act, 0x2d0);
            const rx_addr sp1 = rx_sp();
            rx_set_sp(L - 8);                                                   /* push ebx, edi */
            for (uint32_t n = 0; n < 4; n++) {
                rx_addr drv;
                ACT_CALLR(drv, 1, rx_activity_human_driver(U32(rec, 0x1c), (int32_t)n));
                if (drv == 0) continue;
                const rx_addr pl = U32(drv, 0x74);
                if (pl == 0) continue;
                const uint32_t slot = U32(pl, 0x1f0);
                if (U32(rec, 0x3c + 4 * slot) == 0) continue;
                const rx_addr blk = rec + slot * 0x1138, dirty = rec + slot * 0x89c;
                if (U32(pl, 0x1ac) != 0) {
                    const int32_t miles = rx_cvtt(phys_mul(F32(U32(pl, 0x1ac), 0x154), 0.00018939393f /* 0x977f14 */));
                    const uint32_t cur = U32(blk, 0x100);
                    if ((uint32_t)miles != cur) { SU32(blk, 0x100, cur + (uint32_t)miles); SU32(dirty, 0x4588, 0); }
                    SF32(U32(U32(drv, 0x74), 0x1ac), 0x154, 0.0f);
                }
                if ((int32_t)U32(U32(drv, 0x74), 0x11d0) > 0) {
                    SU32(blk, 0x108, U32(blk, 0x108) + U32(U32(drv, 0x74), 0x11d0));
                    SU32(dirty, 0x458c, 0);
                    SU32(U32(drv, 0x74), 0x11d0, 0);
                }
                if ((int32_t)U32(U32(drv, 0x74), 0x11d4) > 0) {
                    SU32(blk, 0x110, U32(blk, 0x110) + U32(U32(drv, 0x74), 0x11d4));
                    SU32(dirty, 0x4590, 0);
                    SU32(U32(drv, 0x74), 0x11d4, 0);
                }
                rx_addr fd;
                ACT_CALLR(fd, 5, crt_rtdynamiccast(drv, 0, 0x009cd798, 0x009cd554, 0));            /* BaseDriver -> FreestyleDriver */
                if (fd != 0 && U32(U32(fd, 0x74), 0x10a0) != 0) {
                    const rx_addr best_player = U32(U32(fd, 0x74), 0x10a0);
                    const float score = rx_freestyle_driver_best_score(fd);
                    if (score > F32(blk, 0x118)) {
                        SF32(blk, 0x118, score);
                        SU32(blk, 0x120, U32(best_player, 0x74));
                        SU32(dirty, 0x4594, 0);
                        SU32(dirty, 0x4598, 0);
                    }
                }
                rx_addr fr;
                ACT_CALLR(fr, 5, crt_rtdynamiccast(act, 0, 0x009cf3d8, 0x009cf018, 0));            /* Activity -> MXActivityFreeRide */
                const rx_addr table = U32(0, 0x00a946b8) + 0x154b0;
                if (fr != 0) {
                    uint32_t k = U32(rec, 0x67a0) + 0x33;
                    if ((int32_t)k < 0x227) { k += slot * 0x227; SU32(rec, 0x50 + 8 * k, U32(rec, 0x50 + 8 * k) + 1); SU32(rec, 0x4530 + 4 * k, 0); }
                    const rx_addr stat = rx_stat_table_find(table, U32(U32(drv, 0x74), 0x1b0));
                    if (stat == 0) continue;
                    const uint32_t j = slot * 0x227 + U32(stat, 4) + 0x20;
                    SU32(rec, 0x50 + 8 * j, U32(rec, 0x50 + 8 * j) + 1);
                    SU32(rec, 0x4530 + 4 * j, 0);
                } else {
                    if (U32(drv, 0x70) == 0) continue;
                    const rx_addr stat = rx_stat_table_find(table, U32(U32(drv, 0x74), 0x1b0));
                    if (stat != 0) {
                        const uint32_t j = slot * 0x227 + U32(stat, 4) + 0x20;
                        SU32(rec, 0x50 + 8 * j, U32(rec, 0x50 + 8 * j) + 1);
                        SU32(rec, 0x4530 + 4 * j, 0);
                    }
                    uint32_t k = U32(rec, 0x67a0) + 0x33;
                    if ((int32_t)k < 0x227) { k += slot * 0x227; SU32(rec, 0x50 + 8 * k, U32(rec, 0x50 + 8 * k) + 1); SU32(rec, 0x4530 + 4 * k, 0); }
                    rx_addr sm;
                    ACT_CALLR(sm, 0, rx_fx_session_manager());                          /* 006de1c0 */
                    const rx_addr ses = U32(sm, 0x10);
                    if (!rx_session_is_networked(ses)) {
                        SU32(blk, 0xe0, U32(blk, 0xe0) + 1);
                        SU32(dirty, 0x4578, 0);
                        if (own == drv) { SU32(blk, 0xd8, U32(blk, 0xd8) + 1); SU32(dirty, 0x4574, 0); }
                    } else {
                        SU32(blk, 0xf0, U32(blk, 0xf0) + 1);
                        SU32(dirty, 0x4580, 0);
                        ACT_CALLR(sm, 0, rx_fx_session_manager());
                        if (U32(U32(sm, 0x10), 0x1c48) != 0) { SU32(blk, 0x150, U32(blk, 0x150) + 1); SU32(dirty, 0x45b0, 0); }
                        if (own == drv) { SU32(blk, 0xe8, U32(blk, 0xe8) + 1); SU32(dirty, 0x457c, 0); }
                    }
                }
            }
            rx_set_sp(sp1);
        }
        SU32(rec, 0x1c, 0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x007538c0, rx_dataminer_activity_end)
