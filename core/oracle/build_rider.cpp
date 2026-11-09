// build_rider.cpp -- oracle adapters for construction unit 7 (riders, core/game/build_rider*.c; group rx_build_rider): verified per
// call inside the boot (REFLEX_PORT=rx_build_rider REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton). Everything under an adapter runs as
// the core's code (registered functions) or the original (rx_call_original) and is compared as a whole.
#include "oracle.h"
extern "C" {
#include "../game/build_rider.h"
}
using namespace reflex_port;

// 00618180 RiderAnimation constructor (stdcall ra; RET 4) -> EAX
RX_PORT(0x00618180, RiderAnimationConstruct, "rx_build_rider", OUT_EAX) { m.r[0] = rx_rider_animation_construct(m.arg(0)); return m.ret(4); }
// 00619490 RiderAnimation setup (EAX = the hierarchy handle; stdcall ra, scene, tree, vehicle; RET 0x10)
RX_PORT(0x00619490, RiderAnimationInit, "rx_build_rider", 0) {
    rx_rider_animation_init(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 0082d040 AnimationEntity constructor (stdcall e; RET 4) -> EAX
RX_PORT(0x0082d040, AnimationEntityConstruct, "rx_build_rider", OUT_EAX) { m.r[0] = rx_rider_animation_entity_construct(m.arg(0)); return m.ret(4); }
// 00839a40 CharacterEntity constructor (stdcall e; RET 4) -> EAX
RX_PORT(0x00839a40, CharacterEntityConstruct, "rx_build_rider", OUT_EAX) { m.r[0] = rx_character_entity_construct(m.arg(0)); return m.ret(4); }
// 0053e920 race rider (EAX = id; stdcall manager, world, vehicle, callback; RET 0x10) -> EAX
RX_PORT(0x0053e920, RiderSpawn, "rx_build_rider", OUT_EAX) {
    m.r[0] = rx_rider_spawn(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 0053eab0 intro / podium rider (stdcall manager, world, id; RET 0xc) -> EAX
RX_PORT(0x0053eab0, RiderSpawnCharacter, "rx_build_rider", OUT_EAX) { m.r[0] = rx_rider_spawn_character(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 0053ddc0 VehicleLoader_Assemble (ECX = model, EDX = entry; s0 manager; RET 4)
RX_PORT(0x0053ddc0, VehicleLoaderAssemble, "rx_build_rider", 0) { rx_vehloader_assemble(m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
// 0053d8b0 VehicleLoader_OverrideMaterials (stdcall entry, a, b; RET 0xc) -> EAX
RX_PORT(0x0053d8b0, VehicleLoaderOverrideMaterials, "rx_build_rider", OUT_EAX) { m.r[0] = rx_vehloader_override_materials(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 00624990 RiderCharacter_BindMud (EAX = rc; stdcall scene, hierarchy; RET 8)
RX_PORT(0x00624990, RiderCharacterBind, "rx_build_rider", 0) { rx_rider_character_bind(m.r[0], m.arg(0), m.arg(1)); return m.ret(8); }
// 007d4d60 ModelAsset copy (ESI = destination, s0 source; RET 4)
RX_PORT(0x007d4d60, ModelAssetCopy, "rx_build_rider", 0) { rx_model_asset_copy(m.r[6], m.arg(0)); return m.ret(4); }
// 0053cd40 the rider tree of the vehicle type (ECX = vehicle; cdecl world) -> EAX
RX_PORT(0x0053cd40, RiderTreeHandle, "rx_build_rider", OUT_EAX) { m.r[0] = rx_rider_tree_handle(m.r[1]); return m.ret(0); }
// 0053cbd0 FactoryGearLoader (EDI = loader) -> EAX
RX_PORT(0x0053cbd0, GearLoaderConstruct, "rx_build_rider", OUT_EAX) { m.r[0] = rx_gear_loader_construct(m.r[7]); return m.ret(0); }
// 0053e8c0 the entry of a rider id (stdcall manager, rider id, high resolution; RET 0xc)
RX_PORT(0x0053e8c0, RiderEntryCreate, "rx_build_rider", 0) { rx_rider_entry_create(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 0053efa0 a gear change (ECX = &out, ESI = entry; stdcall manager, part; RET 8) -> EAX
RX_PORT(0x0053efa0, RiderEntrySetPending, "rx_build_rider", OUT_EAX) { m.r[0] = rx_rider_entry_set_pending(m.r[1], m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
// 004e74c0 the slot of a gear type (s0; RET 4) -> EAX
RX_PORT(0x004e74c0, GearTypeSlot, "rx_build_rider", OUT_EAX) { m.r[0] = rx_gear_type_slot(m.arg(0)); return m.ret(4); }
// 006aa660 ClipRecorder_Init (no arguments)
RX_PORT(0x006aa660, ClipRecorderInit, "rx_build_rider", 0) { rx_clip_init(); return m.ret(0); }
