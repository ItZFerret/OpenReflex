/* restored_champ_weekend.h -- RESTORED CONTENT (not an original-logic port): the championship weekend's game-side pieces
 * (restored_champ_weekend.c). The public API is include/rx_champ_weekend.h (api/champ_weekend_session.inc). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* The loaded exhibition event becomes the championship sub event of activity `type` (the original's enum, 0x1f .. 0x25 for National):
 * the activity table row and the players' requested activity name the type, the row's NumLaps is `laps` (0 = the row's own), and the
 * championship classes get the two things the console's series sequence did for them (the racing line, the intro's podium cast).
 * Call after the event setup (StartEvent) and before the first load frame. 0 ok; -1 the event has no activity row of `base_type`. */
int rx_restored_champ_apply(uint32_t base_type, uint32_t type, int32_t laps, const int32_t* in_field, const int32_t* zone, uint32_t rows);
/* in_field[r] (Player List row r): 0 = out of the stage (not Active, requests none), 1 = in (Active), -1 = in, Active untouched; zone[r] >= 0: its StartZone (the start
 * gate slot); NULL arrays = everybody in, the event's own zones. */
uint32_t rx_restored_champ_player_rows(void);                                       /* the Player List's rows */
void rx_restored_champ_player_name(uint32_t row, char* out, uint32_t capacity);     /* its PlayerName */

/* TIMED RACES (PORT ADDITION, include/rx_champ_weekend.h): the session's stage runs on a clock of `seconds` plus `extra` laps. Call after
 * rx_restored_champ_apply (which leaves the session lap-only); it patches slot 42 (the end-of-race check, run every InProgress frame) and
 * slot 61 (has driver i finished) of the race classes' vtables with the clock's wrappers, for this session's image. Lap-only stages never
 * call it: their classes and the game are then the original's. */
void rx_restored_champ_timed_arm(float seconds, int32_t extra);
/* the clock of the session's stage (activity A, the local player's; 0: none): 1 when a timed stage was armed */
struct rx_restored_champ_clock { float seconds, elapsed, expired_at, flag_at; int32_t expired, flag, extra, num_laps; };
int rx_restored_champ_timed_state(rx_addr act, struct rx_restored_champ_clock* out);
/* a driver's completed laps when it finished (it rides on after the flag and its count goes on), -1 when not finished / not timed */
int32_t rx_restored_champ_timed_finish_laps(rx_addr act, rx_addr driver);

/* The original's activity enum of an ActivityName (005964b0) of a guest string. */
uint32_t rx_restored_champ_type_of(rx_addr name);

/* A championship class's activity type (0x1f .. 0x3c) as its discipline's base type (National 0, Supercross 1, Freestyle 5,
 * WaypointRace 6); any other type unchanged. For the product's presentation copies (race_hud.c, mode_results.c), whose per-mode values
 * key on the base types. */
int32_t rx_restored_champ_base_type(int32_t type);

/* SeriesPoints.Points of {SeriesPointId = id, Place = place (1-based)} from the game's own table; 0 when no single row matches. */
int32_t rx_restored_champ_points(int32_t id, int32_t place);


#ifdef __cplusplus
}
#endif
