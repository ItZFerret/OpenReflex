/* build_rider.h -- construction unit 7 "rider" (core/notes/construction.md section 13): the riders of an event, from the rider
 * entries of the game's rider manager (MXGame + 0x15504: the "Rider List" rows and their gear) through FUN_0053e920 (a race rider:
 * RiderAnimation 0x5c4f0) and FUN_0053eab0 (an intro / podium rider: RiderCharacter 0xd30) to the objects the race frames read
 * (anim_ / world_rider ports): the AnimationEntity / Character base, the animation tree from the BXML "<vehicle>_RavenRiderAnimation
 * Tree" and its animators, the IK / limb data, the rider bodies in the physics world, the gear models and their colour materials.
 *
 * Calling conventions of the original are in each prototype's comment (EAX / ECX / ESI / EDI = register arguments, sN = N-th stack
 * argument, RET n = callee cleanup). Registered functions (RX_FUNCTION) are mostly the standard-convention ones (thiscall / stdcall
 * / cdecl: (this, stack arguments)), so virtual calls land on them; a few register-convention helpers shared with unit 6 are
 * registered too (the gear helpers), and a caller uses their C prototype here.
 *
 * ---- RiderAnimation RA (0x5c4f0 bytes, vtables 0x00958724 / +0x90 0x009587a4 / +0x2b0 0x009587e4) ----
 *   AnimationEntity base (0x2b0): ModelEntity (scene unit 00401db0), +0x144 the Character (0x2a0, 0082df30), +0x148 the animator name
 *   table, +0x170 an embedded ModelAsset (the hierarchy model's copy), +0x2b4 the hierarchy handle.
 *   +0x2d0.. listener floats, +0x5d0 the IK data (0xa8d0, 00424410; +0xa80c the 128-aligned pose block), +0xaea0 the rider body shape
 *   (CollisionShapeCircle r 1), +0xaee4.. bone indices (head, neck, chest, root, feet), +0xaf1c the vehicle, +0xaf7c / +0xaf9c /
 *   +0xafbc / +0xafdc std::maps, +0xc940 the animation event slots (006b7040), +0x57e00.. five transform arrays (50 x 0x30),
 *   +0x5b210 the impact record, +0x5b51c the rider id, +0x5b54c the WreckChecker, +0x5b590 the look controller, +0x5b6e4 the lean
 *   controller, +0x5b760 / +0x5b850 the limb states, +0x5bb6c a std::map, +0x5b6dc the lens-swipe effect (0x110).
 * ---- RiderCharacter (0xd30): CharacterEntity (00839a40) + the rider's mud / plate fields ---- */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- constructors (build_rider_ctor.c) ---- */
rx_addr  rx_rider_animation_construct(rx_addr ra);              /* 00618180 (stdcall ra; RET 4) */
rx_addr  rx_rider_animation_entity_construct(rx_addr e);        /* 0082d040 (stdcall e; RET 4) */
rx_addr  rx_rider_character_construct(rx_addr c);               /* 0082df30 (EDI = c) */
rx_addr  rx_rider_model_asset_construct(rx_addr m);             /* 007d3f30 (ESI = m) */
rx_addr  rx_rider_strtab_init(rx_addr t);                       /* 00843480 (ESI = t) */
void     rx_rider_vector_ctor(rx_addr a, uint32_t size, int32_t count, void (*ctor)(rx_addr));   /* 004022d0 (stdcall; RET 0x10) */
rx_addr  rx_rider_ik_data_init(rx_addr d);                      /* 00424410 (EDI = d) */
void     rx_rider_ik_chains_init(rx_addr s);                    /* 0041f9f0 (ESI = s) */
uint32_t rx_rider_pose_block_alloc(int32_t n, rx_addr out);     /* 0041da90 (EAX = n, s0 = out) -> the rounded count */
void     rx_rider_joint_limits(rx_addr joint);                  /* 0041dca0 (EAX = joint) */
void     rx_rider_joint_init(rx_addr joint);                    /* 0041dbb0 (ECX) */
void     rx_rider_ik_rec70_init(rx_addr e);                     /* 0041db00 (ECX) */
void     rx_rider_ik_rec_q_init(rx_addr e);                     /* 0041db50 (ECX) */
void     rx_rider_rec40_init(rx_addr e);                        /* 00859ae0 (ECX) */
void     rx_rider_xform30_init(rx_addr e);                      /* 00430430 (ECX) */
rx_addr  rx_rider_map30_head_alloc(void);                       /* 00690910 */
rx_addr  rx_rider_map30_init(rx_addr m);                        /* 0063b790 (ESI = m) */
rx_addr  rx_rider_map18_init(rx_addr m);                        /* 00430060 (ESI = m) */
rx_addr  rx_rider_shape_circle_construct(rx_addr s, float radius);   /* 00845440 (ECX = s, s0 radius; RET 4) */
void     rx_rider_effect_slot_init(rx_addr e);                  /* 00694110 (EAX = e) */
rx_addr  rx_rider_lens_effect_construct(rx_addr e, uint32_t p); /* 0063bff0 (ESI = e, s0; RET 4) */
void     rx_rider_impact_init(rx_addr r);                       /* 00617e80 (ECX = r) */
rx_addr  rx_rider_look_controller_init(rx_addr c);              /* 00624c80 (stdcall c; RET 4) */
void     rx_rider_lean_controller_init(rx_addr c);              /* 00629840 (EAX = c) */
void     rx_rider_limb_init(rx_addr L);                         /* 0062a1d0 (EAX = L) */
rx_addr  rx_rider_event_slots_init(rx_addr s);                  /* 006b7040 (ESI = s) */
rx_addr  rx_character_entity_construct(rx_addr e);            /* 00839a40 (stdcall e; RET 4): CharacterEntity (scene unit calls it) */
rx_addr  rx_character_anim_construct(rx_addr c);              /* 00834cb0 (EDI = c) */
uint32_t rx_std_allocator_alloc(rx_addr self, uint32_t size);   /* 004eef20 StdMemoryAllocator::vf1 */
uint32_t rx_std_allocator_free(rx_addr self, rx_addr p);        /* 00775540 StdMemoryAllocator::vf2 */

