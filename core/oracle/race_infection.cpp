// race_infection.cpp -- oracle adapters for core/game/race_infection.c, race_snake_rules.c and race_snake_wall.c (group rx_party): the
// Infection party mode (MXActivityInfection), its InfectionDriver, RuleInfection and audio logic, RuleOutOfSnakeBounds and the boundary
// wall (XenonSnakeWallVisibleMesh) with its render-side helpers.
#include "race_oracle.h"
extern "C" {
#include "../game/race_snake.h"
}
using namespace reflex_port;

namespace {
struct InfFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit InfFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~InfFpuKeep() { m.fpu = fpu; } };
inline float xmm_f(reflex_vm::Machine& m, int i) { return rx_oracle::f32(uint32_t(m.xmm[i].lo)); }
}

// ---- the wall and its render-side helpers ----
RX_PORT(0x00554e50, FloatImage_Alloc, "rx_party", OUT_EAX) { m.r[0] = rx_float_image_alloc(m.r[6], m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x007b3380, TextureAsset_CreateArgb, "rx_party", 0) { rx_texture_asset_create_argb(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x006a1c10, Texture_FromFloatImage, "rx_party", OUT_EAX) { InfFpuKeep k(m); m.r[0] = rx_texture_from_float_image(m.arg(0), m.arg(1)); return m.ret(0); }
RX_PORT(0x007b12a0, ShaderAsset_VertexShader, "rx_party", 0) { rx_shader_asset_vertex_shader(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x007b1330, ShaderAsset_PixelShader, "rx_party", 0) { rx_shader_asset_pixel_shader(m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x0057efb0, SnakeWall_Shaders, "rx_party", 0) { rx_snake_wall_shaders(m.arg(0)); return m.ret(4); }
RX_PORT(0x0057ebb0, SnakeWall_Construct, "rx_party", OUT_EAX) { InfFpuKeep k(m); m.r[0] = rx_snake_wall_construct(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x0057f020, SnakeWall_Destroy, "rx_party", 0) { rx_snake_wall_destroy(m.r[6]); return m.ret(0); }
RX_PORT(0x0057ef90, SnakeWall_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_snake_wall_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00592b70, SnakeWall_SdtorO90, "rx_party", OUT_EAX) { m.r[0] = rx_snake_wall_sdtor_o90(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x006a0e20, SnakeMesh_SceneAdd, "rx_party", 0) { rx_snake_mesh_scene_add(m.r[0], m.r[6]); return m.ret(0); }
RX_PORT(0x0055fba0, RotateXz, "rx_party", 0) { rx_rotate_xz(xmm_f(m, 1), rx_oracle::f32(m.arg(0)), rx_oracle::f32(m.arg(1)), m.r[1], m.r[2]); return m.ret(0); }
RX_PORT(0x0057fc00, SnakeWall_BuildRing, "rx_party", OUT_EAX) {
    InfFpuKeep k(m); race_check_pc53(m, "0057fc00");
    m.r[0] = rx_snake_wall_build_ring(m.r[7], m.r[0], rx_oracle::f32(m.arg(0)), rx_oracle::f32(m.arg(1))); return m.ret(8);
}

// ---- the activity ----
RX_PORT(0x00572d00, Infection_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_infection_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x00572e70, Infection_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_infection_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00572eb0, Infection_Vf2, "rx_party", 0) { rx_infection_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00573080, Infection_Vf3, "rx_party", 0) { rx_infection_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00573140, Infection_Vf4, "rx_party", 0) { rx_infection_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x00573180, Infection_Vf15, "rx_party", 0) { InfFpuKeep k(m); rx_infection_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x00573330, Infection_Vf16, "rx_party", 0) { rx_infection_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x00573ab0, Infection_Vf18, "rx_party", 0) { rx_infection_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x00573d90, Infection_Vf23, "rx_party", 0) { InfFpuKeep k(m); rx_infection_vf23(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00573f90, Party_Vf25, "rx_party", 0) { InfFpuKeep k(m); rx_party_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00574060, Infection_Vf27, "rx_party", 0) { InfFpuKeep k(m); rx_infection_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00574110, Infection_Vf32, "rx_party", 0) { rx_infection_vf32(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00573b80, Infection_Vf40, "rx_party", 0) { rx_infection_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00574140, Infection_Vf42, "rx_party", 0) { rx_infection_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x00573b20, Infection_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_infection_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x00573af0, Infection_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_infection_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00574290, Infection_Vf47, "rx_party", 0) { rx_infection_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00573450, Infection_PlaceRound, "rx_party", 0) { InfFpuKeep k(m); rx_infection_place_round(m.arg(0)); return m.ret(4); }
RX_PORT(0x00574370, Infection_BoundaryReset, "rx_party", 0) { rx_infection_boundary_reset(m.r[6]); return m.ret(0); }
RX_PORT(0x005744c0, Infection_BoundaryShrink, "rx_party", 0) { rx_infection_boundary_shrink(m.r[7], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00589bd0, Infection_NetRemovedListen, "rx_party", 0) { rx_infection_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589c90, Infection_NetBoundaryListen, "rx_party", 0) { rx_infection_net_boundary_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589d50, Infection_NetRemovedUnlisten, "rx_party", 0) { rx_infection_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589db0, Infection_NetBoundaryUnlisten, "rx_party", 0) { rx_infection_net_boundary_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589e10, Party_PlayerWins, "rx_party", 0) { rx_party_player_wins(m.r[6], m.r[1], m.r[2], m.arg(0)); return m.ret(4); }

// ---- the driver and its rules ----
RX_PORT(0x00515690, InfectionDriver_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_infection_driver_construct(m.r[1], m.arg(0), m.r[0]); return m.ret(4); }
RX_PORT(0x005156e0, InfectionDriver_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_infection_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00515720, InfectionDriver_Vf1, "rx_party", 0) { rx_infection_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x005158c0, InfectionDriver_Vf2, "rx_party", 0) { rx_infection_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x005159d0, InfectionDriver_Vf3, "rx_party", 0) { rx_infection_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00515b60, InfectionDriver_Vf7, "rx_party", 0) { InfFpuKeep k(m); rx_infection_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00515df0, InfectionDriver_Vf9, "rx_party", 0) { rx_infection_driver_vf9(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00515d70, InfectionDriver_Human, "rx_party", 0) { rx_infection_driver_human(m.r[0]); return m.ret(0); }
RX_PORT(0x00516120, InfectionDriver_Ai, "rx_party", 0) { InfFpuKeep k(m); rx_infection_driver_ai(m.r[6], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0053b0d0, Driver_RuleKind9, "rx_party", OUT_EAX) { m.r[0] = rx_driver_rule_kind9(m.arg(0)); return m.ret(4); }
RX_PORT(0x0053b170, InfectionDriver_Listen, "rx_party", 0) { rx_infection_driver_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053b230, InfectionDriver_Unlisten, "rx_party", 0) { rx_infection_driver_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589e70, InfectionRule_Create, "rx_party", OUT_EAX) { m.r[0] = rx_infection_rule_create(m.r[1]); return m.ret(0); }
RX_PORT(0x0051c880, InfectionRule_Vf1, "rx_party", 0) { rx_infection_rule_vf1(m.r[1], m.arg(0)); return m.ret(4); }
// RX_INF_TRACE=1: each RuleInfection's address, its driver and the driver's infected flag on its first call (diagnostics only)
static void inf_trace(uint32_t rule) {
    static uint32_t seen[16]; static int n = 0; if (!std::getenv("RX_INF_TRACE")) return;
    for (int i = 0; i < n; i++) if (seen[i] == rule) return;
    if (n < 16) seen[n++] = rule;
    const uint32_t d = U32(rule, 4);
    std::fprintf(stderr, "INF rule %08x driver %08x infected %u act %08x\n", rule, d, U8(d, 0xf14), U32(U32(d, 0x74), 0x94));
}
RX_PORT(0x0051c8a0, InfectionRule_Vf5, "rx_party", 0) { inf_trace(m.r[1]); InfFpuKeep k(m); rx_infection_rule_vf5(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00589120, SnakeBoundsRule_Create, "rx_party", OUT_EAX) { m.r[0] = rx_snake_bounds_rule_create(m.r[1]); return m.ret(0); }
RX_PORT(0x0051bb30, SnakeBoundsRule_Vf1, "rx_party", 0) { rx_snake_bounds_rule_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051bb50, SnakeBoundsRule_Vf4, "rx_party", 0) { rx_snake_bounds_rule_vf4(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0051bb70, SnakeBoundsRule_Vf5, "rx_party", 0) { InfFpuKeep k(m); rx_snake_bounds_rule_vf5(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056bf20, Snake_OutOfBounds, "rx_party", OUT_EAX) { m.r[0] = rx_snake_out_of_bounds(m.r[0], m.r[1]); return m.ret(0); }
RX_PORT(0x00574300, Infection_OutOfBounds, "rx_party", OUT_EAX) { m.r[0] = rx_infection_out_of_bounds(m.r[0], m.r[6]); return m.ret(0); }

// ---- the audio logic ----
RX_PORT(0x00584460, InfectionAudio_Vf3, "rx_party", 0) { rx_infection_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00584670, InfectionAudio_Vf4, "rx_party", 0) { rx_infection_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058cb70, InfectionAudio_ListenRoundStarted, "rx_party", 0) { rx_infection_audio_listen_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058cc30, InfectionAudio_ListenActivity, "rx_party", 0) { rx_infection_audio_listen_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058ccf0, InfectionAudio_ListenEliminated, "rx_party", 0) { rx_infection_audio_listen_eliminated(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058cdb0, InfectionAudio_ListenPlayerWins, "rx_party", 0) { rx_infection_audio_listen_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058ce70, InfectionAudio_UnlistenRoundStarted, "rx_party", 0) { rx_infection_audio_unlisten_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058ced0, InfectionAudio_UnlistenActivity, "rx_party", 0) { rx_infection_audio_unlisten_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058cf30, InfectionAudio_UnlistenEliminated, "rx_party", 0) { rx_infection_audio_unlisten_eliminated(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058cf90, InfectionAudio_UnlistenPlayerWins, "rx_party", 0) { rx_infection_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00584880, InfectionAudio_Infected, "rx_party", 0) { rx_infection_audio_infected(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
