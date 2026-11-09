#ifndef RX_RACE_FORMAT_H
#define RX_RACE_FORMAT_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The race format's HUD values (w-career, 2026-10-26): what the career formats' HUD components read from the local player's
 * activity, copied. Additive; RX_SESSION_API_VERSION unchanged.
 *   activity_mode  A + 0x238: 1 race, 5 eliminator, 6 overtake, 7 speed and style, 8 outrun
 *   drivers        A + 0x26c (FUN_00559de0)
 *   riders_left    A + 0xff088, the eliminator's pending-rider index (FUN_005f65a0): MXUIEliminatorComponent shows
 *                  "riders_left/drivers" after each "Player Eliminated" (005f6ec0) and "drivers/drivers" at "Pre Entered" (005f6dd0)
 *   section, section_time  the local player's Outrun clock (P + 0x11ac, P + 0x11b4: race_outrun.c), section -1 before the first
 * valid = 1 when the local player has an activity of the race classes (MXActivityRace and its section race). */
#define RX_RACE_FORMAT_VERSION 1u
typedef struct rx_race_format_state {
    uint32_t size, version, valid;
    int32_t activity_mode, activity_state, drivers, riders_left, section;
    float section_time;
    int32_t reserved[3];
} rx_race_format_state;
/* Caller sets size and version (1). 1 on bad arguments (output untouched), else 0. Session owner thread, between advances. */
RX_API int rx_session_race_format(struct rx_session* session, rx_race_format_state* out);
#ifdef __cplusplus
}
#endif
#endif
