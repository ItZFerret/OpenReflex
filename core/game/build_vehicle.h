/* build_vehicle.h -- construction unit 6 "vehicle" (core/notes/construction.md section 15): the race vehicles, from the vehicle spec
 * (the game's vehicle list MXGame + 0x154d8) through VehicleFactory_CreateVehicle 00542b90 to the simulation objects the race frames
 * run (bike_ / world_ / phys_ ports), plus the package cache the vehicle and rider requesters load their parts through, and the
 * generic Rdb table readers the construction code uses.
 *
 * STATUS markers: [done] transliterated + registered; [planned] the API is fixed (other units may code against it), the body is
 * being written (calling it before it lands aborts with the address).
 *
 * ---- RavenVehicleMotocross V (0x17d0 bytes, _aligned_malloc; vtables 0x93cd84, +0x90 0x93ce88, +0x94 0x93ce9c, +0x9c 0x93cedc) ----
 *   +0x14   the vehicle's index in the MXWorld's physics context (the physics world's +0x19f4 counter, under its lock)
 *   +0x188  the MXWorld (vf28's block +0x14)        +0x18c  the spec's id (VehicleList VehicleId)
 *   +0x190  the player slot P (written by the spawn 0054a050: V+0x190 = P)
 *   +0x194  the factory index (0 MotocrossBike, 1 Atv, 3 TrophyTruck, 5 Buggy, 7 Rhino/UTV; VehicleFactory_CreateVehicle)
 *   +0x198  0 (constructor)   +0x1a0  the rider's pointer (written by the rider setup)   +0x1a4..+0x1b0 the hand / foot nodes
 *   +0x234  the simulation S (also +0x16d4 for the MX: RavenSimulationMotocross 0x8fc0 bytes, vtable 0x93c8a4)
 *   +0x2a0  0x400 bytes 0xff (vf28 / the factory)   +0x6a0 a std::map (proxy, head +0x6b8, size +0x6bc)
 *   +0x1328 the vehicle's ModelAsset instance (0x150: a ModelAsset 007d3f30 copied from the hierarchy model, +0x140 2, +0x14e / +0x14f)
 *   +0x1324 the creation serial ([0x00a89bf4]++)   +0x1578..+0x1584 the four TerrainTracks slots (| 0x1000 / 0x2000)
 * Layouts of S: core/notes/bike.md, native/game/notes/bike.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the vehicle object [done] ---- */
rx_addr rx_vehicle_motocross_alloc(void);                 /* 0053f750: _aligned_malloc(0x17d0, 16) */
rx_addr rx_vehicle_motocross_construct(rx_addr v);        /* 004622b0 (ESI = v) */
rx_addr rx_raven_vehicle_construct(rx_addr v, uint32_t p2); /* 0045cec0 (stdcall; RET 8): the RavenVehicle base */

/* shared small constructors [done] */
rx_addr rx_veh_ranged3_alloc(rx_addr out, rx_addr src, uint32_t rate_bits);   /* 0042d700 (ESI = out, EDI = src, s0; RET 4) */
rx_addr rx_veh_map_head_alloc(void);                       /* 00706df0: an empty std::map head node (0x18) */
void    rx_veh_body_rec_init(rx_addr r);                   /* 0042e560 (ECX) */
void    rx_veh_rec9_init(rx_addr r);                       /* 0042e5c0 (EAX) */
void    rx_veh_rec_20_init(rx_addr r);                     /* 0042e620 (ECX) */
void    rx_veh_clamped_init(rx_addr r, uint32_t value_bits, uint32_t rate_bits, uint32_t limit_bits);   /* 0042d630 (EAX, xmm0, s0, s1) */
void    rx_veh_rec_00439240(rx_addr r);                    /* 00439240 (s0; RET 4) */

/* ---- the RavenSimulation objects (build_vehicle_sim.c) [done] ---- */
rx_addr rx_raven_simulation_construct(rx_addr S, uint32_t rays);              /* 00430b50 (stdcall; RET 8) */
rx_addr rx_sim_moto_construct(rx_addr S, rx_addr vblock, uint32_t cls);        /* 0043ac40 (stdcall S, V + 0x16e0, engine class; RET 0xc) */
void    rx_sim_moto_setup_masses(rx_addr S);                                   /* 0043b750 (ESI = S) */
void    rx_veh_tuning_registry_init(void);                                    /* 00790400 */

