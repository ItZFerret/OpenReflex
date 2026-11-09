/* fx.h -- the effects subsystem of the OPEN REFLEX core (group rx_fx, core/plan/rx_fx.tsv): particles, mud, the terrain
 * deformation sync and the tyre tracks, the placed world entities of the 0x40xxxx / 0x6cxxxx ranges. One rx_fx_<name> per
 * original function (address in the comment). Signatures: `this` first for thiscall, then the stack arguments in order, LTCG
 * register arguments after them as the comment names; the result as the original returns it. notes/fx.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- fx_particle.c: the particle managers ---- */
void     rx_fx_particle_copy(rx_addr src, rx_addr dst);                          /* 00692b50 Particle_Copy (ECX = src, EAX = dst) */
void     rx_fx_particle_update_job(rx_addr job);                                 /* 006925f0 Particle_UpdateJob (cdecl) */
void     rx_fx_particle_manager_update(rx_addr mgr, float dt);                   /* 00698510 ParticleManager2D_Update (RET 4) */
void     rx_fx_particle_spawn(rx_addr mgr, rx_addr emitter, rx_addr tmpl);       /* 00698780 SpawnParticle (vf1; RET 8) */
uint32_t rx_fx_particle_manager_vf7(rx_addr mgr);                                /* 00691ba0 */
rx_addr  rx_fx_particle_manager_vf5(rx_addr self, rx_addr out);                  /* 00698740 (RET 4) -> out */
uint32_t rx_fx_particle_manager_vf4(rx_addr self, uint32_t a, uint32_t b, rx_addr out);   /* 00698760 (RET 0xc) */
rx_addr  rx_fx_emitter_vf4(rx_addr self);                                        /* 00691d80 */
void     rx_fx_emitter_vf5(rx_addr self, uint32_t on);                           /* 00691d90 (RET 4) */
void     rx_fx_particle_entity_update_managers(rx_addr ent, float dt);           /* 006960c0 ParticleEntity vf21 (RET 4) */
void     rx_fx_particle_entity_tick(rx_addr ent, float dt);                      /* 00696100 ParticleEntity vf19 (RET 4) */

/* ---- fx_emit.c: emission ---- */
void     rx_fx_matrix_lerp(rx_addr a, float t, rx_addr out, rx_addr b);           /* 006941a0 (ESI = out, EDI = b; RET 8) */
uint32_t rx_fx_spawn_build(rx_addr mgr, rx_addr S, uint32_t index, rx_addr P, rx_addr M);   /* 006944c0 (ECX, EAX = M; RET 0xc) */
uint32_t rx_fx_spawn_build2(rx_addr mgr, rx_addr S, rx_addr M, uint32_t index, rx_addr P);  /* 00694ba0 (ECX; RET 0x10) */
void     rx_yring_spawn(rx_addr S, rx_addr mgr, rx_addr mat, uint32_t index);     /* 0069a170 (RET 0x10) */
float    rx_fx_vec_angle(rx_addr a, rx_addr b);                                  /* 00672fe0 (ESI = a, EDI = b; ECX = the model's; xmm0) */
uint32_t rx_fx_emitter_in_range(rx_addr S, rx_addr pos);                         /* 00695b00 (RET 8) */
uint32_t rx_fx_emitter_gate(rx_addr S, rx_addr mgr);                             /* 00695650 (EAX = S, EDI = mgr) */
void     rx_fx_emitter_emit(rx_addr rec);                                        /* 00695780 (cdecl) */
void     rx_fx_emitter_vf0(rx_addr S, float dt, rx_addr mgr, rx_addr mat, uint32_t interp, uint32_t index);  /* 00695ab0 (RET 0x14) */
void     rx_fx_attachable_emitter_vf3(rx_addr self, float dt);                   /* 0063c040 AttachableParticleEffect vf3 (RET 4) */
void     rx_fx_vehicle_emitter_vf3(rx_addr self, float dt);                      /* 0069b060 (RET 4) */

