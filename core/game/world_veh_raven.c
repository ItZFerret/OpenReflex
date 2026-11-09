/* world_veh_raven.c -- the small RavenVehicleMotocross virtuals (0x00466xxx-0x0047bxxx): getters of the simulation's vectors and
 * counters, the vehicle state code, id tests, the damage behaviour manager's contact-event scan.
 *
 * RavenVehicleMotocross (the game-side vehicle, thiscall this): +0x16d4 its simulation (RavenSimulationMotocross: +0x100 /
 * +0x160 / +0x740 / +0x7c0 / +0x8b60 state vectors, +0x8870 / +0x89c0 floats, +0x8ac0 gear index, +0x8d98 / +0x8da0 counters,
 * +0x8c60.. / +0x8cd0.. / +0x8d44 / +0x8d5c state flags), +0x280 the ATV flag, +0x17c0 / +0x17c4 / +0x17c8 ids, +0x17cc the
 * state code. core/notes/world.md (veh). */
#include "bike_vehicle.h"
#include "bike_slot12.h"
#include "world_veh.h"
#include "world_veh_damage.h"
#include "world_veh_util.h"
#include "world_rider.h"
#include "loop_cam_player.h"
#include "phys_quat.h"
#include "anim.h"
#include "fx.h"

static rx_addr sim_of(rx_addr veh) { return U32(veh, 0x16d4); }

/* 00430290 rtti_shared_vf (thiscall; the second base's slot of the vehicle): this + 0x94 (the sub-object), 0 for a null this. */
rx_addr rx_world_shared_vf_sub94(rx_addr self) { return self != 0 ? self + 0x94 : 0; }
RX_FUNCTION(0x00430290, rx_world_shared_vf_sub94)

/* 00430280 RavenVehicle::vf1_o94 / 00430340 vf11_o94 (thiscall; the sub-object at +0x94 is addressed from its own pointer, so these
 * read the vehicle's +0x1294 block): the block's +0x140 field / the matrix of the block's node (+0x50): the owner's matrix array
 * (owner +0x18) at the node's index (byte +0xb) * 64. */
rx_addr rx_world_vehicle_block_140(rx_addr self) { return U32(U32(self, 0x1294), 0x140); }
RX_FUNCTION(0x00430280, rx_world_vehicle_block_140)
rx_addr rx_world_vehicle_block_matrix(rx_addr self) {
    const rx_addr node = U32(U32(self, 0x1294), 0x50);
    return ((uint32_t)U8(node, 0xb) << 6) + U32(U32(node, 0), 0x18);
}
RX_FUNCTION(0x00430340, rx_world_vehicle_block_matrix)
/* 00462950 RavenVehicleMotocross::vf55 (thiscall, stdcall value; RET 4): the simulation's +4 (its type) = value. */
void rx_world_raven_set_sim_type(rx_addr veh, uint32_t value) { SU32(U32(veh, 0x16d4), 4, value); }
RX_FUNCTION(0x00462950, rx_world_raven_set_sim_type)
/* 004302a0 RavenVehicle::vf59 (thiscall, stdcall byte; RET 4): the block +0x1328's byte +0x14e = value. */
void rx_world_vehicle_set_block_flag(rx_addr veh, uint32_t value) { SU8(U32(veh, 0x1328), 0x14e, (uint8_t)value); }
RX_FUNCTION(0x004302a0, rx_world_vehicle_set_block_flag)
/* 004669d0 RavenVehicleMotocross::vf41 (thiscall, stdcall out; RET 4; EAX = out): out = the simulation's +0x7a0 vector. */
void rx_world_raven_get_7a0(rx_addr veh, rx_addr out) { v4_st(out, v4_ld(sim_of(veh) + 0x7a0)); }
RX_FUNCTION(0x004669d0, rx_world_raven_get_7a0)

/* 0044aec0 (ECX = the simulation; EDX = the caller's EDX; -> EAX, EDX; no stack arguments, RET 0). The simulation's frame-time
 * transform pass: the whole body (0x44aec0..0x44bfa3) builds a long chain of inlined quaternion products (veh_quat_mul, the sign
 * tables 0x93a1f0 / 0x93a200 / 0x93a210, the xyz mask 0x92db80, the "normalize or NaN" sequence 0x92dc20 / 0x92dc30) from the
 * simulation's vectors +0x180 / +0x220 / +0x2c0 / +0x360 / +0x7c0 / +0x900 / +0x9a0 / +0x77d0 / +0x8b50 / +0x8c30 / +0x8c40 /
 * +0x8c50 and the .data bases 0xd615c0 / 0xd61680 / 0xd616a0, applies the per-node vectors at +0x1320 (0x10 bytes each) against
 * +0x1c30 (0x20 bytes each) for the +0x1318 nodes, and calls 0042de10 on +0x88e0 / +0x8790 when the flags +0x89e0 / +0x8890 are
 * set. Every result of that work goes to the function's own frame slots ([esp+0x10]..[esp+0xf8], all below the entry ESP and
 * above the final ESP) or stays in xmm -- nothing is written back: the disassembly has exactly one memory store outside the frame,
 * [ecx+0x80c0]=0, and 0042de10 itself only writes its own dead frame (it loads the four +0x30/+0x40/+0x50/+0x60 vectors of its
 * argument and stores the sums to [esp], never touching the argument). So the pass's observable effect is that one field, plus the
 * registers the tail-calling caller (vf61) hands on: EAX and EDX. They are reproduced exactly below (plan/live.tsv: eax,edx,x87;
 * the x87 stack is untouched). EAX = the security-cookie-mixed frame pointer (cookie 0x9b45e4 ^ ESP after `and esp,-16; sub
 * esp,0xfc`) unless a 0042de10 call left its argument in EAX; EDX = the end of the node pointer loop (+0x1320 + 0x10*count) or the
 * caller's EDX when the loop does not run, and +0x8790 after the second 0042de10 call. The frame pointer is the original's entry
 * ESP (rx_sp), the callee's `push ebp; and esp,-16; sub esp,0xfc` form. */
