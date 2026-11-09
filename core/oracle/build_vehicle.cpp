// build_vehicle.cpp -- oracle adapters for construction unit 6 (core/game/build_vehicle*.c, group rx_build_vehicle): the vehicle
// objects verified per call inside the boot (REFLEX_PORT=rx_build_vehicle REFLEX_PORT_VERIFY=1 REFLEX_BOOT_SCHED=baton): the core
// function runs on the emulated state, then the original from the same state; memory writes and the declared registers compared.
// ABI glue only.
#include "fx_oracle.h"
extern "C" {
#include "../game/build_vehicle.h"
}
using namespace reflex_port;
namespace {
thread_local reflex_vm::Machine* param_load_observed;
struct ParamLoadObserve {
    reflex_vm::Machine* previous;
    explicit ParamLoadObserve(reflex_vm::Machine& m) : previous(param_load_observed) { param_load_observed = &m; }
    ~ParamLoadObserve() { param_load_observed = previous; }
};
}
extern "C" void rx_param_load_set_xmm0(uint64_t low) {
    if (param_load_observed) { param_load_observed->xmm[0].lo = low; param_load_observed->xmm[0].hi = 0; }
}

#define G "rx_build_vehicle"
// RavenVehicleMotocross_Construct 004622b0 (ESI = this) -> EAX
RX_PORT(0x004622b0, VehMotoConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_motocross_construct(m.r[6]); return m.ret(0); }
// 0045cec0 (stdcall this, p2; RET 8) -> EAX
RX_PORT(0x0045cec0, RavenVehicleConstruct, G, OUT_EAX) { m.r[0] = rx_raven_vehicle_construct(m.arg(0), m.arg(1)); return m.ret(8); }
// 00430b50 RavenSimulation (stdcall S, rays; RET 8) -> EAX
RX_PORT(0x00430b50, RavenSimulationConstruct, G, OUT_EAX) { m.r[0] = rx_raven_simulation_construct(m.arg(0), m.arg(1)); return m.ret(8); }
// 0043ac40 RavenSimulationMotocross (stdcall S, vehicle block, engine class; RET 0xc) -> EAX
RX_PORT(0x0043ac40, SimMotoConstruct, G, OUT_EAX) { m.r[0] = rx_sim_moto_construct(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 0043b750 (ESI = S)
RX_PORT(0x0043b750, SimMotoMasses, G, 0) { rx_sim_moto_setup_masses(m.r[6]); return m.ret(0); }
// RavenVehicleMotocross::vf28 004625d0 (thiscall V, block; RET 4)
RX_PORT(0x004625d0, VehMotoVf28, G, 0) { rx_vehicle_moto_vf28(m.r[1], m.arg(0)); return m.ret(4); }
// RavenVehicle::vf28 004602c0 (thiscall V, block; RET 4)
RX_PORT(0x004602c0, RavenVehicleVf28, G, 0) { rx_raven_vehicle_vf28(m.r[1], m.arg(0)); return m.ret(4); }
// RavenSimulationMotocross::vf2 0044a650 (thiscall S, pos*, q*; RET 8)
// 004331d0 (EDI = the replay record; stdcall S; RET 4)
RX_PORT(0x0044a650, SimMotoReset, G, 0) { rx_sim_moto_reset(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0045f600 (ECX = axis b; stack V, pos, axis a; RET 0xc)
RX_PORT(0x0045f600, VehiclePlace, G, 0) { rx_vehicle_place(m.arg(0), m.arg(1), m.arg(2), m.r[1]); return m.ret(0xc); }
// RavenSimulationMotocross::vf18 0044eee0 (ECX = S)
RX_PORT(0x0044eee0, SimMotoVf18, G, 0) { rx_sim_moto_vf18(m.r[1]); return m.ret(0); }
// 00462b90 (stdcall V; RET 4)
RX_PORT(0x00462b90, VehMotoProxy, G, 0) { rx_vehicle_moto_create_proxy(m.arg(0)); return m.ret(4); }
// 00769160 (ECX = h1, EDX = h2; stack world, out, material; RET 0xc) -> EAX = out
RX_PORT(0x00769160, ProxyObjectCreate, G, OUT_EAX) { m.r[0] = rx_proxy_object_create(m.arg(0), m.arg(1), m.arg(2), m.r[1], m.r[2]); return m.ret(0xc); }
// 0084fcc0 (ESI = record, EDI = dims, s0 = material; RET 4) -> EAX
RX_PORT(0x0084fcc0, DynamicRecordInit, G, OUT_EAX) { m.r[0] = rx_phys_dynamic_record_init(m.r[6], m.r[7], m.arg(0)); return m.ret(4); }
// 00844240 (cdecl out, a1, a2, share) -> EAX = out
RX_PORT(0x00844240, ShapeHandleFor, G, OUT_EAX) { m.r[0] = rx_shape_handle_for(m.arg(0), m.arg(1), m.arg(2), (int)(m.arg(3) & 0xff)); return m.ret(0); }
// 007743a0 (stdcall vector, &record; RET 8)
RX_PORT(0x007743a0, DynamicPush, G, 0) { rx_phys_dynamic_push(m.arg(0), m.arg(1)); return m.ret(8); }
// 00774120 (ECX = vector, EDI = index)
RX_PORT(0x00774120, DynamicInsert, G, 0) { rx_phys_dynamic_insert(m.r[1], m.r[7]); return m.ret(0); }
// 00460190 (EDI = V, s0 = handle; RET 4)
RX_PORT(0x00460190, VehModelCreate, G, 0) { rx_vehicle_model_create(m.r[7], m.arg(0)); return m.ret(4); }
// 00542e00 (ECX = list, s0 = hierarchy id; RET 4) -> EAX
RX_PORT(0x00542e00, VehHierarchyHandle, G, OUT_EAX) { m.r[0] = rx_vehicle_hierarchy_handle(m.r[1], (int32_t)m.arg(0)); return m.ret(4); }
// 007c33a0 TerrainTracks_AllocSlot (EDX = tracks, s0 = kind; RET 4) -> EAX
RX_PORT(0x007c33a0, TracksAllocSlot, G, OUT_EAX) { m.r[0] = (uint32_t)rx_tracks_alloc_slot(m.r[2], m.arg(0)); return m.ret(4); }
// 00543130 VehicleSpec_ApplyTuning (s0 = spec; RET 4)
RX_PORT(0x00543130, SpecApplyTuning, G, 0) { rx_vehicle_spec_apply_tuning(m.arg(0)); return m.ret(4); }
// 00543020 VehicleSpec_ControllerOf (s0 = id; RET 4) -> EAX
RX_PORT(0x00543020, SpecControllerOf, G, OUT_EAX) { m.r[0] = (uint32_t)rx_vehicle_spec_controller_of((int32_t)m.arg(0)); return m.ret(4); }
// the Rdb readers
RX_PORT(0x00402160, TableColumnIndex, G, OUT_EAX) { m.r[0] = rx_tbl_column_index(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00402220, DatabaseFindTable, G, OUT_EAX) { m.r[0] = rx_tbl_find(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00418ec0, TableRow, G, OUT_EAX) { m.r[0] = rx_tbl_row(m.arg(0), (int32_t)m.mem.read(m.r[0], 4)); return m.ret(4); }
RX_PORT(0x00418e50, TableRows, G, OUT_EAX) { m.r[0] = rx_tbl_rows(m.arg(0), (int32_t)m.mem.read(m.r[0], 4)); return m.ret(4); }
// VehicleFactory_CreateVehicle 00542b90 (EDX = list; stack MXWorld, spec id; RET 8) -> EAX
RX_PORT(0x00542b90, VehicleFactoryCreate, G, OUT_EAX) { m.r[0] = rx_vehicle_factory_create(m.arg(0), m.r[2], m.arg(1)); return m.ret(8); }
// 00541310 VehicleParts_Assemble (ECX = material map, EDX = list; s0 instance, s1 spec; RET 8)
RX_PORT(0x00541310, VehPartsAssemble, G, 0) { rx_vehicle_parts_assemble(m.r[1], m.r[2], m.arg(0), m.arg(1)); return m.ret(8); }
// 00543db0 VehicleLoader_FindPackage (EAX = cache, EDI = name) -> EAX
RX_PORT(0x00543db0, VehLoaderFindPackage, G, OUT_EAX) { m.r[0] = rx_vehloader_find_package(m.r[0], m.r[7]); return m.ret(0); }
// 00773290 std::map<uint, uint>::operator[] (stdcall map, &key; RET 8) -> EAX
RX_PORT(0x00773290, UintMapIndex, G, OUT_EAX) { m.r[0] = rx_uint_map_index(m.arg(0), m.arg(1)); return m.ret(8); }
// the package cache (build_vehicle_pkgcache.c)
RX_PORT(0x005436d0, PkgCacheConstruct, G, OUT_EAX) { m.r[0] = rx_pkgcache_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x005440f0, PkgCacheIndex, G, OUT_EAX) { m.r[0] = rx_pkgcache_index(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00543750, PkgCacheRequestDb, G, 0) { rx_pkgcache_request_db(m.r[6], (int)m.arg(0)); return m.ret(4); }
RX_PORT(0x00543830, PkgCacheAdd, G, 0) { rx_pkgcache_add(m.r[7], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00543b30, PkgCacheRequest, G, 0) { rx_pkgcache_request(m.r[0], m.arg(0), (int)m.arg(1)); return m.ret(8); }
RX_PORT(0x00543c30, PkgCacheRelease, G, 0) { rx_pkgcache_release(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00543990, PkgCacheUnmark, G, 0) { rx_pkgcache_unmark(m.r[1], m.r[0]); return m.ret(0); }
RX_PORT(0x00543cd0, PkgCacheLoaded, G, OUT_EAX) { m.r[0] = rx_pkgcache_loaded(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00543e50, PkgCacheUpdate, G, 0) { rx_pkgcache_update(m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0041b840, PkgCacheErase, G, OUT_EAX) { m.r[0] = rx_pkgcache_erase(m.arg(0), m.arg(1), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x006d3c60, OnlineContentFind, G, OUT_EAX) { m.r[0] = rx_online_content_find(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00540610, VehListInitLoader, G, 0) { rx_vehicle_list_init_loader(m.arg(0)); return m.ret(4); }
// the specs (build_vehicle_spec.c)
RX_PORT(0x00542b20, VehListCreateSpec, G, OUT_EAX) { m.r[0] = rx_vehicle_list_create_spec(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x005401b0, SpecRequestPackages, G, 0) { rx_spec_request_packages(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x005403b0, PartRequestPackages, G, 0) { rx_part_request_packages(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004ec210, TableRefValue, G, OUT_EAX) { m.r[0] = rx_tbl_ref_value(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x0041aaf0, TableCellWord, G, OUT_EAX) { m.r[0] = rx_tbl_cell_word(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x0041a480, TableCellString, G, OUT_EAX) { m.r[0] = rx_tbl_cell_string(m.arg(0), m.r[7]); return m.ret(4); }
RX_PORT(0x00419b20, TableRefString, G, OUT_EAX) { m.r[0] = rx_tbl_ref_string(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x005404f0, VehListConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_list_construct(m.r[6]); return m.ret(0); }
// the ATV (build_vehicle_atv.c)
RX_PORT(0x0047c4a0, VehAtvConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_atv_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x0047c7f0, VehAtvVf28, G, 0) { rx_vehicle_atv_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00486760, VehAtvVf49, G, 0) { rx_vehicle_atv_vf49(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0048bde0, SimAtvConstruct, G, OUT_EAX) { m.r[0] = rx_sim_atv_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x004da440, SimAtvVf18, G, 0) { rx_sim_atv_vf18(m.r[1]); return m.ret(0); }
// the four-wheel classes (build_vehicle_wheeled.c)
RX_PORT(0x004c03a0, VehUtvConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_utv_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x004a2550, VehBuggyConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_buggy_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x0046a3e0, VehTruckConstruct, G, OUT_EAX) { m.r[0] = rx_vehicle_truck_construct(m.r[6]); return m.ret(0); }
RX_PORT(0x004c0700, VehUtvVf28, G, 0) { rx_vehicle_utv_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004a2960, VehBuggyVf28, G, 0) { rx_vehicle_buggy_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0046a850, VehTruckVf28, G, 0) { rx_vehicle_truck_vf28(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004c7020, VehUtvVf49, G, 0) { rx_vehicle_utv_vf49(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004ac6c0, VehBuggyVf49, G, 0) { rx_vehicle_buggy_vf49(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004735f0, VehTruckVf49, G, 0) { rx_vehicle_truck_vf49(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004c0c00, VehUtvProxy, G, 0) { rx_vehicle_utv_create_proxy(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004a2f00, VehBuggyProxy, G, 0) { rx_vehicle_buggy_create_proxy(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0046aef0, VehTruckProxy, G, 0) { rx_vehicle_truck_create_proxy(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x004cba90, SimUtvConstruct, G, OUT_EAX) { m.r[0] = rx_sim_utv_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x004b1260, SimBuggyConstruct, G, OUT_EAX) { m.r[0] = rx_sim_buggy_construct(m.arg(0)); return m.ret(4); }
RX_PORT(0x0044fe40, SimTruckConstruct, G, OUT_EAX) { m.r[0] = rx_sim_truck_construct(m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x004cb360, SimUtvParamsLoad, G, 0) { FxBind fx(m); ParamLoadObserve observe(m); rx_sim_utv_params_load(m.arg(0)); return m.ret(4); }
RX_PORT(0x004b0b30, SimBuggyParamsLoad, G, 0) { FxBind fx(m); ParamLoadObserve observe(m); rx_sim_buggy_params_load(m.arg(0)); return m.ret(4); }
RX_PORT(0x0044f690, SimTruckParamsLoad, G, 0) { FxBind fx(m); ParamLoadObserve observe(m); rx_sim_truck_params_load(m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x004cc030, SimUtvMasses, G, 0) { rx_sim_utv_setup_masses(m.r[2]); return m.ret(0); }
RX_PORT(0x004b1800, SimBuggyMasses, G, 0) { rx_sim_buggy_setup_masses(m.r[2]); return m.ret(0); }
RX_PORT(0x00450480, SimTruckMasses, G, 0) { rx_sim_truck_setup_masses(m.r[2]); return m.ret(0); }