/* ---- the animation tree (build_rider_tree.c) ---- */
rx_addr  rx_anim_blend_construct(rx_addr a, rx_addr name, float weight, rx_addr c0, rx_addr c1);   /* 00832f50 (ESI, EAX; s0..s2; RET 0xc) */
rx_addr  rx_anim_additive_construct(rx_addr a, rx_addr name, rx_addr src, rx_addr add);           /* 008345c0 (ESI, EAX; RET 8) */
rx_addr  rx_anim_motion_construct(rx_addr a, rx_addr name, rx_addr binding, float time_scale, uint32_t bone);   /* 0083b330 (ESI, EAX, EDI; RET 8) */
rx_addr  rx_anim_switch_construct(rx_addr a, rx_addr name, rx_addr branches);  /* 0083c590 (EAX = name; s0 a, s1 branches; RET 8) */
rx_addr  rx_anim_performance_construct(rx_addr a, rx_addr name, rx_addr h);    /* 0083cf90 (ECX = a, EAX = name; s0; RET 4) */
rx_addr  rx_anim_featherstone_alloc(void);                                     /* 006afa90 */
rx_addr  rx_anim_featherstone_construct(rx_addr a, rx_addr name, rx_addr child);   /* 006afa10 (ESI, EAX; s0; RET 4) */
rx_addr  rx_anim_ik_construct(rx_addr a, rx_addr name, rx_addr child, rx_addr physics);   /* 006afc30 (ESI, EAX; s0, s1; RET 8) */
rx_addr  rx_anim_expression_construct(rx_addr a, rx_addr name, rx_addr child); /* 006ac980 (ESI, EAX; s0; RET 4) */
rx_addr  rx_anim_procedural_construct(rx_addr a, rx_addr name, rx_addr child); /* 006b57c0 (ESI, EAX; s0; RET 4) */
rx_addr  rx_rider_hash_map_construct(rx_addr m, uint8_t traits);               /* 00842430 (EAX = &traits, s0 m; RET 4) */
void     rx_rider_vec8_insert_n(rx_addr v, uint32_t n, rx_addr value, rx_addr where);   /* 00843130 (ECX = n, EDX = v; s0 &value, s1 where, s2 ...) */
rx_addr  rx_anim_bind_tracks(rx_addr e, rx_addr out, rx_addr h);               /* 0082de00 (stdcall entity, out, handle; RET 0xc) */
rx_addr  rx_anim_create_node(rx_addr e, rx_addr r, rx_addr node);             /* 0082db70 (ESI = reader, EDI = entity, s0 node; RET 4) */
rx_addr  rx_animation_entity_vf28(rx_addr e, rx_addr name, rx_addr r, rx_addr node);   /* 0082dbd0 AnimationEntity::vf28 */
rx_addr  rx_rider_animation_vf28(rx_addr e, rx_addr name, rx_addr r, rx_addr node);    /* 006b5e50 RiderAnimation::vf28 */
void     rx_character_set_root(rx_addr ch, uint32_t bones, rx_addr root);      /* 0082e3a0 (ESI = ch, EAX = bones, ECX = root) */
void     rx_character_bind_hierarchy(rx_addr ch, rx_addr ns);                  /* 0082e560 (ECX = ch, EAX = node set) */
void     rx_animation_entity_load_tree(rx_addr e);                             /* 0082dd00 (ECX = entity) */

