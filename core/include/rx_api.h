/* rx_api.h -- the OPEN REFLEX session API for Unity (core/api/rx_session.c).
 *
 * This is the flat, versioned C API the Unity client calls instead of the old lifted/emulator plugin. A session owns one
 * install, one event and one running world; it is created from the install root, the event is loaded from an rx_event_spec
 * (the front end's UI properties), and rx_session_advance runs the game's own frame (MXGame::Update). The getters return the
 * state the C# renderers already read; the struct layouts mirror the old plugin's (ReflexBikeState, ReflexCamera,
 * ReflexRaceState, ...) field for field, with the rx_ prefix.
 *
 * STATUS (2026-10-21): the product session loads and runs MX events through the game's own frame.
 * The current Concord steering acceptance completes 24,000 steps / 400.130 game seconds without a stop.
 * The Unity client renders this API in a standalone Mono player and starts directly from the core session,
 * without a legacy world/plugin. See HANDOFF/SESSION_2026-10-21_CORE_STARTUP.md for validation and remaining boundaries.
 *
 * THREADING: one session belongs to ONE simulation thread, like OriginalSimulation. No two threads may call
 * rx_session_advance (or event_load / destroy) concurrently, and getters must not run while advance is running. The C# side
 * already serializes all native calls on its simulation thread; keep that contract. Create and destroy may be called from the
 * thread that will own the session. The core has ONE global arena: one session per process at a time (destroy releases it).
 * create -> destroy -> create in one process works (the editor's second Play): destroy unmaps the arena, releases the host
 * resources and restores the core's static data to its loaded state, so the next session behaves exactly like the first
 * (core/notes/session-reset.md; tools/rx_api_recreate_test). A create while a session is still open returns NULL.
 * TWO IMAGES (w-loadarena): the arena and the statics belong to the loaded library image, so a second copy of the library loaded
 * from another file (dlopen / LoadLibrary of a copy) is a second, independent core with its own session; the two may run on two
 * threads at once (the client rides the test-ride arena in one while the player's event loads in the other). The fault guard
 * keeps no per-call process-wide state for that (tools/rx_two_images_test.sh checks two sessions side by side). 
 *
 * FAULTS: rx_session_create, rx_session_event_load and rx_session_advance run the game under a signal guard: an unported game
 * function (rx_call_original aborts) or a fault inside the core fails the call, puts the session in RX_SESSION_ERROR and names the
 * cause in rx_session_last_error ("... unported game function 0069b0e0 reached ..."). The game state is then inconsistent: only
 * the getters, last_error and destroy are valid; destroy then resets the process as after a normal end. (On platforms
 * without statics restore, currently other than macOS and the Windows core build, a second create is refused.)
 *
 * LIFETIMES: strings returned by rx_session_last_error are owned by the session (valid until its next call or destroy). The
 * getters write caller-provided buffers and keep no pointers into game memory; the matrices/patches are copied out. A NULL
 * session is accepted by rx_session_last_error (it returns the last create error).
 *
 * VERSIONING: RX_SESSION_API_VERSION is the ABI this header describes. History: 1 = first skeleton; 2 = adds rx_pad,
 * rx_session_set_input and rx_session_sizeof (no existing struct changed; the C# bridge, Unity/Reflex/Assets/Scripts/OpenReflex,
 * requires >= 2). Version 2 also adds rx_session_rider_pose_slot / rx_session_bike_pose_slot and fixes rx_session_bike
 * (each pose field holds four body vec4s; each row is copied from its own 0xa0-stride body record). Also in version 2 (additive exports, no layout change): the
 * input / clock exports rx_session_set_mouse, _set_cursor, _rumble, _step_dt, _pace, _alpha, _engine_dt (core/notes/input-clock.md); the C# bridge
 * does not need them to load. Also in version 2 (additive, unity2, 2026-10-20): rx_hud_state / rx_session_hud, rx_session_race_line,
 * rx_particle / rx_session_particles_array, rx_vehicle_info / rx_session_vehicle_info - the first-ride readers the old plugin had and the
 * session API lacked; no existing struct or call changed. Also in version 2 (additive, unityscene, 2026-10-20): rx_ambient_entity /
 * rx_session_ambient_entities, rx_scene_object / rx_session_scene_objects / rx_session_scene_object_matrix, rx_session_node_flags -
 * the first-ride scene exports (ambient entity matrices, scene objects, vehicle node flags); no existing struct changed.
 * Also in version 2 (additive, tyretracks, 2026-10-20): rx_track_info / rx_track_ribbon, rx_session_tracks and
 * rx_session_track_ribbons - the live TerrainTracks ribbon triangles and their strip descriptors (the old reflex_world_tracks
 * floats/triangle contract; no existing struct changed).
 * Also in version 2 (additive, terrtiles, 2026-10-29): rx_terrain_rect / rx_session_terrain_tiles - the terrain's changed 32 x 32 tiles
 * as at most 16 rectangles (instead of the one bounding rectangle of rx_terrain_info), and rx_session_tracks / rx_session_track_ribbons
 * share one decode per frame; no existing struct or call changed, so a player built before this still runs (it looks the new call up
 * and falls back to the single rectangle when it is missing). rx_session_sizeof knows "rx_terrain_rect".
 * Also in version 2 (additive, freeair, 2026-10-29): rx_session_set_free_air / rx_session_free_air - FREER AIR, the OPEN REFLEX
 * option that leaves out the original's in-air helpers for the local players (off by default); no struct changed, a player built
 * before it still runs.
 * Also in version 2 (additive, freeair2, 2026-10-29): rx_session_set_free_air_looseness / rx_session_free_air_looseness (AIR LOOSENESS).
 * Also in version 2 (additive, frnowalls, 2026-10-30): rx_session_set_free_ride_borders / rx_session_free_ride_borders (FREE RIDE WITHOUT
 * BORDERS, the OPEN REFLEX option; the core's default is the original); no struct changed.
 * Also in version 2 (additive, weather grip, 2026-11-07): rx_session_set_weather_grip / rx_session_weather_grip (rain / snow traction;
 * the default 1 is the original); no struct changed.
 * Also in version 2 (additive, w-lb-practice, 2026-11-01; core/plan/LEADERBOARDS.md 3.3): rx_session_set_practice / rx_session_practice
 * (PRACTICE), rx_lap_record / rx_session_lap_log (the LAP LOG) and rx_session_local_phys_class; rx_session_sizeof knows "rx_lap_record";
 * no existing struct changed.
 * Also in version 2 (additive, w-lb-markers, 2026-11-02): rx_lap_record.markers appended (rx_session_sizeof("rx_lap_record") tells the
 * size: a host built for the older record sees a different size and reads no lap log); rx_session_spawn_at also works in PRACTICE.
 * Also in version 2 (additive, w-alivegates, 2026-11-03): rx_session_set_map_scale_base / rx_session_map_scale_base /
 * rx_session_map_scale_records (BASE SCALE: a track's default MAP SCALE, at which its records are kept); no struct changed.
 * rx_session_version(s) returns the version recorded in
 * the handle; a session created by a different library version is rejected by every call (they return an error and set
 * last_error). The structs below must not change without a version bump.
 */
#ifndef RX_API_H
#define RX_API_H

#include <stdint.h>
#include <stddef.h>

/* rx_event_spec: the event's UI property block (game/build_activity.h owns it). */
#include "../game/build_activity.h"

#if defined(_WIN32)
#define RX_API __declspec(dllexport)
#else
#define RX_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RX_SESSION_API_VERSION 2u

typedef struct rx_session rx_session;

/* ---- the rider cap (w-rc-core, port addition; core/plan/RIDER_CAP.md) ----
 * The original races at most 12 riders (12 player seats, 12 standings ...); the core races up to RX_MAX_RIDERS: riders 13..32 live in
 * the port's side storage (game/rider_cap.h), entries 0..11 stay where the original has them, so an event of 12 or fewer riders is the
 * original's, byte for byte. Every API that takes a player slot / rider index takes 0 .. RX_MAX_RIDERS - 1.
 *   rx_event_spec.riders          an exhibition's EventRiders (the player + AI): 1 .. RX_MAX_RIDERS, no setting needed.
 *   rx_session_set_field_size     OPT-IN: the MaxRidersEventType / MaxRidersVehicleClass game tables (12 for every event type and class:
 *                                 the career's and the shell's field sizes) answer `riders` (13 .. RX_MAX_RIDERS) instead; 0 = the
 *                                 tables (the original, the default). Per session (a destroyed session forgets it); any time before the
 *                                 event / career round loads. 0 ok, 1 bad session, 2 out of range. The level scripts' maxplayerssupported
 *                                 (12 on Alive levels) is the client's to read: the core does not limit by it. */
#define RX_MAX_RIDERS 32
#define RX_ORIGINAL_RIDERS 12
RX_API int rx_session_max_riders(void);
RX_API int rx_session_set_field_size(struct rx_session* s, int32_t riders);
RX_API int rx_session_field_size(const struct rx_session* s);   /* the setting (0 = the tables) */

/* ---- lifecycle ------------------------------------------------------------------------------------------------------ */

/* The ABI this library implements (RX_SESSION_API_VERSION). */
RX_API uint32_t rx_session_api_version(void);

/* Creates a session from the install root (the folder that holds MXReflex.exe and Database/, or the folder above Archive).
 * Does memory + image + startup (rx_globals_init) and the construction stand-ins build_event_test documents. NULL on failure;
 * rx_session_last_error(NULL) then says why. One event per session. */
RX_API struct rx_session* rx_session_create(const char* install_root);

/* Frees the session: the level, the install and the arena (rx_mem_shutdown). No other call may run concurrently. */
RX_API void rx_session_destroy(struct rx_session* s);

