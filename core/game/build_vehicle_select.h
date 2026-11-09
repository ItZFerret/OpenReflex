/* build_vehicle_select.h -- the front end's vehicle choice on the Player List (build_vehicle_select.c; w-tutcore): App.ChangeVehicle's native
 * 005a0670 (MainMenu:OnTutorialKeypress: ChangeVehicle( row, 2 ), an MX Lite for the MotoSkills tutorials; NetworkSessionSearch: an MX),
 * MXTableHelper:EquipVehicle 004e7930 (the garage, the tutorials' re-equip) and MXTableHelper:GetEquippedVehicle 004e7c60.
 * Tables: "Player List" (0x00ae0e08: RequestedVehicleId / VehicleId / RequestedRiderId / RiderId / PlayerType / Controller), "Vehicle List"
 * (0x00ae05c8: VehicleId / VehicleOwner / OwnerVehicleId), the per-device PlayerVehicle / PlayerRider (0x00ae10f0 + 0x1f4 * controller), the
 * default database's AIRider / AIRiderVehicle / PresetVehicle (0x00ae0910). Vehicle owners (MXVehicleOwner): 0..3 a player's (its
 * PlayerVehicleId), 4 an AI rider's (AIVehicleId), 8 a preset (PresetVehicleId), 9 none. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 005a0670 (stdcall Player List row, VehicleTypeId; RET 8): a vehicle of that type for the row's player equipped (004e7930): an AI player
 * (PlayerType 1) its AIRiderVehicle of the type; a human (PlayerType 0) its Favorite PlayerVehicle of the type, else the first PresetVehicle
 * of the type (owner 8); any other player type: none (owner 9, -1). */
void     rx_change_vehicle(uint32_t row, uint32_t vehicle_type);
/* 004e7930 MXTableHelper_EquipVehicle (EAX = owner; stdcall row, owner vehicle id; RET 8): a Vehicle List row for the vehicle (owner 0..3:
 * also the player's PlayerRider.LastEquippedVehicle), the row's RequestedVehicleId (the Player List's change counter 0x00ae0e70), the
 * previously requested Vehicle List row erased when it was not the row's VehicleId */
void     rx_table_equip_vehicle(uint32_t owner, uint32_t row, uint32_t owner_vehicle_id);
/* 004e7c60 MXTableHelper_GetEquippedVehicle (EAX = row; stdcall &owner vehicle id, &owner; RET 8) -> 1: the row's requested (else current)
 * vehicle's Vehicle List OwnerVehicleId / VehicleOwner (row -1: nothing written) */
uint32_t rx_table_get_equipped_vehicle(uint32_t row, rx_addr owner_vehicle_id, rx_addr owner);

#ifdef __cplusplus
}
#endif
