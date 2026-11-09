/* loop_player_destroy.c -- MXPlayer's destruction (MXPlayer::sdtor 005497e0, the destructor 00549830): a player whose Player List row
 * went inactive leaves its slot (FUN_0075ba20 -> 00552c60 -> vf0(1)). The Heat format's activity area re-creation is the career's path
 * there: the riders of a heat that are not in the LCQ / main event are removed. The destructor undoes MXPlayer's construction
 * (build_activity_player.c rx_mxplayer_init 005490f0): its handlers on the session channel [0x00da6ca0], the stunt channel
 * [0x00a94304] and the vehicle channel [0x00a89bc8], its vehicle (0054a500), its stunt system's handlers (006cb010) and trick sets, the
 * owned objects (+0xb24, +0xb28, +0xb00) and the name plate (+0xa30). */
#include "loop_player_destroy.h"
#include "build_activity_util.h"
#include "build_activity_teardown.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "anim.h"
#include "loop_platform.h"
#include "loop_input_suspend.h"
#include "loop_util.h"
#include "crt.h"

/* One handler removed from a registry's event `name`: the key the handler was added with (the proxy class's vtable, the signature's
 * type name, the object and its member function) built in the caller's frame at `key` (+0 vtable, +4 0, +8 the type name, +0xc the
 * object, +0x10 the member), then EventSystem's remove (00592ef0, by equality). */
static void unlisten(rx_addr registry, rx_addr name, rx_addr key, rx_addr vtable, rx_addr type, rx_addr object, rx_addr member) {
    rx_addr event;
    ACT_CALLR(event, 2, (rx_addr)rx_world_registry_find(registry, name));   /* 00593220 */
    SU32(key, 4, 0);
    SU32(key, 0, vtable);
    SU32(key, 0xc, object);
    SU32(key, 0x10, member);
    SU32(key, 8, crt_type_info_name(type, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(event, key));                   /* 00592ef0 */
}

/* ---- the template instances (frame: and esp, -8; sub esp, 0x14; push esi: the key at +4) --------------------------------------- */

/* 005549f0 (EAX = name, ECX = registry; stdcall player, member; RET 8): EventHandlerProxy<MXPlayer, void (NetworkObject*)> 0x00941794 */
void rx_player_unlisten_network_object(rx_addr name, rx_addr registry, rx_addr player, rx_addr member) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(registry, name, F + 4, 0x00941794, 0x009cdc7c, player, member);
    ACT_END(F);
}
/* 00554a50 (EAX = registry; stdcall player; RET 4): "NetworkStuntUpdate", proxy 0x009417b4, member 0054bb90 */
void rx_player_unlisten_stunt_update(rx_addr registry, rx_addr player) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(registry, 0x00941354, F + 4, 0x009417b4, 0x009ce5f4, player, 0x0054bb90);
    ACT_END(F);
}
/* 00554ab0 (EAX = registry; stdcall player; RET 4): "NetworkVehicleReset", proxy 0x009417d4, member 0054ff30 */
void rx_player_unlisten_vehicle_reset(rx_addr registry, rx_addr player) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(registry, 0x00941368, F + 4, 0x009417d4, 0x009ce674, player, 0x0054ff30);
    ACT_END(F);
}
/* 00554b10 (EAX = name, ECX = registry; stdcall player, member; RET 8): a stunt event, proxy 0x009417f4 (void (MXPlayer*,
 * const StuntDescriptor*)) */
void rx_player_unlisten_stunt(rx_addr name, rx_addr registry, rx_addr player, rx_addr member) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(registry, name, F + 4, 0x009417f4, 0x009cbf38, player, member);
    ACT_END(F);
}
/* 00554b70 (EAX = name, ECX = registry; stdcall player, member; RET 8): a physics stunt event, proxy 0x00941814 */
void rx_player_unlisten_physics_stunt(rx_addr name, rx_addr registry, rx_addr player, rx_addr member) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(registry, name, F + 4, 0x00941814, 0x009ce768, player, member);
    ACT_END(F);
}
/* 00554bd0 (stdcall player; RET 4): "VehicleEntered" on the vehicle channel [0x00a89bc8], proxy 0x00941834, member 0054f0b0 */
void rx_player_unlisten_vehicle_entered(rx_addr player) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    unlisten(U32(0, 0x00a89bc8), 0x00935e84, F + 4, 0x00941834, 0x009cbb7c, player, 0x0054f0b0);
    ACT_END(F);
}