/* ---- fx_tree.c: the trick maps' std::map<string, int> tree code ---- */
uint32_t rx_fx_map_size(rx_addr map);                                            /* 00608390 (ECX) */
void     rx_fx_tree_next(rx_addr it);                                            /* 00593dc0 TreeIterator_Increment (ESI = iterator) */
void     rx_fx_tree_prev(rx_addr it);                                            /* 0063bf50 (ESI = iterator) */
uint32_t rx_fx_tree_iter_eq(rx_addr a, rx_addr b);                               /* 004fd0c0 (EDI = a, ESI = b) -> AL */
void     rx_fx_tree_lrotate(rx_addr w, rx_addr map);                             /* 00851f50 (ECX = w; RET 4) */
void     rx_fx_tree_rrotate(rx_addr w, rx_addr map);                             /* 005bb590 (ECX = w; RET 4) */
rx_addr  rx_fx_tree_node_new(rx_addr left, rx_addr parent, rx_addr right, rx_addr value, uint32_t colour);   /* 005bac00 (RET 0x14) */
rx_addr  rx_fx_tree_insert_at(rx_addr map, rx_addr where, rx_addr out, uint32_t addleft, rx_addr value);     /* 005ba570 (EDI = map, EAX = where; RET 0xc) */
rx_addr  rx_fx_tree_insert_unique(rx_addr map, rx_addr out, rx_addr value);      /* 005ba210 (RET 0xc) */
rx_addr  rx_fx_map_insert_hint(rx_addr map, rx_addr out, rx_addr value, rx_addr hint_proxy, rx_addr hint_node);   /* 006cd4a0 (RET 0x14) */

/* ---- fx_vector.c: std::vector<T*> element helpers and _Insert_n ---- */
rx_addr  rx_fx_vector_alloc(uint32_t n);                                         /* 0042bf30 (ECX = n) */
rx_addr  rx_fx_vector_umove(rx_addr end, rx_addr begin, rx_addr dst);            /* 00776d20 (EAX = end, EDI = begin, EDX = dst) */
rx_addr  rx_fx_vector_fill(rx_addr value, rx_addr end, rx_addr begin);           /* 007cfd80 (EDX = value*, ECX = end; cdecl begin) */
void     rx_fx_vector_insert_n(rx_addr vec, uint32_t count, rx_addr value_p, rx_addr proxy, rx_addr where);   /* 006c8260 (RET 0x14) */

/* ---- loop_jobpool.c: the job pool constructor ---- */
rx_addr  rx_jobpool_element_ctor(rx_addr e);                                     /* 004dc090 (ECX) */
rx_addr  rx_jobpool_construct(rx_addr pool);                                     /* 00881a70 (ECX = the pool; EAX = pool) */

/* ---- fx_vehicle.c: the vehicle particle effects ---- */
uint32_t rx_fx_vehicle_effect_gate(rx_addr F, rx_addr V);                        /* 00697460 (ECX = F, EAX = V) -> AL */
void     rx_fx_vehicle_effect_rate(rx_addr V, uint32_t body, float base, float range, rx_addr out, rx_addr out2,
                                   uint32_t wheel, rx_addr R);                   /* 00693560 (EAX = wheel, ESI = R; RET 0x18) */
void     rx_fx_vehicle_effect_update(float dt, rx_addr V, rx_addr F);            /* 00696d80 (EAX = V, EDI = F; RET 4) */
void     rx_fx_vehicle_effects_job(rx_addr job);                                 /* 006979a0 (cdecl) */
void     rx_fx_vehicle_effects_update(rx_addr set, float dt);                    /* 00697a00 (ESI = set; RET 4) */
void     rx_fx_vehicle_effects_update_all(float dt);                             /* 00691bd0 (cdecl) */