uint32_t rx_world_raven_frame(rx_addr sim, uint32_t edx_in, uint32_t *edx_out) {
    const int32_t count = I32(sim, 0x1318);
    uint32_t eax = U32(0, 0x009b45e4) ^ (((rx_sp() - 4u) & 0xfffffff0u) - 0xfcu);   /* cookie ^ S */
    uint32_t edx = count > 0 ? (uint32_t)(sim + 0x1320) + 0x10u * (uint32_t)count : edx_in;
    SU32(sim, 0x80c0, 0);                                                        /* the one non-frame store */
    if (U32(sim, 0x89e0) != 0) eax = sim + 0x88e0;                                /* 0042de10 (EAX = its argument) */
    if (U32(sim, 0x8890) != 0) { eax = sim + 0x8790; edx = sim + 0x8790; }
    if (edx_out) *edx_out = edx;
    return eax;
}
RX_FUNCTION(0x0044aec0, rx_world_raven_frame)

/* 004666e0 RavenVehicleMotocross::vf61 (thiscall): with the frame counter [0xad3ed4] % 3 (signed) at 1 or 2, tail-jump to
 * 0044aec0 on the simulation (ECX = +0x16d4, EDX = 3 * (counter / 3) = counter - m) and return its EAX; else EAX = 0. */
uint32_t rx_world_raven_vf61(rx_addr veh) {
    const int32_t n = I32(0, 0x00ad3ed4), m = n - (n / 3) * 3;
    if (m == 1 || m == 2) return rx_world_raven_frame(U32(veh, 0x16d4), (uint32_t)(n - m), NULL);
    return (uint32_t)m;
}
RX_FUNCTION(0x004666e0, rx_world_raven_vf61)
/* 004302c0 RavenVehicle::vf14_o94 (thiscall, this = the sub-object vehicle + 0x94): the vehicle's vf61 (vtable +0xf4), then the
 * node set +0x10 of the block (this + 0x1294) copied over its +0x50 (NodeSet_CopyFrom 007dd930: ESI = destination = block + 0x50,
 * EDI = source = block + 0x10; `lea edi, [esi + 0x10]; add esi, 0x50`). */
void rx_world_vehicle_o94_copy_nodes(rx_addr self) {
    (void)rx_vcall(self - 0x94, 0xf4, 0, NULL);
    const rx_addr blk = U32(self, 0x1294);
    WORLD_CALL_SP(0, rx_nodeset_copy_from(blk + 0x50, blk + 0x10));
}
RX_FUNCTION(0x004302c0, rx_world_vehicle_o94_copy_nodes)

/* 00466920 vf36 / 00466940 vf37 / 00466960 vf38 / 004669b0 vf40 (thiscall, stdcall out; RET 4; EAX = out): a simulation vector. */
void rx_world_raven_get_7c0(rx_addr veh, rx_addr out) { v4_st(out, v4_ld(sim_of(veh) + 0x7c0)); }
RX_FUNCTION(0x00466920, rx_world_raven_get_7c0)
void rx_world_raven_get_100(rx_addr veh, rx_addr out) { v4_st(out, v4_ld(sim_of(veh) + 0x100)); }
RX_FUNCTION(0x00466940, rx_world_raven_get_100)
void rx_world_raven_get_740(rx_addr veh, rx_addr out) { v4_st(out, v4_ld(sim_of(veh) + 0x740)); }
RX_FUNCTION(0x00466960, rx_world_raven_get_740)
void rx_world_raven_get_160(rx_addr veh, rx_addr out) { v4_st(out, v4_ld(sim_of(veh) + 0x160)); }
RX_FUNCTION(0x004669b0, rx_world_raven_get_160)

/* 00466980 vf39 (thiscall, stdcall out; RET 4; EAX = the simulation, EDX = out): out = sim +0x8b60 + sim +0x740. */
void rx_world_raven_get_8b60_plus_740(rx_addr veh, rx_addr out) {
    const rx_addr sim = sim_of(veh);
    v4_st(out, v4_add(v4_ld(sim + 0x8b60), v4_ld(sim + 0x740)));
}
RX_FUNCTION(0x00466980, rx_world_raven_get_8b60_plus_740)

/* 00466a20 RavenVehicleMotocross_GetGear (thiscall): the simulation's gear index + 1. */
uint32_t rx_world_raven_get_gear(rx_addr veh) { return U32(sim_of(veh), 0x8ac0) + 1; }
RX_FUNCTION(0x00466a20, rx_world_raven_get_gear)

/* 00466ae0 vf43 / 00466af0 vf44 (thiscall): the simulation's counters +0x8d98 / +0x8da0. */
uint32_t rx_world_raven_get_8d98(rx_addr veh) { return U32(sim_of(veh), 0x8d98); }
RX_FUNCTION(0x00466ae0, rx_world_raven_get_8d98)
uint32_t rx_world_raven_get_8da0(rx_addr veh) { return U32(sim_of(veh), 0x8da0); }
RX_FUNCTION(0x00466af0, rx_world_raven_get_8da0)