/* ---- proxy objects of the physics world (build_vehicle_proxy.c) [done] ---- */
rx_addr rx_phys_dynamic_record_init(rx_addr r, rx_addr dims, uint32_t material);   /* 0084fcc0 (ESI = r, EDI = dims, s0; RET 4) */
rx_addr rx_shape_from_pair(rx_addr h1, rx_addr h2);                         /* 00844360 (cdecl; h1 == h2 only) */
rx_addr rx_shape_handle_for(rx_addr out, rx_addr a1, rx_addr a2, int share); /* 00844240 (cdecl out, a1, a2, share) */
void    rx_phys_dynamic_push(rx_addr vec, rx_addr rec);                     /* 007743a0 (s0 = world + 0x1954, s1 = &record; RET 8) */
void    rx_phys_dynamic_insert(rx_addr vec, uint32_t index);                /* 00774120 (ECX = vector, EDI = index) */
rx_addr rx_proxy_object_create(rx_addr world, rx_addr out, uint32_t material, rx_addr h1, rx_addr h2);
                                                                            /* 00769160 (ECX = h1, EDX = h2, s0 world, s1 out, s2 material; RET 0xc) */

/* ---- the factory and the vehicle's construction (build_vehicle.c) [done unless marked] ----
 * VehicleFactory_CreateVehicle 00542b90 (EDX = the vehicle list (MXGame + 0x154b0: +0x28 specs, +0x2c count, +0x3c the
 * VehicleHierarchies table, +0x40 the made vehicles' list), s0 = the MXWorld, s1 = the spec id (VehicleList VehicleId = P + 0x1b0);
 * RET 8) -> EAX = the vehicle (also the spec's +0xc4). The spec (0xc8+: +4 id, +8 the model record {+8 hierarchy id, +0xc factory
 * index, +0x10, +0x14 engine class, +0x1c.. the eight tuning defaults}, +0xc.. +0x4c the 17 part slots, +0x50.. pending parts,
 * +0x94 CrewChief shirt texture, +0x9c PlayerVehicleId, +0xa0.. the tuning values, +0xc0 HR flag, +0xc4 the vehicle). The creation
 * block passed to vf28: {+0 model instance, +4 0, +8 model+0x10, +0xc engine class, +0x10 spec id, +0x14 the MXWorld}. */
rx_addr rx_vehicle_factory_create(rx_addr mxworld, rx_addr list, uint32_t spec_id);   /* 00542b90 */
rx_addr rx_vehicle_hierarchy_handle(rx_addr list, int32_t hierarchy_id);              /* 00542e00 (ECX = list, s0; RET 4) */
void    rx_vehicle_model_create(rx_addr V, rx_addr hierarchy_handle);                 /* 00460190 (EDI = V, s0; RET 4) */
void    rx_vehicle_parts_assemble(rx_addr map, rx_addr list, rx_addr inst, rx_addr spec);   /* 00541310 (ECX = V + 0x6a0, EDX = list,
                                                                                           s0 = V + 0x1328, s1 = spec; RET 8) [planned] */
void    rx_vehicle_moto_vf28(rx_addr V, rx_addr blk);           /* RavenVehicleMotocross::vf28 004625d0 (thiscall; RET 4) */
void    rx_raven_vehicle_vf28(rx_addr V, rx_addr blk);          /* RavenVehicle::vf28 004602c0 (thiscall; RET 4) */
void    rx_vehicle_moto_create_proxy(rx_addr V);                /* 00462b90 (stdcall V; RET 4) */
void    rx_sim_moto_vf18(rx_addr S);                            /* RavenSimulationMotocross::vf18 0044eee0 (ECX = S) */
int32_t rx_tracks_alloc_slot(rx_addr tracks, uint32_t kind);    /* 007c33a0 TerrainTracks_AllocSlot (EDX = tracks, s0; RET 4) */
int32_t rx_vehicle_spec_controller_of(int32_t vehicle_id);      /* 00543020 VehicleSpec_ControllerOf (s0; RET 4) */
void    rx_vehicle_spec_apply_tuning(rx_addr spec);             /* 00543130 VehicleSpec_ApplyTuning (s0; RET 4) */
/* placing (build_vehicle_place.c; also the race's respawn path) */
void    rx_sim_moto_reset(rx_addr S, rx_addr pos, rx_addr q);   /* RavenSimulationMotocross::vf2 0044a650 (thiscall S, pos*, q*; RET 8) */
void    rx_vehicle_place(rx_addr V, rx_addr pos, rx_addr axis_a, rx_addr axis_b);   /* 0045f600 (ECX = axis_b; stack V, pos, axis_a;
                                                                                       RET 0xc) */

