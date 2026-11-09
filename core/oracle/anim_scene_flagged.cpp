// anim_scene_flagged.cpp -- oracle adapters for core/game/anim_scene_flagged.c (group rx_anim).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scene_flagged.h"
}
using namespace reflex_port;

// 007d2ab0 (stdcall scene; RET 4). Void.
ANIM_PORT(0x007d2ab0, SceneCollectFlag4, "rx_anim", 0) { rx_scene_collect_flag4(m.arg(0)); return m.ret(4); }
// 0040a9e0 (void)
ANIM_PORT(0x0040a9e0, CurrentSceneCollectFlag4, "rx_anim", 0) { rx_current_scene_collect_flag4(); return m.ret(0); }