/* 00466b70 vf56 (thiscall, stdcall which; RET 4; result st(0), passed through the argument slot): which 0 / 1: sim +0x89c0,
 * else (sim +0x89c0 + sim +0x8870) * 0.5. The float is stored into the argument slot (fld from there), as the original. */
float rx_world_raven_wheel_value(rx_addr veh, uint32_t which, rx_addr arg_slot) {
    const rx_addr sim = sim_of(veh);
    float v = F32(sim, 0x89c0);
    if (which != 0 && which != 1) v = phys_mul(phys_add(v, F32(sim, 0x8870)), 0.5f /* 0x9322cc */);
    SF32(arg_slot, 0, v);
    return v;
}
RX_FUNCTION(0x00466b70, rx_world_raven_wheel_value)

/* 00466a30 vf58 (thiscall): the vehicle's state code (+0x17cc): 0, or the last of the simulation's flags that is set:
 * +0x8cd0 1, +0x8cd4 2, +0x8cd8 3, +0x8c60 4, +0x8c64 5, +0x8c68 6, +0x8d44 7, +0x8d5c 8. */
uint32_t rx_world_raven_state_code(rx_addr veh) {
    static const uint16_t flags[8] = {0x8cd0, 0x8cd4, 0x8cd8, 0x8c60, 0x8c64, 0x8c68, 0x8d44, 0x8d5c};
    const rx_addr sim = sim_of(veh);
    SU32(veh, 0x17cc, 0);
    for (uint32_t i = 0; i < 8; i++)
        if (U32(sim, flags[i]) != 0) SU32(veh, 0x17cc, i + 1);
    return U32(veh, 0x17cc);
}
RX_FUNCTION(0x00466a30, rx_world_raven_state_code)

/* 00467440 vf53 (thiscall, stdcall id; RET 4; EDX = id): id is one of +0x17c0 / +0x17c4. */
uint32_t rx_world_raven_has_id(rx_addr veh, uint32_t id) {
    uint32_t r = 0;
    if (U32(veh, 0x17c0) == id) r = 1;
    if (U32(veh, 0x17c4) == id) r = 1;
    return r;
}
RX_FUNCTION(0x00467440, rx_world_raven_has_id)

/* 00467470 RavenVehicleATV::vf12 (thiscall): 0x2e with the ATV flag (+0x280), else 2. */
uint32_t rx_world_raven_atv_kind(rx_addr veh) { return U32(veh, 0x280) != 0 ? 0x2e : 2; }
RX_FUNCTION(0x00467470, rx_world_raven_atv_kind)

/* 00467490 vf54 (thiscall, stdcall id; RET 4): 0x20 when vfunc 0xd4 (id) holds, 0x3e when id is +0x17c8, else vfunc 0x30 (). */
uint32_t rx_world_raven_id_kind(rx_addr veh, uint32_t id) {
    world_frame f = world_frame_flat(2, 0);   /* push esi; push edi */
    uint32_t r;
    if (WORLD_VCALL(veh, 0xd4, id) != 0) r = 0x20;
    else if (U32(veh, 0x17c8) == id) r = 0x3e;
    else r = WORLD_VCALL0(veh, 0x30);
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00467490, rx_world_raven_id_kind)

/* 0046a380 (EAX = vehicle, stdcall float; RET 4): without the ATV flag, 00440da0 (EDI = the simulation, the float passed on
 * through the x87). */
void rx_world_raven_sim_440da0(rx_addr veh, float x) {
    world_frame f = world_frame_flat(2, 0);   /* push ecx; push edi */
    if (U32(veh, 0x280) == 0)
        WORLD_CALL_SP(4, (void)bike_stunt_latches(sim_of(veh), rx_bf(world_x87_pass(rx_fb(x)))));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0046a380, rx_world_raven_sim_440da0)

/* 0047bb40: the damage-behaviour manager singleton (0xdb1410): constructed once (guard bit 1 of 0xdb45f8: vtable 0x93cd74,
 * 12 behaviours of 0x410 bytes from 0xdb1420 by `vector constructor iterator` with 004798e0, destructor 00911f90 at exit). */
rx_addr rx_world_damage_manager_instance(void) {
    world_frame f = veh_frame_align8(0);
    if ((U32(0, 0x00db45f8) & 1) == 0) {
        SU32(0, 0x00db45f8, U32(0, 0x00db45f8) | 1);
        SU32(0, 0x00db1410, 0x0093cd74);
        WORLD_CALL_SP(16, rx_fx_vector_ctor_iterator(0x00db1420, 0x410, 0xc, 0x004798e0));   /* 004022d0, the registered port */
#ifdef RX_ORACLE
        WORLD_ORIG(0x008f9fbf, RX_CDECL, 0, 0x00911f90);                            /* _atexit (oracle only) */
#endif
    }
    world_frame_leave(&f);
    return 0x00db1410;
}
RX_FUNCTION(0x0047bb40, rx_world_damage_manager_instance)

/* 0047bd10 VehicleDamageBehaviorManager::vf0 (thiscall manager, stdcall world; RET 4): each of the 12 behaviours (0x410 bytes
 * from +0x10) with a target (+0x3fc) that appears in one of the world's contact events of this step (id A or B; events of 64
 * bytes at world +0, count +0x1900) gets 0047a970 (thiscall ECX = that event, stdcall behaviour). */
