/* freeride_any_level.c -- RESTORATION (not original game code, not a port of any address): Free Ride on every level of the install.
 *
 * Why. 36 of the install's 38 race levels have a "<level>FreeRide" scene section (the original's menus never offer it), and on 33 it
 * holds the twelve free ride starts (0x1014) that MXActivityFreeRide::vf2 / vf18 read (00560cd0). Five have none:
 * NAT_RenneB and TRX_Halifax (the section only holds storage entities / a camera path), FMX_DoverB (its twelve free ride starts sit in
 * the Base section, not in the activity's group) and the two DLC arenas FMX_DLC03_FunFactory / FMX_DLC04_FunFactoryB (no section).
 * The original then fills the slots with its all-default pattern (slot k at (2 k, 0, 0)): the rider would start at the world origin.
 *
 * What. Called by the product's 00560cd0 right after that default fill (never on a level with free ride starts, so the original
 * Free Ride maps and the 33 levels are untouched). The spawn comes from the level's own data, first that exists:
 *   1. free ride starts (0x1014) anywhere in the scene (SceneManager_CollectTyped 007d2920): each one's transform (vf15), its slot
 *      number (+0x90 - 1) as 00560cd0 takes it;
 *   2. the arena's Freestyle starts (0x1011, what MXActivityFreestyleStub::vf2 00562560 reads): their transforms in order;
 *   3. the level's race track (.tdf07 of the first race kind the database has: nat, sx, trx, pwx, fmx) loaded into a scratch track
 *      block (00536ef0): its start record (+0x10 the centre snapped to the base terrain, +0x20 the direction), moved 10 ft forward
 *      (off the start line; on NAT_RenneB, the one of them with a start gate, the rider rides away freely);
 *   4. (w-fresnob) the start gate: the scene's model entity placing GL_StartGate (the supercross / arena gate), 10 ft out of it along
 *      its forward axis (matrix row 2). Only the RESTORED cut level SX_FresnoB gets here (its track definition is lost: the shipped
 *      package ends before it, _re/formats/fresnob_repair.py); every shipped level stops at 1 .. 3.
 * Slots without a start of their own take the first one's, 4 ft to the side each (local split screen). */
#include "freeride_any_level.h"
#include "build_activity_int.h"
#include "build_activity.h"
#include "build_rider_util.h"
#include "build_alloc.h"
#include "anim.h"
#include "map_scale.h"
#include "../src/fmt_scene.h"
#include <math.h>

uint32_t rx_fr_any_level_source;

#define FR_SLOTS 0xc
#define FR_SLOT(A, k) ((A) + 0x340 + 0x30u * (uint32_t)(k))

static void put_slot(rx_addr A, uint32_t k, const float p[3], const float d[3]) {
    const rx_addr s = FR_SLOT(A, k);
    SF32(s, 0x00, p[0]); SF32(s, 0x04, p[1]); SF32(s, 0x08, p[2]);
    SF32(s, 0x10, d[0]); SF32(s, 0x14, d[1]); SF32(s, 0x18, d[2]);
}

/* the slots no start filled: the first filled one, 4 ft further to its right each */
static void spread(rx_addr A, const uint8_t used[FR_SLOTS]) {
    int first = -1;
    for (int k = 0; k < FR_SLOTS && first < 0; k++) if (used[k]) first = k;
    if (first < 0) return;
    const rx_addr b = FR_SLOT(A, first);
    const float p0[3] = {F32(b, 0), F32(b, 4), F32(b, 8)}, d[3] = {F32(b, 0x10), F32(b, 0x14), F32(b, 0x18)};
    float rx = d[2], rz = -d[0];
    const float rl = sqrtf(rx * rx + rz * rz);
    if (rl > 1e-6f) { rx /= rl; rz /= rl; } else { rx = 1.0f; rz = 0.0f; }
    int n = 0;
    for (int k = 0; k < FR_SLOTS; k++) {
        if (used[k]) continue;
        n++;
        const float p[3] = {p0[0] + rx * 4.0f * (float)n, p0[1], p0[2] + rz * 4.0f * (float)n};
        put_slot(A, (uint32_t)k, p, d);
    }
}

