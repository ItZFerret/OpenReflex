/* loop_cam_parts_view.c -- see loop_cam_parts_view.h. Hand transliteration of MXUIPartsViewCamera::vf19 00656070 and its unported
 * callees 00658550 / 00658d80 (one body, the offsets differ), 006596a0, 0063e160, 004073e0, 0063c890, from the disassembly.
 *
 * Main thread (the Garage / gate-selection UI). Scalar SSE in the original's operand order (phys_add / phys_mul ... keep x86's NaN
 * rule), the inlined sine / cosine polynomial, the double-precision arc-cosine approximation of the blends (double steps with the
 * float roundings the code makes); x87 only as fld / fstp copies of floats (bike_x87_copyf) and the KeyMap result (a float).
 * Stack frames as the original's: the vectors the cross products, the matrix helpers and the look-at take by address live there,
 * and the code reads several stack slots back (some never written on a path: they are read at the same addresses). */
#include "loop_cam_parts_view.h"
#include "bike_math.h"
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "phys_crt.h"
#include "race_driver.h"
#include "world_node.h"
#include "world_veh.h"

/* ---- small helpers ------------------------------------------------------------------------------------------------------------ */

/* the frame of `push ebp; mov ebp, esp; and esp, -16; sub esp, size` */
static rx_addr aligned_frame(rx_addr entry, uint32_t size) {
    return ((entry - 4u) & ~15u) - size;
}

/* SqrtF 0042c230 of a value passed through fld / fstp (the cdecl argument pushed at esp) */
static float sqrt_x87_arg(rx_addr esp, float x) {
    rx_set_sp(esp - 8);                        /* push ecx; fstp [esp]; call */
    SF32(esp - 4, 0, bike_x87_copyf(x));
    const float r = phys_sqrtf(bike_x87_copyf(x));
    rx_set_sp(esp);
    return r;
}
/* SqrtF of a value stored with movss */
static float sqrt_arg(rx_addr esp, float x) {
    rx_set_sp(esp - 8);
    SF32(esp - 4, 0, x);
    const float r = phys_sqrtf(x);
    rx_set_sp(esp);
    return r;
}
/* Vec3_Cross 00401630 (cdecl out; ECX = c, EAX = a): out = c x a, called at esp */
static void cross_at(rx_addr esp, rx_addr out, rx_addr c, rx_addr a) {
    rx_set_sp(esp - 8);
    SU32(esp - 4, 0, out);
    rx_world_vec3_cross(out, c, a);
    rx_set_sp(esp);
}
/* KeyMap::vf4 00811710 of the global key map `map` (mask 1, the camera's pad) */
static float key_value(rx_addr B, rx_addr map, uint32_t pad) {
    rx_set_sp(B - 12);                         /* push pad; push 1; call */
    SU32(B - 8, 0, 1);
    SU32(B - 4, 0, pad);
    const float v = rx_world_keymap_value(map, 1, pad);
    rx_set_sp(B);
    return v;
}

/* the inlined sine / cosine of an angle in radians: the sign taken out (comiss a, 0; jae: NaN counts as negative), reduced by 2 pi
 * (cvttss2si), r = pi - x, then the two polynomials in r^2 (constants 0x935d3c .. 0x935d68). */
static void fast_sincos(float a, float* sine, float* cosine) {
    const float sign = a >= 0.0f ? 1.0f : -1.0f;                                       /* 0x931f24 / 0x9cd9e8 */
    float x = phys_mul(sign, a);
    const float turns = (float)rx_cvtt(phys_mul(x, rx_bf(0x3e22f983u) /* 0x9774a0 1/2pi */));
    x = phys_sub(x, phys_mul(turns, rx_bf(0x40c90fdbu) /* 0x97749c 2pi */));
    const float r = phys_sub(rx_bf(0x40490fdbu) /* 0x977498 pi */, x);
    const float q = phys_mul(r, r);
    const float q2 = phys_mul(q, q);
    float pc = phys_mul(phys_add(phys_mul(q, rx_bf(0xb46beed0u)), rx_bf(0x37cb076bu)), q2);          /* 0x935d50, 0x935d48 */
    pc = phys_add(pc, phys_add(phys_mul(q, rx_bf(0xbab5a662u)), rx_bf(0x3d2aa372u)));                 /* 0x935d44, 0x935d4c */
    pc = phys_mul(pc, q2);
    pc = phys_add(pc, phys_add(phys_mul(q, rx_bf(0xbeffff3du)), rx_bf(0x3f7ffff3u)));                 /* 0x935d40, 0x935d3c */
    *cosine = phys_sub(-0.0f /* 0x9774a8 */, pc);
    float ps = phys_mul(phys_add(phys_mul(q, rx_bf(0xb2aee90au)), rx_bf(0x36352cc0u)), q2);          /* 0x935d68, 0x935d64 */
    ps = phys_add(ps, phys_add(phys_mul(q, rx_bf(0xb94fb563u)), rx_bf(0x3c0884a2u)));                 /* 0x935d60, 0x935d5c */
    ps = phys_mul(ps, q2);
    ps = phys_add(ps, phys_add(phys_mul(q, rx_bf(0xbe2aaa5cu)), rx_bf(0x3f7ffff9u)));                 /* 0x935d58, 0x935d54 */
    *sine = phys_mul(phys_mul(ps, r), sign);
}

/* the quintic ease-out inlined in 00656070 / 00658550: ((((u^3 u^2 - (u^2 5) u^2) + u^3 10) - u^2 10) + u 5) + 0 */
static float quintic_inline(float u) {
    const float u2 = phys_mul(u, u);
    const float u3 = phys_mul(u2, u);
    float r = phys_sub(phys_mul(u3, u2), phys_mul(phys_mul(u2, 5.0f), u2));          /* 0x935890 */
    r = phys_add(r, phys_mul(u3, 10.0f));                                             /* 0x92fd88 */
    r = phys_sub(r, phys_mul(u2, 10.0f));
    r = phys_add(r, phys_mul(u, 5.0f));
    return phys_add(r, 0.0f);
}

/* a point through the camera's 4x3 placement (+0x310 ..): x = ((m320 y + m310 x) + m330 z) + m340, y = ((m314 x + m324 y) + m334 z)
 * + m344, z = ((m318 x + m328 y) + m338 z) + m348; x_first: the x product written x * m310 (else m310 * x) */
static void transform_point(rx_addr cam, float x, float y, float z, int x_first, float out[3]) {
    const float xx = x_first ? phys_mul(x, F32(cam, 0x310)) : phys_mul(F32(cam, 0x310), x);
    out[0] = phys_add(phys_add(phys_add(phys_mul(F32(cam, 0x320), y), xx), phys_mul(F32(cam, 0x330), z)), F32(cam, 0x340));
    out[1] = phys_add(phys_add(phys_add(phys_mul(F32(cam, 0x314), x), phys_mul(F32(cam, 0x324), y)), phys_mul(F32(cam, 0x334), z)),
                      F32(cam, 0x344));
    out[2] = phys_add(phys_add(phys_add(phys_mul(F32(cam, 0x318), x), phys_mul(F32(cam, 0x328), y)), phys_mul(F32(cam, 0x338), z)),
                      F32(cam, 0x348));
}

/* a vec3 field copied through fld / fstp */
static void copy3_x87(rx_addr cam, uint32_t to, uint32_t from) {
    for (uint32_t k = 0; k < 12; k += 4) SU32(cam, to + k, bike_x87_copy32(U32(cam, from + k)));
}

/* a blend's clock: elapsed (+4) += dt while below the duration (+0); on reaching it the elapsed time is clamped and the start
 * (from) set to the goal (fld / fstp). Returns whether the blend was running (duration > elapsed before the step). */
