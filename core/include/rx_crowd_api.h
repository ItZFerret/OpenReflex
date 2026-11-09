/* rx_crowd_api.h -- the stands crowd's camera flashes for the renderer (additive, w-screens 2026-10-27; no existing struct changed).
 *
 * The crowd sections (BillboardCrowdEntity, rx_session_ambient_entities kind 8) simulate camera flashes: 006c6650 gives every idle flash
 * record (16 bytes per LOD-0 binding of the section's model: +0 active, +4 age, +8 length, +0xc seat number 0 .. 499) a chance per frame to
 * start, for 0.01 .. 0.11 s. XenonBillboardCrowdRenderEvent draws each active record (006c6340 queues it, 006c7880 draws it) with the
 * "Crowd" shader asset's CameraFlashVS / CameraFlashPS and the level's "CameraFlash" texture: ONE quad of the binding's own seat vertex
 * buffer - the seat (seat % (vertices of that binding / 4)) - so a flash sits on a seat of the section, camera-facing, 32 x 32 world units,
 * alpha blended, no depth write (the shader programs: Unity/Reflex/Assets/Shaders/ReflexCrowdFlash.shader). The shader does not read the
 * age or length: a flash is the same picture for its whole life.
 *
 * Version 1: rx_crowd_flash, rx_session_crowd_flashes. */
#ifndef RX_CROWD_API_H
#define RX_CROWD_API_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define RX_CROWD_API_VERSION 1u

/* One ACTIVE flash record of one crowd section. 32 bytes. */
typedef struct rx_crowd_flash {
    uint32_t object_id;           /* the section's rx_ambient_entity.object_id (an identity; it changes on rx_session_ambient_rescan) */
    int32_t entity_id;            /* the section's scene entity id (rx_ambient_entity.entity_id) */
    int32_t binding;              /* the record's index = the LOD-0 binding of the section's model whose seat mesh the flash is drawn on */
    uint32_t seat;                /* record +0xc: the original draws the quad of seat (seat % (vertex count of that binding / 4)) */
    float age;                    /* record +4: seconds since the flash started */
    float length;                 /* record +8: how long it lasts (0.01 .. 0.11 s) */
    float empty_chair_ratio;      /* the vertex program's emptyChairRatio (c66) of this section's tier: a seat whose frac(3.222 x |position|) is at or
                                   * above it is an empty chair and shows no flash (the same value as rx_ambient_entity.param[1]) */
    float reserved;
} rx_crowd_flash;                 /* 32 bytes */

/* The active flash records of every crowd section the world has (the ambient scan's BillboardCrowdEntity objects), section by
 * section in the scan's order, record by record. `out` may be NULL (count only). Returns the number of active records (all of
 * them, even when more than `max`; at most `max` are written), -1 for an invalid handle, 0 before the world exists or when no
 * level has the CameraFlash texture. Read-only: the records are the game's live fields. */
RX_API int rx_session_crowd_flashes(struct rx_session* s, rx_crowd_flash* out, uint32_t max);

#ifdef __cplusplus
}
#endif
#endif
