/* fx_animal.c -- LandAnimalProximity::vf20 (006d09f0): a land animal's per-frame placement. The steering (006d2eb0, fx_world.c)
 * leaves a velocity (+0x7c0 .. +0x7c8, its speed +0x830 against the top speed +0x834); the heading history (+0x7f0 / +0x800 /
 * +0x810 / +0x820, four directions in the XZ plane) turns towards the velocity's opposite by at most pi/8 a frame, its average
 * yaws the animal's forward vector by speed / top speed * 0.8 [0x9cc8a8] of the angle to it, the terrain (00402dc0 /
 * 007c5c90: height and normal; 007c5950 for a level without normals, +0x854 0) carries it, and the transform (vf16) is rebuilt
 * from the right / up / forward vectors scaled by +0x7e0 .. +0x7e8. The nearest of its three sound points (+0x780 / +0x790
 * / +0x7a0) goes to +0x7b0, the walk state (+0x770: 0 moving, 2 standing below 0.08), the audio emitter (+0x86c) follows
 * the position (Audio_PostCommand 10). Angles come from the inlined arcsine (in double with float steps) and the sine /
 * cosine polynomials (0x935d3c ..). notes/fx.md. */
#include "fx.h"
#include "fx_util.h"
#include "world_veh.h"
#include "world_node.h"
#include "phys_terrain.h"

#define A_(a, b) rx_adds((a), (b))
#define S_(a, b) rx_subs((a), (b))
#define M_(a, b) rx_muls((a), (b))
#define D_(a, b) rx_divs((a), (b))

/* the polynomial constants (0x935d3c .. 0x935d68) */
static inline float K(uint32_t a) { return F32(0, a); }

/* Float_Abs_Thunk 00401040 (xmm0) at the current ESP: its return address, the push ecx (ECX is 0 here: vf15's rep movsd), the
 * call of Float_Abs and its push ebp */
static float fabs_thunk(float x, rx_addr ret) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, ret); SU32(sp, -8, 0); SU32(sp, -0xc, 0x00401046); SU32(sp, -0x10, fx_reg(FX_EBP));
    return rx_bf(rx_fb(x) & 0x7fffffffu);
}
/* SqrtF 0042c230 (cdecl) from ESP = E: the caller's push ecx + store of the argument (arg_bits: through the x87 for fld /
 * fstp), the return address, SqrtF's push ebp */
static float sqrt_call(float x, uint32_t arg_bits, rx_addr ret) {
    (void)x;
    const rx_addr sp = rx_sp();
    SU32(sp, -4, arg_bits); SU32(sp, -8, ret); SU32(sp, -0xc, fx_reg(FX_EBP));
    return phys_sqrtf(rx_bf(arg_bits));
}

/* the inlined arcsine of x (clamped to [-1, 1] by the caller): |x| (Float_Abs_Thunk) into E + 0xc, above 0.7071 [0x9ddfc8]
 * via sqrt(|1 - x^2|); a rational function of y^2 in double with float steps; *high = |x| >= the threshold */
static float asin_part(rx_addr E, float x, rx_addr r_abs, rx_addr r_abs2, rx_addr r_sqrt) {
    SF32(E, 8, x);
    const float ax = fabs_thunk(x, r_abs);
    SF32(E, 0xc, ax);
    const int high = ax >= F32(0, 0x009ddfc8);
    float y = ax;
    if (high) {
        const float t = (float)((double)x * (double)x);
        const float u = fabs_thunk((float)(1.0 - (double)t), r_abs2);
        y = sqrt_call(u, rx_fb(u), r_sqrt);
    }
    const float y2 = (float)((double)y * (double)y);
    const float y4 = (float)((double)y2 * (double)y2);
    const double p = (double)(float)(0.5648093223571777 - (double)y2 * 0.04992850869894028) * (double)y4;    /* 0x9777b0 / a8 */
    const double q = (double)(float)(1.0 - (double)y2 * 1.4430354833602905);                                   /* 0x9777a0 */
    const float r = (float)(1.0 / (p + q));
    const double n1 = (double)(float)(0.3993034064769745 - (double)y2 * 0.0193784236907959) * (double)y4;     /* 0x977798 / 90 */
    const double n0 = (double)(float)(1.0 - (double)y2 * 1.2763687372207642);                                  /* 0x977788 */
    const float num = (float)((n1 + n0) * (double)y);
    float a = (float)((double)r * (double)num);
    if (high) a = (float)(1.5707963705062866 - (double)a);                                                     /* 0x977510 */
    return a;
}
/* the signed angle from the clamped cosine x: the arcsine a; a positive cross (pos) gives pi/2 - (x >= 0 ? a : -a), else
 * (x >= 0 ? a : -a) - pi/2 */