void rx_world_damage_manager_update(rx_addr mgr, rx_addr world) {
    world_frame f = veh_frame_align8(4);   /* push ecx; push ebx; push esi; push edi */
    rx_addr beh = mgr + 0x10;
    for (int left = 0xc; left != 0; left--, beh += 0x410) {
        SU32(f.esp, 0xc, (uint32_t)left);
        const uint32_t target = U32(beh, 0x3fc);
        if (target == 0) continue;
        const uint32_t n = U32(world, 0x1900);
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr ev = world + 0x40 * i;
            if (target == U32(ev, 0) || target == U32(ev, 4)) {
                WORLD_CALL_SP(4, rx_world_damage_apply_contact(ev, beh));   /* 0047a970 (ECX = the event) */
                break;
            }
        }
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0047bd10, rx_world_damage_manager_update)

/* 00466720 vf30 (thiscall, stdcall out; RET 4): the point between the two wheels' attach points in world space, raised by 3:
 * a = R(q) sim +0x8820 + sim +0x360, b = R(q) sim +0x8970 + sim +0x2c0 (q = the frame orientation sim +0x7c0, R = q v q*),
 * out = (b - a) * 0.5 + a + (0, 3, 0, 0). */
void rx_world_raven_wheels_mid(rx_addr veh, rx_addr out) {
    const rx_addr sim = sim_of(veh);
    const rx_v4 q = v4_ld(sim + 0x7c0);
    const rx_v4 a = v4_add(veh_rotate(q, v4_ld(sim + 0x8820)), v4_ld(sim + 0x360));
    const rx_v4 b = v4_add(veh_rotate(v4_ld(sim + 0x7c0), v4_ld(sim + 0x8970)), v4_ld(sim + 0x2c0));
    const rx_v4 mid = v4_add(v4_mul(v4_sub(b, a), v4_splat(0.5f /* 0x9322cc */)), a);
    v4_st(out, v4_add(mid, v4(0.0f /* 0x92e12c */, 3.0f /* 0x92fe48 */, 0.0f, 0.0f)));
}
RX_FUNCTION(0x00466720, rx_world_raven_wheels_mid)

/* 00466bd0 vf57 (thiscall, stdcall wheel; RET 4; result st(0)): the wheel's grip factor sqrt-shaped: with wheel 0 / 1 in
 * contact (sim +0x89e0 / +0x8890), a = n . up' (n = the contact normal sim +0x8a10 / +0x88c0, up' = R(q) up with the writable
 * up vector 0xd616a0), b = n . up; s = max(a, 0) * 1.1 - 0.1, t = clamp(s, 0, 1), lo = max(0.1, t) (0.1 when s <= 0); the result
 * is pow(b < 0 ? lo : b == 0 ? lo : b >= 1 ? t : (t - lo) * b + lo, 0.5) (_CIpow). Without contact a = 1, b = 1. */
rx_f80 rx_world_raven_grip_factor(rx_addr veh, uint32_t wheel) {
    world_frame f = world_frame_enter(0x10, 0, 1);
    const rx_addr sim = sim_of(veh);
    const rx_v4 upr = veh_rotate(v4_ld(sim + 0x7c0), v4_ld(0x00d616a0));
    float a = 1.0f /* 0x931f24 */, b = 1.0f;
    rx_addr n = 0;
    if (wheel == 0) { if (U32(sim, 0x89e0) != 0) n = sim + 0x8a10; }
    else if (wheel == 1) { if (U32(sim, 0x8890) != 0) n = sim + 0x88c0; }
    if (n != 0) {
        const rx_v4 nv = v4_ld(n);
        a = phys_dot3(nv, upr);
        b = phys_dot3(nv, v4_ld(0x00d616a0));
        if (0.0f > a) a = 0.0f;
    }
    const float s = phys_sub(phys_mul(a, 1.1f /* 0x9774c4 */), 0.1f /* 0x931d94 */);
    float t, lo = 0.1f;
    if (s >= 1.0f) { t = 1.0f; lo = 0.1f > t ? 0.1f : t; }
    else if (!(0.0f >= s)) { t = s; lo = 0.1f > t ? 0.1f : t; }   /* 0 < s, or unordered */
    else t = 0.0f;
    float x;
    if (0.0f > b || !(0.0f < b || b != b)) x = lo;          /* b < 0, or b == 0 */
    else if (b >= 1.0f) x = t;
    else x = phys_add(phys_mul(phys_sub(t, lo), b), lo);
    SF32(f.esp, 0xc, x);
    const rx_f80 r = veh_ci_pow(rx_f80_from_f32(x), rx_f80_from_f64(0.5 /* 0x9774e8 */));
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x00466bd0, rx_world_raven_grip_factor)

/* 004647c0 vf60 (thiscall vehicle, stdcall (unused); RET 4): the vehicle's render nodes from the simulation: the model matrix
 * (+0xf0 to the node ref +0x1328 + 0x10), the frame node +0x1560 (rotation +0xe0, position +0xc0), the fork / swingarm nodes
 * +0x1564 / +0x1568 (rotations sim +0x8a80 / +0x900), the rear wheel node +0x156c (position sim +0x2c0), the front axle node
 * +0x1570 (sim +0x8a90 times a fixed tilt quaternion: axis normalize([0xd615c0]).xyz, angle from 0.1875 through
 * Vec4Cos_RangeReduced: (sin.xyz, cos.w) * axis), the node +0x1574 (sim +0x9a0); then the rider pose 004674e0 unless the
 * simulation's mode +0x2de4 is 3. Rotations / positions go through a stack copy (their callees take addresses). */