/* The ABI recorded in the handle, 0 for NULL. */
RX_API uint32_t rx_session_version(const struct rx_session* s);

/* ---- the event ------------------------------------------------------------------------------------------------------ */

/* The reference event (National NAT_Concord, 12 riders, AI 95): rx_event_spec_default. No session needed. */
RX_API void rx_session_event_default(rx_event_spec* spec);

/* The event's setup and start, exactly as build_event_test.c does it: copies the spec (rx_event_set_spec), opens the level
 * from the spec's location (fmt_level_open, rx_event_set_level), builds the front-end world state StartEvent reads, runs
 * EnsureLocalPlayers twice, SetupEvent and StartEvent. The session then advances through the load (MXGame states 3..0xb).
 * 0 on success, nonzero on failure (last_error). One event per session. */
/* Validated garage profile snapshot. Complete rows of the five supported Player tables, before event_load.
 * type 0: signed integer; type 1: NUL-terminated UTF-8 text. All buffers must terminate within capacity.
 * 65536 cells maximum, 4096 rows per table; no pointer or address from the original memory model crosses this API. */
typedef struct rx_profile_cell {
    char table[32], column[32], text[128];
    int32_t row, type, value;
} rx_profile_cell;
/* Values in order Engine1, Engine2, Suspension1, Suspension2, Tire1, Tire2, Braking, Steering.
 * Writes are profile bonuses; readback is the applied vehicle's effective values (base + bonuses for the first six). */
RX_API int rx_session_profile_import(struct rx_session* s, const rx_profile_cell* cells, int32_t count);
RX_API int rx_session_tuning_set(struct rx_session* s, const int32_t values[8]);
RX_API int rx_session_tuning_get(struct rx_session* s, int32_t values[8]);

/* Complete controller0 Player* snapshot, independent of garage compatibility import.
 * Output query: NULL/0 returns required bytes. Import only before event_load.
 * Types/layout in rx_player_snapshot_types.h; all calls serialize with advance.
 * 0 success,1 invalid session/native fault,2 rejected snapshot/capacity/state.
 * On native fault session becomes ERROR; destroy/recreate. No disk I/O. */
#include "rx_player_snapshot_types.h"
/* Explicit final leave, on the simulation worker after its final frame. Commits
 * eligible local-human working records through canonical00764560/007647f0.
 * 0=success/idempotent,2=state/validation rejection,1=fault (session ERROR).
 * Success prohibits further advance but permits full Player export and getters.
 * No standings/rewards, disk I/O or complete activity teardown is synthesized.
 * On any nonzero result do not replace the durable checkpoint. */
RX_API int rx_session_finalize_records(struct rx_session* s);
RX_API int rx_session_player_export(struct rx_session* s, void* output, uint32_t capacity, uint32_t* required);
RX_API int rx_session_player_import(struct rx_session* s, const void* input, uint32_t bytes);

/* Controller remapping uses native input IDs, not platform key codes. Column order:
 * Throttle, Brake, SteerLeft, SteerRight, TrickModifier, Clutch, ReflexLeft,
 * ReflexRight, ReflexUp, ReflexDown, Camera, SetResetPoint, Reset, UseResetPoint,
 * Reverse, CameraTweakLeft, CameraTweakRight, CameraTweakUp, CameraTweakDown.
 * Defaults is stateless and safe before session creation or concurrently with a race.
 * Session get/set run on the session owner thread. Set accepts keyboard[19] then
 * pad[19], validates the whole snapshot before writing, and requires pre-event state.
 * Return 0 success, 1 invalid session/output or fault, 2 invalid values/state. */
typedef struct rx_remap_table {
    int32_t keyboard[19], pad[19], scan[256];
} rx_remap_table;
RX_API int rx_remap_defaults(rx_remap_table* out);
RX_API int rx_session_remap_get(struct rx_session* s, rx_remap_table* out);
RX_API int rx_session_remap_set(struct rx_session* s, const int32_t values[38]);
/* CONTROLS IN A RACE (port addition, w-fb-hud; api/fbhud_session.inc): rx_session_remap_set before the event loads; once it is loaded the
 * table is written the same way and the local human's KeyMaps are built again from it (Controller_BuildKeyMaps 00547f30 with the UI
 * settings row's preset / handedness / motion, as the Player List does), so the bindings apply from the next frame. Between frames on
 * the session owner thread. 0 applied, 1 invalid session / values or fault, 2 invalid values / state (last_error). */
RX_API int rx_session_remap_apply(struct rx_session* s, const int32_t values[38]);

/* rx_session_event_load: 0 loaded; 1 bad session / spec / a fault (RX_SESSION_ERROR); 2 a career session; 3 refused (w-partylast): the
 * party modes EggHunt, Graffiti and Capture need scene entities no installed level holds (last_error names the missing one); nothing was
 * loaded and the session stays usable for another event. */
RX_API int rx_session_event_load(struct rx_session* s, const rx_event_spec* spec);

/* MAP SCALE (port addition, additive in version 2; core/plan/MAP_SCALE.md): the event's track loads scaled by `scale` about the world
 * origin (the terrain, the level's collision, every scene placement, the track definition, trees, water ...); vehicles and riders keep
 * their real size. Call after create and before rx_session_event_load, exhibition events only (a career session is always 1).
 * 0.25 .. 4; 1 is the game's track bit for bit. 0 ok, 1 invalid session, 2 wrong state / value (last_error).
 * rx_session_map_scale: the scale this session's track loaded with (1 when none was set). */
RX_API int rx_session_set_map_scale(struct rx_session* s, float scale);

/* BASE SCALE (port addition, additive in version 2, w-alivegates; core/plan/MAP_SCALE.md "Base scale"): the track's default scale, the
 * scale at which its event counts as unscaled: its offline records, best laps, highlights and the lap log (rx_lap_record.mods bit 16) are
 * kept when rx_session_set_map_scale's scale equals it, and not at any other. 1 (the default) for the game's tracks; the client gives the
 * MX vs ATV Alive race levels theirs (Nationals and short tracks 1.1, Supercross 1.3) and passes their scale as base x the player's MAP
 * SCALE. It does not scale anything itself. Call after create and before rx_session_event_load, exhibition events only; 0.25 .. 4.
 * 0 ok, 1 invalid session, 2 wrong state / value (last_error). rx_session_map_scale_base: the base (1 when none was set);
 * rx_session_map_scale_records: 1 when this session's scale keeps the records (it is its track's default), else 0. */
RX_API int rx_session_set_map_scale_base(struct rx_session* s, float base);
RX_API float rx_session_map_scale_base(const struct rx_session* s);
RX_API int32_t rx_session_map_scale_records(const struct rx_session* s);

/* MODS/TRACKS (port addition, additive in version 2): a custom track (Mods/Tracks/<name>/ of the client) bound to its level name for this
 * session only: `level` ("Beta_Track_Slot_1", the stem of the track's files) is read from `folder` (UTF-8) - its .dx9.level, .database,
 * .package and .scene, any case - instead of the install, which is never modified; two tracks shipped as the same slot coexist because
 * each session mounts only the one chosen. Call after create and before rx_session_event_load, with the spec's location = level;
 * exhibition events only. 0 ok, 1 invalid session, 2 wrong state / a file missing (last_error). */
RX_API int rx_session_mount_level(struct rx_session* s, const char* level, const char* folder);
RX_API float rx_session_map_scale(const struct rx_session* s);

/* FREER AIR (port addition, additive in version 2; game/free_air.h, core/notes/bike.md "FREER AIR"): leaves out the original's in-air
 * helpers that turn the local human players' bikes / ATVs back toward their take-off orientation (never the computer riders'):
 *   RX_FREE_AIR_NO_HOLD (1)    the trick hold: after a whip past the trick cone, pushing the steering stick past 0.9 the other way stops
 *                              stick control and slerps the bike back (0.012 per 7.5 ms step); without it the stick keeps control;
 *   RX_FREE_AIR_NO_SETTLE (2)  the auto-level: after 0.25 s of stick use, every airborne step without a stick slerps the bike back
 *                              toward level (0.008 per step); without it a released bike keeps its angle (and coasts, slowed by the
 *                              original's air damping).
 * mode = an OR of those (3 = both; 0 = the original bit for bit, the default). Any time on the session's thread (between frames): the
 * next airborne step uses it. 0 ok, 1 invalid session, 2 unknown bits (last_error). rx_session_free_air: the mode in use. */
#define RX_FREE_AIR_NO_HOLD   1
#define RX_FREE_AIR_NO_SETTLE 2
RX_API int rx_session_set_free_air(struct rx_session* s, int32_t mode);
RX_API int32_t rx_session_free_air(const struct rx_session* s);
/* AUTO LEVEL and COUNTER-STEER SNAP strengths (port addition, additive in version 2, w-airsliders): the two helpers above as 0 .. 1 each
 * instead of removed / kept. 1 = the original's helper exactly (its code path untouched: bit for bit the original), 0 = removed (the same
 * as RX_FREE_AIR_NO_SETTLE / RX_FREE_AIR_NO_HOLD), in between the helper still runs with its per-step slerp blend (0.008 auto-level,
 * 0.012 trick hold) scaled by the strength: the bike is turned back that much slower. Same players as rx_session_set_free_air (the local
 * humans, never the computer riders), any time between frames. rx_session_set_free_air(mode) sets each strength to 0 (its bit set) or 1
 * (clear); rx_session_free_air reports the bits of the strengths that are 0. 0 ok, 1 invalid session, 2 a value outside 0 .. 1 (last_error).
 * Defaults 1, 1. Independent of rx_session_set_free_air_looseness. */
