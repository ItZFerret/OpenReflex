// race_beh2_perform_stunt.cpp -- oracle adapters for core/game/race_beh2_perform_stunt.c (BehaviorPerformStunt's update, its state
// functions and helpers; group rx_race).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/race_beh2.h"
}
using namespace reflex_port;
using rx_oracle::f32;
using rx_oracle::bits;

// xmm0 = (v, 0, 0, 0)
static inline void set_xmm0(reflex_vm::Machine& m, float v) { m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; }

// 0052aac0 BehaviorPerformStunt::vf4 (ECX = behaviour; RET 4) -> EAX 0.
// RX_PS_STATS=1: a histogram of (state, mode) at entry, printed every 3000 calls (development)
static void ps_stats(reflex_vm::Machine& m) {
    static uint64_t hist[8][4]; static uint32_t calls = 0, en = 0;
    const uint32_t b = m.r[1];
    if (U32(b, 4)) { en++; const uint32_t st = U32(b, 0xc), md = U32(b, 0x10); if (st < 8 && md < 4) hist[st][md]++; if (st == 2) std::fprintf(stderr, "[ps2] call %u beh %08x mode %u\n", calls + 1, b, md); }
    if (++calls % 3000 == 0) {
        std::fprintf(stderr, "[ps] calls %u enabled %u:", calls, en);
        for (int i = 0; i < 8; i++) for (int j = 0; j < 4; j++) if (hist[i][j]) std::fprintf(stderr, " s%d/m%d=%llu", i, j, (unsigned long long)hist[i][j]);
        std::fprintf(stderr, "\n");
    }
}
RX_PORT(0x0052aac0, BehaviorPerformStunt_Update, "rx_race", OUT_EAX) { if (std::getenv("RX_PS_STATS")) ps_stats(m); m.r[0] = rx_behavior_perform_stunt_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
// the states: 0052ae10 (ESI), 0052aed0 (EAX), 0052af90 (EAX), 0052b2f0 (EAX), 0052b390 (ESI), 0052b410 (ESI); the take-off test 0052ab80 (ESI)
RX_PORT(0x0052ae10, PerformStunt_Idle, "rx_race", 0) { rx_perform_stunt_idle(m.r[6]); return m.ret(0); }
RX_PORT(0x0052aed0, PerformStunt_PlanGround, "rx_race", 0) { rx_perform_stunt_plan_ground(m.r[0]); return m.ret(0); }
RX_PORT(0x0052af90, PerformStunt_PlanAir, "rx_race", 0) { rx_perform_stunt_plan_air(m.r[0]); return m.ret(0); }
RX_PORT(0x0052b2f0, PerformStunt_Wait, "rx_race", 0) { rx_perform_stunt_wait(m.r[0]); return m.ret(0); }
RX_PORT(0x0052b390, PerformStunt_Run, "rx_race", 0) { rx_perform_stunt_run(m.r[6]); return m.ret(0); }
RX_PORT(0x0052b410, PerformStunt_Next, "rx_race", 0) { rx_perform_stunt_next(m.r[6]); return m.ret(0); }
RX_PORT(0x0052ab80, PerformStunt_TakeoffTest, "rx_race", 0) { rx_perform_stunt_takeoff_test(m.r[6]); return m.ret(0); }
// 0052b0a0 (ESI = behaviour; stack budget, flag; RET 8) -> xmm0
RX_PORT(0x0052b0a0, PerformStunt_Plan, "rx_race", OUT_XMM0) { set_xmm0(m, rx_perform_stunt_plan(m.r[6], f32(m.arg(0)), m.arg(1))); return m.ret(8); }
// 0051dff0 / 0051e040 (EAX = driver) -> xmm0
RX_PORT(0x0051dff0, Driver_StuntTuningA, "rx_race", OUT_XMM0) { set_xmm0(m, rx_driver_stunt_tuning_a(m.r[0])); return m.ret(0); }
RX_PORT(0x0051e040, Driver_StuntTuningB, "rx_race", OUT_XMM0) { set_xmm0(m, rx_driver_stunt_tuning_b(m.r[0])); return m.ret(0); }
// 0078fcf0 (EAX = haystack, EDI = needle) -> EAX
RX_PORT(0x0078fcf0, StrFindNoCase, "rx_race", OUT_EAX) { m.r[0] = rx_str_find_nocase(m.r[0], m.r[7]); return m.ret(0); }

// ---- BehaviorRoamWorld (core/game/race_beh2_roam_world.c) ----
// 0052fcf0 BehaviorRoamWorld::vf4 (ECX = behaviour; RET 4) -> EAX 0.
static void rw_stats(reflex_vm::Machine& m) {
    static uint64_t hist[8]; static uint32_t calls = 0, en = 0;
    const uint32_t b = m.r[1];
    if (U32(b, 4)) { en++; const uint32_t st = U32(b, 0xc); if (st < 8) hist[st]++; }
    if (++calls % 5000 == 0) {
        std::fprintf(stderr, "[rw] calls %u enabled %u:", calls, en);
        for (int i = 0; i < 8; i++) std::fprintf(stderr, " s%d=%llu", i, (unsigned long long)hist[i]);
        std::fprintf(stderr, "\n");
    }
}
RX_PORT(0x0052fcf0, BehaviorRoamWorld_Update, "rx_race", OUT_EAX) { if (std::getenv("RX_RW_STATS")) rw_stats(m); m.r[0] = rx_behavior_roam_world_update(m.r[1], f32(m.arg(0))); return m.ret(4); }
// 0052efa0 (EAX = behaviour, ESI = area) -> EAX
RX_PORT(0x0052efa0, RoamWorld_AreaMatches, "rx_race", OUT_EAX) { m.r[0] = rx_roam_area_matches(m.r[0], m.r[6]); return m.ret(0); }
// 00530dc0 (ECX = type, EAX = class; stack kind; RET 4) -> EAX
RX_PORT(0x00530dc0, RoamWorld_ClassAllowed, "rx_race", OUT_EAX) { m.r[0] = rx_roam_class_allowed(m.r[1], m.r[0], m.arg(0)); return m.ret(4); }
// 00530590 (EDI = out; stack behaviour; RET 4) -> EAX = the area
RX_PORT(0x00530590, RoamWorld_NearestArea, "rx_race", OUT_EAX) { m.r[0] = rx_roam_nearest_area(m.arg(0), m.r[7]); return m.ret(4); }
// 0052f0e0 (stack behaviour, out; RET 8)
RX_PORT(0x0052f0e0, RoamWorld_ChooseArea, "rx_race", 0) { rx_roam_choose_area(m.arg(0), m.arg(1)); return m.ret(8); }
// 0052fca0 (ESI = behaviour) -> EAX; 0052f4a0 (ECX = behaviour) -> EAX; 0052f990 (ESI = behaviour) -> EAX
RX_PORT(0x0052fca0, RoamWorld_PickTarget, "rx_race", OUT_EAX) { m.r[0] = rx_roam_pick_target(m.r[6]); return m.ret(0); }
RX_PORT(0x0052f4a0, RoamWorld_TargetOk, "rx_race", OUT_EAX) { m.r[0] = rx_roam_target_ok(m.r[1]); return m.ret(0); }
RX_PORT(0x0052f990, RoamWorld_Arrive, "rx_race", OUT_EAX) { m.r[0] = rx_roam_arrive(m.r[6]); return m.ret(0); }
// 0052f510 (stack behaviour; RET 4) -> EAX = the target node; 00530740 (stack behaviour; RET 4)
RX_PORT(0x0052f510, RoamWorld_Follow, "rx_race", OUT_EAX) { m.r[0] = rx_roam_follow(m.arg(0)); return m.ret(4); }
RX_PORT(0x00530740, RoamWorld_Arrived, "rx_race", 0) { rx_roam_arrived(m.arg(0)); return m.ret(4); }
// 00538a60 (EAX = end, ECX = begin, ESI = point; stack out; plain RET) -> xmm0 = the parameter t
RX_PORT(0x00538a60, Waypoint_LineProject, "rx_race", OUT_XMM0) { set_xmm0(m, rx_waypoint_line_project(m.r[0], m.r[1], m.r[6], m.arg(0))); return m.ret(0); }
