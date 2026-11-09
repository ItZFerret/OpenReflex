/* loop_cam_replay.c -- the replay cameras, the TV director's cameras (loop_cam_replay.h): the MXReplayCamera base and the four classes
 * MXReplayTowerCamera, MXZipLineCamera, MXSplineCamera and MXAnimatedReplayCamera, hand transliterated from MXReflex.exe: constructors,
 * destructors, the RSData readers (vf5), the camera update (vf19), the director's value (vf37), the activation / curve debug draw (vf39).
 * vf19 of the base and of the tower are in loop_cam_misc.c, the tower's destructor in anim_entity_dtors.c.
 *
 * Fields (cam, an MXCameraEntity, loop_cam_entity.c: +0x90 the Camera, +0xe0 its fov, +0x2f4 "pose valid"):
 *   base (0x95a504): +0x310 / +0x314 the director list's next / previous (world +0x21dd8), +0x318 retarget clock, +0x31c Sloppyness (drift
 *     distance factor), +0x320 Spaz (drift acceleration), +0x324 FrequencyOfError (retarget period), +0x328 AngleOfError (tan of it),
 *     +0x32c MaxVelocity, +0x330 MaxVelScale, +0x334 OrderIndex, +0x340 the drift vector spring {offset, +0x350 target, +0x360 velocity},
 *     +0x370 drifting on, +0x380 the authored pose (the entity's matrix, copied by vf5).
 *   the four classes: +0x3c0 .. +0x3c8 the fovs of three ranges (radians; the animated camera's are at +0x3c4 .. +0x3cc, everything after
 *     it one word up), +0x3cc .. +0x3d4 the ranges, +0x3d8 the fov acceleration, +0x3dc the number of activation triangles, +0x3e0 the
 *     ActivationArea list {data of 16-byte elements, +4 count, +8 capacity, +0xc growth, +0x10 max} (animated: +0x3e4, count at +0x3e0).
 *   tower: +0x3f4 follow, +0x3f8 / +0x3fc / +0x400 the fov spring {value, target, speed}, +0x404 / +0x408 / +0x40c the height spring,
 *     +0x410 a debug colour. zip line: +0x400 / +0x410 the line's ends, +0x420 the camera position, +0x434 the fov spring, +0x440 the height
 *     spring. spline: +0x430 .. +0x470 five control points, +0x400 the camera position, +0x410 (double) the curve parameter, +0x418
 *     (double) the sampling step, +0x420 the chord length, +0x484 the fov spring, +0x490 the height spring, +0x4a0 the sampled curve (100
 *     points of 16 bytes). animated: +0x3c0 its MXCameraPath, +0x3f8 loop flag, +0x3fc speed, +0x400 clock, +0x404 maximum step, +0x410 the
 *     position, +0x420 its scale, +0x430 the height spring, +0x43c the fov spring.
 * Every vf37 returns its director value as the x87 float. Product boundary: none (everything is ported). */
#include "loop_cam_replay.h"
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_cam_misc.h"
#include "loop_cam_player.h"
#include "loop_platform.h"
#include "loop_util.h"
#include "build_scene_int.h"
#include "build_mxworld.h"
#include "build_alloc.h"
#include "build_activity_util.h"
#include "build_activity_freeride_run.h"
#include "anim_rider_dtors.h"
#include "race.h"
#include "race_driver.h"
#include "fx.h"
#include "crt.h"

/* ======================================================================================================================== */
/* helpers                                                                                                                  */

/* the free the compiler inlined into the scalar deleting destructors: _aligned_free between the allocator lock's enter / leave while
 * [0xda67d8] is set (as anim_entity_dtors.c) */
static void delete_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    ACT_CALL(1, (SU32(rx_sp(), 4, p), crt_aligned_free(p)));
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
}
/* ReflexAlignedFree (0078fc50) of the entity itself, with the flag bit 0 of a scalar deleting destructor */
static void engine_free(rx_addr p) {
    ACT_CALL(1, (SU32(rx_sp(), 4, p), rx_reflex_aligned_free(p)));
}

/* 0065c9c0 (ESI = list; stack capacity, growth; ret 8): an empty growable list of 16-byte elements: the block (capacity * 16 bytes, the
 * size saturating on overflow) from _aligned_malloc, count 0, capacity, growth, the maximum 0x7fffffff */
static void list16_init(rx_addr list, uint32_t capacity, uint32_t growth) {
    const rx_addr data = rx_reflex_aligned_alloc(rx_mul_sat(capacity, 16));
    SU32(list, 0, data);
    SU32(list, 8, capacity);
    SU32(list, 4, 0);
    SU32(list, 0xc, growth);
    SU32(list, 0x10, 0x7fffffffu);
}

/* the scene-wide values the replay cameras share: 0.01745329 (degrees to radians, 0x977678) */
static float deg2rad(float degrees) { return degrees * rx_bf(0x3c8efa34u) /* 0x977678 */; }
/* 30 degrees in radians (0x977a10) */
#define FOV_30_DEGREES 0x3f060a91u

/* the world of the running mode (the three-way test of FUN_00648990 / 0075c530 on the engine scenes' running flags) -> the world, or 0 */
static rx_addr mode_world(void) {
    const rx_addr ctx = U32(0, 0x00a95844);
    if (U32(U32(ctx, 0x20), 0x34) == 1) return U32(U32(0, 0x00a946b8), 0x5a54);
    if (U32(U32(ctx, 0x1c), 0x34) == 1) return U32(U32(0, 0x00a946b8), 0x20c8);
    return 0;
}

/* The terrain object of the replay cameras' debug draw: the entry of the static table 0x9e30d4 (count 0x9e30dc, first use: emptied) whose
 * +0x44a8 is the running world's scene manager (+0x20138), or 0. */
static rx_addr debug_terrain(void) {
    const rx_addr scene = U32(mode_world(), 0x20138);
    uint32_t count;
    if (!(U8(0, 0x009e30e0) & 1)) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        count = 0;
        SU32(0, 0x009e30dc, 0);
    } else {
        count = U32(0, 0x009e30dc);
    }
    for (uint32_t k = 0; k < count; k++) {
        const rx_addr t = U32(0, 0x009e30d4 + 4 * k);
        if (t != 0 && scene == U32(t, 0x44a8)) return t;
    }
    return 0;
}
/* 007c5950 (thiscall ECX = terrain; stack point; ret 4): the point snapped onto the terrain's base height. The replay cameras' debug draw
 * snaps stack copies of their points, nothing else reads them. */
static void snap_to_ground(rx_addr terrain, rx_addr point, rx_addr ret) {
    uint32_t edx;
    const uint32_t a[1] = {point};
    const rx_addr s = loop_push(1, a, ret);
    phys_terrain_base_height_at(terrain, point, &edx);
    loop_pop(s);
}

/* the focus point of the camera's controller raised by 3 (006489e0), the y of it */
static float focus_height(void) {
    const rx_addr p = rx_replay_focus_point();
    return F32(p, 4);
}

/* The vf28 sequence of the four classes (the base's reset: pose valid, the drift spring cleared) and the two height words the class keeps:
 * each is the focus point's y (the focus point recomputed for each) */
static void reset_springs(rx_addr cam) {
    SU32(cam, 0x2f4, 1);
    SF32(cam, 0x340, 0.0f);
    SF32(cam, 0x344, 0.0f);
    SF32(cam, 0x348, 0.0f);
    SF32(cam, 0x350, 0.0f);
    SF32(cam, 0x354, 0.0f);
    SF32(cam, 0x358, 0.0f);
}

/* the fov of the first range whose radius squared exceeds the squared distance (x87 copy to `target`); unchanged beyond the last range */
static void select_fov(rx_addr cam, float d2, uint32_t range0, uint32_t fov0, uint32_t target) {
    const float r0 = F32(cam, range0), r1 = F32(cam, range0 + 4), r2 = F32(cam, range0 + 8);
    if (r0 * r0 > d2) cam_x87_copy(cam + target, cam + fov0);
    else if (r1 * r1 > d2) cam_x87_copy(cam + target, cam + fov0 + 4);
    else if (r2 * r2 > d2) cam_x87_copy(cam + target, cam + fov0 + 8);
}

/* The squared distance from the camera (position `pos`) to the point (px, py, pz): (dz^2 + dy^2) + dx^2 */
static float dist2(float px, float py, float pz, float lx, float ly, float lz) {
    const float dx = px - lx, dy = py - ly, dz = pz - lz;
    return ((dz * dz) + (dy * dy)) + (dx * dx);
}

/* ======================================================================================================================== */
/* the base class MXReplayCamera                                                                                            */

/* 006485e0 (ESI = the camera; the camera entity base 0063d930 takes it as the pushed stack word): the vtable 0x95a504 and the defaults.
 * Frame: push ebp; and esp, -16; push esi. -> EAX = the camera. */