/* ---- fx_effect.c: the placed effects ---- */
void     rx_fx_sound_emitter_update(rx_addr em);                                 /* 00678980 (EAX = the sound emitter) */
void     rx_fx_placeable_effect_tick(rx_addr e, float dt);                       /* 00696940 PlaceableParticleEffect vf19 (RET 4) */
void     rx_fx_fireworks_effect_tick(rx_addr e, float dt);                       /* 00692340 (RET 4) */
void     rx_fx_triggered_effect_tick(rx_addr e, float dt);                       /* 00692520 (RET 4) */
void     rx_fx_fireworks_start(float delay, rx_addr e);                          /* 006922c0 (ESI = e; RET 4) */
void     rx_fx_sound_start(rx_addr snd);                                          /* 00678bd0 (EAX = the sound event) */
void     rx_fx_placeable_effect_enable(rx_addr e, uint32_t on);                  /* 006968e0 (RET 4) */

/* ---- fx_terrain.c: the terrain GPU sync, tyre tracks, mud ---- */
void     rx_fx_tracks_reset(rx_addr T);                                          /* 007cc630 TerrainTracks_Reset (EAX = T) */
void     rx_fx_tracks_flush(rx_addr T);                                          /* 007cbe30 TerrainTracks_FlushPending (EAX = T) */
void     rx_fx_patch_readback(rx_addr P);                                        /* 007cb930 DeformablePatch_ReadbackToSysmem (ESI = P) */
void     rx_fx_terrain_sync(rx_addr surf);                                       /* 007c4fa0 HFTerrainSurface::vf14 (ECX = surface) */
rx_addr  rx_fx_terrain_registry(void);                                           /* 00402d90 */
void     rx_fx_mud_update(rx_addr rec);                                          /* 00408c50 MudRecord_Update (ESI = record) */

/* ---- fx_deform.c: the deformation pass (the original's GPU pass 007caf70 + ps_hft_deform as CPU code; product) ---- */
void     rx_fx_deform_render(rx_addr P);                                         /* after vf14's sync: queue[!q] into the targets */
float    rx_fx_ps_hft_deform(float D, float brush_r, float brush_g, float soft_r, float soft_g, float soft_b, float z);

