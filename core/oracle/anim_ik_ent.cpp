// anim_ik_ent.cpp -- oracle adapters for core/game/anim_ik_ent.c (animated scene entities).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_ik.h"
}
using namespace reflex_port;

// 006bb6e0 (stack: spline entity, time, matrix out; RET 0xc). Void: the callers read no register after it.
ANIM_PORT(0x006bb6e0, SplineEntity_Sample, "rx_anim", 0) {
    rx_spline_entity_sample(m.arg(0), rx_oracle::f32(m.arg(1)), m.arg(2));
    return m.ret(0xc);
}
// Jobs (cdecl (job record); the job pool reads no register after them).
ANIM_PORT(0x006bb010, SplineEntity_Job, "rx_anim", 0) {
    rx_spline_entity_job(m.arg(0));
    return m.ret(0);
}
ANIM_PORT(0x006ba170, AnimEntitySingleAnimLoop_Job, "rx_anim", 0) {
    rx_single_anim_loop_job(m.arg(0));
    return m.ret(0);
}
// 006a5b30 (stdcall (destination, source); RET 8): EAX = destination; ECX = 0 (rep movsd), read by callers.
ANIM_PORT(0x006a5b30, StateRecord_Assign, "rx_anim", OUT_EAX | OUT_ECX) {
    m.r[0] = rx_state_record_assign(m.arg(0), m.arg(1));
    m.r[1] = 0;
    return m.ret(8);
}
// 006b85e0 (ESI = board, xmm0 = dt; stack: to B; RET 4). Void.
ANIM_PORT(0x006b85e0, Board_WalkStep, "rx_anim", 0) {
    rx_board_walk_step(m.r[6], rx_oracle::f32(uint32_t(m.xmm[0].lo)), m.arg(0));
    return m.ret(4);
}
ANIM_PORT(0x006b7c70, Board_Job, "rx_anim", 0) {
    rx_board_job(m.arg(0));
    return m.ret(0);
}
// 006b6a30 (fastcall ECX = flag out, EDX = name to avoid; cdecl name): EAX = the variant name (callers read it).
ANIM_PORT(0x006b6a30, AnimAlias_Pick, "rx_anim", OUT_EAX) {
    m.r[0] = rx_anim_alias_pick(m.r[1], m.r[2], m.arg(0));
    return m.ret(0);
}
// 006b7fa0 (EAX = board; plain ret). Void (the callers read no register after it).
ANIM_PORT(0x006b7fa0, Board_Setup, "rx_anim", 0) {
    rx_board_setup(m.r[0]);
    return m.ret(0);
}
ANIM_PORT(0x006b7f00, ThirtySecondBoard_vf19, "rx_anim", 0) {
    rx_thirty_second_board_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// The vf22s: EAX = the class's type id.
#define RX_TYPE_PORT(addr, Name, fn) ANIM_PORT(addr, Name, "rx_anim", OUT_EAX) { m.r[0] = fn(m.r[1]); return m.ret(0); }
RX_TYPE_PORT(0x006b9ac0, AnimEntitySingleAnimLoop_vf22, rx_anim_entity_single_anim_loop_type)
RX_TYPE_PORT(0x006ba560, AnimEntitySplineAnimLoop_vf22, rx_anim_entity_spline_anim_loop_type)
RX_TYPE_PORT(0x006bbdd0, AnimEntitySplineAnimLoopWithParticles_vf22, rx_anim_entity_spline_particles_type)
RX_TYPE_PORT(0x006bd480, CrewChief_vf22, rx_crew_chief_type)
RX_TYPE_PORT(0x006b76c0, ThirtySecondBoard_vf22, rx_thirty_second_board_type)
RX_TYPE_PORT(0x006bd900, DropGate_vf22, rx_drop_gate_type)
RX_TYPE_PORT(0x006bf530, LeaderBoard_vf22, rx_leader_board_type)
// 006be330 (EAX = angle, ECX = negate, xmm6 = target, xmm2 = step; stack: gate; RET 4): EAX = reached.
ANIM_PORT(0x006be330, DropGate_Swing, "rx_anim", OUT_EAX) {
    m.r[0] = rx_drop_gate_swing(m.arg(0), m.r[0], m.r[1], rx_oracle::f32(uint32_t(m.xmm[6].lo)), rx_oracle::f32(uint32_t(m.xmm[2].lo)));
    return m.ret(4);
}
ANIM_PORT(0x006be1c0, DropGate_vf19, "rx_anim", 0) {
    rx_drop_gate_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
// 006bf990 (stdcall (board, index, name); RET 0xc). Void.
ANIM_PORT(0x006bf990, LeaderBoard_SetPosition, "rx_anim", 0) {
    rx_leader_board_set_position(m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 006bfcc0 (ECX = lap; stack: board; RET 4). Void.
ANIM_PORT(0x006bfcc0, LeaderBoard_SetLap, "rx_anim", 0) {
    rx_leader_board_set_lap(m.arg(0), int32_t(m.r[1]));
    return m.ret(4);
}
ANIM_PORT(0x006bf760, LeaderBoard_vf19, "rx_anim", 0) {
    rx_leader_board_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
