/* loop_player_destroy.h -- MXPlayer's destruction (loop_player_destroy.c). */
#pragma once
#include "rx_game.h"

rx_addr rx_mxplayer_sdtor(rx_addr player, uint32_t flags);            /* MXPlayer::sdtor 005497e0 (thiscall; flags; RET 4) */
void    rx_mxplayer_destroy(rx_addr player);                           /* 00549830 (stdcall player; RET 4) */
void    rx_player_unlisten_network_object(rx_addr name, rx_addr registry, rx_addr player, rx_addr member);   /* 005549f0 (EAX, ECX; RET 8) */
void    rx_player_unlisten_stunt_update(rx_addr registry, rx_addr player);                                   /* 00554a50 (EAX; RET 4) */
void    rx_player_unlisten_vehicle_reset(rx_addr registry, rx_addr player);                                  /* 00554ab0 (EAX; RET 4) */
void    rx_player_unlisten_stunt(rx_addr name, rx_addr registry, rx_addr player, rx_addr member);            /* 00554b10 (EAX, ECX; RET 8) */
void    rx_player_unlisten_physics_stunt(rx_addr name, rx_addr registry, rx_addr player, rx_addr member);    /* 00554b70 (EAX, ECX; RET 8) */
void    rx_player_unlisten_vehicle_entered(rx_addr player);                                                  /* 00554bd0 (RET 4) */
void    rx_stunt_unlisten(rx_addr name, rx_addr registry, rx_addr system, rx_addr member);                   /* 006cdbd0 (EAX, ECX; RET 8) */
void    rx_stunt_unlisten_combo(rx_addr registry, rx_addr system);                                           /* 006cdc30 (EAX; RET 4) */
void    rx_stunt_unlisten_physics(rx_addr name, rx_addr registry, rx_addr system, rx_addr member);           /* 006cdc90 (EAX, ECX; RET 8) */
void    rx_stunt_unlisten_vehicle(rx_addr name, rx_addr registry, rx_addr system, rx_addr member);           /* 006cdcf0 (EAX, ECX; RET 8) */
void    rx_stunt_system_unregister(rx_addr system);                     /* 006cb010 (EDI = the stunt system) */
void    rx_stunt_set_destroy(rx_addr set);                              /* 005ba710 (ESI = the set) */