/* ---- the model setup (build_rider_model.c) ---- */
void     rx_nodeset_init(rx_addr d, rx_addr src, uint32_t flag);               /* 007ddb40 (ECX = d, EDX = src; s0 flag; RET 4) */
void     rx_model_lods_assign(rx_addr dv, rx_addr sv);                         /* 007f92d0 (EAX = dv, s0 sv; RET 4) */
void     rx_model_asset_copy(rx_addr dst, rx_addr src);                        /* 007d4d60 (ESI = dst, s0 src; RET 4) */
void     rx_entity_attach_model(rx_addr scene, uint32_t p, rx_addr e, rx_addr handle, uint32_t flags);   /* 0041c700 (EAX, ECX; s0..s2; RET 0xc) */
void     rx_rider_entity_set_model(rx_addr hier, rx_addr e, rx_addr scene, rx_addr tree);   /* 006b5de0 (EDI = hier; stdcall e, scene, tree; RET 0xc) */

/* ---- the setup (build_rider_init.c) ---- */
void     rx_rider_animation_init(rx_addr hier, rx_addr ra, rx_addr scene, rx_addr tree, rx_addr veh);   /* 00619490 (EAX = hier; stdcall ra, scene, tree, vehicle; RET 0x10) */
int32_t  rx_rider_bone_index(rx_addr ch, rx_addr name);                       /* 00830160 (ESI = character, s0 name; RET 4) */
void     rx_rider_mirror_table_init(rx_addr ch);                               /* 0083af10 (EAX = character) */
void     rx_rider_strset_erase(rx_addr set, rx_addr n);                        /* 00851f10 (ECX = set, s0 node; RET 4): empty sets only */
void     rx_rider_wreck_checker_bind(rx_addr wc, rx_addr veh, rx_addr ra);     /* 00637360 (EAX = checker, ECX = vehicle, s0 ra; RET 4) */
void     rx_rider_bind_anim_tracks(rx_addr ns, rx_addr handle, uint32_t p, rx_addr rec);   /* 008403b0 (EAX = ns; s0..s2; RET 0xc) */
void     rx_rider_event_slot_bind(rx_addr slot, rx_addr h, rx_addr ns);        /* 006b6c80 (EDI = slot; s0 h, s1 ns; RET 8) */
void     rx_rider_vehicle_events_init(rx_addr ra, rx_addr s, rx_addr veh);     /* 006b7110 (ECX = ra; s0 slots, s1 vehicle; RET 8) */
void     rx_rider_lean_anims_init(rx_addr veh, rx_addr ra);                    /* 0061c6e0 (EAX = vehicle, EDI = ra) */
void     rx_rider_truck_setup(rx_addr veh, rx_addr ra);                        /* 0061aa10 (EAX = vehicle, ECX = ra): machine-tree plugs */

/* ---- the plugs (build_rider_plugs.c) ---- */
rx_addr  rx_rider_strmap_index(rx_addr map, rx_addr key, uint32_t vsize);     /* 0063b6e0 (vsize 2) / 00850cd0 (vsize 4) (EDI = map, s0 &key; RET 4) */
void     rx_rider_bind_switch(rx_addr sw, rx_addr ra);                         /* 0061afc0 (ECX = switch, s0 ra; RET 4) */
rx_addr  rx_vehicle_ik_target(rx_addr veh, rx_addr name);                     /* 00460450 (ESI = name, EDI = vehicle) */
void     rx_ik_solver_load(rx_addr s, rx_addr node, rx_addr r, uint32_t def_iter, rx_addr hier, rx_addr target);   /* 006afdc0 (ECX, EDX; s0..s3; RET 0x10) */
void     rx_ik_animator_load(rx_addr ik, rx_addr r);                           /* 006b01b0 (stdcall ik, reader; RET 8) */
void     rx_ik_animator_attach(rx_addr ik, rx_addr veh, rx_addr ch);           /* 006afd50 (ESI = ik, EAX = vehicle, ECX = character) */
void     rx_rider_machine_bind_switch(rx_addr ra, rx_addr sw);               /* 0061ab30 cdecl rider,switch RET8 */
uint32_t rx_rider_machine_walker_enter(rx_addr walker, rx_addr animator);    /* 0061aaa0 ECX walker,stack animator RET4; result unused */
void     rx_rider_attach_plugs(rx_addr veh, rx_addr ra);                       /* 0061ad70 (ECX = ra, EAX = vehicle) */