rx_addr rx_replay_camera_construct(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu;
    rx_set_sp(A);
    {
        const uint32_t a[1] = {cam};
        const rx_addr s = loop_push(1, a, 0x006485e7);
        rx_camera_entity_construct(cam);
        loop_pop(s);
    }
    SU32(cam, 0, 0x0095a504);
    SF32(cam, 0x360, 0.0f);
    SF32(cam, 0x364, 0.0f);
    SF32(cam, 0x368, 0.0f);
    SU32(cam, 0x310, 0);                              /* the director list's links */
    SU32(cam, 0x314, 0);
    SF32(cam, 0x320, 1.0f /* 0x931f24 */);
    SF32(cam, 0x32c, 1000.0f /* 0x9cf5b0 */);
    SU32(cam, 0x334, 0);
    SF32(cam, 0x31c, 0.0f);
    SF32(cam, 0x324, 0.0f);
    SF32(cam, 0x328, 0.0f);
    SF32(cam, 0x318, 0.0f);
    SF32(cam, 0x330, 10.0f /* 0x92fd88 */);
    SF32(cam, 0x340, 0.0f);
    SF32(cam, 0x344, 0.0f);
    SF32(cam, 0x348, 0.0f);
    SF32(cam, 0x350, 0.0f);
    SF32(cam, 0x354, 0.0f);
    SF32(cam, 0x358, 0.0f);
    SU8(cam, 0x370, 0);
    SU32(cam, 0x70, 1);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x006485e0, rx_replay_camera_construct)

/* 006486c0 MXReplayCamera::sdtor (vf0; thiscall, flags; ret 4) -> this: the vtable 0x95a504, the camera entity base (0063da30), freed with
 * bit 0 (ReflexAlignedFree). Frame: push esi. */
rx_addr rx_replay_camera_sdtor(rx_addr cam, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    SU32(cam, 0, 0x0095a504);
    ACT_CALL(0, rx_camera_entity_destroy(cam));
    if (flags & 1) engine_free(cam);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x006486c0, rx_replay_camera_sdtor)

/* 006486f0 MXReplayCamera::vf5 (thiscall, the reader; ret 4): Entity::vf5, then the fields the camera's RSData sets. AngleOfError turns
 * into its tangent through the game's inline sine / cosine (the sine from the polynomial in y = pi - (|x| mod 2pi) at 0x935d54 ..
 * 0x935d68, the cosine as -0.0 minus the polynomial of cam_trig_poly), and the entity's matrix (vf14) is kept as the authored pose
 * +0x380. */
uint32_t rx_replay_camera_vf5(rx_addr cam, uint32_t reader) {
    (void)reader;
    rx_entity_vf5(cam);
    if (rs_has("Sloppyness", 0)) SF32(cam, 0x31c, rs_float("Sloppyness"));
    if (rs_has("Spaz", 0)) SF32(cam, 0x320, rs_float("Spaz"));
    if (rs_has("FrequencyOfError", 0)) SF32(cam, 0x324, rs_float("FrequencyOfError"));
    if (rs_has("AngleOfError", 0)) {
        const float angle = rs_float("AngleOfError") * rx_bf(0x3c8efa34u) /* 0x977678 */;
        const float sign = angle >= 0.0f ? 1.0f /* 0x931f24 */ : -1.0f /* 0x9cd9e8 */;     /* comiss; jb */
        const float magnitude = sign * angle;
        const float y = 3.1415927f /* 0x977498 */ - (magnitude - ((float)rx_cvtt(magnitude * 0.15915494f /* 0x9774a0 */) * 6.2831855f /* 0x97749c */));
        const float y2 = y * y, y4 = y2 * y2;
        const float cos_poly = ((((y2 * -2.197296e-07f /* 0x935d50 */ + 2.420294e-05f /* 0x935d48 */) * y4) +
                                 (y2 * -0.001385879f /* 0x935d44 */ + 0.04165978f /* 0x935d4c */)) * y4) +
                               (y2 * -0.4999942f /* 0x935d40 */ + 0.9999992f /* 0x935d3c */);
        const float cosine = -0.0f /* 0x9774a8 */ - cos_poly;
        const float sin_poly = ((((y2 * -2.036224e-08f /* 0x935d68 */ + 2.699715e-06f /* 0x935d64 */) * y4) +
                                 (y2 * -0.0001980863f /* 0x935d60 */ + 0.008332403f /* 0x935d5c */)) * y4) +
                               (y2 * -0.1666655f /* 0x935d58 */ + 0.9999996f /* 0x935d54 */);
        const float sine = (sin_poly * y) * sign;
        SF32(cam, 0x328, rx_divs(sine, cosine));
    }
    if (rs_has("MaxVelocity", 0)) SF32(cam, 0x32c, rs_float("MaxVelocity"));
    if (rs_has("MaxVelScale", 0)) SF32(cam, 0x330, rs_float("MaxVelScale"));
    if (rs_has("OrderIndex", 0)) SU32(cam, 0x334, rs_uint("OrderIndex"));
    const rx_addr matrix = loop_tc0(loop_vslot(cam, 0x38), cam);
    scene_copy_words(cam + 0x380, matrix, 16);
    return 0;
}
RX_FUNCTION(0x006486f0, rx_replay_camera_vf5)

/* 00648a40 MXReplayCamera::vf37 -> st(0): the director's value of a camera without an activation area: FLT_MAX (0x977494) */
float rx_replay_camera_vf37(rx_addr cam) {
    (void)cam;
    return rx_bf(0x7f7fffffu);
}
RX_FUNCTION(0x00648a40, rx_replay_camera_vf37)

/* 00648a50 MXReplayCamera::vf28 (thiscall; ret 4): pose valid, the drift vector spring cleared */
void rx_replay_camera_vf28(rx_addr cam, uint32_t arg) {
    (void)arg;
    reset_springs(cam);
}
RX_FUNCTION(0x00648a50, rx_replay_camera_vf28)

/* 00655880 MXReplayCamera::vf38 (thiscall): two updates (vf19, vtable +0x4c) of one second each: the camera settled on its first view */
void rx_replay_camera_vf38(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    loop_tc_f(loop_vslot(cam, 0x4c), cam, 1.0f);
    loop_tc_f(loop_vslot(cam, 0x4c), cam, 1.0f);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00655880, rx_replay_camera_vf38)

/* ======================================================================================================================== */
/* the shared activation-area test                                                                                          */

/* 0063d060 (stack a; EAX = b, EDX = c, ESI = n, EDI = p; plain ret) -> EAX: whether the point p lies inside the prism of the triangle
 * (a, b, c) along the vector n: the three edge tests n . ((q - e0) x (e1 - e0)) of the edges a -> b, b -> c, c -> a, each by the sign BIT
 * of the float (integer compare with 0: -0.0 and negative NaNs count as outside). */
uint32_t rx_point_in_triangle_prism(rx_addr a, rx_addr b, rx_addr c, rx_addr n, rx_addr p) {
    const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8);
    const float bx = F32(b, 0), by = F32(b, 4), bz = F32(b, 8);
    const float cx = F32(c, 0), cy = F32(c, 4), cz = F32(c, 8);
    const float nx = F32(n, 0), ny = F32(n, 4), nz = F32(n, 8);
    const float px = F32(p, 0), py = F32(p, 4), pz = F32(p, 8);
    /* edge a -> b */
    {
        const float ex = bx - ax, ey = by - ay, ez = bz - az;
        const float qx = px - ax, qy = py - ay, qz = pz - az;
        const float kx = (qz * ey) - (qy * ez);
        const float ky = (qx * ez) - (qz * ex);
        const float kz = (qy * ex) - (qx * ey);
        const float d = ((nx * kx) + (ny * ky)) + (nz * kz);
        if ((int32_t)rx_fb(d) < 0) return 0;
    }
    /* edge b -> c */
    {
        const float ex = cx - bx, ey = cy - by, ez = cz - bz;
        const float qx = px - bx, qy = py - by, qz = pz - bz;
        const float kx = (qz * ey) - (qy * ez);
        const float ky = (ez * qx) - (qz * ex);
        const float kz = (qy * ex) - (ey * qx);
        const float d = ((ny * ky) + (nx * kx)) + (nz * kz);
        if ((int32_t)rx_fb(d) < 0) return 0;
    }
    /* edge c -> a */
    const float ex = ax - cx, ey = ay - cy, ez = az - cz;
    const float qx = px - cx, qy = py - cy, qz = pz - cz;
    const float kx = (qz * ey) - (qy * ez);
    const float ky = (ez * qx) - (qz * ex);
    const float kz = (qy * ex) - (ey * qx);
    const float d = ((ny * ky) + (nx * kx)) + (nz * kz);
    return (int32_t)rx_fb(d) >= 0;
}
RX_FUNCTION(0x0063d060, rx_point_in_triangle_prism)

/* The director's value of the classes with an ActivationArea (the vf37 loop of the tower, spline, zip line and animated cameras): the
 * triangles are the list's elements three at a time (0 1 2, 3 4 5 ...); the player's point P (frame word `fp`) inside any of them gives
 * 0.0, else FLT_MAX. The original copies each triangle's vertices to stack words (v0 / v1 / v2, the vector (0, 1, 0) at `fn`) and tests
 * with 0063d060. */
