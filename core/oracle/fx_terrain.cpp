// fx_terrain.cpp -- oracle adapters for core/game/fx_terrain.c (group "rx_fx").
#include "fx_oracle.h"
extern "C" {
#include "../game/fx.h"
}
using namespace reflex_port;

FX_PORT(0x007cc630, TerrainTracks_Reset, 0) { rx_fx_tracks_reset(m.r[0]); return m.ret(0); }
FX_PORT(0x007cbe30, TerrainTracks_FlushPending, 0) { rx_fx_tracks_flush(m.r[0]); return m.ret(0); }
FX_PORT(0x007cb930, DeformablePatch_ReadbackToSysmem, 0) { rx_fx_patch_readback(m.r[6]); return m.ret(0); }
FX_PORT(0x007c4fa0, HFTerrainSurface_vf14, 0) { rx_fx_terrain_sync(m.r[1]); return m.ret(0); }
FX_PORT(0x00402d90, TerrainRegistry_00402d90, OUT_EAX) { m.r[0] = rx_fx_terrain_registry(); return m.ret(0); }
FX_PORT(0x00408c50, MudRecord_Update, 0) { rx_fx_mud_update(m.r[6]); return m.ret(0); }
