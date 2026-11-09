/* fx_rtbump.h -- the RT_Bump cascade layout of a frame (HFTerrainTextureRenderEvent 007c1010's camera preparation,
 * RTBump_FitCascadeMatrices 007ba8d0, RTBump_TrapezoidMatrix 007ba260). See fx_rtbump.c. */
#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif

/* What 007c1010 hands 007ba8d0, from a camera block (+0x10 position, +0x30 up, +0x40 forward, +0x50 vertical fov (radians),
 * +0x54 aspect, +0x58 near distance): the horizontal forward (ECX) and right (EDX), the re-orthogonalised up, the widened fov
 * (xmm1) and the aspect (stack). */
typedef struct fx_rtbump_inputs {
    float fwd_h[4], right_h[4], up[4], fov, aspect;
} fx_rtbump_inputs;
void fx_rtbump_camera_inputs(rx_addr camera, fx_rtbump_inputs* out);

/* The trapezoid of the frame in the camera's horizontal frame (x along right_h, z along fwd_h, from the camera position):
 * near edge z = near (A left, B right), far edge z = far (C left, D right); the slabs end at near + 16, near + 64, far. */
typedef struct fx_rtbump_quad {
    float near_z, far_z, ax, bx, cx, dx;
} fx_rtbump_quad;

/* 007ba8d0 (ECX fwd_h*, EDX right_h*; stack event*, camera*, position*, forward*, up*, aspect, view_out*, trap_out*; xmm1 fov;
 * RET 0x20): view_out / trap_out get the three slabs' 4x4 view-projection and trapezoid matrices (0x40 each). quad (may be
 * NULL) gets the trapezoid. */
void fx_rtbump_fit_cascades(rx_addr fwd_h, rx_addr right_h, rx_addr event, rx_addr camera, rx_addr position, rx_addr forward,
                            rx_addr up, float aspect, rx_addr view_out, rx_addr trap_out, float fov, fx_rtbump_quad* quad);

/* 007ba260 (ECX p0*, EDX p1*; stack out*, p3*, p2*; RET 0xc): the projective map of the slab's projected corners onto the
 * unit square. */
void fx_rtbump_trapezoid_matrix(rx_addr p0, rx_addr p1, rx_addr out, rx_addr p3, rx_addr p2);

#ifdef __cplusplus
}
#endif