typedef struct { uint32_t p, v0, v1, v2, n; } prism_frame;
static float activation_value(rx_addr E, rx_addr list, int32_t count, const prism_frame* L) {
    for (int32_t i = 2; i < count; i += 3) {
        const rx_addr t0 = list + 16u * (uint32_t)(i - 2), t1 = t0 + 16u, t2 = t0 + 32u;
        SF32(E, L->n, 0.0f);
        SF32(E, L->n + 4, 1.0f /* 0x931f24 */);
        SF32(E, L->n + 8, 0.0f);
        SU32(E, L->v0 + 4, U32(t0, 4));
        SU32(E, L->v0 + 8, U32(t0, 8));
        SU32(E, L->v0, U32(t0, 0));
        SU32(E, L->v1 + 4, U32(t1, 4));
        SU32(E, L->v1 + 8, U32(t1, 8));
        SU32(E, L->v1, U32(t1, 0));
        SU32(E, L->v2, U32(t2, 0));
        SU32(E, L->v2 + 4, U32(t2, 4));
        SU32(E, L->v2 + 8, U32(t2, 8));
        const uint32_t a[1] = {E + L->v0};
        const rx_addr s = loop_push(1, a, 0);
        const uint32_t inside = rx_point_in_triangle_prism(E + L->v0, E + L->v1, E + L->v2, E + L->n, E + L->p);
        loop_pop(s);
        if (inside) return 0.0f;
    }
    return rx_bf(0x7f7fffffu) /* 0x977494 */;
}

/* ======================================================================================================================== */
/* MXReplayTowerCamera (0x1046)                                                                                             */

/* 00648cc0 (EAX = the camera) -> EAX: the base constructor, then the tower's defaults: fovs 30 degrees, ranges 20 / 40 / 60, fov
 * acceleration 50, follow on, and the debug colour from the 28-entry table 0x9c4a80 by the counter 0xa94190 (counter % 0x70 / 4) which
 * then advances. Frame: push esi. */
rx_addr rx_replay_tower_camera_construct(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_replay_camera_construct(cam));
    const uint32_t counter = U32(0, 0x00a94190);
    SU32(cam, 0, 0x0095ab04);
    for (uint32_t k = 0x3e0; k <= 0x3f0; k += 4) SU32(cam, k, 0);
    SF32(cam, 0x404, 0.0f);
    SF32(cam, 0x408, 0.0f);
    SF32(cam, 0x40c, 0.0f);
    SU32(cam, 0x3dc, 0);
    SF32(cam, 0x3cc, 20.0f /* 0x935aac */);
    SF32(cam, 0x3d0, 40.0f /* 0x9cc37c */);
    SF32(cam, 0x3d4, 60.0f /* 0x931da0 */);
    SU32(cam, 0x3c0, FOV_30_DEGREES);
    SU32(cam, 0x3c4, FOV_30_DEGREES);
    SU32(cam, 0x3c8, FOV_30_DEGREES);
    SF32(cam, 0x3d8, 50.0f /* 0x9d78dc */);
    SU32(cam, 0x3f4, 1);
    SU32(cam, 0x3fc, FOV_30_DEGREES);
    SU32(cam, 0x3f8, FOV_30_DEGREES);
    SF32(cam, 0x400, 0.0f);
    SU32(cam, 0x410, U32(0, 0x009c4a80 + ((counter % 0x70u) & ~3u)));
    SU32(0, 0x00a94190, counter + 1);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x00648cc0, rx_replay_tower_camera_construct)

/* 004046a0 case 0x1046: ReflexAlignedAlloc(0x430) and the constructor */
rx_addr rx_replay_tower_camera_create(void) {
    rx_addr cam = rx_reflex_aligned_alloc(0x430);
    return cam != 0 ? rx_replay_tower_camera_construct(cam) : 0;
}

/* The fov, range and ActivationArea fields the four classes read (the tower's layout; the animated camera's is shifted one word up):
 * CloseFOV / MidFOV / FarFOV (degrees) at `fov`, CloseRange / MidRange / FarRange at `fov + 0xc`, FOVTransitionSpeed at `fov + 0x18`,
 * then the ActivationArea: its Vector3 list (first 24 elements, growth 12) at `list`, and the triangle count (a third of the list's
 * count, the signed division by 3 of imul 0x55555556) at `triangles`. */
static void read_area_props(rx_addr cam, uint32_t fov, uint32_t list, uint32_t triangles) {
    if (rs_has("CloseFOV", 0)) SF32(cam, fov + 0x00, deg2rad(rs_float("CloseFOV")));
    if (rs_has("MidFOV", 0)) SF32(cam, fov + 0x04, deg2rad(rs_float("MidFOV")));
    if (rs_has("FarFOV", 0)) SF32(cam, fov + 0x08, deg2rad(rs_float("FarFOV")));
    if (rs_has("CloseRange", 0)) SF32(cam, fov + 0x0c, rs_float("CloseRange"));
    if (rs_has("MidRange", 0)) SF32(cam, fov + 0x10, rs_float("MidRange"));
    if (rs_has("FarRange", 0)) SF32(cam, fov + 0x14, rs_float("FarRange"));
    if (rs_has("FOVTransitionSpeed", 0)) SF32(cam, fov + 0x18, rs_float("FOVTransitionSpeed"));
    if (rs_has("ActivationArea", 0)) {
        list16_init(cam + list, 0x18, 0xc);
        scene_rs_vec3_list("ActivationArea", cam + list);
        SU32(cam, triangles, (uint32_t)(I32(cam, list + 4) / 3));
    }
}

/* 00648f50 (EDI = the camera): the tower's RSData fields: the area fields (fov at +0x3c0, list at +0x3e0, triangle count +0x3dc) and
 * FollowCamera (Bool) into +0x3f4 */
void rx_replay_tower_camera_props(rx_addr cam) {
    read_area_props(cam, 0x3c0, 0x3e0, 0x3dc);
    if (rs_has("FollowCamera", RS_BOOL)) SU32(cam, 0x3f4, rs_bool("FollowCamera"));
}
RX_FUNCTION(0x00648f50, rx_replay_tower_camera_props)

/* 00648e40 MXReplayTowerCamera::vf5 (thiscall, the reader; ret 4): the base's fields, then the tower's */
uint32_t rx_replay_tower_camera_vf5(rx_addr cam, uint32_t reader) {
    rx_replay_camera_vf5(cam, reader);
    rx_replay_tower_camera_props(cam);
    return 0;
}
RX_FUNCTION(0x00648e40, rx_replay_tower_camera_vf5)

/* 00648e60 MXReplayTowerCamera::vf28 (thiscall; ret 4): pose valid, the drift spring cleared, and both heights of the height spring
 * (+0x404 value and +0x408 target) at the focus point's raised y. Frame: push ebp; and esp, -16; sub esp, 0xc; push esi. */
void rx_replay_tower_camera_vf28(rx_addr cam, uint32_t arg) {
    (void)arg;
    reset_springs(cam);
    SF32(cam, 0x404, focus_height());
    SF32(cam, 0x408, focus_height());
}
RX_FUNCTION(0x00648e60, rx_replay_tower_camera_vf28)

/* 00654720 (MXReplayTowerCamera::vf37 and MXSplineCamera::vf37) -> st(0): 0.0 while the controller's point (+0xc0 of the controller) lies
 * inside one of the ActivationArea's triangles (+0x3e0 data, +0x3e4 count), else FLT_MAX. Frame: push ebp; and esp, -16; sub esp, 0x64;
 * push ebx / esi / edi: E = the aligned base - 0x70 (the point at +0x60, the vertices at +0x20 / +0x40 / +0x30, the vector at +0x50). */
float rx_replay_tower_camera_vf37(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x70u;
    rx_set_sp(E);
    const rx_addr c = rx_replay_camera_controller() + 0xc0u;
    SU32(E, 0x60, U32(c, 0));
    SU32(E, 0x64, U32(c, 4));
    SU32(E, 0x68, U32(c, 8));
    static const prism_frame layout = {0x60, 0x20, 0x40, 0x30, 0x50};
    const float v = I32(cam, 0x3e4) > 2 ? activation_value(E, U32(cam, 0x3e0), I32(cam, 0x3e4), &layout) : rx_bf(0x7f7fffffu);
    rx_set_sp(sp0);
    return v;
}
RX_FUNCTION(0x00654720, rx_replay_tower_camera_vf37)

/* 00649470 MXReplayTowerCamera::vf39 and MXZipLineCamera::vf39 (thiscall): the debug draw of the activation area: the triangles' points
 * and the camera's own position snapped onto the terrain of the running scene (stack copies; nothing else reads them). Frame: push ebp;
 * and esp, -16; sub esp, 0x44; push ebx / esi / edi: E = the aligned base - 0x50. */
