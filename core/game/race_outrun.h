/* race_outrun.h -- the Outrun format's timed sections (race_outrun.c). */
#pragma once
#include "rx_game.h"

rx_addr rx_timed_section_create(void);                                                     /* the factory arm 0040571f (type 0x107b) */
rx_addr rx_timed_section_sdtor(rx_addr section, uint32_t flags);                           /* 00413960 TimedRaceSection slot 0 */
void    rx_timed_section_load(rx_addr section, rx_addr reader);                            /* 00413990 TimedRaceSection::vf5 */
void    rx_event_timed_section_entered(rx_addr channel, rx_addr driver, uint32_t number, rx_addr activity);   /* 00554df0 */
void    rx_player_timed_section_entered(rx_addr player, rx_addr section);                  /* 0054f4b0 (ECX = player, EAX = section) */
void    rx_race_entered_timed_section(rx_addr activity, rx_addr vehicle, rx_addr section); /* 00569550 (thiscall; RET 8) */