void rx_world_raven_update_nodes(rx_addr veh) {
    world_frame f = world_frame_enter(0x5c, 1, 1);   /* push ebp; and esp, -16; sub esp, 0x5c; push esi */
    const rx_addr s10 = f.esp + 0x10, s20 = f.esp + 0x20, s30 = f.esp + 0x30, s40 = f.esp + 0x40;
    const world_ctx cx = WCTX(.esi = veh);
    veh_model_node_set_matrix(U32(veh, 0x1328) + 0x10, veh + 0xf0, WCTX(.eax = veh + 0xf0, .esi = veh));
    veh_node_set_world_rotation(U32(veh, 0x1560), veh + 0xe0, cx);
    veh_node_set_world_position(U32(veh, 0x1560), veh + 0xc0, WCTX(.eax = U32(veh, 0x1560), .edx = veh + 0xc0, .esi = veh));
    v4_st(s10, v4_ld(sim_of(veh) + 0x8a80));
    veh_node_set_world_rotation(U32(veh, 0x1564), s10, WCTX(.edx = s10, .esi = veh));
    v4_st(s10, v4_ld(sim_of(veh) + 0x900));
    veh_node_set_world_rotation(U32(veh, 0x1568), s10, cx);
    v4_st(s20, v4_ld(sim_of(veh) + 0x2c0));
    veh_node_set_world_position(U32(veh, 0x156c), s20, WCTX(.eax = s20, .edx = sim_of(veh), .esi = veh));
    const rx_v4 qa = v4_ld(sim_of(veh) + 0x8a90);
    v4_st(s10, qa);
    const rx_v4 axis = v4_or(v4_xyz(v4_normalize_or_nan(v4_ld(0x00d615c0))), v4(0.0f, 0.0f, 0.0f, 1.0f) /* 0x92db60 */);
    v4_st(s40, axis);
    veh_vec4_sincos(v4_splat(0.1875f /* 0x977dc0 */), s20, s30, WCTX(.eax = s30, .esi = veh));
    const rx_v4 cs = v4_ld(s20), sn = v4_ld(s30);
    const rx_v4 tilt = v4_mul(v4(sn.v[0], sn.v[1], sn.v[2], cs.v[3]), axis);   /* (s30 & xyz) | (s20 & w) */
    v4_st(s10, veh_quat_mul(qa, tilt));
    veh_node_set_world_rotation(U32(veh, 0x1570), s10, cx);
    v4_st(s10, v4_ld(sim_of(veh) + 0x9a0));
    veh_node_set_world_rotation(U32(veh, 0x1574), s10, cx);
    if (U32(sim_of(veh), 0x2de4) != 3) WORLD_CALL_SP(4, rx_world_rider_pose(veh));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004647c0, rx_world_raven_update_nodes)

/* A node's world / local matrices: the node record (16 bytes) holds its model at +0, its index +0xb and its parent's index
 * +0xf (the model's node records at model +8); the model's world matrices at +0x1c and local matrices at +0x18, 64 bytes each. */
static rx_addr node_world_matrix(rx_addr node) { return U32(U32(node, 0), 0x1c) + ((uint32_t)U8(node, 0xb) << 6); }
static rx_addr node_local_matrix(rx_addr node) { return U32(U32(node, 0), 0x18) + ((uint32_t)U8(node, 0xb) << 6); }

/* The general 4x4 inverse of the original (cofactor expansion along the rows, every product / difference in its order):
 * cof = the 16 signed cofactors (row-major), inv[i][j] = cof[j][i] / det (the transpose through 00401760). Rows a b c d. */
static float inverse4(const float m[4][4], float inv[4][4]) {
#define MU phys_mul
#define SB phys_sub
#define AD phys_add
    const float *a = m[0], *b = m[1], *c = m[2], *d = m[3];
    const float s5 = SB(MU(d[3], c[2]), MU(d[2], c[3]));
    const float s10 = SB(MU(d[3], b[2]), MU(d[2], b[3]));
    const float s13 = SB(MU(c[3], b[2]), MU(c[2], b[3]));
    const float s19 = SB(MU(d[3], a[2]), MU(d[2], a[3]));
    const float s22 = SB(MU(c[3], a[2]), MU(c[2], a[3]));
    const float s30 = SB(MU(a[2], b[3]), MU(a[3], b[2]));
    float cof[16];
    cof[0] = AD(SB(MU(b[1], s5), MU(c[1], s10)), MU(d[1], s13));
    const float s27 = AD(SB(MU(a[1], s5), MU(c[1], s19)), MU(d[1], s22));
    cof[8] = AD(SB(MU(a[1], s10), MU(b[1], s19)), MU(s30, d[1]));
    const float s40 = AD(SB(MU(a[1], s13), MU(b[1], s22)), MU(s30, c[1]));
    const float det = SB(AD(SB(MU(cof[0], a[0]), MU(b[0], s27)), MU(cof[8], c[0])), MU(s40, d[0]));
    const float rdet = phys_div(1.0f /* 0x931f24 */, det);
    cof[1] = SB(-0.0f /* 0x9774a8 */, AD(SB(MU(b[0], s5), MU(c[0], s10)), MU(d[0], s13)));
    const float s57 = SB(MU(d[3], c[1]), MU(d[1], c[3]));
    const float s60 = SB(MU(d[3], b[1]), MU(d[1], b[3]));
    const float s63 = SB(MU(b[1], c[3]), MU(c[1], b[3]));
    cof[2] = AD(SB(MU(s57, b[0]), MU(s60, c[0])), MU(s63, d[0]));
    const float s71 = SB(MU(d[2], c[1]), MU(d[1], c[2]));
    const float s74 = SB(MU(d[2], b[1]), MU(d[1], b[2]));
    const float s77 = SB(MU(b[1], c[2]), MU(c[1], b[2]));
    cof[3] = SB(-0.0f, AD(SB(MU(s71, b[0]), MU(s74, c[0])), MU(s77, d[0])));
    cof[4] = SB(-0.0f, s27);
    cof[5] = AD(SB(MU(s5, a[0]), MU(c[0], s19)), MU(d[0], s22));
    const float s92 = SB(MU(d[3], a[1]), MU(d[1], a[3]));
    const float s95 = SB(MU(a[1], c[3]), MU(c[1], a[3]));
    cof[6] = SB(-0.0f, AD(SB(MU(s57, a[0]), MU(s92, c[0])), MU(s95, d[0])));
    const float s104 = SB(MU(d[2], a[1]), MU(d[1], a[2]));
    const float s107 = SB(MU(a[1], c[2]), MU(c[1], a[2]));
    cof[7] = AD(SB(MU(s71, a[0]), MU(s104, c[0])), MU(s107, d[0]));
    cof[9] = SB(-0.0f, AD(SB(MU(s10, a[0]), MU(b[0], s19)), MU(s30, d[0])));
    const float s123 = SB(MU(a[1], b[3]), MU(b[1], a[3]));
    cof[10] = AD(SB(MU(s60, a[0]), MU(s92, b[0])), MU(s123, d[0]));
    const float s129 = SB(MU(a[1], b[2]), MU(b[1], a[2]));
    cof[11] = SB(-0.0f, AD(SB(MU(s74, a[0]), MU(s104, b[0])), MU(s129, d[0])));
    cof[12] = SB(-0.0f, s40);
    cof[13] = AD(SB(MU(s13, a[0]), MU(b[0], s22)), MU(s30, c[0]));
    cof[14] = SB(-0.0f, AD(SB(MU(s63, a[0]), MU(s95, b[0])), MU(s123, c[0])));
    cof[15] = AD(SB(MU(s77, a[0]), MU(s107, b[0])), MU(s129, c[0]));
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) inv[i][j] = MU(cof[4 * j + i], rdet);
    return rdet;
