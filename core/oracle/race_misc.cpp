// race_misc.cpp -- oracle adapters for core/game/race_misc_*.c.
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_misc.h"
}
using namespace reflex_port;

// x87 returns: the value pushed on the emulated x87 stack (OUT_FPU).
RX_PORT(0x004669f0, Moto_EngineRpm, "rx_race", OUT_EAX | OUT_FPU) {
    const double r = rx_moto_engine_rpm(m.r[1]);
    uint64_t b; std::memcpy(&b, &r, 8);
    m.fpu.push_back(reflex_f80::from_f64(b));
    m.r[0] = U32(m.r[1], 0x16d4);
    return m.ret(0);
}
RX_PORT(0x00466a10, Moto_vf48, "rx_race", OUT_EAX | OUT_FPU) {
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_moto_vf48(m.r[1]))));
    m.r[0] = U32(m.r[1], 0x16d4);
    return m.ret(0);
}
RX_PORT(0x00466b30, Moto_vf49, "rx_race", OUT_EAX | OUT_ECX) {
    const uint32_t v = m.r[1], a = m.arg(0);
    rx_moto_vf49(v, a); m.r[0] = U32(v, 0x16d4); m.r[1] = a;
    return m.ret(4);
}
RX_PORT(0x00466b50, Moto_vf50, "rx_race", OUT_EAX | OUT_ECX) {
    const uint32_t v = m.r[1];
    m.r[0] = rx_moto_vf50(v); m.r[1] = U32(U32(v, 0x16d4), 0x5c30);
    return m.ret(0);
}
RX_PORT(0x00462290, Moto_vf46, "rx_race", OUT_EAX) { m.r[0] = rx_moto_vf46(m.r[1]); return m.ret(0); }
RX_PORT(0x00466b00, Moto_vf45, "rx_race", OUT_EAX) { m.r[0] = rx_moto_vf45(m.r[1]); return m.ret(0); }
RX_PORT(0x007f1110, BehaviorFixProgress_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_fix_progress_type(m.r[1]); return m.ret(0); }
RX_PORT(0x0089e360, BehaviorPerformStunt_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_perform_stunt_type(m.r[1]); return m.ret(0); }
RX_PORT(0x008a5dc0, BehaviorAirLeveling_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_air_leveling_type(m.r[1]); return m.ret(0); }
RX_PORT(0x008ac4a0, BehaviorSeperate_Type, "rx_race", OUT_EAX) { m.r[0] = rx_behavior_seperate_type(m.r[1]); return m.ret(0); }