static int blend_clock(rx_addr cam, uint32_t duration_off, float dt, uint32_t start_off, uint32_t goal_off) {
    const float duration = F32(cam, duration_off);
    float elapsed = F32(cam, duration_off + 4);
    if (!(duration > elapsed)) return 0;
    elapsed = phys_add(elapsed, dt);
    SF32(cam, duration_off + 4, elapsed);
    if (elapsed > duration) {
        SF32(cam, duration_off + 4, duration);
        copy3_x87(cam, start_off, goal_off);
    }
    return 1;
}

/* ---- 006596a0, 0063c890, 004073e0, 0063e160 ----------------------------------------------------------------------------------- */

float rx_ease_out_quintic(float t, float duration) {
    const float u = phys_div(t, duration);
    const float u2 = phys_mul(u, u);
    const float u3 = phys_mul(u2, u);
    float r = phys_sub(u3, phys_mul(u2, 5.0f));
    r = phys_sub(r, 10.0f);
    r = phys_mul(r, u2);
    r = phys_add(r, phys_mul(u3, 10.0f));
    return phys_add(r, phys_mul(u, 5.0f));
}
RX_FUNCTION(0x006596a0, rx_ease_out_quintic)

void rx_store_float(rx_addr p, float v) { SF32(p, 0, v); }
RX_FUNCTION(0x0063c890, rx_store_float)

rx_addr rx_matrix_row3(rx_addr out, rx_addr matrix, uint32_t row) {
    const rx_addr r = matrix + (row << 4);
    const uint32_t x = bike_x87_copy32(U32(r, 0));
    const float z = F32(r, 8), y = F32(r, 4);
    SU32(out, 0, x);
    SF32(out, 4, y);
    SF32(out, 8, z);
    return r;
}
RX_FUNCTION(0x004073e0, rx_matrix_row3)

/* 0063e160: f = target - eye (normalized when its length^2 (dz^2 + dx^2) + dy^2 is above zero); the rows: side (fz - fy 0,
 * fx 0 - fz 0, fy 0 - fx), up = f x side as the code expands it, f; the translation the eye. Frame: push ebp; and esp, -16;
 * sub esp, 0x30 (the eye at +4, f at +0x10, the side's z at +0x28). */
rx_addr rx_look_at_matrix(rx_addr out, rx_addr target, rx_addr eye) {
    const rx_addr entry = rx_sp();
    const rx_addr E = aligned_frame(entry, 0x30);
    rx_set_sp(E);
    float fx = phys_sub(F32(target, 0), F32(eye, 0));
    SF32(E, 4, F32(eye, 0));
    float fy = phys_sub(F32(target, 4), F32(eye, 4));
    SF32(E, 8, F32(eye, 4));
    float fz = phys_sub(F32(target, 8), F32(eye, 8));
    SF32(E, 0xc, F32(eye, 8));
    const float len2 = phys_add(phys_add(phys_mul(fz, fz), phys_mul(fx, fx)), phys_mul(fy, fy));
    SF32(E, 0x10, fx);
    SF32(E, 0x14, fy);
    SF32(E, 0x18, fz);
    SF32(E, 0, len2);
    if (len2 > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(E, F32(E, 0)));
        fy = phys_mul(inv, F32(E, 0x14));
        fx = phys_mul(F32(E, 0x10), inv);
        fz = phys_mul(F32(E, 0x18), inv);
    }
    SF32(out, 0xc, 0.0f);
    SF32(out, 0x1c, 0.0f);
    SF32(out, 0x2c, 0.0f);
    const float fy0 = phys_mul(fy, 0.0f);
    const float side_y = phys_sub(phys_mul(fx, 0.0f), phys_mul(fz, 0.0f));
    const float side_x = phys_sub(fz, fy0);
    const float side_z = phys_sub(fy0, fx);
    SF32(E, 0x28, side_z);
    const float up_x = phys_sub(phys_mul(fy, F32(E, 0x28)), phys_mul(fz, side_y));
    SF32(E, 0x10, up_x);
    const float up_y = phys_sub(phys_mul(fz, side_x), phys_mul(F32(E, 0x28), fx));
    SF32(E, 0x14, up_y);
    const float up_z = phys_sub(phys_mul(side_y, fx), phys_mul(fy, side_x));
    SF32(out, 0x3c, 1.0f);
    SF32(out, 0x08, F32(E, 0x28));
    SF32(out, 0x10, F32(E, 0x10));
    SF32(out, 0x14, F32(E, 0x14));
    SF32(out, 0x30, F32(E, 4));
    SF32(out, 0x34, F32(E, 8));
    SF32(out, 0x00, side_x);
    SF32(out, 0x04, side_y);
    SF32(out, 0x18, up_z);
    SF32(out, 0x20, fx);
    SF32(out, 0x24, fy);
    SF32(out, 0x28, fz);
    SF32(out, 0x38, F32(E, 0xc));
    rx_set_sp(entry);
    return out;
}
RX_FUNCTION(0x0063e160, rx_look_at_matrix)

/* ---- 00658550 / 00658d80: a direction blend ----------------------------------------------------------------------------------- */
/* From the start vector b (start_off) toward the goal a (goal_off) by the clock (duration_off / +4), u = elapsed / duration: the
 * length eases from |b| to |a| (the inline quintic), the direction turns from b about k = b x a (another axis when they are
 * parallel) by the eased (006596a0) angle between them: v = (k x b) sin + ((k x b) x k) cos + k (k . b), scaled to the length.
 * The angle: y = |a . b| (or sqrt|1 - dot^2| at |dot| >= 0.7071), the game's arc-sine approximation in doubles, pi/2 minus it.
 * Frame: push ebp; and esp, -16; sub esp, 0x60 (E): +0 / +4 |a|^2 / |b|^2, +8 |b|, +0xc u^3 then the length, +0x10 u^2 then |dot|,
 * +0x14 u then scratch, +0x18 / +0x1c the duration / elapsed then cos / sin, +0x20 k, +0x30 a then k x b, +0x40 b, +0x50
 * (k x b) x k. */
