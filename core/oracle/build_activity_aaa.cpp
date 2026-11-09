// build_activity_aaa.cpp -- oracle adapters (group rx_build8, MXGame's unit) for core/game/build_activity_aaa.c (the activity area's
// re-creation: game states 9 / 10), loop_player_destroy.c (MXPlayer's destruction) and the asset destruction a package unload runs
// (build_asset_release.c, build_render_asset_surface.c).
#include "oracle.h"
#include "fx_oracle.h"     /* FxBind: the register model of the asset destructors' frames (build_render_asset_surface.c) */
#include "anim_oracle.h"   /* AnimBind: ModelInstance_Destroy 007d4030's (anim_scene.c) */
extern "C" {
#include "../game/build_activity.h"
#include "../game/loop_player_destroy.h"
#include "../game/build_render_asset.h"
}
using namespace reflex_port;
#define G "rx_build8"

extern "C" {
rx_addr rx_model_asset_sdtor(rx_addr asset, uint32_t flags);
rx_addr rx_model_asset_get_io(rx_addr asset);
void rx_triangle_surface_destroy(rx_addr t);
rx_addr rx_triangle_surface_sdtor(rx_addr t, uint32_t flags);
void rx_surface_asset_destroy(rx_addr a);
rx_addr rx_surface_asset_sdtor(rx_addr a, uint32_t flags);
rx_addr rx_surface_asset_get_io(rx_addr a);
}

// ---- the activity area ----
// 0075a380 Game_StartAAACreation (EAX = activity type, ECX = activity mode)
RX_PORT(0x0075a380, GameStartAAACreation, G, 0) { rx_game_start_aaa_creation(m.r[0], m.r[1]); return m.ret(0); }
// 0075a3d0 the job (cdecl game) -> EAX 0
RX_PORT(0x0075a3d0, GameAAACreationJob, G, OUT_EAX) { m.r[0] = rx_game_aaa_creation_job(m.arg(0)); return m.ret(0); }
// 007663d0 / 00766420 / 007665b0 / 00766e70 (stdcall world; RET 4)
RX_PORT(0x007663d0, WorldActivitiesSuspend, G, 0) { rx_world_activities_suspend(m.arg(0)); return m.ret(4); }
RX_PORT(0x00766420, WorldRecreateActivities, G, 0) { rx_world_recreate_activities(m.arg(0)); return m.ret(4); }
RX_PORT(0x007665b0, WorldActivitiesRestart, G, 0) { rx_world_activities_restart(m.arg(0)); return m.ret(4); }
RX_PORT(0x00766e70, WorldPlayersSpawn, G, 0) { rx_world_players_spawn(m.arg(0)); return m.ret(4); }

// ---- MXPlayer's destruction ----
// 005497e0 MXPlayer::sdtor (thiscall; flags; RET 4) -> EAX
RX_PORT(0x005497e0, MXPlayerSdtor, G, OUT_EAX) { m.r[0] = rx_mxplayer_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00549830 (stdcall player; RET 4)
RX_PORT(0x00549830, MXPlayerDestroy, G, 0) { rx_mxplayer_destroy(m.arg(0)); return m.ret(4); }
// 005549f0 / 00554b10 / 00554b70 (EAX = name, ECX = registry; player, member; RET 8)
RX_PORT(0x005549f0, PlayerUnlistenNetworkObject, G, 0) { rx_player_unlisten_network_object(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00554b10, PlayerUnlistenStunt, G, 0) { rx_player_unlisten_stunt(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00554b70, PlayerUnlistenPhysicsStunt, G, 0) { rx_player_unlisten_physics_stunt(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 00554a50 / 00554ab0 (EAX = registry; player; RET 4)
RX_PORT(0x00554a50, PlayerUnlistenStuntUpdate, G, 0) { rx_player_unlisten_stunt_update(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00554ab0, PlayerUnlistenVehicleReset, G, 0) { rx_player_unlisten_vehicle_reset(m.r[0], m.arg(0)); return m.ret(4); }
// 00554bd0 (stdcall player; RET 4)
RX_PORT(0x00554bd0, PlayerUnlistenVehicleEntered, G, 0) { rx_player_unlisten_vehicle_entered(m.arg(0)); return m.ret(4); }
// 006cdbd0 / 006cdc90 / 006cdcf0 (EAX = name, ECX = registry; system, member; RET 8)
RX_PORT(0x006cdbd0, StuntUnlisten, G, 0) { rx_stunt_unlisten(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006cdc90, StuntUnlistenPhysics, G, 0) { rx_stunt_unlisten_physics(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006cdcf0, StuntUnlistenVehicle, G, 0) { rx_stunt_unlisten_vehicle(m.r[0], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 006cdc30 (EAX = registry; system; RET 4)
RX_PORT(0x006cdc30, StuntUnlistenCombo, G, 0) { rx_stunt_unlisten_combo(m.r[0], m.arg(0)); return m.ret(4); }
// 006cb010 (EDI = the stunt system)
RX_PORT(0x006cb010, StuntSystemUnregister, G, 0) { rx_stunt_system_unregister(m.r[7]); return m.ret(0); }
// 005ba710 (ESI = the set)
RX_PORT(0x005ba710, StuntSetDestroy, G, 0) { rx_stunt_set_destroy(m.r[6]); return m.ret(0); }

// ---- the assets a package unload destroys ----
// 007d4010 ModelAsset::sdtor / 007b2040 XenonSurfaceAsset::sdtor / 007b21f0 XenonTriangleSurface::sdtor (thiscall; flags; RET 4) -> EAX
RX_PORT(0x007d4010, ModelAssetSdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_model_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007b2040, SurfaceAssetSdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_surface_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007b21f0, TriangleSurfaceSdtor, G, OUT_EAX) { FxBind fb_(m); AnimBind ab_(m); m.r[0] = rx_triangle_surface_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 007b2060 / 007b22b0 the destructors (ECX = the object)
RX_PORT(0x007b2060, SurfaceAssetDestroy, G, 0) { FxBind fb_(m); AnimBind ab_(m); rx_surface_asset_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x007b22b0, TriangleSurfaceDestroy, G, 0) { FxBind fb_(m); AnimBind ab_(m); rx_triangle_surface_destroy(m.r[1]); return m.ret(0); }
// 007d40b0 / 007b20c0 the AssetIO getters (thiscall asset) -> EAX
RX_PORT(0x007d40b0, ModelAssetGetIO, G, OUT_EAX) { m.r[0] = rx_model_asset_get_io(m.r[1]); return m.ret(0); }
RX_PORT(0x007b20c0, SurfaceAssetGetIO, G, OUT_EAX) { m.r[0] = rx_surface_asset_get_io(m.r[1]); return m.ret(0); }
// XenonShaderAsset: 007b0f90 sdtor (thiscall; flags; RET 4) -> EAX, 007b13d0 the destructor (ECX), 00794720 vf2 the AssetIO -> EAX
RX_PORT(0x007b0f90, ShaderAssetSdtor, G, OUT_EAX) { m.r[0] = rx_shader_asset_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007b13d0, ShaderAssetDestroy, G, 0) { rx_shader_asset_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x00794720, ShaderAssetGetIO, G, OUT_EAX) { m.r[0] = rx_shader_asset_get_io(m.r[1]); return m.ret(0); }
