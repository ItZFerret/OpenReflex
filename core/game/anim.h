/* anim.h -- MX vs ATV Reflex rider / character animation (core/game/anim_*.c). Layouts and conventions: core/notes/anim.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* anim_node.c -- node hierarchy helpers of the animation code */
void rx_node_set_local_rotation(rx_addr rec, rx_addr quat);                       /* 007dcdf0 */
void rx_node_set_local_translation(rx_addr rec, rx_addr v);                       /* 007dd160 */
void rx_node_local_rotation(rx_addr rec, rx_addr out);                            /* 007dd380 */
rx_addr rx_nodeset_next_unvisited(rx_addr walk);                                  /* 007deaa0 */
rx_addr rx_nodeset_next_unvisited_regs(rx_addr walk, uint32_t caller_ebx, uint32_t caller_esi);   /* 007deaa0 with the caller's EBX / ESI */
void rx_nodeset_copy_from(rx_addr dst, rx_addr src);                              /* 007dd930 */
void rx_nodeset_release(rx_addr set);                                             /* 007dd4d0 (EDI = the set) */
uint32_t rx_hierarchy_has_node(rx_addr h, rx_addr name);                          /* 007de980 */
rx_addr rx_hierarchy_find_node(rx_addr h, rx_addr name);                          /* 007de9e0 */
void rx_octree_replace_item(rx_addr node, rx_addr item_new, rx_addr item_old);    /* 007df2e0 */
void rx_node_update_subtree_bounds(rx_addr rec);                                  /* 007dbde0 */
void rx_node_subtree_bounds(rx_addr rec, rx_addr ext_out, rx_addr center_out, rx_addr radius_out); /* 007dd450 */

/* anim_animators.c -- animators (vtable slot 4 = evaluate (character, root delta out, pose)) */
void rx_blend_animator_eval_one(rx_addr self, rx_addr pose, rx_addr ch, rx_addr delta_out, uint32_t which);   /* 00834390 */
void rx_additive_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);                   /* 008349c0 */
void rx_blend_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);                      /* 00833080 */
void rx_motion_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);                     /* 0083bc70 */
void rx_pose_mirror(rx_addr pose);                                                                            /* 0083c2f0 */
uint32_t rx_motion_animator_get_param(rx_addr self, rx_addr name, rx_addr out);                              /* 0083b3e0 */
uint32_t rx_blend_animator_get_param(rx_addr self, rx_addr name, rx_addr out);                               /* 00834570 */
uint32_t rx_switch_animator_get_param(rx_addr self, rx_addr name, rx_addr out);                              /* 0083ce40 */
uint32_t rx_performance_animator_get_param(rx_addr self, rx_addr name, rx_addr out);                         /* 0083d430 */
uint32_t rx_animator_get_param(rx_addr self, rx_addr name, rx_addr out);                                     /* 008d8db0 */
uint32_t rx_hashmap_bucket(rx_addr map, rx_addr key);                                                         /* 00842630 (ESI = map, EAX = key*; EAX) */
rx_addr  rx_hashmap_find(rx_addr map, rx_addr out, rx_addr key);                                              /* 008424e0 (EAX = map; RET 8; EAX = out) */
void rx_performance_additive_clip(rx_addr clip, rx_addr ch, rx_addr pose, float weight);   /* 0083e930 (EAX = clip animator; stack ch, pose, weight; RET 0xc) */
void rx_performance_additive_clip(rx_addr clip, rx_addr ch, rx_addr pose, float weight);   /* 0083e930 (EAX = clip animator; stack ch, pose, weight; RET 0xc) */
void rx_performance_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);                /* 0083e290 */
void rx_switch_animator_start_transition(rx_addr self);                                                       /* 0083c9c0 */
void rx_switch_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);                     /* 0083cad0 */
void rx_pose_copy_bone(rx_addr src, rx_addr dst);                                                             /* 0083c530 */

/* anim_character.c -- keyframe sampling */
/* helpers shared with the animators (not original functions on their own: the samplers inline them) */
void anim_sample_rotation_track(rx_addr track, float time, rx_addr q);
int32_t anim_key16_upper(rx_addr keys, int32_t n, float time);
void rx_character_sample_keyframes(rx_addr c, float dt);                                                      /* 00835430 */
void rx_character_transition_blend(rx_addr c, float w);                                                       /* 00838ba0 */
void rx_character_transition_sample(rx_addr c, float w);                                                      /* 00837590 */
void rx_float_track_sample(rx_addr track, rx_addr out, float time);                                           /* 00839f40 */
void rx_character_random_motion_time(rx_addr c);                                                              /* 00835170 (ESI = C) */
uint32_t rx_floatkeys_find_segment(rx_addr keys, uint32_t count, float time, rx_addr prev, rx_addr next, rx_addr frac); /* 008435f0 */
struct fmt_vfs;
void rx_anim_sets_load(const struct fmt_vfs* vfs);                                                          /* 006b68e0 AnimationSets (product construction) */
void rx_character_sample_motion_at(rx_addr slot, rx_addr c, float phase);                                    /* 00835bf0 */
rx_addr rx_character_anim_set_animation(rx_addr c, rx_addr handle, uint32_t loops);                         /* 00834eb0 */
uint32_t rx_bone_cache_find(rx_addr map, rx_addr name_cell, rx_addr out);                                   /* 00843350 (EAX, EDI, s0; RET 4) */
void     rx_bone_cache_insert(rx_addr map, rx_addr name_cell, rx_addr value);                              /* 008433b0 (EDI, s0, s1; RET 8) */
rx_addr  rx_action_context_dtor(rx_addr self, uint32_t flag);                                             /* 0082af60 (ECX, s0; RET 4) */
void rx_character_anim_release(rx_addr c);                                                                    /* 00834d80 */
rx_addr rx_character_anim_find_slot(rx_addr handle, rx_addr c);                                               /* 00835000 */
void rx_character_anim_play(rx_addr c, rx_addr slot, float blend);                                            /* 00837510 */
void rx_character_anim_capture_pose(rx_addr c);                                                               /* 008351c0 (stdcall C) */
void rx_character_entity_play_slot(rx_addr e, rx_addr slot);                                                  /* 00839bb0 */
void rx_character_entity_animate(rx_addr e, float dt);                                                        /* 00839be0 */
void rx_character_entity_set_loop_animation(rx_addr e, rx_addr handle, uint32_t loops);                       /* 00839c30 */
void rx_character_entity_release(rx_addr e);                                                                  /* 00839b00 */
void rx_animator_nop3(rx_addr self, rx_addr a, rx_addr b, rx_addr c);                                         /* 0083d480 */
void rx_character_sample_blend3(rx_addr slot_c, rx_addr c, rx_addr slot_a, rx_addr slot_b, float phase_a, float phase_b,
                                float phase_c, float w);                                                      /* 008363b0 */

