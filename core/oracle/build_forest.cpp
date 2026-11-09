// build_forest.cpp -- oracle adapter for core/game/build_scene_forest.c (group rx_build_forest): the world's forest install
// (FUN_00772940: the entity list block, the trunk collision shapes of 00667cc0), verified per call inside the boot
// (REFLEX_PORT=rx_build_forest REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 ...).
#include "oracle.h"
extern "C" {
void rx_physics_world_install_forest(uint32_t world);
}
using namespace reflex_port;

// 00772940 (EDI = the physics world; no stack arguments, plain ret; no result register)
RX_PORT(0x00772940, InstallForest, "rx_build_forest", 0) { rx_physics_world_install_forest(m.r[7]); return m.ret(0); }
