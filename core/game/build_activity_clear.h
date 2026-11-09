/* build_activity_clear.h -- construction unit 11: the player's vehicle / rider released (Player_ClearVehicle 0054a500 and its
 * callees): the vehicle spec and the rider entry freed with their part records and package references, the vehicle and the
 * rider objects taken out of their managers and destroyed, the slot tables of the vehicle's fx / mud / pose records released
 * (build_activity_clear.c). Calling conventions in each prototype's comment. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_ptr_vec_remove(rx_addr vec, rx_addr valp);                         /* 00544c80 (ECX = a pointer vector {+0 data, +4 count}, EDX = &value) */
void rx_part_unmark_packages(rx_addr part, rx_addr loader);                /* 00540300 (ECX = part; stdcall loader; RET 4) */
void rx_part_release_packages(rx_addr part, rx_addr loader, uint32_t hr);  /* 00540450 (ECX = part; stdcall loader, hr; RET 8) */
void rx_part_destroy(rx_addr part);                                        /* 0053f9f0 (EAX = part) */
void rx_vehicle_spec_free(rx_addr list, uint32_t vehicle_id);              /* 00540dd0 (EAX = the vehicle id; stdcall list; RET 4) */
void rx_vehicle_remove(rx_addr vehicle, rx_addr list);                     /* 005408e0 (EAX = the vehicle, EDI = the list (MXGame + 0x154b0)) */
void rx_gear_unmark_packages(rx_addr gear, rx_addr loader);                /* 0053d020 (ECX = gear part; stdcall loader; RET 4) */
void rx_gear_release_packages(rx_addr gear, rx_addr loader, uint32_t hr);  /* 0053d170 (ECX = gear part; stdcall loader, hr; RET 8) */
void rx_gear_part_destroy(rx_addr gear);                                   /* 0053ca30 (EAX = gear part) */
void rx_rider_entry_free(rx_addr entry, rx_addr manager);                  /* 0053ed20 (EDI = the rider manager; stdcall entry; RET 4) */
void rx_rider_release(rx_addr manager, uint32_t rider_id);                 /* 0053ece0 (EAX = the rider manager; stdcall rider id; RET 4) */
void rx_rider_remove(rx_addr rider, rx_addr manager);                      /* 0053ebc0 (EAX = the rider object, EDI = the manager (MXGame + 0x15504)) */
void rx_mud_record_free(rx_addr vehicle);                                  /* 00408fe0 MudRecord_Free (EDX = the vehicle) */
void rx_pose_override_release(rx_addr vehicle);                            /* 0047c380 (EDX = the vehicle) */
void rx_vehicle_effects_release(rx_addr vehicle);                          /* 00691d30 (EDX = the vehicle) */
void rx_particle_manager2d_clear(rx_addr manager);                         /* 0069a0c0 ParticleManager2D_Clear (EDI = the manager) */
void rx_vehicle_audio_release(rx_addr vehicle, rx_addr manager);          /* 00680d10 (ECX = EDX... see the .c; EDX = the vehicle; stdcall manager; RET 4) */
void rx_damage_entry_release(rx_addr entry);                               /* 00479dd0 (ECX = a damage manager entry) */
void rx_damage_release(rx_addr manager, rx_addr vehicle);                  /* 0047bbe0 (EDX = the damage manager, ESI = the vehicle) */
void rx_player_release_objects(rx_addr player);                            /* 0054bda0 (ESI = the player) */
void rx_player_save_respawn_pose(rx_addr player);                          /* 00550e10 (stdcall player; RET 4) */
#ifdef __cplusplus
}
#endif