RX_API int rx_session_set_air_helpers(struct rx_session* s, float auto_level, float snap);
RX_API float rx_session_air_auto_level(const struct rx_session* s);
RX_API float rx_session_air_snap(const struct rx_session* s);
/* AIR LOOSENESS (port addition, additive in version 2, w-freeair2): 0 .. 1 eases the local players' air model toward "looser"
 * (game/free_air.h): less air damping while the bike coasts after stick use (0.985 -> 0.9895 per step at 1), a quicker stick lag
 * (0.1 s -> 0.08 s), +10 % stick authority; the take-off damping, the 10 rad/s cap, the ground and the computer riders are untouched. 0 (default) = off: the air
 * model of rx_session_set_free_air's mode bit for bit. Independent of the mode in the core (the OPEN REFLEX options send it only with
 * FREER AIR on). Any time between frames. 0 ok, 1 invalid session, 2 outside 0 .. 1. */
RX_API int rx_session_set_free_air_looseness(struct rx_session* s, float looseness);
RX_API float rx_session_free_air_looseness(const struct rx_session* s);
/* FREE RIDE WITHOUT BORDERS (port addition, additive in version 2, w-frnowalls; game/free_ride_borders.h): in an event world's Free Ride
 * (never the races, the career events, the challenges, the party modes or the front end's UI_Compound world: MotoSkills, the test ride)
 * a human rider is no longer reset for where he rides:
 *   RX_FREE_RIDE_BORDERS_OPEN (1)      no reset zone (the race tracks' out-of-bounds terrain paint; a drowned vehicle still), no "ResetPlayer" trigger volume but
 *                                      the deep-water ones, and the terrain-edge check (30 ft inside the heightfield's edge) only as a fall
 *                                      net: past the real edge and 4 s in the air (nothing to land on) the original's reset brings him back;
 *   RX_FREE_RIDE_BORDERS_ORIGINAL (0)  the original bit for bit (the core's default; the OPEN REFLEX options default to OPEN).
 * The reset key, the save spot, the crash recovery, the water's drowning and every collision stay the original's; computer riders keep
 * the original's resets. Any time between frames. 0 ok, 1 invalid session, 2 unknown mode. rx_session_free_ride_borders: the mode. */
#define RX_FREE_RIDE_BORDERS_ORIGINAL 0
#define RX_FREE_RIDE_BORDERS_OPEN     1
RX_API int rx_session_set_free_ride_borders(struct rx_session* s, int32_t mode);
RX_API int32_t rx_session_free_ride_borders(const struct rx_session* s);

/* WEATHER GRIP (port addition, additive in version 2, 2026-11-07; game/weather_grip.h): the client's rain / snow take grip off the tyres of
 * every vehicle of the event (MX, ATV, UTV, buggy, trucks; players and computer riders): each tyre's static rolling and cornering grip
 * (StaticMuRoll / StaticMuSideSlip at the ground value under it) times `factor`, so its friction limits shrink by it. 1 (the default, every
 * session's start) leaves the original bit for bit. Any time between frames; RX_WEATHER_GRIP_MIN .. MAX (0.1 .. 1.5). 0 ok, 1 invalid
 * session, 2 refused (NaN / out of range: the factor stays). rx_session_weather_grip: the factor. */
RX_API int rx_session_set_weather_grip(struct rx_session* s, float factor);
RX_API float rx_session_weather_grip(const struct rx_session* s);

/* PRACTICE (port addition, additive in version 2, w-lb-practice; game/practice.h, core/plan/LEADERBOARDS.md 2.1): the event is an endless lap
 * race (the original's Practice rule, build_activity_start.c: NumLaps = 0x7fffffff at the activity start). Call after create, before
 * rx_session_event_load; exhibition lap races only (ActivityMode 1 of a National, Supercross, Terracross, UTV Moto or Powercross: any other
 * event loads as the original). The lap counter (laps_completed) stops at 97; the race never finishes. Off (the default) is the original bit
 * for bit. 0 ok, 1 invalid session, 2 wrong state (last_error). rx_session_practice: the flag. */
RX_API int rx_session_set_practice(struct rx_session* s, int32_t on);
RX_API int32_t rx_session_practice(const struct rx_session* s);

/* LAP LOG (port addition, additive in version 2, w-lb-practice; game/practice.h): every line crossing of the local human that the race
 * counted (race_driver.c lap_completed), in any exhibition lap race (practice or not). A ring of 64 per session; read with
 * the last serial seen (0: from the oldest kept), oldest first. Only host statics are written: the game runs the same with or without it. */
typedef struct rx_lap_record {
    uint32_t serial;      /* 1, 2, ... per session (the gate lap is 1) */
    int32_t  flying;      /* 1: the lap began crossing the line (D+0x10f0 was 0); 0: the first lap, from the gate */
    float    time;        /* D+0xf20 at the crossing: the raw lap time (s), a sum of whole frames */
    float    exact;       /* the time from the sub-frame crossing to the sub-frame crossing (s): time - over + the previous crossing's over,
                             over = (1 - f) * the frame's dt, f where the frame's move met the line (game/practice.h) */
    float    penalty;     /* the off-track rule's penalty (+0xd0) at the crossing (s) */
    float    off_track;   /* s of the lap with rx_hud_state.on_track == 0 */
    float    race_time;   /* D+0x134 */
    uint32_t resets;      /* resets of the local player in the lap: rising edges of P+0x116c > 0 (crash, manual, out of bounds, off-track) */
    int32_t  wrong_way;   /* the wrong-way indicator showed in the lap */
    uint32_t mods;        /* OR over the lap's frames: 1 / 2 / 4 the rx_session_phys_active bits, 8 air helpers not original (auto level != 1,
                             snap != 1, looseness != 0, rx_session_free_air bits), 16 map scale != 1 */
    uint32_t markers;     /* (additive, w-lb-markers) rides back to the SPAWN MARKER in the lap (PRACTICE): the lap is invalid, and the timing
                             restarts from the finish line (its crossing ends this lap; the next lap is a flying one from the line) */
} rx_lap_record;
/* the records with serial > after_serial, at most max, into out; the count, -1 on an error (an invalid session, max < 0, out NULL) */
RX_API int32_t rx_session_lap_log(struct rx_session* s, uint32_t after_serial, rx_lap_record* out, int32_t max);

/* The local human's vehicle physics class (port addition, w-lb-practice): the parameter file the core loaded for it, RX_PS_* of
 * game/phys_studio.h (0 mx125, 1 mx250, 2 mx250f, 3 mx450f, 4 atv, 5 utv, 6 buggy, 7 truck2wd, 8 truck2wdlite; 9 Alive 50cc, 10 Alive 650, 11 Alive ATV1000:
 * rx_session_vehicle_physics); -1 unknown (no vehicle). */
RX_API int32_t rx_session_local_phys_class(const struct rx_session* s);

/* SPAWN MARKER (port addition, w-spawnmarker; core/api/spawn_marker_session.inc): the OPEN REFLEX options' SPAWN MARKERS, after Open Unleashed's.
 * rx_session_spawn_at sends the local human's vehicle through the game's own reset (the reset key's path: Player_ResetTo 005501f0, its fade,
 * Player_Reset 0054fff0 at the end of the timer) to pos (the vehicle's frame body position, as rx_bike_state.position[0]) heading dir
 * (horizontal; y ignored), upright, with its velocity gone. Only where the reset key itself works in a human's Free Ride of an event world
 * (never a race, a challenge, MotoSkills, the test ride), and not while a reset is already running. Also in PRACTICE (w-lb-markers), any
 * time while the race runs: the lap in progress is then invalid (rx_lap_record.markers) and the timing restarts from the finish line.
 * Any time between frames, on the session's thread. 0 ok, 1 invalid session / error state, 2 not in a Free Ride or PRACTICE / a bad pose
 * (last_error), 3 not now (reset running / paused / not racing).
 * rx_session_spawn_pose reads the pose to keep as a marker: the local vehicle's frame body position and its heading (horizontal unit vector),
 * the same convention rx_session_spawn_at takes; 0 ok, 2 no vehicle. */
RX_API int rx_session_spawn_at(struct rx_session* s, const float pos[3], const float dir[3]);
/* rx_session_spawn_at_instant (PORT ADDITION, w-markerbtns): the same respawn without the reset key's wait: the same gate and answers, the
 * same steps of the game's reset (Player_ResetTo's spot, fade hold, timer and reason, then Player_Reset at once, as Player_ResetTo does for
 * a player without a viewport), so the rider is at the marker, upright, standing and in control when the call returns (the game's ghost time
 * after a reset still runs). No white viewport fade and no PlayerReset\FadeOutTime (0.5 s) hold. The RESET key, the track resets and
 * rx_session_spawn_at keep the original timing. */
RX_API int rx_session_spawn_at_instant(struct rx_session* s, const float pos[3], const float dir[3]);
/* rx_session_rtbump_fit (w-rtbumpfit): the game's own RT_Bump cascade layout (RTBump_FitCascadeMatrices 007ba8d0 with 007c1010's camera
 * preparation) for a camera (position, forward, up, vertical fov in radians, aspect, near distance) on the active terrain. out: near, far,
 * the near edge's left / right x, the far edge's left / right x (the camera's horizontal frame: x along the horizontal right, z along the
 * horizontal forward, from the position), the horizontal forward (x, z), the horizontal right (x, z), the position (x, z). Between frames
 * only. 0 ok, 1 bad arguments, 2 no terrain. */
RX_API int rx_session_rtbump_fit(struct rx_session* s, const float pos[3], const float fwd[3], const float up[3], float fov, float aspect,
                                 float near_dist, float out[12]);
RX_API int rx_session_spawn_pose(struct rx_session* s, float pos[3], float dir[3]);

/* ---- frame loop ----------------------------------------------------------------------------------------------------- */

