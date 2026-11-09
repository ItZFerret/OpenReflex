/* world_veh_transfer.h -- a vehicle and its rider moved from one world to another (MXGame's UI world -> the event world): world_veh_transfer.c. */
#pragma once
#include "rx_game.h"

void    rx_world_physics_remove_listener(rx_addr pw, rx_addr listener);          /* 00769900 (ECX = the physics world; stack the listener; RET 4) */
void    rx_world_physics_remove_simulation(rx_addr pw, rx_addr sim);             /* 00769110 (ECX = the physics world; stack the simulation; RET 4) */
void    rx_tracks_free_slot(rx_addr tracks, int32_t slot);                       /* TerrainTracks_FreeSlot 007c3400 (ECX = the tracks, EAX = the slot) */
rx_addr rx_dyn_record_assign(rx_addr dst, rx_addr src);                          /* 0042b4a0 (ESI = dst, EDI = src) -> EAX = dst */
void    rx_dyn_records_move_slot(rx_addr vec, uint32_t index, rx_addr src);      /* 0042b300 (stdcall vector, index, source record; RET 0xc) */
void    rx_grid_slot_clear(uint32_t index, rx_addr grid, rx_addr cursor);        /* 008825f0 (ECX = index, EDX = the cursor (+0x41c / +0x420 boxes)); ESI = the grid */
void    rx_grid_slot_clear_fr(uint32_t index, rx_addr grid, uint32_t ebp, uint32_t ebx, uint32_t esi, uint32_t edi);   /* 008825f0 with its frame */
void    rx_dyn_records_remove(rx_addr vec, uint32_t index);                      /* 0042af70 (ESI = the vector; stack index; RET 4) */
void    rx_ragdoll_bodies_move(rx_addr bodies, rx_addr world);                   /* 0042a0d0 (EAX = the body handle table (rider + 0x5d0); stack the world) */
void    rx_rider_world_move(rx_addr rider, rx_addr world);                       /* 0061d160 (EAX = the rider; stack the world; RET 4) */
void    rx_vehicle_world_move(rx_addr vehicle, rx_addr world);                   /* RavenVehicleMotocross::vf31 00464a00 (ECX; stack the world; RET 4) */