void rx_replay_activation_vf39(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x50u;
    rx_set_sp(E);
    const rx_addr terrain = debug_terrain();
    const int32_t count = I32(cam, 0x3e4);
    for (int32_t i = 2; count > 2 && i < count; i += 3) {
        const rx_addr t = U32(cam, 0x3e0) + 16u * (uint32_t)(i - 2);
        for (uint32_t v = 0; v < 3; v++) {
            const rx_addr dst = E + 0x20u + 0x10u * v;
            SU32(dst, 0, U32(t + 16u * v, 0));
            SU32(dst, 4, U32(t + 16u * v, 4));
            SU32(dst, 8, U32(t + 16u * v, 8));
        }
        if (terrain != 0) {
            snap_to_ground(terrain, E + 0x20u, 0x0064958e);
            snap_to_ground(terrain, E + 0x30u, 0x0064959a);
            snap_to_ground(terrain, E + 0x40u, 0x006495ab);
        }
    }
    const rx_addr m = loop_tc0(loop_vslot(cam, 0x38), cam);
    SU32(E, 0x10, U32(m, 0x30));
    SU32(E, 0x14, U32(m, 0x34));
    SU32(E, 0x18, U32(m, 0x38));
    if (terrain != 0) snap_to_ground(terrain, E + 0x10u, 0x006495f9);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00649470, rx_replay_activation_vf39)

/* ======================================================================================================================== */
/* MXZipLineCamera (0x1047)                                                                                                 */

/* 006534f0 (EAX = the camera) -> EAX: the base constructor and the zip line's defaults (fovs 30 degrees, ranges 20 / 40 / 60, fov speed
 * 50; the line's ends (10, 5, 0) and (-10, 5, 0), the fov spring at 30 degrees, the height spring at 5). Frame: push ebp; and esp, -16;
 * sub esp, 0xc; push esi. */
rx_addr rx_zipline_camera_construct(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_replay_camera_construct(cam));
    SU32(cam, 0, 0x0095abac);
    for (uint32_t k = 0x3e0; k <= 0x3f0; k += 4) SU32(cam, k, 0);
    for (uint32_t k = 0x434; k <= 0x448; k += 4) SF32(cam, k, 0.0f);
    SU32(cam, 0x3c0, FOV_30_DEGREES);
    SU32(cam, 0x3c4, FOV_30_DEGREES);
    SU32(cam, 0x3c8, FOV_30_DEGREES);
    SF32(cam, 0x3cc, 20.0f /* 0x935aac */);
    SF32(cam, 0x3d0, 40.0f /* 0x9cc37c */);
    SF32(cam, 0x3d4, 60.0f /* 0x931da0 */);
    SF32(cam, 0x3d8, 50.0f /* 0x9d78dc */);
    SU32(cam, 0x3dc, 0);
    SF32(cam, 0x400, 10.0f /* 0x92fd88 */);
    SF32(cam, 0x404, 5.0f /* 0x935890 */);
    SF32(cam, 0x408, 0.0f);
    SF32(cam, 0x410, -10.0f /* 0x9d78b8 */);
    SF32(cam, 0x414, 5.0f);
    SF32(cam, 0x418, 0.0f);
    SF32(cam, 0x420, 0.0f);
    SF32(cam, 0x424, 0.0f);
    SF32(cam, 0x428, 0.0f);
    SU32(cam, 0x438, U32(cam, 0x3c4));
    SU32(cam, 0x434, U32(cam, 0x3c4));
    SF32(cam, 0x43c, 0.0f);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x006534f0, rx_zipline_camera_construct)

/* case 0x1047: ReflexAlignedAlloc(0x450) and the constructor */
rx_addr rx_zipline_camera_create(void) {
    rx_addr cam = rx_reflex_aligned_alloc(0x450);
    return cam != 0 ? rx_zipline_camera_construct(cam) : 0;
}

/* 00653670 (EAX = the camera): the zip line vtable, the ActivationArea block (+0x3e0) freed under the allocator lock, the base vtable
 * 0x95a504 and the camera entity base (0063da30). Frame: push esi / edi. */
void rx_zipline_camera_destroy(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8u);
    const rx_addr block = U32(cam, 0x3e0);
    SU32(cam, 0, 0x0095abac);
    delete_locked(block);
    SU32(cam, 0, 0x0095a504);
    ACT_CALL(0, rx_camera_entity_destroy(cam));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00653670, rx_zipline_camera_destroy)

/* 00653650 MXZipLineCamera::sdtor (thiscall, flags; ret 4) -> this. Frame: push esi. */
rx_addr rx_zipline_camera_sdtor(rx_addr cam, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_zipline_camera_destroy(cam));
    if (flags & 1) engine_free(cam);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x00653650, rx_zipline_camera_sdtor)

/* 006537d0 (EDI = the camera): the area fields and the line: ZipLinePoints (a list of two Vector3, the stack list capacity 2, growth 2)
 * gives the line's ends +0x400 and +0x410 (movss) and the camera's starting position +0x420 (x87 copy of +0x400); the list's block is
 * freed under the allocator lock. Frame: push ebp; and esp, -16; sub esp, 0x18; push ebx / esi: E = the aligned base - 0x20 (the list at
 * +0xc). */