/* One frame of the game's own loop: Engine::Update (0084ed20): Engine::UpdateTiming (the game's clock, frame cap and dt smoothing),
 * the input manager's device poll (keyboard, pads, mouse sums: set with rx_session_set_input / _set_mouse), the key maps, then
 * MXGame::Update (the load states 3..0xb while loading, a race frame once the world is up). No real time is given: the frame
 * starts when the game's frame cap says (the original busy-waits on its performance counter; here that counter is the session's
 * virtual clock, which a busy wait moves on by 10 us per read), so the game's dt is its cap (1/60 s). 0 while usable; nonzero when
 * the session is in the error state / the handle is invalid. Check rx_session_get_state. */
RX_API int rx_session_advance(struct rx_session* s);

/* The same frame, after `dt` seconds of real time passed since the previous frame (the host's elapsed time; the original reads
 * QueryPerformanceCounter). The session's performance counter (10 MHz) moves on by dt; UpdateTiming measures it, waits for the cap
 * when dt is shorter (the busy wait costs no host time), smooths over the last five frames and clamps at its DeltaSecondsClamp
 * (0.1 s): all the game's own code. dt <= 0 or NaN = no time passed (see rx_session_advance); dt is limited to one hour. */
RX_API int rx_session_advance_dt(struct rx_session* s, float dt);

/* ---- the frame clock for a host that runs the simulation at the game's own step ---------------------------------------------
 * The game runs one frame per frame cap (Engine +0x14 = 1/60 s, read from the game: rx_session_step_dt). A host with a different
 * display rate accumulates real time and runs whole steps:
 *     n = rx_session_pace(s, real_dt, max_steps);          // adds real_dt, returns the whole steps due (at most max_steps)
 *     for (i = 0; i < n; i++) rx_session_advance_dt(s, rx_session_step_dt(s));
 *     alpha = rx_session_alpha(s);                         // [0, 1): how far the display time is into the next step
 * The game exposes no inter-frame interpolation weight of its own: every pose rx_session_bike / _rider_pose / _bike_pose returns is
 * already interpolated inside the frame (the vehicle's 133 Hz fixed-step scheduler, +0xc0 lead), so a host blends the last two
 * frames' poses with alpha (or shows the last frame). */
RX_API float rx_session_step_dt(const struct rx_session* s);   /* the frame cap in seconds; 0 = uncapped (FLT_MAX in the game) */
RX_API uint32_t rx_session_pace(struct rx_session* s, float real_dt, uint32_t max_steps);   /* real_dt <= 0 / NaN adds nothing; a backlog beyond max_steps is dropped */
RX_API float rx_session_alpha(const struct rx_session* s);
/* The game's own dt of the last frame (Engine +0x18: after the smoothing and the clamp). rx_session_frame_dt is the dt the host passed. */
RX_API float rx_session_engine_dt(const struct rx_session* s);

typedef enum rx_session_state {
    RX_SESSION_LOADING = 0,    /* created / event setup / MXGame states 3..0xb / the activity not up yet */
    RX_SESSION_COUNTDOWN = 1,  /* activity Pre or Countdown (A+0x21c 0 / 1), the start board running */
    RX_SESSION_RACING = 2,     /* activity InProgress (A+0x21c 2) */
    RX_SESSION_FINISHED = 3,   /* activity WindDown / Post / Inactive (A+0x21c 3 / 4 / 5), or the race driver finished */
    RX_SESSION_ERROR = 4       /* setup failed; last_error says why (an unported function aborts, it cannot return) */
} rx_session_state;

RX_API rx_session_state rx_session_get_state(const struct rx_session* s);
RX_API const char* rx_session_last_error(const struct rx_session* s);

/* Presentation timeline: frames advanced, the last dt passed to advance_dt (the real time the host reported), and the game's Engine
 * clock after the frame (Engine 0xd728d0 +0x3c time: the sum of the game's own dt values; +0x18 dt: rx_session_engine_dt). */
RX_API uint64_t rx_session_frames(const struct rx_session* s);
RX_API float rx_session_frame_dt(const struct rx_session* s);
RX_API float rx_session_game_time(const struct rx_session* s);

/* ---- first-ride state (layouts match the old plugin / C# readers) --------------------------------------------------- */

/* Four body poses: 0 frame, 1 rider, 2 front wheel, 3 rear wheel. Each row is one body's vec4,
 * not a row of a transform matrix. Positions use native world units (+Y up) with a fourth padding component;
 * orientations are quaternions (x, y, z, w). Same layout as ReflexBikeState (320 bytes). */
typedef struct rx_bike_state {
    float position[4][4];             /* S+0xf0+0xa0*i */
    float orientation[4][4];          /* S+0x730+0xa0*i */
    float render_position[4][4];      /* B+0x90  (the scheduler-interpolated pose) */
    float render_orientation[4][4];   /* B+0x6d0 */
    float velocity[4];                /* body 0, S+0x100 */
    float speed;                      /* S+0x94 */
    int32_t gear;                     /* S+0x8ac0 (MX) / S+0x96f0 (ATV) */
    float inputs[8];                  /* S+0x1c: throttle, reverse, front brake, rear brake, steer x/y, lean x/y */
    uint32_t frames;                  /* session frames */
    uint32_t reserved;
} rx_bike_state;

/* The local player's vehicle state. 0 ok, 1 no local player / vehicle / simulation. */
RX_API int rx_session_bike(struct rx_session* s, rx_bike_state* out);

/* Read-only state for the presentation camera. No physics inputs or original code execution.
 * valid/rider_active: a live local RiderAnimation; wrecked includes pending wreck and fallen-off state.
 * contacts_valid only for known MX/ATV simulation layouts; front/rear mean at least one wheel on that axle.
 * animation_mode and stunt_state are raw Reflex animation fields, not Unleashed enum values. */
typedef struct rx_onboard_state {
    int32_t valid, rider_active, wrecked, trick_active, contacts_valid, front_grounded, rear_grounded;
    int32_t animation_mode, stunt_state;
} rx_onboard_state;
RX_API int rx_session_onboard_state(struct rx_session* s, rx_onboard_state* out);
/* Read-only local-controller bindings, captured by clients once after event startup.
 * keyboard_scan is DIK (0 unbound), pad_input is the game's input ID (0 unbound),
 * driving_camera is the actual selected preset0..3 (-1 if not a driving entity). */
typedef struct rx_camera_bindings {
    int32_t valid, keyboard_scan, pad_input, right_stick_rides, driving_camera, look_back_pad_input;
} rx_camera_bindings;
RX_API int rx_session_camera_bindings(struct rx_session* s, rx_camera_bindings* out);


/* The local rider's 69 bone world matrices (MXRider_Hierarchy; 16 floats each, row vectors). The game's lazy node evaluator
 * (rx_world_node_evaluate_world, 007db890) completes them first, as the renderer does. Writes min(69, max_bones); returns the
 * count, 0 when there is no rider. */
RX_API int rx_session_rider_pose(struct rx_session* s, float* out, uint32_t max_bones);

/* The local vehicle model's node world matrices (18 for MX, 50 for an ATV), completed the same way. Returns the count. */
RX_API int rx_session_bike_pose(struct rx_session* s, float* out, uint32_t max_nodes);

/* The same two poses for any player slot (rx_session_slots lists them): the other riders' bike model nodes and rider bones.
 * Returns the count, 0 when the slot has no vehicle / rider. */
RX_API int rx_session_rider_pose_slot(struct rx_session* s, int32_t slot, float* out, uint32_t max_bones);
RX_API int rx_session_bike_pose_slot(struct rx_session* s, int32_t slot, float* out, uint32_t max_nodes);

/* The game's render camera ([0xa7b1d8], or the static Camera 0xd6f620), native axes, vertical fov radians. */
typedef struct rx_camera {
    float position[3], right[3], up[3], forward[3];
    float fov, near_plane, far_plane;
} rx_camera;
RX_API int rx_session_camera(struct rx_session* s, rx_camera* out);

/* Selects the local player's view through the game's own camera setter (Player_SetCamera 0054c9b0), the function the shipped
 * Camera key drives (keyboard C, DIK 0x2e: the "Camera" remap column -> key id 0x25 -> the compound KeyMap at input block +0xf34,
 * Player_UpdateCamera 0054c150 -> Player_SetCamera; that key also reaches it through rx_session_set_input while the race's
 * camera-key group is enabled). The API modes map to the game's own view entities: 0 = the default third-person GamePlay camera
 * (entity 1, the shipped [0x9c5410] value and the session's default; DrivingCamera 1), 2 = the first-person camera (entity 2,
 * DrivingCamera 3). The other entities (0 Visual, 4 User) stay reachable only through the key's cycle, so no view is invented.
 * The call does exactly what the key does: the activity-state 1..3 / human-player-with-vehicle validation, P+0x988, the old view
 * entity's blend offsets, the replay camera pass, the per-controller index [0x9c5410 + 4 * controller], the DrivingCamera
 * setting write-back + UISettings_Apply and the "Camera Changed" event. The default view is untouched unless this is called.
 * 0 ok; 1 invalid session / error state; 2 no local player, or the game's setter refused (not a human player with a vehicle in an
 * activity in state 1..3). A session that never calls it keeps the game's default camera. */
RX_API int rx_session_set_camera(struct rx_session* s, int32_t mode);
/* All four original DrivingCamera presets: 0 Visual, 1 GamePlay, 2 User, 3 FirstPerson. Same validated game setter. */
RX_API int rx_session_set_driving_camera(struct rx_session* s, int32_t preset);

