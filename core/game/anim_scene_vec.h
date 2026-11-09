#pragma once
#include "rx_game.h"
rx_addr rx_scene_vector_insert(rx_addr vec, rx_addr out_it, rx_addr value_p, rx_addr proxy, rx_addr where);   /* 007f90a0 */
void    rx_scene_vector_reallocate(rx_addr vec, rx_addr value_p, rx_addr proxy, rx_addr where);               /* 007f95a0 */
/* 007f90a0 with the original's frames (anim_scene_vec.c): e = the return slot, r = EBX / EBP / ESI / EDI at the call */
#include "crt.h"
rx_addr rx_scene_vector_insert_at(rx_addr e, crt_regs4 r, rx_addr vec, rx_addr out, rx_addr value_p, rx_addr proxy, rx_addr where);
