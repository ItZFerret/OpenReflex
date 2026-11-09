// race_challenge_activity.cpp -- oracle adapters for core/game/race_challenge_activity.c (the Free Ride challenge activities, their
// driver and boundary rule, the shared Activity methods). Groups: the constructors "rx_build11" (built inside the boot: verified per
// call in a baton boot of a challenge event), the run-time methods "rx_race". ABI glue only.
#include "race_oracle.h"
#include "f80/f80.h"
#include <cstdlib>
extern "C" {
#include "../game/race_challenge_activity.h"
}
using namespace reflex_port;

namespace {
// the x87 stack as the original leaves it for a void port (the core functions keep their x87 values in C)
struct ChFpuKeep {
    reflex_vm::Machine& m;
    std::vector<reflex_f80::F80> fpu;
    explicit ChFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {}
    ~ChFpuKeep() { m.fpu = fpu; }
};
inline float argf(reflex_vm::Machine& m, int i) { return rx_oracle::f32(m.arg(i)); }
// an x87 float return (fld dword: a signalling NaN arrives quiet)
inline void push_f32(reflex_vm::Machine& m, float f) {
    uint32_t b = rx_oracle::bits(f);
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) != 0) b |= 0x00400000u;
    m.fpu.push_back(reflex_f80::from_f32(b));
}
inline void set_xmm0_ss(reflex_vm::Machine& m, float f) {
    m.xmm[0].lo = (m.xmm[0].lo & 0xffffffff00000000ull) | rx_oracle::bits(f);
}
}  // namespace

#define GB "rx_build11"
#define GR "rx_race"