/* Race progress of the local player (the pointer chains of _re/notes/race-state.md). state: 0 Pre, 1 Countdown,
 * 2 InProgress, 3 WindDown, 4 Post, 5 Inactive (A+0x21c); valid = 1 when the local player's driver was found.
 * start_timer: the start board's hold timer ([A+0x2cc]+0x4f4: the gate drops once it is below 0), 0 without a board.
 * countdown_step (w-raceflow): the activity's own start countdown, in the activities whose Countdown has no start board (A+0x2cc == 0:
 * Waypoint, Terracross, Powercross, Freestyle, Section Race, the challenges ...; National and Supercross wait for the board instead).
 * Activity::vf26 00559190 counts A+0x2a0 down from A+0x2b8 (3), one "Countdown Step" (count) each time the clock A+0x29c falls to
 * count * A+0x2b0 / A+0x2b8, and starts the race below 0. The value is the number the original's MXUIBeginningCountdownComponentStub
 * shows: "Countdown Entered" (005fb830) shows 3, each "Countdown Step" (005fb880) its count (0 = STR_GO, the moment InProgress starts),
 * so 3 before the first step, else A+0x2a0 + 1 (in an Overtake, A+0x238 6, 005fb830 hides the component: its own countdown runs at
 * InProgress). -1 outside Countdown or with a start board. */
typedef struct rx_race_state {
    int32_t valid, state, num_laps, laps_completed, place, riders;
    float lap_time, last_lap, best_lap, race_time, lap_progress, start_timer, reset_timer;
    int32_t countdown_step;
} rx_race_state;
RX_API int rx_session_race(struct rx_session* s, rx_race_state* out);

/* Read-only local activity lifecycle. A field is authoritative only when its known bit is set;
 * unknown signed fields are -1. No pause policy or game state is changed. No native page owner
 * is established by the core route, so ALLOW_PAUSE/PAUSE_OWNER remain unknown (including if
 * the UI manager's raw default happens to be nonzero). Call between advances, on the session thread.
 * Returns 0 for a valid session (including absent/unsupported activity: known=0), 1 for bad args. */
enum rx_event_ui_known {
    RX_EVENT_UI_ACTIVITY_TYPE = 1u << 0,
    RX_EVENT_UI_RACE_STATE = 1u << 1,
    RX_EVENT_UI_RULES_HOLD = 1u << 2,
    RX_EVENT_UI_POST_STATE = 1u << 3,
    RX_EVENT_UI_SKIP_REQUESTED = 1u << 4,
    RX_EVENT_UI_ALLOW_PAUSE = 1u << 5,
    RX_EVENT_UI_PAUSE_OWNER = 1u << 6
};
typedef struct rx_event_ui_state {
    uint32_t known;
    int32_t activity_type;      /* original activity enum A+23c, NOT activity mode A+238 */
    int32_t race_state;         /* 0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive */
    int32_t rules_hold;         /* raw original rules-popup hold word */
    int32_t post_state;         /* exact MXActivityRace while Post only: 0 before, 1 scenario, 2 results, 3 results scenario */
    int32_t skip_requested;     /* raw global scenario skip request; not permission to skip */
    int32_t native_allow_pause; /* -1: no authoritative native page policy */
    int32_t native_pause_owner; /* -1 unknown; reserved 0 client/1 native when ownership is established */
} rx_event_ui_state;            /* 32 bytes, all fields 4 bytes */
RX_API int rx_session_event_ui_state(struct rx_session* s, rx_event_ui_state* out);


/* The original in-race HUD's other values (the old plugin's ReflexHudState; _re/notes/ui.md): speed (V+0x150, ft/s; the
 * speedometer shows int(x * 0.682) mph), the activity state the HUD branches on (A+0x238; 5 hides the lap counter), time behind
 * the leader / ahead (D+0x10b4 / D+0x10b8), the lap-time hold (D+0x130 clock, D+0x10dc until), RuleOfftrackTimePenalty
 * ([D+0xac]: +0x10 on track, +0x20 penalty) and RuleBackwards ([D+0xa8]+0xc: riding the wrong way). valid = 1 when the local
 * player's driver was found. 0 ok, 1 invalid handle / no output. */
typedef struct rx_hud_state {
    int32_t valid, activity_state, on_track, wrong_way;
    float speed, time_behind, time_ahead, hold_clock, hold_until, penalty;
} rx_hud_state;
RX_API int rx_session_hud(struct rx_session* s, rx_hud_state* out);

/* The race line ahead of the local player: node centres (x, y, z; the node's own matrix translation, 0x500-byte nodes) from the
 * driver's current race-line node (D+0x1c) following the nodes' next links (+0x64, stopping when the chain returns to the first).
 * Returns the number of points written (at most max_points; 0 outside a race), -1 on an invalid handle / no output. */
RX_API int rx_session_race_line(struct rx_session* s, float* out, uint32_t max_points);
/* Same slice, plus whether its last node actually links back to its first (never infer a closing chord for a truncated slice). */
RX_API int rx_session_race_line_ex(struct rx_session* s, float* out, uint32_t max_points, int32_t* closed);

/* One standing: player slot, local flag, place (1-based), laps completed, finished, and the driver's clocks (seconds; race
 * time stops at the driver's finish). Returns the count written, 0 outside a race. Written in the activity's standings order
 * (+0x2d0: the original's results page numbers its rows by it, Index + 1) when the standings hold every driver, else the driver list's.
 * The place is the driver's +0x78 (the HUD's position); after InProgress the original's places can repeat (00565570), so a results
 * page numbers riders by this order, not by the place. */
typedef struct rx_driver_result {
    int32_t slot, local, place, laps_completed, finished;
    float race_time, best_lap, last_lap;
} rx_driver_result;
RX_API int rx_session_standings(struct rx_session* s, rx_driver_result* out, uint32_t max);

/* Every player slot with a vehicle: slot, type (P+0x10: 0 human, 1 computer), local, rider/vehicle table rows, addresses. */
typedef struct rx_slot {
    int32_t slot, type, local, rider_row, vehicle_row;
    uint32_t vehicle, rider;
} rx_slot;
RX_API int rx_session_slots(struct rx_session* s, rx_slot* out, uint32_t max);

/* The actual equipped construction records for a live slot. IDs resolve through the install's asset tables;
 * no profile defaults or emulated world are needed to build the matching visual models. plate_name / plate_number are the
 * number / name plates' text as 0053f3f0 writes it (P+0x200 / P+0x210: the profile's PlayerRider Name / Number for the player,
 * the AIRider row's for an AI); name (the display name) is left empty. 0 success, 1 invalid args, 2 not constructed yet. */
typedef struct rx_appearance {
    int32_t rider_owner, owner_rider_id, gear_set_id, vehicle_owner, owner_vehicle_id, vehicle_model_id;
    int32_t part_count, gear_count;
    int32_t part_ids[17], gear_ids[16];
    char name[64], plate_name[16], plate_number[16];
    int32_t bike_dark, on_jersey, invert;
} rx_appearance;
RX_API int rx_session_appearance(struct rx_session* s, int32_t slot, rx_appearance* out);


/* A race slot's vehicle (the old plugin's ReflexVehicleInfo; _re/notes/atv.md): its factory class and, for an ATV / truck /
 * buggy / UTV, every body of its simulation. The local bike's bodies are also in rx_bike_state; this carries them for any slot
 * and the values the renderers read off V (speed V+0x150, gear / rpm of the simulation). slot -1 = the local player. 0 ok,
 * 1 no local player / no vehicle / invalid handle. type: factory index 0 MX, 1 ATV, 3 TrophyTruck, 5 Buggy, 7 UTV, -1 other. */
typedef struct rx_vehicle_info {
    int32_t slot, type, bodies, model_nodes;
    uint32_t vehicle, sim;                 /* V = [P+0x1ac], S = [V+0x234] */
    int32_t gear;                          /* as the HUD shows it (S+0x8ac0 + 1 for MX) */
    float rpm, speed;                      /* S+0x8abc * 9.55, V+0x150 (ft/s) */
    float render_position[24];             /* 6 bodies x (x, y, z, w): B+0x90 */
    float render_orientation[24];          /* 6 bodies x quaternion: B+0x6d0 */
} rx_vehicle_info;
RX_API int rx_session_vehicle_info(struct rx_session* s, int32_t slot, rx_vehicle_info* out);


/* ---- terrain -------------------------------------------------------------------------------------------------------- */

/* The selected HFTerrainSurface (the collision terrain of the active view, list 0x009e30d4). Grid: samples x samples,
 * column c at x = min_x + c * spacing, row r at z = max_z - r * spacing. dirty_* is the rectangle [x0,x1) x [z0,z1)
 * changed since the last call made in an earlier game frame (repeated calls in the same frame return the same rectangle and
 * version, so the getter is pure within a frame); deform_version changes when the deformation surface changes (the API
 * hashes a coarse sample set: the core does not track deformation dirtiness yet, so a change may be reported late and the
 * dirty rectangle is the whole grid). */
typedef struct rx_terrain_info {
    uint32_t samples, stride;
    float spacing, min_x, min_z, max_x, max_z, gain;
    uint32_t deform_version;
    int32_t dirty_x0, dirty_z0, dirty_x1, dirty_z1;
} rx_terrain_info;
RX_API int rx_session_terrain(struct rx_session* s, rx_terrain_info* out);

/* The changed texels of the same frame as rectangles of whole 32 x 32 tiles (terrtiles; additive, version 2): the tiles that hold a texel
 * that differs from the previous frame's copy, runs of tiles joined into rectangles, then merged down to at most
 * RX_TERRAIN_MAX_RECTS (the pair whose bounding box adds the least area first). A rectangle is [x0,x1) x [z0,z1) in grid columns / rows,
 * every edge a multiple of RX_TERRAIN_TILE or the grid size; the rectangles do not overlap, every changed texel lies in one, and their
 * union lies inside the tile-aligned box around rx_terrain_info's dirty rectangle (with 16 or fewer changed-tile runs it holds no tile
 * that did not change). Calls rx_session_terrain itself (a repeated call in a frame returns the same rectangles), so it may be called
 * alone. The first call after a terrain (re)load, and any resync, returns the whole grid as one rectangle; no change returns 0.
 * Returns the number of rectangles written (at most `max`; with max smaller than the count the rectangles are merged further to fit);
 * out == NULL or max <= 0 only counts. -1 invalid session, -2 no terrain. Use rx_terrain_info.deform_version to see if anything changed. */