static rx_addr blend_direction(rx_addr cam, rx_addr out, uint32_t duration_off, uint32_t goal_off, uint32_t start_off) {
    const rx_addr entry = rx_sp();
    const rx_addr E = aligned_frame(entry, 0x60);
    rx_set_sp(E);
    const float elapsed = F32(cam, duration_off + 4);
    const float duration = F32(cam, duration_off);
    const float ax = F32(cam, goal_off), ay = F32(cam, goal_off + 4), az = F32(cam, goal_off + 8);
    const float bx = F32(cam, start_off), by = F32(cam, start_off + 4), bz = F32(cam, start_off + 8);
    SF32(E, 0x18, duration);
    SF32(E, 0x1c, elapsed);
    const float u = phys_div(elapsed, duration);
    const float u2 = phys_mul(u, u);
    SF32(E, 0x10, u2);
    const float u3 = phys_mul(u2, u);
    SF32(E, 0x14, u);
    SF32(E, 0x0c, u3);
    SF32(E, 0x00, phys_add(phys_add(phys_mul(ay, ay), phys_mul(ax, ax)), phys_mul(az, az)));
    SF32(E, 0x04, phys_add(phys_add(phys_mul(by, by), phys_mul(bx, bx)), phys_mul(bz, bz)));
    SF32(E, 0x40, bx); SF32(E, 0x44, by); SF32(E, 0x48, bz);
    SF32(E, 0x30, ax); SF32(E, 0x34, ay); SF32(E, 0x38, az);
    rx_set_sp(E - 8);                                                   /* push ecx: the argument slot E - 4 serves both calls */
    SF32(E - 4, 0, bike_x87_copyf(F32(E, 4)));
    const float len_b = phys_sqrtf(bike_x87_copyf(F32(E, 4)));
    SF32(E - 4, 0, bike_x87_copyf(F32(E, 0)));
    SF32(E, 0x08, len_b);
    const float len_a = phys_sqrtf(bike_x87_copyf(F32(E, 0)));
    rx_set_sp(E);
    {   /* the length: (|a| - |b|) * ease + |b| */
        const float ease = quintic_inline(F32(E, 0x14));
        SF32(E, 0x0c, phys_add(phys_mul(phys_sub(len_a, F32(E, 8)), ease), F32(E, 8)));
    }
    /* a and b normalized (a in memory, b in memory when normalized) */
    SF32(E, 0x14, F32(E, 0));
    if (F32(E, 0x14) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(E, F32(E, 0x14)));
        SF32(E, 0x30, phys_mul(inv, F32(E, 0x30)));
        SF32(E, 0x34, phys_mul(F32(E, 0x34), inv));
        SF32(E, 0x38, phys_mul(F32(E, 0x38), inv));
    }
    SF32(E, 0x14, F32(E, 4));
    float nbx, nby, nbz;
    if (F32(E, 0x14) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(E, F32(E, 0x14)));
        nbx = phys_mul(inv, F32(E, 0x40));
        nby = phys_mul(F32(E, 0x44), inv);
        nbz = phys_mul(F32(E, 0x48), inv);
        SF32(E, 0x40, nbx); SF32(E, 0x44, nby); SF32(E, 0x48, nbz);
    } else {
        nbx = F32(E, 0x40); nby = F32(E, 0x44); nbz = F32(E, 0x48);
    }
    /* the axis k = b x a */
    const float nax = F32(E, 0x30), nay = F32(E, 0x34), naz = F32(E, 0x38);
    const float cx = phys_sub(phys_mul(naz, nby), phys_mul(nay, nbz));
    const float cy = phys_sub(phys_mul(nbz, nax), phys_mul(naz, nbx));
    const float cz = phys_sub(phys_mul(nay, nbx), phys_mul(nby, nax));
    SF32(E, 0x24, cy);
    SF32(E, 0x20, cx);
    SF32(E, 0x28, cz);
    const float len_c = sqrt_arg(E, phys_add(phys_add(phys_mul(cy, cy), phys_mul(cx, cx)), phys_mul(cz, cz)));
    float kx, ky, kz;
    if (len_c == 0.0f) {
        /* parallel: an axis from a and the world up (0, 1, 0) as the code expands the products */
        const float ax0 = phys_mul(nax, 0.0f), az0 = phys_mul(naz, 0.0f);
        const float d = phys_add(phys_add(ax0, nay), az0);
        if (d == 1.0f || d == -1.0f) {
            kx = phys_sub(phys_mul(nay, 0.0f), az0);
            ky = phys_sub(naz, ax0);
            kz = phys_sub(ax0, nay);
        } else {
            const float t2 = phys_sub(az0, ax0);
            const float ay0 = phys_mul(nay, 0.0f);
            const float t0 = phys_sub(nax, ay0);
            const float t5 = phys_sub(ay0, naz);
            kx = phys_sub(phys_mul(nay, t0), phys_mul(naz, t2));
            ky = phys_sub(phys_mul(naz, t5), phys_mul(t0, nax));
            kz = phys_sub(phys_mul(t2, nax), phys_mul(nay, t5));
        }
        SF32(E, 0x20, kx); SF32(E, 0x24, ky); SF32(E, 0x28, kz);
    } else {
        kx = F32(E, 0x20); ky = F32(E, 0x24); kz = F32(E, 0x28);
    }
    SF32(E, 0x14, phys_add(phys_add(phys_mul(ky, ky), phys_mul(kx, kx)), phys_mul(kz, kz)));
    if (F32(E, 0x14) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(E, F32(E, 0x14)));
        SF32(E, 0x20, phys_mul(F32(E, 0x20), inv));
        SF32(E, 0x24, phys_mul(F32(E, 0x24), inv));
        SF32(E, 0x28, phys_mul(F32(E, 0x28), inv));
    }
    /* the angle between a and b */
    const float dot = phys_add(phys_add(phys_mul(F32(E, 0x34), F32(E, 0x44)), phys_mul(F32(E, 0x30), F32(E, 0x40))),
                               phys_mul(F32(E, 0x38), F32(E, 0x48)));
    SF32(E, 0x14, dot);
    const float abs_dot = rx_world_float_abs_thunk(dot);
    SF32(E, 0x10, abs_dot);
    SF32(E, 0x14, dot);
    const int wide = abs_dot >= rx_bf(0x3f350481u);                    /* 0x9ddfc8 0.7071 (comiss; jb) */
    float y;
    if (wide) {
        const float sq = (float)((double)dot * (double)dot);
        rx_set_sp(E - 8);
        const float v = rx_world_float_abs_thunk((float)(1.0 - (double)sq));
        SF32(E - 4, 0, v);
        y = phys_sqrtf(v);
        rx_set_sp(E);
    } else {
        y = abs_dot;
    }
    const float y2 = (float)((double)y * (double)y);
    const float y4 = (float)((double)y2 * (double)y2);
    const float p1 = (float)(0x1.212ebp-1 - (double)y2 * 0x1.9903acp-5);                /* 0x9777b0, 0x9777a8 */
    const float q1 = (float)(1.0 - (double)y2 * 0x1.716ac6p+0);                         /* 0x977638, 0x9777a0 */
    const float r = (float)(1.0 / ((double)p1 * (double)y4 + (double)q1));
    const float s1 = (float)(0x1.98e2fep-2 - (double)y2 * 0x1.3d7fp-6);                 /* 0x977798, 0x977790 */
    const float s2 = (float)(1.0 - (double)y2 * 0x1.46c01ap+0);                         /* 0x977788 */
    const float num = (float)(((double)s1 * (double)y4 + (double)s2) * (double)y);
    float angle = (float)((double)r * (double)num);
    if (wide) angle = (float)(0x1.921fb6p+0 - (double)angle);                           /* 0x977510 pi/2 */
    const float signed_angle = F32(E, 0x14) >= 0.0f ? angle : phys_sub(-0.0f, angle);
    /* the eased turn */
    rx_set_sp(E - 8);
    SF32(E - 4, 0, bike_x87_copyf(F32(E, 0x18)));
    const float ease = rx_ease_out_quintic(F32(E, 0x1c), bike_x87_copyf(F32(E, 0x18)));
    rx_set_sp(E);
    const float theta = (float)(0x1.921fb6p+0 - (double)signed_angle);
    float sine, cosine;
    fast_sincos(phys_mul(ease, theta), &sine, &cosine);
    SF32(E, 0x18, cosine);
    SF32(E, 0x1c, sine);
    cross_at(E, E + 0x30, E + 0x20, E + 0x40);                         /* k x b */
    cross_at(E, E + 0x50, E + 0x30, E + 0x20);                         /* (k x b) x k */
    const float k_x = F32(E, 0x20), k_y = F32(E, 0x24);
    const float kb_x = F32(E, 0x30), kb_y = F32(E, 0x34);
    const float kd = phys_add(phys_add(phys_mul(k_y, F32(E, 0x44)), phys_mul(k_x, F32(E, 0x40))), phys_mul(F32(E, 0x28), F32(E, 0x48)));
    SF32(E, 0x28, phys_mul(kd, F32(E, 0x28)));
    const float t4 = phys_mul(kd, k_x);
    const float t1 = phys_mul(F32(E, 0x38), F32(E, 0x1c));
    const float t5 = phys_mul(kd, k_y);
    const float t6 = phys_mul(kb_x, F32(E, 0x1c));
    const float t7 = phys_mul(kb_y, F32(E, 0x1c));
    SF32(E, 0x48, t1);
    const float c = F32(E, 0x18);
    float o1 = phys_mul(F32(E, 0x50), c);
    float o2 = phys_mul(F32(E, 0x54), c);
    float o3 = phys_mul(F32(E, 0x58), c);
    o3 = phys_add(phys_add(o3, F32(E, 0x48)), F32(E, 0x28));
    const float len = F32(E, 0x0c);
    o1 = phys_add(phys_add(o1, t6), t4);
    o2 = phys_add(phys_add(o2, t7), t5);
    SF32(out, 0, phys_mul(o1, len));
    SF32(out, 4, phys_mul(o2, len));
    SF32(out, 8, phys_mul(o3, len));
    rx_set_sp(entry);
    return out;
}
rx_addr rx_parts_view_blend_offset(rx_addr cam, rx_addr out) { return blend_direction(cam, out, 0x3c0, 0x370, 0x380); }
RX_FUNCTION(0x00658550, rx_parts_view_blend_offset)
rx_addr rx_parts_view_blend_target(rx_addr cam, rx_addr out) { return blend_direction(cam, out, 0x3c8, 0x350, 0x360); }
RX_FUNCTION(0x00658d80, rx_parts_view_blend_target)

