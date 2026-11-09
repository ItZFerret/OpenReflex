#ifndef RX_WATER_H
#define RX_WATER_H
#include "rx_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* The water wake's pressure splats (agent w-water, 2026-10-30). Additive: a new query and struct, RX_SESSION_API_VERSION unchanged.
 *
 * The game queues a splat whenever a vehicle (or the Tag ball) touches a water surface: Wake_AddSplatQuad 007ec280 / Wake_AddSplat
 * 007ecd80 (core/game/vehicle_water_wake.c) append six vertices of five floats {x, y, strength, u, v} to the wake object
 * [0x00a95790] +0x3d0 (float count +0x1a50, max 0x5a0) and the splat's world centre {x, z} to +0x80 (count +0x200, max 0x30).
 * The original's render event (WakeSimRenderEvent 007e3e90 -> Wake_Step 007ed300) draws them into the wake simulation's
 * target once per rendered frame and clears the counts. The core has no renderer: at the render's place of every frame
 * (rx_session.c frame_render_run) the session takes the queue the way Wake_Step does (view 0's queue, then its three counts
 * cleared) into a host ring of the last RX_WATER_SPLAT_FRAMES frames. Unity's wake simulation (OriginalWater, the original's
 * transliterated programs) draws them; without the hand-off the queue filled after ~48 splats and the game stopped adding any.
 *
 * Per splat (30 floats): six vertices {x, y, strength, u, v} with x / y in the wake target's clip units around the centre
 * (256 world units = 1 with one view; 128 with two), and one centre {world x, world z}. Vertices of the splats of one frame are
 * contiguous; centres are one per 30 vertex floats, as Wake_Step reads them (index = float / 30). */
#define RX_WATER_SPLATS_VERSION 1u
#define RX_WATER_SPLAT_FRAMES 32u
typedef struct rx_water_splats {
    uint32_t size, version;            /* caller: sizeof (rx_water_splats), RX_WATER_SPLATS_VERSION */
    uint64_t after_frame;              /* caller: the frames after this rx_session_frames value (0: every frame the ring holds) */
    uint32_t frames;                   /* frames copied (each with at least one splat), oldest first */
    uint32_t vertices, centres;        /* vertices (5 floats each) and centres (2 floats each) copied in all */
    uint32_t primitives;               /* the frames' primitive counts (+0x390: 2 per splat) */
    uint32_t truncated;                /* later frames left out: the caller's buffers were full */
    uint32_t dropped;                  /* frames the ring overwrote before they were read (since the session started) */
    uint32_t views;                    /* the scene's view count at the last capture (1: 512-unit target, 256 units = 1) */
    uint32_t reserved0;
    uint64_t last_frame;               /* the newest frame copied (after_frame when none): pass it as after_frame next time */
    uint64_t frame[RX_WATER_SPLAT_FRAMES];             /* rx_session_frames of each copied frame */
    uint32_t frame_vertices[RX_WATER_SPLAT_FRAMES];    /* its vertices */
    uint32_t frame_centres[RX_WATER_SPLAT_FRAMES];     /* its centres */
} rx_water_splats;                     /* 568 bytes */
/* Copies the splats of the frames after io->after_frame (whole frames only) into vertices (5 floats each, max_vertices) and centres
 * (2 floats each, max_centres). Either buffer may be NULL with max 0 (then only the counts). Reads only: the ring is not cleared, the
 * game is not touched. 1 on bad arguments, else 0. Session owner thread, between advances. */
RX_API int rx_session_water_splats(struct rx_session* session, rx_water_splats* io, float* vertices, uint32_t max_vertices,
                                   float* centres, uint32_t max_centres);
#ifdef __cplusplus
}
#endif
#endif