#define RX_TERRAIN_TILE      32
#define RX_TERRAIN_MAX_RECTS 16
typedef struct rx_terrain_rect {
    int32_t x0, z0, x1, z1;
} rx_terrain_rect;
RX_API int rx_session_terrain_tiles(struct rx_session* s, rx_terrain_rect* out, int32_t max);

/* Collision heights base*2048 + (deformation - 0.5) * 2 * gain for columns [x0,x0+w), rows [z0,z0+h); out is h rows of w
 * floats, row 0 = max z. The deformation is 0.5 (untouched) when the surface has no CPU-visible grid. */
RX_API int rx_session_terrain_heights(struct rx_session* s, float* out, int32_t x0, int32_t z0, int32_t w, int32_t h);

/* The deformation grid (0.5 = untouched, ruts below, berms above) for the same rectangle. */
RX_API int rx_session_terrain_deform(struct rx_session* s, float* out, int32_t x0, int32_t z0, int32_t w, int32_t h);

/* RESET TRACK (port addition, w-fb-hud; api/fbhud_session.inc): arms the game's own level reset of the event terrain (Activity::vf18's
 * +0x44b8 = 1): the next frame clears the deformation (ruts, berms) to the untouched surface and resets the tyre tracks; the riders,
 * the activity and its clock are untouched. The next rx_session_terrain / _tiles reports the whole grid. Between frames. 0 armed,
 * 1 invalid session, 2 no deformable terrain. */
RX_API int rx_session_terrain_reset(struct rx_session* s);

/* ---- fx / ambient / mud counts -------------------------------------------------------------------------------------- */

/* The live particles after the last frame, as the original renderer draws them. Same layout as ReflexParticleInfo; the live
 * counts are managers 0xc11bd0 (normal) / 0xc11d80 (additive) at +0xac. */
typedef struct rx_particle_info {
    char texture[64];
    float luminance[4], luminance_delta[4];
    float fade[4];
    int32_t soft, normal, additive, live;
    float near_plane, far_plane, reserved[2];
} rx_particle_info;
RX_API int rx_session_particles(struct rx_session* s, rx_particle_info* out);

/* One live particle as the original renderer instances it (the old plugin's ReflexParticle, 64 bytes). Camera-facing quad:
 * corner c in (-1,1) (1,1) (1,-1) (-1,-1) with atlas uv (0,0) (1,0) (1,1) (0,1) (D3D v down) -> uv * frame.xy + frame.zw;
 * X = size.x * scale * (c.x - pivot.x), Y = size.y * scale * (c.y - pivot.y); rotated by `rotation` in the view plane; colour =
 * color * color1 (D3DCOLOR ARGB) * (luminance + shadow * luminance_delta); alpha * sat(fade.x * area + fade.y), killed below
 * fade.z; flags bit 0 = the additive manager (SRCALPHA / ONE, else SRCALPHA / INVSRCALPHA). */
typedef struct rx_particle {
    float position[3];       /* world, native axes */
    float size[2];           /* quad half extents (before scale) */
    float pivot[2];          /* quad pivot in corner units */
    float scale, rotation;   /* rotation: radians, in the view plane */
    uint32_t color, color1;  /* D3DCOLOR (A8R8G8B8), multiplied */
    float uv[4];             /* atlas frame: scale u, v, offset u, v */
    uint32_t flags;          /* bit 0: additive */
} rx_particle;

/* The live particles of the last frame, as the original renderer draws them: the normal manager first, then the additive one,
 * each far to near (10000 view-depth buckets, at most 3000 per manager; the frame's atlas uv from the particle asset). Writes
 * at most max into out and fills *info (same data as rx_session_particles). Returns the count written, -1 on an invalid handle /
 * no output, -2 when out is NULL (info only) - use rx_session_particles for that. */
RX_API int rx_session_particles_array(struct rx_session* s, rx_particle* out, uint32_t max, rx_particle_info* info);


/* Ambient animated entities (crew chiefs, pit crews, flaggers, score-tower people, the instanced grandstand crowd, billboard
 * crowds, animals): the same classes and class-table scan the old plugin uses (native/drafts/world_runtime/ambient.cpp; the 16
 * vtable pairs of _re/names/76_ambient.tsv), plus the start board (kind 16, ThirtySecondBoard 0095f41c). `entities` = entries in rx_session_ambient_entities, `matrices` = the node matrices
 * it has, `objects` = every object of those classes the scan found; fade_start / lod_divisor are the renderer's own constants
 * (0x009c4b4c, [[0x00a95844 + 0x1c + 4 * [0x00a95844 + 4]] + 0x19d0]). The scan runs once the world exists (state past
 * Loading: before that there is nothing to export and the call does not cache) and is cached; rx_session_ambient_rescan repeats
 * it after new objects are created. */
typedef struct rx_ambient_info {
    int32_t entities, matrices, objects, reserved0;
    float fade_start, lod_divisor, reserved[2];
} rx_ambient_info;
RX_API int rx_session_ambient(struct rx_session* s, rx_ambient_info* out);
RX_API int rx_session_ambient_rescan(struct rx_session* s);

/* One ambient animated entity: the old plugin's ReflexAmbientEntity (304 bytes, the layout OriginalAmbient.cs parses: object 0,
 * entity id 4, kind 8, flags 12, bones 16, first matrix 20, source 24, lod 28, params 32, matrix 48, model 112, texture 176,
 * texture package 240). Fields are the game's live values (see rx_session_ambient_entities). */
typedef struct rx_ambient_entity {
    uint32_t object_id;    /* the heap object's game address: an IDENTITY only (the old .object), not a host pointer; it changes on a rescan */
    int32_t entity_id;     /* scene entity id (the object's +0x14) */
    int32_t kind;          /* class index 0..15 in the old KindNames order (SingleAnimLoop, Scenario, SplineAnimLoop, CrewChief, Instanced, ...),
                            * 16 ThirtySecondBoard (the start board girl; additive, 2026-11-08: older hosts skip an unknown kind's name only) */
    uint32_t flags;        /* 1 visible (+0xa2), 2 skinned, 4 instance, 8 template, 16 animated (billboard), 32 completed; as the old plugin */
    int32_t bones;         /* skeleton nodes: matrices [first_matrix, first_matrix + bones) (0: no skeleton) */
    int32_t first_matrix;  /* index into the matrices array of the same call */
    int32_t source;        /* instance: the index of its template entry in this export, else -1 */
    float lod_distance;    /* ModelAsset draw distance (+0x120) / lod_divisor; 0 = no distance cull */
    float param[4];        /* billboard crowd: phase (+0x140), empty chair ratio, is-non-animated (+0x144), active flashes; instance: [0] LOD index */
    float matrix[16];      /* the entity's world matrix (row vectors): the skeleton's entity node, else the entity's +0x30 */
    char model[64];        /* ModelAsset name (handle +4) */
    char texture[64];      /* crew chief: the shirt texture the game set (FUN_006bd760) */
    char texture_package[64];
} rx_ambient_entity;

/* Export the live ambient entities and their node world matrices, as the old plugin's reflex_world_ambient did. `out` may be
 * NULL (info only); `matrices` may be NULL (no matrices, but info->matrices still counts them). Writes at most max entities and
 * max_matrices matrices (16 floats each, row vectors, the skeleton's nodes in node order, the entity node last in the skeleton
 * but NOT part of the matrices array: it is `matrix`). Returns the number of entities written (also info->entities) and fills
 * info; -1 on an invalid handle; 0 with entities 0 before the world exists. Nodes the animation left invalid are completed
 * with the game's own node evaluator (rx_world_node_evaluate_world, 007db890), as the renderer does. The scan happens once
 * (rx_session_ambient_rescan repeats it). */
RX_API int rx_session_ambient_entities(struct rx_session* s, rx_ambient_entity* out, uint32_t max,
                                       float* matrices, uint32_t max_matrices, rx_ambient_info* info);

/* One live scene entity (a prop, gate, bale, board, model, camera, ...): the event scene's objects, in the scene's own lists
 * (unsorted + the four sorted lists). type = the object's class vtable (the old find_object's vtable: the start gate's DropGate
 * is 0x0095fc24; the scene RSData type id is not kept on the object), id = the scene entity id (+0x14), matrix = the entity
 * world matrix (+0x30, row vectors), name = the entity's Name (+0x18), model = its model instance's ModelAsset name (+0xa0/+0x60)
 * when it has one. */
typedef struct rx_scene_object {
    uint32_t vtable;
    int32_t entity_id;
    int32_t flags;          /* +0x1c (the scene name flag) and +0x7c (UI node), as bits 0 / 1 */
    float matrix[16];
    char name[64];
    char model[64];
} rx_scene_object;

/* The event scene's objects, up to max, in the scene lists' order; returns the count written, -1 on an invalid handle, 0 when
 * there is no scene yet. */
RX_API int rx_session_scene_objects(struct rx_session* s, rx_scene_object* out, uint32_t max);

/* Read-only live physics scenery snapshot. kind: 0 PhysicsEntity, 1 PopUpBale, 2 PopDownBale,
 * 3 StepUpBar. flags: 1 actual ModelInstance +0xa2 visible, 2 matrix differs from first snapshot,
 * 4 first snapshot, 8 physics body present, 16 body +0xb0 enabled. No visibility/collision is inferred
 * from lap counters. Scene props are cached once after loading; runtime vehicle debris is excluded.
 * Returns total matching props (may exceed max); writes min(total,max). NULL out is a count query. */
