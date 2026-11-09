// build_render_asset_surface.cpp -- oracle adapter for core/game/build_render_asset_surface.c: XenonSurfaceAsset::vf4 (reached by
// VehicleLoader_Assemble's per-element surface lookup; group rx_build_rider).
#include "oracle.h"
extern "C" {
#include "../game/build_render_asset.h"
}
using namespace reflex_port;

// 00794f90 XenonSurfaceAsset::vf4 (thiscall index; RET 4) -> EAX
RX_PORT(0x00794f90, SurfaceAssetVf4, "rx_build_rider", OUT_EAX) { m.r[0] = rx_surface_asset_get(m.r[1], m.arg(0)); return m.ret(4); }