/* ---- the ATV (factory 1; game/build_vehicle_atv.c) ---- */
rx_addr rx_vehicle_atv_alloc(void);                                     /* 0053f790 RavenVehicleATV_Alloc */
rx_addr rx_vehicle_atv_construct(rx_addr v);                            /* 0047c4a0 (ESI = v) */
void    rx_vehicle_atv_vf28(rx_addr v, rx_addr blk);                    /* 0047c7f0 RavenVehicleATV::vf28 (thiscall; RET 4) */
void    rx_vehicle_atv_vf49(rx_addr v, uint32_t value);                 /* 00486760 (thiscall; RET 4) */
rx_addr rx_sim_atv_construct(rx_addr S);                                /* 0048bde0 RavenSimulationATV_Construct (stdcall; RET 4) */
void    rx_sim_atv_vf18(rx_addr S);                                     /* 004da440 RavenSimulationATV::vf18 (ECX = S) */
/* the four-wheel classes (build_vehicle_wheeled.c): UTV (factory 7, the factory's 0x15a0 block), Buggy (5, 0x1620), TrophyTruck
 * (3, 0x1630) */
rx_addr rx_vehicle_utv_construct(rx_addr v);                            /* 004c03a0 RavenVehicleUTV_Construct (ESI = v) */
rx_addr rx_vehicle_buggy_construct(rx_addr v);                          /* 004a2550 RavenVehicleBuggy_Construct (ESI = v) */
rx_addr rx_vehicle_truck_construct(rx_addr v);                          /* 0046a3e0 RavenVehicleTrophyTruck_Construct (ESI = v) */
void    rx_vehicle_utv_vf28(rx_addr v, rx_addr blk);                    /* 004c0700 (thiscall; RET 4) */
void    rx_vehicle_buggy_vf28(rx_addr v, rx_addr blk);                  /* 004a2960 (thiscall; RET 4) */
void    rx_vehicle_truck_vf28(rx_addr v, rx_addr blk);                  /* 0046a850 (thiscall; RET 4) */
void    rx_vehicle_utv_vf49(rx_addr v, uint32_t value);                 /* 004c7020 (thiscall; RET 4) */
void    rx_vehicle_buggy_vf49(rx_addr v, uint32_t value);               /* 004ac6c0 (thiscall; RET 4) */
void    rx_vehicle_truck_vf49(rx_addr v, uint32_t value);               /* 004735f0 (thiscall; RET 4) */
void    rx_vehicle_utv_create_proxy(rx_addr blk, rx_addr v);            /* 004c0c00 (ECX = the creation block, s0 = v; RET 4) */
void    rx_vehicle_buggy_create_proxy(rx_addr blk, rx_addr v);          /* 004a2f00 (the same) */
void    rx_vehicle_truck_create_proxy(rx_addr blk, rx_addr v);          /* 0046aef0 (the same) */
rx_addr rx_sim_utv_construct(rx_addr S);                                /* 004cba90 RavenSimulationUTV_Construct (stdcall; RET 4) */
rx_addr rx_sim_buggy_construct(rx_addr S);                              /* 004b1260 RavenSimulationBuggy_Construct (stdcall; RET 4) */
rx_addr rx_sim_truck_construct(rx_addr S, uint32_t engine_class);       /* 0044fe40 RavenSimulationTrophyTruck_Construct (RET 8) */
void    rx_sim_utv_params_load(rx_addr P);                              /* 004cb360 (stdcall P; RET 4): "utv_vehicle" */
void    rx_sim_buggy_params_load(rx_addr P);                            /* 004b0b30 (stdcall P; RET 4): "buggy_vehicle" */
void    rx_sim_truck_params_load(rx_addr P, uint32_t engine_class);     /* 0044f690 (EDI = P, s0; RET 4): "truck2WD[lite]_vehicle" */
void    rx_sim_utv_setup_masses(rx_addr S);                             /* 004cc030 (EDX = S) */
void    rx_sim_buggy_setup_masses(rx_addr S);                           /* 004b1800 (EDX = S) */
void    rx_sim_truck_setup_masses(rx_addr S);                           /* 00450480 (EDX = S) */

