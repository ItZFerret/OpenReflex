// world_node_audio.cpp -- oracle adapters for core/game/world_node_audio.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_node.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x0080cd90, FUN_0080cd90, "rx_world", OUT_EAX) { return m.ret(0, rx_world_vector_last(m.r[0])); }
RX_PORT(0x0080cdf0, FUN_0080cdf0, "rx_world", 0) { rx_world_vector_push_back(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x0080c440, FUN_0080c440, "rx_world", OUT_EAX) { return m.ret(0, rx_world_event_pool_take(m.r[6])); }
RX_PORT(0x0080c400, FUN_0080c400, "rx_world", 0) { rx_world_event_pool_give(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0080c280, FUN_0080c280, "rx_world", 0) { rx_world_event_queue(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.r[5]); return m.ret(0); }
RX_PORT(0x0080a990, FUN_0080a990, "rx_world", 0) {
    rx_world_event_set_callback(m.r[6], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    return m.ret(0x18);
}
RX_PORT(0x0080a810, FUN_0080a810, "rx_world", 0) { rx_world_event_release_instance(m.r[6], m.r[5]); return m.ret(0); }
RX_PORT(0x0080a770, FUN_0080a770, "rx_world", 0) { rx_world_event_stop(m.r[7]); return m.ret(0); }
RX_PORT(0x0080c970, FUN_0080c970, "rx_world", 0) { rx_world_event_pool_return(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0080c810, FUN_0080c810, "rx_world", 0) { rx_world_event_release(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0080bce0, AudioEvent_SetChannel, "rx_world", 0) { rx_world_event_set_channel(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0080c760, FUN_0080c760, "rx_world", OUT_EAX) { return m.ret(8, rx_world_event_create(m.r[0], m.arg(0), m.arg(1))); }
RX_PORT(0x0080a9f0, FUN_0080a9f0, "rx_world", 0) { rx_world_event_start(m.r[6], m.r[5]); return m.ret(0); }
RX_PORT(0x0080b460, FUN_0080b460, "rx_world", 0) { rx_world_event_apply_volume(m.r[7], m.r[5]); return m.ret(0); }
RX_PORT(0x0080abd0, AudioEvent_Play, "rx_world", 0) { rx_world_event_play(m.r[6], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0080b500, AudioEvent_IsPlaying, "rx_world", OUT_EAX) { return m.ret(0, rx_world_event_is_playing(m.r[1])); }
RX_PORT(0x0080b310, FUN_0080b310, "rx_world", 0) { rx_world_event_set_range(m.r[0], m.arg(0), m.arg(1), m.r[5]); return m.ret(8); }
RX_PORT(0x0080ae80, FUN_0080ae80, "rx_world", 0) { rx_world_event_set_position(m.r[7], m.arg(0), m.r[5]); return m.ret(4); }
RX_PORT(0x0080afc0, FUN_0080afc0, "rx_world", 0) { rx_world_event_set_parameter(m.r[7], m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x0080bc80, FMODEvent_vf1, "rx_world", 0) { rx_world_fmod_event_vf1(m.r[1], m.arg(0), m.arg(1), m.r[5]); return m.ret(8); }
RX_PORT(0x00805570, FUN_00805570, "rx_world", 0) { rx_world_audio_log(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x0080da40, FUN_0080da40, "rx_world", OUT_EAX) {
    return m.ret(0x10, rx_world_map_node_new(m.arg(0), m.arg(1), m.arg(2), m.arg(3)));
}
RX_PORT(0x00803ed0, FUN_00803ed0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_audio_queue_pop()); }
RX_PORT(0x007a0760, rtti_shared_vf_007a0760, "rx_world", OUT_EAX) { return m.ret(0, rx_world_shared_vf_ret0()); }
RX_PORT(0x007e3be0, rtti_shared_vf_007e3be0, "rx_world", 0) { rx_world_shared_vf_empty(); return m.ret(0); }
RX_PORT(0x00819cc0, CollisionEntity_vf13, "rx_world", OUT_EAX) { return m.ret(0, rx_world_collision_entity_vf13(m.r[1])); }
// 008b98c0 (cdecl out): EAX = ECX = out.
RX_PORT(0x008b98c0, FUN_008b98c0, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t out = rx_world_int64_from_counter(m.arg(0));
    m.r[1] = out;
    return m.ret(0, out);
}
RX_PORT(0x008519a0, FUN_008519a0, "rx_world", OUT_EAX) { return m.ret(4, rx_world_string_map_lower_bound(m.r[0], m.arg(0))); }
RX_PORT(0x00830130, FUN_00830130, "rx_world", OUT_EAX) { return m.ret(4, rx_world_object_query_248(m.r[1], m.arg(0))); }
RX_PORT(0x00805860, Audio_PostCommand, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_audio_post_command(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)));
}
RX_PORT(0x00805ff0, Audio_ProcessQueue, "rx_world", 0) { rx_world_audio_process_queue(); return m.ret(0); }
RX_PORT(0x00805420, Audio_NearestListener, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_audio_nearest_listener(m.arg(0), m.arg(1)));
}
// 0080c2c0 (ECX = command, ESI = record, EAX = a, EDI = b; RET 0). The caller reloads every register it uses.
RX_PORT(0x0080c2c0, FUN_0080c2c0, "rx_world", 0) { rx_world_audio_command_args(m.r[1], m.r[6], m.r[0], m.r[7]); return m.ret(0); }
RX_PORT(0x0080b160, FUN_0080b160, "rx_world", OUT_EAX) { return m.ret(0, rx_world_event_parameter_range(m.r[6], m.arg(0), m.arg(1))); }
RX_PORT(0x00892170, FUN_00892170, "rx_world", OUT_EAX) { return m.ret(0, rx_world_clock_now(m.arg(0), m.arg(1))); }
// 00892120 (cdecl out; EAX = out): the clock under the clock lock, may_step = 1
RX_PORT(0x00892120, FUN_00892120, "rx_world", OUT_EAX) { return m.ret(0, rx_world_clock_now_locked(m.arg(0))); }
