// build_mxgame.cpp -- oracle adapters for construction unit 10 (the MXGame object and its startup: core/game/build_mxgame*.c, group
// rx_build10). Verified per call inside a baton boot of the reference event (REFLEX_PORT=rx_build10 REFLEX_PORT_VERIFY=1): the core
// function runs on the emulated state, then the original from the same state; memory writes and the declared registers compared.
// ABI glue only.
#include "oracle.h"
extern "C" {
#include "../game/build_mxgame.h"
#include "../game/build_mxworld.h"
#include "../game/crt.h"
#include "../game/race_driver.h"
}
using namespace reflex_port;

#define G "rx_build10"
// ---- the MXGame object (build_mxgame.c) ----
RX_PORT(0x00755f20, MXGameNew, G, OUT_EAX) { m.r[0] = rx_mxgame_new(); return m.ret(0); }
RX_PORT(0x00756760, MXGameConstruct, G, OUT_EAX) { m.r[0] = rx_mxgame_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x00755ff0, WorldConfigConstruct, G, OUT_EAX) { m.r[0] = rx_world_config_construct(m.r[7]); return m.ret(0); }
RX_PORT(0x00552890, PlayerArrayConstruct, G, OUT_EAX) { m.r[0] = rx_player_array_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x0053d210, GearListConstruct, G, OUT_EAX) { m.r[0] = rx_gear_list_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x00761330, GarageConstruct, G, OUT_EAX) { m.r[0] = rx_garage_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x0063d930, CameraEntityConstruct, G, OUT_EAX) { m.r[0] = rx_camera_entity_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x00675890, MusicPlayerConstruct, G, OUT_EAX) { m.r[0] = rx_music_player_construct(m.r[7]); return m.ret(0); }
RX_PORT(0x006d78c0, HighlightTrackingConstruct, G, OUT_EAX) { m.r[0] = rx_highlight_tracking_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x006d86e0, OnlineAchievementsConstruct, G, OUT_EAX) { m.r[0] = rx_online_achievements_construct(m.r[0]); return m.ret(0); }
RX_PORT(0x00752fd0, SaveObjectConstruct, G, OUT_EAX) { m.r[0] = rx_save_object_construct(m.arg(0)); return m.ret(4); }
// ---- the scenes (build_mxgame_scene.c) ----
RX_PORT(0x00882080, GameBaseConstruct, G, OUT_EAX) { m.r[0] = rx_game_base_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x007d14e0, SceneManagerConstruct, G, OUT_EAX) { m.r[0] = rx_scene_manager_construct(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x007df410, RenderContainerConstruct, G, OUT_EAX) { m.r[0] = rx_render_container_construct(m.r[7]); return m.ret(0); }
RX_PORT(0x007d1d50, SceneOctreeInit, G, 0) { rx_scene_octree_init(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x007dfba0, RenderGridCreate, G, 0) { rx_render_grid_create(m.r[0], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x007d6510, RenderGridInit, G, 0) { rx_render_grid_init(m.r[6], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x005ba080, VecpReserve, G, 0) { rx_vecp_reserve_ecx(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00843480, StrtabConstruct, G, OUT_EAX) { m.r[0] = rx_strtab_construct(m.r[6]); return m.ret(0); }
// ---- the channels (build_mxgame_event.c) ----
RX_PORT(0x006c8fd0, StuntChannelCreate, G, 0) { rx_stunt_channel_create(); return m.ret(0); }
RX_PORT(0x006cb870, StuntScoringChannelCreate, G, 0) { rx_stunt_scoring_channel_create(); return m.ret(0); }
RX_PORT(0x005ed6e0, ChannelAddFloatEvent, G, 0) { rx_channel_add_float_event(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005a61b0, SavedGameManagerConstruct, G, OUT_EAX) { m.r[0] = rx_saved_game_manager_construct(); return m.ret(0); }
// 0078fd60 (EDI = buffer, ESI = size; cdecl format, ...): the variadic arguments from [esp + 8]
RX_PORT(0x0078fd60, FormatBuffer, G, 0) { rx_format_buffer(m.r[7], m.r[6], m.arg(0), m.r[4] + 8); return m.ret(0); }
// ---- the CRT's first type name (crt_undname.c) ----
RX_PORT(0x7857c2b8, TypeNameBase, G, OUT_EAX) { m.r[0] = crt_type_name_base(m.arg(0), m.arg(1)); return m.ret(0); }
// ---- MXGame::Startup's callees (build_mxgame_init.c) ----
RX_PORT(0x0055ed50, ActivityGlobalChannel, G, 0) { rx_activity_global_channel_create(); return m.ret(0); }
RX_PORT(0x00409690, VehicleTriggerChannels, G, 0) { rx_vehicle_trigger_channels_create(); return m.ret(0); }
RX_PORT(0x00419700, ActivityLoadedHandlerAdd, G, 0) { rx_activity_loaded_handler_add(m.arg(0)); return m.ret(4); }
RX_PORT(0x006c8ee0, StuntSettingsRead, G, 0) { rx_stunt_settings_read(); return m.ret(0); }
RX_PORT(0x006caa80, StuntLinkSettingsRead, G, 0) { rx_stunt_link_settings_read(); return m.ret(0); }
RX_PORT(0x00549b30, EnvironmentSettingsRead, G, 0) { rx_environment_settings_read(); return m.ret(0); }
RX_PORT(0x0051c620, TagRadiiRead, G, 0) { rx_tag_radii_read(); return m.ret(0); }
// ---- the session/stats static (race_driver.c; called from race_static_dcf304 / FUN_006de1c0) ----
RX_PORT(0x006de1f0, StatsStaticConstruct, G, OUT_EAX) { m.r[0] = rx_stats_static_construct(); return m.ret(0); }
// ---- the NetworkManager's channel and the network handlers of MXGame::Startup (build_mxgame_init.c) ----
RX_PORT(0x006de4b0, NetMgrChannelInit, G, 0) { rx_netmgr_channel_init(m.r[6]); return m.ret(0); }
RX_PORT(0x007071a0, NetMgrListenEventDetails, G, 0) { rx_netmgr_listen_event_details(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00707260, NetMgrListenObjectAdded, G, 0) { rx_netmgr_listen_object_added(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x007604b0, MXGameListenSessionState, G, 0) { rx_mxgame_listen_session_state(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00760570, MXGameListenTimerEvents, G, 0) { rx_mxgame_listen_timer_events(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00554eb0, PlayerManagerListen, G, 0) { rx_player_manager_listen(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00552d20, PlayerManagerNetListen, G, 0) { rx_player_manager_net_listen(m.r[6]); return m.ret(0); }
RX_PORT(0x007558e0, DataMinerListenLogin, G, 0) { rx_data_miner_listen_login(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0075cf50, MXGameOnNetTimer, G, 0) { rx_mxgame_on_net_timer(m.r[1], m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
