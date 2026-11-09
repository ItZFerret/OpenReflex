/* loop_cam_player.c -- MXPlayerCamera (the base of the gameplay cameras that follow a player: third person, first person, quick,
 * start gate, replay tower) and MXAnimatedCamera (the authored path cameras of the intro / replays).
 *
 * MXPlayerCamera `cam` (an MXCameraEntity, loop_cam_entity.c): +0x344 the camera controller C (C +0x1ac the followed vehicle
 * entity: vf17 slot 0x44 writes its position, +0x130 its forward, +0x194 its class; C +0x1b4 an attachment whose vf29 gives a
 * node), +0x320 the vehicle's forward of the last frame, +0x330 its position, +0x340 "moved more than 15 units" (a cut).
 * MXAnimatedCamera: +0x310 the path (0063e850), +0x430 the path length, +0x440 restart, +0x444 loop, +0x448 speed, +0x44c clock,
 * +0x460 the path's base matrix, +0x4a0 "flip the path's x axis". */
#include "tuning_reload_log.h"
#include "loop_cam.h"
#include "../src/rx_os.h"
#include "loop_cam_entity.h"
#include "loop_cam_player.h"
#include "loop_util.h"
#include "anim_math.h"
#include "build_xml.h"
#include "fx.h"
#include "race_driver.h"
#include "loop_cam_tuning.h"

/* the followed vehicle entity */
static rx_addr cam_vehicle(rx_addr cam) { return U32(U32(cam, 0x344), 0x1ac); }
/* its vf17 (slot 0x44): the position into a vec3 */
static void vehicle_position(rx_addr ent, rx_addr out) { loop_tc1(loop_vslot(ent, 0x44), ent, out); }

/* 006467c0 (LTCG: EAX = the camera; plain ret) -> EAX: the followed vehicle's camera class: the vehicle (controller +0x1ac)'s class +0x194,
 * but class 3 splits by +0x170: 3 when it is 8, else 8. (Leaves ECX = the vehicle.) */
uint32_t rx_cam_vehicle_class(rx_addr cam) {
    const rx_addr vehicle = U32(U32(cam, 0x344), 0x1ac);
    uint32_t cls = U32(vehicle, 0x194);
    if (cls == 3) cls = (U32(vehicle, 0x170) != 8) ? 8u : 3u;
    return cls;
}
RX_FUNCTION(0x006467c0, rx_cam_vehicle_class)

/* 00646a50 (LTCG: EDI = camera): the camera tuning reload of the developer's XML watch. Nothing to do unless a reload is
 * pending (0x9e3100 = the changed file, 0x9d7b2c = "reload allowed"): then, under the lock 0xad7538, an XMLReader on the stack
 * reads the file (008252c0), the camera's tuning record (0065c200 of slot 006467c0 in the table 0xaf6d10, 0x1da80 each) is filled
 * from it (0042fed0 / 00790540) and the camera's vf32 (slot 0x80) applies it; the file object is released (its vf0 with 1).
 * Frame: sub esp, 0x470 (the security cookie at +0x468); push esi; the reader at the frame + 4. */
