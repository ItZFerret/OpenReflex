// race_tag.cpp -- oracle adapters for core/game/race_tag*.c (group rx_party): the Tag party mode (MXActivityTag), its TagDriver,
// RuleTag and TagActivityAudioLogic.
#include "race_oracle.h"
extern "C" {
#include "../game/race_tag.h"
}
using namespace reflex_port;

namespace {
struct TagFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit TagFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~TagFpuKeep() { m.fpu = fpu; } };
}

// ---- the activity ----
RX_PORT(0x0056d550, Tag_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_tag_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x0056d680, Tag_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_tag_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056d6e0, Tag_Vf1, "rx_party", 0) { rx_tag_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056d720, Tag_Vf2, "rx_party", 0) { rx_tag_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056e1a0, Tag_Vf3, "rx_party", 0) { rx_tag_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0056d9d0, Tag_Vf4, "rx_party", 0) { rx_tag_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0056fe60, Tag_Vf15, "rx_party", 0) { rx_tag_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x0056feb0, Tag_Vf16, "rx_party", 0) { rx_tag_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0056e510, Tag_Vf18, "rx_party", 0) { rx_tag_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0056e800, Tag_Vf25, "rx_party", 0) { TagFpuKeep k(m); rx_tag_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056fe30, Tag_Vf32, "rx_party", 0) { rx_tag_vf32(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056e8d0, Tag_Vf39, "rx_party", 0) { rx_tag_vf39(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056e790, Tag_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_tag_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x0056e6e0, Tag_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_tag_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056e750, Tag_Vf47, "rx_party", 0) { rx_tag_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056e390, Tag_BallPhysics, "rx_party", 0) { rx_tag_ball_physics(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0056da60, Tag_PlaceRound, "rx_party", 0) { rx_tag_place_round(m.arg(0)); return m.ret(4); }
RX_PORT(0x00589170, Tag_NetRemovedListen, "rx_party", 0) { rx_tag_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589230, Tag_NetActivityListen, "rx_party", 0) { rx_tag_net_activity_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005892f0, Tag_NetRoundListen, "rx_party", 0) { rx_tag_net_round_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005893c0, Tag_NetActivityUnlisten, "rx_party", 0) { rx_tag_net_activity_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589420, Tag_NetRemovedUnlisten, "rx_party", 0) { rx_tag_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589480, Tag_NetRoundUnlisten, "rx_party", 0) { rx_tag_net_round_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00570300, Tag_NetObjectRemoved, "rx_party", 0) { rx_tag_net_object_removed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005708d0, Tag_NetRoundComplete, "rx_party", 0) { rx_tag_net_round_complete(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x006964d0, PlaceableEffect_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_placeable_effect_construct(m.r[1]); return m.ret(0); }

// ---- the driver and its rule ----
RX_PORT(0x00514ce0, TagDriver_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_tag_driver_construct(m.r[1], m.arg(0), m.r[0], m.arg(1)); return m.ret(8); }
RX_PORT(0x00514d30, TagDriver_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_tag_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00514d60, TagDriver_Vf1, "rx_party", 0) { rx_tag_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x00514e40, TagDriver_Vf2, "rx_party", 0) { rx_tag_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x00514f20, TagDriver_Vf3, "rx_party", 0) { rx_tag_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00515140, TagDriver_Vf7, "rx_party", 0) { TagFpuKeep k(m); rx_tag_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00515330, TagDriver_Ai, "rx_party", 0) { TagFpuKeep k(m); rx_tag_driver_ai(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00515440, TagDriver_WaterKick, "rx_party", 0) { rx_tag_driver_water_kick(m.r[6]); return m.ret(0); }
RX_PORT(0x00515490, TagDriver_Vf9, "rx_party", 0) { rx_tag_driver_vf9(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005155d0, TagDriver_Vf16, "rx_party", 0) { rx_tag_driver_vf16(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00515650, TagDriver_Vf17, "rx_party", 0) { rx_tag_driver_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x0053b120, TagDriver_Rule, "rx_party", OUT_EAX) { m.r[0] = rx_tag_driver_rule(m.arg(0)); return m.ret(4); }
RX_PORT(0x005894e0, TagRule_Create, "rx_party", OUT_EAX) { m.r[0] = rx_tag_rule_create(m.r[1]); return m.ret(0); }
RX_PORT(0x0051bd30, TagRule_Vf1, "rx_party", 0) { rx_tag_rule_vf1(m.r[1], m.arg(0)); return m.ret(4); }

// ---- the audio logic ----
RX_PORT(0x00583ab0, TagAudio_Vf3, "rx_party", 0) { rx_tag_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00583d30, TagAudio_Vf4, "rx_party", 0) { rx_tag_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058c4b0, TagAudio_ListenRoundStarted, "rx_party", 0) { rx_tag_audio_listen_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c570, TagAudio_ListenActivity, "rx_party", 0) { rx_tag_audio_listen_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c630, TagAudio_ListenDriver, "rx_party", 0) { rx_tag_audio_listen_driver(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c6f0, TagAudio_ListenPlayerWins, "rx_party", 0) { rx_tag_audio_listen_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c7b0, TagAudio_UnlistenRoundStarted, "rx_party", 0) { rx_tag_audio_unlisten_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c810, TagAudio_UnlistenActivity, "rx_party", 0) { rx_tag_audio_unlisten_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c870, TagAudio_UnlistenDriver, "rx_party", 0) { rx_tag_audio_unlisten_driver(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c8d0, TagAudio_UnlistenPlayerWins, "rx_party", 0) { rx_tag_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00584df0, TagAudio_RoundStarted, "rx_party", 0) { rx_tag_audio_round_started(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(16); }
RX_PORT(0x00583a30, TagAudio_RoundOver, "rx_party", 0) { rx_tag_audio_round_over(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00583a50, TagAudio_MatchOver, "rx_party", 0) { rx_tag_audio_match_over(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00583fe0, TagAudio_BallDropped, "rx_party", 0) { rx_tag_audio_ball_dropped(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00584000, TagAudio_BallGrabbed, "rx_party", 0) { rx_tag_audio_ball_grabbed(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// ---- the ball's PhysicsEntity placement (world_veh_entity.c) ----
extern "C" {
#include "../game/world_veh.h"
}
RX_PORT(0x004037c0, PhysicsEntity_Place, "rx_party", 0) { rx_physics_entity_place(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00403940, PhysicsEntity_Teleport, "rx_party", 0) { rx_physics_entity_teleport(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0051bd70, TagRule_Vf5, "rx_party", 0) { TagFpuKeep k(m); rx_tag_rule_vf5(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// RX_TAG_TRACE=<n>: every n-th InProgress frame of the activity (the native's entry state) one TAG line: the holder, each driver's
// round time / total / rounds won (diagnostics only)
static void tag_trace(uint32_t act) {
    static uint32_t n = 0; const char* e = std::getenv("RX_TAG_TRACE"); if (!e) return;
    if (n++ % uint32_t(std::atoi(e) > 0 ? std::atoi(e) : 300)) return;
    std::fprintf(stderr, "TAG call %u act %08x holder %08x state %u winner %08x over %u:", n - 1, act, U32(act, 0xff078), U32(act, 0x21c), U32(act, 0xff398), U32(act, 0xff394));
    for (int32_t i = 0; i < I32(act, 0x26c); i++) { const uint32_t d = U32(U32(act, 0x268), 4u * uint32_t(i)); std::fprintf(stderr, " [%08x %.2f %.2f %u]", d, F32(d, 0xf14), F32(d, 0xf18), U32(d, 0xf1c)); }
    std::fprintf(stderr, "\n");
}
RX_PORT(0x0056e940, Tag_Vf28, "rx_party", 0) { tag_trace(m.r[1]); TagFpuKeep k(m); rx_tag_vf28(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00570b30, Tag_Vf27, "rx_party", 0) { TagFpuKeep k(m); rx_tag_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00570f00, Tag_Vf29, "rx_party", 0) { TagFpuKeep k(m); rx_tag_vf29(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00570970, Tag_Vf40, "rx_party", 0) { rx_tag_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x005700a0, Tag_Vf42, "rx_party", 0) { rx_tag_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x0056e320, Tag_BallImpulse, "rx_party", 0) { rx_tag_ball_impulse(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0056f210, Tag_BallVisible, "rx_party", 0) { rx_tag_ball_visible(m.r[0]); return m.ret(0); }
RX_PORT(0x0056f290, Tag_BallWater, "rx_party", 0) { TagFpuKeep k(m); rx_tag_ball_water(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x0056fa50, Tag_BallCannonball, "rx_party", 0) { rx_tag_ball_cannonball(m.r[6]); return m.ret(0); }
RX_PORT(0x0056fcf0, Tag_BallGround, "rx_party", 0) { rx_tag_ball_ground(m.r[6]); return m.ret(0); }
RX_PORT(0x00770d00, World_BodyVelocityClear, "rx_party", 0) { rx_world_body_velocity_clear(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00570fc0, Tag_BallContactValue, "rx_party", OUT_XMM0) {
    (void)rx_tag_ball_contact_value(m.arg(0));   // the core's value is lane 0; the register as the original leaves it (movss / movaps)
    const uint32_t body = rx_world_physics_handle_body(U32(U32(m.arg(0), 0xff090), 0x158)), rec = U32(body + 0x28, 4);
    if (I32(rec, 8) == 2) { m.xmm[0].lo = U32(rec, 0x20) | (uint64_t)U32(rec, 0x24) << 32; m.xmm[0].hi = U32(rec, 0x28) | (uint64_t)U32(rec, 0x2c) << 32; }
    else { m.xmm[0].lo = 0x3f800000u; m.xmm[0].hi = 0; }
    return m.ret(4);
}
RX_PORT(0x007efe90, Tag_WakeQuad, "rx_party", 0) { TagFpuKeep k(m); rx_tag_wake_quad(m.r[0], m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), rx_oracle::f32(m.arg(4)), rx_oracle::f32(m.arg(5))); return m.ret(0x18); }