/* ---- fx_entity.c: triggers, timed sections, overlay / scrolling / gate camera ticks, leaves ---- */
uint32_t rx_fx_trigger_inside(rx_addr T, rx_addr p);                             /* 00403cf0 (EAX = T, ESI = p) */
uint32_t rx_fx_trigger_contains(rx_addr T, rx_addr P);                           /* 00403c10 (EAX = T, ECX = P) -> AL */
rx_addr  rx_fx_trigger_find(rx_addr obj, rx_addr T);                             /* 0040a7f0 (EDI = T; RET 4) */
void     rx_fx_trigger_add(rx_addr obj, rx_addr T);                              /* 0040a830 (EAX = T; RET 4) */
void     rx_fx_trigger_unsee(rx_addr T);                                         /* 0040a870 (ECX = T) */
void     rx_fx_trigger_post(rx_addr veh, rx_addr T, rx_addr name);               /* 00419820 (ECX = T, EDX = name; RET 4) */
void     rx_fx_trigger_post_physics(rx_addr obj, rx_addr T, rx_addr name);       /* 00419880 (ECX = T, EDX = name; RET 4) */
void     rx_fx_section_post(rx_addr veh, rx_addr T);                             /* 00419a10 (ECX = T; RET 4) */
void     rx_fx_trigger_stay(rx_addr T);                                          /* 0040a600 (ESI = T) */
void     rx_fx_trigger_watch_object(rx_addr obj, rx_addr T);                     /* 0040a900 (EDI = T; RET 4) */
void     rx_fx_trigger_player_inside(rx_addr pl, rx_addr T);                     /* 0040a8a0 (EAX = player, ECX = T) */
void     rx_fx_trigger_prune(rx_addr T);                                         /* 0040a980 (ESI = T) */
void     rx_fx_trigger_tick(rx_addr T, float dt);                                /* 0040a650 MXTrigger vf19 (RET 4) */
void     rx_fx_section_player_inside(rx_addr pl, rx_addr T);                     /* 00413b40 (EAX = player, ECX = T) */
void     rx_fx_timed_section_tick(rx_addr T, float dt);                          /* 00413a80 TimedRaceSection vf19 (RET 4) */
void     rx_fx_overlay_colour_tick(rx_addr e, float dt);                         /* 0040b770 (RET 4) */
void     rx_fx_scrolling_texture_tick(rx_addr e, float dt);                      /* 00410dd0 (RET 4) */
void     rx_fx_gate_camera_tick(rx_addr e, float dt);                            /* 00404020 (RET 4) */
void     rx_fx_copy_dword(rx_addr dst, rx_addr src);                             /* 00401060 (RET 4) */
uint32_t rx_fx_deref2(rx_addr p);                                                /* 00401070 (ECX) */
uint32_t rx_fx_second(uint32_t a, uint32_t b);                                   /* 004010e0 (cdecl) */
uint32_t rx_fx_holder_lock(rx_addr h, rx_addr out);                              /* 00401c80 (EAX, EDX) */
void     rx_fx_holder_unlock(rx_addr h);                                         /* 00401ca0 (EAX) */
float    rx_fx_renderable_vf7(rx_addr self);                                     /* 00401d40 (x87) */
uint32_t rx_fx_model_entity_vf1(rx_addr self);                                   /* 00401f00 */
uint32_t rx_fx_field_10(rx_addr o);                                              /* 004023c0 (EAX) */
uint32_t rx_fx_field_68(rx_addr o);                                              /* 00402af0 (EAX) */
void     rx_fx_set_118(rx_addr o, float v);                                      /* 00402ad0 (ECX, xmm0) */
uint32_t rx_fx_ui_manager(void);                                                 /* 00402c20 */
void     rx_fx_push_dword(rx_addr cur, rx_addr src);                             /* 0041ad70 (EAX, EDX) */
float    rx_fx_atan2f(float y, float x);                                         /* 0041c4b0 (cdecl; x87) */
rx_v4    rx_fx_int4_div(const int32_t in[4], float s, rx_v4* splat);             /* 0041c580 (xmm0 ints; cdecl s) -> xmm0, xmm1 */
rx_v4    rx_fx_dot3_splat(rx_v4 a, rx_v4 b);                                     /* 0041c5c0 (xmm0, xmm1) -> xmm0 */

/* ---- fx_world.c: bales, flag holders, land animals, key maps, online / storage / session ---- */
void     rx_fx_popup_bale_tick(rx_addr b, float dt);                             /* 006c1250 PopUpBale vf19 (RET 4) */
void     rx_fx_popdown_bale_tick(rx_addr b, float dt);                           /* 006c08f0 PopDownBale vf19 (RET 4) */
void     rx_fx_trackside_flag_tick(rx_addr e, float dt);                         /* 006c2eb0 TrackSideFlagHolder vf19 (RET 4) */
void     rx_fx_land_animal_vf29(rx_addr e, float dt, uint32_t a1, uint32_t a2, rx_addr a3);   /* 006d07a0 (RET 0x10) */
void     rx_fx_land_animal_steer(rx_addr e);                                     /* 006d2eb0 (ESI = e) */
void     rx_fx_land_animal_vf20(rx_addr e);                                      /* 006d09f0 LandAnimalProximity::vf20 (fx_animal.c) */
void     rx_fx_keymap_vf9(rx_addr k, uint32_t slot, uint32_t v);                 /* 006d7840 (RET 8) */
uint32_t rx_fx_keymap_vf10(rx_addr k, uint32_t slot);                            /* 006d7860 (RET 4) */
void     rx_fx_event_proxy_call(rx_addr p, rx_addr ev);                          /* 006c5960 (RET 4) */
rx_addr  rx_fx_pc_storage_device_construct(rx_addr dev);                         /* 006d4ac0 (EDI = block) -> EAX = dev */
void     rx_fx_pc_storage_enumerate(rx_addr storage, uint32_t v, rx_addr evs);   /* 006d5600 (ECX = storage, ESI = evs; RET 4) */
void     rx_player_leave_spectate(rx_addr player, uint32_t flag);                  /* 0054de80 (stdcall player, flag; RET 8) */
void     rx_fx_online_content_poll(rx_addr oc);                                  /* 006d3d30 (stdcall; RET 4) */
void     rx_fx_online_content_vf2(rx_addr oc, uint32_t arg);                     /* 006d3c40 (RET 4) */
void     rx_fx_pc_storage_vf7(rx_addr st, uint32_t arg);                         /* 006d4a70 (RET 4) */
rx_addr  rx_fx_session_manager(void);                                            /* 006de1c0 */
void     rx_fx_session_tick(float dt, rx_addr ses);                              /* 006de690 (EDI = session; RET 4) */
void     rx_fx_session_clock(rx_addr root, float t);                             /* 006decd0 (EAX = stats root; RET 4) */