void rx_cam_reload_tuning(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), E = sp0 - 0x470u;
    if (U32(0, 0x009e3100) != 0) {
        rx_set_sp(E - 4u);
        SU32(E, 0x468, U32(0, 0x009b45e4) ^ E);
        if (U32(0, 0x009d7b2c) != 0) {
            const rx_addr R = E + 4u;
            { const uint32_t a[1] = {0x00ad7538}; rx_os_import(0x0091600c, 1, a); }   /* EnterCriticalSection */
            rx_regs in = loop_regs(), out;
            rx_xml_reader_construct(R);                                                  /* 00824e70 */
#ifdef RX_ORACLE
            /* 008252c0 takes the guest CRTFileInputStream at 0x009e3100 (a runtime object with its own vtable), not a host
             * rx_stream: run the original loader here, exactly as 00646a50's reload branch does (cdecl, reader then stream) */
            int loaded;
            { const uint32_t a2[2] = {R, U32(0, 0x009e3100)};
              loaded = rx_call_original(0x008252c0, RX_CDECL, 0, 2, a2) != 0; }
#else
            /* boundary (dev-only): the XML reload watcher's guest file stream is the runtime's and 0x009e3100 is never set in
             * the product, so nothing is loaded; the core reader below is still constructed and destroyed */
            const int loaded = 0;
#endif
            if (loaded) {
                uint32_t cam_class;
                {   /* push edx (= R: 0065c200's second argument); call 006467c0 (EAX = the camera; no stack arguments, cdecl) */
                    const uint32_t pushed[1] = {R};
                    const rx_addr s = loop_push(1, pushed, 0x00646ab5);
                    cam_class = rx_cam_vehicle_class(cam);
                    loop_pop(s);
                }
                const uint32_t a3[1] = {cam_class * 0x1da80u + 0x00af6d10u};
                in = loop_regs(); in.r[LOOP_EDI] = cam;
                {   /* 0065c200 (stdcall table, reader; RET 8; loop_cam_tuning.c): EAX = loaded, ECX = the security cookie it leaves */
                    const uint32_t aa[2] = {a3[0], R};
                    const rx_addr s = loop_push(2, aa, 0x00646ac6);
                    out = loop_regs();
                    out.r[LOOP_EAX] = rx_cam_tuning_parse(aa[0], aa[1]);
                    out.r[LOOP_ECX] = U32(0, 0x009b45e4);
                    loop_pop(s);
                }
                const rx_addr rec = out.r[LOOP_EAX];
                rx_addr file;
                if (rec != 0) {
                    rx_tuning_reload_log(out.r[LOOP_ECX], 0x00646ad8, 0x00646ae1);   /* ECX: 0065c200's cookie, pushed as it is */
                    loop_tc0(loop_vslot(cam, 0x80), cam);
                    file = U32(0, 0x009e3100);
                } else {
                    file = U32(0, 0x009e3100);
                    SU32(0, 0x009d7b2c, 0);
                    SU32(file, 0xc, 0);
                }
                if (rec != 0) {
                    if (file != 0) loop_tc1(loop_vslot(file, 0), file, 1);
                    SU32(0, 0x009e3100, 0);
                    SU32(0, 0x009d7b2c, 1);
                }
            }
            { const uint32_t a[1] = {0x00ad7538}; rx_os_import(0x009160f8, 1, a); }   /* LeaveCriticalSection */
#ifdef RX_ORACLE
            in = loop_regs(); in.r[LOOP_ESI] = R;
            rx_call_original_ex(0x00824fe0, &in, &out, 0, 0);                            /* the reader's destructor */
#else
            rx_xml_reader_destroy(R);                                                    /* 00824fe0's port */
#endif
        }
        if (U32(0, 0x009e3100) == 0) SU32(0, 0x009d7b2c, 1);
        rx_set_sp(sp0);
        return;
    }
    SU32(0, 0x009d7b2c, 1);
}
RX_FUNCTION(0x00646a50, rx_cam_reload_tuning)

/* 006468a0 (LTCG: EAX = camera, ESI = out vec3) -> EAX = out: the point the camera follows. Without an attachment (controller
 * +0x1b4) the vehicle's position (vf17); else the world translation of the attachment's node (vf29 -> +8, brought up to date by
 * Node_EvaluateWorld, the matrix at hierarchy +0x1c + 64 * node +0xb). */
rx_addr rx_cam_follow_point(rx_addr cam, rx_addr out) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu;
    const rx_addr C = U32(cam, 0x344);
    rx_set_sp(A - 0x20u);
    if (U32(C, 0x1b4) == 0) {
        const rx_addr L = A - 0x10u;
        vehicle_position(U32(C, 0x1ac), L);
        SU32(out, 0, U32(L, 0));
        SU32(out, 4, U32(L, 4));
        SU32(out, 8, U32(L, 8));
    } else {
        const rx_addr att = U32(C, 0x1b4);
        const rx_addr node = U32(loop_tc0(loop_vslot(att, 0x74), att), 8);
        loop_tc1(0x007db890, node, 0);                                  /* Node_EvaluateWorld (rx_world) */
        const rx_addr m = (uint32_t)U8(node, 0xb) * 0x40u + U32(U32(node, 0), 0x1c);
        const uint32_t x = U32(m, 0x30), y = U32(m, 0x34), z = U32(m, 0x38);
        SU32(out, 0, x);
        SU32(out, 4, y);
        SU32(out, 8, z);
    }
    rx_set_sp(sp0);
    return out;
}
RX_FUNCTION(0x006468a0, rx_cam_follow_point)