/* ---- the stunt system's handlers: 006cb010 (EDI = the stunt system P + 0xc70), the instances 006cdbd0 / 006cdc30 / 006cdc90 /
 * 006cdcf0 (frame: sub esp, 0x14; push esi: the key at +4) -------------------------------------------------------------------- */

/* 006cdbd0 (EAX = name, ECX = registry; stdcall system, member; RET 8): proxy 0x00960ae4 (void (MXPlayer*, const StuntDescriptor*)) */
void rx_stunt_unlisten(rx_addr name, rx_addr registry, rx_addr system, rx_addr member) {
    ACT_FRAME(F, 0x14, 1);
    unlisten(registry, name, F + 4, 0x00960ae4, 0x009cbf38, system, member);
    ACT_END(F);
}
/* 006cdc30 (EAX = registry; stdcall system; RET 4): "ComboFromStunt", proxy 0x00960b04, member 006cb560 */
void rx_stunt_unlisten_combo(rx_addr registry, rx_addr system) {
    ACT_FRAME(F, 0x14, 1);
    unlisten(registry, 0x009607b4, F + 4, 0x00960b04, 0x009d9a04, system, 0x006cb560);
    ACT_END(F);
}
/* 006cdc90 (EAX = name, ECX = registry; stdcall system, member; RET 8): proxy 0x00960b24 (physics stunts) */
void rx_stunt_unlisten_physics(rx_addr name, rx_addr registry, rx_addr system, rx_addr member) {
    ACT_FRAME(F, 0x14, 1);
    unlisten(registry, name, F + 4, 0x00960b24, 0x009ce768, system, member);
    ACT_END(F);
}
/* 006cdcf0 (EAX = name, ECX = registry; stdcall system, member; RET 8): proxy 0x00960b44 (void (MXPlayer*): the vehicle events) */
void rx_stunt_unlisten_vehicle(rx_addr name, rx_addr registry, rx_addr system, rx_addr member) {
    ACT_FRAME(F, 0x14, 1);
    unlisten(registry, name, F + 4, 0x00960b44, 0x009cbeb4, system, member);
    ACT_END(F);
}

/* 006cb010 (EDI = the stunt system): its handlers on the stunt channel removed, in the order stunt_system_register (006caf20) adds
 * them. Frame: and esp, -8; sub esp, 0x18; push ebx / esi: the two inline removals use a key at +0xc. */
void rx_stunt_system_unregister(rx_addr system) {
    ACT_FRAME_EBP(F, 8, 0x18, 2);
    const rx_addr channel = U32(0, 0x00a94304);
    ACT_CALL(2, rx_stunt_unlisten(0x0095ba28, channel, system, 0x006cb1b0));   /* "StuntBegin" */
    ACT_CALL(2, rx_stunt_unlisten(0x00942784, channel, system, 0x006cb3c0));   /* "StuntCompleted" */
    ACT_CALL(1, rx_stunt_unlisten_combo(channel, system));                     /* "ComboFromStunt" */
    ACT_CALL(2, rx_stunt_unlisten(0x00960764, channel, system, 0x006cb6a0));   /* "StuntFailed" */
    ACT_CALL(2, rx_stunt_unlisten(0x00960770, channel, system, 0x006cb660));   /* "StuntKickOut" */
    unlisten(channel, 0x00960790, F + 0xc, 0x00960ae4, 0x009cbf38, system, 0x005a4150);   /* "StuntHoldBegin" (inline) */
    unlisten(channel, 0x00960780, F + 0xc, 0x00960ae4, 0x009cbf38, system, 0x005a4150);   /* "StuntHoldEnd" (inline) */
    ACT_CALL(2, rx_stunt_unlisten_physics(0x009607a0, channel, system, 0x006cbbe0));   /* "PhysicsStuntBegin" */
    ACT_CALL(2, rx_stunt_unlisten_physics(0x00942794, channel, system, 0x006cbdb0));   /* "PhysicsStuntEnd" */
    ACT_CALL(2, rx_stunt_unlisten(0x00960758, channel, system, 0x006cb5a0));   /* "StuntLanded" */
    ACT_CALL(2, rx_stunt_unlisten(0x00960748, channel, system, 0x006cb610));   /* "StuntTakeoff" */
    ACT_CALL(2, rx_stunt_unlisten_vehicle(0x009407c4, channel, system, 0x006cc0d0));   /* "VehicleLanded" */
    ACT_CALL(2, rx_stunt_unlisten_vehicle(0x00960738, channel, system, 0x006cc1a0));   /* "VehicleTakeoff" */
    ACT_END(F);
}