/* ---- 00656070 MXUIPartsViewCamera::vf19 ---------------------------------------------------------------------------------------- */
/* Frame: push ebp; and esp, -16; sub esp, 0x134; push ebx / esi / edi (B = ESP after the pushes). The slots (B + ...): 0x0c a length^2
 * (the square-root argument copy) / the distance, 0x10 a vector (a cross product, the first matrix row), 0x2c / 0x6c the two stick
 * values (then scratch), 0x30 a vector (an axis, the third row), 0x4c a sine / cosine / length^2, 0x50 a vector (a cross product,
 * the second row), 0x70 the eye, 0x80 the up vector, 0x90 the look-at point, 0xac / 0xb0 scratch (0xb0 the blend output), 0xc0 the
 * look-at matrix (copied from 0x100). */

/* the zoom: the eye moves along (target - eye) normalized by (+0x494 - +0x490) * +0x48c * |target - eye| */
static float zoom_factor(rx_addr cam, float distance) {
    return phys_mul(phys_mul(phys_sub(F32(cam, 0x494), F32(cam, 0x490)), F32(cam, 0x48c)), distance);
}

/* the look-at matrix of the eye B + 0x70 and the point B + 0x90 (0063e160 into B + 0x100, copied to B + 0xc0), its rows 2 / 1 / 0
 * into B + 0x30 / B + 0x50 / B + 0x10 (004073e0) */
static void view_rows(rx_addr B) {
    rx_set_sp(B - 4);
    rx_look_at_matrix(B + 0x100, B + 0x90, B + 0x70);
    rx_set_sp(B);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(B, 0xc0 + k, U32(B, 0x100 + k));      /* rep movsd */
    rx_set_sp(B - 8);
    SU32(B - 4, 0, B + 0xc0);
    rx_matrix_row3(B + 0x30, B + 0xc0, 2);
    rx_matrix_row3(B + 0x50, B + 0xc0, 1);
    rx_matrix_row3(B + 0x10, B + 0xc0, 0);
    rx_set_sp(B);
}

/* +0x3e0 == 0 (006560cd): the orbit. The stick turns the yaw / pitch (+0x3ec / +0x3f0); without input for +0x448 seconds the yaw
 * auto-rotates by the eased rate +0x420. The offset direction is (0, 0, 1) turned by the pitch about x, then by the yaw about y
 * (the inlined Rodrigues rotations), times the distance +0x3d4, into +0x370 (the eye offset goal). Then the three blends, the zoom
 * and the camera-space shift by the up vector. */