/* 00646940 MXPlayerCamera::vf19 (dt; ret 4): the tuning reload (00646a50), the entity velocity (MXCameraEntity::vf19); then the
 * vehicle's position: a jump of more than 15 units (squared distance > 0x9d78c0, ((dz^2 + dy^2) + dx^2)) since the last frame
 * sets the cut flag +0x340; position +0x330 and forward +0x320 kept for the next frame. */
void rx_player_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, L = A - 0x10u;
    rx_set_sp(A - 0x20u);
    { const rx_addr s = loop_push(0, 0, 0x00646951); rx_cam_reload_tuning(cam); loop_pop(s); }
    { const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
      const rx_addr s = loop_push(1, a, 0x0064695f); rx_cam_entity_vf19(cam, rx_bf(a[0])); loop_pop(s); }
    vehicle_position(cam_vehicle(cam), L);
    const float dz = F32(L, 8) - F32(cam, 0x338);
    const float dy = F32(L, 4) - F32(cam, 0x334);
    const float x = F32(L, 0);
    const rx_addr ent = cam_vehicle(cam);
    const uint32_t f0 = U32(ent, 0x130), f1 = U32(ent, 0x134), f2 = U32(ent, 0x138);
    const float dx = x - F32(cam, 0x330);
    const float d2 = ((dz * dz) + (dy * dy)) + (dx * dx);
    SU8(cam, 0x340, d2 > F32(0, 0x009d78c0) ? 1 : 0);
    SU32(cam, 0x334, U32(L, 4));
    SF32(cam, 0x330, x);
    SU32(cam, 0x338, U32(L, 8));
    SU32(cam, 0x320, f0);
    SU32(cam, 0x324, f1);
    SU32(cam, 0x328, f2);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00646940, rx_player_camera_vf19)

/* 00646bf0 MXPlayerCamera::vf31: the render camera record. Unless a vehicle with a rider controller (+0x1a0, whose +0xaf00 is
 * clear) is followed, the entity's own record (MXCameraEntity::vf31). Else a camera behind and above the vehicle: forward F =
 * the vehicle's forward with y = 0, up (0, 1, 0), position = the vehicle's position + (h * 0, h, h * 0) - s * F with h = 0x9ca484
 * and s = 0x9ca458 times the class factor (0x932098 [vehicle +0x194]); the render basis (007911b0) and position (00790cb0). */
void rx_player_camera_vf31(rx_addr cam) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu;
    const rx_addr ent0 = cam_vehicle(cam);
    if (ent0 == 0 || U32(ent0, 0x1a0) == 0 || U32(U32(ent0, 0x1a0), 0xaf00) != 0) {
        const rx_addr s = loop_push(0, 0, 0x00646d5d);
        rx_cam_entity_record(cam);
        loop_pop(s);
        return;
    }
    const rx_addr L = A - 0x30u, F = A - 0x20u, U = A - 0x10u;
    rx_set_sp(A - 0x40u);
    vehicle_position(cam_vehicle(cam), L);
    const rx_addr ent = cam_vehicle(cam);
    const float fx = F32(ent, 0x130), fz = F32(ent, 0x138);
    float k = 1.0f /* 0x931f24 */;
    SF32(F, 0, fx); SF32(F, 8, fz);
    SF32(U, 0, 0.0f); SF32(U, 4, k); SF32(U, 8, 0.0f);
    SF32(F, 4, 0.0f);
    if (ent != 0) k = F32(0, 0x00932098 + 4u * U32(cam_vehicle(cam), 0x194));
    const float h = F32(0, 0x009ca484);
    const float h0 = h * 0.0f;
    const float pz = F32(L, 8) + h0;
    const float px = F32(L, 0) + h0;
    const float py = F32(L, 4) + h;
    const float s = F32(0, 0x009ca458) * k;
    const float sx = s * fx, s0 = s * 0.0f, sz = s * F32(F, 8);
    SF32(L, 0, (-0.0f /* 0x9774a8 */ - sx) + px);
    SF32(L, 4, (-0.0f - s0) + py);
    SF32(L, 8, (-0.0f - sz) + pz);
    rx_cam_set_render_basis_from(cam + 0x90, U, F);
    rx_cam_set_render_position(cam + 0x90, L);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00646bf0, rx_player_camera_vf31)