/* 005ba710 (ESI = a std::set of the stunt system: +0 proxy, +0x18 head, +0x1c size; node isnil +0x2d): clear() through erase(begin,
 * end) (005baaf0's whole-range branch: _Erase(root) 00851f10, the head linked to itself, size 0), then the head freed and +0x18 /
 * +0x1c zeroed. Frame: and esp, -8; sub esp, 8 (the iterator 005baaf0 returns). */
void rx_stunt_set_destroy(rx_addr set) {
    ACT_FRAME_EBP(F, 8, 8, 0);
    {   /* 005baaf0 (EAX = set; &out, begin, end; RET 0x14): sub esp, 8; push ebx / ebp / esi / edi */
        ACT_FRAME(E, 8, 4);
        ACT_CALL(1, rx_channel_map_erase_subtree(set, U32(U32(set, 0x18), 4)));   /* 00851f10 */
        const rx_addr head = U32(set, 0x18);
        SU32(head, 4, head);
        SU32(set, 0x1c, 0);
        SU32(head, 0, head);
        SU32(head, 8, head);
        SU32(F, 4, U32(U32(set, 0x18), 0));                                  /* the returned iterator {proxy, node} */
        SU32(F, 0, U32(set, 0));
        ACT_END(E);
    }
    ACT_CALL(1, rx_reflex_aligned_free(U32(set, 0x18)));
    SU32(set, 0x18, 0);
    SU32(set, 0x1c, 0);
    ACT_END(F);
}

/* ---- the destructor ------------------------------------------------------------------------------------------------------------ */

/* 00549830 (stdcall player; RET 4). Frame: and esp, -8; sub esp, 0x14; push ebx / esi / edi: the inline keys at +0xc. */
void rx_mxplayer_destroy(rx_addr P) {
    ACT_FRAME_EBP(F, 8, 0x14, 3);
    SU32(P, 0, 0x00941784);                                                  /* MXPlayer's vtable */
    {   /* Player_SetInputSuspended( P, 0 ) 0054f540 (loop_input_suspend.c): a suspended player (a UI page held its input) gets its input
         * groups back; nothing when +0x11c8 is 0 */
        const uint32_t a[2] = {P, 0};
        const rx_addr s = loop_push(2, a, 0x0054984d);
        rx_player_set_input_suspended(P, 0);
        loop_pop(s);
    }
    if (U32(P, 0x9a4) != 0) {                                                /* the scene entity of the player */
        ACT_CALL(1, rx_scene_manager_remove_entity(U32(P, 0x9a4), U32(U32(P, 0x9a4), 0x10)));   /* 007d3040 (ESI = the entity; its scene) */
        act_vcall(U32(P, 0x9a4), 0x24, 0, 0);
        if (U32(P, 0x9a4) != 0) act_vcall(U32(P, 0x9a4), 0, 1, (const uint32_t[]){1});
        SU32(P, 0x9a4, 0);
    }
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x0054989d, "the session's static initialiser has not run");
    ACT_CALL(2, rx_player_unlisten_network_object(0x0093ee98, U32(0, 0x00da6ca0), P, 0x0054b950));   /* "NetworkObjectRemoved" */
    ACT_CALL(2, rx_player_unlisten_network_object(0x0093eeb0, U32(0, 0x00da6ca0), P, 0x0054b9d0));   /* "NetworkObjectMigrated" */
    ACT_CALL(1, rx_player_unlisten_stunt_update(U32(0, 0x00da6ca0), P));
    ACT_CALL(1, rx_player_unlisten_vehicle_reset(U32(0, 0x00da6ca0), P));
    const rx_addr stunts = U32(0, 0x00a94304);
    ACT_CALL(2, rx_player_unlisten_stunt(0x0095ba28, stunts, P, 0x0054d120));          /* "StuntBegin" */
    unlisten(stunts, 0x00960770, F + 0xc, 0x009417f4, 0x009cbf38, P, 0x005a4150);     /* "StuntKickOut" (inline) */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00960790, stunts, P, 0x0054d1a0));          /* "StuntHoldBegin" */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00960780, stunts, P, 0x0054d1e0));          /* "StuntHoldEnd" */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00942784, stunts, P, 0x0054d220));          /* "StuntCompleted" */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00960764, stunts, P, 0x0054d2a0));          /* "StuntFailed" */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00960758, stunts, P, 0x0054d310));          /* "StuntLanded" */
    ACT_CALL(2, rx_player_unlisten_stunt(0x00960748, stunts, P, 0x0054d360));          /* "StuntTakeoff" */
    unlisten(stunts, 0x009607a0, F + 0xc, 0x00941814, 0x009ce768, P, 0x0083d480);     /* "PhysicsStuntBegin" (inline) */
    ACT_CALL(2, rx_player_unlisten_physics_stunt(0x00942794, stunts, P, 0x0054d530));  /* "PhysicsStuntEnd" */
    ACT_CALL(1, rx_player_unlisten_vehicle_entered(P));
    ACT_CALL(1, rx_player_vehicle_clear(P));                                 /* 0054a500 */
    ACT_CALL(0, rx_stunt_system_unregister(P + 0xc70));                      /* 006cb010 */
    ACT_CALL(0, rx_stunt_set_destroy(P + 0xc70 + 0x344));                    /* 005ba710: the second trick set */
    ACT_CALL(1, rx_reflex_aligned_free(U32(P, 0xc70 + 0x344)));              /* its proxy */
    ACT_CALL(0, rx_stunt_set_destroy(P + 0xc70 + 0x324));
    ACT_CALL(1, rx_reflex_aligned_free(U32(P, 0xc70 + 0x324)));
    if (U32(P, 0xb24) != 0) {
        act_vcall(U32(P, 0xb24), 0, 1, (const uint32_t[]){1});
        SU32(P, 0xb24, 0);
    }
    if (U32(P, 0xb28) != 0) {
        act_vcall(U32(P, 0xb28), 0, 1, (const uint32_t[]){1});
        SU32(P, 0xb28, 0);
    }
    if (U32(P, 0xb00) != 0) {
        SU32(0, 0x00a93f70, U32(P, 0xb00));
        act_vcall(U32(P, 0xb00), 0, 1, (const uint32_t[]){1});
        SU32(P, 0xb00, 0);
    }
    {   /* 007ffba0 (ECX = P + 0xa30): TextIcon3dBillboard's destructor, the name plate. RENDER BOUNDARY: the product never builds the
         * name plate (rx_mxplayer_init: 006b62a0 is the oracle's only), so it has nothing to destroy; it writes only the billboard (inside
         * this player), its text buffer (+0x70) and the shared icon resource's count [0x00a957a8] / [0x00a957ac], which only the
         * renderer reads. The oracle runs the original. */
#ifdef RX_ORACLE
        rx_call_original(0x007ffba0, RX_THISCALL, P + 0xa30, 0, 0);
#endif
    }
    SU32(P, 0, 0x0094177c);                                                  /* Player's vtable */
    ACT_END(F);
}
RX_FUNCTION(0x00549830, rx_mxplayer_destroy)

