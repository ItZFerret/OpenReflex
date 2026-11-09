// build_scene_crowd.cpp -- oracle adapters for core/game/build_scene_crowd.c (group rx_build_crowd): the BillboardCrowdEntity (scene type 0x1041)
// construction, vf5, flash vector and destruction, verified per call inside a baton boot of a stadium / National event
// (REFLEX_PORT=rx_build_crowd REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_EVENT_SPEC=... : builds/w-npcs/boot.sh).
#include "oracle.h"
extern "C" {
uint32_t rx_billboard_crowd_event_construct(uint32_t ev);
void rx_billboard_crowd_event_destroy(uint32_t ev);
uint32_t rx_billboard_crowd_construct(uint32_t e);
uint32_t rx_billboard_crowd_vf5(uint32_t e, uint32_t reader);
void rx_billboard_flashes_init(uint32_t v, uint32_t count);
void rx_billboard_crowd_destroy(uint32_t e);
uint32_t rx_billboard_crowd_sdtor(uint32_t e, uint32_t flags);
}
using namespace reflex_port;

// 006c5df0 (EDI = the 0xd0 block; plain ret) -> EAX = the block
RX_PORT(0x006c5df0, BillboardCrowd_EventConstruct, "rx_build_crowd", OUT_EAX) { m.r[0] = rx_billboard_crowd_event_construct(m.r[7]); return m.ret(0); }
// 006c5c80 (ESI = the event; plain ret)
RX_PORT(0x006c5c80, BillboardCrowd_EventDestroy, "rx_build_crowd", 0) { rx_billboard_crowd_event_destroy(m.r[6]); return m.ret(0); }
// 006c5f30 (ESI = the entity; plain ret) -> EAX = the entity
RX_PORT(0x006c5f30, BillboardCrowd_Construct, "rx_build_crowd", OUT_EAX) { m.r[0] = rx_billboard_crowd_construct(m.r[6]); return m.ret(0); }
// 006c6180 vf5 (thiscall ECX = entity, stdcall reader; RET 4). Its own group: it reads the RSData through the core's rs_* readers (a fmt_entity),
// which a baton boot (the original's XML reader) does not have; verified by the canonical compare of the built scene (core/notes/construction.md 14.4)
RX_PORT(0x006c6180, BillboardCrowd_vf5, "rx_build_crowd_vf5", 0) { rx_billboard_crowd_vf5(m.r[1], m.arg(0)); return m.ret(4); }
// 006c6940 BillboardCrowd_InitFlashes (stdcall vector, count; RET 8)
RX_PORT(0x006c6940, BillboardCrowd_InitFlashes, "rx_build_crowd", 0) { rx_billboard_flashes_init(m.arg(0), m.arg(1)); return m.ret(8); }
// 006c6040 (EDI = the entity; plain ret)
RX_PORT(0x006c6040, BillboardCrowd_Destroy, "rx_build_crowd", 0) { rx_billboard_crowd_destroy(m.r[7]); return m.ret(0); }
// 006c5ff0 sdtor (thiscall ECX = entity, stdcall flags; RET 4) -> EAX = the entity
RX_PORT(0x006c5ff0, BillboardCrowd_sdtor, "rx_build_crowd", OUT_EAX) { m.r[0] = rx_billboard_crowd_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
