// fx_mud_bind.cpp -- oracle adapters for core/game/fx_mud_bind.c (group rx_build11: the player's rider spawn in the baton boot).
#include "oracle.h"
extern "C" {
#include "../game/fx_mud_bind.h"
}
using namespace reflex_port;

// 004089d0 MudRecord_Bind (EDI = record, EAX = vehicle)
RX_PORT(0x004089d0, MudRecordBind, "rx_build11", 0) { rx_mud_record_bind(m.r[7], m.r[0]); return m.ret(0); }
// 00408fb0 MudRecord_Alloc (cdecl vehicle)
RX_PORT(0x00408fb0, MudRecordAlloc, "rx_build11", 0) { rx_mud_record_alloc(m.arg(0)); return m.ret(0); }
// 005510a0 Model_OverrideTexture (cdecl map, name, texture)
RX_PORT(0x005510a0, ModelOverrideTexture, "rx_build11", 0) { rx_model_override_texture(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0); }
// 0054bfd0 MXPlayer_BindNamePlates (EDI = player)
RX_PORT(0x0054bfd0, PlayerBindNamePlates, "rx_build11", 0) { rx_player_bind_name_plates(m.r[7]); return m.ret(0); }
// 0054a2e0 MudAndNumberCallback::vf0 (thiscall rider; RET 4)
RX_PORT(0x0054a2e0, MudAndNumberCallback_vf0, "rx_build11", 0) { rx_mud_and_number_callback(m.r[1], m.arg(0)); return m.ret(4); }