// 00755d30 (ECX = key) -> EAX; the original leaves ECX = the value before the last step, EDX = the third step's value >> 6.
RX_PORT(0x00755d30, IntHash_Wang32, "rx_race", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t key = m.r[1];
    uint32_t k = key + ~(key << 15);
    k = (k ^ (k >> 10)) * 9;
    m.r[2] = k >> 6;
    k = k ^ (k >> 6);
    m.r[1] = k + ~(k << 11);
    m.r[0] = rx_int_hash_wang32(key);
    return m.ret(0);
}
RX_PORT(0x00755d60, Game_ModeIndex, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_game_mode_index(m.arg(0))); return m.ret(0); }
RX_PORT(0x008a7310, Stat4_Value, "rx_race", OUT_EAX) { m.r[0] = rx_stat4_value(); return m.ret(0); }
RX_PORT(0x0075c5f0, Game_IsViewTrack, "rx_race", OUT_EAX | OUT_EDX) { m.r[0] = rx_game_is_view_track(m.r[1]); m.r[2] = m.r[0]; return m.ret(0); }
RX_PORT(0x00757f90, GameTickDuringSync, "rx_race", 0) { rx_game_tick_during_sync(m.r[1]); return m.ret(0); }
// 00892090 (ECX = a, arg 0 = b; RET 4) -> AL (EAX's upper bytes from [a]), ECX = the result.
RX_PORT(0x00892090, Handle_Equal, "rx_race", OUT_EAX | OUT_ECX) {
    const uint32_t a = m.r[1], eq = rx_handle_equal(a, m.arg(0));
    m.r[0] = (U32(a, 0) & 0xffffff00u) | eq; m.r[1] = eq; m.r[2] = m.arg(0);
    return m.ret(4);
}
RX_PORT(0x007fa6a0, Field14, "rx_race", OUT_EAX) { m.r[0] = rx_field_14(m.r[0]); return m.ret(0); }
RX_PORT(0x007fa6c0, FieldGet4, "rx_race", OUT_EAX) { m.r[0] = rx_field_get4(m.r[0]); return m.ret(0); }
RX_PORT(0x007f9e40, Array20_Find, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_array20_find(m.r[1], m.arg(0))); return m.ret(4); }
RX_PORT(0x007f9ee0, Array20_Remove, "rx_race", 0) { rx_array20_remove(m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x007f9f80, Array40_Find, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_array40_find(m.r[0], m.arg(0))); return m.ret(4); }
// 007f9140 (ESI = vector, EDI = out, args (unused, position); RET 8) -> EAX = out; ECX = position.
RX_PORT(0x007f9140, Vector_Erase, "rx_race", OUT_EAX | OUT_ECX) { m.r[0] = rx_vector_erase(m.r[6], m.r[7], m.arg(1)); m.r[1] = m.arg(1); return m.ret(8); }
// 008953c0 -> AL.
RX_PORT(0x008953c0, Net_SessionState, "rx_race", OUT_EAX) {
    const uint32_t r = rx_net_session_state();       /* EAX's upper bytes: as the last original call (0088bac0) left them */
    m.r[0] = (m.r[0] & 0xffffff00u) | r;
    return m.ret(0);
}
RX_PORT(0x00460020, Vehicle_EffectQuery, "rx_race", 0) { rx_vehicle_effect_query(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0047c3f0, Pose_ClearAll, "rx_race", 0) { rx_pose_overrides_clear(); return m.ret(0); }
RX_PORT(0x0047c0b0, Pose_Start, "rx_race", 0) { rx_pose_override_start(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0047c310, Pose_Set, "rx_race", 0) { rx_pose_override_set(m.r[2], m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x0047c190, Pose_Update, "rx_race", 0) { rx_pose_override_update(m.r[1], rx_oracle::f32(uint32_t(m.xmm[0].lo))); return m.ret(0); }
RX_PORT(0x0047c2a0, Pose_UpdateAll, "rx_race", 0) { rx_pose_overrides_update(rx_oracle::f32(m.arg(0))); return m.ret(0); }
RX_PORT(0x0047bc60, EffectList_Update, "rx_race", 0) { rx_effect_list_update(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
struct MiscFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit MiscFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~MiscFpuKeep() { m.fpu = fpu; } };
RX_PORT(0x00466de0, Moto_WheelReport, "rx_race", 0) { rx_moto_wheel_report(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00467070, Moto_StateReport, "rx_race", 0) { MiscFpuKeep k(m); rx_moto_state_report(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00466340, Moto_SafePoint, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_moto_safe_point(m.r[1], m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2))); return m.ret(0x10); }
// 00461980 (ECX = time out, EDX = vehicle; args: point out, normal out; RET 8) -> EAX.
RX_PORT(0x00461980, Landing_Predict, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_landing_predict(m.r[1], m.r[2], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004664f0, Moto_ResetGround, "rx_race", 0) { MiscFpuKeep k(m); rx_moto_reset_ground(m.r[1]); return m.ret(0); }
RX_PORT(0x00462130, SpawnPoint_Clear, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_spawn_point_clear(m.arg(0), m.r[0]); return m.ret(4); }
RX_PORT(0x0075ec80, Game_PlayerMode, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_game_player_mode(); return m.ret(0); }
RX_PORT(0x00759d60, Game_DebugReload, "rx_race", 0) { MiscFpuKeep k(m); rx_game_debug_reload(m.r[7]); return m.ret(0); }
RX_PORT(0x007fa250, NameHash_Find, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_name_hash_find(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x007fa1e0, NameHash_Free, "rx_race", 0) { MiscFpuKeep k(m); rx_name_hash_free(m.r[0]); return m.ret(0); }
// ECX is not an output: the only caller (SceneManager_Tick 007d23f4) overwrites it at 007d2426 before reading it, and the value the
// original leaves is the CRT's residue after memmove_s inside 00776d20 (4 in the waypoint championship world, call 2327).
RX_PORT(0x007f8fa0, Vector_Assign, "rx_race", OUT_EAX) { MiscFpuKeep k(m); uint32_t c = 0; m.r[0] = rx_vector_assign(m.r[0], m.arg(0), m.r[1], &c); m.r[1] = c; return m.ret(4); }
RX_PORT(0x007ea020, Scene_PatchHeight, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_scene_patch_height(m.arg(0), m.arg(1), m.arg(2), m.arg(3), rx_oracle::f32(m.arg(4)), rx_oracle::f32(m.arg(5))); return m.ret(0x18); }
RX_PORT(0x007f0050, Scene_GroundProbe, "rx_race", OUT_EAX) { MiscFpuKeep k(m); m.r[0] = rx_scene_ground_probe(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x007532e0, Records_Update, "rx_race", 0) { MiscFpuKeep k(m); rx_records_update(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
// 008220e0 IMemoryAllocator::vf0 (scalar deleting destructor; ECX = the object, stack the flag; RET 4)
extern "C" {
#include "../game/build_alloc.h"
}
RX_PORT(0x008220e0, MemoryAllocator_Dtor, "rx_race", OUT_EAX) { m.r[0] = rx_memory_allocator_dtor(m.r[1], m.arg(0)); return m.ret(4); }
// 0051dcd0 (EAX = the driver) -> xmm0: the rider's start reaction chance. 0050fb00 RaceDriver::vf5 (ECX = the driver).
extern "C" {
#include "../game/race_driver.h"
#include "../game/crt.h"
}
RX_PORT(0x0051dcd0, Driver_StartChance, "rx_race", reflex_port::OUT_XMM0) {
    const float f = rx_driver_start_chance(m.r[0]);
    m.xmm[0].lo = rx_oracle::bits(f); m.xmm[0].hi = 0;
    return m.ret(0);
}
RX_PORT(0x0050fb00, Driver_vf5, "rx_race", 0) { rx_driver_vf5(m.r[1]); return m.ret(0); }
// 007f9570 (EDI = the count, ESI = the vector; EAX = 1)
RX_PORT(0x007f9570, Vector_StorageAlloc, "rx_race", OUT_EAX) { m.r[0] = rx_vector_storage_alloc(m.r[6], m.r[7]); return m.ret(0); }
// 007fa2b0 (EAX = end, EDI = begin, EDX = destination) -> EAX = destination + the copied bytes
RX_PORT(0x007fa2b0, Vector_MoveRange, "rx_race", OUT_EAX) { m.r[0] = rx_vector_move_range(m.r[0], m.r[7], m.r[2]); return m.ret(0); }