/* 1 / 2: the scene's start entities of a type; 0x1014 by their slot number, 0x1011 in order. Returns the count used. */
static int from_entities(rx_addr A, uint32_t type, rx_addr list, rx_addr out) {
    const rx_addr scene = U32(U32(A, 0x234), 0x20138);
    for (uint32_t o = 0; o < 4u * FR_SLOTS; o += 4) SU32(list, o, 0);
    uint32_t n;
    ACT_CALLR(n, 4, rx_scene_manager_collect_typed(scene, type, list, FR_SLOTS, 0));
    if (n == 0) return 0;
    uint8_t used[FR_SLOTS] = {0};
    int placed = 0;
    for (uint32_t i = 0; i < n && i < FR_SLOTS; i++) {
        const rx_addr e = U32(list, 4 * i);
        { const uint32_t a[1] = {out}; rr_vcall(e, 0x3c, 1, a); }        /* vf15: the entity's transform (row 2 forward, row 3 position) */
        int slot = (int)i;
        if (type == 0x1014) slot = (int32_t)U32(e, 0x90) - 1;
        if (slot < 0 || slot >= FR_SLOTS || used[slot]) {               /* a missing / repeated number: the first free slot */
            slot = -1;
            for (int k = 0; k < FR_SLOTS; k++) if (!used[k]) { slot = k; break; }
            if (slot < 0) break;
        }
        const float p[3] = {F32(out, 0x30), F32(out, 0x34), F32(out, 0x38)}, d[3] = {F32(out, 0x20), F32(out, 0x24), F32(out, 0x28)};
        put_slot(A, (uint32_t)slot, p, d);
        used[slot] = 1;
        placed++;
    }
    spread(A, used);
    return placed;
}

/* 3: the race track's start record */
static int from_race_track(rx_addr A) {
    static const uint32_t kinds[5] = {0, 1, 10, 12, 6};   /* nat, sx, trx, pwx, fmx (build_activity_track.c k_suffix) */
    const rx_addr w = U32(A, 0x234), level = w + 0x20030;
    uint32_t kind = 0xffffffffu;
    for (int k = 0; k < 5 && kind == 0xffffffffu; k++) {
        uint32_t has;
        ACT_CALLR(has, 1, rx_track_has_asset(kinds[k], level, 0));
        if (has) kind = kinds[k];
    }
    if (kind == 0xffffffffu) return 0;
    const rx_addr block = rx_reflex_aligned_alloc(0x400);
    if (!block) return 0;
    for (uint32_t o = 0; o < 0x400; o += 4) SU32(block, o, 0);
    SU32(block, 0, w);                                    /* the holder's world: the start record snaps onto its terrain */
    uint32_t ok;
    ACT_CALLR(ok, 3, rx_track_load(level, block, kind, 0));
    int placed = 0;
    if (ok) {
        float d[3] = {F32(block, 0x20), F32(block, 0x24), F32(block, 0x28)};
        const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l > 1e-6f && isfinite(l)) {
            d[0] /= l; d[1] /= l; d[2] /= l;
            const float p[3] = {F32(block, 0x10) + d[0] * 10.0f, F32(block, 0x14), F32(block, 0x18) + d[2] * 10.0f};
            if (isfinite(p[0]) && isfinite(p[1]) && isfinite(p[2])) {
                uint8_t used[FR_SLOTS] = {0};
                put_slot(A, 0, p, d);
                used[0] = 1;
                spread(A, used);
                placed = 1;
            }
        }
    }
    ACT_CALL(0, rx_track_block_reset(block));
    rx_reflex_aligned_free(block);
    return placed;
}

/* 4: the start gate model entity of the level's loaded subscenes (the scene file, as the track loader reads it) */
static int from_start_gate(rx_addr A) {
    fmt_scene* sc = rx_event_level() ? fmt_level_scene((fmt_level*)rx_event_level()) : NULL;
    if (!sc) return 0;
    for (int nth = 0;; nth++) {
        const fmt_entity* e = fmt_scene_find_type(sc, -1, "ModelRSData", nth);
        if (!e) return 0;
        const char* model = fmt_entity_asset(e, "ModelAsset");
        float m[16];
        if (!model || fmt_stricmp(model, "GL_StartGate") != 0 || fmt_entity_matrix(e, m) != 0) continue;
        float d[3] = {m[8], 0.0f, m[10]};
        const float l = sqrtf(d[0] * d[0] + d[2] * d[2]), s = rx_ms_scale();
        if (!(l > 1e-6f) || !isfinite(l)) continue;
        d[0] /= l; d[2] /= l;
        const float p[3] = {m[12] * s + d[0] * 10.0f, m[13] * s, m[14] * s + d[2] * 10.0f};
        if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])) continue;
        uint8_t used[FR_SLOTS] = {0};
        put_slot(A, 0, p, d);
        used[0] = 1;
        spread(A, used);
        return 1;
    }
}

void rx_fr_any_level_spawn(rx_addr A) {
    ACT_FRAME_EBP(L, 16, 0x80, 4);   /* 16-aligned: the transform buffer at L + 0x40 */
    const rx_addr list = L, out = L + 0x40;
    rx_fr_any_level_source = 0;
    if (from_entities(A, 0x1014, list, out) > 0) rx_fr_any_level_source = 1;
    else if (from_entities(A, 0x1011, list, out) > 0) rx_fr_any_level_source = 2;
    else if (from_race_track(A) > 0) rx_fr_any_level_source = 3;
    else if (from_start_gate(A) > 0) rx_fr_any_level_source = 4;
    ACT_END(L);
}
