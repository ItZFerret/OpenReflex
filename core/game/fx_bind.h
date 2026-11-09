/* fx_bind.h -- the particle effect objects' bind virtual and its name lookup (fx_bind.c). */
#pragma once
#include "rx_game.h"

int32_t rx_fx_effect_index_by_name(rx_addr set, rx_addr name);                        /* 007e3400 (ECX = set; RET 4) */
void rx_fx_effect_bind(rx_addr self, rx_addr effect_set, rx_addr name);               /* 0069b0e0 slot 1 of 18 classes (thiscall; RET 8) */