/* ---- MXAnimatedCamera ---- */

/* The engine's time source (a function-local static at 0x00da6ca0: constructed once, guard bit 0 of 0x00dcf304, destructor thunk
 * 00913e00 registered with atexit). */
static void time_source_init(void) {
    race_static_dcf304();   /* 006de1f0 behind its guard 00dcf304, + the atexit of 00913e00 */
}

/* 0063d7e0 MXAnimatedCamera::vf19 (dt; ret 4): a restart (+0x440) rewinds the clock +0x44c and zeroes dt (its argument slot). The
 * camera runs unless the game is paused: the time source's +0x1c4c set with its +0x1b0 != 1, and then neither a menu (+0x58 /
 * +0x5c of 0xda6cac), a frame-step debugger (0xa9596c with 0xa95960's vf4 +0x34 clear) nor the game's pause (0xa946b8 +0x745c ->
 * +0x288) holds it. Running: the entity velocity (MXCameraEntity::vf19), then the path at clock + speed * dt (0 when looping past
 * the end: clock >= length with +0x444) (0063d600).
 *
 * The race intro's camera path is set by the scenario's CameraAnimation handler (0055d290 -> 0063d780 / 0063e7c0) and ticked by
 * 007473f0 while the intro script runs; this update is what poses the world's MXAnimatedCamera (world + 0x218f0, "+0x2b0 pose set"),
 * which rx_session_camera exports when no player camera entity is active yet. */