typedef struct rx_prop {
    uint32_t vtable;
    int32_t entity_id, kind;
    uint32_t flags;
    float matrix[16];
} rx_prop;
RX_API int rx_session_props(struct rx_session* s, rx_prop* out, uint32_t max);

/* The world matrix at `offset` (bytes, a multiple of 4, at most 0x2000) of the scene object whose class vtable and entity id are
 * given: the old find_object (vtable, +0x14 id) followed by reflex_world_read of 64 bytes, without exposing a game address to
 * the caller. The object must be in rx_session_scene_objects' list. 0 ok, 1 not found / invalid. */
RX_API int rx_session_scene_object_matrix(struct rx_session* s, uint32_t vtable, int32_t entity_id, uint32_t offset, float* out16);

/* A vehicle model's node flags (the record +0xc byte of the model hierarchy's records; bit 0x10 = the damage panel is hidden),
 * as OriginalSimulation's CollectObjects read them: model = V+0x1328, records = model+0x18, count = model+0x1c, flags[i] =
 * record[i].+0xc. slot -1 = the local player. Writes at most max bytes; returns the count (0: no vehicle / no model). */
RX_API int rx_session_node_flags(struct rx_session* s, int32_t slot, uint8_t* out, uint32_t max);

/* A slot's mud record (the game's table 0xa96d90, 12 x 0x40): the three float4s bound as usr_mud_amount / _wetness / _color.
 * slot -1 = the local player. 0 ok, 1 no vehicle / no record; the struct has record = -1. */
typedef struct rx_mud_state {
    int32_t slot, record;
    uint32_t vehicle, rider;
    float amount[4];
    float wetness[4];
    float color[4];
} rx_mud_state;
RX_API int rx_session_mud(struct rx_session* s, int32_t slot, rx_mud_state* out);

/* How many of the 12 mud records have an owner vehicle. */
RX_API int rx_session_mud_count(struct rx_session* s);

/* ---- tyre-track ribbons (TerrainTracks) ----------------------------------------------------------------------------- */

/* The live tyre-track decal ribbons of the active terrain, exactly the geometry TerrainTracks_Draw (007cc4f0) submits.
 * The game keeps three TerrainTracks rings (30 / 10 / 5 chunks; 24 strips each), the wheel emitters append cross sections
 * (TerrainTracks_AddPoint 007c3510) and HFTerrainSurface::vf14 (007c4fa0) flushes them into the ring's vertex / index buffers
 * once per frame. The core builds those objects (rx_hfterrain_surface_init) and runs the same game code; this export reads the
 * core's CPU-visible D3D buffers through rx_d3d_bits (the product's memory-backed device), de-indexed into triangles.
 *
 * One vertex is 6 floats (24 bytes):
 *   [0] world x, [1] world z (the SHORT4N pair * half the terrain extent: the same values the original VS uses),
 *   [2] side (1 = right edge, 0 = left edge: the shader's U coordinate 0/1),
 *   [3] V along the ribbon (32 * |w|: the shader's texture V coordinate),
 *   [4] intensity |s| in 0..1 (0 at the fading tip; the shader's alpha multiplier),
 *   [5] material type (0 Generic, 1 MX, 2 ATV, 3 UTV, 4 ProLite, 5 Buggy; the texture set the slot was allocated with).
 * Three vertices per triangle, no index buffer (the core already resolved the ring's triangle strip). */
#define RX_TRACK_VERTEX_FLOATS   6u
#define RX_TRACK_TRIANGLE_FLOATS 18u
#define RX_TRACK_MATERIALS       6
#define RX_TRACK_VERTEX_XZ_SIDE_V_INTENSITY_TYPE 1u   /* vertex_format */
#define RX_TRACK_INDEX_TRIANGLE_LIST 0u               /* index_format: `out` is a de-indexed triangle list */

typedef struct rx_track_info {
    uint32_t version;          /* bumped when the game flushed new ribbon vertices (once per advanced frame) */
    int32_t triangles;         /* total triangles available (even when max_triangles is smaller) */
    int32_t vertices;          /* 3 * triangles */
    int32_t strips;            /* allocated TerrainTracks slots with geometry */
    int32_t chunks;            /* live ring chunks with geometry */
    int32_t slots;             /* strips per ring (24) */
    int32_t sets;              /* TerrainTracks rings (3: 30 / 10 / 5 chunks) */
    uint32_t vertex_stride;    /* bytes per vertex: 6 * sizeof(float) */
    uint32_t vertex_format;    /* RX_TRACK_VERTEX_XZ_SIDE_V_INTENSITY_TYPE */
    uint32_t index_format;     /* RX_TRACK_INDEX_TRIANGLE_LIST */
    float half_x, half_z;      /* the terrain's +0x44d0 / +0x44d4 halved: the SHORT4N decode scale */
    float centre_x, centre_z;  /* the terrain's world bounds centre */
    int32_t material_count;    /* 6 */
    int32_t reserved0;
    char material[RX_TRACK_MATERIALS][24];   /* "Generic", "MX", "ATV", "UTV", "ProLite", "Buggy" */
} rx_track_info;

/* One allocated TerrainTracks strip with geometry. `first_vertex` / `vertices` index the triangle list
 * rx_session_tracks writes (vertices = 3 * triangles). `flags` bit 0 = allocated, bit 1 = has geometry. */
typedef struct rx_track_ribbon {
    int32_t set;            /* 0..2: the ring (30 / 10 / 5 chunks) */
    int32_t slot;           /* 0..23 */
    int32_t type;           /* material type 0..5 */
    int32_t first_vertex;
    int32_t vertices;
    int32_t triangles;
    uint32_t flags;         /* 1 allocated, 2 has geometry */
    uint32_t reserved;
} rx_track_ribbon;

/* The live ribbon triangles. Writes at most max_triangles * 18 floats into `out` (NULL: info only, returns 0) and fills info.
 * Returns the number of triangles written; info->triangles is the true total. -1 on an invalid handle, 0 before the world
 * (no active terrain). Pure within a game frame (the same buffers until the next advance). A version change means the game
 * flushed new vertices; the C# OriginalTracks renderer redraws its track map then. */
RX_API int rx_session_tracks(struct rx_session* s, float* out, uint32_t max_triangles, rx_track_info* info);

/* The per-strip descriptors of the same triangles (for a renderer that wants the ring / strip / material layout).
 * Writes at most max descriptors; returns the count (also info->strips). `out` may be NULL. The first_vertex values refer to
 * the rx_session_tracks list. */
RX_API int rx_session_track_ribbons(struct rx_session* s, rx_track_ribbon* out, uint32_t max, rx_track_info* info);

/* ---- input (the bridge's per-frame call) ---------------------------------------------------------------------------- */

/* One gamepad, the same 12 bytes as the old plugin's ReflexPad (XINPUT_GAMEPAD order). */
typedef struct rx_pad {
    uint16_t buttons;
    uint8_t left_trigger, right_trigger;
    int16_t lx, ly, rx, ry;
} rx_pad;

/* Sets the device state the game's own poll reads at every following frame (until the next call): the original's platform layer
 * (DirectInput GetDeviceState, XInputGetState) is served from it and the game's XenonInputManager poll (Engine::Update ->
 * XenonInputManager::vf43: Keyboard_Poll, Pad_Update) turns it into its records: the dead zones (0x1eb8 sticks, 30 triggers), the
 * pressed / released edges, the held times, the KeyMaps and the vehicle's input block are the game's. Nothing else writes them.
 *   keys     256 DirectInput keyboard bytes indexed by DIK scan code (DIK_W = 0x11, DIK_UP = 0xc8, ...); bit 0x80 = down (other
 *            bits ignored); NULL = no key down.
 *   pads     4 XINPUT_GAMEPADs (rx_pad: wButtons, bLeftTrigger, bRightTrigger, sThumbLX, LY, RX, RY as XInput reports them;
 *            Y up positive); NULL = zeros.
 *   pad_mask bit i = pad i connected (the game's XInputGetCapabilities / GetState succeed for it; else ERROR_DEVICE_NOT_CONNECTED
 *            and the game's pad record is "not connected").
 * Per-step input: every frame of a rx_session_advance_dt loop sees this state. Mapping and the records it reaches: notes/input-clock.md.
 * 0 ok, nonzero invalid handle. */
RX_API int rx_session_set_input(struct rx_session* s, const uint8_t* keys, const rx_pad* pads, uint32_t pad_mask);

/* The mouse, as raw input (WM_INPUT) events the message pump would deliver between two frames: the game's window procedure
 * (MXWndProc 00880dc0) sums them into its mouse record and the next frame consumes the sums (Mouse_Update). dx / dy = relative
 * pixels (+y down, as Raw Input), wheel = the wheel delta (120 per notch), button_flags = RAWMOUSE usButtonFlags (bit 2i down / bit
 * 2i + 1 up of left, right, middle, button 4, button 5). Call once per display frame with the motion since the last call (not once
 * per sim step: the first step after it consumes the sums). Runs game code: only on the simulation thread. 0 ok; 1 invalid /
 * error state / no input manager yet. rx_session_set_cursor sets the OS cursor position (client pixels) GetCursorPos returns. */
RX_API int rx_session_set_mouse(struct rx_session* s, int32_t dx, int32_t dy, int32_t wheel, uint32_t button_flags);
RX_API void rx_session_set_cursor(struct rx_session* s, int32_t x, int32_t y);

/* The motor speeds the game last sent to pad `pad` through XInputSetState (Pad_UpdateVibration; the game only sends them for a
 * connected pad with vibration on) and how many calls it made. 0 = at least one call was made; 1 none / invalid. */
