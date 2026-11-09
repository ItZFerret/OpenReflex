// race_snake.cpp -- oracle adapters for core/game/race_snake.c and race_snake_driver.c (group rx_party): the Snake party mode
// (MXActivitySnake), its SnakeDriver and SnakeActivityAudioLogic. The trails' adapters are race_snake_trail.cpp.
#include "race_oracle.h"
extern "C" {
#include "../game/race_snake.h"
}
using namespace reflex_port;

namespace {
struct SnakeFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit SnakeFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~SnakeFpuKeep() { m.fpu = fpu; } };
}

// ---- the activity ----
RX_PORT(0x0056b050, Snake_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_snake_construct(m.arg(0), m.r[1], m.r[0]); return m.ret(4); }
RX_PORT(0x0056b230, Snake_Destroy, "rx_party", 0) { rx_snake_destroy(m.r[7]); return m.ret(0); }
RX_PORT(0x0056b210, Snake_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_snake_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056d340, Snake_Vf1, "rx_party", 0) { rx_snake_vf1(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056b310, Snake_Vf2, "rx_party", 0) { rx_snake_vf2(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056bc00, Snake_Vf3, "rx_party", 0) { rx_snake_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x0056b530, Snake_Vf4, "rx_party", 0) { rx_snake_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0056b2b0, Snake_Vf5, "rx_party", 0) { rx_snake_vf5(m.r[1]); return m.ret(0); }
RX_PORT(0x0056b2d0, Snake_Vf6, "rx_party", 0) { rx_snake_vf6(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056ce80, Snake_Vf15, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_vf15(m.r[1]); return m.ret(0); }
RX_PORT(0x0056c200, Snake_Vf16, "rx_party", 0) { rx_snake_vf16(m.r[1]); return m.ret(0); }
RX_PORT(0x0056bd60, Snake_Vf18, "rx_party", 0) { rx_snake_vf18(m.r[1]); return m.ret(0); }
RX_PORT(0x0056bfd0, Snake_Vf23, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_vf23(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056c460, Snake_Vf25, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056c530, Snake_Vf26, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_vf26(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056c700, Snake_Vf27, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_vf27(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0056bec0, Snake_Vf32, "rx_party", 0) { rx_snake_vf32(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056caa0, Snake_Vf34, "rx_party", 0) { rx_snake_vf34(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056c550, Snake_Vf39, "rx_party", 0) { rx_snake_vf39(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056c5c0, Snake_Vf40, "rx_party", 0) { rx_snake_vf40(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056cae0, Snake_Vf42, "rx_party", 0) { rx_snake_vf42(m.r[1]); return m.ret(0); }
RX_PORT(0x0056be60, Snake_Vf43, "rx_party", OUT_EAX) { m.r[0] = (uint32_t)rx_snake_vf43(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(12); }
RX_PORT(0x0056bda0, Snake_Vf44, "rx_party", OUT_EAX) { m.r[0] = rx_snake_vf44(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056be10, Snake_Vf47, "rx_party", 0) { rx_snake_vf47(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0056b5c0, Snake_PlaceRound, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_place_round(m.arg(0)); return m.ret(4); }
RX_PORT(0x0056cd20, Snake_BoundaryReset, "rx_party", 0) { rx_snake_boundary_reset(m.r[6]); return m.ret(0); }
RX_PORT(0x0056d080, Snake_BoundaryShrink, "rx_party", 0) { rx_snake_boundary_shrink(m.r[7], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00588dc0, Snake_NetRemovedListen, "rx_party", 0) { rx_snake_net_removed_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588e80, Snake_NetBoundaryListen, "rx_party", 0) { rx_snake_net_boundary_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00588f40, Snake_NetRoundListen, "rx_party", 0) { rx_snake_net_round_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589000, Snake_NetRemovedUnlisten, "rx_party", 0) { rx_snake_net_removed_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00589060, Snake_NetBoundaryUnlisten, "rx_party", 0) { rx_snake_net_boundary_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005890c0, Snake_NetRoundUnlisten, "rx_party", 0) { rx_snake_net_round_unlisten(m.r[0], m.arg(0)); return m.ret(4); }

// ---- the audio logic ----
RX_PORT(0x00583550, SnakeAudio_Vf3, "rx_party", 0) { rx_snake_audio_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x005837c0, SnakeAudio_Vf4, "rx_party", 0) { rx_snake_audio_vf4(m.r[1]); return m.ret(0); }
RX_PORT(0x0058bf10, SnakeAudio_ListenRoundStarted, "rx_party", 0) { rx_snake_audio_listen_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058bfd0, SnakeAudio_ListenActivity, "rx_party", 0) { rx_snake_audio_listen_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c090, SnakeAudio_ListenEliminated, "rx_party", 0) { rx_snake_audio_listen_eliminated(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c150, SnakeAudio_ListenPlayerWins, "rx_party", 0) { rx_snake_audio_listen_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c210, SnakeAudio_ListenTrailHit, "rx_party", 0) { rx_snake_audio_listen_trail_hit(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c2d0, SnakeAudio_UnlistenRoundStarted, "rx_party", 0) { rx_snake_audio_unlisten_round_started(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c330, SnakeAudio_UnlistenActivity, "rx_party", 0) { rx_snake_audio_unlisten_activity(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x0058c390, SnakeAudio_UnlistenEliminated, "rx_party", 0) { rx_snake_audio_unlisten_eliminated(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c3f0, SnakeAudio_UnlistenPlayerWins, "rx_party", 0) { rx_snake_audio_unlisten_player_wins(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0058c450, SnakeAudio_UnlistenTrailHit, "rx_party", 0) { rx_snake_audio_unlisten_trail_hit(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00583a90, SnakeAudio_TrailHit, "rx_party", 0) { rx_snake_audio_trail_hit(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }

// ---- the driver ----
RX_PORT(0x00513910, SnakeDriver_Construct, "rx_party", OUT_EAX) { m.r[0] = rx_snake_driver_construct(m.r[1], m.arg(0), m.r[0], m.arg(1)); return m.ret(8); }
RX_PORT(0x0050df60, NetworkDriver_Sdtor, "rx_party", OUT_EAX) { m.r[0] = rx_network_driver_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053aef0, SnakeDriver_Listen, "rx_party", 0) { rx_snake_driver_listen(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0053afb0, SnakeDriver_Unlisten, "rx_party", 0) { rx_snake_driver_unlisten(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00513990, SnakeDriver_Vf1, "rx_party", 0) { rx_snake_driver_vf1(m.r[1]); return m.ret(0); }
RX_PORT(0x00513b30, SnakeDriver_Vf2, "rx_party", 0) { rx_snake_driver_vf2(m.r[1]); return m.ret(0); }
RX_PORT(0x00513c50, SnakeDriver_Vf3, "rx_party", 0) { rx_snake_driver_vf3(m.r[1]); return m.ret(0); }
RX_PORT(0x00513f30, SnakeDriver_Vf7, "rx_party", 0) { SnakeFpuKeep k(m); rx_snake_driver_vf7(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x005148f0, SnakeDriver_Vf9, "rx_party", 0) { rx_snake_driver_vf9(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00514c70, SnakeDriver_Vf16, "rx_party", 0) { rx_snake_driver_vf16(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00514cc0, SnakeDriver_Vf17, "rx_party", 0) { rx_snake_driver_vf17(m.r[1]); return m.ret(0); }
RX_PORT(0x00514790, SnakeDriver_Human, "rx_party", 0) { rx_snake_driver_human(m.r[0]); return m.ret(0); }
RX_PORT(0x005149b0, SnakeDriver_KeepState, "rx_party", 0) { rx_snake_driver_keep_state(m.r[1]); return m.ret(0); }
RX_PORT(0x0053b070, Snake_TrailHitRaise, "rx_party", 0) { rx_snake_trail_hit_raise(m.r[6], m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
