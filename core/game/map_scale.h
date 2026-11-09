/* map_scale.h -- PORT ADDITION (not original game code): MAP SCALE, after Open Unleashed's map_scale.h (core/plan/MAP_SCALE.md).
 *
 * A session's track loads scaled by s about the world origin (the open shell's race setup row MAP SCALE, 50 % .. 200 %): the
 * terrain (its cell spacing and heights), the level's static collision geometry, every scene entity's placement, the volumes
 * of the trigger-like entities, the .tdf07 track definition (AI racing line, start / finish records, section and node lengths),
 * the forest's trees, the water heights, the replay cameras' areas / paths / ranges ... The vehicles, the riders, gravity and the
 * physics keep their real size.
 *
 * s = 1 is the original game bit for bit: every site is guarded by `rx_ms_load() != 1.0f` (or rx_ms_on()) and does nothing.
 *
 * The scale only applies to data of the event's own level: rx_ms_level_begin / rx_ms_level_end bracket the loads whose source is
 * the level (the level database's packages, the level's subscenes); the front end's world (UI_Compound) that an event session also
 * builds, and the vehicle / rider databases, load unscaled. rx_ms_load() is the scale of the data being loaded right now (s inside
 * such a bracket, else 1); rx_ms_scale() is the session's scale (for the few runtime sites, and the exports). */
#ifndef RX_MAP_SCALE_H
#define RX_MAP_SCALE_H

#include "../src/rx_mem.h"
#include <stdint.h>

/* the session's scale (1 = off); set before the event loads (rx_session_set_map_scale) */
void  rx_ms_set(float s);
float rx_ms_scale(void);
/* the scale of the data loading now: rx_ms_scale() inside a level bracket, else 1 */
float rx_ms_load(void);
/* a bracket around a load whose source is `name` (a database, a package or a subscene name): it is the level's when the name
 * starts with the event's location (case-insensitive). Nested brackets are counted; returns the previous depth for _end. */
int   rx_ms_level_begin(const char* name);
void  rx_ms_level_end(int previous);
/* an unconditional bracket (the caller knows the data is the level's: the .tdf07 of the event's level) */
int   rx_ms_level_force(void);

static inline int rx_ms_on(void) { return rx_ms_scale() != 1.0f; }
/* BASE SCALE (port addition, w-alivegates): the track's default scale, the scale its event counts as unscaled at (1 for the game's tracks;
 * the client gives an MX vs ATV Alive level its own: Nationals and short tracks 1.1, Supercross 1.3, core/plan/MAP_SCALE.md "Base scale").
 * The world loads at rx_ms_scale() as before (the base is already in it); only the records test the base: at the track's default the event
 * keeps its offline records, best laps and highlights, at any other scale none (rx_ms_records_off). Set before the event loads
 * (rx_session_set_map_scale_base); restored to 1 with the core's statics at destroy. */
void  rx_ms_set_base(float base);
float rx_ms_base(void);
/* the world's scale is not its track's default: no offline records, best laps, highlights (and the lap log's map scale bit) */
int   rx_ms_records_off(void);
/* MODS/TRACKS (port addition, rx_session_mount_level): the event rides a custom track read from a mod's folder. Like a scaled track it
 * keeps no offline records or highlight progress (the profile's records are keyed by level name: a mod's slot name is shared by every
 * mod that ships as that slot). Restored to 0 with the core's statics at destroy. */
void  rx_ms_set_custom_track(int on);
int   rx_ms_custom_track(void);
/* the drop gate's slots (GateStart_NN, model space) scale with the gate, but never below 0.38 (Open Unleashed's floor: the bikes keep
 * room side by side) */
static inline float rx_ms_gate_slots(void) { const float s = rx_ms_scale(); return s < 0.38f ? 0.38f : s; }

/* helpers on game memory (no-ops when k == 1) */
static inline void rx_ms_f(rx_addr p, uint32_t off, float k) { if (k != 1.0f) SF32(p, off, F32(p, off) * k); }
static inline void rx_ms_v3(rx_addr p, uint32_t off, float k) {
    if (k == 1.0f) return;
    for (uint32_t i = 0; i < 12; i += 4) SF32(p, off + i, F32(p, off + i) * k);
}
static inline void rx_ms_v4(rx_addr p, uint32_t off, float k) {
    if (k == 1.0f) return;
    for (uint32_t i = 0; i < 16; i += 4) SF32(p, off + i, F32(p, off + i) * k);
}

/* the scene entity class of the RSData being read (rx_scene_entity_from_rsdata): the classes whose matrix axes are the size of a
 * volume (Trigger, TutorialTrigger, WaypointGate, SectionRaceTrigger, TimedSectionTrigger, StartEntitySnakeTag) scale whole, the
 * others move only (their axes stay a rotation) */
void rx_ms_scene_class(uint32_t id);
void rx_ms_scene_matrix(float m[16]);

/* the collision geometry of a collis asset (build_collis.c's kinds 2 / 11 / 15 / 16 / 17 / 22), scaled in place */
void rx_ms_collision_geometry(rx_addr g, float k);

#endif
