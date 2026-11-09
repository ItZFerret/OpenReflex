/* water_session.inc -- the water wake's pressure splats (include/rx_water.h; w-water), included by rx_session.c.
 *
 * RENDER BOUNDARY (documented): Wake_Step 007ed300 (WakeSimRenderEvent 007e3e90, the original's render thread, once per rendered frame
 * and view; render counter stamp at wake +0x30 + 4 view) steps the wake simulation's targets, then, when the view's primitive count
 * is positive, draws its queued splats under the wake's critical section (+0x30d8) and clears that view's three counts: centres
 * (+0x200 + 0x188 view), vertex floats (+0x1a50 + 0x1684 view) and primitives (+0x390 + 4 view). The targets, the rotation (+0x24)
 * and the camera snap (+0x3b0, +0x398) belong to the renderer (Unity OriginalWater); the counts are the game's: Wake_AddSplatQuad /
 * Wake_AddSplat append only below 0x5a0 floats. The session does the queue's part of Wake_Step at the render's place of every frame
 * (frame_render_run): copy view 0's queue into the host ring, clear the counts of view 0 and, with two views, of view 1 (its render
 * does the same). Nothing else of the game is touched; the copies are host memory. */
#include "rx_water.h"

#define WATER_WAKE_GLOBAL 0x00a95790u
#define WATER_WAKE_MAX_FLOATS 0x5a0u
#define WATER_WAKE_MAX_CENTRES 0x30u

struct water_splat_frame {
    uint64_t frame;
    uint32_t floats, centres, primitives;
    float v[WATER_WAKE_MAX_FLOATS];
    float c[2 * WATER_WAKE_MAX_CENTRES];
};
struct water_splat_ring {
    struct water_splat_frame f[RX_WATER_SPLAT_FRAMES];
    uint32_t next, count, dropped, views;
    uint64_t read_upto;                 /* the newest frame a read returned (host bookkeeping for `dropped`) */
};

/* the scene's view count as Wake_Step / Wake_AddSplatQuad read it: g_EngineContext [0xa95844] +0x1c + 4 [+4] -> scene +0x19d0 */
static int32_t water_views(void) {
    const rx_addr g = U32(0, 0x00a95844);
    if (!g) return 1;
    const rx_addr scene = U32(g, 0x1c + 4 * U32(g, 4));
    return scene ? I32(scene, 0x19d0) : 1;
}

static void water_splats_capture(struct rx_session* s) {
    const rx_addr W = U32(0, WATER_WAKE_GLOBAL);
    if (!W) return;
    const int32_t views = water_views();
    for (uint32_t view = 0; view < 2; view++) {
        if (view == 1 && views < 2) break;
        const rx_addr centres = W + 0x200 + 0x188 * view, floats = W + 0x1a50 + 0x1684 * view, prims = W + 0x390 + 4 * view;
        if (I32(prims, 0) <= 0) continue;   /* Wake_Step draws and clears only a view with primitives */
        const uint32_t nf = U32(floats, 0), nc = U32(centres, 0);
        if (nf > WATER_WAKE_MAX_FLOATS || nc > WATER_WAKE_MAX_CENTRES) continue;   /* not the wake's queue: leave it alone */
        if (view == 0) {
            if (!s->water_splats) s->water_splats = (struct water_splat_ring*)calloc(1, sizeof *s->water_splats);
            struct water_splat_ring* r = s->water_splats;
            if (r) {
                struct water_splat_frame* e = &r->f[r->next];
                if (r->count == RX_WATER_SPLAT_FRAMES) { if (e->frame > r->read_upto) r->dropped++; } else r->count++;
                r->next = (r->next + 1) % RX_WATER_SPLAT_FRAMES;
                e->frame = s->frames + 1;   /* rx_session_frames once this frame is done */
                e->floats = nf - nf % 5; e->centres = nc; e->primitives = U32(prims, 0);
                memcpy(e->v, rx_base + W + 0x3d0, 4u * e->floats);
                memcpy(e->c, rx_base + W + 0x80, 8u * nc);
                r->views = (uint32_t)views;
            }
        }
        SU32(centres, 0, 0); SU32(floats, 0, 0); SU32(prims, 0, 0);
    }
}

int rx_session_water_splats(struct rx_session* s, rx_water_splats* io, float* vertices, uint32_t max_vertices, float* centres, uint32_t max_centres) {
    if (!session_ok(s) || !io || io->size != sizeof *io || io->version != RX_WATER_SPLATS_VERSION) return 1;
    if ((!vertices && max_vertices) || (!centres && max_centres)) return 1;
    const uint64_t after = io->after_frame;
    memset(io, 0, sizeof *io);
    io->size = sizeof *io; io->version = RX_WATER_SPLATS_VERSION; io->after_frame = after; io->last_frame = after;
    struct water_splat_ring* r = s->water_splats;
    if (!r) { io->views = 1; return 0; }
    io->views = r->views; io->dropped = r->dropped;
    uint32_t nv = 0, nc = 0;
    for (uint32_t k = 0; k < r->count; k++) {
        const struct water_splat_frame* e = &r->f[(r->next + RX_WATER_SPLAT_FRAMES - r->count + k) % RX_WATER_SPLAT_FRAMES];
        if (e->frame <= after) continue;
        const uint32_t ev = e->floats / 5;
        if (io->truncated || nv + ev > max_vertices || nc + e->centres > max_centres || io->frames == RX_WATER_SPLAT_FRAMES) { io->truncated++; continue; }
        memcpy(vertices + 5 * nv, e->v, 4u * e->floats);
        memcpy(centres + 2 * nc, e->c, 8u * e->centres);
        io->frame[io->frames] = e->frame; io->frame_vertices[io->frames] = ev; io->frame_centres[io->frames] = e->centres;
        io->frames++; nv += ev; nc += e->centres; io->primitives += e->primitives;
        io->last_frame = e->frame;
    }
    io->vertices = nv; io->centres = nc;
    if (io->last_frame > r->read_upto) r->read_upto = io->last_frame;
    return 0;
}