static float angle_from(float x, float a, int pos) {
    const float s = x >= 0.0f ? a : S_(-0.0f, a);
    return pos ? (float)(1.5707963705062866 - (double)s) : (float)((double)s - 1.5707963705062866);
}
/* clamp to [-1 ([0x9cd9e8]), 1] as compiled (comiss -1, x; comiss x, 1): NaN stays */
static float clamp_pm1(float x) {
    const float lo = F32(0, 0x009cd9e8);
    if (lo > x) return lo;
    if (x > 1.0f) return 1.0f;
    return x;
}
/* the sine / cosine polynomials of u = pi - wrapped angle: S ~ sin(u) (times u), C ~ -cos(u) */
static void sincos_poly(float u, float* S, float* C) {
    const float u2 = M_(u, u), u4 = M_(u2, u2);
    float s = A_(M_(u2, K(0x00935d68)), K(0x00935d64));
    s = A_(M_(s, u4), A_(M_(u2, K(0x00935d60)), K(0x00935d5c)));
    s = A_(M_(s, u4), A_(M_(u2, K(0x00935d58)), K(0x00935d54)));
    *S = M_(s, u);
    float c = A_(M_(u2, K(0x00935d50)), K(0x00935d48));
    c = A_(M_(c, u4), A_(M_(u2, K(0x00935d44)), K(0x00935d4c)));
    *C = A_(M_(c, u4), A_(M_(u2, K(0x00935d40)), K(0x00935d3c)));
}
/* a virtual of the animal with one pointer argument (vf15 / vf16 / vf17) at the current ESP */
static void vcall1(rx_addr self, uint32_t off, rx_addr arg, rx_addr ret) {
    const rx_addr fn = U32(U32(self, 0), off);
    void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
    const uint32_t a[1] = {arg};
    if (f) { const rx_addr s = fx_push(1, a, ret); f(self, arg); fx_pop(s); }
    else rx_call_original(fn, RX_THISCALL, self, 1, a);
}
/* the squared distance of a sound point to the position (dx^2 + dy^2 summed as compiled) */
static float dist2(rx_addr P, uint32_t o, rx_addr E, int order) {
    const float dx = S_(F32(P, o), F32(E, 0x30)), dy = S_(F32(P, o + 4), F32(E, 0x34)), dz = S_(F32(P, o + 8), F32(E, 0x38));
    return order ? A_(A_(M_(dy, dy), M_(dx, dx)), M_(dz, dz)) : A_(A_(M_(dy, dy), M_(dx, dx)), M_(dz, dz));
}

/* ---- 006d09f0 LandAnimalProximity::vf20 (thiscall). Frame: push ebp; and esp, -16; sub esp, 0x9c; push esi: E = A - 0xa0:
 * +8 / +0xc temporaries, +0x10 the forward vector (later the up / terrain normal), +0x20 the heading (later the new forward),
 * +0x30 the position, +0x40 the right vector / position out, +0x5c the activity (+0x878), +0x60 the transform. */