void rx_zipline_camera_props(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x20u;
    rx_set_sp(E);
    read_area_props(cam, 0x3c0, 0x3e0, 0x3dc);
    if (rs_has("ZipLinePoints", 0)) {
        const rx_addr list = E + 0xcu;
        for (uint32_t k = 0; k < 5; k++) SU32(list, 4 * k, 0);
        list16_init(list, 2, 2);
        scene_rs_vec3_list("ZipLinePoints", list);
        const rx_addr pts = U32(list, 0);
        SU32(cam, 0x400, U32(pts, 0x00));
        SU32(cam, 0x404, U32(pts, 0x04));
        SU32(cam, 0x408, U32(pts, 0x08));
        SU32(cam, 0x410, U32(pts, 0x10));
        SU32(cam, 0x414, U32(pts, 0x14));
        SU32(cam, 0x418, U32(pts, 0x18));
        cam_x87_copy(cam + 0x420, cam + 0x400);
        cam_x87_copy(cam + 0x424, cam + 0x404);
        cam_x87_copy(cam + 0x428, cam + 0x408);
        delete_locked(pts);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006537d0, rx_zipline_camera_props)

/* 006536c0 MXZipLineCamera::vf5 (thiscall, the reader; ret 4) */
uint32_t rx_zipline_camera_vf5(rx_addr cam, uint32_t reader) {
    rx_replay_camera_vf5(cam, reader);
    rx_zipline_camera_props(cam);
    return 0;
}
RX_FUNCTION(0x006536c0, rx_zipline_camera_vf5)

/* 006536e0 MXZipLineCamera::vf28 (thiscall; ret 4): as the tower's with the heights at +0x440 and +0x444 */
void rx_zipline_camera_vf28(rx_addr cam, uint32_t arg) {
    (void)arg;
    reset_springs(cam);
    SF32(cam, 0x440, focus_height());
    SF32(cam, 0x444, focus_height());
}
RX_FUNCTION(0x006536e0, rx_zipline_camera_vf28)

/* 00653aa0 MXZipLineCamera::vf37 -> st(0): the controller's point P (+0xc0); the closest point of the line (+0x400 .. +0x410) to it is
 * the camera's position +0x420 (00407430); then the activation triangles as the tower's: 0.0 inside one, else FLT_MAX. Frame: push ebp;
 * and esp, -16; sub esp, 0x64; push ebx / esi / edi: E = the aligned base - 0x70 (P at +0x20, the closest point at +0x30, the vertices at
 * +0x30 / +0x50 / +0x40, the vector at +0x60). */
float rx_zipline_camera_vf37(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x70u;
    rx_set_sp(E);
    const rx_addr c = rx_replay_camera_controller() + 0xc0u;
    SU32(E, 0x20, U32(c, 0));
    SU32(E, 0x24, U32(c, 4));
    SU32(E, 0x28, U32(c, 8));
    {
        const uint32_t a[1] = {cam + 0x400u};
        const rx_addr s = loop_push(1, a, 0x00653af6);
        rx_closest_point_on_segment(E + 0x30u, E + 0x20u, cam + 0x410u, cam + 0x400u);
        loop_pop(s);
    }
    SU32(cam, 0x420, U32(E, 0x30));
    SU32(cam, 0x424, U32(E, 0x34));
    SU32(cam, 0x428, U32(E, 0x38));
    static const prism_frame layout = {0x20, 0x30, 0x50, 0x40, 0x60};
    const float v = I32(cam, 0x3e4) > 2 ? activation_value(E, U32(cam, 0x3e0), I32(cam, 0x3e4), &layout) : rx_bf(0x7f7fffffu);
    rx_set_sp(sp0);
    return v;
}
RX_FUNCTION(0x00653aa0, rx_zipline_camera_vf37)

/* 00653c30 MXZipLineCamera::vf19 (thiscall, dt; ret 4): with the pose valid: the replay camera's update, then the fov of the range the
 * controller's point P is in (+0x438) chased by the fov spring {+0x434, +0x438, +0x43c} (acceleration +0x3d8, rate 2, no speed cap) and
 * given to the Camera (+0xe0, matrix flags cleared); the height spring {+0x440 value, +0x444 target = P.y + 3, +0x448 speed} (100, no cap,
 * rate 10) chases P.y; the camera looks from its position on the line (+0x420) at (P.x, the height spring's value before the step, P.z).
 * Frame: push ebp; and esp, -16; sub esp, 0x2c; push esi: E = the aligned base - 0x30 (P at +0x10, the camera position at +0x20). */
void rx_zipline_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x30u;
    rx_set_sp(E);
    if (U32(cam, 0x2f4) == 0) {
        rx_set_sp(sp0);
        return;
    }
    {
        const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
        const rx_addr s = loop_push(1, a, 0x00653c55);
        rx_replay_camera_vf19(cam, rx_bf(a[0]));
        loop_pop(s);
    }
    const rx_addr c = rx_replay_camera_controller() + 0xc0u;
    SU32(E, 0x10, U32(c, 0));
    SU32(E, 0x14, U32(c, 4));
    SU32(E, 0x18, U32(c, 8));
    loop_tc1(loop_vslot(cam, 0x44), cam, E + 0x20u);
    select_fov(cam, dist2(F32(E, 0x10), F32(E, 0x14), F32(E, 0x18), F32(E, 0x20), F32(E, 0x24), F32(E, 0x28)), 0x3cc, 0x3c0, 0x438);
    rx_cam_spring(cam + 0x434, dt, F32(cam, 0x3d8), 999999.0f /* 0x977aa8 */, 2.0f /* 0x92e330 */);   /* inlined in the original */
    SU32(cam, 0x164, 0);
    SU32(cam, 0x16c, 0);
    SU32(cam, 0xe0, U32(cam, 0x434));
    SU32(cam, 0x168, 0);
    if (rx_replay_camera_controller() != 0) {
        SF32(cam, 0x444, F32(0, 0x009c4a7c) + F32(E, 0x14));
        SU32(E, 0x14, U32(cam, 0x440));
        rx_cam_spring(cam + 0x440, dt, 100.0f /* 0x93005c */, 999999.0f /* 0x977aa8 */, 10.0f /* 0x92fd88 */);   /* inlined */
        const uint32_t b[1] = {cam};
        const rx_addr s2 = loop_push(1, b, 0x00653f57);
        rx_cam_entity_look_at(cam + 0x420u, E + 0x10u, cam);
        loop_pop(s2);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00653c30, rx_zipline_camera_vf19)

/* ======================================================================================================================== */
/* MXSplineCamera (0x1048)                                                                                                  */

/* 00653f60 (EAX = the camera) -> EAX: the base constructor and the spline's defaults (fovs 30 degrees, ranges 20 / 40 / 60, fov speed 50;
 * five control points (20, 15, 0) (10, 15, 0) (0, 15, 0) (-10, 15, 0) (-20, 15, 0); the sampling step 0.01 and 100 sample points of 16
 * bytes (_aligned_malloc 0x640 under the allocator lock); the chord length). Frame: push ebp; and esp, -16; push ecx / ebx / esi / edi. */
rx_addr rx_spline_camera_construct(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_replay_camera_construct(cam));
    SU32(cam, 0, 0x0095ac54);
    for (uint32_t k = 0x3e0; k <= 0x3f0; k += 4) SU32(cam, k, 0);
    SF32(cam, 0x490, 0.0f);
    SF32(cam, 0x494, 0.0f);
    SF32(cam, 0x498, 0.0f);
    SF32(cam, 0x3d0, 40.0f /* 0x9cc37c */);
    SF32(cam, 0x3cc, 20.0f /* 0x935aac */);
    SF32(cam, 0x3d4, 60.0f /* 0x931da0 */);
    SF32(cam, 0x3d8, 50.0f /* 0x9d78dc */);
    SU32(cam, 0x3c0, FOV_30_DEGREES);
    SU32(cam, 0x3c4, FOV_30_DEGREES);
    SU32(cam, 0x3c8, FOV_30_DEGREES);
    SU32(cam, 0x3dc, 0);
    SF32(cam, 0x48c, 0.0f);
    SU32(cam, 0x488, FOV_30_DEGREES);
    SU32(cam, 0x484, FOV_30_DEGREES);
    SF32(cam, 0x430, 20.0f /* 0x935aac */);
    SF32(cam, 0x438, 0.0f);
    SF32(cam, 0x434, 15.0f /* 0x9322d0 */);
    SF32(cam, 0x440, 10.0f /* 0x92fd88 */);
    SF32(cam, 0x448, 0.0f);
    SF32(cam, 0x444, 15.0f);
    SF32(cam, 0x450, 0.0f);
    SF32(cam, 0x458, 0.0f);
    SF32(cam, 0x454, 15.0f);
    SF32(cam, 0x460, -10.0f /* 0x9d78b8 */);
    SF32(cam, 0x468, 0.0f);
    SF32(cam, 0x464, 15.0f);
    SF32(cam, 0x478, 0.0f);
    SF32(cam, 0x470, -20.0f /* 0x97780c */);
    SF32(cam, 0x474, 15.0f);
    SF32(cam, 0x400, 0.0f);
    SF32(cam, 0x404, 0.0f);
    SF32(cam, 0x408, 0.0f);
    SU32(cam, 0x410, 0);                                 /* movsd: the curve parameter, a double 0 */
    SU32(cam, 0x414, 0);
    SU32(cam, 0x418, 0x47ae147bu);                       /* movsd 0x977aa0: the sampling step, the double 0.01 */
    SU32(cam, 0x41c, 0x3f847ae1u);
    SU32(cam, 0x480, 0);
    SU32(cam, 0x4a0, rx_reflex_aligned_alloc(0x64u * 0x10u));
    {
        const float dz = F32(cam, 0x478) - F32(cam, 0x438);
        const float dy = F32(cam, 0x474) - F32(cam, 0x434);
        const float dx = F32(cam, 0x470) - F32(cam, 0x430);
        SF32(cam, 0x420, rx_sqrts(((dz * dz) + (dy * dy)) + (dx * dx)));
    }
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x00653f60, rx_spline_camera_construct)

/* case 0x1048: ReflexAlignedAlloc(0x4b0) and the constructor */
rx_addr rx_spline_camera_create(void) {
    rx_addr cam = rx_reflex_aligned_alloc(0x4b0);
    return cam != 0 ? rx_spline_camera_construct(cam) : 0;
}

/* 006541e0 (EAX = the camera): the spline vtable, the sample block (+0x4a0) and the ActivationArea block (+0x3e0) freed under the
 * allocator lock, the base vtable 0x95a504 and the camera entity base (0063da30). Frame: push ebx / ebp / esi / edi. */
void rx_spline_camera_destroy(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 16u);
    const rx_addr samples = U32(cam, 0x4a0);
    SU32(cam, 0, 0x0095ac54);
    delete_locked(samples);
    const rx_addr area = U32(cam, 0x3e0);
    delete_locked(area);
    SU32(cam, 0, 0x0095a504);
    ACT_CALL(0, rx_camera_entity_destroy(cam));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006541e0, rx_spline_camera_destroy)

/* 006541c0 MXSplineCamera::sdtor (thiscall, flags; ret 4) -> this. Frame: push esi. */
rx_addr rx_spline_camera_sdtor(rx_addr cam, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_spline_camera_destroy(cam));
    if (flags & 1) engine_free(cam);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x006541c0, rx_spline_camera_sdtor)

/* 00654380 (EDI = the camera): the area fields and the curve: ControlPoints (a list of five Vector3, the stack list capacity 5, growth 5)
 * gives the points +0x430 .. +0x470 (movss); the list's block is freed; the camera's starting position +0x400 is an x87 copy of the first
 * point and the chord length +0x420 the distance between the first and the last. Frame: push ebp; and esp, -16; sub esp, 0x18; push ebx /
 * esi: E = the aligned base - 0x20 (the list at +0xc). */
