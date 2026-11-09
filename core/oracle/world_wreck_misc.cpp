// world_wreck_misc.cpp -- oracle adapters for core/game/world_wreck_misc.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_wreck.h"
}
using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;
static void xmm0_ss(reflex_vm::Machine& m, float v) { m.xmm[0].lo = bits(v); m.xmm[0].hi = 0; }

RX_PORT(0x00679000, SharedVf_Ret4, "rx_world", 0) { rx_world_shared_vf_ret4(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00703d50, SharedVf_Type10, "rx_world", OUT_EAX) { return m.ret(0, rx_world_shared_vf_type10(m.r[1])); }
RX_PORT(0x0065cab0, ForestSystem_vf22, "rx_world", OUT_EAX) { return m.ret(0, rx_world_forest_system_vf22(m.r[1])); }
RX_PORT(0x006d7870, KeyMap_vf11, "rx_world", OUT_EAX) { return m.ret(4, rx_world_key_map_vf11(m.r[1], m.arg(0))); }
// 0075c580 (EDX = track): EAX = the table, ECX = 1.
RX_PORT(0x0075c580, FUN_0075c580, "rx_world", OUT_EAX | OUT_ECX) {
    const uint32_t t = rx_world_active_sound_table(m.r[2]);
    m.r[1] = 1;
    return m.ret(0, t);
}
// 0067ec50 / 0067eca0: EAX = 0 / 1, EDX = 1, ECX = the game-mode record it looked at last (when there is a track).
static void bank_regs(reflex_vm::Machine& m) {
    m.r[2] = 1;
    if (U32(0, 0x00a946b8) == 0) return;
    uint32_t c = U32(0, 0x00a95844);
    if (U32(U32(c, 0x20), 0x34) != 1) c = U32(c, 0x1c);
    m.r[1] = c;
}
RX_PORT(0x0067ec50, FUN_0067ec50, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const int r = rx_world_crowd_bank_in_use();
    bank_regs(m);
    return m.ret(0, r ? 1u : 0u);
}
RX_PORT(0x0067eca0, FUN_0067eca0, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const int r = rx_world_ambience_bank_in_use();
    bank_regs(m);
    return m.ret(0, r ? 1u : 0u);
}
RX_PORT(0x00679420, FUN_00679420, "rx_world", OUT_EAX) { return m.ret(0, rx_world_crowd_slot(m.r[0])); }
RX_PORT(0x0067abc0, FUN_0067abc0, "rx_world", OUT_XMM0) {
    xmm0_ss(m, rx_world_crowd_slot_level(m.r[0], m.r[2]));
    return m.ret(0);
}
RX_PORT(0x00679540, FUN_00679540, "rx_world", OUT_XMM0) {
    xmm0_ss(m, rx_world_crowd_level(m.r[6]));
    return m.ret(0);
}
RX_PORT(0x00674440, FUN_00674440, "rx_world", 0) { rx_world_crowd_excite(f32(m.arg(0))); return m.ret(0); }
// 006745d0 (EAX = kind): ECX kept.
RX_PORT(0x006745d0, FUN_006745d0, "rx_world", OUT_ECX) {
    const uint32_t ecx = m.r[1];
    rx_world_crowd_react(m.r[0]);
    m.r[1] = ecx;
    return m.ret(0);
}
// 00674540 (ECX = kind): ECX = the excitement it popped from its local.
RX_PORT(0x00674540, FUN_00674540, "rx_world", OUT_ECX) {
    const uint32_t kind = m.r[1];
    rx_world_crowd_event(int32_t(kind));
    uint32_t b = U32(0, 0x009c47dc + 4 * kind);
    if ((b & 0x7fffffffu) > 0x7f800000u) b |= 0x400000u;
    m.r[1] = b;
    return m.ret(0);
}
RX_PORT(0x006b0480, FUN_006b0480, "rx_world", 0) { rx_world_ik_restart_fades(m.r[6]); return m.ret(0); }
RX_PORT(0x006b7560, FUN_006b7560, "rx_world", OUT_EAX) {
    rx_world_get_three_vectors(m.r[1], m.arg(0));
    return m.ret(4, m.arg(0));
}
RX_PORT(0x006def20, FUN_006def20, "rx_world", OUT_XMM0) {
    xmm0_ss(m, rx_world_timer_seconds(m.r[0]));
    return m.ret(0);
}
// 00702ad0 (ESI = map; stdcall out, key; RET 8): EAX = ECX = out, EDX = the iterator's container.
RX_PORT(0x00702ad0, StringMap_Find, "rx_world", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t out = rx_world_string_map_find(m.r[6], m.arg(0), m.arg(1));
    m.r[1] = out; m.r[2] = U32(out, 0);
    return m.ret(8, out);
}
RX_PORT(0x006b6d80, FUN_006b6d80, "rx_world", 0) { rx_world_keyframe_blend_select(m.r[0]); return m.ret(0); }
RX_PORT(0x006aa460, ClipRecorder_FillEntry, "rx_world", 0) {
    rx_world_clip_recorder_fill_entry(m.r[7], m.arg(0), m.arg(1));
    return m.ret(8);
}
RX_PORT(0x006abbc0, ClipRecorder_FillEntries, "rx_world", 0) { rx_world_clip_recorder_fill_entries(m.arg(0)); return m.ret(0); }
RX_PORT(0x006aacf0, ClipRecorder_Candidate, "rx_world", 0) {
    rx_world_clip_recorder_candidate(m.r[1], m.r[2], f32(m.arg(0)), f32(m.arg(1)));
    return m.ret(0);
}
RX_PORT(0x006aaca0, ClipRecorder_CandidateOfType, "rx_world", OUT_ECX) {
    const uint32_t ecx = m.r[1];
    rx_world_clip_recorder_candidate_of_type(m.r[1], m.r[2]);
    m.r[1] = ecx;
    return m.ret(0);
}
RX_PORT(0x006ab8a0, ClipRecorder_LogWreck, "rx_world", 0) { rx_world_clip_recorder_log_wreck(m.r[0], m.arg(0)); return m.ret(0); }
RX_PORT(0x006ab930, ClipRecorder_LogStunt, "rx_world", 0) { rx_world_clip_recorder_log_stunt(m.r[0], m.arg(0)); return m.ret(0); }
RX_PORT(0x006ab9b0, ClipRecorder_LogRecover, "rx_world", OUT_FPU) { rx_world_clip_recorder_log_recover(m.r[0]); return m.ret(0); }
