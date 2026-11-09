// world_wreck_audio.cpp -- oracle adapters for core/game/world_wreck_audio.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_wreck.h"
}
using namespace reflex_port;

RX_PORT(0x0067ac80, CollisionKey_Less, "rx_world", OUT_EAX) { return m.ret(0, rx_world_collision_key_less(m.r[6], m.r[7]) ? 1u : 0u); }
RX_PORT(0x0068fc80, CollisionMap_IterNext, "rx_world", 0) { rx_world_collision_map_iter_next(m.r[6]); return m.ret(0); }
// 0068e130 (stdcall map, out, key; RET 0xc): EAX = out.
RX_PORT(0x0068e130, CollisionMap_Insert, "rx_world", OUT_EAX) {
    rx_world_collision_map_insert(m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc, m.arg(1));
}
RX_PORT(0x0068c360, FUN_0068c360, "rx_world", 0) { rx_world_rider_sound_slot_stop(m.arg(0), m.arg(1), m.r[5]); return m.ret(8); }
RX_PORT(0x00681840, FUN_00681840, "rx_world", 0) { rx_world_rider_sound_stop(m.r[6], m.arg(0), m.r[5]); return m.ret(0); }
RX_PORT(0x0068bfb0, FUN_0068bfb0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_rider_sound_source(m.r[7])); }
RX_PORT(0x006817c0, FUN_006817c0, "rx_world", 0) { rx_world_rider_sound_start(m.r[6], m.arg(0)); return m.ret(0); }
RX_PORT(0x0067d310, AudioTable_Name, "rx_world", OUT_EAX) { return m.ret(0, rx_world_audio_table_name(m.r[1], m.r[0])); }
RX_PORT(0x0067b890, AudioCollision_SoundName, "rx_world", OUT_EAX) { return m.ret(0, rx_world_audio_collision_sound_name(m.r[6], m.r[0])); }
// 0067c930 (EAX = record): EAX = 0 / 1, ECX kept.
RX_PORT(0x0067c930, AudioCollision_EventIsOurs, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t ecx = m.r[1];
    const int r = rx_world_audio_collision_event_is_ours(m.r[0]);
    m.r[1] = ecx;
    return m.ret(0, r ? 1u : 0u);
}
RX_PORT(0x0067bb10, AudioCollision_Add, "rx_world", 0) { rx_world_audio_collision_add(m.r[0], m.arg(0)); return m.ret(4); }
// 0067bac0 (thiscall manager; stdcall events; RET 4): ECX = the map lock's address (popped from its local).
RX_PORT(0x0067bac0, AudioCollisionManager_vf0, "rx_world", OUT_ECX) {
    const uint32_t mgr = m.r[1];
    rx_world_audio_collision_manager_vf0(mgr, m.arg(0));
    m.r[1] = mgr + 0x30;
    return m.ret(4);
}
RX_PORT(0x0067c2c0, AudioCollision_Update, "rx_world", 0) { rx_world_audio_collision_update(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x0067bd30, AudioCollision_Play, "rx_world", 0) { rx_world_audio_collision_play(m.arg(0), m.arg(1)); return m.ret(8); }
// 0067c350 (thiscall manager; stdcall record; RET 4): ECX = the security cookie its epilogue check leaves.
RX_PORT(0x0067c350, AudioCollision_UpdateRoll, "rx_world", OUT_ECX) {
    rx_world_audio_collision_update_roll(m.r[1], m.arg(0));
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4);
}