// ---- construction ----
// 00536ca0 (ESI = block) -> EAX = block
RX_PORT(0x00536ca0, TrackBlockConstruct, GB, OUT_EAX) { rx_track_block_construct(m.r[6]); m.r[0] = m.r[6]; return m.ret(0); }
// 00584e50 (ESI = logic) -> EAX
RX_PORT(0x00584e50, ChallengeAudioLogicConstruct, GB, OUT_EAX) { m.r[0] = rx_challenge_audio_logic_construct(m.r[6]); return m.ret(0); }
// 0057aa30 (EAX = world, ECX = type; stdcall A; RET 4) -> EAX
RX_PORT(0x0057aa30, ChallengeConstruct, GB, OUT_EAX) { m.r[0] = rx_challenge_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
// 0057b740 / 0057c3e0 (ESI = A, EAX = world, ECX = type) -> EAX
RX_PORT(0x0057b740, PathfinderConstruct, GB, OUT_EAX) { m.r[0] = rx_pathfinder_construct(m.r[6], m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x0057c3e0, StepUpConstruct, GB, OUT_EAX) { m.r[0] = rx_step_up_construct(m.r[6], m.r[1], m.r[0]); return m.ret(0); }
// 0058a7b0 / 0058a870 (stdcall A; RET 4)
RX_PORT(0x0058a7b0, TrailBlazerListen, GB, 0) { rx_trail_blazer_listen(m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a870, TrailBlazerUnlisten, GR, 0) { rx_trail_blazer_unlisten(m.arg(0)); return m.ret(4); }

// ---- MXActivityChallenge ----
RX_PORT(0x0057aab0, Challenge_Destroy, GR, 0) { ChFpuKeep k(m); rx_challenge_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x0057aa90, Challenge_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_challenge_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b240, HillClimb_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_hill_climb_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0055eca0, BigAir_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_big_air_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d370, TrailBlazer_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_trail_blazer_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d500, TargetPractice_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_target_practice_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 0057b1a0 (stdcall A; RET 4)
RX_PORT(0x0057b1a0, Challenge_LoadTrack, GB, 0) { ChFpuKeep k(m); rx_challenge_load_track(m.arg(0)); return m.ret(4); }
RX_PORT(0x0057ab00, Challenge_vf2, GB, 0) { ChFpuKeep k(m); rx_challenge_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d530, TargetPractice_vf2, GB, 0) { ChFpuKeep k(m); rx_target_practice_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057abd0, Challenge_vf4, GB, 0) { ChFpuKeep k(m); rx_challenge_vf4(m.r[1]); return m.ret(0); }
// 0057ac50 (EAX = kind) -> EAX
RX_PORT(0x0057ac50, Challenge_StartType, GR, OUT_EAX) { m.r[0] = rx_challenge_start_type(m.r[0]); return m.ret(0); }
RX_PORT(0x0057acc0, Challenge_vf61, GB, 0) { ChFpuKeep k(m); rx_challenge_vf61(m.r[1]); return m.ret(0); }
RX_PORT(0x0057ae60, Challenge_vf3, GR, 0) { ChFpuKeep k(m); rx_challenge_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057ae90, Challenge_vf18, GR, 0) { ChFpuKeep k(m); rx_challenge_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057aeb0, Challenge_vf44, GB, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_challenge_vf44(m.r[1], m.arg(0)); return m.ret(4); }
// 0058a570 (ECX = driver) -> EAX
RX_PORT(0x0058a570, Challenge_RuleGet, GB, OUT_EAX) { m.r[0] = rx_challenge_rule_get(m.r[1]); return m.ret(0); }
RX_PORT(0x0057aef0, Challenge_vf47, GB, 0) { ChFpuKeep k(m); rx_challenge_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057af10, Challenge_vf38, GR, 0) { ChFpuKeep k(m); rx_challenge_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057af30, Challenge_vf39, GR, 0) { ChFpuKeep k(m); rx_challenge_vf39(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b020, Challenge_vf40, GR, 0) { ChFpuKeep k(m); rx_challenge_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057af40, Challenge_vf23, GR, 0) { ChFpuKeep k(m); rx_challenge_vf23(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057d750, Challenge_UpdateTail, GR, 0) { ChFpuKeep k(m); rx_challenge_update_tail(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057af90, Challenge_vf33, GR, 0) { ChFpuKeep k(m); rx_challenge_vf33(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057afc0, Challenge_vf15, GB, 0) { ChFpuKeep k(m); rx_challenge_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x0057afe0, Challenge_vf16, GR, 0) { ChFpuKeep k(m); rx_challenge_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0057b030, Challenge_vf27, GR, 0) { ChFpuKeep k(m); rx_challenge_vf27(m.r[1], argf(m, 0)); return m.ret(4); }
// 0058a5d0 (ECX = driver, EDX = medal 2, ESI = channel; stdcall A, medal 1, medal 0; RET 0xc)
RX_PORT(0x0058a5d0, Challenge_AwardRaise, GR, 0) { ChFpuKeep k(m); rx_challenge_award_raise(m.r[6], m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057b050, Challenge_vf41, GR, 0) { ChFpuKeep k(m); rx_challenge_vf41(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b140, Challenge_vf52, GR, 0) { rx_challenge_vf52(m.r[1]); m.r[0] = 0xffffffffu; return m.ret(0); }
RX_PORT(0x0057c2f0, Challenge_vf32, GR, 0) { ChFpuKeep k(m); rx_challenge_vf32(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057c220, Challenge_vf1, GB, 0) { ChFpuKeep k(m); rx_challenge_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005189f0, Challenge_ValueZero, GR, OUT_FPU) { push_f32(m, rx_challenge_value_zero(m.r[1])); return m.ret(0); }
// 0057b160 (ECX = A, ESI = medal) -> xmm0
RX_PORT(0x0057b160, Challenge_MedalValue, GR, OUT_XMM0) { set_xmm0_ss(m, rx_challenge_medal_value(m.r[1], m.r[6])); return m.ret(0); }

// ---- the shared Activity methods ----
RX_PORT(0x00794f60, Activity_vf19, GR, OUT_EAX) { m.r[0] = rx_activity_vf19(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00559a80, Activity_vf58, GR, 0) { ChFpuKeep k(m); rx_activity_vf58(m.r[1]); return m.ret(0); }

// ---- HillClimb / BigAir / TrailBlazer / TargetPractice ----
RX_PORT(0x0057b510, HillClimb_vf1, GB, 0) { ChFpuKeep k(m); rx_hill_climb_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b320, HillClimb_vf18, GR, 0) { ChFpuKeep k(m); rx_hill_climb_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057b270, HillClimb_vf28, GR, 0) { ChFpuKeep k(m); rx_hill_climb_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057b470, HillClimb_vf42, GR, 0) { ChFpuKeep k(m); rx_hill_climb_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x0055ed00, HillClimb_Value, GR, OUT_FPU) { push_f32(m, rx_hill_climb_value(m.r[1])); return m.ret(0); }
RX_PORT(0x0057b370, HillClimb_vf64, GR, 0) { rx_hill_climb_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057b5e0, BigAir_vf18, GR, 0) { ChFpuKeep k(m); rx_big_air_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057b550, BigAir_vf28, GR, 0) { ChFpuKeep k(m); rx_big_air_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057b620, BigAir_vf64, GR, 0) { rx_big_air_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057d3c0, TrailBlazer_vf18, GR, 0) { ChFpuKeep k(m); rx_trail_blazer_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057d3a0, TrailBlazer_vf28, GR, 0) { ChFpuKeep k(m); rx_trail_blazer_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057d3f0, TrailBlazer_VehicleEntered, GR, 0) { ChFpuKeep k(m); rx_trail_blazer_vehicle_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0057d440, TrailBlazer_vf64, GR, 0) { rx_trail_blazer_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057d5b0, TargetPractice_vf1, GB, 0) { ChFpuKeep k(m); rx_target_practice_vf1(m.r[1], m.arg(0)); return m.ret(4); }
// 0057d7b0 (EAX = A)
RX_PORT(0x0057d7b0, TargetPractice_Clear, GR, 0) { rx_target_practice_clear(m.r[0]); return m.ret(0); }
// 0057d6d0 (ESI = A; stdcall group name; RET 4)
RX_PORT(0x0057d6d0, TargetPractice_Collect, GR, 0) { ChFpuKeep k(m); rx_target_practice_collect(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d590, TargetPractice_vf37, GB, 0) { ChFpuKeep k(m); rx_target_practice_vf37(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d540, TargetPractice_vf42, GR, 0) { ChFpuKeep k(m); rx_target_practice_vf42(m.r[1]); return m.ret(0); }
// 0058a8e0 (ESI = channel, ECX = hits, EDX = total; stdcall player, &position; RET 8)
RX_PORT(0x0058a8e0, TargetPractice_HitEvent, GR, 0) { ChFpuKeep k(m); rx_target_hit_event(m.r[6], m.r[1], m.r[2], m.arg(0), m.arg(1)); return m.ret(8); }
// 0057d880 (EDI = A; stdcall player; RET 4)
RX_PORT(0x0057d880, TargetPractice_Hits, GR, 0) { ChFpuKeep k(m); rx_target_practice_hits(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057d780, TargetPractice_vf28, GR, 0) { ChFpuKeep k(m); rx_target_practice_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057d6a0, TargetPractice_vf47, GB, 0) { ChFpuKeep k(m); rx_target_practice_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0055ed10, TargetPractice_Value, GR, OUT_FPU) {
    m.fpu.push_back(reflex_f80::from_int(int64_t(rx_target_practice_value(m.r[1]))));   // fild dword: the exact integer
    return m.ret(0);
}
RX_PORT(0x0057d9e0, TargetPractice_vf64, GR, 0) { rx_target_practice_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }

// ---- ChallengeDriver / RuleChallengeBoundary ----
RX_PORT(0x005188c0, ChallengeDriver_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_challenge_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00516020, ChallengeDriver_vf1, GB, 0) { ChFpuKeep k(m); rx_challenge_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x005188f0, NetworkDriver_vf2, GR, 0) { ChFpuKeep k(m); rx_network_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x0050eb50, NetworkDriver_vf16, GR, 0) { ChFpuKeep k(m); rx_network_driver_vf16(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00518900, ChallengeDriver_vf3, GR, 0) { ChFpuKeep k(m); rx_challenge_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0050bee0, ChallengeDriver_vf4, GR, 0) { ChFpuKeep k(m); rx_challenge_driver_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00516190, Driver_RulesTick, GR, 0) { ChFpuKeep k(m); rx_driver_rules_tick(m.arg(0)); return m.ret(4); }
RX_PORT(0x005189c0, ChallengeDriver_vf7, GR, 0) { ChFpuKeep k(m); rx_challenge_driver_vf7(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0051daa0, ChallengeBoundary_vf1, GR, OUT_EAX) { m.r[0] = rx_challenge_boundary_vf1(m.r[1], m.arg(0)); return m.ret(4); }
// 0051dae0 (EAX = rule, ECX = position) -> EAX
RX_PORT(0x0051dae0, ChallengeBoundary_Outside, GR, OUT_EAX) { m.r[0] = rx_challenge_boundary_outside(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x0051dbe0, ChallengeBoundary_vf5, GR, 0) { ChFpuKeep k(m); rx_challenge_boundary_vf5(m.r[1], argf(m, 0)); return m.ret(4); }

// ---- the challenges' audio logic ----
RX_PORT(0x00581ba0, AudioLogic_VehicleWrecked, GR, 0) { ChFpuKeep k(m); rx_audio_logic_vehicle_wrecked(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582030, AudioLogic_InProgressExited, GR, 0) { ChFpuKeep k(m); rx_audio_logic_in_progress_exited(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582050, AudioLogic_WindDownEntered, GR, 0) { ChFpuKeep k(m); rx_audio_logic_wind_down_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582090, AudioLogic_PostEntered, GR, 0) { ChFpuKeep k(m); rx_audio_logic_post_entered(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582180, AudioLogic_FinishCountdownStep, GR, 0) { ChFpuKeep k(m); rx_audio_logic_finish_countdown_step(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00585150, ChallengeAudio_Award, GR, 0) { ChFpuKeep k(m); rx_challenge_audio_award(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4)); return m.ret(0x14); }
RX_PORT(0x005851a0, ChallengeAudio_TargetFound, GR, 0) { ChFpuKeep k(m); rx_challenge_audio_target_found(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x005851d0, ChallengeAudio_HitDiscovery, GR, 0) { ChFpuKeep k(m); rx_challenge_audio_hit_discovery(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00584e90, ChallengeAudio_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_challenge_audio_logic_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00584ed0, ChallengeAudio_vf3, GB, 0) { ChFpuKeep k(m); rx_challenge_audio_logic_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00585010, ChallengeAudio_vf4, GR, 0) { ChFpuKeep k(m); rx_challenge_audio_logic_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0040e730, TargetPracticeCollectable_vf19, GR, 0) { rx_targetpractice_collectable_vf19(m.r[1], argf(m, 0)); return m.ret(4); }

// ---- Pathfinder / StepUp (not staged by any shipped level: probe-only) ----
RX_PORT(0x0058a640, Pathfinder_Listen, GB, 0) { rx_pathfinder_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a700, Pathfinder_Unlisten, GR, 0) { rx_pathfinder_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058a760, Pathfinder_RoamBehaviorGet, GB, OUT_EAX) { m.r[0] = rx_pathfinder_roam_behavior_get(m.r[1]); return m.ret(0); }
RX_PORT(0x0057b7f0, Pathfinder_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_pathfinder_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b830, Pathfinder_vf5, GB, 0) { ChFpuKeep k(m); rx_pathfinder_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0057b8a0, Pathfinder_vf2, GB, 0) { ChFpuKeep k(m); rx_pathfinder_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057b9a0, Pathfinder_vf4, GB, 0) { ChFpuKeep k(m); rx_pathfinder_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0057ba10, Pathfinder_vf67, GB, 0) { rx_pathfinder_vf67(m.r[1]); return m.ret(0); }
RX_PORT(0x0057bb60, Pathfinder_vf3, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0057bbe0, Pathfinder_vf18, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057bc10, Pathfinder_vf16, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0057bc30, Pathfinder_vf23, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf23(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057bc50, Pathfinder_vf47, GB, 0) { ChFpuKeep k(m); rx_pathfinder_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057bd30, Capture_vf37, GB, 0) { ChFpuKeep k(m); rx_capture_vf37(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005775c0, Capture_vf39, GR, 0) { ChFpuKeep k(m); rx_capture_vf39(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057bda0, Pathfinder_vf38, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf38(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057be20, Pathfinder_vf25, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf25(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057be50, Pathfinder_vf28, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057c050, Pathfinder_vf41, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf41(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057c1d0, Pathfinder_vf44, GB, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_pathfinder_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057c230, Pathfinder_vf20, GR, 0) { ChFpuKeep k(m); rx_pathfinder_vf20(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057c320, Pathfinder_vf64, GR, 0) { rx_pathfinder_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0055ecd0, StepUp_Sdtor, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_step_up_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 0057c640 (EAX = A, ECX = level)
RX_PORT(0x0057c640, StepUp_SetLevel, GR, 0) { rx_step_up_set_level(m.r[0], m.r[1]); return m.ret(0); }
// 0057cbb0 (EDI = A; stdcall restart; RET 4)
RX_PORT(0x0057cbb0, StepUp_Place, GR, 0) { ChFpuKeep k(m); rx_step_up_place(m.r[7], m.arg(0)); return m.ret(4); }
// 0057c6d0 / 0057c760 (EAX = A)
RX_PORT(0x0057c6d0, StepUp_Fail, GR, 0) { ChFpuKeep k(m); rx_step_up_fail(m.r[0]); return m.ret(0); }
RX_PORT(0x0057c760, StepUp_Clear, GR, 0) { ChFpuKeep k(m); rx_step_up_clear(m.r[0]); return m.ret(0); }
// 0057cdf0 (ESI = A; stdcall player; RET 4) -> EAX
RX_PORT(0x0057cdf0, StepUp_PastBar, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_step_up_past_bar(m.r[6], m.arg(0)); return m.ret(4); }
// 0057ce90 (EDI = A, ESI = position) -> EAX
RX_PORT(0x0057ce90, StepUp_BetweenUprights, GR, OUT_EAX) { ChFpuKeep k(m); m.r[0] = rx_step_up_between_uprights(m.r[7], m.r[6]); return m.ret(0); }
RX_PORT(0x0057cd30, StepUp_vf2, GB, 0) { ChFpuKeep k(m); rx_step_up_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0057cb20, StepUp_vf18, GR, 0) { ChFpuKeep k(m); rx_step_up_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0057c810, StepUp_vf28, GR, 0) { ChFpuKeep k(m); rx_step_up_vf28(m.r[1], argf(m, 0)); return m.ret(4); }
RX_PORT(0x0057cd00, StepUp_vf64, GR, 0) { rx_step_up_vf64(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057c9d0, StepUp_vf65, GR, 0) { rx_step_up_vf65(m.r[1]); return m.ret(0); }
RX_PORT(0x0057cf50, StepUp_vf61, GB, 0) { ChFpuKeep k(m); rx_step_up_vf61(m.r[1]); return m.ret(0); }
