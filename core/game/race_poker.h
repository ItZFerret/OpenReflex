/* race_poker.h -- MXActivityPokerRace (the PokerRace party mode, activity type 0xd) and its driver, rule and audio logic
 * (race_poker.c; vf47 in build_activity_driver.c). */
#ifndef RX_RACE_POKER_H
#define RX_RACE_POKER_H
#include "rx_game.h"

uint32_t rx_poker_draw_card(rx_addr act);                                                  /* 00571ca0 (EDI = act) -> card or -1 */
int32_t  rx_activity_driver_place(rx_addr act, rx_addr driver);                            /* 00557760 (EDX = act, ESI = driver) */
void     rx_event_activity_driver_at(rx_addr manager, rx_addr name, rx_addr driver, rx_addr act);   /* 00589b80 (stdcall act) */
uint32_t rx_poker_track_audio_kind(rx_addr level, uint32_t area);                          /* 00571c20 (EDI = level, ESI = area) */
rx_addr  rx_poker_audio_logic_construct(rx_addr logic);                                    /* 00584020 (ESI = logic) */
void     rx_race_audio_logic_vf32(rx_addr logic);                                          /* 00582d50 */
rx_addr  rx_poker_race_construct(rx_addr act, uint32_t type, rx_addr world);               /* 00571000 (EDI = act; stdcall type, world) */
rx_addr  rx_poker_race_sdtor(rx_addr act, uint32_t flags);                                 /* 00571070 */
void     rx_poker_race_vf2(rx_addr act, rx_addr arg);
void     rx_poker_race_vf3(rx_addr act);
void     rx_poker_race_vf4(rx_addr act);
void     rx_poker_race_vf18(rx_addr act);
void     rx_poker_race_vf23(rx_addr act, float dt);
void     rx_poker_race_vf27(rx_addr act, float dt);
void     rx_poker_race_vf28(rx_addr act, float dt);
void     rx_poker_race_vf29(rx_addr act, float dt);
void     rx_poker_race_vf34(rx_addr act, uint32_t unused);
void     rx_poker_race_vf38(rx_addr act, uint32_t old_state);
void     rx_poker_race_vf42(rx_addr act);
int32_t  rx_poker_race_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);
rx_addr  rx_poker_race_vf44(rx_addr act, rx_addr player);
void     rx_poker_race_vf47(rx_addr act, rx_addr driver);                                  /* 005714b0 (build_activity_driver.c) */
void     rx_poker_lap_completed(rx_addr act, rx_addr driver, int32_t lap, uint32_t c, uint32_t d);   /* 00571d60 */
void     rx_poker_evaluate_driver(rx_addr act, rx_addr driver);                            /* 005723c0 (stdcall) */
rx_addr  rx_poker_driver_construct(rx_addr d, rx_addr player);                             /* 005134c0 (ESI = d, EAX = player) */
void     rx_poker_driver_pick(rx_addr d);                                                  /* 00513630 (ESI = d) */
void     rx_rule_offtrack_vf1(rx_addr rule, rx_addr d);                                   /* 0051ab70 RuleOfftrack::vf1 */
void     rx_rule_offtrack_vf4(rx_addr rule, uint32_t on);                               /* 0051aba0 RuleOfftrack::vf4 */
void     rx_rule_offtrack_vf5(rx_addr rule, float dt);                                  /* 0051abc0 RuleOfftrack::vf5 */
void     rx_race_driver_vf16(rx_addr d, uint32_t kind, rx_addr object);                    /* 00512bc0 RaceDriver::vf16 */
void     rx_race_driver_vf17(rx_addr d);                                                   /* 00512c30 RaceDriver::vf17 */
void     rx_poker_audio_vf3(rx_addr logic);                                                /* 00584070 */
void     rx_poker_audio_vf4(rx_addr logic);                                                /* 00584230 */
void     rx_poker_sort5(rx_addr hand);                                                      /* 00572360 (ECX = hand) */
void     rx_poker_rank_hand(rx_addr act, rx_addr kind, rx_addr hand, rx_addr k1, rx_addr k2, rx_addr k3, rx_addr k4, rx_addr k5);   /* 00572720 */
void     rx_rule_offtrack_left(rx_addr rule);                                              /* 0051ae90 (EDX = rule) */
void     rx_rule_offtrack_put_back(rx_addr rule);                                          /* 0051af60 (EAX = rule) */
void     rx_poker_lap_listen(rx_addr channel, rx_addr act);                                /* 00589590 (EAX = channel; stdcall act) */
void     rx_poker_net_removed_listen(rx_addr channel, rx_addr act);                        /* 00589650 */
void     rx_poker_net_community_listen(rx_addr channel, rx_addr act);                      /* 00589710 */
void     rx_poker_net_hand_listen(rx_addr channel, rx_addr act);                           /* 005897d0 */
void     rx_poker_net_choice_listen(rx_addr channel, rx_addr act);                         /* 00589890 */
void     rx_poker_lap_unlisten(rx_addr channel, rx_addr act);                              /* 00589950 */
void     rx_poker_net_removed_unlisten(rx_addr channel, rx_addr act);                      /* 005899b0 */
void     rx_poker_net_community_unlisten(rx_addr channel, rx_addr act);                    /* 00589a10 */
void     rx_poker_net_hand_unlisten(rx_addr channel, rx_addr act);                         /* 00589a70 */
void     rx_poker_net_choice_unlisten(rx_addr channel, rx_addr act);                       /* 00589ad0 */
void     rx_poker_net_object_removed(rx_addr act, uint32_t kind, rx_addr object);          /* 00571d40 */
void     rx_poker_audio_listen_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);   /* 0058c930 (ECX, EAX) */
void     rx_poker_audio_listen_player_wins(rx_addr channel, rx_addr logic);                /* 0058c9f0 (EAX) */
void     rx_poker_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058cab0 (ECX, EAX) */
void     rx_poker_audio_unlisten_player_wins(rx_addr channel, rx_addr logic);              /* 0058cb10 (EAX) */
void     rx_poker_audio_flop(rx_addr logic, rx_addr act, rx_addr driver);                  /* 005843f0 */
void     rx_poker_audio_card_available(rx_addr logic, rx_addr act, rx_addr driver);        /* 00584410 */
void     rx_poker_audio_card_selected(rx_addr logic, rx_addr act, rx_addr driver);         /* 00584430 */
void     rx_poker_driver_vf1(rx_addr d);                                                   /* 00513560 */
void     rx_poker_driver_vf2(rx_addr d);                                                   /* 00513570 */
void     rx_poker_driver_vf3(rx_addr d);                                                   /* 00513580 */
void     rx_poker_driver_vf7(rx_addr d, float dt);                                         /* 005135e0 */
#endif
