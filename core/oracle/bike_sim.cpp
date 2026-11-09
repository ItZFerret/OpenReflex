// bike_sim.cpp -- oracle adapters for core/game/bike_sim.c (group "rx_bike"): the simulation framework.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_sim.h"
}
using namespace bike_oracle;

static inline float xmm0_f(const reflex_vm::Machine& m) { return xmm_f(m, 0); }

// 00430270: result EAX = 0x1082 (standard ABI slot; EDX untouched).
RX_PORT(0x00430270, RavenVehicle_vf22, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) { m.r[0] = bike_vehicle_type_id(); return m.ret(0); }

// 004392d0: ECX = S; leaves EAX = 0.
RX_PORT(0x004392d0, RavenSimulationMotocross_vf3, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    bike_clear_wheel_contact_words(m.r[1]); m.r[0] = 0; return m.ret(0);
}

// 004304b0..004304f0: EAX = record, xmm0 lane 0 = value; no live outputs (live.tsv mask 0 for 004304b0; the others unlisted).
RX_PORT(0x004304b0, RiderRecord_SetFloatA0, "rx_bike", 0) { bike_rider_record_set_a0(m.r[0], xmm0_f(m)); return m.ret(0); }
RX_PORT(0x004304c0, RiderRecord_SetFloatA4, "rx_bike", 0) { bike_rider_record_set_a4(m.r[0], xmm0_f(m)); return m.ret(0); }
RX_PORT(0x004304d0, RiderRecord_SetFloatA8, "rx_bike", 0) { bike_rider_record_set_a8(m.r[0], xmm0_f(m)); return m.ret(0); }
RX_PORT(0x004304e0, RiderRecord_SetFloat88, "rx_bike", 0) { bike_rider_record_set_88(m.r[0], xmm0_f(m)); return m.ret(0); }
RX_PORT(0x004304f0, RiderRecord_SetFloatAC, "rx_bike", 0) { bike_rider_record_set_ac(m.r[0], xmm0_f(m)); return m.ret(0); }

