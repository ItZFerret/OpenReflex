/* fx_mud_bind.h -- the mud records' binding to a vehicle / rider and the rider-inserted callback with the number plate textures. */
#pragma once
#include "rx_game.h"

void rx_mud_record_bind(rx_addr rec, rx_addr veh);                                    /* 004089d0 (EDI = record, EAX = vehicle) */
void rx_mud_record_alloc(rx_addr veh);                                                /* 00408fb0 (cdecl vehicle) */
void rx_model_override_texture(rx_addr map, rx_addr name, rx_addr tex);               /* 005510a0 (cdecl) */
void rx_player_bind_name_plates(rx_addr player);                                      /* 0054bfd0 (EDI = player) */
void rx_mud_and_number_callback(rx_addr self, uint32_t rider);                        /* 0054a2e0 MudAndNumberCallback::vf0 (thiscall; RET 4) */
