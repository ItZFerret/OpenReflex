#pragma once
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The HUD compass' markers (game/rx_compass_api.h; api/compass_session.inc), additive to RX_SESSION_API_VERSION 2 (no existing struct
 * changed).
 *
 * The original's Free Ride HUD layer (InGame SinglePlayerFreeride.lyr) and the Trailblazer / TargetPractice layers carry an
 * MXUICompassComponent. Its heading is the camera's; the marks it places on its strip come from the running activity:
 *   - the Free Ride (the Race & Ride challenges' discovery mode, MXActivityFreeRide::vf54): one icon per challenge type, at the
 *     level's "StartActivity" trigger of that type (MXUICompassComponent::vf1_oe0 collects the group's 0x1028 triggers and files
 *     each under the index of its activity name; vf11 -> 005fe1e0 places the ten icons within 2500 ft, fading with the distance);
 *   - a TargetPractice challenge: the targets not yet hit (MXTargetPracticeCollectable at A+0xff3b4, count A+0xff3b0, hit flag
 *     +0x140; 006023a0 places the 16 target icons within 500 ft);
 *   - a Trailblazer challenge: the "Trailblazer_Finish_Trigger" entity (vf1_oe0 / 00601270's FinishLine icon);
 *   - a Waypoint race (UI kind 6): the level's WaypointRaceGate triggers (RSData 0x1043) of the activity's group, in the order
 *     vf1_oe0's scene search 007d2820 returns them (the group's bucket order: the activity's own A+0xff078 list is the same search
 *     sorted by gate index, the compass keeps the unsorted one), at most 16 (WaypointGate1..16). 005ff150 shows the icon of slot
 *     (gate + 1) at full alpha with the Distance Label under it and slot (gate + 2) at 0.35, gate = the local driver's +0x10d4;
 *     "Player Wins" for the local driver (00602ac0) hides them for the rest of the race.
 * This API hands the host those positions (the game's world space, feet); the strip, the bearing and the fades are the host's, drawn
 * from the install's layer. It computes nothing the game did not. */
#define RX_COMPASS_API_VERSION 1u
#define RX_COMPASS_MARKS 40
enum { RX_COMPASS_AREA = 1, RX_COMPASS_TARGET = 2, RX_COMPASS_FINISH = 3, RX_COMPASS_GATE = 4 };
/* One mark. type RX_COMPASS_*; index: AREA the challenge type (0 BigAir, 1 HillClimb, 2 Jumper, 3 Pathfinder, 4 StepUp, 5 Stoppie,
 * 6 Trailblazer, 7 HitDiscovery, 8 TargetPractice, 9 Wheelie: the order of the layer's icons), TARGET the target's slot, FINISH 0,
 * GATE the compass slot (0 = WaypointGate1); aux: AREA the trigger's TrackId, TARGET 1 when the target was hit, FINISH 0, GATE the
 * gate's own index (+0x280, the order the race takes them). pos: the entity's world position (matrix row 3). */
typedef struct rx_compass_mark {
    int32_t type, index, aux;
    float pos[3];
} rx_compass_mark;
/* Copied snapshot, 32 + 40 * 24 = 992 bytes; no guest addresses. valid = 1 when the local player's activity is the Free Ride or a
 * challenge or a Waypoint race. challenge_kind is the challenge's A+0xff070 (0 BigAir, 1 HillClimb, 3 Pathfinder, 4 StepUp, 6 Trailblazer,
 * 8 TargetPractice), -1 in the Free Ride and the Waypoint race. discovery_mode is MXActivityFreeRide::vf54 (A+0xff180): the activation
 * icons show only then. waypoint_gate (Waypoint race) is the local driver's last gate passed (+0x10d4, -1 before the first), else -1;
 * waypoint_finished is 1 once that driver has finished (+0x70, the race's "Player Wins" for it). Both were reserved (0) before. */
typedef struct rx_compass_state {
    uint32_t size, version, valid;
    int32_t challenge_kind;
    uint32_t discovery_mode, count;
    int32_t waypoint_gate;
    uint32_t waypoint_finished;
    rx_compass_mark marks[RX_COMPASS_MARKS];
} rx_compass_state;
/* Caller sets size (992) and version (1). Invalid arguments return 1 and leave the output alone. Session owner thread, between
 * advances. */
RX_API int rx_session_compass(struct rx_session* session, rx_compass_state* out);
#ifdef __cplusplus
}
#endif