/* ---- the ragdoll / bodies (build_rider_ik.c) ---- */
void     rx_ragdoll_add_body(rx_addr ch, rx_addr body_t, rx_addr state_t, int32_t node, int32_t parent);   /* 0041fad0 (ESI, ECX, EDX; s0, s1; RET 8) */
void     rx_ragdoll_add_pin(rx_addr ch, int32_t parent, int32_t node, rx_addr pin);   /* 0041fc20 (ESI = chains, EAX = parent; s0 node, s1 &pin; RET 8) */
void     rx_ragdoll_finish(rx_addr ch);                                        /* 00420090 (ESI = chains) */
void     rx_character_set_ragdoll_bones(rx_addr c, uint32_t n, rx_addr bones); /* 0082e160 (ESI = character, EAX = n; s0; RET 4) */
rx_addr  rx_rider_body_create(rx_addr owner, rx_addr pw, rx_addr out, float radius);   /* 00772760 (ECX = owner; stdcall world, out, radius; RET 0xc) */
void     rx_rider_bodies_create(rx_addr d, rx_addr ra);                        /* 00429700 (stdcall data, ra; RET 8) */
void     rx_rider_ik_setup(rx_addr d, rx_addr chr, rx_addr ra);                /* 004246c0 (stdcall data, character, ra; RET 0xc) */

/* ---- the gear (build_rider_gear.c) ----
 * A rider entry L: +0x14 the high-resolution flag, +0x1c + 4k the 8 current parts, +0x3c + 4k the 8 pending ones. A part: +0 std::string
 * its material package, +0x1c std::string its model package (size +0x2c), +0x38 high-resolution ("HR_<name>"), +0x40 gear slot,
 * +0x48 references. A model's LOD vector (model + 0x124, std::vector<LodRecord>: 0x1c records {+0 threshold, +4 std::vector of
 * 16-byte entries {part, node index, model handle, material}}). */
void     rx_vehloader_assemble(rx_addr model, rx_addr L, rx_addr mgr);        /* 0053ddc0 VehicleLoader_Assemble (ECX = model, EDX = L; s0 manager; RET 4) */
uint32_t rx_vehloader_override_materials(rx_addr L, uint32_t a, uint32_t b);  /* 0053d8b0 (stdcall L, a, b; RET 0xc) */
void     rx_model_lods_clear(rx_addr v);                                       /* 005440b0 (EAX = vector): the records destroyed */
/* the gear helpers (shared with unit 6's vehicle parts assembly 00541310) */
void     rx_rider_name_number(rx_addr name_buf, rx_addr number_buf, rx_addr found, uint32_t rider_id);   /* 0053f3f0 (stdcall; RET 0x10; build_rider_entry.c; not registered): the rider's plate name / number text */
void     rx_gear_hr_name(rx_addr buf, rx_addr name);                           /* 0078fd60 (EDI = buf, ESI = 0x40; "HR_%s", name): buf <- "HR_<name>" (64 bytes, terminated) */
uint32_t rx_asset_handle_is(rx_addr h, uint32_t lo, uint32_t hi);              /* the handle's vf1 (type tag EDX:EAX) == (lo, hi) */
void     rx_iarr_init(rx_addr a, uint32_t cap);                                /* inline: an Rdb int array {data, count 0, cap, growth = cap, max 0x7fffffff} */
void     rx_iarr_reserve(rx_addr a, uint32_t cap);                             /* 007cff20 (ESI = array, s0 capacity; RET 4) */
void     rx_iarr_push(rx_addr a, uint32_t v);                                  /* inline push (grow by +0xc) */
void     rx_iarr_insert_sorted(rx_addr a, int32_t v);                          /* 00544e70 (ESI = array; cdecl value): ascending, unique */
void     rx_iarr_indices_in_range(rx_addr out, rx_addr in, int32_t lo, int32_t hi);   /* 00544f70 (EAX = out; cdecl in, lo, hi) */
void     rx_lod_entries_push_back(rx_addr v, rx_addr val);                     /* 00416f10 (EAX = entries vector, s0 &entry (16 bytes); RET 4) */
void     rx_model_push_empty_lod(rx_addr m);                                   /* 007d4ce0 (EDI = model) */
void     rx_lod_set_init(rx_addr obj);                                         /* 00543410 (EDI = threshold set) */
void     rx_lod_set_add_package(rx_addr obj, rx_addr pkg);                     /* 005434c0 (stdcall set, package; RET 8) */
void     rx_lod_set_apply(rx_addr obj, rx_addr m);                             /* 005435a0 (stdcall set, model; RET 8) */
void     rx_material_key(rx_addr out, uint32_t size, rx_addr name);           /* 00543fc0 MaterialKey (EAX = size, ESI = out; cdecl name) */