#undef MU
#undef SB
#undef AD
}

/* 004674e0 (stdcall vehicle; RET 4): the shake of two rider-attached nodes (vehicle +0x1554 / +0x1550, rest points +0x1540 /
 * +0x1530, timers +0x155c / +0x1558): per node its world matrix and its parent's are evaluated; the timer runs at 10 x the
 * frame dt (the game clock [[0xa95844] +0x10] +0x18); the rest point in world space (parent rows: ((a.x px + c.x pz) + b.x py)
 * + d.x, ...) is offset along the node's own y axis by Noise1D(timer) * 0.045 and brought back into the parent's frame through
 * the parent's inverse; a result more than 1 from the rest point snaps back to it; the node's local translation is set and the
 * node invalidated. */
void rx_world_rider_pose(rx_addr veh) {
    world_frame f = world_frame_enter(0x1e4, 3, 1);   /* push ebp; and esp, -16; sub esp, 0x1e4; push ebx, esi, edi */
    const float frame_dt = F32(U32(U32(0, 0x00a95844), 0x10), 0x18);
    const rx_addr nodes[2] = {U32(veh, 0x1554), U32(veh, 0x1550)};
    const rx_addr timers[2] = {veh + 0x155c, veh + 0x1558};
    float rest[2][3];
    for (int k = 0; k < 3; k++) { rest[0][k] = F32(veh, 0x1540 + 4 * k); rest[1][k] = F32(veh, 0x1530 + 4 * k); }
    for (int i = 0; i < 2; i++) {
        const rx_addr node = nodes[i];
        if (node == 0) continue;
        const world_ctx cx = WCTX(.ebx = timers[i], .edi = node);
        veh_node_evaluate_world(node, 0, cx);
        const rx_addr own = node_world_matrix(node);
        const float yx = F32(own, 0x10), yy = F32(own, 0x14), yz = F32(own, 0x18);   /* the node's y axis (row 1) */
        SF32(timers[i], 0, phys_add(phys_mul(frame_dt, 10.0f /* 0x92fd88 */), F32(timers[i], 0)));
        const float px = rest[i][0], py = rest[i][1], pz = rest[i][2];
        const rx_addr parent = U32(U32(node, 0), 8) + ((uint32_t)U8(node, 0xf) << 4);
        veh_node_evaluate_world(parent, 0, cx);
        const rx_addr pm = node_world_matrix(parent);
        float m[4][4], inv[4][4];
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++) m[r][c] = F32(pm, 0x10 * r + 4 * c);
        inverse4(m, inv);
        const float wx = phys_add(phys_add(phys_add(phys_mul(m[0][0], px), phys_mul(m[2][0], pz)), phys_mul(m[1][0], py)), m[3][0]);
        const float wy = phys_add(phys_add(phys_add(phys_mul(m[0][1], px), phys_mul(m[1][1], py)), phys_mul(m[2][1], pz)), m[3][1]);
        const float wz = phys_add(phys_add(phys_add(phys_mul(m[0][2], px), phys_mul(m[2][2], pz)), phys_mul(m[1][2], py)), m[3][2]);
        const float n = phys_mul(veh_noise1d(F32(timers[i], 0), cx), 0.045f /* 0x977a24 */);
        const float qx = phys_add(phys_mul(n, yx), wx), qz = phys_add(phys_mul(n, yz), wz), qy = phys_add(phys_mul(n, yy), wy);
        /* back into the parent's frame (row vector times the inverse) */
        float lx = phys_add(phys_add(phys_add(phys_mul(qx, inv[0][0]), phys_mul(qz, inv[2][0])), phys_mul(qy, inv[1][0])), inv[3][0]);
        float lz = phys_add(phys_add(phys_add(phys_mul(qz, inv[2][2]), phys_mul(qy, inv[1][2])), phys_mul(inv[0][2], qx)), inv[3][2]);
        float ly = phys_add(phys_add(phys_add(phys_mul(qz, inv[2][1]), phys_mul(qy, inv[1][1])), phys_mul(inv[0][1], qx)), inv[3][1]);
        const float dz = phys_sub(lz, pz), dy = phys_sub(ly, py), dx = phys_sub(lx, px);
        const float dist = veh_sqrtf(phys_add(phys_add(phys_mul(dz, dz), phys_mul(dy, dy)), phys_mul(dx, dx)), cx);
        if (dist > 1.0f /* 0x931f24 */) { lx = px; ly = py; lz = pz; }
        const rx_addr lm = node_local_matrix(node);
        SF32(lm, 0x30, lx); SF32(lm, 0x34, ly); SF32(lm, 0x38, lz);
        veh_node_invalidate(node, cx);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x004674e0, rx_world_rider_pose)