/* ---- the specs (game/build_vehicle_spec.c): see that file for the list / model / part / spec layouts ---- */
rx_addr rx_vehicle_list_construct(rx_addr list);                               /* 005404f0 (ESI = MXGame + 0x154b0) */
rx_addr rx_vehicle_list_create_spec(rx_addr list, uint32_t vehicle_id, uint32_t hr);   /* 00542b20 (s0, s1, s2; RET 0xc) -> spec */
void    rx_spec_request_packages(rx_addr loader, rx_addr spec);                /* 005401b0 (s0, s1; RET 8) */
void    rx_names_to_frame(uint32_t factory, uint32_t slot);                    /* 005400c0 (cdecl): the factory / slot names copied to a frame buffer (debug) */
void    rx_part_request_packages(rx_addr part, rx_addr loader, uint32_t hr);   /* 005403b0 (ECX = part; s0, s1; RET 8) */
rx_addr rx_tbl_ref_value(rx_addr ref, rx_addr col_name);                       /* 004ec210 (EAX = row reference; s0; RET 4): the
                                                                                  row a foreign-key column names */

/* ---- the package cache (game/build_vehicle_pkgcache.c; the parts loader [0x00d92ef0] + 4, the gear loader [0x00d92ef4] + 8) ----
 * A std::map<std::string, entry> (+0 proxy, +0x18 head, +0x1c size; node 0x58: +0xc the package name std::string (the key, _stricmp),
 * +0x28 the database name std::string, +0x44 references, +0x48 GetTickCount of the last use, +0x4c requested, +0x50 marked), +0x20
 * the requester value passed to Database vf16, +0x24 "something to unload", +0x28 the unload delay (500 ms, float), +0x2c / +0x30
 * the purge period (0.25 s) / accumulator, +0x34 the unmarked entries. GetTickCount = loop_get_tick_count. Strings are msvcp90
 * std::string (0x1c: +4 inline buffer or pointer, +0x14 size, +0x18 capacity: inline below 0x10). */