void rx_fx_land_animal_vf20(rx_addr P) {
    fx_frame f;
    fx_frame_enter(&f, 0x9c, 16, FX_ESI);
    const rx_addr E = f.E;
    if (U32(0, 0x00a94324) != 0) { fx_frame_leave(&f); return; }
    fx_set_reg(FX_ESI, P);
    const rx_addr eng = U32(0, FX_G_ENGINE), game = U32(0, FX_G_GAME);
    const float act = F32(P, 0x878);
    SF32(E, 0x5c, act);
    const rx_addr ctl = U32(P, 0x10);
    const rx_addr side = ctl == U32(eng, 0x20) ? U32(game, 0x5a54) : ctl == U32(eng, 0x1c) ? U32(game, 0x20c8) : 0;
    if (U32(side, 0x20014) != 0 || U32(U32(eng, 0x10), 0xc) != 0 || U32(P, 0x77c) != 0 || act == 0.0f) { fx_frame_leave(&f); return; }
    { const rx_addr s = fx_push(0, 0, 0x006d0a87); rx_fx_land_animal_steer(P); fx_pop(s); }
    if (!(F32(P, 0x830) > 0.0015f /* 0x977e9c */)) {
        SU32(P, 0x770, 2);
    } else {
        vcall1(P, 0x3c, E + 0x60, 0x006d0aaa);                                    /* vf15: the transform */
        SF32(E, 0x30, A_(F32(P, 0x7c0), F32(E, 0x90)));
        SF32(E, 0x34, A_(F32(P, 0x7c4), F32(E, 0x94)));
        SF32(E, 0x38, A_(F32(P, 0x7c8), F32(E, 0x98)));
        {   /* the velocity's direction, reversed, into +0x7d0 */
            const float vx = F32(P, 0x7c0), vy = F32(P, 0x7c4), vz = F32(P, 0x7c8);
            SF32(E, 0xc, vx);
            const float l2 = A_(A_(M_(vx, vx), M_(vy, vy)), M_(vz, vz));
            SF32(E, 8, l2);
            if (l2 > 0.0f) {
                const float r = sqrt_call(l2, fx_x87_bits(rx_fb(l2)), 0x006d0b4b);
                const float inv = D_(1.0f, r);
                SF32(P, 0x7d0, M_(F32(E, 0xc), inv));
                SF32(P, 0x7d4, M_(F32(P, 0x7c4), inv));
                SF32(P, 0x7d8, M_(F32(P, 0x7c8), inv));
            }
            SF32(P, 0x7d0, S_(-0.0f, F32(P, 0x7d0)));
            SF32(P, 0x7d4, S_(-0.0f, F32(P, 0x7d4)));
            SF32(P, 0x7d8, S_(-0.0f, F32(P, 0x7d8)));
        }
        {   /* the forward vector (the transform's third row), normalised, into E + 0x10 */
            const float fx = F32(E, 0x80), fy = F32(E, 0x84), fz = F32(E, 0x88);
            SF32(E, 0x14, fy); SF32(E, 0x10, fx); SF32(E, 0x18, fz);
            const float l2 = A_(A_(M_(fy, fy), M_(fx, fx)), M_(fz, fz));
            SF32(E, 0xc, l2);
            if (l2 > 0.0f) {
                const float r = sqrt_call(l2, fx_x87_bits(rx_fb(l2)), 0x006d0c3b);
                const float inv = D_(1.0f, r);
                SF32(E, 0x10, M_(inv, F32(E, 0x80)));
                SF32(E, 0x14, M_(inv, F32(E, 0x84)));
                SF32(E, 0x18, M_(inv, F32(E, 0x88)));
            }
        }
        SU32(P, 0x770, 0.08f /* 0x977d90 */ > F32(P, 0x830) ? 2 : 0);
        /* the heading history moves down a slot */
        for (uint32_t i = 0; i < 3; i++) fx_x87_copy(P + 0x7f0 + 4 * i, P + 0x800 + 4 * i);
        for (uint32_t i = 0; i < 3; i++) fx_x87_copy(P + 0x800 + 4 * i, P + 0x810 + 4 * i);
        for (uint32_t i = 0; i < 3; i++) fx_x87_copy(P + 0x810 + 4 * i, P + 0x820 + 4 * i);
        {   /* the newest heading turns towards +0x7d0 by at most pi/8 */
            const float dx = F32(P, 0x7d0), hx = F32(P, 0x810), hz = F32(P, 0x818), dz = F32(P, 0x7d8);
            const float cross = S_(M_(dx, hz), M_(hx, dz));
            const float dot = A_(M_(dx, hx), M_(dz, hz));
            const int pos = cross > 0.0f;
            const float x = clamp_pm1(dot);
            const float a = asin_part(E, x, pos ? 0x006d0d86 : 0x006d0f40, pos ? 0x006d0dc7 : 0x006d0f81, pos ? 0x006d0dd2 : 0x006d0f8c);
            const float ang = angle_from(x, a, pos);
            SF32(E, 8, ang);
            {   const rx_addr sp = rx_sp(); SU32(sp, -4, 0x006d10cd); SU32(sp, -8, fx_reg(FX_EBP)); }   /* Float_Abs */
            const float aa = rx_bf(rx_fb(ang) & 0x7fffffffu);
            if (aa > 0.39269909262657166f /* 0x977bbc */) {
                const float u = S_(3.1415927410125732f, S_(0.39269909262657166f, M_(0.0f, 6.2831854820251465f)));
                float s, c;
                sincos_poly(u, &s, &c);
                const float cc = S_(-0.0f, c);
                const float h0 = F32(P, 0x810), h2 = F32(P, 0x818);
                if (0.0f > ang) {
                    const float ss = M_(s, F32(0, 0x009cd9e8));
                    SF32(P, 0x820, A_(M_(h0, cc), M_(h2, ss)));
                    fx_x87_copy(P + 0x824, P + 0x814);
                    SF32(P, 0x828, S_(M_(h2, cc), M_(h0, ss)));
                } else {
                    SF32(P, 0x820, A_(M_(cc, h0), M_(s, h2)));
                    fx_x87_copy(P + 0x824, P + 0x814);
                    SF32(P, 0x828, S_(M_(cc, h2), M_(s, h0)));
                }
            } else {
                for (uint32_t i = 0; i < 3; i++) fx_x87_copy(P + 0x820 + 4 * i, P + 0x7d0 + 4 * i);
            }
        }
        float X, Z;
        {   /* the heading average (+0x7f0 .. +0x820), its XZ direction normalised by the full length */
            X = M_(A_(F32(P, 0x820), A_(F32(P, 0x810), A_(F32(P, 0x800), A_(F32(P, 0x7f0), 0.0f)))), 0.25f);
            const float Y = M_(A_(F32(P, 0x824), A_(F32(P, 0x814), A_(F32(P, 0x804), A_(F32(P, 0x7f4), 0.0f)))), 0.25f);
            Z = M_(A_(F32(P, 0x828), A_(F32(P, 0x818), A_(F32(P, 0x808), A_(F32(P, 0x7f8), 0.0f)))), 0.25f);
            const float l2 = A_(A_(M_(Y, Y), M_(X, X)), M_(Z, Z));
            SF32(E, 0x20, X); SF32(E, 0x28, Z); SF32(E, 0xc, l2);
            if (l2 > 0.0f) {
                const float r = sqrt_call(l2, fx_x87_bits(rx_fb(l2)), 0x006d1384);
                const float inv = D_(1.0f, r);
                X = M_(inv, F32(E, 0x20)); Z = M_(inv, F32(E, 0x28));
            }
        }
        {   /* the yaw from the forward vector towards the average, scaled by the speed */
            const float fz = F32(E, 0x18), fx = F32(E, 0x10);
            const float cross = S_(M_(X, fz), M_(Z, fx));
            const float dot = A_(M_(Z, fz), M_(X, fx));
            const int pos = cross > 0.0f;
            const float x = clamp_pm1(dot);
            const float a = asin_part(E, x, pos ? 0x006d1405 : 0x006d15ba, pos ? 0x006d1442 : 0x006d15f7, pos ? 0x006d144d : 0x006d1602);
            const float ang = angle_from(x, a, pos);
            const float v = M_(M_(D_(F32(P, 0x830), F32(P, 0x834)), F32(0, 0x009cc8a8)), ang);
            const float sg = v >= 0.0f ? 1.0f : F32(0, 0x009cd9e8);
            float w = M_(sg, v);
            w = S_(w, M_((float)rx_cvtt((double)M_(w, 0.15915493667125702f)), 6.2831854820251465f));
            const float u = S_(3.1415927410125732f, w);
            float s, c;
            sincos_poly(u, &s, &c);
            s = M_(s, sg);
            const float cc = S_(-0.0f, c);
            const float fy = F32(E, 0x14);
            const float nx = A_(M_(s, fz), M_(cc, fx));
            SF32(E, 0x20, nx);
            const float nz = S_(M_(cc, fz), M_(s, fx));
            const float l2 = A_(A_(M_(nx, nx), M_(fy, fy)), M_(nz, nz));
            SF32(E, 0x24, fy); SF32(E, 0x28, nz); SF32(E, 0xc, l2);
            if (l2 > 0.0f) {
                const float r = sqrt_call(l2, fx_x87_bits(rx_fb(l2)), 0x006d18c4);
                const float inv = D_(1.0f, r);
                SF32(E, 0x20, M_(inv, F32(E, 0x20)));
                SF32(E, 0x24, M_(inv, F32(E, 0x14)));
                SF32(E, 0x28, M_(inv, F32(E, 0x28)));
            }
        }
        /* the up vector, the terrain under the position */
        SF32(E, 0x10, 0.0f); SF32(E, 0x14, 1.0f); SF32(E, 0x18, 0.0f);
        if ((U32(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
        rx_addr T;
        { const rx_addr s = fx_push(0, 0, 0x006d1941); T = rx_world_find_by_controller(0x009e30d4); fx_pop(s); }
        if (U32(P, 0x854) != 0) {
            if (T != 0) {
                const uint32_t a[2] = {T, E + 0x30};
                const rx_addr s = fx_push(2, a, 0x006d1961); phys_terrain_base_height_normal(E + 0x10, T, E + 0x30); fx_pop(s);
            }
            /* the forward vector re-orthogonalised against the normal: D' = D x N with D = (-Dx, Dy, Dz) as compiled */
            const float dy = F32(E, 0x24), mdx = S_(-0.0f, F32(E, 0x20)), nz = F32(E, 0x18), ny = F32(E, 0x14), nx = F32(E, 0x10);
            const float dz = F32(E, 0x28);
            const float x1 = S_(M_(dy, nz), M_(mdx, ny));
            const float y1 = S_(M_(mdx, nx), M_(dz, nz));
            const float z1 = S_(M_(dz, ny), M_(dy, nx));
            SF32(E, 0x20, x1); SF32(E, 0x24, y1); SF32(E, 0x28, z1);
        } else if (T != 0) {
            const uint32_t a[1] = {E + 0x30}; uint32_t edx = 0;
            const rx_addr s = fx_push(1, a, 0x006d19de); phys_terrain_base_height_at(T, E + 0x30, &edx); fx_pop(s);
        }
        /* the right vector = N x D; the transform rows right * s0, up * s1, forward * s2, the position */
        { const uint32_t a[1] = {E + 0x40}; const rx_addr s = fx_push(1, a, 0x006d19f0); rx_world_vec3_cross(E + 0x40, E + 0x10, E + 0x20); fx_pop(s); }
        SF32(E, 0x6c, 0.0f); SF32(E, 0x7c, 0.0f); SF32(E, 0x8c, 0.0f);
        SU32(E, 0x90, U32(E, 0x30)); SU32(E, 0x94, U32(E, 0x34)); SU32(E, 0x98, U32(E, 0x38));
        SF32(E, 0x9c, 1.0f);
        {   const float s0 = F32(P, 0x7e0);
            SF32(E, 0x60, M_(F32(E, 0x40), s0)); SF32(E, 0x64, M_(F32(E, 0x44), s0)); SF32(E, 0x68, M_(F32(E, 0x48), s0));
            const float s1 = F32(P, 0x7e4);
            SF32(E, 0x70, M_(s1, F32(E, 0x10))); SF32(E, 0x74, M_(s1, F32(E, 0x14))); SF32(E, 0x78, M_(s1, F32(E, 0x18)));
            const float s2 = F32(P, 0x7e8);
            SF32(E, 0x80, M_(s2, F32(E, 0x20)));
            const float b = M_(F32(E, 0x24), s2), c = M_(s2, F32(E, 0x28));
            SF32(E, 0x84, b); SF32(E, 0x88, c); }
        vcall1(P, 0x40, E + 0x60, 0x006d1b00);                                    /* vf16: the transform */
        {   /* the nearest sound point */
            float m = 9999999.0f;   /* 0x977d64 */
            uint32_t k = 0;
            const float d0 = dist2(P, 0x780, E, 0);
            if (m > d0) m = d0;
            const float d1 = dist2(P, 0x790, E, 0);
            if (m > d1) { m = d1; k = 1; }
            const float px = F32(E, 0x30), py = F32(E, 0x34), pz = F32(E, 0x38);
            const float ey = S_(F32(P, 0x7a4), py), ez = S_(F32(P, 0x7a8), pz), ex = S_(F32(P, 0x7a0), px);
            const float d2 = A_(A_(M_(ey, ey), M_(ex, ex)), M_(ez, ez));
            if (m > d2) k = 2;
            SU32(P, 0x7b0, k);
        }
    }
    if (U32(P, 0x86c) != 0) {
        vcall1(P, 0x44, E + 0x40, 0x006d1c21);                                    /* vf17: the position */
        const rx_addr em = U32(P, 0x86c) + 0x1b0;
        SU32(em, 0, U32(E, 0x40)); SU32(em, 4, U32(E, 0x44)); SU32(em, 8, U32(E, 0x48));
        if (U32(0, 0x009b7430) == 0) {
            const uint32_t a[5] = {0xa, 0x00678b70, U32(P, 0x86c), E + 0x5c, 0};
            const rx_addr s = fx_push(5, a, 0x006d1c6f);
            rx_world_audio_post_command(0xa, 0x00678b70, a[2], E + 0x5c, 0);
            fx_pop(s);
        }
    }
    fx_frame_leave(&f);
}
RX_FUNCTION(0x006d09f0, rx_fx_land_animal_vf20)