/* ---- fx_stunt.c: the trick tracker (player +0xb58) and the stunt scorer (player +0xc70) ---- */
rx_addr  rx_fx_stunt_kind_name(uint32_t kind);                                   /* 006cd000 (ECX) */
float    rx_fx_stunt_kind_time(uint32_t kind);                                   /* 006ca070 (EAX) -> xmm0 */
rx_addr  rx_fx_trick_find(rx_addr name, uint32_t kind);                          /* 006c8570 (cdecl) */
void     rx_fx_trick_event(rx_addr player, rx_addr trick, rx_addr name);         /* 006cd750 (ECX = trick, EDX = name; RET 4) */
void     rx_fx_stunt_event(rx_addr name, rx_addr player, rx_addr rec, uint32_t kind);  /* 006cd810 (ECX = rec, EDX = kind; RET 8) */
void     rx_fx_stunt_end(rx_addr K);                                             /* 006ca030 (EAX) */
void     rx_fx_stunt_begin(rx_addr K, uint32_t kind);                            /* 006c9fc0 (ESI, EDX) */
uint32_t rx_fx_stunt_wanted(rx_addr K);                                          /* 006c9ed0 (RET 4) */
void     rx_fx_stunt_update(float dt, rx_addr K);                                /* 006ca0c0 (EAX = K; RET 4) */
void     rx_fx_trick_followup(rx_addr K);                                        /* 006c9480 (ESI) */
void     rx_fx_trick_land(rx_addr K);                                            /* 006c9250 (ESI) */
void     rx_fx_trick_jump(float dt, rx_addr K);                                  /* 006c9380 (EAX = K; RET 4) */
void     rx_fx_trick_abort(rx_addr K);                                           /* 006c9190 (ESI) */
uint32_t rx_fx_trick_ground_ok(rx_addr K);                                       /* 006c9940 (EAX) */
void     rx_fx_trick_ground(rx_addr K);                                          /* 006c9b00 (EAX) */
void     rx_fx_trick_input(rx_addr K, float dt);                                 /* 006c9510 (RET 8) */
void     rx_fx_trick_update(float dt, rx_addr K);                                /* 006c9770 (EAX = K; RET 4) */
uint32_t rx_fx_tricks_landed(rx_addr P);                                         /* 006cbad0 (EDI) */
void     rx_fx_stunt_variety(rx_addr P);                                         /* 006cc670 (ESI) */
void     rx_fx_stunt_ratings(rx_addr P);                                         /* 006cc4b0 (EAX) */
uint32_t rx_fx_tricks_unique(rx_addr P);                                         /* 006cb9b0 (EDI) */
void     rx_fx_stunt_rating_e(rx_addr P);                                        /* 006cc830 (EAX) */
void     rx_fx_stunt_rating_a(rx_addr P);                                        /* 006cc740 (EAX) */
void     rx_fx_stunt_refresh(rx_addr P);                                         /* 006cc8b0 (ESI) */
void     rx_fx_highlight_recover(uint32_t mode, uint32_t pad);                   /* 006d83d0 (EAX = mode, ESI = pad; RET 0) */
void     rx_fx_stunt_combo(rx_addr P);                                           /* 006cae70 (EAX) */
void     rx_fx_stunt_wreck(rx_addr P);                                           /* 006cb720 (ESI) */
void     rx_fx_stunt_score_drop(rx_addr mgr, rx_addr player);                    /* 00588b50 (EAX = player; stack manager; RET 4) */
void     rx_fx_stunt_score(float dt, rx_addr P);                                 /* 006cab80 (EAX = P; RET 4) */
void     rx_fx_stunt_combo_finish(rx_addr P);                                    /* 006cc040 (ESI) */
void     rx_fx_stunt_combo_end(rx_addr P);                                       /* 006caaf0 (ECX = the scorer) */
void     rx_fx_stunt_final_score(rx_addr player, float score);                    /* 006cd870 (EAX = player; xmm0) */
void     rx_fx_stunt_landed(rx_addr P, rx_addr player);                          /* 006cc0d0 (ECX; RET 4) */
uint32_t rx_fx_stunt_combo_new(rx_addr P, rx_addr trick);                        /* 006cc1f0 (EAX, EDI) */
void     rx_fx_stunt_vehicle_entered(rx_addr P, rx_addr vehicle, rx_addr trigger);   /* 006cc230 (ECX = the scorer; RET 8) */
void     rx_fx_stunt_best(rx_addr B, rx_addr cand, float v);                     /* 006cb500 (ECX, EDX; RET 4) */
rx_addr  rx_fx_trick_map_get(rx_addr map, rx_addr key);                         /* 006cd3f0 (EDI = map; RET 4) */
uint32_t rx_fx_trick_count(rx_addr P, rx_addr trick);                            /* 006cb960 (ECX, EAX) */
void     rx_fx_stunt_begins(rx_addr P, rx_addr player, rx_addr trick, uint32_t kind);   /* 006cbbe0 (ECX; RET 0xc) */
void     rx_fx_stunt_ends(rx_addr P, rx_addr player, rx_addr trick);             /* 006cbdb0 (ECX; RET 0xc) */
void     rx_fx_stunt_combo_lost(rx_addr P, rx_addr player);                      /* 006cb6a0 (ECX; RET 8) */
void     rx_fx_trick_table_count(rx_addr act);                                   /* 006ccbc0 (EAX = activity) */
void     rx_fx_stunt_chain_note(rx_addr P, rx_addr trick);                       /* 006ccef0 (EAX = scorer, ECX = trick) */

