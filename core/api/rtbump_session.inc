/* rtbump_session.inc -- RT_Bump cascade layout for the client (w-rtbumpfit), included by rx_session.c (uses session_ok,
 * active_terrain).
 *
 * rx_session_rtbump_fit runs the game's own layout of RT_Bump's three slices (game/fx_rtbump.c: HFTerrainTextureRenderEvent
 * 007c1010's camera preparation and RTBump_FitCascadeMatrices 007ba8d0, the terrain queries 007c5950 / 007c6cc0) for the client's
 * camera, on the active terrain. The client draws the slices and the terrain's lookups from the trapezoid it returns (the original's
 * matrices map exactly that quad's slabs onto the slices). Call between frames (it uses the game stack). */
#include "../game/fx_rtbump.h"

int rx_session_rtbump_fit(struct rx_session* s, const float pos[3], const float fwd[3], const float up[3], float fov, float aspect,
                          float near_dist, float out[12]) {
    if (!session_ok(s) || !pos || !fwd || !up || !out) return 1;
    const rx_addr terrain = active_terrain();
    if (!terrain) return 2;
    const rx_addr sp0 = rx_sp(), F = ((sp0 - 0x40u) & ~0xfu) - 0x300u;
    rx_set_sp(F);
    /* a camera block as 007c1010 reads it, the event (+4 the terrain), the inputs, the outputs */
    const rx_addr cam = F, ev = F + 0x60, fh = F + 0x70, rh = F + 0x80, upv = F + 0x90, p = F + 0xa0, f = F + 0xb0, vo = F + 0xc0, to = F + 0x180;
    SF32(cam, 0x10, pos[0]); SF32(cam, 0x14, pos[1]); SF32(cam, 0x18, pos[2]);
    SF32(cam, 0x30, up[0]); SF32(cam, 0x34, up[1]); SF32(cam, 0x38, up[2]);
    SF32(cam, 0x40, fwd[0]); SF32(cam, 0x44, fwd[1]); SF32(cam, 0x48, fwd[2]);
    SF32(cam, 0x50, fov); SF32(cam, 0x54, aspect); SF32(cam, 0x58, near_dist);
    SU32(ev, 4, terrain);
    fx_rtbump_inputs in;
    fx_rtbump_camera_inputs(cam, &in);
    for (int i = 0; i < 4; i++) {
        SF32(fh, 4u * (uint32_t)i, in.fwd_h[i]); SF32(rh, 4u * (uint32_t)i, in.right_h[i]); SF32(upv, 4u * (uint32_t)i, in.up[i]);
    }
    SF32(p, 0, pos[0]); SF32(p, 4, pos[1]); SF32(p, 8, pos[2]);
    SF32(f, 0, fwd[0]); SF32(f, 4, fwd[1]); SF32(f, 8, fwd[2]);
    rx_set_sp(F - 0x10u);
    fx_rtbump_quad q;
    fx_rtbump_fit_cascades(fh, rh, ev, cam, p, f, upv, in.aspect, vo, to, in.fov, &q);
    rx_set_sp(sp0);
    out[0] = q.near_z; out[1] = q.far_z; out[2] = q.ax; out[3] = q.bx; out[4] = q.cx; out[5] = q.dx;
    out[6] = in.fwd_h[0]; out[7] = in.fwd_h[2]; out[8] = in.right_h[0]; out[9] = in.right_h[2]; out[10] = pos[0]; out[11] = pos[2];
    return 0;
}
