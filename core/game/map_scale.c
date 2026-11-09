/* map_scale.c -- PORT ADDITION (not original game code): MAP SCALE (map_scale.h, core/plan/MAP_SCALE.md). */
#include "map_scale.h"
#include "build_activity.h"
#include <math.h>
#include <string.h>
#include <strings.h>

static float g_scale = 1.0f;     /* the session's (rx_session_set_map_scale); restored to 1 with the core's statics at destroy */
static int   g_depth;            /* open level brackets */
static uint32_t g_class;         /* the scene entity class being read */

static int g_custom;             /* MODS/TRACKS: the session rides a mounted custom track (rx_session_mount_level) */
void  rx_ms_set_custom_track(int on) { g_custom = on ? 1 : 0; }
int   rx_ms_custom_track(void) { return g_custom; }

void  rx_ms_set(float s) { g_scale = s; g_depth = 0; }
float rx_ms_scale(void) { return g_scale; }
float rx_ms_load(void) { return g_depth > 0 ? g_scale : 1.0f; }

static float g_base = 1.0f;      /* the track's default scale (rx_session_set_map_scale_base); restored to 1 at destroy */
void  rx_ms_set_base(float base) { g_base = base; }
float rx_ms_base(void) { return g_base; }
/* the client passes the scale as base x the player's percent: at 100 % the same float as the base; a relative tolerance covers a client
 * that rounds it differently */
int   rx_ms_records_off(void) { return fabsf(g_scale - g_base) > 1e-4f * g_base; }

static int is_level(const char* name, int exact) {
    if (g_scale == 1.0f || !name) return 0;
    const char* loc = rx_event_spec_get()->location;
    size_t n = strlen(loc);
    if (!n) return 0;
    if (exact) return strcasecmp(name, loc) == 0;
    return strncasecmp(name, loc, n) == 0;
}
int rx_ms_level_begin(const char* name) {
    int was = g_depth;
    /* a database / package name matches exactly, a subscene ("<level>Base", "<level>FreeRide" ...) or a track asset by its prefix */
    if (is_level(name, 0)) g_depth++;
    return was;
}
int rx_ms_level_force(void) { int was = g_depth; if (g_scale != 1.0f) g_depth++; return was; }
void rx_ms_level_end(int previous) { g_depth = previous; }

/* ---- scene entity matrices ---- */

void rx_ms_scene_class(uint32_t id) { g_class = id; }

static int volume_class(uint32_t id) {
    switch (id) {
    case 0x1028: case 0x1071:   /* TriggerRSData, TutorialTriggerRSData: a box / sphere of the axes' size */
    case 0x1043:                /* WaypointGateRSData */
    case 0x1042: case 0x107b:   /* SectionRaceTriggerRSData, TimedSectionTriggerRSData */
    case 0x1017:                /* StartEntitySnakeTagRSData (its area) */
        return 1;
    }
    return 0;
}

void rx_ms_scene_matrix(float m[16]) {
    const float k = rx_ms_load();
    if (k == 1.0f) return;
    m[12] *= k; m[13] *= k; m[14] *= k;
    if (volume_class(g_class)) for (int i = 0; i < 12; i++) m[i] *= k;
}

/* ---- collision geometry (build_collis.c layouts) ---- */

static void xyzz(rx_addr p, float k) { rx_ms_v4(p, 0, k); }

void rx_ms_collision_geometry(rx_addr g, float k) {
    if (k == 1.0f || !g) return;
    switch (U32(g, 4)) {
    case 0: {    /* triangles: +0xc 3 n vertices (x, y, z, z), +0x10 n, +0x14 the radius */
        uint32_t n = U32(g, 0x10) * 3;
        for (uint32_t i = 0; i < n; i++) xyzz(U32(g, 0xc) + 16 * i, k);
        rx_ms_f(g, 0x14, k);
        break;
    }
    case 2: {    /* box list: +0xc n boxes of two vectors, +0x10 n, +0x14 the radius */
        uint32_t n = U32(g, 0x10) * 2;
        for (uint32_t i = 0; i < n; i++) xyzz(U32(g, 0xc) + 16 * i, k);
        rx_ms_f(g, 0x14, k);
        break;
    }
    case 11:     /* oriented box: +0x10 the axis vector, +0x30 the radius splat, +0x50 the half extents */
        rx_ms_v4(g, 0x10, k); rx_ms_v4(g, 0x30, k); rx_ms_v4(g, 0x50, k);
        break;
    case 15: {   /* composite: the children, each matrix's translation, the translations */
        uint32_t n = (U32(g, 0x4c) - U32(g, 0x48)) >> 2;
        for (uint32_t i = 0; i < n; i++) {
            rx_ms_collision_geometry(U32(U32(g, 0x48), 4 * i), k);
            rx_ms_v3(U32(g, 0x60) + 0x40 * i, 0x30, k);
            xyzz(U32(g, 0x90) + 0x10 * i, k);
        }
        break;
    }
    case 16:     /* sphere: +0xc the radius */
        rx_ms_f(g, 0xc, k);
        break;
    case 17:     /* cylinder: +0x10 / +0x20 / +0x30 the class vectors, +0x44 radius, +0x48 half height */
        rx_ms_v4(g, 0x10, k); rx_ms_v4(g, 0x20, k); rx_ms_v4(g, 0x30, k);
        rx_ms_f(g, 0x44, k); rx_ms_f(g, 0x48, k);
        break;
    case 22: {   /* triangle groups: +0xc n centres, +0x10 the groups, +0x14 n */
        uint32_t n = U32(g, 0x14);
        for (uint32_t i = 0; i < n; i++) {
            xyzz(U32(g, 0xc) + 16 * i, k);
            rx_ms_collision_geometry(U32(U32(g, 0x10), 4 * i), k);
        }
        break;
    }
    default: break;
    }
}
