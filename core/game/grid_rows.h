/* grid_rows.h -- PORT ADDITION (not original game code; w-secondrow, core/plan/GRID_ROWS.md): the start grid's rows behind the gate.
 *
 * The original's grid (build_activity_grid.c 005663b0) is the start gate's "GateStart_01" .. "_16" nodes: 16 slots, one rider each. An
 * MX vs ATV Alive gate has 12 (w-aliveai), a custom gate may have any number. When a race has more riders than its gate has slots, the
 * riders past the slot count line up in rows behind the gate (as amateur motocross does), held there by the race's start like the gate's
 * own riders, and race when the gate drops.
 *
 * Places. A grid zone (the Player List's StartZone, the index of the activity's start slot A + 0x340 + 0x30 z) below the gate's slot count n
 * is a gate slot (the original); zone n and up are the places behind it, in rows: row 2 has n - 1 places (centred behind the gaps between
 * two gate slots), row 3 has n places (behind the gate slots), and so on alternating (a gate of one slot: one place per row). Within a row
 * the places count from the centre of the gate outwards (the gap or slot nearest the gate's middle first, ties to the GateStart_01 side),
 * so a short row fills the middle of the start straight; a lane with no clear spot (below) comes last in its row. rx_grid_place maps a
 * zone to its row (and its nominal lane); the online gate pick labels the places by row the same way (OnlineProtocol.GatePlace).
 *
 * Where. All from the gate's own slots and the level: the gate's forward axis (the slots' mean direction, flat), its pitch (the median
 * spacing of neighbouring slots), the riders' vehicle (the class tables 0x0093582c / 0x0093580c: a bike 7.5 x 2.5 ft). A place's
 * preferred depth is the rear of the bikes ahead plus a vehicle length plus a gap of half a pitch (1 .. 3 ft); then each place is
 * searched along its lane for a clear spot: even ground under the bike, a rideable lane to the gate, nothing in the rider's way
 * (grid_rows.c "the places": converted Alive gates have a rail, a raised pad or a start tower behind or in them). Its height is the ground
 * there (the terrain, or a static collision surface above it: a vertical ray) plus the gate slots' own height above their ground; it faces
 * the gate slots' way. Its track section / node (the AI's navigation start) is the gate slot's ahead of it (the place's own track search
 * is kept when it finds the same section). Its start slot gets the gate slots' "placed" flag (+0x2c: no start-line offset,
 * race_driver_reset.c). Every place depends only on the gate, the level and the event's vehicle, never on which places are taken: every
 * rider's game of an online race (each holds at most 12 of the field) puts a zone in the same spot.
 *
 * When. Only the riders past the gate need it, so the grid of a race whose riders fit is untouched:
 *  - a 16-slot (Reflex) gate: nothing is read or written unless the Player List has more rows than the gate has slots or an online grid
 *    names a zone past the gate (never with today's 12-player core; the rows are ready when the rider cap rises);
 *  - a smaller gate (Alive's 12, a custom one): the start zones are packed onto the gate as before (w-aliveai alive_gate_pack: kept when
 *    they are distinct gate slots, else ranked in their order); riders past n take the places behind it in that order; an online grid's
 *    zones (rx_session_set_start_zones: the riders' picks) are kept when they are distinct places.
 * The places live in the activity's 28 start slots (A + 0x340 .. +0x880), so a grid holds at most 28 riders (a 16-slot gate: 12 behind
 * it); the count of slots in use (+0x880) covers the places taken.
 *
 * Host statics only (the session's statics are restored at destroy); rx_grid_rows_info lists the last grid's places (tools). */
#pragma once
#include "../src/rx_mem.h"
#include <stdint.h>

#define RX_GRID_SLOTS 32u   /* the activity's start slots (A + 0x340, 0x30 each, 28 to +0x880; w-rc-core: 28..31 in rider_cap.h's side storage) */

/* the row (1 = the gate) and the index across the gate (0 = the GateStart_01 side; a row-2 place j sits behind the gap between gate
 * slots j and j + 1) of grid zone `zone` on a gate of `slots` slots; -1 for a bad zone / gate */
int rx_grid_place(uint32_t slots, uint32_t zone, uint32_t* row, uint32_t* across);

/* the grid pass found a gate of n slots (1 .. 16) and read them into the activity's first n start slots: the start zones packed (a gate
 * of fewer than 16 slots) and the places behind the gate the zones name built. The gate is the "TO_StartingGate" entity. */
void rx_grid_rows(rx_addr activity, rx_addr gate, uint32_t n);

/* a test hook (tools only, never the game): read at most `n` gate slots (0 = all), e.g. 8 of a Reflex gate to put a 12-rider race's
 * last 4 behind it */
void rx_grid_rows_test_cap(uint32_t n);
uint32_t rx_grid_rows_cap(void);

/* the last grid's places behind the gate (tools): */
typedef struct {
    int32_t zone, row, across;
    float pos[3], dir[3];
    float depth, shift;          /* ft behind the gate's slot line; ft across from its lane's middle */
    int32_t how;                 /* 0 the preferred depth, 1 moved back, 2 moved forward, 3 on a lane beside, 4 no clear spot (the preferred one) */
    int32_t why;                 /* at the preferred depth: 1 uneven ground under the bike, 2 an obstacle or a too high rise ahead, 3 something in the rider's way */
    float ground;                /* the ground under the place */
    int32_t on_static;           /* the ground is a static collision surface (1) or the terrain (0) */
    int32_t found_section;       /* the place's own track search found the gate slot's section (1), another (2), none (0) */
} rx_grid_place_info;
typedef struct {
    int32_t slots, riders, places;   /* the gate's slots, the active riders, places built behind it */
    float pitch, depth, clearance;   /* the slots' spacing, the preferred row depth, the slots' height over their ground */
    uint32_t activity; float fwd[3], across[3];   /* the activity; the gate's forward and across axes (flat) */
} rx_grid_info;
int rx_grid_ground_probe(rx_addr activity, float x, float y, float z, float* h, int32_t* on_static);   /* (tools) the ground, as the places read it */
int rx_grid_ray_probe(rx_addr activity, const float p0[3], const float p1[3], float hit[3]);   /* (tools) a static collision ray */
uint32_t rx_grid_rows_info(rx_grid_info* info, rx_grid_place_info* out, uint32_t max);