/* ---- the trick start / hold machine (fx_stunt.c; agent "trickstart") ---- */
uint32_t rx_fx_trick_perf_exists(rx_addr trick, uint32_t klass);                 /* 006cd050 (ECX = trick; stack class) */
rx_addr  rx_fx_trick_asset_handle(rx_addr trick, uint32_t klass);                /* 006cd0f0 (ECX = trick; stack class) */
float    rx_fx_trick_hold_time(rx_addr trick, uint32_t klass);                   /* 006cd1c0 (ECX = trick, EAX = class) -> xmm0 */
uint32_t rx_fx_trick_min_reached(rx_addr K);                                     /* 006c9a40 (EAX = tracker) -> EAX */
void     rx_fx_trick_start(rx_addr K, rx_addr trick);                            /* 006c98c0 (ESI = tracker, EDI = trick) */
void     rx_fx_trick_mode1(rx_addr K, float dt);                                 /* 006c9b60 (ESI; RET 4) */
void     rx_fx_trick_mode2(rx_addr K, float dt);                                 /* 006c9cc0 (ESI; RET 4) */
void     rx_fx_trick_mode3(rx_addr K);                                           /* 006c9da0 (ESI; RET 4) */
void     rx_fx_trick_chain(rx_addr K);                                           /* 006c9c20 (EAX = tracker) */
void     rx_fx_trick_hold_begin(rx_addr K);                                      /* 006c9e80 (ESI) */
void     rx_fx_trick_hold_end(rx_addr K);                                        /* 006c9eb0 (ESI) */
void     rx_fx_trick_complete(rx_addr K);                                        /* 006c9df0 (EAX = tracker) */
void     rx_fx_trick_switch_event(rx_addr player, rx_addr oldt, rx_addr newt);   /* 006cd7b0 (ECX = old, EDX = new; RET 4) */
void     rx_fx_stunt_begin_evt(rx_addr P, rx_addr player, rx_addr trick);        /* 006cb1b0 "StuntBegin" (ECX; RET 8) */
void     rx_fx_stunt_completed(rx_addr P, rx_addr player, rx_addr trick);        /* 006cb3c0 "StuntCompleted" (ECX; RET 8) */
void     rx_fx_stunt_combo_from(rx_addr P, rx_addr player, rx_addr trick);       /* 006cb560 "ComboFromStunt" (ECX; RET 0xc) */
void     rx_fx_stunt_landed_evt(rx_addr P, rx_addr player, rx_addr trick);       /* 006cb5a0 "StuntLanded" (ECX; RET 8) */
void     rx_fx_stunt_takeoff_evt(rx_addr P, rx_addr player, rx_addr trick);      /* 006cb610 "StuntTakeoff" (ECX; RET 8) */
void     rx_fx_stunt_kickout(rx_addr P, rx_addr player, rx_addr trick);          /* 006cb660 "StuntKickOut" (ECX; RET 8) */
void     rx_fx_stunt_audio_begin(rx_addr logic, rx_addr player, rx_addr trick);  /* 00581830 ActivityAudioLogic::vf5 (ECX; RET 8) */
void     rx_fx_stunt_audio_complete(rx_addr logic, rx_addr player, rx_addr trick); /* 00581950 ActivityAudioLogic::vf8 (ECX; RET 8) */