void rx_animated_camera_vf19(rx_addr cam, float dt) {
    (void)dt;                                     /* read from its argument slot (the restart rewrites it) */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~7u, T = A - 4u;
    const rx_addr arg = sp0 + 4u;
    rx_set_sp(A - 0x10u);
    if (U32(cam, 0x440) != 0) {
        SU32(cam, 0x440, 0);
        SF32(cam, 0x44c, 0.0f);
        SF32(arg, 0, 0.0f);
    }
    time_source_init();
#ifdef RX_ORACLE
    const rx_addr ts = U32(0, 0x00da6cb0);
    int run = 0;
    if (U32(ts, 0x1c4c) != 0 && U32(ts, 0x1b0) != 1) run = 1;
    else {
        time_source_init();
        const rx_addr ui = U32(0, 0x00da6cac);
        if (U32(ui, 0x58) == 0 && U32(ui, 0x5c) == 0 && U32(0, 0x00a9596c) != 0) {
            const rx_addr dbg = U32(0, 0x00a95960);
            if (U8(loop_tc0(loop_vslot(dbg, 0x10), dbg), 0x34) == 0 &&
                U32(U32(U32(0, 0x00a946b8), 0x745c), 0x288) == 0) run = 1;
        }
    }
#else
    /* PRODUCT BOUNDARY, the same pause rule as 006e0c50 in build_activity_game.c: this inlined copy reads the PCSession
     * [0x00da6cb0] and the network / device manager [0x00da6cac] that MXGame::Startup's 006de390 creates (Quazal / Steam, not
     * core), plus the window focus 0xa9596c (the WM_SETFOCUS the product has no window for). The product has neither object, so
     * the original's test would read the arena's zero page as if it were them (and hold every animated camera still: the intro
     * fly-by's world camera never got its pose, the gap cameraschema found). A headless product run is the focused, offline game
     * with no menu open; the game's own pause value still holds the camera, as in the original. */
    const rx_addr g = U32(0, 0x00a946b8);
    const int run = g != 0 && U32(U32(g, 0x745c), 0x288) == 0;
#endif
    if (run) {
        const uint32_t a[1] = {cam_x87_bits(U32(arg, 0))};
        { const rx_addr s = loop_push(1, a, 0x0063d8c6); rx_cam_entity_vf19(cam, rx_bf(a[0])); loop_pop(s); }
        const float clock = F32(cam, 0x44c);
        const uint32_t past = clock >= F32(cam, 0x430) ? 1u : 0u;      /* comiss; jb: not below (or unordered: below) */
        float t = (F32(cam, 0x448) * F32(arg, 0)) + clock;
        SF32(T, 0, t);
        if ((U32(cam, 0x444) & past) != 0) { t = 0.0f; SF32(T, 0, t); }
        const uint32_t b[1] = {cam_x87_bits(rx_fb(t))};
        const rx_addr s = loop_push(1, b, 0x0063d912);
        rx_animated_camera_at(cam, rx_bf(b[0]));
        loop_pop(s);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063d7e0, rx_animated_camera_vf19)

/* 0063d600 (LTCG: ESI = camera; stack t; ret 4): the camera at time t of its path: clock +0x44c = t; within the path (t < length
 * +0x430) the path (0063e850) gives the pose matrix M (the rows right / up / forward / position at the frame - 0x40), the fov
 * (frame +4) and a two-value status (+8 / +0xc). With +0x4a0 the x axis is flipped (right, up and forward x negated by
 * 0x9cd9e8 = -1, and right = up x forward again). M is transformed by the base matrix +0x460 (00401830) and set as the pose
 * (0063dee0); the fov goes to the Camera (+0xe0, its matrix flags cleared); a status with both values nonzero is kept (+0x2fc =
 * 1, +0x300 / +0x304). Frame: push ebp; and esp, -16; sub esp, 0x50: f0 / fov / s0 / s1 at +0 / +4 / +8 / +0xc, M at +0x10. */
void rx_animated_camera_at(rx_addr cam, float t) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x50u, M = E + 0x10u;
    rx_set_sp(E);
    SF32(cam, 0x44c, t);
    if (!(F32(cam, 0x430) > t)) { rx_set_sp(sp0); return; }
    {
        const uint32_t a[6] = {cam + 0x310, cam_x87_bits(rx_fb(t)), M, E + 4u, E + 8u, E + 0xcu};
        const rx_addr s = loop_push(6, a, 0x0063d64e);
        rx_cam_path_sample(cam + 0x310, rx_bf(a[1]), M, E + 4u, E + 8u, E + 0xcu);
        loop_pop(s);
    }
    if (U32(cam, 0x4a0) != 0) {
        const float m1 = F32(0, 0x009cd9e8);
        const float ux = F32(E, 0x20) * m1;     /* up.x (M +0x10) */
        const float fx = F32(E, 0x30) * m1;     /* forward.x (M +0x20) */
        const float px = F32(E, 0x40) * m1;     /* position.x (M +0x30) */
        const float uy = F32(E, 0x24), uz = F32(E, 0x28), fy = F32(E, 0x34), fz = F32(E, 0x38);
        SF32(E, 0x10, (uy * fz) - (uz * fy));
        SF32(E, 0x14, (uz * fx) - (fz * ux));
        SF32(E, 0x18, (fy * ux) - (uy * fx));
        SF32(E, 0x20, ux);
        SF32(E, 0x30, fx);
        SF32(E, 0x40, px);
    }
    {
        const uint32_t a[1] = {M};
        const rx_addr s = loop_push(1, a, 0);
        rx_fx_matrix4_mul(M, cam + 0x460, M);                  /* Matrix4_Mul (rx_fx): M = M x base, cdecl */
        loop_pop(s);
    }
    {
        const uint32_t a[2] = {cam, M};
        const rx_addr s = loop_push(2, a, 0x0063d709);
        rx_cam_entity_set_pose(cam, M);
        loop_pop(s);
    }
    const float s0 = F32(E, 8), fov = F32(E, 4);
    SU32(cam, 0x164, 0);
    SU32(cam, 0x16c, 0);
    SF32(cam, 0xe0, fov);
    SU32(cam, 0x168, 0);
    if (s0 != 0.0f) {
        const float s1 = F32(E, 0xc);
        if (s1 != 0.0f) {
            SU8(cam, 0x2fc, 1);
            SF32(cam, 0x300, s0);
            SF32(cam, 0x304, s1);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063d600, rx_animated_camera_at)

/* ---- the camera path (MXCameraPath, the animated camera's +0x310) ---- */

/* 004dbe00 (LTCG: EAX = count, EDI = keys of 16 bytes, xmm0 = t) -> EAX: the number of keys at or before t (binary search; 0 for
 * no keys or t before the first, count when t is at or past the last). */
uint32_t rx_keytrack_count_before(uint32_t n, rx_addr keys, float t) {
    if (n == 0) return 0;
    if (t >= F32(keys, n * 16u - 16u)) return n;
    if (F32(keys, 0) > t) return 0;
    uint32_t lo = 0, hi = n;
    for (;;) {
        const uint32_t mid = (uint32_t)((int32_t)(hi + lo) >> 1);
        const rx_addr k = keys + mid * 16u;
        if (!(t >= F32(k, 0))) {          /* jb */
            if (t >= F32(k, -16)) return mid;
            hi = mid;
            continue;
        }
        if (F32(k, 16) > t) return mid + 1;
        lo = mid;
    }
}
RX_FUNCTION(0x004dbe00, rx_keytrack_count_before)

/* 0063e850 (stdcall path, t, out matrix, out fov, out status a, out status b; ret 0x18): the camera path at time t (clamped to
 * [0, length +0x120]; the clamped value is written back into the argument). The path's animation O (+0x11c: +0x4c busy while
 * sampled, +0x44 the track set T) gives:
 *  - the rotation (T +0x24: keys of 12 bytes {time, packed quaternion}, +4 the search cursor, +0xc enabled, +0x10 count): the
 *    segment of t (the cursor is reset to 1 / moved to the end when t is far from it, then walked), the slerp of its two keys
 *    (QuatSlerp_Packed), or the single key's quaternion when t is outside the keys;
 *  - the position (T +0x1c: keys of 16 bytes {time, xyz}, +8 enabled, +0xc count): the lerp of the segment (KeyTrack_FindSegment;
 *    extrapolated from the later key outside), or the last key at or before t (004dbe00) when the track is not interpolated;
 *  - the matrix rows from the quaternion q (axes permuted for the camera): (1 - (zz + yy), xy + wz, xz - wy), (xz + wy, yz - wx,
 *    1 - (xx + yy)), -(xy - wz, 1 - (zz + xx), wx + yz), the position;
 *  - the fov (T +0x28 count, +0x2c track, degrees): the horizontal angle of the track turned into the vertical fov of the screen
 *    (2 atan(tan(a / 2) / aspect), aspect = the display's width / height of 0xa95960's vf4 +8 / +0xc; inline sine / cosine and an
 *    atan polynomial), 0x977980 without a track;
 *  - two status values (T +0x30 count, +0x34 / +0x38 tracks; 0 without).
 * Frame: push ebp; and esp, -16; sub esp, 0x74; push ebx / esi / edi: E = the aligned base - 0x80. */
void rx_cam_path_sample(rx_addr path, float t, rx_addr M, rx_addr out_fov, rx_addr out_a, rx_addr out_b) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x80u;
    const rx_addr targ = sp0 + 8u;
    rx_set_sp(E);
    const rx_addr O = U32(path, 0x11c);
    SU32(O, 0x4c, 1);
    const rx_addr T = U32(O, 0x44);
    SU32(E, 0xc, T);
    if (0.0f > t) { t = 0.0f; SF32(targ, 0, t); }
    if (t > F32(path, 0x120)) { t = F32(path, 0x120); SF32(targ, 0, t); }
    const rx_addr Q = E + 0x20u;
    const rx_addr R = U32(T, 0x24);
    const int32_t nr = I32(R, 0x10);
    if (U32(R, 0xc) != 0 && nr >= 1) {
        SU32(E, 0x14, 1);
        rx_addr ka, kb;           /* the two keys of the slerp */
        float frac;
        int found = 1;
        if (nr <= 2) {
            const rx_addr keys = U32(R, 0);
            if (nr == 2) {
                frac = (t - F32(keys, 0)) / (F32(keys, 0xc) - F32(keys, 0));
                kb = keys + 0xcu;
            } else {
                frac = 0.0f;
                kb = keys;
            }
            ka = keys;
            SF32(E, 0x10, frac);
        } else {
            const int32_t last = nr - 1, q = last >> 2, h = (last >> 1) + q;
            SU32(E, 0x10, (uint32_t)last);
            if (I32(R, 4) > h && F32(U32(R, 0), (uint32_t)q * 12u) > t) SU32(R, 4, 1);
            else if (I32(R, 4) < q && t > F32(U32(R, 0), (uint32_t)h * 12u)) SU32(R, 4, (uint32_t)last);
            int32_t i = I32(R, 4);
            if (i < 1) i = 1;
            if (t > F32(U32(R, 0), (uint32_t)i * 12u)) {
                for (;;) {
                    i++;
                    if (i >= nr) { i = last; found = 0; break; }
                    if (!(t > F32(U32(R, 0), (uint32_t)i * 12u))) break;
                }
            }
            const rx_addr keys = U32(R, 0);
            if (F32(keys, (uint32_t)i * 12u - 12u) > t) {
                for (;;) {
                    i--;
                    if (i < 1) { i = 1; found = 0; break; }
                    if (!(F32(keys, (uint32_t)i * 12u - 12u) > t)) break;
                }
            }
            SU32(R, 4, (uint32_t)i);
            kb = keys + (uint32_t)i * 12u;
            ka = kb - 12u;
            frac = (t - F32(ka, 0)) / (F32(kb, 0) - F32(ka, 0));
            SF32(E, 0x10, frac);
        }
        if (found) {
            cam_quat_slerp_packed(ka + 4u, kb + 4u, Q, rx_bf(cam_x87_bits(rx_fb(frac))), 0x0063e9bb);
        } else {
            {   /* the key's quaternion (anim_packed_quat's decode, with the original's locals and the 1 / 32767 of 0x9c6400) */
                const rx_addr key = kb + 4u;
                float a = (float)U16(key, 0) * 0.0007669904f /* 0x9778fc */;
                const float s = anim_sign(a);
                a = rx_muls(a, s);
                const float y = anim_trig_reduce(a);
                const float y2 = y * y, y4 = y2 * y2;
                const float c = rx_subs(-0.0f /* 0x9774a8 */, anim_cos_poly(y2, y4));
                const float k = rx_muls(rx_muls(anim_sin_poly(y2, y4) * y, s), F32(0, 0x009c6400));
                const float x = (float)(int16_t)U16(key, 2) * k;
                const float yy = (float)(int16_t)U16(key, 4) * k;
                const float z = (float)(int16_t)U16(key, 6) * k;
                const float flip = anim_sign(c);
                SF32(E, 0x14, flip);
                SF32(Q, 0xc, fabsf(c));
                SF32(Q, 0, rx_muls(x, flip));
                SF32(Q, 4, rx_muls(flip, yy));
                SF32(Q, 8, rx_muls(flip, z));
            }
        }
    }
    /* the position */
    const rx_addr P = U32(T, 0x1c);
    float px, py, pz;
    if (U32(P, 8) != 0 && I32(P, 0xc) > 1) {
        const uint32_t in = cam_keytrack_find(U32(P, 0), U32(P, 0xc), t, E + 0x10u, E + 0x14u, E + 0x18u, 0x0063ebca);
        const float f = F32(E, 0x18);
        const rx_addr a = U32(E, 0x10), b = U32(E, 0x14);
        if (in != 0) {
            px = ((F32(b, 4) - F32(a, 4)) * f) + F32(a, 4);
            py = ((F32(b, 8) - F32(a, 8)) * f) + F32(a, 8);
            pz = ((F32(b, 0xc) - F32(a, 0xc)) * f) + F32(a, 0xc);
        } else {
            px = ((F32(b, 4) - F32(a, 4)) * f) + F32(b, 4);
            py = ((F32(b, 8) - F32(a, 8)) * f) + F32(b, 8);
            pz = ((F32(b, 0xc) - F32(a, 0xc)) * f) + F32(b, 0xc);
        }
    } else {
        const rx_addr keys = U32(P, 0);
        const uint32_t k = rx_keytrack_count_before(U32(P, 0xc), keys, t);
        const rx_addr key = keys + k * 16u;
        px = F32(key, -0xc);
        py = F32(key, -8);
        pz = F32(key, -4);
    }
    SF32(E, 0x30, px);
    SF32(E, 0x34, py);
    SF32(E, 0x38, pz);
    {   /* the matrix */
        const float qx = F32(Q, 0), qy = F32(Q, 4), qz = F32(Q, 8), qw = F32(Q, 0xc);
        const float z2 = qz * 2.0f /* 0x92e330 */, x2 = qx * 2.0f, y2 = qy * 2.0f;
        const float xx = x2 * qx, zz = z2 * qz, yy = y2 * qy;
        const float wz = z2 * qw, wx = x2 * qw;
        const float yz = y2 * qz, wy = y2 * qw;
        const float xy = x2 * qy, xz = x2 * qz;
        SF32(E, 0x18, wz); SF32(E, 0x14, wx); SF32(E, 0x10, yz);
        const float m00 = 1.0f - (zz + yy);
        const float m02 = xz - wy;
        const float xymwz = xy - wz;
        SF32(E, 0x40, m00); SF32(E, 0x48, m02); SF32(E, 0x50, xymwz);
        const float m11 = 1.0f - (zz + xx);
        SF32(M, 0x10, xz + wy);
        SF32(M, 0x14, yz - wx);
        SF32(M, 0x20, -0.0f /* 0x9774a8 */ - xymwz);
        SF32(M, 0x28, -0.0f - (wx + yz));
        SF32(M, 0x04, xy + wz);
        SF32(M, 0x30, px);
        SF32(M, 0x08, m02);
        SF32(M, 0x34, py);
        SF32(M, 0x00, m00);
        SF32(M, 0x0c, 0.0f);
        SF32(M, 0x18, 1.0f - (xx + yy));
        SF32(M, 0x1c, 0.0f);
        SF32(M, 0x24, -0.0f - m11);
        SF32(M, 0x2c, 0.0f);
        SF32(M, 0x38, pz);
        SF32(M, 0x3c, 1.0f);
    }
    if (out_fov != 0) {
        float fov;
        if (I32(T, 0x28) <= 0) fov = 1.5707961f /* 0x977980 */;
        else {
            SF32(E, 0x18, 0.0f);
            cam_float_track(U32(T, 0x2c), E + 0x18u, rx_bf(cam_x87_bits(rx_fb(t))), 0x0063ee75);
            float a = (F32(E, 0x18) * 0.017453291f /* 0x977678 */) * 0.5f /* 0x9322cc */;
            const float s = a >= 0.0f /* 0x92e12c */ ? 1.0f : -1.0f /* 0x9cd9e8 */;
            a = a * s;
            SF32(E, 0x18, s);
            const float y = 3.1415927f /* 0x977498 */ - (a - ((float)rx_cvtt(a * 0.15915494f /* 0x9774a0 */) * 6.2831855f /* 0x97749c */));
            const float y2 = y * y, y4 = y2 * y2;
            SF32(E, 0x1c, y); SF32(E, 0x14, y2); SF32(E, 0x10, y4);
            const rx_addr disp = U32(0, 0x00a95960);
            const rx_addr d1 = loop_tc0(loop_vslot(disp, 0x10), disp);
            const int32_t width = I32(d1, 8);
            const rx_addr d2 = loop_tc0(loop_vslot(U32(0, 0x00a95960), 0x10), U32(0, 0x00a95960));
            const float sn = ((anim_sin_poly(y2, y4) * y) * s);
            const float cs = -0.0f /* 0x9774a8 */ - anim_cos_poly(y2, y4);
            const float height = (float)I32(d2, 0xc);
            float v = sn / cs;
            v = v / ((float)width / height);
            const float s2 = v >= 0.0f ? 1.0f : -1.0f;
            SF32(E, 0x18, s2);
            const float av = fabsf(v);
            const float x = (av >= 1.0f /* 0x931f24 */) ? 1.0f / av : av;
            const float x2 = x * x;
            const float den = ((x2 * 0.058319379f /* 0x93a728 */) + 0.76443946f /* 0x93a724 */) * x2 + 1.0f;
            const float r = (1.0f / den) * (((x2 * 0.43157974f /* 0x93a720 */) + 1.0f) * x);
            const float at = (av >= 1.0f) ? 1.5707964f /* 0x977508 */ - r : r;
            fov = (at * s2) * 2.0f /* 0x92e330 */;
        }
        SF32(out_fov, 0, fov);
    }
    if (out_a != 0) {
        if (I32(T, 0x30) <= 0) SF32(out_a, 0, 0.0f);
        else cam_float_track(U32(T, 0x34), out_a, rx_bf(cam_x87_bits(rx_fb(t))), 0x0063f0b0);
    }
    if (out_b != 0) {
        if (I32(T, 0x30) <= 0) SF32(out_b, 0, 0.0f);
        else cam_float_track(U32(T, 0x38), out_b, rx_bf(cam_x87_bits(rx_fb(t))), 0x0063f0dc);
    }
    SU32(U32(path, 0x11c), 0x4c, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0063e850, rx_cam_path_sample)