/* One keyed rotation track (0x18-byte record: keys* +0, cached key index +4, enabled +0xc, key count +0x10; keys of 12 bytes:
 * time, a packed quaternion) at time t into out (x, y, z, w; 16 bytes). Disabled or empty: out is left as it is. Up to two keys: the
 * slerp of key 0 and key 1 (one key: with itself at 0). More keys: the key pair around t found from the cached index (reset to 1
 * or to the last key when a quarter / three-quarter probe says so, then stepped up and down), slerped (QuatSlerp_Packed); a
 * time past the last key or before the first gives that key's quaternion unpacked as it is (w >= 0). The same code as the pose
 * sampler 00840440's rotation tracks (phys_anim_sample.c); the fraction goes to the slerp through x87 (fld / fstp). */
static void key_track_rotation(rx_addr track, float t, rx_addr out) {
    if (U32(track, 0xc) == 0 || I32(track, 0x10) < 1) return;
    const int32_t n = I32(track, 0x10);
    const rx_addr keys = U32(track, 0);
    if (n <= 2) {
        float frac = 0.0f;
        rx_addr b = keys + 4;
        if (n == 2) {
            const float t0 = F32(keys, 0);
            b = keys + 0xc + 4;
            frac = phys_div(phys_sub(t, t0), phys_sub(F32(keys, 0xc), t0));
        }
        WORLD_CALL_SP(4, phys_quat_slerp_packed(keys + 4, b, out, rx_bf(world_x87_pass(rx_fb(frac)))));
        return;
    }
    const int32_t last = n - 1, quarter = last >> 2, mid = (last >> 1) + quarter;
    if (I32(track, 4) > mid && F32(keys, 12u * (uint32_t)quarter) > t) SU32(track, 4, 1);
    else if (I32(track, 4) < quarter && t > F32(keys, 12u * (uint32_t)mid)) SU32(track, 4, (uint32_t)last);
    int32_t k = I32(track, 4) < 1 ? 1 : I32(track, 4);
    int inside = 1;
    while (t > F32(keys, 12u * (uint32_t)k)) {
        if (++k >= n) { k = last; inside = 0; break; }
    }
    while (F32(keys, 12u * (uint32_t)(k - 1)) > t) {
        if (--k < 1) { k = 1; inside = 0; break; }
    }
    SU32(track, 4, (uint32_t)k);
    const rx_addr a = keys + 12u * (uint32_t)(k - 1), b = keys + 12u * (uint32_t)k;
    const float frac = phys_div(phys_sub(t, F32(a, 0)), phys_sub(F32(b, 0), F32(a, 0)));
    if (inside) WORLD_CALL_SP(4, phys_quat_slerp_packed(a + 4, b + 4, out, rx_bf(world_x87_pass(rx_fb(frac)))));
    else v4_st(out, phys_quat_unpack(b + 4));
}

/* 00476340 KeyTrackBlock_Sample (stdcall block; RET 4): the vehicle's keyed-track block (vehicle +0x16f0: +0 the clip, +4 the
 * time, +8 the track index, +0xc the second rotation track's index) sampled at its time:
 *   +0x10 .. +0x18: the position from the clip's translation track `index` (clip +0x1c, 0x14-byte records: keys* +0, linear +8,
 *     key count +0xc; keys of 16 bytes: time, x, y, z): a linear track with more than one key through KeyTrack_FindSegment
 *     (inside the keys: prev + (next - prev) * f; outside: next + (next - prev) * f), else the key at or before t (004dbe00;
 *     the float components copied through the x87 stack: a signalling NaN comes out quiet);
 *   +0x20 .. +0x2c: the rotation of the clip's rotation track `index` (clip +0x24, 0x18-byte records), +0x30 .. +0x3c that of the
 *     track +0xc (key_track_rotation).
 * Frame: sub esp, 8; push ebx, ebp, esi, edi (FindSegment's prev / frac / next slots at esp + 0x10 / + 0x14 / + 0x1c). */
