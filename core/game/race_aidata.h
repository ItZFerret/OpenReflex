/* race_aidata.h -- the AIData tables (AIBehavior / AIAbility / AIAbilityOverride) loaded by the game's init thread.
 *
 * 0075ed60 loads the "AIData" database and its package and then calls 0051e350 (after the package, at 0075f01a): the three
 * BXML assets are read with the game's XMLReader and fill the personality tables at 0x00a89d08 (memset 0xa24c: 13 driver
 * types x 17 abilities x 0xbc) and the per-behaviour tuning tables at 0x009e3xxx .. 0x00a04xxx that the AI frame code reads.
 * Ported: 0051e350 (load), 0051eac0 (AIBehavior: the behaviour value groups), 0051e430 (AIAbility: the DriverType_ /
 * DriverAbility_ tier rows), 0051e7b0 (AIAbilityOverride: Player01..Player11), 0051e1d0 (the ability's default explicit
 * slots), the 13 behaviour vf1s and their 16 value readers, plus the XMLReader attribute-cursor virtuals they need
 * (vf59 00828b50 / vf60 00828c00 / vf61 00828c30, kept local: the AIData loaders are their only caller). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

void rx_aidata_load(void);                         /* 0051e350: the three assets through the engine's Database */
void rx_aidata_behaviors(rx_addr reader);          /* 0051eac0 (EDI = the XMLReader of AIBehavior) */
void rx_aidata_abilities(rx_addr reader);          /* 0051e430 (ESI = the XMLReader of AIAbility) */
void rx_aidata_overrides(rx_addr reader);          /* 0051e7b0 (ESI = the XMLReader of AIAbilityOverride) */
void rx_aidata_ability_defaults(uint32_t type, uint32_t slot);   /* 0051e1d0 (EAX = type, ECX = the PlayerNN slot) */

/* the behaviour vf1s (ECX = the XMLReader, one stack argument: the behaviour / value-group row) */
void rx_aidata_vf_general(rx_addr r, rx_addr node);        /* 0051eed0 GeneralBehaviors */
void rx_aidata_vf_airlevel(rx_addr r, rx_addr node);       /* 0051f190 AirLeveling */
void rx_aidata_vf_avoidobjects(rx_addr r, rx_addr node);   /* 005203b0 AvoidObjects */
void rx_aidata_vf_boundary(rx_addr r, rx_addr node);       /* 00521910 AvoidTrackBoundaries */
void rx_aidata_vf_braketurn(rx_addr r, rx_addr node);      /* 00522bd0 BrakeTurn */
void rx_aidata_vf_handlebraking(rx_addr r, rx_addr node);  /* 00524630 HandleBraking */
void rx_aidata_vf_center(rx_addr r, rx_addr node);         /* 00526e80 NavigateTrack */
void rx_aidata_vf_offtrack(rx_addr r, rx_addr node);       /* 00528e80 Offtrack */
void rx_aidata_vf_overridespline(rx_addr r, rx_addr node); /* 00529920 OverrideSpline */
void rx_aidata_vf_separate(rx_addr r, rx_addr node);       /* 00531650 Seperate */
void rx_aidata_vf_splitsection(rx_addr r, rx_addr node);   /* 00533730 SplitSection */
void rx_aidata_vf_takeline(rx_addr r, rx_addr node);       /* 005341d0 TakeLine */
void rx_aidata_vf_suckracing(rx_addr r, rx_addr node);     /* 00534e30 SuckRacing */

/* the value readers (ESI = the XMLReader, EDX = the value-group row) */
void rx_aidata_read_general(rx_addr r, rx_addr node);        /* 0051ef80 */
void rx_aidata_read_airlevel(rx_addr r, rx_addr node);       /* 0051f240 */
void rx_aidata_read_avoidobjects(rx_addr r, rx_addr node);   /* 005204b0 */
void rx_aidata_read_avoidsphere(rx_addr r, rx_addr node);    /* 00520620 */
void rx_aidata_read_superavoid(rx_addr r, rx_addr node);     /* 00520790 */
void rx_aidata_read_avoideco(rx_addr r, rx_addr node);       /* 00520900 */
void rx_aidata_read_boundary(rx_addr r, rx_addr node);       /* 005219c0 */
void rx_aidata_read_braketurn(rx_addr r, rx_addr node);      /* 00522c80 */
void rx_aidata_read_handlebraking(rx_addr r, rx_addr node);  /* 005246e0 */
void rx_aidata_read_center(rx_addr r, rx_addr node);         /* 00526f30 */
void rx_aidata_read_offtrack(rx_addr r, rx_addr node);       /* 00528f30 */
void rx_aidata_read_overridespline(rx_addr r, rx_addr node); /* 005299d0 */
void rx_aidata_read_separate(rx_addr r, rx_addr node);       /* 00531700 */
void rx_aidata_read_splitsection(rx_addr r, rx_addr node);   /* 005337e0 */
void rx_aidata_read_takeline(rx_addr r, rx_addr node);       /* 00534280 */
void rx_aidata_read_suckracing(rx_addr r, rx_addr node);     /* 00534ee0 */

/* 006cc1a0 "VehicleTakeoff" (the stunt system's handler; see race_aidata.c) */
void rx_stunt_vehicle_takeoff(rx_addr s, uint32_t vehicle);

#ifdef __cplusplus
}
#endif