rx_addr rx_pkgcache_construct(rx_addr cache);                                  /* 005436d0 (ESI = cache) */
void    rx_pkgcache_request_db(rx_addr dbname, int async);                     /* 00543750 (ESI = name, s0 = async; RET 4) */
void    rx_pkgcache_add(rx_addr cache, rx_addr pkgname, rx_addr dbname);       /* 00543830 (EDI = cache, s0, s1; RET 8) */
void    rx_pkgcache_request(rx_addr cache, rx_addr pkgname, int async);        /* 00543b30 (EAX = cache, s0, s1; RET 8) */
void    rx_pkgcache_release(rx_addr cache, rx_addr pkgname);                   /* 00543c30 (ECX = cache, EAX = name) */
void    rx_pkgcache_unmark(rx_addr cache, rx_addr pkgname);                    /* 00543990 (ECX = cache, EAX = name) */
uint32_t rx_pkgcache_loaded(rx_addr cache, rx_addr pkgname);                   /* 00543cd0 (EAX = cache, s0; RET 4) */
rx_addr rx_pkgcache_find(rx_addr out_iter, rx_addr cache, rx_addr key);        /* 00544220 (ESI = cache, s0 = out, s1 = &std::string) */
rx_addr rx_pkgcache_lower_bound(rx_addr cache, rx_addr key);                   /* 005449d0 (EAX = cache, s0 = &std::string) -> node */
rx_addr rx_pkgcache_index(rx_addr cache, rx_addr key);                         /* 005440f0 (s0 = cache, s1 = &std::string): map[] -> entry */
rx_addr rx_pkgcache_erase(rx_addr cache, rx_addr out, rx_addr node);           /* 0041b840 erase (s0, s1 out, s2 / s3 iterator; RET 0x10) */
rx_addr rx_vehloader_find_package(rx_addr cache, rx_addr name);                /* 00543db0 (EAX = cache, EDI = C string) -> package */
void    rx_pkgcache_update(rx_addr cache, float dt);                           /* 00543e50 PackageCache_Update (EAX = cache, s0; RET 4) */
rx_addr rx_online_content_find(rx_addr content, rx_addr dbname);               /* XenonOnlineContent::vf3 006d3c60 (thiscall; RET 4) */
rx_addr rx_parts_loader_construct(rx_addr loader);                             /* 00540000 FactoryPartsLoader_Construct (EDI) */
void    rx_vehicle_list_init_loader(rx_addr list);                             /* 00540610 (s0 = the vehicle list; RET 4) */
rx_addr rx_uint_map_index(rx_addr map, rx_addr keyp);                          /* 00773290 std::map<uint, uint>::operator[] (s0, s1 = &key;
                                                                                  RET 8) -> &value (build_vehicle_parts.c) */

/* ---- generic Rdb table readers (game/build_vehicle_tables.c) [planned] ----
 * Column storage. A table / record store T (Rdb::Table, vtable 0x93eb6c, or the store a row reference names): +0x10 / +0x14 the column
 * vector (column objects: +0x40 the value vector, +0xc begin: ints / floats 4 bytes, strings std::string 0x1c, one per row), +0x1c the
 * column map std::map<const char*, index> (_stricmp; head +0x34; node +0xc name, +0x10 index), +0x3c the table's name (inline).
 * A column object: +0 name (0x20), +0x20 its table, +0x2c the type (1 int, 4 std::string, ..), +0x38 the default (a std::string
 * object for strings), +0x40 the value vector object (+0xc begin), +0x48 the index map of an indexed column (std::map<int, row
 * reference>: node +0xc key, +0x10 the reference, +0x1d isnil). A row reference: {+0 the row index, +8 the store T}. Database (static 0x00ae0910,
 * controllers 0x00ae10f0 + 0x1f4 * k): +0x18 the table map name -> table (head +0x30). The null row reference: 0x009c9ff4.
 * Names are game addresses (.rdata literals or game strings). */
uint32_t rx_tbl_column_index(rx_addr table, rx_addr name);     /* 00402160 Table_ColumnIndex (EAX = table, s0; RET 4): -1 absent */
rx_addr  rx_tbl_find(rx_addr database, rx_addr name);          /* 00402220 Database_FindTable (EAX = database, s0; RET 4): 0 absent */
rx_addr  rx_tbl_row(rx_addr column, int32_t key);              /* 00418ec0 (EAX = &key, s0 = column; RET 4): the column's index map
                                                                  (+0x48: std::map<int, row reference>) -> &row reference (null: 0x9c9ff4) */
rx_addr  rx_tbl_rows(rx_addr column, int32_t key);             /* 00418e50 (EAX = &key, s0 = column; RET 4): the non-unique index
                                                                  (+0x44: std::map<int, std::set<row>>) -> &std::set (+0x18 head, +0x1c size;
                                                                  node +0xc the row) or the empty set 0x00ae1c1c */
rx_addr  rx_tbl_cell_word(rx_addr store, uint32_t col, uint32_t row);   /* 0041aaf0 (EAX = store, EDI = col, s0 = row; RET 4) -> &value */
rx_addr  rx_tbl_cell_string(rx_addr column, uint32_t row);     /* 0041a480 (s0 = column, EDI = row; RET 4) -> &std::string */
rx_addr  rx_tbl_ref_string(rx_addr ref, rx_addr col_name);     /* 00419b20 (EAX = row ref, s0; RET 4) -> &std::string */

#ifdef __cplusplus
}
#endif
