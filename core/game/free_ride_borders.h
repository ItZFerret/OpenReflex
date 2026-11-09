/* free_ride_borders.h -- PORT ADDITION (not original game code): FREE RIDE WITHOUT BORDERS, an OPEN REFLEX option (core/plan/QUEUE.md
 * "w-frnowalls"). The owner: "i'd rather invisible walls/barriers not exist at all in free ride".
 *
 * What resets a rider for where he is (all original, all in MXPlayer_Update 0054af90 or its trigger handler; loop_player.c):
 *   0054f330 Player_CheckOutOfBounds   the vehicle within 30 ft (terrain +0x44f0 = -30) of the base terrain's edge -> 00550a20, reason 7
 *   0054f3e0 Player_CheckResetTerrain  the vehicle over the terrain's "reset" zone (TerrainMaterialAt layer 0 == 7: the race tracks'
 *                                      out-of-bounds paint, the Alive national tracks' too) -> 00550b70, reason 6
 *   0054f0b0 the "VehicleEntered" handler: a trigger volume with TriggerID "ResetPlayer" (the scene's TriggerRSData boxes:
 *                                      BoatResetTrigger, HalfPipe_ResetTrigger, AircraftCarrierResetTrigger, ResetTunnelTrigger,
 *                                      VistaInstantResetTrigger, WaterResetTrigger) -> 00550cc0, reason 9
 * and the Free Ride's own boundary, its driver rule RuleCannonball (MXActivityFreeRide::vf47 00560c20; race_rules.c 0051a300): the world's
 * "cannonball" spline ("AI\<level>-cb.tdf07", world +0x21e10 while +0x22130; FR_Gibson: 58 nodes, a loop near the heightfield's edge)
 * is a band: past its inner line (00519fa0) the screen fades to white and the music down, at its centre line (0051a110) a local human
 * is put back (0054f150 -> 00550920 "in place": the vehicle's last ground spot +0x200, reason 5, the fade-in; no reset timer
 * +0x116c, so a counter of reset timers never sees it), and a rider beyond the line is put back every frame (he cannot move). The 3 FR
 * levels have one (Reyes, AZ = FR_Gibson: the report "still being reset in free ride if I try to leave the map"); the camera code
 * (0054c348) also treats the band as a cannon. RuleOfftrack / RuleOfftrackTimePenalty / RuleBackwards / RuleChallengeBoundary /
 * RuleInFreestyleCrowd / RuleOutOfSnakeBounds belong to the races, the challenges, Freestyle and the party modes and are never built for
 * a Free Ride driver.
 *
 * With the option on, for a HUMAN player (+0x10 == 0) whose activity (+0x94) is a Free Ride of an event world (type +0x23c == 8, mode
 * +0x238 != 4; mode 4 is the front end's UI_Compound world: the MotoSkills lessons and the garage's test ride keep the original):
 *   - no reset zone (0054f3e0 returns before its material query), except for a drowned vehicle (+0x1314, set by the water system after
 *     3 s under water or at full depth): several levels paint the zone under their sea (FMX_Dover's), and there it is the original's
 *     way out of deep water (a drowned rider is not respawned by anything else there);
 *   - no "ResetPlayer" trigger, except the deep-water ones (a trigger whose entity Name (+0x18) contains "Water": the original's way
 *     of taking a rider out of the sea on PX_Laguna / FMX_Dover; the water system's own drowning is untouched anyway);
 *   - no cannonball band: RuleCannonball never enters it for this player (no white-out, no put-back; a band already entered when the
 *     option turns on is left as the original leaves it), and the camera does not treat it as a cannon;
 *   - the edge check becomes a fall net: only a vehicle that is past the base terrain's real edge (no margin) AND has been in the air
 *     for RX_FRB_FALL_TIME seconds (the player's airtime clock +0x10bc, updated just before the check) is reset, with the original's
 *     own out-of-bounds reset (00550a20: the activity's respawn point, else the vehicle's last safe ground spot +0x200). Past the edge
 *     there is no terrain to land on, so without it the rider would fall forever; on scenery past the edge he rides on. (The FR maps'
 *     drawn end-of-world landscape, FR_GI_EoW_* on FR_Gibson, goes on past the edge with no collision: he drops through it.)
 * The player's own resets (the reset key, the save spot / back to spot), the crash recovery, the water system and every visible
 * collision stay the original's. AI riders keep the original's resets (their roam behaviours rely on them).
 *
 * Mode 0 (the core's default; the OPEN REFLEX options turn it on, the default there) is the original bit for bit: every gate reads
 * one host static first. The oracle build (RX_ORACLE) never takes the gate. */
#ifndef RX_FREE_RIDE_BORDERS_H
#define RX_FREE_RIDE_BORDERS_H

#include "../src/rx_mem.h"
#include <stdint.h>

#define RX_FREE_RIDE_BORDERS_ORIGINAL 0   /* the same spelling as rx_api.h */
#define RX_FREE_RIDE_BORDERS_OPEN     1

#define RX_FRB_FALL_TIME 4.0f             /* seconds of air past the terrain's edge before the fall net resets (a 257 ft fall) */

extern uint32_t rx_frb_mode_value;        /* the session's mode (rx_session_set_free_ride_borders) */
void     rx_frb_set(uint32_t mode);
uint32_t rx_frb_free_ride_human(rx_addr player);    /* 1: a human player in an event world's Free Ride */
uint32_t rx_frb_keep_trigger(rx_addr trigger);      /* 1: a "ResetPlayer" trigger that stays (a deep-water one) */
uint32_t rx_frb_fallen(rx_addr player, rx_addr terrain, rx_addr vehicle);   /* the fall net: past the edge and RX_FRB_FALL_TIME in the air */

/* No collision is left out (w-dfcollide). w-frnowalls left FMX_DoverB's "COL_DO_MegaRamp_Screen" out of a Free Ride as an invisible wall,
 * but that was the survey tool's error: core/tools/collis_visibility_check.c placed each collision chunk's triangles without the
 * placement's rotation (the original rotates them: 00819650 / 00769550). Placed right, the panel is the JumboTron's screen above the mega
 * ramp (a 171 x 84 ft box on the drawn FMX_Roll_In_Screen, between the frame's beams), and riders flew through it. Corrected, the survey
 * lists only collision of drawn objects on every level of the install and the 26 Alive levels (the hulls of cliffs, rocks and trusses, the
 * Alive Farm walls a few feet over their drawn tops), so every CollisionRSData placement is the original's in Free Ride too. */

/* the gate: 1 when the borders are off for this player (never in the oracle build) */
#ifdef RX_ORACLE
static inline uint32_t rx_frb_for(rx_addr player) { (void)player; return 0; }
#else
static inline uint32_t rx_frb_for(rx_addr player) { return rx_frb_mode_value ? rx_frb_free_ride_human(player) : 0u; }
#endif

/* counters for the development tools (core/tools/freeride_borders.c): frames the zone test did not run, triggers skipped / kept, frames
 * the edge zone did not reset, fall-net resets started, frames in the cannonball band not entered */
typedef struct rx_frb_stats { uint32_t zone_skipped, trigger_skipped, trigger_kept, edge_skipped, fall_resets, cannonball_skipped; } rx_frb_stats;
extern rx_frb_stats rx_frb_stats_value;

#endif
