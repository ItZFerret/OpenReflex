// anim_rider_stunt.cpp -- oracle adapters for core/game/anim_rider_stunt.c (group "rx_fx", agent "trickstart").
#include "fx_oracle.h"
extern "C" {
#include "../game/anim_rider_stunt.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

/* 0054d120 (ECX = the player; stdcall player, trick; RET 8) */
FX_PORT(0x0054d120, RiderStuntBegin_0054d120, 0) { rx_rider_stunt_begin_evt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x0054d220, RiderStuntCompleted_0054d220, 0) { rx_rider_stunt_completed_evt(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
/* 006226c0 (EDI = the rider animation; stdcall ra, player, trick; RET 0xc) */
FX_PORT(0x006226c0, RiderStuntClip_006226c0, 0) { rx_rider_stunt_clip(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
/* 00622bc0 (ECX = the trick; stdcall ra, player; RET 8) */
FX_PORT(0x00622bc0, RiderStuntAir_00622bc0, 0) { rx_rider_stunt_air(m.arg(0), m.arg(1), m.r[1]); return m.ret(8); }
/* 00622790 (EDI = the rider animation; stdcall airborne, clip byte; RET 8) */
FX_PORT(0x00622790, RiderStuntGround_00622790, 0) { rx_rider_stunt_ground(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
/* 0054d3b0 (EAX = the player; stdcall trick; RET 4) */
FX_PORT(0x0054d3b0, RiderStuntControls_0054d3b0, 0) { rx_rider_stunt_controls(m.r[0], m.arg(0)); return m.ret(4); }
/* 006ccfc0 (EAX = the vehicle class; EDI = the destination, ESI = the size; stack the source; RET 0) */
FX_PORT(0x006ccfc0, RiderStuntName_006ccfc0, 0) { rx_rider_stunt_name(m.r[7], m.r[6], m.r[0], m.arg(0)); return m.ret(0); }
/* 0063b6e0 / 00850cd0 (EAX = the key, EDI = the map; RET 4) */
FX_PORT(0x0063b6e0, RiderStuntMapU16_0063b6e0, OUT_EAX) { m.r[0] = (uint32_t)rx_rider_stunt_map_u16(m.r[7], m.r[0]); return m.ret(4); }
FX_PORT(0x00850cd0, RiderStuntMapU32_00850cd0, OUT_EAX) { m.r[0] = (uint32_t)rx_rider_stunt_map_u32(m.r[7], m.r[0]); return m.ret(4); }
/* 0063b2b0 / 0063b1e0 (stdcall vector, id; RET 8) */
FX_PORT(0x0063b2b0, RiderStuntListenerDisable_0063b2b0, 0) { rx_rider_stunt_listener_disable(m.arg(0), m.arg(1)); return m.ret(8); }
FX_PORT(0x0063b1e0, RiderStuntListenerSelect_0063b1e0, 0) { rx_rider_stunt_listener_select(m.arg(0), m.arg(1)); return m.ret(8); }
/* 0063c190 (stack: out*, value*, proxy, begin, proxy2, end; plain RET: the caller does add esp, 0x18) */
FX_PORT(0x0063c190, RiderListenersBroadcast_0063c190, 0) {
    rx_rider_listeners_broadcast(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5)); return m.ret(0);
}
/* 0063b4b0 (EAX = the clamped float; stdcall value*; RET 4) */
FX_PORT(0x0063b4b0, RiderClampedFloatAdd_0063b4b0, 0) { rx_rider_clamped_float_add(m.r[0], m.arg(0)); return m.ret(4); }
/* 0063b7d0 (EAX = the listener vector; stdcall value*; RET 4) */
FX_PORT(0x0063b7d0, RiderListenersBroadcastVec_0063b7d0, 0) {
    rx_rider_listeners_broadcast_vec(m.r[0], m.arg(0)); return m.ret(4);
}