static void orbit_update(rx_addr cam, float dt, rx_addr B) {
    const uint32_t pad = U32(cam, 0x450);
    SF32(B, 0x80, 0.0f);
    SF32(B, 0x84, 0.0f);
    SF32(B, 0x88, 1.0f);
    SF32(B, 0x2c, 0.0f);
    SF32(B, 0x6c, 0.0f);
    int idle = 1;                                               /* 00656177: no stick input */
    if ((int32_t)pad >= 0) {
        SF32(B, 0x2c, key_value(B, 0x00d5b258, pad));
        SF32(B, 0x6c, key_value(B, 0x00d5dbc8, pad));
        if (!(F32(B, 0x2c) == 0.0f)) idle = 0;                  /* fucompi with 0; jp */
        else if (F32(B, 0x6c) == 0.0f) idle = 1;
        else if (phys_sub(F32(cam, 0x430), F32(cam, 0x434)) > 0.0f) idle = 0;
    }
    if (idle) {
        if (U32(cam, 0x41c) != 0) {
            const float delay = F32(cam, 0x448);
            const float t = phys_add(dt, F32(cam, 0x44c));
            SF32(cam, 0x44c, t);
            if (t > delay) {
                const float limit = phys_mul(delay, 2.0f);      /* 0x92e330 */
                if (t > limit) SF32(cam, 0x44c, limit);
                const float u = phys_div(phys_sub(F32(cam, 0x44c), delay), delay);
                const float ease = quintic_inline(u);
                const float rate = phys_mul(F32(cam, 0x420), dt);
                SF32(cam, 0x3ec, phys_add(phys_mul(ease, rate), F32(cam, 0x3ec)));
            }
        }
    } else {
        SF32(cam, 0x44c, 0.0f);
    }
    /* 0065624f: the stick */
    const float pitch = phys_add(phys_mul(phys_mul(F32(cam, 0x428), F32(B, 0x6c)), dt), F32(cam, 0x3f0));
    float yaw = phys_add(phys_mul(phys_mul(F32(cam, 0x424), F32(B, 0x2c)), dt), F32(cam, 0x3ec));
    SF32(cam, 0x3ec, yaw);
    SF32(cam, 0x3f0, pitch);
    if (pitch > F32(cam, 0x430)) SF32(cam, 0x3f0, F32(cam, 0x430));
    if (F32(cam, 0x434) > F32(cam, 0x3f0)) SF32(cam, 0x3f0, F32(cam, 0x434));
    if (yaw >= 180.0f) {                                        /* 0x977bc4 */
        yaw = phys_sub(phys_sub(yaw, 180.0f), 180.0f);
        SF32(cam, 0x3ec, yaw);
    }
    yaw = F32(cam, 0x3ec);
    if (-180.0f >= yaw) {                                       /* 0x977bc0 */
        yaw = phys_add(phys_add(yaw, 180.0f), 180.0f);
        SF32(cam, 0x3ec, yaw);
    }

    /* 00656303: (0, 0, 1) about the x axis by -pitch */
    float sine, cosine;
    SF32(B, 0x30, 1.0f);
    SF32(B, 0x34, 0.0f);
    SF32(B, 0x38, 0.0f);
    fast_sincos(phys_mul(phys_sub(-0.0f, F32(cam, 0x3f0)), rx_bf(0x3c8efa34u) /* 0x977678 */), &sine, &cosine);
    SF32(B, 0x2c, cosine);
    SF32(B, 0x4c, sine);
    cross_at(B, B + 0x50, B + 0x30, B + 0x80);                 /* x x v */
    cross_at(B, B + 0x10, B + 0x50, B + 0x30);                 /* (x x v) x x */
    {
        const float d = phys_add(phys_add(phys_mul(0.0f, 0.0f), 0.0f), 0.0f);  /* x . v as the code folds it */
        const float d0 = phys_mul(d, 0.0f);
        SF32(B, 0x98, d0);
        const float s = F32(B, 0x4c), c = F32(B, 0x2c);
        const float s_y = phys_mul(s, F32(B, 0x54));
        const float s_x = phys_mul(F32(B, 0x50), s);
        SF32(B, 0xb8, phys_mul(s, F32(B, 0x58)));
        float vx = phys_mul(F32(B, 0x10), c);
        float vy = phys_mul(c, F32(B, 0x14));
        float vz = phys_add(phys_mul(c, F32(B, 0x18)), F32(B, 0xb8));
        vx = phys_add(vx, s_x);
        vy = phys_add(phys_add(vy, s_y), d0);
        SF32(B, 0x34, vy);
        SF32(B, 0x84, vy);
        vx = phys_add(vx, d);
        vz = phys_add(vz, d0);
        SF32(B, 0x30, vx);
        SF32(B, 0x38, vz);
        SF32(B, 0x80, vx);
        SF32(B, 0x88, vz);
    }
    /* 0065652a: about the y axis by the yaw */
    SF32(B, 0x50, 0.0f);
    SF32(B, 0x54, 1.0f);
    SF32(B, 0x58, 0.0f);
    fast_sincos(phys_mul(F32(cam, 0x3ec), rx_bf(0x3c8efa34u)), &sine, &cosine);
    SF32(B, 0x2c, cosine);
    SF32(B, 0x4c, sine);
    cross_at(B, B + 0x10, B + 0x50, B + 0x80);                 /* y x v */
    cross_at(B, B + 0x80, B + 0x10, B + 0x50);                 /* (y x v) x y */
    float nx, ny, nz;
    {
        const float d = phys_add(phys_add(phys_mul(F32(B, 0x30), 0.0f), F32(B, 0x34)), phys_mul(F32(B, 0x38), 0.0f));  /* y . v */
        const float d0 = phys_mul(d, 0.0f);
        SF32(B, 0x38, d0);
        const float s = F32(B, 0x4c), c = F32(B, 0x2c);
        const float s_y = phys_mul(s, F32(B, 0x14));
        const float s_x = phys_mul(F32(B, 0x10), s);
        SF32(B, 0xb8, phys_mul(s, F32(B, 0x18)));
        const float c_y = phys_mul(c, F32(B, 0x84));
        float vx = phys_mul(F32(B, 0x80), c);
        float vz = phys_add(phys_mul(c, F32(B, 0x88)), F32(B, 0xb8));
        vx = phys_add(phys_add(vx, s_x), d0);
        vz = phys_add(vz, d0);
        const float vy = phys_add(phys_add(c_y, s_y), d);
        SF32(B, 0x84, vy);
        SF32(B, 0x80, vx);
        const float len2 = phys_add(phys_add(phys_mul(vy, vy), phys_mul(vx, vx)), phys_mul(vz, vz));
        SF32(B, 0x88, vz);
        SF32(B, 0x4c, len2);
        nx = vx; ny = vy; nz = vz;
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x4c)));
            ny = phys_mul(inv, F32(B, 0x84));
            nx = phys_mul(F32(B, 0x80), inv);
            nz = phys_mul(inv, F32(B, 0x88));
        }
    }
    {   /* 00656793: the eye offset goal */
        const float distance = F32(cam, 0x3d4);
        SF32(cam, 0x370, phys_mul(nx, distance));
        SF32(cam, 0x374, phys_mul(distance, ny));
        SF32(cam, 0x378, phys_mul(distance, nz));
    }

    /* 006567c2: the look-at point */
    float p[3];
    if (blend_clock(cam, 0x3c8, dt, 0x360, 0x350)) {
        rx_set_sp(B - 4);
        const rx_addr v = rx_parts_view_blend_target(cam, B + 0xb0);
        rx_set_sp(B);
        const float x = F32(v, 0), z = F32(v, 8), y = F32(v, 4);
        SF32(cam, 0x3a0, x); SF32(cam, 0x3a8, z); SF32(cam, 0x3a4, y);
        transform_point(cam, x, y, z, 1, p);
    } else {
        transform_point(cam, F32(cam, 0x350), F32(cam, 0x354), F32(cam, 0x358), 1, p);
        copy3_x87(cam, 0x3a0, 0x350);
    }
    SF32(B, 0x94, p[1]); SF32(B, 0x90, p[0]); SF32(B, 0x98, p[2]);
    /* 006569d0: the eye */
    if (blend_clock(cam, 0x3c0, dt, 0x380, 0x370)) {
        rx_set_sp(B - 4);
        const rx_addr v = rx_parts_view_blend_offset(cam, B + 0xb0);
        rx_set_sp(B);
        const float x = F32(v, 0), z = F32(v, 8), y = F32(v, 4);
        SF32(cam, 0x390, x); SF32(cam, 0x398, z); SF32(cam, 0x394, y);
        transform_point(cam, x, y, z, 1, p);
    } else {
        transform_point(cam, F32(cam, 0x370), F32(cam, 0x374), F32(cam, 0x378), 0, p);
        copy3_x87(cam, 0x390, 0x370);
    }
    SF32(B, 0x74, p[1]); SF32(B, 0x70, p[0]); SF32(B, 0x78, p[2]);
    /* 00656bee: the up vector */
    if (blend_clock(cam, 0x480, dt, 0x470, 0x460)) {
        const float u = phys_div(F32(cam, 0x484), F32(cam, 0x480));
        SF32(B, 0x84, phys_sub(F32(cam, 0x464), F32(cam, 0x474)));
        const float dx = phys_sub(F32(cam, 0x460), F32(cam, 0x470));
        SF32(B, 0x88, phys_sub(F32(cam, 0x468), F32(cam, 0x478)));
        const float ease = quintic_inline(u);
        const float uy = phys_add(F32(cam, 0x474), phys_mul(ease, F32(B, 0x84)));
        const float ux = phys_add(phys_mul(dx, ease), F32(cam, 0x470));
        const float uz = phys_add(F32(cam, 0x478), phys_mul(ease, F32(B, 0x88)));
        SF32(B, 0x80, ux); SF32(B, 0x84, uy); SF32(B, 0x88, uz);
        SF32(cam, 0x3b0, ux); SF32(cam, 0x3b4, uy); SF32(cam, 0x3b8, uz);
    } else {
        SF32(B, 0x80, F32(cam, 0x460));
        SF32(B, 0x84, F32(cam, 0x464));
        SF32(B, 0x88, F32(cam, 0x468));
        copy3_x87(cam, 0x3b0, 0x460);
    }

    /* 00656db8: the zoom */
    {
        const float dy = phys_sub(F32(B, 0x94), F32(B, 0x74));
        const float dx = phys_sub(F32(B, 0x90), F32(B, 0x70));
        const float dz = phys_sub(F32(B, 0x98), F32(B, 0x78));
        SF32(B, 0x6c, dy);
        SF32(B, 0x2c, dx);
        SF32(B, 0x4c, dz);
        SF32(B, 0x0c, sqrt_arg(B, phys_add(phys_add(phys_mul(dy, dy), phys_mul(dx, dx)), phys_mul(dz, dz))));
        const float x = F32(B, 0x2c), y = F32(B, 0x6c), z = F32(B, 0x4c);
        const float len2 = phys_add(phys_add(phys_mul(y, y), phys_mul(x, x)), phys_mul(z, z));
        SF32(B, 0x30, x); SF32(B, 0x34, y); SF32(B, 0x38, z);
        SF32(B, 0x4c, len2);
        float ux = x, uy = y, uz = z;
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x4c)));
            uy = phys_mul(inv, F32(B, 0x34));
            ux = phys_mul(F32(B, 0x30), inv);
            uz = phys_mul(inv, F32(B, 0x38));
        }
        const float k = zoom_factor(cam, F32(B, 0x0c));
        const float ex = phys_add(phys_mul(ux, k), F32(B, 0x70));
        const float ey = phys_add(phys_mul(k, uy), F32(B, 0x74));
        const float ez = phys_add(phys_mul(k, uz), F32(B, 0x78));
        SF32(B, 0x70, ex); SF32(B, 0x74, ey); SF32(B, 0x78, ez);
    }

    /* 00656ef2: the view's rows, normalized (row 2 only in registers), and the shift of both points by up in view space */
    view_rows(B);
    float r0x = F32(B, 0x10), r0y = F32(B, 0x14), r0z = F32(B, 0x18);
    SF32(B, 0x0c, phys_add(phys_add(phys_mul(r0y, r0y), phys_mul(r0x, r0x)), phys_mul(r0z, r0z)));
    if (F32(B, 0x0c) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
        r0y = phys_mul(inv, F32(B, 0x14));
        r0x = phys_mul(F32(B, 0x10), inv);
        r0z = phys_mul(inv, F32(B, 0x18));
        SF32(B, 0x10, r0x); SF32(B, 0x14, r0y); SF32(B, 0x18, r0z);
    }
    {
        const float x = F32(B, 0x50), y = F32(B, 0x54), z = F32(B, 0x58);
        SF32(B, 0x0c, phys_add(phys_add(phys_mul(y, y), phys_mul(x, x)), phys_mul(z, z)));
        if (F32(B, 0x0c) > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
            SF32(B, 0x50, phys_mul(inv, F32(B, 0x50)));
            const float ny1 = phys_mul(inv, F32(B, 0x54));
            SF32(B, 0x58, phys_mul(inv, F32(B, 0x58)));
            SF32(B, 0x54, ny1);
        }
    }
    float r2x = F32(B, 0x30), r2y = F32(B, 0x34), r2z = F32(B, 0x38);
    SF32(B, 0x0c, phys_add(phys_add(phys_mul(r2y, r2y), phys_mul(r2x, r2x)), phys_mul(r2z, r2z)));
    if (F32(B, 0x0c) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
        r2x = phys_mul(inv, F32(B, 0x30));
        r2z = phys_mul(inv, F32(B, 0x38));
        r2y = phys_mul(inv, F32(B, 0x34));
    }
    {
        const float upx = F32(B, 0x80), upy = F32(B, 0x84), upz = F32(B, 0x88);
        SF32(B, 0x14, phys_mul(r0y, upx));
        const float a_x = phys_mul(F32(B, 0x50), upy);
        const float a0x = phys_mul(r0x, upx);
        SF32(B, 0x18, phys_mul(r0z, upx));
        const float oy = phys_add(phys_mul(F32(B, 0x54), upy), F32(B, 0x14));
        const float oz = phys_add(phys_mul(F32(B, 0x58), upy), F32(B, 0x18));
        SF32(B, 0x30, phys_mul(r2x, upz));
        const float o2y = phys_mul(r2y, upz);
        const float o2z = phys_mul(r2z, upz);
        const float ox = phys_add(a0x, a_x);
        const float eye_x = phys_add(phys_add(ox, F32(B, 0x30)), F32(B, 0x70));
        const float at_x = phys_add(phys_add(ox, F32(B, 0x30)), F32(B, 0x90));
        const float eye_y = phys_add(phys_add(oy, o2y), F32(B, 0x74));
        const float eye_z = phys_add(phys_add(oz, o2z), F32(B, 0x78));
        const float at_y = phys_add(phys_add(oy, o2y), F32(B, 0x94));
        const float at_z = phys_add(phys_add(oz, o2z), F32(B, 0x98));
        SF32(B, 0x70, eye_x); SF32(B, 0x74, eye_y); SF32(B, 0x78, eye_z);
        SF32(B, 0x90, at_x); SF32(B, 0x94, at_y); SF32(B, 0x98, at_z);
    }
}