void rx_spline_camera_props(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x20u;
    rx_set_sp(E);
    read_area_props(cam, 0x3c0, 0x3e0, 0x3dc);
    if (rs_has("ControlPoints", 0)) {
        const rx_addr list = E + 0xcu;
        for (uint32_t k = 0; k < 5; k++) SU32(list, 4 * k, 0);
        list16_init(list, 5, 5);
        scene_rs_vec3_list("ControlPoints", list);
        const rx_addr pts = U32(list, 0);
        for (uint32_t i = 0; i < 5; i++) {
            SU32(cam, 0x430 + 0x10 * i, U32(pts, 0x10 * i));
            SU32(cam, 0x434 + 0x10 * i, U32(pts, 0x10 * i + 4));
            SU32(cam, 0x438 + 0x10 * i, U32(pts, 0x10 * i + 8));
        }
        delete_locked(pts);
    }
    cam_x87_copy(cam + 0x400, cam + 0x430);
    cam_x87_copy(cam + 0x404, cam + 0x434);
    cam_x87_copy(cam + 0x408, cam + 0x438);
    {
        const float dz = F32(cam, 0x478) - F32(cam, 0x438);
        const float dy = F32(cam, 0x474) - F32(cam, 0x434);
        const float dx = F32(cam, 0x470) - F32(cam, 0x430);
        SF32(cam, 0x420, rx_sqrts(((dz * dz) + (dy * dy)) + (dx * dx)));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00654380, rx_spline_camera_props)

/* 00654270 MXSplineCamera::vf5 (thiscall, the reader; ret 4) */
uint32_t rx_spline_camera_vf5(rx_addr cam, uint32_t reader) {
    rx_replay_camera_vf5(cam, reader);
    rx_spline_camera_props(cam);
    return 0;
}
RX_FUNCTION(0x00654270, rx_spline_camera_vf5)

/* 00654290 MXSplineCamera::vf28 (thiscall; ret 4): as the tower's with the heights at +0x490 and +0x494 */
void rx_spline_camera_vf28(rx_addr cam, uint32_t arg) {
    (void)arg;
    reset_springs(cam);
    SF32(cam, 0x490, focus_height());
    SF32(cam, 0x494, focus_height());
}
RX_FUNCTION(0x00654290, rx_spline_camera_vf28)

/* 00655050 (LTCG: ECX = the camera, EAX = out vec3, xmm0 = u as a double): the quartic Bezier of the five control points at u (clamped to
 * [0, 1]) in doubles: p4 (1-u)^4 + p3 4 (1-u)^3 u + p2 6 (1-u)^2 u^2 + p1 4 (1-u) u^3 + p0 u^4, each component rounded to a float */
void rx_spline_curve_at(rx_addr cam, rx_addr out, double u) {
    if (u > 1.0) u = 1.0;
    else if (0.0 > u) u = 0.0;
    const double w = 1.0 - u;
    double w_u3 = w * u;                         /* (w u) u u */
    w_u3 = w_u3 * u;
    w_u3 = w_u3 * u;
    double u4 = u * u;                           /* ((u u) u) u */
    u4 = u4 * u;
    u4 = u4 * u;
    double w2 = w * w;
    double w3 = w2 * w;
    double w2_u2 = w2 * u;                       /* ((w^2) u) u * 6 */
    w2_u2 = w2_u2 * u;
    w2_u2 = w2_u2 * 6.0 /* 0x977668 */;
    double w4 = w3 * w;
    double w3_u = w3 * u;                        /* (w^3) u * 4 */
    w3_u = w3_u * 4.0 /* 0x977670 */;
    w_u3 = w_u3 * 4.0;
    for (uint32_t k = 0; k < 3; k++) {
        const double p0 = (double)F32(cam, 0x430 + 4 * k), p1 = (double)F32(cam, 0x440 + 4 * k), p2 = (double)F32(cam, 0x450 + 4 * k);
        const double p3 = (double)F32(cam, 0x460 + 4 * k), p4 = (double)F32(cam, 0x470 + 4 * k);
        double v = p4 * w4;
        v = v + (p3 * w3_u);
        v = v + (p2 * w2_u2);
        v = v + (p1 * w_u3);
        v = v + (p0 * u4);
        SF32(out, 4 * k, (float)v);
    }
}
RX_FUNCTION(0x00655050, rx_spline_curve_at)

/* 00654870 MXSplineCamera::vf19 (thiscall, dt; ret 4): with the pose valid: the replay camera's update; the fov of the range the focus
 * point (006489e0) is in chased by the fov spring {+0x484, +0x488, +0x48c} (acceleration +0x3d8, rate 2, no speed cap) and given to the
 * Camera (+0xe0). The closest point of the chord (+0x430 .. +0x470) to the camera's own position gives the parameter 1 - (its distance
 * from the first point / the chord length) (+0x410, a double) and the camera's position +0x400 = the curve at it (unless the chord is
 * zero); with a controller the height spring {+0x490, +0x494 = P.y + 3, +0x498} (100, rate 10) chases P.y and the camera looks from
 * its position at (P.x, the spring's value before the step, P.z) (P = the controller's +0xc0). Frame: push ebp; and esp, -16; sub esp,
 * 0x44; push ebx / esi / edi: E = the aligned base - 0x50 (the focus point at +0x20, the camera position at +0x30, the closest point and
 * the curve point at +0x40). */
void rx_spline_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x50u;
    rx_set_sp(E);
    if (U32(cam, 0x2f4) == 0) {
        rx_set_sp(sp0);
        return;
    }
    {
        const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
        const rx_addr s = loop_push(1, a, 0x00654898);
        rx_replay_camera_vf19(cam, rx_bf(a[0]));
        loop_pop(s);
    }
    const rx_addr focus = rx_replay_focus_point();
    SU32(E, 0x20, U32(focus, 0));
    SU32(E, 0x24, U32(focus, 4));
    SU32(E, 0x28, U32(focus, 8));
    loop_tc1(loop_vslot(cam, 0x44), cam, E + 0x30u);
    select_fov(cam, dist2(F32(E, 0x20), F32(E, 0x24), F32(E, 0x28), F32(E, 0x30), F32(E, 0x34), F32(E, 0x38)), 0x3cc, 0x3c0, 0x488);
    rx_cam_spring(cam + 0x484, dt, F32(cam, 0x3d8), 999999.0f /* 0x977aa8 */, 2.0f /* 0x92e330 */);   /* inlined in the original */
    SU32(cam, 0x164, 0);
    SU32(cam, 0x16c, 0);
    SU32(cam, 0x168, 0);
    SU32(cam, 0xe0, U32(cam, 0x484));
    {
        const uint32_t a[1] = {cam + 0x430u};
        const rx_addr s = loop_push(1, a, 0x00654a84);
        rx_closest_point_on_segment(E + 0x40u, E + 0x20u, cam + 0x470u, cam + 0x430u);
        loop_pop(s);
    }
    const float length = F32(cam, 0x420);
    const float cx = F32(E, 0x40) - F32(cam, 0x430), cy = F32(E, 0x44) - F32(cam, 0x434), cz = F32(E, 0x48) - F32(cam, 0x438);
    if (!(length == 0.0f /* 0x92e12c */)) {
        const uint32_t a[1] = {rx_fb(((cz * cz) + (cy * cy)) + (cx * cx))};
        const rx_addr s = loop_push(1, a, 0x00654aea);
        const float t = rx_sqrts(((cz * cz) + (cy * cy)) + (cx * cx)) / length;
        loop_pop(s);
        const float u = 1.0f /* 0x931f24 */ - t;
        rx_spline_curve_at(cam, E + 0x40u, (double)u);
        cam_x87_copy(cam + 0x400, E + 0x40u);
        cam_x87_copy(cam + 0x404, E + 0x44u);
        cam_x87_copy(cam + 0x408, E + 0x48u);
        const double du = (double)u;
        uint64_t bits;
        __builtin_memcpy(&bits, &du, 8);
        SU32(cam, 0x410, (uint32_t)bits);
        SU32(cam, 0x414, (uint32_t)(bits >> 32));
    }
    if (rx_replay_camera_controller() != 0) {
        const rx_addr c = rx_replay_camera_controller() + 0xc0u;
        SU32(E, 0x40, U32(c, 0));
        SU32(E, 0x48, U32(c, 8));
        const float target = F32(0, 0x009c4a7c) + F32(c, 4);
        SU32(E, 0x44, U32(cam, 0x490));
        SF32(cam, 0x494, target);
        rx_cam_spring(cam + 0x490, dt, 100.0f /* 0x93005c */, 999999.0f /* 0x977aa8 */, 10.0f /* 0x92fd88 */);   /* inlined */
        const uint32_t b[1] = {cam};
        const rx_addr s2 = loop_push(1, b, 0x00654c81);
        rx_cam_entity_look_at(cam + 0x400u, E + 0x40u, cam);
        loop_pop(s2);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00654870, rx_spline_camera_vf19)

/* 00654c90 MXSplineCamera::vf39 (thiscall): the debug draw: the curve sampled at u = step * i (i = 0 .. 1 / step - 1, u clamped to [0, 1]) in
 * doubles into the sample block (+0x4a0, 16 bytes a point), then the activation triangles and the camera's position snapped onto the
 * terrain as the tower's. Frame: push ebp; and esp, -16; sub esp, 0x44; push ebx / esi / edi: E = the aligned base - 0x50. */
void rx_spline_camera_vf39(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x50u;
    rx_set_sp(E);
    uint64_t bits = (uint64_t)U32(cam, 0x418) | ((uint64_t)U32(cam, 0x41c) << 32);
    double step;
    __builtin_memcpy(&step, &bits, 8);
    if (1.0 / step > 0.0) {
        uint32_t offset = 0;
        for (int32_t i = 0;; i++) {
            double u = step * (double)i;
            if (u > 1.0) u = 1.0;
            else if (0.0 > u) u = 0.0;
            const double u4 = ((u * u) * u) * u;
            const double w = 1.0 - u;
            double w_u3 = ((w * u) * u) * u;
            w_u3 = w_u3 * 4.0 /* 0x977670 */;
            const double w2 = w * w;
            double w2_u2 = (w2 * u) * u;
            w2_u2 = w2_u2 * 6.0 /* 0x977668 */;
            const double w3 = w2 * w;
            const double w4 = w3 * w;
            double w3_u = w3 * u;
            w3_u = w3_u * 4.0;
            float out[3];
            {   /* x: ((p3 + p1) + p4) + p2) + p0 */
                const double p1 = (double)F32(cam, 0x440), p3 = (double)F32(cam, 0x460), p4 = (double)F32(cam, 0x470);
                const double p2 = (double)F32(cam, 0x450), p0 = (double)F32(cam, 0x430);
                double v = p3 * w3_u;
                v = v + (p1 * w_u3);
                v = v + (p4 * w4);
                v = v + (p2 * w2_u2);
                v = v + (p0 * u4);
                out[0] = (float)v;
            }
            {   /* y: (((p2 + p0) + p3) + p1) + p4 */
                const double p0 = (double)F32(cam, 0x434), p2 = (double)F32(cam, 0x454), p3 = (double)F32(cam, 0x464);
                const double p1 = (double)F32(cam, 0x444), p4 = (double)F32(cam, 0x474);
                double v = p2 * w2_u2;
                v = v + (p0 * u4);
                v = v + (p3 * w3_u);
                v = v + (p1 * w_u3);
                v = v + (p4 * w4);
                out[1] = (float)v;
            }
            {   /* z: (((p3 + p1) + p4) + p2) + p0 */
                const double p3 = (double)F32(cam, 0x468), p1 = (double)F32(cam, 0x448), p4 = (double)F32(cam, 0x478);
                const double p2 = (double)F32(cam, 0x458), p0 = (double)F32(cam, 0x438);
                double v = p3 * w3_u;
                v = v + (p1 * w_u3);
                v = v + (p4 * w4);
                v = v + (p2 * w2_u2);
                v = v + (p0 * u4);
                out[2] = (float)v;
            }
            SF32(E, 0x10, out[0]);
            const rx_addr dst = U32(cam, 0x4a0) + offset;
            SF32(dst, 0, out[0]);
            SF32(dst, 8, out[2]);
            SF32(dst, 4, out[1]);
            offset += 0x10;
            bits = (uint64_t)U32(cam, 0x418) | ((uint64_t)U32(cam, 0x41c) << 32);
            __builtin_memcpy(&step, &bits, 8);
            if (!(1.0 / step > (double)(i + 1))) break;
        }
    }
    const rx_addr terrain = debug_terrain();
    const int32_t count = I32(cam, 0x3e4);
    for (int32_t i = 2; count > 2 && i < count; i += 3) {
        const rx_addr t = U32(cam, 0x3e0) + 16u * (uint32_t)(i - 2);
        for (uint32_t v = 0; v < 3; v++) {
            const rx_addr dst = E + 0x20u + 0x10u * v;
            SU32(dst, 0, U32(t + 16u * v, 0));
            SU32(dst, 4, U32(t + 16u * v, 4));
            SU32(dst, 8, U32(t + 16u * v, 8));
        }
        if (terrain != 0) {
            snap_to_ground(terrain, E + 0x20u, 0x00654fd1);
            snap_to_ground(terrain, E + 0x30u, 0x00654fdd);
            snap_to_ground(terrain, E + 0x40u, 0x00654fe9);
        }
    }
    const rx_addr m = loop_tc0(loop_vslot(cam, 0x38), cam);
    SU32(E, 0x10, U32(m, 0x30));
    SU32(E, 0x14, U32(m, 0x34));
    SU32(E, 0x18, U32(m, 0x38));
    if (terrain != 0) snap_to_ground(terrain, E + 0x10u, 0x0065503d);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00654c90, rx_spline_camera_vf39)

/* ======================================================================================================================== */
/* MXAnimatedReplayCamera (0x1049)                                                                                          */

/* 00655200 (EAX = the camera) -> EAX: the base constructor and the defaults (fovs 30 degrees at +0x3c4 .. +0x3cc, ranges 20 / 40 / 60,
 * fov speed 50, loop on, the speed 1, the maximum step 1000). Frame: push ebp; and esp, -16; sub esp, 0xc; push esi. */
rx_addr rx_animated_replay_camera_construct(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_replay_camera_construct(cam));
    SU32(cam, 0, 0x0095acfc);
    for (uint32_t k = 0x3e4; k <= 0x3f4; k += 4) SU32(cam, k, 0);
    SF32(cam, 0x430, 0.0f);
    SF32(cam, 0x434, 0.0f);
    SF32(cam, 0x438, 0.0f);
    SF32(cam, 0x3d0, 20.0f /* 0x935aac */);
    SF32(cam, 0x3d4, 40.0f /* 0x9cc37c */);
    SF32(cam, 0x3d8, 60.0f /* 0x931da0 */);
    SU32(cam, 0x3c4, FOV_30_DEGREES);
    SU32(cam, 0x3c8, FOV_30_DEGREES);
    SU32(cam, 0x3cc, FOV_30_DEGREES);
    SU32(cam, 0x3e0, 0);
    SF32(cam, 0x3dc, 50.0f /* 0x9d78dc */);
    SU32(cam, 0x440, FOV_30_DEGREES);
    SU32(cam, 0x43c, FOV_30_DEGREES);
    SF32(cam, 0x444, 0.0f);
    SU8(cam, 0x3f8, 1);
    SF32(cam, 0x410, 0.0f);
    SF32(cam, 0x414, 0.0f);
    SF32(cam, 0x418, 0.0f);
    SF32(cam, 0x420, 0.0f);
    SF32(cam, 0x424, 0.0f);
    SF32(cam, 0x428, 0.0f);
    SF32(cam, 0x400, 0.0f);
    SU32(cam, 0x3c0, 0);
    SF32(cam, 0x404, 1000.0f /* 0x9cf5b0 */);
    SF32(cam, 0x3fc, 1.0f /* 0x931f24 */);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x00655200, rx_animated_replay_camera_construct)