/* ---- fx_types.c: static instances, array iterators, small fills ---- */
rx_addr  rx_fx_static_9e30e4(void);                                              /* 00402b10 */
rx_addr  rx_fx_static_9e30e4_construct(void);                                    /* 00402be0 (cdecl) -> 0x009e30e4 */
rx_addr  rx_fx_series_manager(void);                                             /* 00413d20 */
void     rx_fx_vector_ctor_iterator(rx_addr ptr, uint32_t size, int32_t count, rx_addr fn);   /* 004022d0 (RET 0x10) */
void     rx_fx_vector_dtor_iterator(rx_addr ptr, uint32_t size, int32_t count, rx_addr fn);   /* 00402500 (RET 0x10) */
rx_addr  rx_fx_fill_dwords(uint32_t n, rx_addr dst, rx_addr src);               /* 006c8230 (ESI, EDI, EDX) */
void     rx_fx_stunt_combo_start(rx_addr P);                                     /* 006cb180 (EDX) */
uint32_t rx_fx_stunt_add_30(rx_addr P, float v);                                 /* 006ccf60 (EAX; RET 4) */
void     rx_fx_package_queue_drain(rx_addr Q);                                   /* 0041b410 (RET 4) */
void     rx_fx_package_request(rx_addr name, rx_addr db_name, rx_addr Q);        /* 0041b4b0 (ECX = name chars, EDX = database name chars; RET 4) */
void     rx_fx_package_queue_push(rx_addr list, rx_addr args);                   /* 0041bca0 (EDI = list; two strings by value; RET 0x38) */
rx_addr  rx_fx_package_node_construct(rx_addr node, rx_addr args);               /* 0041bdd0 (ESI = node; two strings by value; RET 0x38) */
rx_addr  rx_fx_package_queue_pop(rx_addr out, rx_addr list);                     /* 0041bd10 (ESI = list; RET 4) -> out */
rx_addr  rx_fx_localized_name(rx_addr dst, rx_addr name);                        /* 005dbf00 (EAX = dst; RET 4) -> dst */
rx_addr  rx_fx_language_name(uint32_t index);                                    /* 005dffc0 (ESI = index) -> characters */