/* the eye offset / look-at point blend when its clock runs (B path): goal - start eased by 006596a0 */
static void eased_lerp(rx_addr B, rx_addr cam, uint32_t duration_off, uint32_t goal_off, uint32_t start_off, uint32_t dz_slot,
                       float out[3]) {
    const float dz = phys_sub(F32(cam, goal_off + 8), F32(cam, start_off + 8));
    const float dx = phys_sub(F32(cam, goal_off), F32(cam, start_off));
    const float dy = phys_sub(F32(cam, goal_off + 4), F32(cam, start_off + 4));
    if (dz_slot == 0x84) SF32(B, 0x84, dy);
    rx_set_sp(B - 8);                                                   /* push ecx; fstp [esp] (the duration); call */
    SF32(B - 4, 0, bike_x87_copyf(F32(cam, duration_off)));
    SF32(B, dz_slot == 0x84 ? 0x88 : dz_slot, dz);
    const float ease = rx_ease_out_quintic(F32(cam, duration_off + 4), bike_x87_copyf(F32(cam, duration_off)));
    rx_set_sp(B);
    out[0] = phys_add(phys_mul(dx, ease), F32(cam, start_off));
    out[1] = dz_slot == 0x84 ? phys_add(F32(cam, start_off + 4), phys_mul(ease, F32(B, 0x84)))
                             : phys_add(F32(cam, start_off + 4), phys_mul(ease, dy));
    out[2] = phys_add(F32(cam, start_off + 8), phys_mul(ease, F32(B, dz_slot == 0x84 ? 0x88 : dz_slot)));
}

/* +0x3e0 != 0 (0065721b): the springs and the blends. The two angle springs (0063c8c0: 10000, FLT_MAX, 2) clamped to their
 * limits give the angles +0x3f4 (yaw) / +0x3f8 (pitch). The look-at point, eye and up blends (eased lerps, the eye by 00658550).
 * When all three are done, the stick drives the springs' targets and the eye is the look-at point plus (eye - point) turned by the
 * pitch about the side axis and by the yaw about y. Then the zoom and the view-space shift by the up vector, as in the orbit. */