/* case 0x1049: ReflexAlignedAlloc(0x450) and the constructor */
rx_addr rx_animated_replay_camera_create(void) {
    rx_addr cam = rx_reflex_aligned_alloc(0x450);
    return cam != 0 ? rx_animated_replay_camera_construct(cam) : 0;
}

/* 00655360 (EAX = the camera): the animated vtable, the camera's path (+0x3c0) deleted through its vf0 (flag 1), the ActivationArea block
 * (+0x3e4) freed under the allocator lock, the base vtable and the camera entity base. Frame: push esi, edi. */
void rx_animated_replay_camera_destroy(rx_addr cam) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    const rx_addr path = U32(cam, 0x3c0);
    SU32(cam, 0, 0x0095acfc);
    if (path != 0) loop_tc1(loop_vslot(path, 0), path, 1);
    rx_set_sp(sp0 - 8u);
    const rx_addr area = U32(cam, 0x3e4);
    delete_locked(area);
    SU32(cam, 0, 0x0095a504);
    ACT_CALL(0, rx_camera_entity_destroy(cam));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00655360, rx_animated_replay_camera_destroy)

/* 00655340 MXAnimatedReplayCamera::sdtor (thiscall, flags; ret 4) -> this. Frame: push esi. */
rx_addr rx_animated_replay_camera_sdtor(rx_addr cam, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    ACT_CALL(0, rx_animated_replay_camera_destroy(cam));
    if (flags & 1) engine_free(cam);
    rx_set_sp(sp0);
    return cam;
}
RX_FUNCTION(0x00655340, rx_animated_replay_camera_sdtor)

/* 006554e0 (EDI = the camera): the area fields (one word up: fovs +0x3c4, ranges +0x3d0, speed +0x3dc, the list at +0x3e4 with the triangle
 * count at +0x3e0) and the AnimationPath: the asset's name becomes the clip of a new MXCameraPath (0x130 bytes, labelled by the entity's
 * "Name") kept at +0x3c0. Frame: push ecx / esi; the path is built with 0063e590. */
void rx_animated_replay_camera_props(rx_addr cam) {
    read_area_props(cam, 0x3c4, 0x3e4, 0x3e0);
    if (rs_has("AnimationPath", 0)) {
        const rx_addr handle = rs_asset("AnimationPath");
        char clip[0x40];
        for (uint32_t k = 0; k < 0x40; k++) clip[k] = (char)U8(handle, 4 + k);   /* AssetHandle::vf2: the handle's name (+4) */
        const char* label = rs_str("Name");
        const rx_addr path = rx_reflex_aligned_alloc(0x130);
        SU32(cam, 0x3c0, path != 0 ? rx_camera_path_construct_named(path, label, clip) : 0);
    }
}
RX_FUNCTION(0x006554e0, rx_animated_replay_camera_props)

/* 006553d0 MXAnimatedReplayCamera::vf5 (thiscall, the reader; ret 4) */
uint32_t rx_animated_replay_camera_vf5(rx_addr cam, uint32_t reader) {
    rx_replay_camera_vf5(cam, reader);
    rx_animated_replay_camera_props(cam);
    return 0;
}
RX_FUNCTION(0x006553d0, rx_animated_replay_camera_vf5)

/* 006553f0 MXAnimatedReplayCamera::vf28 (thiscall; ret 4): as the tower's with the heights at +0x430 and +0x434 */
void rx_animated_replay_camera_vf28(rx_addr cam, uint32_t arg) {
    (void)arg;
    reset_springs(cam);
    SF32(cam, 0x430, focus_height());
    SF32(cam, 0x434, focus_height());
}
RX_FUNCTION(0x006553f0, rx_animated_replay_camera_vf28)