/* ---- fx_text.c: the matrix product, the text element's string setters, the per-thread table lookup ---- */
void     rx_fx_matrix4_mul(rx_addr a, rx_addr b, rx_addr out);                  /* 00401830 (ECX = a, EAX = b; cdecl out) */
void     rx_fx_text_set_wide(rx_addr T, rx_addr str, int32_t len);               /* 004029a0 (ESI = T; RET 8) */
void     rx_fx_text_set_wide_z(rx_addr T, rx_addr str);                          /* 00402970 (EDX = str; RET 4) */
void     rx_fx_text_set(rx_addr T, uint32_t len, rx_addr str);                   /* 004027e0 (ESI = T, EAX = len; RET 4) */
uint32_t rx_fx_thread_table_get(rx_addr self, uint32_t key, uint32_t thread);    /* 00402420 (ECX; RET 8) */

/* ---- fx_rdb.c: the small Rdb helpers (register-exact) ---- */
rx_addr  rx_fx_strmap_lower_bound(rx_addr map, rx_addr keyp);                    /* 00417da0 (EAX = map; RET 4) */
rx_addr  rx_fx_strmap_find(rx_addr map, rx_addr out, rx_addr keyp);              /* 00416e40 (ESI = map; RET 8) */
void     rx_fx_rowset_next(rx_addr it);                                          /* 00417df0 (ESI = iterator) */
rx_addr  rx_fx_crit_iter_add(rx_addr it, int32_t n);                             /* 00418e00 (ESI = iterator; RET 4) */
rx_addr  rx_fx_intsetmap_find(rx_addr map, rx_addr out, rx_addr keyp);           /* 00419ea0 (EDI = map; RET 8) */
rx_addr  rx_fx_table_row_by(rx_addr table, rx_addr name, rx_addr keyp);          /* 00419ae0 (ESI = table, EAX = name; RET 4) */
rx_addr  rx_fx_row_cell(rx_addr ref, rx_addr name);                              /* 004199a0 (EAX = ref; RET 4) */
rx_addr  rx_fx_crit_init_int(rx_addr c, uint32_t op, rx_addr valp, rx_addr name);   /* 00419a70 (ESI, EAX, ECX; RET 4) */
rx_addr  rx_fx_crit_init_float(rx_addr c, uint32_t op, rx_addr valp, rx_addr name); /* 004f73b0 (ESI, EAX, ECX; RET 4): the float criterion (value type 2) */
void     rx_fx_crit_destroy(rx_addr c);                                          /* 00415320 (ESI) */
void     rx_tbl_free(rx_addr p);                                                  /* 004f3320 (stdcall p; RET 4) */
rx_addr  rx_fx_char_vector_free(rx_addr v);                                       /* 004eee20 (ESI = v) */
void     rx_fx_variant_destroy(rx_addr v);                                        /* 004eedc0 (EDI = variant) */
void     rx_fx_crit_range_destroy(rx_addr begin, rx_addr end);                    /* 0041ac70 (cdecl begin, end, ecx) */
void     rx_fx_crit_vec_destroy(rx_addr v);                                      /* 00417590 (ESI) */
uint32_t rx_fx_table_rows(rx_addr t);                                            /* 004021d0 (EAX) */
void     rx_fx_model_entity_vf9(rx_addr self);                                   /* 00401ea0 ModelEntity::vf9 */
uint32_t rx_fx_convar_bool_toggle(rx_addr self, rx_addr out);                    /* 004172e0 ConVar<bool>::vf2 (RET 4) */

#ifdef __cplusplus
}
#endif