void rx_world_key_track_block_sample(rx_addr block) {
    const rx_addr entry = rx_sp();
    const rx_addr X = entry - 24;
    rx_set_sp(X);
    const float t = F32(block, 4);
    const rx_addr prev_slot = X + 0x10, frac_slot = X + 0x14, next_slot = X + 0x1c;
    {
        const rx_addr trk = U32(U32(block, 0), 0x1c) + U32(block, 8) * 0x14u;
        if (U32(trk, 8) != 0 && I32(trk, 0xc) > 1) {
            uint32_t inside;
            WORLD_CALL_SP(0xc, inside = rx_world_keytrack_find_segment(U32(trk, 0), U32(trk, 0xc), t, prev_slot, next_slot, frac_slot));
            const rx_addr p = U32(prev_slot, 0), q = U32(next_slot, 0);
            const float f = F32(frac_slot, 0);
            const rx_addr from = inside ? p : q;
            for (uint32_t i = 0; i < 3; i++) {
                SF32(block, 0x10 + 4 * i, phys_add(phys_mul(phys_sub(F32(q, 4 + 4 * i), F32(p, 4 + 4 * i)), f), F32(from, 4 + 4 * i)));
            }
        } else {
            uint32_t k;
            WORLD_CALL_SP(0, k = rx_keytrack_count_before(U32(trk, 0xc), U32(trk, 0), t));
            const uint32_t offset = k * 16u;
            SU32(block, 0x10, world_x87_pass(U32(U32(trk, 0), offset - 0xc)));
            SU32(block, 0x14, world_x87_pass(U32(U32(trk, 0), offset - 8)));
            SU32(block, 0x18, world_x87_pass(U32(U32(trk, 0), offset - 4)));
        }
    }
    key_track_rotation(U32(U32(block, 0), 0x24) + U32(block, 8) * 0x18u, F32(block, 4), block + 0x20);
    key_track_rotation(U32(U32(block, 0), 0x24) + U32(block, 0xc) * 0x18u, F32(block, 4), block + 0x30);
    rx_set_sp(entry);
}
RX_FUNCTION(0x00476340, rx_world_key_track_block_sample)

/* 004636f0 RavenVehicleMotocross::vf0_o90 (thiscall vehicle, stdcall dt; RET 4): the per-frame update after the base class's
 * (RavenVehicle::vf0_o90 0045d510): the effect timer +0x16f4 (+0x16e0 enabled; +0x16f0 block to 00476340 when set), the rider
 * input's lean target (rider object +0x1644: +0x24 / +0x28 cleared, +0x1c, +0x2c) from the player controller's mode
 * (+0x100: +0xb38 > 0, mode +0xb44: 2 / 3 -> +0x1c = 1, 5 / 6 -> 0, 8 / 9 / 11 / 12 -> +0x2c += 0.25 / 0.5 / -0.25 / -0.5
 * clamped to [[0x9cd9e8], 1]), the two input toggles (engine context [0xa95844] vfunc 2 -> input; its vfunc 0x1f (0x23 / 0x33,
 * 0): +0x8c28 flipped and +0x8c2c set / +0x8c2c set), and +0x2c copied to the rider's +0x5c30 object +0x40. */
void rx_world_raven_frame_update(rx_addr veh, float dt) {
    world_frame f = world_frame_flat(1, 0);   /* push esi */
    WORLD_CALL_SP(4, bike_vehicle_pre_step(veh, rx_bf(world_x87_pass(rx_fb(dt)))));   /* RavenVehicle::vf0_o90 */
    if (U32(veh, 0x16e0) != 0) {
        const uint32_t blk = U32(veh, 0x16f0);
        SF32(veh, 0x16f4, phys_add(F32(veh, 0x16f4), dt));
        if (blk != 0) WORLD_CALL_SP(4, rx_world_key_track_block_sample(veh + 0x16f0));
    }
    const rx_addr pc = U32(veh, 0x100);
    if (U32(veh, 0x1f0) != 0 && pc != 0 && F32(pc, 0xb38) > 0.0f) {
        const rx_addr r = U32(veh, 0x1644);
        SF32(r, 0x28, 0.0f); SF32(r, 0x24, 0.0f);
        const uint32_t mode = U32(U32(veh, 0x100), 0xb44);
        float delta = 0.0f;
        int lean = 0;
        switch (mode) {
        case 2: case 3: SF32(U32(veh, 0x1644), 0x1c, 1.0f /* 0x931f24 */); break;
        case 5: case 6: SF32(U32(veh, 0x1644), 0x1c, 0.0f); break;
        case 8: delta = 0.25f /* 0x92ff84 */; lean = 1; break;
        case 9: delta = 0.5f /* 0x9322cc */; lean = 1; break;
        case 11: delta = -0.25f; lean = 1; break;
        case 12: delta = -0.5f; lean = 1; break;
        default: break;
        }
        if (lean) {
            const rx_addr t = U32(veh, 0x1644) + 0x1c;
            float v = delta > 0.0f ? phys_add(F32(t, 0x10), delta) : phys_sub(F32(t, 0x10), -delta);
            SF32(t, 0x10, v);
            if (v > 1.0f) v = 1.0f;
            SF32(t, 0x10, v);
            const float lo = F32(0, 0x009cd9e8);
            if (lo > v) v = lo;
            SF32(t, 0x10, v);
        }
    }
    static const uint32_t keys[2] = {0x23, 0x33};
    for (int k = 0; k < 2; k++) {
        const rx_addr ctx = U32(0, 0x00a95844);
        const rx_addr in = WORLD_VCALL0(ctx, 8);
        if (WORLD_VCALL(in, 0x7c, keys[k], 0) != 0) {
            const rx_addr r = U32(veh, 0x1644);
            if (k == 0) SU32(r, 0x8c28, U32(r, 0x8c28) == 0);
            SU32(U32(veh, 0x1644), 0x8c2c, 1);
        }
    }
    const rx_addr r = U32(veh, 0x1644);
    SU32(U32(r, 0x5c30), 0x40, world_x87_pass(U32(r, 0x2c)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004636f0, rx_world_raven_frame_update)
