/* race_activity_freestyle.h -- the Freestyle event mode: MXActivityFreestyleStub (vtable 0x009441cc), MXActivityFreestyle
 * (0x009442cc) and the Freestyle championship classes MXActivityFreestyleChampIntro (0x009443cc), ...ChampPractice (0x009444cc)
 * and ...ChampMainEvent (0x009445cc): their virtual functions and the activity-level helpers they call
 * (core/game/race_activity_freestyle.c, adapters in core/oracle/race_freestyle.cpp, group rx_race). The virtual signature is the
 * core's (rx_addr this, stack arguments in order); a float stack argument is a C float. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_freestyle_stub_vf2(rx_addr A, rx_addr arg);                                                            /* 00562560 */
void rx_freestyle_start_line(rx_addr A);                                                                       /* 00561a70 */
void rx_freestyle_start_slots(rx_addr A);                                                                      /* 00562800 */
void rx_freestyle_vf2(rx_addr A, rx_addr arg);                                                                 /* 00561990 */
void rx_freestyle_stub_vf5(rx_addr A);                                                                         /* 00562440 */
void rx_freestyle_stub_vf18(rx_addr A);                                                                        /* 005626c0 */
rx_addr rx_freestyle_stub_vf44(rx_addr A, rx_addr player);                                                     /* 00562ac0 */
void rx_freestyle_stub_vf37(rx_addr A, uint32_t old);                                                          /* 00562d40 */
void rx_freestyle_vf52(rx_addr A);                                                                             /* 005620e0 */
void rx_freestyle_stunt_begin(rx_addr A, rx_addr player, rx_addr stunt);                                       /* 00562220 */
void rx_freestyle_stunt_failed(rx_addr A, rx_addr player, rx_addr stunt);                                      /* 00562250 */
void rx_freestyle_physics_stunt_begin(rx_addr A, rx_addr player, rx_addr stunt, uint32_t type);                /* 005621f0 */
void rx_freestyle_vf28(rx_addr A, float dt);                                                                   /* 00561e30 */
void rx_freestyle_vf38(rx_addr A, uint32_t old);                                                               /* 00561dc0 */
void rx_freestyle_vf49(rx_addr A, uint32_t arg);                                                               /* 005620d0 */
void rx_freestyle_vf46(rx_addr A, uint32_t kind, uint32_t on, float value);                                    /* 00562170 */
void rx_freestyle_stub_vf23(rx_addr A, float dt);                                                              /* 00562ce0 */
void rx_freestyle_stub_vf25(rx_addr A, float dt);                                                              /* 00562e00 */
void rx_freestyle_vf25(rx_addr A, float dt);                                                                   /* 00561e10 */
uint32_t rx_freestyle_stub_vf8(rx_addr A);                                                                     /* 00563d00 */
uint32_t rx_freestyle_stub_vf9(rx_addr A);                                                                     /* 0055ec10 */
uint32_t rx_freestyle_stub_vf48(rx_addr A);                                                                    /* 0055ec00 */
void rx_freestyle_stub_vf42(rx_addr A);                                                                        /* 00562bd0 */
int32_t rx_freestyle_stub_vf43(rx_addr A, rx_addr a, rx_addr b, uint32_t tie);                                 /* 00562af0 */
void rx_freestyle_stub_vf14(rx_addr A, rx_addr driver, int32_t place);                                         /* 005626e0 */
uint32_t rx_freestyle_stub_vf19(rx_addr A, rx_addr player, rx_addr pos, rx_addr out_pos, rx_addr out_up);      /* 00563820 */
uint32_t rx_freestyle_stub_vf61(rx_addr A);                                                                    /* 00563380 */
void rx_freestyle_stub_vf53(rx_addr A, uint32_t which);                                                        /* 00563ce0 */
void rx_freestyle_stub_vf39(rx_addr A, uint32_t old);                                                          /* 00562f70 */
void rx_freestyle_stub_vf40(rx_addr A, uint32_t old);                                                          /* 005633a0 */
void rx_freestyle_highlight(rx_addr tracking, uint32_t pad, rx_addr player, uint32_t remote);                  /* 006d85e0 */
float rx_fx_ai_trick_deviation(float spread);                                                                  /* 006ccc70 */
rx_addr rx_fx_trick_record(uint32_t index);                                                                    /* 006c8640 */
void rx_fx_ai_trick_scores(rx_addr scorer, float chance, float spread);                                        /* 006ccd10 */
void rx_freestyle_ai_scores(rx_addr A);                                                                        /* 005639a0 */
void rx_freestyle_stub_vf41(rx_addr A, uint32_t old);                                                          /* 00563090 */
void rx_freestyle_vf41(rx_addr A, uint32_t old);                                                               /* 00561ef0 */
void rx_freestyle_post_state(rx_addr A, uint32_t state);                                                       /* 00563aa0 */
void rx_freestyle_update_post(rx_addr A, float dt);                                                            /* 00563b10 */
void rx_freestyle_stub_vf29(rx_addr A, float dt);                                                              /* 00563330 */
void rx_freestyle_vf29(rx_addr A, float dt);                                                                   /* 00561f10 */
void rx_freestyle_stub_vf58(rx_addr A);                                                                        /* 00563c50 */
void rx_freestyle_stub_vf31(rx_addr A, uint32_t arg);                                                          /* 005635f0 */
void rx_vehicle_body_link_unset(rx_addr v);                                                                    /* 0045d4f0 */
void rx_pad_net_release(rx_addr ra);                                                                           /* 0061c690 */
void rx_freestyle_stub_vf45(rx_addr A, rx_addr from, rx_addr to);                                              /* 00563640 */
void rx_freestyle_ghost_completed(rx_addr A, rx_addr activity, rx_addr driver);                                /* 00561f80 */
void rx_freestyle_stub_vf3(rx_addr A);                                                                         /* 005627b0 */
void rx_freestyle_unlisten_ghost(rx_addr channel, rx_addr A);                                                  /* 00587b70 */
void rx_freestyle_unlisten_stunt(rx_addr name, rx_addr A, rx_addr method);                                     /* 00587bd0 */
void rx_freestyle_unlisten_physics_stunt(rx_addr A);                                                           /* 00587c30 */
void rx_freestyle_vf3(rx_addr A);                                                                              /* 00561d30 */
void rx_freestyle_champ_intro_vf3(rx_addr A);                                                                  /* 0057dc90 */
void rx_freestyle_destroy(rx_addr A);                                                                          /* 005623f0 */
rx_addr rx_freestyle_stub_sdtor(rx_addr A, uint32_t flags);                                                    /* 00562390 */
rx_addr rx_freestyle_sdtor(rx_addr A, uint32_t flags);                                                         /* 00561960 */
rx_addr rx_freestyle_champ_intro_sdtor(rx_addr A, uint32_t flags);                                             /* 0057dab0 */
rx_addr rx_freestyle_champ_main_sdtor(rx_addr A, uint32_t flags);                                              /* 0057dbf0 */
rx_addr rx_freestyle_champ_practice_sdtor(rx_addr A, uint32_t flags);                                          /* 0057de90 */
void rx_freestyle_champ_intro_vf2(rx_addr A, rx_addr arg);                                                     /* 0057daf0 */
void rx_freestyle_champ_main_vf2(rx_addr A, rx_addr arg);                                                      /* 0057dc30 */
void rx_freestyle_champ_intro_vf5(rx_addr A);                                                                  /* 0057db40 */
void rx_freestyle_champ_main_vf5(rx_addr A);                                                                   /* 0057ded0 */
void rx_freestyle_champ_intro_vf25(rx_addr A, float dt);                                                       /* 0057dbe0 */
void rx_freestyle_champ_main_vf28(rx_addr A, float dt);                                                        /* 0057dd00 */
rx_addr rx_world_freestyle_intro_cam(rx_addr w);                                                               /* 00767760 */
void rx_entity_copy_construct(rx_addr e, rx_addr src);                                                         /* 0063d580 */
rx_addr rx_camera_path_copy_construct(rx_addr path, rx_addr src);                                              /* 0063d4d0 */
rx_addr rx_animated_camera_construct(rx_addr cam, rx_addr path);                                               /* 0063d3e0 */
rx_addr rx_animated_camera_sdtor(rx_addr cam, uint32_t flags);                                                 /* 0063d3b0 */
void rx_freestyle_champ_main_vf34(rx_addr A, uint32_t state);                                                  /* 0057dd40 */
void rx_freestyle_champ_main_vf35(rx_addr A, uint32_t state);                                                  /* 0057ddd0 */
void rx_freestyle_champ_practice_vf35(rx_addr A, uint32_t state);                                              /* 0057df10 */
void rx_champ_vf38(rx_addr A, uint32_t old);                                                                   /* 0057dcf0 */
void rx_freestyle_stub_vf47(rx_addr A, rx_addr driver);                                                        /* 00563740 (build_activity_driver.c) */
#ifdef __cplusplus
}
#endif
