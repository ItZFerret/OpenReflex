// anim_scenario_misc.cpp -- oracle adapters for core/game/anim_scenario_misc.c (group rx_anim).
#include "anim_oracle.h"
extern "C" {
#include "../game/anim_scenario_misc.h"
}
using namespace reflex_port;

ANIM_PORT(0x0068f300, Tree_FreeSubtree, "rx_anim", 0) { rx_tree_free_subtree(m.r[1], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x006bd760, CrewChief_SetShirt, "rx_anim", 0) { rx_crew_chief_set_shirt(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074b180, Podium_Release, "rx_anim", 0) { rx_podium_release(m.r[0], m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074c6d0, Podium_SetShirts, "rx_anim", 0) { rx_podium_set_shirts(m.r[7]); return m.ret(0); }
ANIM_PORT(0x0074ba40, PodiumRiders_Create, "rx_anim", 0) { rx_podium_riders_create(m.arg(0)); return m.ret(4); }
ANIM_PORT(0x0074ada0, Podium_Setup, "rx_anim", 0) { rx_podium_setup(m.r[0], m.r[1]); return m.ret(0); }
