#ifndef RX_CAREER_WAYPOINT_H
#define RX_CAREER_WAYPOINT_H
#include "rx_career_completion.h"
/* Explicit additional owned HUD capability. Loading a library/header never
 * grants presenter support. No existing public struct or enum is modified. */
#define RX_CAREER_PRESENT_WAYPOINT_HUD 256u
#define RX_CAREER_WAYPOINT_REQUIRED_CAPABILITIES 498u
#define RX_CAREER_ALL_MODE_CAPABILITIES 511u
/* Finite catalog adds actual Tour SingleEvent WaypointRace3/4/class15/type14.
 * Current profile locks still decide eligibility. Load rechecks the needed HUD
 * capability before mutation. National still requires bit1 and minimum243;
 * Waypoint requires bit256 and minimum498. Full completion also needs explicit
 * Rewards|Career bits4|8. No renderer or replay acceptance is implied.
 * Copied status.activity_substate uses Waypoint+13e618, National+13e640.
 * For Waypoint result rows, dnf is logical NOT of actual0057eb80 finished
 * predicate (signed completed laps >= authored laps), not a RaceDriver field.
 * Existing completion ABI160B, exact typed witness and durable receipt policy
 * remain unchanged. Replay startup/pose/tick/host closure is a prerequisite
 * before advertising end-to-end Waypoint completion. */
#endif
