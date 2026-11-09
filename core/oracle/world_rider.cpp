// world_rider.cpp -- oracle adapters for core/game/world_rider*.c (group rx_world, rider share).
#include "oracle.h"
extern "C" {
#include "../game/world_rider.h"
}
using namespace reflex_port;
using rx_oracle::f32;
#include <cstdio>
#include <set>
#include <string>
// Development aid: the x87 control word each x87-computing port runs with (written once per function to RIDER_CW_LOG).
static void cw_note(const char* name, const reflex_vm::Machine& m) {
    static std::set<std::string> seen;
    const char* path = getenv("RIDER_CW_LOG");
    if (!path || !seen.insert(name).second) return;
    if (FILE* f = fopen(path, "a")) { fprintf(f, "%s cw %04x\n", name, unsigned(m.fpu_cw)); fclose(f); }
}
static inline float xf(const reflex_vm::Machine& m, int i) { return f32(uint32_t(m.xmm[i].lo)); }   // xmm i lane 0

// 005452d0 (stdcall out; xmm3, xmm2, xmm1, xmm0 = x, y, z, w; RET 4). EAX = out.
RX_PORT(0x005452d0, Vec4_FromScalars, "rx_world", OUT_EAX) {
    const uint32_t out = m.arg(0);
    rx_world_vec4_from_scalars(out, xf(m, 3), xf(m, 2), xf(m, 1), xf(m, 0));
    return m.ret(4, out);
}
// 004dbce0 (EAX = keys, EDX = count, xmm0 = t; cdecl out prev, out next, out fraction).
RX_PORT(0x004dbce0, KeyTrack_FindSegment, "rx_world", OUT_EAX) {
    const uint32_t r = rx_world_keytrack_find_segment(m.r[0], m.r[2], xf(m, 0), m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0, r);
}
// 00552b90 (EDX = players; stack index, kind; RET 8).
RX_PORT(0x00552b90, FUN_00552b90, "rx_world", OUT_EAX) {
    return m.ret(8, rx_world_players_find(m.r[2], m.arg(0), m.arg(1)));
}
// 00625390 (EAX = look).
RX_PORT(0x00625390, FUN_00625390, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_lookat_find_target(m.r[0]));
}
// 006258a0 (ESI = look, EAX = rider).
RX_PORT(0x006258a0, FUN_006258a0, "rx_world", 0) {
    rx_world_lookat_set_target(m.r[6], m.r[0]);
    return m.ret(0);
}
// 004db710 (ECX = last positions, EDX = active; stack owner, count, touching, positions, velocities, slots, intensities; RET 0x1c).
RX_PORT(0x004db710, TerrainTracks_UpdateContactSlots, "rx_world", 0) {
    rx_world_tracks_update_contact_slots(m.r[1], m.r[2], m.arg(0), int32_t(m.arg(1)), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6));
    return m.ret(0x1c);
}
// 004db8e0 (ECX = surface, EDI = positions; stack prev, slots, scale, touching, group, weights, radii, dt; RET 0x20).
RX_PORT(0x004db8e0, TerrainTracks_AddContactPoints, "rx_world", 0) {
    cw_note("004db8e0", m);
    rx_world_tracks_add_contact_points(m.r[1], m.r[7], m.arg(0), m.arg(1), f32(m.arg(2)), m.arg(3), m.arg(4), m.arg(5), m.arg(6),
                                       f32(m.arg(7)));
    return m.ret(0x20);
}
// 0061ca60 (ESI = ra, xmm1 = dt).
RX_PORT(0x0061ca60, FUN_0061ca60, "rx_world", 0) {
    rx_world_rider_blend_overlay(m.r[6], xf(m, 1));
    return m.ret(0);
}
// 0061d350 (ESI = ra; stack dt; RET 4).
RX_PORT(0x0061d350, FUN_0061d350, "rx_world", 0) {
    rx_world_rider_update_lean(m.r[6], f32(m.arg(0)));
    return m.ret(4);
}
// 0061faa0 (EAX = ra; stack dt + 4 words; RET 0x14).
RX_PORT(0x0061faa0, FUN_0061faa0, "rx_world", 0) {
    rx_world_rider_update_body_weight(m.r[0], f32(m.arg(0)));
    return m.ret(0x14);
}
// 00625560 (EAX = look).
RX_PORT(0x00625560, FUN_00625560, "rx_world", OUT_EAX) {
    return m.ret(0, rx_world_lookat_update_search(m.r[0]));
}
// 00508480 (ECX = body, EAX = out).
RX_PORT(0x00508480, FUN_00508480, "rx_world", 0) {
    rx_world_body_forward(m.r[1], m.r[0]);
    return m.ret(0);
}
// 00508460 (ECX = body, EAX = out).
RX_PORT(0x00508460, FUN_00508460, "rx_world", 0) {
    rx_world_body_up(m.r[1], m.r[0]);
    return m.ret(0);
}
// 006255a0 (ESI = look, EDI = out, EAX = direction; stack h; RET 4).
RX_PORT(0x006255a0, FUN_006255a0, "rx_world", 0) {
    rx_world_lookat_aim_ahead(m.r[6], m.r[7], m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 00624ef0 (EAX = look; stack dt; RET 4).
RX_PORT(0x00624ef0, FUN_00624ef0, "rx_world", 0) {
    rx_world_lookat_update(m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 006230c0 (EAX = ra; stack dt; RET 4).
RX_PORT(0x006230c0, FUN_006230c0, "rx_world", 0) {
    rx_world_rider_update_pose_weights(m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 0061ecd0 (EAX = ra; stack dt; RET 4).
RX_PORT(0x0061ecd0, FUN_0061ecd0, "rx_world", 0) {
    rx_world_rider_update_steer_weight(m.r[0], f32(m.arg(0)));
    return m.ret(4);
}
// 00617e60 (stdcall shape, radius; RET 8).
RX_PORT(0x00617e60, FUN_00617e60, "rx_world", 0) {
    rx_world_shape_set_radius(m.arg(0), f32(m.arg(1)));
    return m.ret(8);
}
// 00624560 (ESI = ra). Returns with ECX = the cookie check's value (the cookie).
RX_PORT(0x00624560, FUN_00624560, "rx_world", 0) {
    rx_world_rider_check_body_hits(m.r[6]);
    return m.ret(0);
}
// 0061e770 (EAX = ra).
RX_PORT(0x0061e770, FUN_0061e770, "rx_world", 0) {
    rx_world_rider_update_limb_ranges(m.r[0]);
    return m.ret(0);
}
// 006206d0 (ECX = float* steer, EAX = float* lean pair; xmm0 out).
RX_PORT(0x006206d0, FUN_006206d0, "rx_world", OUT_XMM0) {
    const float r = rx_world_rider_lean_target(m.r[1], m.r[0]);
    m.xmm[0].lo = rx_oracle::bits(r); m.xmm[0].hi = 0;   // movss / andps / mulss: lanes 1..3 are 0
    return m.ret(0);
}
// 00620110 (EAX = ra; stack steer, b, lean, roll; RET 0x10).
RX_PORT(0x00620110, FUN_00620110, "rx_world", 0) {
    rx_world_rider_update_steer(m.r[0], f32(m.arg(0)), f32(m.arg(1)), f32(m.arg(2)), f32(m.arg(3)));
    return m.ret(0x10);
}
// 0061feb0 (EAX = ra; stack 4 unused floats; RET 0x10).
RX_PORT(0x0061feb0, FUN_0061feb0, "rx_world", 0) {
    rx_world_rider_update_crouch(m.r[0]);
    return m.ret(0x10);
}
// 006237a0 (ECX = ra, EAX = selector, ESI = force; stack fade time; RET 4).
RX_PORT(0x006237a0, FUN_006237a0, "rx_world", 0) {
    rx_world_rider_chains_fade_in(m.r[1], m.r[0], m.r[6], f32(m.arg(0)));
    return m.ret(4);
}
// 006235e0 (ECX = ra, EAX = selector; stack fade time, force; RET 8).
RX_PORT(0x006235e0, FUN_006235e0, "rx_world", 0) {
    rx_world_rider_chains_fade_out(m.r[1], m.r[0], f32(m.arg(0)), m.arg(1));
    return m.ret(8);
}
// 0061ea90 (ESI = ra).
RX_PORT(0x0061ea90, FUN_0061ea90, "rx_world", 0) { rx_world_rider_start_idle_gesture(m.r[6]); return m.ret(0); }
// 0061eac0 (EDI = ra; stack dt; RET 4).
RX_PORT(0x0061eac0, FUN_0061eac0, "rx_world", 0) {
    rx_world_rider_update_gesture(m.r[7], f32(m.arg(0)));
    return m.ret(4);
}
// 00620750 (EDI = ra).
RX_PORT(0x00620750, FUN_00620750, "rx_world", 0) { rx_world_rider_update_arm_chains(m.r[7]); return m.ret(0); }
// 00621030 (EAX = event, ECX = ra).
RX_PORT(0x00621030, FUN_00621030, "rx_world", 0) { rx_world_rider_chain_event(m.r[0], m.r[1]); return m.ret(0); }
// 00620fc0 (stdcall ra, clip, t1, t2; RET 0x10).
RX_PORT(0x00620fc0, FUN_00620fc0, "rx_world", 0) {
    rx_world_rider_play_clip_events(m.arg(0), m.arg(1), f32(m.arg(2)), f32(m.arg(3)));
    return m.ret(0x10);
}
// 00621320 (ECX = ra).
RX_PORT(0x00621320, FUN_00621320, "rx_world", 0) { rx_world_rider_end_trick(m.r[1]); return m.ret(0); }
// 00621490 (stdcall ra; RET 4).
RX_PORT(0x00621490, FUN_00621490, "rx_world", 0) { rx_world_rider_end_pose_override(m.arg(0)); return m.ret(4); }
// 0061dd60 (EAX = ra; stack dt; RET 4).
RX_PORT(0x0061dd60, FUN_0061dd60, "rx_world", 0) { rx_world_rider_update_trick(m.r[0], f32(m.arg(0))); return m.ret(4); }
// 0061d8b0 (EAX = ra; stack dt; RET 4).
RX_PORT(0x0061d8b0, FUN_0061d8b0, "rx_world", 0) { rx_world_rider_update_blends(m.r[0], f32(m.arg(0))); return m.ret(4); }
// 0061e120 (ESI = ra; stack dt; RET 4).
RX_PORT(0x0061e120, FUN_0061e120, "rx_world", 0) { rx_world_rider_update_idle(m.r[6], f32(m.arg(0))); return m.ret(4); }
// 0061dfd0 (ECX = ra; stack dt; RET 4).
RX_PORT(0x0061dfd0, FUN_0061dfd0, "rx_world", 0) { rx_world_rider_update_scripted(m.r[1], f32(m.arg(0))); return m.ret(4); }
// 00621400 (EAX = ra).
RX_PORT(0x00621400, FUN_00621400, "rx_world", 0) { rx_world_rider_end_stunt(m.r[0]); return m.ret(0); }
// 00622e30 (stdcall ra; RET 4).
RX_PORT(0x00622e30, FUN_00622e30, "rx_world", 0) { rx_world_rider_end_crash(m.arg(0)); return m.ret(4); }
// 00621b90 (EDI = ra; stack index; RET 4).
RX_PORT(0x00621b90, FUN_00621b90, "rx_world", 0) { rx_world_rider_start_scripted(m.r[7], m.arg(0)); return m.ret(4); }
// 00621b20 (EAX = ra, ESI = index).
RX_PORT(0x00621b20, FUN_00621b20, "rx_world", 0) { rx_world_rider_start_scripted_mode(m.r[0], m.r[6]); return m.ret(0); }
// 00621ab0 (EAX = ra).
RX_PORT(0x00621ab0, FUN_00621ab0, "rx_world", 0) { rx_world_rider_update_scripted_start(m.r[0]); return m.ret(0); }

// ---- 0061db10 / 00621850 / 006217a0: the scripted-mode / ragdoll lean and blend updates (hidden register-context calls) ----
// EDX after a pass over a listener vector (FloatListeners_Set): the last enabled target, else the value it had.
static uint32_t listeners_last_target(uint32_t v, uint32_t edx) {
    const uint32_t begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return edx;
    for (uint32_t p = begin; p != end; p += 8) if (U32(p, 4) != 0) edx = U32(p, 0);
    return edx;
}
// 0061db10 (EAX = ra; stdcall dt; RET 4). ECX comes back from the slot `push ecx` reserved (pop ecx): the entry value, unless the
// scripted branch ran, which keeps the normalised lean weight there (the float the listeners are given). EDX depends on the
// listeners call and on 00621400 (undeclared); x87 is untouched.
RX_PORT(0x0061db10, RiderAnim_UpdateScriptedLean, "rx_world", OUT_ECX) {
    const uint32_t ra = m.r[0], slot = m.r[4] - 4;
    const bool scripted = f32(0x3dcccccdu) > f32(U32(ra, 0x5b534)) && I32(ra, 0x5bb50) == 7;
    rx_world_rider_update_scripted_lean(ra, f32(m.arg(0)));
    if (scripted) m.r[1] = U32(slot, 0);
    return m.ret(4);
}
// 00621850 (EAX = ra; stdcall freeze; RET 4). ECX = the reserved slot (pop ecx), which holds the state's final weight (1.0 for
// states 0 / 1, else 0); EDX = the entry value when frozen, else the last enabled target of the listeners +0x360 (the float slot
// the call passed, entry esp + 4, was EDX going in).
RX_PORT(0x00621850, RiderAnim_RagdollBlend, "rx_world", OUT_ECX | OUT_EDX) {
    const uint32_t ra = m.r[0], freeze = m.arg(0), slot = m.r[4] + 4;
    rx_world_rider_ragdoll_blend(ra, freeze);
    m.r[1] = U32(m.r[4] - 4, 0);
    if (freeze == 0) m.r[2] = listeners_last_target(ra + 0x360, slot);
    return m.ret(4);
}
// 006217a0 (ESI = ra; stdcall dt; RET 4). ECX is 00620a60's leftover when it ran (undeclared).
RX_PORT(0x006217a0, RiderAnim_UpdateRagdoll, "rx_world", 0) {
    rx_world_rider_update_ragdoll(m.r[6], f32(m.arg(0)));
    return m.ret(4);
}
