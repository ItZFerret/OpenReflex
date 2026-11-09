// build_activity_teardown.cpp -- oracle adapters for core/game/build_activity_teardown.c and build_activity_audio_teardown.c (group
// rx_build11): Activity::vf3, the event system's destruction, the audio logic's teardown. No reference world tears an activity
// down; the probe core/oracle/race_teardown_probe.cpp drives it.
#include "oracle.h"
extern "C" {
#include "../game/build_activity_teardown.h"
}
using namespace reflex_port;

#define G "rx_build11"
// ---- the unsubscription templates ----
RX_PORT(0x00587330, Activity_UnlistenNetTimer, G, 0) { rx_activity_unlisten_net_timer(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587390, Activity_UnlistenNetSession, G, 0) { rx_activity_unlisten_net_session(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005873f0, Activity_UnlistenNetPositions, G, 0) { rx_activity_unlisten_net_positions(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587450, Activity_UnlistenNetMode, G, 0) { rx_activity_unlisten_net_mode(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005874b0, Activity_UnlistenGhostReset, G, 0) { rx_activity_unlisten_ghost_reset(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587510, Activity_UnlistenVehicleEntered, G, 0) { rx_activity_unlisten_vehicle_entered(m.arg(0)); return m.ret(4); }
RX_PORT(0x00587580, Activity_UnlistenPageWipe, G, 0) { rx_activity_unlisten_page_wipe(m.r[0], m.arg(0)); return m.ret(4); }
// 00755880 / 007074d0 (ECX = channel, EAX = name; stdcall self, method; RET 8)
RX_PORT(0x00755880, DataMiner_Unlisten, G, 0) { rx_dataminer_unlisten(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x007074d0, NetMgr_Unlisten, G, 0) { rx_netmgr_unlisten(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// 006deff0 (ESI = stats block, EAX = activity)
RX_PORT(0x006deff0, NetMgr_ActivityUnlisten, G, 0) { rx_netmgr_activity_unlisten(m.r[6], m.r[0]); return m.ret(0); }
// ---- the event system ----
// 00593770 (stdcall registry, channel; RET 8)
RX_PORT(0x00593770, ChannelRegistry_Remove, G, 0) { rx_channel_registry_remove(m.arg(0), m.arg(1)); return m.ret(8); }
// 00593a70 (stdcall map, out, cont, node; RET 0x10) -> EAX = out, ECX = the map's proxy
RX_PORT(0x00593a70, ChannelMap_Erase, G, OUT_EAX | OUT_ECX) {
    const uint32_t map = m.arg(0);
    m.r[0] = rx_channel_map_erase(map, m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = rx_rd32(map);
    return m.ret(0x10);
}
// 00851f10 (ECX = map; stdcall node; RET 4)
RX_PORT(0x00851f10, ChannelMap_EraseSubtree, G, 0) { rx_channel_map_erase_subtree(m.r[1], m.arg(0)); return m.ret(4); }
// 00705070 (EAX = map; stdcall out, fcont, fnode, lcont, lnode; RET 0x14) -> EAX = out
RX_PORT(0x00705070, ChannelMap_EraseRange, G, OUT_EAX) {
    m.r[0] = rx_channel_map_erase_range(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
// 00593ef0 (ESI = list)
RX_PORT(0x00593ef0, ChannelList_Clear, G, 0) { rx_channel_list_clear(m.r[6]); return m.ret(0); }
// 00592c40 / 00593120 / 00593330 (ECX = object)
RX_PORT(0x00592c40, Event_Dtor, G, 0) { rx_event_dtor(m.r[1]); return m.ret(0); }
RX_PORT(0x00593120, BaseChannel_Dtor, G, 0) { rx_base_channel_dtor(m.r[1]); return m.ret(0); }
RX_PORT(0x00593330, Channel_Dtor, G, 0) { rx_channel_dtor(m.r[1]); return m.ret(0); }
// 00592c20 / 00593310 sdtors (thiscall; flags; RET 4) -> EAX = this
RX_PORT(0x00592c20, Event_Sdtor, G, OUT_EAX) { m.r[0] = rx_event_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00593310, Channel_Sdtor, G, OUT_EAX) { m.r[0] = rx_channel_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00557790 Activity::vf3 (thiscall)
RX_PORT(0x00557790, Activity_Vf3, G, 0) { rx_activity_vf3(m.r[1]); return m.ret(0); }
// ---- the audio logic ----
RX_PORT(0x0058b010, Audio_UnlistenState, G, 0) { rx_audio_unlisten_state(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058ad70, Audio_UnlistenStunt, G, 0) { rx_audio_unlisten_stunt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058add0, Audio_UnlistenPhysicsStunt, G, 0) { rx_audio_unlisten_physics_stunt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058aef0, Audio_UnlistenWreck, G, 0) { rx_audio_unlisten_wreck(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058b850, Audio_UnlistenDriver, G, 0) { rx_audio_unlisten_driver(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058b370, Audio_UnlistenRestart, G, 0) { rx_audio_unlisten_restart(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058b8b0, Audio_UnlistenBestLap, G, 0) { rx_audio_unlisten_best_lap(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058b910, Audio_UnlistenLapped, G, 0) { rx_audio_unlisten_lapped(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058b970, Audio_UnlistenPlayerWins, G, 0) { rx_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058b9d0, Audio_UnlistenEliminated, G, 0) { rx_audio_unlisten_eliminated(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058ba30, Audio_UnlistenTimedSection, G, 0) { rx_audio_unlisten_timed_section(m.r[0], m.arg(0)); return m.ret(4); }
// 00581e20 / 00581b10 (ESI = logic)
RX_PORT(0x00581e20, Audio_UnlistenStates, G, 0) { rx_audio_unlisten_states(m.r[6]); return m.ret(0); }
RX_PORT(0x00581b10, Audio_UnlistenWrecks, G, 0) { rx_audio_unlisten_wrecks(m.r[6]); return m.ret(0); }
// virtuals (thiscall)
RX_PORT(0x00580f80, AudioLogic_Vf30, G, 0) { rx_audio_logic_vf30(m.r[1]); return m.ret(0); }
RX_PORT(0x005825f0, RaceAudioLogic_Vf30, G, 0) { rx_race_audio_logic_vf30(m.r[1]); return m.ret(0); }
RX_PORT(0x005829b0, RaceAudioLogic_Vf4, G, 0) { rx_race_audio_logic_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058b130, Audio_UnlistenChallengeAward, G, 0) { rx_audio_unlisten_challenge_award(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005823b0, FreeRideAudioLogic_Vf4, G, 0) { rx_freeride_audio_logic_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00582260, FreeRideAudioLogic_Vf30, G, 0) { rx_freeride_audio_logic_vf30(m.r[1]); return m.ret(0); }
// ---- the activity area's release (Activity::vf6) ----
RX_PORT(0x007d1470, Scene_SubsceneRelease, G, 0) { rx_scene_subscene_release(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x007d19f0, Scene_SubsceneUnload, G, 0) { rx_scene_subscene_unload(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00767850, World_RacingLineRelease, G, 0) { rx_world_racing_line_release(m.arg(0)); return m.ret(4); }
RX_PORT(0x00557a10, Activity_Vf6, G, 0) { rx_activity_vf6(m.r[1], m.arg(0)); return m.ret(4); }