/* MXPlayer::sdtor 005497e0 (thiscall; flags; RET 4) -> the player: the destructor, then (flags bit 0) _aligned_free under the
 * allocator lock (critical section 0x00d61704 when [0x00da67d8] is set) */
rx_addr rx_mxplayer_sdtor(rx_addr P, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(1, rx_mxplayer_destroy(P));
    if (flags & 1) {
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        rx_set_sp(F - 4);
        SU32(F, -4, P);
        crt_aligned_free(P);                                                 /* [0x00916350] _aligned_free */
        rx_set_sp(F);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
    }
    ACT_END(F);
    return P;
}
RX_FUNCTION(0x005497e0, rx_mxplayer_sdtor)

/* the register-convention instances, registered with their core signatures (their adapters map the registers) */
RX_FUNCTION(0x005549f0, rx_player_unlisten_network_object)
RX_FUNCTION(0x00554a50, rx_player_unlisten_stunt_update)
RX_FUNCTION(0x00554ab0, rx_player_unlisten_vehicle_reset)
RX_FUNCTION(0x00554b10, rx_player_unlisten_stunt)
RX_FUNCTION(0x00554b70, rx_player_unlisten_physics_stunt)
RX_FUNCTION(0x00554bd0, rx_player_unlisten_vehicle_entered)
RX_FUNCTION(0x006cdbd0, rx_stunt_unlisten)
RX_FUNCTION(0x006cdc30, rx_stunt_unlisten_combo)
RX_FUNCTION(0x006cdc90, rx_stunt_unlisten_physics)
RX_FUNCTION(0x006cdcf0, rx_stunt_unlisten_vehicle)
RX_FUNCTION(0x006cb010, rx_stunt_system_unregister)
RX_FUNCTION(0x005ba710, rx_stunt_set_destroy)
