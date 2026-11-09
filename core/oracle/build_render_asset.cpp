// build_render_asset.cpp -- oracle adapter for the asset release of core/game/build_asset.c (group rx_build_asset): Database::vf31 with the
// texture destruction chain under it (asset vf2, AssetIO::vf4 007d3ee0, XenonTextureAsset::sdtor 007b2620), verified per call inside the boot
// (REFLEX_PORT=rx_build_asset REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 ...: the arcade poster package is unloaded by the event).
#include "oracle.h"
extern "C" {
#include "../game/build_asset.h"
}
using namespace reflex_port;

// Database::vf31 0084af30 (thiscall db; stack: handle, package; RET 8)
RX_PORT(0x0084af30, ReleaseAsset, "rx_build_asset", 0) { rx_database_release_asset(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// Database::vf36 0084cf20 (thiscall db; ResetEvent of the event handle +0x83b48)
RX_PORT(0x0084cf20, Database_ResetEvent, "rx_build_asset", OUT_EAX) { m.r[0] = rx_database_reset_event(m.r[1]); return m.ret(0); }
// Database::vf22 0084ce00 (thiscall db; stack: handle, frame; RET 8): the handle queued for removal (w-partdtor: the damage part's model)
RX_PORT(0x0084ce00, QueueRemove, "rx_build_asset", 0) { rx_database_queue_remove(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// ---- the end-of-frame removal (build_asset_frame.c; w-frametail): Engine::Frame 0084f260 -> vf33 after the render ----
// Verified inside a baton boot (the original Engine::Frame calls vf33 every frame) and by wave_ctx_probe case "drain" (the damage
// parts' queued model handles drained after 0047bbe0).
// Database::vf33 0084cc80 (thiscall db; stack: frame; RET 4). EAX: LeaveCriticalSection's (tail jump), not read by Engine::Frame.
RX_PORT(0x0084cc80, Database_DrainRemovals, "rx_build_asset", 0) { rx_database_drain_removals(m.r[1], m.arg(0)); return m.ret(4); }
// Database::vf32 0084af90 (thiscall db; stack: handle; RET 4)
RX_PORT(0x0084af90, Database_RemoveHandle, "rx_build_asset", 0) { rx_database_remove_handle(m.r[1], m.arg(0)); return m.ret(4); }
// 0084d9c0 (EDI = the vector; RET 0)
RX_PORT(0x0084d9c0, PtrVector_Clear, "rx_build_asset", 0) { rx_ptr_vector_clear(m.r[7]); return m.ret(0); }
// AssetHandle::vf3 0082a1f0 (thiscall): EAX = +0x48
RX_PORT(0x0082a1f0, AssetHandle_Refs, "rx_build_asset", OUT_EAX) { m.r[0] = rx_asset_handle_refs(m.r[1]); return m.ret(0); }
// AssetHandle::vf8 004dce60 (thiscall): +0x48 - 1 (EAX untouched)
RX_PORT(0x004dce60, AssetHandle_Unref, "rx_build_asset", 0) { rx_asset_handle_unref(m.r[1]); return m.ret(0); }
// 0084a2c0 (thiscall): the base vtable
RX_PORT(0x0084a2c0, AssetHandle_BaseDtor, "rx_build_asset", 0) { rx_asset_handle_base_dtor(m.r[1]); return m.ret(0); }
// IAssetHandle::vf0 00668b20 / IconGeometryAssetHandle::sdtor 005e15c0 / SplitTriangleCollisionAssetHandle::sdtor 007726b0
// (thiscall handle; stack: flags; RET 4): EAX = the handle
RX_PORT(0x00668b20, IAssetHandle_Sdtor, "rx_build_asset", OUT_EAX) { m.r[0] = rx_asset_handle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005e15c0, IconGeometryHandle_Sdtor, "rx_build_asset", OUT_EAX) { m.r[0] = rx_icon_geometry_handle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x007726b0, SplitTriangleHandle_Sdtor, "rx_build_asset", OUT_EAX) { m.r[0] = rx_split_triangle_handle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