// 00430490..00430540: ECX = record, EAX = vec3 source; the x87 stack is left empty.
RX_PORT(0x00430490, RiderRecord_SetVec90, "rx_bike", 0) { bike_rider_record_set_vec90(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00430500, RiderRecord_SetVec60, "rx_bike", 0) { bike_rider_record_set_vec60(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00430520, RiderRecord_SetVec70, "rx_bike", 0) { bike_rider_record_set_vec70(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00430540, RiderRecord_SetVec80, "rx_bike", OUT_ECX | OUT_FPU) { bike_rider_record_set_vec80(m.r[1], m.r[0]); return m.ret(0); }

// 00447730: ECX = S; EAX = 0 / 1 (EDX untouched).
RX_PORT(0x00447730, RavenSimulationMotocross_vf29, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    m.r[0] = bike_both_wheels_in_contact(m.r[1]); return m.ret(0);
}

// 00447750: ECX = S; the virtual call to slot 29 leaves EDX = its target (the callee does not touch it) and EAX = result.
RX_PORT(0x00447750, RavenSimulationMotocross_vf30, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    const uint32_t S = m.r[1];
    const uint32_t r = bike_wheel_contact_scalars_set(S);
    m.r[0] = r; m.r[2] = m.mem.read(m.mem.read(S, 4) + 0x74, 4);
    return m.ret(0);
}

// 00447790: ECX = S, stack out vec4; RET 4; EAX = out (the caller reads the out pointer from EAX).
RX_PORT(0x00447790, RavenSimulationMotocross_vf31, "rx_bike", OUT_EAX | OUT_FPU) {
    m.r[0] = bike_wheel_contact_normal(m.r[1], m.arg(0));
    return m.ret(4);
}

// 0043b5e0: ECX = S; EAX = the OR; EDX = 1 when S+8c20 == 0 else 0 (sete dl after xor edx,edx).
RX_PORT(0x0043b5e0, RavenSimulationMotocross_vf5, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    const uint32_t S = m.r[1];
    m.r[2] = (m.mem.read(S + 0x8c20, 4) == 0) ? 1 : 0;
    m.r[0] = bike_body_probe_contact_any(S);
    return m.ret(0);
}

// 004301f0: ECX = this, stack out vec3; ret 4. EAX = out (mov eax,[esp+4]).
RX_PORT(0x004301f0, RavenSimulation_vf_004301f0, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    const uint32_t out = m.arg(0);
    bike_get_vec_c0(m.r[1], out); m.r[0] = out; return m.ret(4);
}

// 004300a0: ECX = node reference, stack matrix; ret 4. The function is a leaf of Node_Invalidate 007db7e0 (EAX = the matrix slot,
// ECX = the node, EDX = the node's hierarchy word at the call), so EAX / ECX / EDX are that call's leftovers. They are live:
// EAX / EDX pass through UI3DModelComponent::vf15 (005cd6c0) as its return values, ECX is pushed by VehicleDamage_SplitPart
// (00479ef0) into FUN_007dd930, and xmm4's upper lanes are carried by RavenVehicleUTV::vf60's movss (004c20c4) before the
// register is rewritten; 004300a0 does not touch xmm4, so its entry value is returned. x87 untouched.
RX_PORT(0x004300a0, ModelNodeRef_SetMatrix, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | XMM(4) | OUT_FPU) {
    Scratch s(m);
    uint32_t regs[3];
    bike_model_node_set_matrix_regs(m.r[1], m.arg(0), regs);
    m.r[0] = regs[0];
    m.r[1] = regs[1];
    m.r[2] = regs[2];
    m.xmm[4] = s.x[4];
    return m.ret(4);
}

// 00430850: EAX = 64-byte input block; also leaves ECX = 0 and xmm0 = 0 (not live: live.tsv EAX only).
RX_PORT(0x00430850, Simulation_InitInputBlock, "rx_bike", OUT_EAX) { bike_init_input_block(m.r[0]); return m.ret(0); }

// 0043b4a0 vf4: ECX = S; EAX = 0 / 1. (EDX holds a mask sum on some paths: a 32-bit virtual result, not read by callers.)
RX_PORT(0x0043b4a0, RavenSimulationMotocross_vf4, "rx_bike", OUT_EAX | OUT_FPU) {
    m.r[0] = bike_moto_contact_or_impulse(m.r[1]); return m.ret(0);
}

// 0043c440 vf7: ECX = S, stack dt; ret 4. EAX = S (mov eax, ecx at entry, never changed).
RX_PORT(0x0043c440, RavenSimulationMotocross_vf7, "rx_bike", OUT_EAX | OUT_FPU) {
    const uint32_t S = m.r[1];
    bike_moto_save_or_restore_pose(S, argf(m, 0)); m.r[0] = S; return m.ret(4);
}

// 0044c010 vf27: ECX = S; EAX = 0 / 1.
RX_PORT(0x0044c010, RavenSimulationMotocross_vf27, "rx_bike", OUT_EAX | OUT_FPU) {
    m.r[0] = bike_moto_resting_axis_flat(m.r[1]); return m.ret(0);
}

// 0044e570 vf10: ECX = S, stack pointer to three vec4 targets; ret 4. A void slot: EAX / EDX hold mask sums of the angle sign
// test, not a result (declared: the x87 stack only).
RX_PORT(0x0044e570, RavenSimulationMotocross_vf10, "rx_bike", OUT_FPU) {
    bike_moto_set_pose_override_targets(m.r[1], m.arg(0));
    return m.ret(4);
}

// 00446cd0 vf33: ECX = S; void slot, EAX = S (mov eax, ecx).
RX_PORT(0x00446cd0, RavenSimulationMotocross_vf33, "rx_bike", OUT_EAX | OUT_FPU) {
    const uint32_t S = m.r[1];
    bike_moto_begin_pose_override(S); m.r[0] = S; return m.ret(0);
}

// 00434ef0: EDI = S. Registers the original leaves (live.tsv EAX ECX EDX): ECX = the security cookie ([009b45e4], xor esp twice);
// EAX = cookie ^ ESP of the aligned frame when no override is active, mode - 1 when the mode is not 2 (dec eax), else what
// the last mechanism call (0085c4b0) leaves; EDX = the entry value except after the mechanism calls.
RX_PORT(0x00434ef0, Simulation_EndPoseOverride, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const uint32_t S = m.r[7], cookie = uint32_t(m.mem.read(0x009b45e4, 4)), esp0 = m.r[4];
    const uint32_t flag = uint32_t(m.mem.read(S + 0x2de0, 4)), mode = uint32_t(m.mem.read(S + 0x2de4, 4));
    Scratch s(m);
    bike_end_pose_override(S);
    if (flag == 0) { m.r[0] = cookie ^ (((esp0 - 4) & ~15u) - 0x2c); m.r[2] = s.r2; }
    else if (mode != 2) { m.r[0] = mode - 1; m.r[2] = s.r2; }
    else {   // mode 2: the mechanism calls' leftovers (0085c4b0 last: EAX = its body pointer past the last body, EDX = the mechanism)
        const uint32_t M = S + 0xd0, n = uint32_t(m.mem.read(M + 0x1c, 4));
        if (int32_t(n) > 0) m.r[0] = M + 0x90 + 0xa0 * n;
        m.r[2] = M;
    }
    m.r[1] = cookie;
    return m.ret(0);
}

// 0044e830 vf34: ECX = S; a void slot (the scheduler, 00431555, redefines EAX right after and reads no result): x87 stack only.
RX_PORT(0x0044e830, RavenSimulationMotocross_vf34, "rx_bike", OUT_FPU) {
    bike_moto_apply_pose_override(m.r[1]); return m.ret(0);
}

// 00433120: ECX = S, stack out; ret 4. EAX = out (mov eax,[ebp+8]); xmm0 = (reverse input, 0, 0, 0) (movss from memory); live: EDX / x87.
RX_PORT(0x00433120, Simulation_GetReplayState, "rx_bike", OUT_EDX | OUT_FPU) {
    bike_get_replay_state(m.r[1], m.arg(0)); return m.ret(4);
}

// 00433550: ESI = out record, stack S; ret 4; no live outputs. (Vec4Floor inside runs the original: its registers are not live.)
RX_PORT(0x00433550, ReplayRecord_Encode, "rx_bike", 0) {
    bike_replay_record_encode(m.r[6], m.arg(0)); return m.ret(4);
}

// 004308a0: EAX = controller, EDI = block; plain ret. All five callers ignore every register after the call (they reload EAX / ECX
// from the simulation first): live.tsv's EAX / EDX are not results; declared: the x87 stack only.
RX_PORT(0x004308a0, Simulation_ReadControllerInput, "rx_bike", OUT_FPU) { bike_read_controller_input(m.r[0], m.r[7]); return m.ret(0); }

// 00433380: ECX = S, stack block; ret 4. A void slot (EAX / EDX are the _finite calls' leftovers): x87 stack only.
RX_PORT(0x00433380, RavenSimulation_vf24, "rx_bike", OUT_FPU) { bike_set_input_block(m.r[1], m.arg(0)); return m.ret(4); }

// 0044bff0: ECX = S, stack block; ret 4; void slot.
RX_PORT(0x0044bff0, RavenSimulationMotocross_vf24, "rx_bike", OUT_FPU) { bike_moto_set_input_block(m.r[1], m.arg(0)); return m.ret(4); }

// 0044bfb0: ECX = S, stack controller; ret 4; void slot.
RX_PORT(0x0044bfb0, RavenSimulationMotocross_vf23, "rx_bike", OUT_FPU) { bike_moto_read_controller(m.r[1], m.arg(0)); return m.ret(4); }

// 004321a0 / 00432360 / 004324d0 (vf28 / vf26 / vf25): ECX = S; EAX = 0 / 1 (standard-ABI virtual slots; EDX holds scratch).
RX_PORT(0x004321a0, RavenSimulation_vf28, "rx_bike", OUT_EAX | OUT_FPU) { m.r[0] = bike_sim_activity(m.r[1]); return m.ret(0); }
RX_PORT(0x00432360, RavenSimulation_vf26, "rx_bike", OUT_EAX | OUT_FPU) { m.r[0] = bike_sim_leave_rest_test(m.r[1]); return m.ret(0); }
RX_PORT(0x004324d0, RavenSimulation_vf25, "rx_bike", OUT_EAX | OUT_FPU) { m.r[0] = bike_sim_enter_rest_test(m.r[1]); return m.ret(0); }

// 0043b640 vf8: ECX = S; a void slot.
RX_PORT(0x0043b640, RavenSimulationMotocross_vf8, "rx_bike", OUT_FPU) { bike_moto_apply_pending_tuning(m.r[1]); return m.ret(0); }

// 00434d00: ESI = S; EAX = the flags (also in S+10).
RX_PORT(0x00434d00, FUN_00434d00, "rx_bike", OUT_EAX) { m.r[0] = bike_state_divergence_check(m.r[6]); return m.ret(0); }

// 00435320: ECX = S; leaves ECX = the security cookie (xor with ESP twice). 
RX_PORT(0x00435320, Simulation_ApplyDebugKeyForces, "rx_bike", OUT_ECX | OUT_FPU) {
    const uint32_t cookie = uint32_t(m.mem.read(0x009b45e4, 4));
    bike_apply_debug_key_forces(m.r[1]); m.r[1] = cookie; return m.ret(0);
}

// 00435030: EDI = S. EDX = what the last call leaves (the input manager's vtable or the mechanism call's EDX), left by the nested calls.
RX_PORT(0x00435030, Simulation_HandleDebugKeys, "rx_bike", OUT_EDX | OUT_FPU) { bike_handle_debug_keys(m.r[7]); return m.ret(0); }

// 004355d0: stack S; ret 4. ECX = what the original leaves: the external object (nothing attached / S+5c set), 0 without a linear impulse
// or bodies, else the body count (loop counter). live.tsv: ECX / x87.
RX_PORT(0x004355d0, Simulation_ApplyExternalImpulse, "rx_bike", OUT_ECX | OUT_FPU) {
    const uint32_t S = m.arg(0);
    const uint32_t obj = uint32_t(m.mem.read(S + 0x2ebc, 4));
    const int r = bike_apply_external_impulse(S);
    const int32_t count = int32_t(m.mem.read(S + 0xec, 4));
    m.r[1] = r == 0 ? obj : (r == 1 ? 0 : (count > 0 ? uint32_t(count) : 0));
    return m.ret(4);
}

// 00431d60: ESI = S, stack dt; ret 4; no live registers (live.tsv mask 0).
void whip_trace_step(reflex_vm::Machine& m, uint32_t S, uint32_t dt_bits);   // bike_whip_reverse_probe.cpp (RX_WHIP_TRACE)
RX_PORT(0x00431d60, RavenSimulation_IntegrateStep, "rx_bike", 0) { whip_trace_step(m, m.r[6], m.arg(0)); bike_integrate_step(m.r[6], argf(m, 0)); return m.ret(4); }
// 00432680: EDI = S, stack dt; RET 4; the remote-simulation smoothing step (live.tsv mask 0). EDI is m.r[7] (w-remote4w: the
// adapter read ESI; no world reached it before remote_smooth_probe.cpp).
RX_PORT(0x00432680, RavenSimulation_RemoteSmooth, "rx_bike", 0) { bike_remote_smooth(m.r[7], argf(m, 0)); return m.ret(4); }

// 00430ef0: ECX = S, stack frame dt; ret 4. The only caller (slot 1, 00430eb0) pops its registers and returns: live.tsv's EAX / EDX
// are not results (it is a void slot); declared: the x87 stack only.
RX_PORT(0x00430ef0, RavenSimulation_UpdateFixedSteps, "rx_bike", OUT_FPU) { bike_update_fixed_steps(m.r[1], argf(m, 0)); return m.ret(4); }

// 00430eb0: ECX = S, stack frame dt; ret 4; a void slot.
RX_PORT(0x00430eb0, RavenSimulation_vf1, "rx_bike", OUT_FPU) { bike_sim_update(m.r[1], argf(m, 0)); return m.ret(4); }
// 0048d2f0 (ECX = the simulation, stack a vec4; RET 4)
RX_PORT(0x0048d2f0, Sim_SetPosition, "rx_bike", 0) { rx_sim_set_position(m.r[1], m.arg(0)); return m.ret(4); }