static void spring_update(rx_addr cam, float dt, rx_addr B) {
    const rx_addr yaw_spring = cam + 0x3fc, pitch_spring = cam + 0x408;
    for (int i = 0; i < 2; i++) {
        const rx_addr s = i == 0 ? yaw_spring : pitch_spring;
        rx_set_sp(B - 0x14);                                             /* sub esp, 0x10; call */
        SF32(B - 0x10, 0x0, bike_x87_copyf(dt));
        SF32(B - 0x10, 0x4, 10000.0f);                                   /* 0x931e98 */
        SF32(B - 0x10, 0x8, rx_bf(0x7f7fffffu));                         /* 0x977494 FLT_MAX */
        SF32(B - 0x10, 0xc, 2.0f);                                       /* 0x92e330 */
        rx_cam_spring(s, bike_x87_copyf(dt), 10000.0f, rx_bf(0x7f7fffffu), 2.0f);
        rx_set_sp(B);
    }
    /* 00657281: the limits */
    rx_set_sp(B - 4);
    SF32(B, 0x0c, F32(cam, 0x440));
    if (rx_float_get(yaw_spring) > F32(B, 0x0c)) rx_store_float(yaw_spring, F32(B, 0x0c));
    SF32(B, 0x0c, F32(cam, 0x444));
    if (F32(B, 0x0c) > rx_float_get(yaw_spring)) rx_store_float(yaw_spring, F32(B, 0x0c));
    SF32(B, 0x0c, F32(cam, 0x438));
    if (rx_float_get(pitch_spring) > F32(B, 0x0c)) rx_store_float(pitch_spring, F32(B, 0x0c));
    SF32(B, 0x0c, F32(cam, 0x43c));
    if (F32(B, 0x0c) > rx_float_get(pitch_spring)) rx_store_float(pitch_spring, F32(B, 0x0c));
    SF32(cam, 0x3f4, rx_float_get(yaw_spring));
    SF32(cam, 0x3f8, rx_float_get(pitch_spring));
    rx_set_sp(B);

    /* 00657345: the look-at point */
    float p[3];
    if (blend_clock(cam, 0x3c8, dt, 0x360, 0x350)) {
        float v[3];
        eased_lerp(B, cam, 0x3c8, 0x350, 0x360, 0xb8, v);
        SF32(cam, 0x3a4, v[1]);
        SF32(cam, 0x3a8, v[2]);
        SF32(cam, 0x3a0, v[0]);
        transform_point(cam, v[0], v[1], v[2], 1, p);
    } else {
        transform_point(cam, F32(cam, 0x350), F32(cam, 0x354), F32(cam, 0x358), 1, p);
        copy3_x87(cam, 0x3a0, 0x350);
    }
    SF32(B, 0x90, p[0]); SF32(B, 0x94, p[1]); SF32(B, 0x98, p[2]);
    /* 006575f3: the eye */
    if (blend_clock(cam, 0x3c0, dt, 0x380, 0x370)) {
        rx_set_sp(B - 4);
        const rx_addr v = rx_parts_view_blend_offset(cam, B + 0xb0);
        rx_set_sp(B);
        const float x = F32(v, 0), y = F32(v, 4), z = F32(v, 8);
        SF32(cam, 0x390, x); SF32(cam, 0x394, y); SF32(cam, 0x398, z);
        transform_point(cam, x, y, z, 1, p);
    } else {
        transform_point(cam, F32(cam, 0x370), F32(cam, 0x374), F32(cam, 0x378), 1, p);
        copy3_x87(cam, 0x390, 0x370);
    }
    SF32(B, 0x78, p[2]); SF32(B, 0x74, p[1]); SF32(B, 0x70, p[0]);
    /* 006577fb: the up vector */
    if (blend_clock(cam, 0x480, dt, 0x470, 0x460)) {
        float v[3];
        eased_lerp(B, cam, 0x480, 0x460, 0x470, 0x84, v);
        SF32(B, 0x80, v[0]); SF32(B, 0x84, v[1]); SF32(B, 0x88, v[2]);
        SF32(cam, 0x3b0, v[0]); SF32(cam, 0x3b4, v[1]); SF32(cam, 0x3b8, v[2]);
    } else {
        SF32(B, 0x80, F32(cam, 0x460));
        SF32(B, 0x84, F32(cam, 0x464));
        SF32(B, 0x88, F32(cam, 0x468));
        copy3_x87(cam, 0x3b0, 0x460);
    }

    /* 0065797c: the stick, once the three blends are done */
    float eye_x = F32(B, 0x70);
    if (F32(cam, 0x3c4) >= F32(cam, 0x3c0) && F32(cam, 0x484) >= F32(cam, 0x480) && F32(cam, 0x3cc) >= F32(cam, 0x3c8)) {
        const uint32_t pad = U32(cam, 0x450);
        SF32(B, 0x6c, 0.0f);
        SF32(B, 0x2c, 0.0f);
        float yaw_input = 0.0f;
        if ((int32_t)pad >= 0) {
            SF32(B, 0x6c, key_value(B, 0x00d5b258, pad));
            SF32(B, 0x2c, key_value(B, 0x00d5dbc8, pad));
            yaw_input = F32(B, 0x6c);
            eye_x = F32(B, 0x70);
        }
        /* the direction d = eye - point (B + 0x10) and the side axis n = d x (0, 1, 0) as the code expands it (B + 0x30, normalized
         * into B + 0x50) */
        const float dy = phys_sub(F32(B, 0x74), F32(B, 0x94));
        const float dx = phys_sub(eye_x, F32(B, 0x90));
        const float dz = phys_sub(F32(B, 0x78), F32(B, 0x98));
        SF32(B, 0x10, dx);
        SF32(B, 0x14, dy);
        const float len2 = phys_add(phys_add(phys_mul(dx, dx), phys_mul(dy, dy)), phys_mul(dz, dz));
        SF32(B, 0x18, dz);
        SF32(B, 0x0c, len2);
        float nx, ny, nz;
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
            yaw_input = F32(B, 0x6c);
            ny = phys_mul(inv, F32(B, 0x14));
            nx = phys_mul(F32(B, 0x10), inv);
            nz = phys_mul(inv, F32(B, 0x18));
        } else {                                                         /* 00657ab5: the stack's B + 0xb0 .. as they are */
            nz = F32(B, 0xb8);
            ny = F32(B, 0xb4);
            nx = F32(B, 0xb0);
        }
        const float side_x = phys_sub(nz, phys_mul(ny, 0.0f));
        const float side_y = phys_sub(phys_mul(nx, 0.0f), phys_mul(nz, 0.0f));
        const float side_z = phys_sub(phys_mul(ny, 0.0f), nx);
        SF32(B, 0x34, side_y); SF32(B, 0x54, side_y);
        SF32(B, 0x30, side_x); SF32(B, 0x50, side_x);
        const float side2 = phys_add(phys_add(phys_mul(side_y, side_y), phys_mul(side_x, side_x)), phys_mul(side_z, side_z));
        SF32(B, 0x38, side_z); SF32(B, 0x58, side_z);
        SF32(B, 0x0c, side2);
        if (side2 > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
            yaw_input = F32(B, 0x6c);
            SF32(B, 0x50, phys_mul(F32(B, 0x30), inv));
            SF32(B, 0x54, phys_mul(F32(B, 0x34), inv));
            SF32(B, 0x58, phys_mul(F32(B, 0x38), inv));
        }
        /* 00657b8c: the springs' targets from the stick (0 at rest; |input| * the rate of its sign) */
        rx_set_sp(B - 4);
        if (yaw_input == 0.0f) rx_float_set4(yaw_spring, 0.0f);
        else if (0.0f > yaw_input) rx_float_set4(yaw_spring, phys_mul(rx_world_float_abs(yaw_input), F32(cam, 0x440)));
        else if (yaw_input > 0.0f) rx_float_set4(yaw_spring, phys_mul(rx_world_float_abs(yaw_input), F32(cam, 0x444)));
        const float pitch_input = F32(B, 0x2c);
        if (pitch_input == 0.0f) rx_float_set4(pitch_spring, 0.0f);
        else if (0.0f > pitch_input) rx_float_set4(pitch_spring, phys_mul(rx_world_float_abs(pitch_input), F32(cam, 0x438)));
        else if (pitch_input > 0.0f) rx_float_set4(pitch_spring, phys_mul(rx_world_float_abs(F32(B, 0x2c)), F32(cam, 0x43c)));
        rx_set_sp(B);

        /* 00657c17: d about the side axis by the pitch angle */
        eye_x = F32(B, 0x10);
        if (!(F32(cam, 0x3f8) == 0.0f)) {
            float sine, cosine;
            fast_sincos(phys_mul(F32(cam, 0x3f8), rx_bf(0x3c8efa34u) /* 0x977678 */), &sine, &cosine);
            SF32(B, 0x4c, cosine);
            SF32(B, 0x0c, sine);
            cross_at(B, B + 0x30, B + 0x50, B + 0x10);                   /* n x d */
            cross_at(B, B + 0xb0, B + 0x30, B + 0x50);                   /* (n x d) x n */
            const float n_x = F32(B, 0x50), n_y = F32(B, 0x54), n_z = F32(B, 0x58);
            const float dot = phys_add(phys_add(phys_mul(F32(B, 0x10), n_x), phys_mul(n_y, F32(B, 0x14))), phys_mul(n_z, F32(B, 0x18)));
            const float s = F32(B, 0x0c), c = F32(B, 0x4c);
            const float kx = phys_mul(dot, n_x);
            const float t4 = phys_mul(F32(B, 0x30), s);
            const float t5 = phys_mul(F32(B, 0x34), s);
            const float t6 = phys_mul(F32(B, 0x38), s);
            SF32(B, 0x54, phys_mul(n_y, dot));
            SF32(B, 0x58, phys_mul(n_z, dot));
            const float x = phys_add(phys_add(phys_mul(F32(B, 0xb0), c), t4), kx);
            const float y = phys_add(phys_add(phys_mul(F32(B, 0xb4), c), t5), F32(B, 0x54));
            const float z = phys_add(phys_add(phys_mul(F32(B, 0xb8), c), t6), F32(B, 0x58));
            eye_x = x;
            SF32(B, 0x14, y);
            SF32(B, 0x10, x);
            SF32(B, 0x18, z);
        }
        /* 00657e55: about y by the yaw angle */
        if (!(F32(cam, 0x3f4) == 0.0f)) {
            float sine, cosine;
            const float a = phys_mul(F32(cam, 0x3f4), rx_bf(0x3c8efa34u));
            SF32(B, 0x30, 0.0f);
            SF32(B, 0x34, 1.0f);
            SF32(B, 0x38, 0.0f);
            fast_sincos(a, &sine, &cosine);
            SF32(B, 0x4c, cosine);
            SF32(B, 0x0c, sine);
            cross_at(B, B + 0x50, B + 0x30, B + 0x10);                   /* y x d */
            cross_at(B, B + 0xb0, B + 0x50, B + 0x30);                   /* (y x d) x y */
            const float dot = phys_add(phys_add(phys_mul(F32(B, 0x10), 0.0f), F32(B, 0x14)), phys_mul(F32(B, 0x18), 0.0f));
            const float dot0 = phys_mul(dot, 0.0f);
            SF32(B, 0x34, dot);
            SF32(B, 0x38, dot0);
            const float s = F32(B, 0x0c), c = F32(B, 0x4c);
            const float t4 = phys_mul(F32(B, 0x50), s);
            const float t5 = phys_mul(F32(B, 0x54), s);
            const float t6 = phys_mul(F32(B, 0x58), s);
            const float x = phys_add(phys_add(phys_mul(F32(B, 0xb0), c), t4), dot0);
            const float y = phys_add(phys_add(phys_mul(F32(B, 0xb4), c), t5), F32(B, 0x34));
            const float z = phys_add(phys_add(phys_mul(F32(B, 0xb8), c), t6), dot0);
            eye_x = x;
            SF32(B, 0x14, y);
            SF32(B, 0x18, z);
        }
        /* 00658056: the eye = the point + d */
        eye_x = phys_add(eye_x, F32(B, 0x90));
        const float eye_y = phys_add(F32(B, 0x14), F32(B, 0x94));
        const float eye_z = phys_add(F32(B, 0x18), F32(B, 0x98));
        SF32(B, 0x70, eye_x);
        SF32(B, 0x74, eye_y);
        SF32(B, 0x78, eye_z);
    }

    /* 0065808f: the zoom */
    {
        const float dy = phys_sub(F32(B, 0x94), F32(B, 0x74));
        const float dz = phys_sub(F32(B, 0x98), F32(B, 0x78));
        const float dx = phys_sub(F32(B, 0x90), eye_x);
        SF32(B, 0x2c, dx);
        SF32(B, 0x6c, dy);
        SF32(B, 0x4c, dz);
        SF32(B, 0xac, sqrt_arg(B, phys_add(phys_add(phys_mul(dx, dx), phys_mul(dy, dy)), phys_mul(dz, dz))));
        const float x = F32(B, 0x2c), y = F32(B, 0x6c), z = F32(B, 0x4c);
        const float len2 = phys_add(phys_add(phys_mul(x, x), phys_mul(y, y)), phys_mul(z, z));
        SF32(B, 0x30, x); SF32(B, 0x34, y); SF32(B, 0x38, z);
        SF32(B, 0x0c, len2);
        float ux = x, uy = y, uz = z;
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0x0c)));
            ux = phys_mul(F32(B, 0x30), inv);
            uy = phys_mul(F32(B, 0x34), inv);
            uz = phys_mul(F32(B, 0x38), inv);
        }
        const float k = zoom_factor(cam, F32(B, 0xac));
        const float ey = phys_add(phys_mul(uy, k), F32(B, 0x74));
        const float ez = phys_add(phys_mul(uz, k), F32(B, 0x78));
        const float ex = phys_add(F32(B, 0x70), phys_mul(ux, k));
        SF32(B, 0x70, ex); SF32(B, 0x74, ey); SF32(B, 0x78, ez);
    }

    /* 006581d0: the view's rows (rows 0 and 1 normalized in memory, row 2's x only in a register) and the shift by up */
    view_rows(B);
    SF32(B, 0xac, phys_add(phys_add(phys_mul(F32(B, 0x10), F32(B, 0x10)), phys_mul(F32(B, 0x14), F32(B, 0x14))),
                           phys_mul(F32(B, 0x18), F32(B, 0x18))));
    if (F32(B, 0xac) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0xac)));
        SF32(B, 0x10, phys_mul(F32(B, 0x10), inv));
        const float y = phys_mul(F32(B, 0x14), inv);
        const float z = phys_mul(F32(B, 0x18), inv);
        SF32(B, 0x14, y);
        SF32(B, 0x18, z);
    }
    SF32(B, 0xac, phys_add(phys_add(phys_mul(F32(B, 0x50), F32(B, 0x50)), phys_mul(F32(B, 0x54), F32(B, 0x54))),
                           phys_mul(F32(B, 0x58), F32(B, 0x58))));
    if (F32(B, 0xac) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0xac)));
        SF32(B, 0x50, phys_mul(inv, F32(B, 0x50)));
        const float y = phys_mul(F32(B, 0x54), inv);
        const float z = phys_mul(F32(B, 0x58), inv);
        SF32(B, 0x54, y);
        SF32(B, 0x58, z);
    }
    float r2x = F32(B, 0x30);
    SF32(B, 0xac, phys_add(phys_add(phys_mul(r2x, r2x), phys_mul(F32(B, 0x34), F32(B, 0x34))), phys_mul(F32(B, 0x38), F32(B, 0x38))));
    if (F32(B, 0xac) > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_x87_arg(B, F32(B, 0xac)));
        r2x = phys_mul(inv, F32(B, 0x30));
        SF32(B, 0x34, phys_mul(F32(B, 0x34), inv));
        SF32(B, 0x38, phys_mul(F32(B, 0x38), inv));
    }
    {
        const float upx = F32(B, 0x80), upy = F32(B, 0x84), upz = F32(B, 0x88);
        SF32(B, 0x14, phys_mul(F32(B, 0x14), upx));
        const float a_x = phys_mul(upy, F32(B, 0x50));
        const float oy = phys_add(phys_mul(F32(B, 0x54), upy), F32(B, 0x14));
        const float z1 = phys_mul(F32(B, 0x58), upy);
        const float z0 = phys_mul(F32(B, 0x18), upx);
        const float ox = phys_add(phys_mul(upx, F32(B, 0x10)), a_x);
        SF32(B, 0xac, ox);
        SF32(B, 0x18, z0);
        const float oz = phys_add(z1, F32(B, 0x18));
        SF32(B, 0x0c, oz);
        const float x2 = phys_mul(upz, r2x);
        const float z2 = phys_mul(upz, F32(B, 0x38));
        const float y2 = phys_mul(upz, F32(B, 0x34));
        const float eye_z = phys_add(phys_add(oz, z2), F32(B, 0x78));
        const float eye_x2 = phys_add(F32(B, 0x70), phys_add(ox, x2));
        SF32(B, 0x78, eye_z);
        const float ox2 = phys_add(F32(B, 0xac), x2);
        const float eye_y = phys_add(phys_add(oy, y2), F32(B, 0x74));
        const float at_y = phys_add(phys_add(oy, y2), F32(B, 0x94));
        const float at_z = phys_add(phys_add(F32(B, 0x0c), z2), F32(B, 0x98));
        const float at_x = phys_add(F32(B, 0x90), ox2);
        SF32(B, 0x70, eye_x2);
        SF32(B, 0x74, eye_y);
        SF32(B, 0x90, at_x);
        SF32(B, 0x94, at_y);
        SF32(B, 0x98, at_z);
    }
}

void rx_parts_view_camera_update(rx_addr cam, float dt) {
    const rx_addr entry = rx_sp();
    const rx_addr B = aligned_frame(entry, 0x134) - 12;                  /* push ebx / esi / edi */
    rx_set_sp(B);
    if (U32(U32(U32(0, 0x00a95844), 0x10), 0xc) != 0) {                 /* the engine paused */
        rx_set_sp(entry);
        return;
    }
    const int32_t pad = I32(cam, 0x450);
    if (pad < 0) SF32(cam, 0x48c, 0.0f);
    else SF32(cam, 0x48c, key_value(B, 0x00d690c8, (uint32_t)pad));     /* the zoom input */
    if (U32(cam, 0x3e0) == 0) orbit_update(cam, dt, B);
    else spring_update(cam, dt, B);
    /* 00658536: 0063df90 (ECX = the eye, EAX = the look-at point, stack the camera) */
    rx_set_sp(B - 8);
    SU32(B - 4, 0, cam);
    rx_cam_entity_look_at(B + 0x70, B + 0x90, cam);
    rx_set_sp(entry);
}
RX_FUNCTION(0x00656070, rx_parts_view_camera_update)