RX_API int rx_session_rumble(struct rx_session* s, uint32_t pad, uint16_t* left, uint16_t* right, uint32_t* calls);

/* ---- PHYSICS STUDIO (port addition, after Open Unleashed; game/phys_studio.h, core/plan/PHYSICS_STUDIO.md) -------------------
 * Player-made values for the vehicle parameter files (mx125 / mx250 / mx250f / mx450f / atv / utv / buggy / truck2WD / truck2WDlite:
 * every registered parameter, every component) and the terrain grip table. Text in and out, UTF-8, one record per line, tab separated.
 * rx_phys_catalog: the adjustable values (no session needed: it reads the install's EngineDependencies with the format library):
 *   H <param-set hash %08x> <classes> <values> <grip materials> <drag materials>
 *   C <class> <id> <file> <label> <parameters> <live 0/1>
 *   P <class> <parameter> <name> <group> <kind 0 float, 1 vec3, 2 curve, 3 three grip levels> <components> <in the file 0/1>
 *   V <class> <parameter> <component> <label> <default> <min> <max> <unit>         (default = the file's value, %.9g: exact)
 *   G <material> <label> <default> <min> <max>     D <material> <label> <default> <min> <max>
 * rx_session_phys_set: the config for the session's next event (before rx_session_event_load), replacing any earlier one:
 *   v <class id> <parameter name> <component> <value>     g <material 0..5> <grip>     d <material 0..3> <factor>
 *   o grip_global <0|1>     o all_vehicles <0|1> (the vehicle values for every rider: the computer riders too)
 * Values are clamped to the catalog's bounds, NaN / inf and unknown names refused (listed in `report`); returns the values set.
 * Empty text = the original game (nothing is touched: bit for bit main). The values go to the LOCAL players' vehicles only (a spec
 * with a controller), over the file's values at its load point, under the garage tuning (vf35 applies the tuning on top). The grip
 * table also is per local vehicle unless grip_global; the drag factors are global (00461760 has no vehicle). A config with changes
 * keeps no offline records or highlights for the event (as MAP SCALE).
 * rx_session_phys_arm_live(s, 1) before the load: the local vehicles' file values are kept so rx_session_phys_apply_live can redo
 * them (every class: the original's own reloads, the masses 0043b750 / 0048c750 / 004cc030 / 004b1800 / 00450480 + vf35; the local vehicles, every
 * tracked one with all_vehicles) after a later rx_session_phys_set in the event.
 * rx_session_phys_live_values: "L <vehicle> <class> <sim>" then "l <vehicle> <name> <component> <now bits> <file bits>" lines. */
RX_API int rx_phys_catalog(const char* install_root, char* out, uint32_t capacity, uint32_t* required);
RX_API uint32_t rx_phys_param_set_hash(void);   /* 0 before the catalog is read */

RX_API int rx_session_phys_set(struct rx_session* s, const char* text, char* report, uint32_t capacity);
RX_API int rx_session_phys_get(struct rx_session* s, char* out, uint32_t capacity, uint32_t* required);
/* ALIVE CLASSES (port addition, core/plan/ALIVE_BIKES.md; game/phys_studio.h): a motorcycle parameter file of another game (a bxml of the
 * mx*_vehicle schema, root type "mx": MX vs ATV Alive's mx50_vehicle / mx650f_vehicle) written over the class file of the LOCAL players'
 * vehicles of `vehicle_model` when they are made (before rx_session_event_load; up to 4 models). cls: 9 Alive 50cc, 10 Alive 650 (mx files),
 * 11 Alive ATV1000 (an atv file, root type "atv", over an ATV's atv_vehicle); the
 * vehicle's rx_session_local_phys_class is then cls. data NULL / size 0 clears every one. Returns the records read (> 0) or < 0. */
RX_API int rx_session_vehicle_physics(struct rx_session* s, int32_t vehicle_model, int32_t cls, const uint8_t* data, uint32_t size, char* report, uint32_t capacity);
/* ... and that bike's own hierarchy (a bike of another size: the Alive 50cc): "<node name> <16 floats>" lines, each node's rest local matrix
 * (row-major, the game's row-vector convention, translation in elements 12..14), written over the made vehicle's model hierarchy nodes
 * of the same names, so the model pose (rx_session_bike_pose) has the bike's own geometry. After rx_session_vehicle_physics for the model. */
RX_API int rx_session_vehicle_physics_nodes(struct rx_session* s, int32_t vehicle_model, const char* text);
RX_API int rx_session_phys_live_values(struct rx_session* s, char* out, uint32_t capacity, uint32_t* required);
RX_API int rx_session_phys_arm_live(struct rx_session* s, int32_t on);
RX_API int rx_session_phys_apply_live(struct rx_session* s);   /* vehicles updated, -1 error */
RX_API int rx_session_phys_active(const struct rx_session* s); /* bits: 1 a config with changes is set; 2 the guard put a diverged vehicle back on the file's values; 4 a config was set at some point of the session (no records) */

/* ---- Reflex archives (port addition, core/plan/RXARCHIVE.md) ---------------------------------------------------------------
 * The indexed, compressed container of .rxdata (packed game data) and .rxtrack version 5 (one track's files). The core's own file
 * system reads them itself (an install root's *.rxdata and Tracks/<level>.rxtrack sit below its loose files; rx_session_mount_level
 * takes a .rxtrack path for `folder`). These exports let the client read the same archives (its own content readers) and write them
 * (the import, the track converter). Not session state: any thread, any time, no session needed.
 *   rx_archive_open        a handle, or NULL (rx_archive_error says why); rx_archive_close frees it (NULL is fine).
 *   rx_archive_kind        0x41544144 "DATA" or 0x4B415254 "TRAK".
 *   rx_archive_meta        the UTF-8 JSON metadata (owned by the handle); *length its bytes.
 *   rx_archive_count / _path / _size   the files, sorted by path ('/' separators below the game root).
 *   rx_archive_find        a file's index by path (case and separator insensitive), -1 when absent.
 *   rx_archive_read        n bytes at offset of file index into dst: 0, or -1 (range, read, decode or checksum failure).
 *   rx_archive_create      a writer (zstd level 1..19, 0 = 9); the file is <path>.partial until rx_archive_finish renames it.
 *   rx_archive_add         one file (thread safe: call from any number of threads); flags 1 = store as is. 0 or -1.
 *   rx_archive_set_meta    the JSON metadata; rx_archive_written: stored (and raw) bytes so far.
 *   rx_archive_finish      writes the index and renames; frees the writer either way. 0 or -1. rx_archive_abort: removes the partial file.
 *   rx_archive_error       the calling thread's last failure text. */
RX_API void*       rx_archive_open(const char* path_utf8);
RX_API void        rx_archive_close(void* archive);
RX_API uint32_t    rx_archive_kind(void* archive);
RX_API const char* rx_archive_meta(void* archive, int64_t* length);
RX_API int32_t     rx_archive_count(void* archive);
RX_API const char* rx_archive_path(void* archive, int32_t index);
RX_API int64_t     rx_archive_size(void* archive, int32_t index);
RX_API int32_t     rx_archive_find(void* archive, const char* path_utf8);
RX_API int32_t     rx_archive_read(void* archive, int32_t index, uint64_t offset, void* dst, uint64_t n);
/* Every path at once ('\n' between them, index order, no NUL) into dst when cap is enough, each file's size into sizes[0 .. nsizes - 1] and
 * (kinds non-NULL) its kind into kinds[0 .. nsizes - 1]: 0 bytes in this archive, 1 a pool reference (RXARCHIVE.md "Dedupe"): the bytes
 * the paths need (call with dst NULL to size the buffer), -1 for a bad handle. */
RX_API int64_t     rx_archive_list(void* archive, char* dst, int64_t cap, int64_t* sizes, int32_t nsizes, uint8_t* kinds);
/* An entry's SHA-256 as 64 hex digits into hex (65 bytes): 0 none (a container version 1 entry), 1 the bytes are here, 2 a pool
 * reference (its bytes are the root's Pool/<hex>). rx_archive_version: the container version (1 or 2). */
RX_API int32_t     rx_archive_entry_sha(void* archive, int32_t index, char* hex);
RX_API int32_t     rx_archive_version(void* archive);
RX_API void*       rx_archive_create(const char* path_utf8, uint32_t kind, int32_t level);
RX_API int32_t     rx_archive_add(void* writer, const char* path_utf8, const void* data, uint64_t n, uint32_t flags);
/* A pool reference: path's bytes (given to take their size, CRC-32 and SHA-256) are the root's Pool/<sha256>, not stored here. */
RX_API int32_t     rx_archive_add_ref(void* writer, const char* path_utf8, const void* data, uint64_t n);
/* Raw bytes stored once for several paths (dedupe) and left to the pool so far. */
RX_API int64_t     rx_archive_deduped(void* writer, int64_t* pooled);
RX_API int32_t     rx_archive_set_meta(void* writer, const char* json_utf8, int64_t length);
RX_API int64_t     rx_archive_written(void* writer, int64_t* raw);
RX_API int32_t     rx_archive_finish(void* writer);
RX_API void        rx_archive_abort(void* writer);
RX_API const char* rx_archive_error(void);

/* ---- ABI self-check ------------------------------------------------------------------------------------------------- */

/* sizeof of a struct of this header by its C name ("rx_bike_state", "rx_event_spec", "rx_pad", ...), -1 if unknown. The C#
 * bridge compares every one against Marshal.SizeOf before it creates a session (like reflex_world_sizeof). */
RX_API int32_t rx_session_sizeof(const char* name);

#ifdef __cplusplus
}
#endif
#endif /* RX_API_H */
