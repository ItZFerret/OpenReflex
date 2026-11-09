/* anim_scenario.h -- the rider animation entry points, clip recorder and race scenarios (core/game/anim_scenario*.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* anim_scenario_rider.c */
rx_addr rx_animation_entity_nodes(rx_addr e);                                     /* 00617e10 */
rx_addr rx_character_entity_root_slot(rx_addr e);                                 /* 006180c0 */
uint32_t rx_rider_character_type(rx_addr e);                                      /* 006180d0 */
rx_addr rx_character_entity_root_matrix(rx_addr e);                               /* 00617fd0 */
void rx_character_entity_get_root_matrix(rx_addr e, rx_addr out);                 /* 00617ff0 */
void rx_character_entity_get_root_position(rx_addr e, rx_addr out);               /* 00618020 */
void rx_character_entity_set_root_matrix(rx_addr e, rx_addr m);                   /* 00618070 */
void rx_character_entity_reset_nodes(rx_addr e);                                  /* 00618100 */
void rx_rider_character_animate(rx_addr e, float dt);                             /* 00624ad0 */
void rx_rider_parts_set_float(rx_addr o, float x);                                /* 00616f30 */
void rx_rider_animation_kick_jobs(void);                                          /* 0061d060 */
void rx_rider_animation_wait_jobs(void);                                          /* 0061d130 */
void rx_rider_animation_reset_stunt(rx_addr ra);                                  /* 0061c5f0 */
void rx_rider_animation_set_motion_outputs(rx_addr ra);                           /* 006215e0 */
void rx_rider_animation_stance2_outputs(rx_addr ra);                               /* 00621640 (EAX = RA) */
void rx_rider_animation_set_stance(rx_addr ra, uint32_t stance);                  /* 00621720 */
void rx_rider_animation_stop(rx_addr ra);                                         /* 00621d70 */
void rx_rider_animation_reset_pose(rx_addr ra);                                   /* 00619050 */
void rx_rider_animation_restart(rx_addr ra);                                      /* 00623500 */
void rx_rider_character_release_animation(rx_addr e);                             /* 006248c0 */
rx_addr rx_anim_single_loop_sdtor(rx_addr e, uint32_t flags);                    /* 006b9b20 */
void rx_rider_character_destroy(rx_addr e);                                       /* 00624930 */
rx_addr rx_rider_character_sdtor(rx_addr e, uint32_t flags);                      /* 00624870 */
void rx_rider_list_splice(rx_addr self, rx_addr args);                            /* 00612c50 */
rx_addr rx_rider_list_construct(rx_addr self);                                    /* 00612850 */
void rx_rider_list_swap(rx_addr self, rx_addr other);                             /* 00612890 */
void rx_rider_list_splice_all(rx_addr self, uint32_t where0, uint32_t where1, rx_addr other); /* 00612a00 */
void rx_rider_list_merge(rx_addr self, rx_addr other);                            /* 00612aa0 */
void rx_rider_list_begin(rx_addr list, rx_addr out);                              /* 005c0470 (ECX = list, EAX = out) */
rx_addr rx_rider_list_end(rx_addr list, rx_addr head, rx_addr out);               /* 005c0540 (EDI = list, EAX = head, ESI = out) -> out */
uint32_t rx_rider_list_iter_ne(rx_addr a, rx_addr b);                             /* 005bb950 (EDI = a, ESI = b) -> AL */
rx_addr rx_rider_list_iter_next(rx_addr it);                                      /* 00706600 (ESI = iterator) -> it */
uint32_t rx_rider_list_grow_check(uint32_t count, rx_addr list);                  /* 005c0900 (ECX = count, EDX = list) -> new size */
rx_addr rx_rider_list_proxy_construct(rx_addr list);                              /* 00594080 (ESI = list; RET 4) -> list */
rx_addr rx_rider_list_head_alloc(void);                                           /* 00842df0 -> the head node */
void rx_rider_swap_dwords(rx_addr a, rx_addr b);                                  /* 006084d0 (cdecl a, b) */
void rx_rider_list_swap_proxies(rx_addr other, rx_addr self);                     /* 005f3520 (ESI = other, EDI = self) */
void rx_loc_sound_list_stop_owner(rx_addr owner);                                  /* 0080c880 (stdcall; RET 4) */
/* anim_scenario_clip.c */
void rx_clip_recorder_clear_clips(void);                                          /* 006aa750 */
void rx_clip_recorder_record_places(void);                                        /* 006ab530 */
uint32_t rx_clip_recorder_snapshot(void);                                         /* 006abcb0 */
void rx_clip_recorder_start_recording(rx_addr a);                                 /* 006aa7b0 */
void rx_clip_recorder_filler_clip(void);                                          /* 006ab560 */
void rx_clip_recorder_place_clips(void);                                          /* 006ab5b0 */
rx_addr rx_clip_recorder_clip_at(float t);                                        /* 006abae0 */
void rx_clip_recorder_order_clip(uint32_t idx);                                   /* 006abb30 */
rx_addr rx_clip_recorder_find_slot(int32_t type);                                 /* 006aadc0 */
void rx_clip_recorder_copy_record(rx_addr src, rx_addr dst);                      /* 006ab160 */
rx_addr rx_clip_recorder_copy_event(rx_addr src, rx_addr dst);                    /* 006ab110 */
void rx_clip_recorder_copy_samples(int32_t riders, uint32_t first, uint32_t section); /* 006ac050 */
void rx_clip_recorder_save_clip(rx_addr cand, rx_addr slot);                      /* 006aaf20 */
void rx_clip_recorder_commit_candidate(void);                                     /* 006ab220 */
void rx_clip_recorder_record(float dt);                                           /* 006aab70 */
void rx_clip_recorder_rider_reset(rx_addr s);                                     /* 006ab1c0 */
void rx_clip_recorder_log_reset(rx_addr s, rx_addr rec);                          /* 006ab800 */
void rx_clip_recorder_log_effect(rx_addr name);                                   /* 006aba80 */
/* anim_scenario_intro.c */
uint32_t rx_scenario_anim_entity_type(rx_addr e);                                 /* 00749770 */
void rx_scenario_anims_clear_attachments(rx_addr sm);                             /* 0074b060 */
rx_addr rx_scenario_cast_find(rx_addr sm, rx_addr rider);                         /* 0074b080 */
void rx_scenario_anims_stop_actors(rx_addr sm);                                   /* 0074b450 */
void rx_scenario_anims_reset_cameras(rx_addr sm);                                 /* 0074b480 */
void rx_scenario_cast_stop_all(rx_addr sm);                                       /* 0074b4f0 */
void rx_scenario_cast_clear_states(rx_addr sm);                                   /* 0074b610 */
void rx_scenario_set_riders_paused(rx_addr sc, uint32_t flag);                    /* 00749590 */
rx_addr rx_scenario_anims_find_actor(rx_addr sm, rx_addr name);                   /* 0074b0c0 */
rx_addr rx_scenario_anims_find_camera(rx_addr sm, rx_addr name);                  /* 0074b120 */
rx_addr rx_scenario_anims_find_prop(rx_addr sm, rx_addr name);                    /* 0074b680 */
void rx_scenario_anims_update_attachments(rx_addr sm);                            /* 0074b000 */
void rx_scenario_anims_attach(rx_addr owner, rx_addr sm, rx_addr e);              /* 0074af60 */
uint32_t rx_scenario_play_on(rx_addr t, uint32_t what, uint32_t arg);             /* 0074c810 */
uint32_t rx_scenario_name_of(rx_addr t, uint32_t what, rx_addr buf);              /* 0074c7e0 */
void rx_scenario_anim_entity_job(rx_addr job);                                    /* 0074ab10 */
void rx_scenario_anim_entity_set_animation(rx_addr e, rx_addr name);              /* 0074aab0 */
void rx_scenario_anim_entity_play(rx_addr e);                                     /* 0074a9f0 */
uint32_t rx_freeride_intro_update(rx_addr s);                                     /* 0074d080 */
void rx_freeride_intro_end(rx_addr s);                                            /* 0074d460 */
uint32_t rx_scenario_load_intro_script(rx_addr sc, uint32_t kind);                /* 00747750 */
void rx_scenario_begin(rx_addr sc, uint32_t kind);                                /* 00747720 */
uint32_t rx_scenario_event_read(rx_addr ev, rx_addr parser, rx_addr elem);        /* 0074c830 */
uint32_t rx_scenario_event_parse(rx_addr parser, rx_addr sc, rx_addr ev, rx_addr elem); /* 0074cd80 */
uint32_t rx_scenario_cast_load_script(rx_addr parser, rx_addr sc);                /* 0074ce40 */
void rx_scenario_script_run(rx_addr sc, float dt, rx_addr target);                /* 0074cf70 */
uint32_t rx_scenario_anims_find_player_entity(rx_addr name, rx_addr sm, rx_addr out); /* 0074b750 */
rx_addr rx_scenario_anims_find_entity(rx_addr sm, rx_addr name);                  /* 0074b970 */
void rx_scenario_intro_update(rx_addr sc, float dt);                              /* 007473f0 */
void rx_scenario_intro_script_name(uint32_t kind, rx_addr activity, rx_addr buf); /* 00747940 */
void rx_scenario_screen_effect(int32_t ms, uint32_t mode, rx_addr f);             /* 0074b200 */
void rx_overlay_color_start_blend(rx_addr ent, float duration, rx_addr esi, rx_addr edx, rx_addr ecx, float x0, float x1);   /* 0040b4e0 (ESI -> +0x120, EDX -> +0x130, ECX -> +0x140, xmm0 / xmm1; RET 8) */
void rx_scenario_cast_release(rx_addr sm);                                        /* 0074bd50 */
void rx_scenario_cast_place_entity(rx_addr sm, rx_addr avoid, rx_addr e, rx_addr place, rx_addr name, uint32_t loops); /* 0074be10 */
void rx_rider_set_start_line_animation(uint32_t state, rx_addr sc);               /* 0074d4c0 */
void rx_freeride_intro_begin(rx_addr sc, uint32_t state);                         /* 0074d2b0 */
void rx_stl_map_erase_range(rx_addr map, rx_addr out, uint32_t begin_c, uint32_t begin_n, uint32_t end_c, uint32_t end_n);   /* 00705150 (EAX; s0..s4; RET 0x14) */
void rx_rider_tree_release(rx_addr tree);                                          /* 006893b0 (ESI = the tree) */

#ifdef __cplusplus
}
#endif