/* ---- the spawn (build_rider_spawn.c) ---- */
rx_addr  rx_rider_spawn(uint32_t id, rx_addr mgr, rx_addr world, rx_addr veh, rx_addr cb);   /* 0053e920 (EAX = id; stdcall manager, world, vehicle, callback; RET 0x10) -> RA */
rx_addr  rx_rider_spawn_character(rx_addr mgr, rx_addr world, uint32_t id);   /* 0053eab0 (stdcall manager, world, id; RET 0xc) -> RiderCharacter */
rx_addr  rx_rider_tree_handle(rx_addr veh);                                    /* 0053cd40 (ECX = vehicle; cdecl world (unused)) -> the tree's BXML handle */
void     rx_rider_type_ranges(uint32_t type, rx_addr a, rx_addr b);           /* 0053c7d0 (EAX = a, ECX = b; cdecl type) */
void     rx_rider_map18_erase(rx_addr map, rx_addr n);                         /* 0068f300 (ECX = map, s0 node; RET 4) */
void     rx_rider_assemble_gear(rx_addr ra, rx_addr mgr, rx_addr entry);      /* 0053dd20 (EDI = ra; stdcall manager, entry; RET 8) */
void     rx_rider_character_assemble_gear(rx_addr rc, rx_addr mgr, rx_addr entry);   /* 0053dd70 (EDI = rc; stdcall manager, entry; RET 8) */
rx_addr  rx_ridercharacter_construct(rx_addr c);                               /* 00624750 RiderCharacter (ESI = c) */
void     rx_rider_character_bind(rx_addr rc, rx_addr scene, rx_addr hier);    /* 00624990 RiderCharacter_BindMud (EAX = rc; stdcall scene, hier; RET 8) */

/* ---- the entries and their packages (build_rider_entry.c) ---- */
rx_addr  rx_gear_loader_construct(rx_addr gl);                                 /* 0053cbd0 FactoryGearLoader (EDI = gl) */
uint32_t rx_gear_slot_of(rx_addr name);                                        /* 004dcf30 (cdecl factory name) -> slot (8: none) */
uint32_t rx_gear_type_slot(uint32_t type);                                     /* 004e74c0 (s0 RiderGearTypeId; RET 4) -> slot */
void     rx_rider_entry_create(rx_addr mgr, uint32_t id, uint32_t hr);        /* 0053e8c0 (stdcall manager, rider id, high resolution; RET 0xc) */
uint32_t rx_rider_entry_set_pending(rx_addr out, rx_addr e, rx_addr mgr, rx_addr p);   /* 0053efa0 (ECX = &out, ESI = entry; stdcall manager, part; RET 8) */
uint32_t rx_rider_part_packages_ready(rx_addr p, uint32_t hr);                 /* 0053ca70 (ECX = part; s0; RET 4) */
void     rx_rider_part_release_packages(rx_addr p, uint32_t hr);               /* 0053d170 (ECX = part; stdcall loader, hr; RET 8) */
void     rx_rider_part_release(rx_addr mgr, rx_addr p);                        /* 0053d480 (ECX = manager; s0 part; RET 4) */
void     rx_rider_part_destroy(rx_addr p);                                     /* 0053ca30 (EAX = part) */
void     rx_rider_ptr_array_remove(rx_addr a, rx_addr value);                  /* 00544c80 (ECX = array, EDX = &value) */

/* ---- the clip recorder's setup (build_rider_clip.c) ---- */
void     rx_clip_alloc_buffer(void);                                           /* 006abc40 ClipRecorder_AllocBuffer: [0x00a7f124] 0x5b68 x 0x50 */
void     rx_clip_init(void);                                                   /* 006aa660 ClipRecorder_Init */

#ifdef __cplusplus
}
#endif