/* 00655730 MXAnimatedReplayCamera::vf37 -> st(0): as the tower's with the area at +0x3e4 (data) / +0x3e8 (count) */
float rx_animated_replay_camera_vf37(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x70u;
    rx_set_sp(E);
    const rx_addr c = rx_replay_camera_controller() + 0xc0u;
    SU32(E, 0x60, U32(c, 0));
    SU32(E, 0x64, U32(c, 4));
    SU32(E, 0x68, U32(c, 8));
    static const prism_frame layout = {0x60, 0x20, 0x40, 0x30, 0x50};
    const float v = I32(cam, 0x3e8) > 2 ? activation_value(E, U32(cam, 0x3e4), I32(cam, 0x3e8), &layout) : rx_bf(0x7f7fffffu);
    rx_set_sp(sp0);
    return v;
}
RX_FUNCTION(0x00655730, rx_animated_replay_camera_vf37)

/* 00655b40 (ESI = the camera; stack t; plain ret): the path's position at time t: the clock +0x400 = t; within the path's length
 * (+0x120) the path (0063e850) gives the fov and two status values; the camera's position +0x410 is the path's position scaled by
 * +0x420; the fov goes to the Camera (+0xe0, matrix flags cleared), a status pair with both values nonzero is kept (+0x2fc = 1, +0x300 /
 * +0x304) as the animated camera's. Frame: push ebp; and esp, -16; sub esp, 0x50 (the matrix at +0x10, the fov / status values at +4 /
 * +8 / +0xc). */
void rx_animated_replay_camera_set_time(rx_addr cam, float t) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, S = A - 0x50u;
    rx_set_sp(S);
    const rx_addr path = U32(cam, 0x3c0);
    SF32(cam, 0x400, t);
    if (F32(path, 0x120) > t) {
        const uint32_t a[6] = {path, cam_x87_bits(rx_fb(t)), S + 0x10u, S + 4u, S + 8u, S + 0xcu};
        const rx_addr s = loop_push(6, a, 0x00655b8e);
        rx_cam_path_sample(path, rx_bf(a[1]), S + 0x10u, S + 4u, S + 8u, S + 0xcu);
        loop_pop(s);
        SF32(cam, 0x410, F32(cam, 0x420) * F32(S, 0x40));
        SF32(cam, 0x414, F32(cam, 0x424) * F32(S, 0x44));
        SF32(cam, 0x418, F32(cam, 0x428) * F32(S, 0x48));
        SU32(cam, 0xe0, U32(S, 4));
        const float status_a = F32(S, 8), status_b = F32(S, 0xc);
        SU32(cam, 0x164, 0);
        SU32(cam, 0x16c, 0);
        SU32(cam, 0x168, 0);
        if (!(status_a == 0.0f) && !(status_b == 0.0f)) {
            SU8(cam, 0x2fc, 1);
            SF32(cam, 0x300, status_a);
            SF32(cam, 0x304, status_b);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00655b40, rx_animated_replay_camera_set_time)

/* The device table's paused test (006e0c50; EAX = the table [0xda6ca0 + 0xc]): the session's flags +0x58 / +0x5c, the window focus flag
 * [0xa9596c] and the input manager's byte +0x34. PLATFORM BOUNDARY, as build_activity_game.c: the product has no window and no device
 * session (the arena reads 0, which the original counts as paused), so it answers 0 (running); the oracle runs the original. */
static uint32_t device_paused(rx_addr table) {
#ifdef RX_ORACLE
    rx_regs in, out;
    memset(&in, 0, sizeof in);
    in.r[0] = table;
    rx_call_original_ex(0x006e0c50, &in, &out, 0, 0);
    return out.r[0];
#else
    (void)table;
    return 0;
#endif
}

/* 006558b0 MXAnimatedReplayCamera::vf19 (thiscall, dt; ret 4): with the pose valid and a path: not while the session is paused (a local
 * game: the device table's paused test, or the first human's +0x288 flag); the step capped at +0x404; the Camera entity's update; the
 * fov of the range the controller's point is in (+0x440) chased by the spring {+0x43c, +0x440, +0x444} (acceleration +0x3dc, speed
 * cap 10, rate 2) and given to the Camera (+0xe0); the clock advanced by the speed +0x3fc and, at the path's end, wrapped to 0 when
 * looping (+0x3f8) (set_time); with a controller the height spring {+0x430, +0x434 = P.y + 3, +0x438} (100, no cap, rate 10) chases P.y
 * and the camera looks from the path's position (+0x410) at (P.x, the spring's value before the step, P.z). Frame: push ebp; and esp,
 * -16; sub esp, 0x18; push esi / edi: E = the aligned base - 0x20 (P at +0x10). */
void rx_animated_replay_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x20u;
    rx_set_sp(E);
    if (U32(cam, 0x2f4) == 0 || U32(cam, 0x3c0) == 0) {
        rx_set_sp(sp0);
        return;
    }
    race_static_dcf304();
    const rx_addr session = U32(0, 0x00da6cb0);
    if (!rx_session_is_networked(session)) {
        if (device_paused(U32(rx_fx_session_manager(), 0xc)) != 0) {
            rx_set_sp(sp0);
            return;
        }
        if (U32(U32(U32(0, 0x00a946b8), 0x745c), 0x288) != 0) {
            rx_set_sp(sp0);
            return;
        }
    }
    if (dt >= F32(cam, 0x404)) dt = F32(cam, 0x404);
    {
        const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
        const rx_addr s = loop_push(1, a, 0x0065595d);
        rx_cam_entity_vf19(cam, rx_bf(a[0]));
        loop_pop(s);
    }
    const rx_addr c = rx_replay_camera_controller() + 0xc0u;
    const float px = F32(c, 0), py = F32(c, 4), pz = F32(c, 8);
    SF32(E, 0x10, px);
    SF32(E, 0x18, pz);
    SF32(E, 0x14, py);
    {
        const float dx = px - F32(cam, 0x60), dz = pz - F32(cam, 0x68), dy = py - F32(cam, 0x64);
        const float d2 = (dz * dz + dy * dy) + dx * dx;
        select_fov(cam, d2, 0x3d0, 0x3c4, 0x440);
    }
    {
        const uint32_t a[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(U32(cam, 0x3dc)), rx_fb(10.0f /* 0x92fd88 */), rx_fb(2.0f /* 0x92e330 */)};
        const rx_addr s = loop_push(4, a, 0x00655a4f);
        rx_cam_spring(cam + 0x43c, rx_bf(a[0]), rx_bf(a[1]), rx_bf(a[2]), rx_bf(a[3]));
        loop_pop(s);
    }
    SU32(cam, 0xe0, U32(cam, 0x43c));
    SU32(cam, 0x164, 0);
    SU32(cam, 0x16c, 0);
    SU32(cam, 0x168, 0);
    {
        float t = (F32(cam, 0x3fc) * dt) + F32(cam, 0x400);
        const int ended = t >= F32(U32(cam, 0x3c0), 0x120);            /* comiss; jb: not above-or-equal for NaN */
        if ((U8(cam, 0x3f8) & (uint8_t)ended) != 0) t = 0.0f;
        rx_animated_replay_camera_set_time(cam, t);
    }
    if (rx_replay_camera_controller() != 0) {
        SF32(cam, 0x434, F32(0, 0x009c4a7c) + F32(E, 0x14));
        SU32(E, 0x14, U32(cam, 0x430));
        const uint32_t a[4] = {cam_x87_bits(rx_fb(dt)), rx_fb(100.0f /* 0x93005c */), rx_fb(999999.0f /* 0x977aa8 */), rx_fb(10.0f /* 0x92fd88 */)};
        const rx_addr s = loop_push(4, a, 0x00655b27);
        rx_cam_spring(cam + 0x430, rx_bf(a[0]), rx_bf(a[1]), rx_bf(a[2]), rx_bf(a[3]));
        loop_pop(s);
        const uint32_t b[1] = {cam};
        const rx_addr s2 = loop_push(1, b, 0x00655b37);
        rx_cam_entity_look_at(cam + 0x410u, E + 0x10u, cam);
        loop_pop(s2);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006558b0, rx_animated_replay_camera_vf19)

/* 00655c30 MXAnimatedReplayCamera::vf39 (thiscall): the debug draw: the camera's own position (vf14's matrix +0x30) snapped onto the
 * terrain of the running scene. Frame: push ebp; and esp, -16; sub esp, 0x18; push ebx / esi. */
void rx_animated_replay_camera_vf39(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x20u;
    rx_set_sp(E);
    const rx_addr m = loop_tc0(loop_vslot(cam, 0x38), cam);
    SU32(E, 0x10, U32(m, 0x30));
    SU32(E, 0x14, U32(m, 0x34));
    SU32(E, 0x18, U32(m, 0x38));
    const rx_addr terrain = debug_terrain();
    if (terrain != 0) snap_to_ground(terrain, E + 0x10u, 0x00655cf4);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00655c30, rx_animated_replay_camera_vf39)