/* anim_scene.c -- scene manager bookkeeping, tickers, event grid, lights */
void rx_scene_profile_scope(rx_addr name);                                                                    /* 007d1cd0 */
void rx_scene_register_entity_name(rx_addr scene, rx_addr entity);                                           /* 007d2500 (EAX = scene, stack entity) */
void rx_scene_manager_tick(rx_addr s);                                                                        /* 007d23a0 */
void rx_scene_manager_tick_late(rx_addr s);                                                                   /* 007d21d0 */
void rx_scene_manager_update_render_sync(rx_addr s);                                                          /* 007d2190 */
void rx_scene_manager_post_update(rx_addr s);                                                                 /* 007d2490 */
rx_addr rx_scene_manager_find_typed(rx_addr s, uint32_t type, rx_addr name);                                  /* 007d2e70 */
uint32_t rx_scene_manager_collect_typed(rx_addr s, uint32_t type, rx_addr out, int32_t cap, rx_addr overflow); /* 007d2920 */
rx_addr rx_scene_manager_find_named(rx_addr s, rx_addr name);                                                 /* 007d2da0 */
rx_addr rx_scene_manager_find_sorted0(rx_addr s, rx_addr out, uint32_t e);                                    /* 007d3a40 */
rx_addr rx_scene_manager_find_sorted1(rx_addr s, rx_addr out, uint32_t e);                                    /* 007d3b50 */
rx_addr rx_scene_manager_find_flagged28(rx_addr s, rx_addr out, uint32_t e);                                  /* 007d3c40 */
rx_addr rx_scene_manager_find_flagged2c(rx_addr s, rx_addr out, uint32_t e);                                  /* 007d3d30 */
void rx_scene_manager_unlink(rx_addr s, rx_addr e);                                                           /* 007d3850 */
void rx_scene_manager_link(rx_addr s, rx_addr e);                                                             /* 007d3580 */
void rx_scene_manager_add_entity(rx_addr e, rx_addr s);                                                       /* 007d2f70 */
rx_addr rx_scene_manager_find_in_subscene(rx_addr s, rx_addr name, rx_addr subscene);                      /* 007d2c80 (stdcall; RET 0xc) */
void rx_scene_manager_remove_entity(rx_addr e, rx_addr s);                                                    /* 007d3040 */
uint32_t rx_event_grid_cells_in_radius(rx_addr g, rx_addr pos, float radius, rx_addr out);                    /* 007d7230 */
uint32_t rx_event_cell_remove_b(rx_addr cell, rx_addr ev);                                                    /* 007da810 */
uint32_t rx_event_cell_remove_a(rx_addr cell, rx_addr ev);                                                    /* 007dad20 */
void rx_event_cell_push_l38(rx_addr cell, rx_addr ev);   /* 007d61a0 */
void rx_event_cell_push_l44(rx_addr cell, rx_addr ev);   /* 007d6220 */
void rx_event_cell_push_l20(rx_addr cell, rx_addr ev);   /* 007d62a0 */
void rx_event_grid_add_l38(rx_addr g, rx_addr pos, uint32_t id, float radius, float value);   /* 007da4d0 */
void rx_event_grid_add_l44(rx_addr g, rx_addr pos, uint32_t id, float radius, float value);   /* 007da5a0 */
void rx_event_grid_add_l20(rx_addr g, rx_addr pos, uint32_t id, float radius, float value);   /* 007da670 */
void rx_event_cell_push_a(rx_addr cell, rx_addr ev);                                                          /* 007d6320 */
uint32_t rx_event_grid_remove_b(rx_addr g, uint32_t id, float radius, rx_addr pos);                           /* 007da1d0 */
uint32_t rx_event_grid_remove_a(rx_addr g, uint32_t id, float radius, rx_addr pos);                           /* 007da410 */
void rx_event_grid_add_a(rx_addr g, rx_addr pos, uint32_t id, float radius, float value);                     /* 007da740 */
void rx_lights_add(rx_addr L, rx_addr e);                                                                     /* 007dfdd0 */
void rx_model_instance_destroy(rx_addr o, uint32_t ecx_in);                                                   /* 007d4030 */

#ifdef __cplusplus
}
#endif
