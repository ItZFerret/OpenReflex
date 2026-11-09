// loop_cam_schema.cpp -- oracle adapters for core/game/loop_cam_schema.c: the camera tuning schema initializer
// 00908500 (group rx_loop, the camera unit's group) and its two helpers 006596f0 / 00659ef0 (group rx_camschema:
// the helpers are only reached from inside 00908500, so a run that installs the parent must not install them too -
// the child's core fill would run on the parent's already-filled state; the helper group is run without 00908500).
#include "oracle.h"
extern "C" {
#include "../game/loop_cam_schema.h"
}
using namespace reflex_port;

// 00908500 (void): the nine class records at 0xaf6d10
RX_PORT(0x00908500, CameraSchemaInit, "rx_loop", 0) { rx_camera_schema_init(); return m.ret(0); }
// 006596f0 (ECX = class record) -> EAX = record
RX_PORT(0x006596f0, CameraSchemaConstruct, "rx_camschema", OUT_EAX) { return m.ret(0, rx_camera_schema_construct(m.r[1])); }
// 00659ef0 (stack = class record; RET 4)
RX_PORT(0x00659ef0, CameraSchemaFill, "rx_camschema", 0) { rx_camera_schema_fill(m.arg(0)); return m.ret(4); }
