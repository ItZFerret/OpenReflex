// fx_world.cpp -- oracle adapters for core/game/fx_world.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;
using rx_oracle::f32;

FX_PORT(0x006c1250, PopUpBale_vf19, 0) { rx_fx_popup_bale_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x006c08f0, PopDownBale_vf19, 0) { rx_fx_popdown_bale_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x006c2eb0, TrackSideFlagHolder_vf19, 0) { rx_fx_trackside_flag_tick(m.r[1], f32(m.arg(0))); return m.ret(4); }
FX_PORT(0x006d07a0, LandAnimalProximity_vf29, 0) { rx_fx_land_animal_vf29(m.r[1], f32(m.arg(0)), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
FX_PORT(0x006d2eb0, LandAnimalSteer_006d2eb0, 0) { rx_fx_land_animal_steer(m.r[6]); return m.ret(0); }
FX_PORT(0x006d7840, KeyMap_vf9, 0) { rx_fx_keymap_vf9(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006d7860, KeyMap_vf10, OUT_EAX) { m.r[0] = rx_fx_keymap_vf10(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x006c5960, EventProxyCall_006c5960, 0) { rx_fx_event_proxy_call(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x006d3d30, OnlineContentPoll_006d3d30, 0) { rx_fx_online_content_poll(m.arg(0)); return m.ret(4); }
FX_PORT(0x006d3c40, XenonOnlineContent_vf2, 0) { rx_fx_online_content_vf2(m.r[1], m.arg(0)); return m.ret(4); }
// 006d4ac0 (EDI = the block; plain ret) -> EAX = the device
FX_PORT(0x006d4ac0, PCStorageDevice_Construct, OUT_EAX) { m.r[0] = rx_fx_pc_storage_device_construct(m.r[7]); return m.ret(0); }
// 006d5600 (ECX = the storage, ESI = the event system; stdcall v; RET 4)
FX_PORT(0x006d5600, PCStorage_Enumerate, 0) { rx_fx_pc_storage_enumerate(m.r[1], m.arg(0), m.r[6]); return m.ret(4); }
// 0054de80 (stdcall player, flag; RET 8)
FX_PORT(0x0054de80, Player_LeaveSpectate, 0) { rx_player_leave_spectate(m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x006d4a70, PCStorage_vf7, 0) { rx_fx_pc_storage_vf7(m.r[1], m.arg(0)); return m.ret(4); }
FX_PORT(0x006de1c0, SessionManager_006de1c0, OUT_EAX) { m.r[0] = rx_fx_session_manager(); return m.ret(0); }
FX_PORT(0x006de690, SessionTick_006de690, 0) { rx_fx_session_tick(f32(m.arg(0)), m.r[7]); return m.ret(4); }
FX_PORT(0x006d09f0, LandAnimalProximity_vf20, 0) { rx_fx_land_animal_vf20(m.r[1]); return m.ret(0); }
FX_PORT(0x006decd0, SessionClock_006decd0, 0) { rx_fx_session_clock(m.r[0], f32(m.arg(0))); return m.ret(4); }
