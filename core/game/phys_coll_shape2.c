/* phys_coll_shape2.c -- static box / composite shapes, the composite's scale / restore / first-child bounds and the box and
 * cylinder vertex tables (native/game/notes/collision.md). Shape layouts: see phys_coll_shape.c; a static box (+0x30 kind 1 sphere /
 * 2 capsule-like single axis / 3 box with two axis vectors) keeps its vectors at +0x40, +0x50 and the margin at +0x60; a static
 * composite has a name pool (+0x8c begin, +0x90 end) and a vector of offsets into it (+0xa4 begin, +0xa8 end). */
#include "phys_coll_shape2.h"
#include "phys_coll_shape.h"
#include "phys_coll_quat.h"
#include "phys_crt.h"

/* 00846380 CollisionShapeStaticBox::vf3 (thiscall: q*, pos*, out min*, out max*; RET 0x10): the world box by the kind at +0x30:
 *   1  pos -+ margin (+0x60)
 *   2  vector a = (+0x40, w cleared) rotated, r = q * (a * (q * conj)): bound = |r| + margin
 *   3  two vectors (+0x40, +0x50) rotated the same way: bound = max(|r1| + margin, |r2| + margin)
 * min = pos - bound, max = pos + bound (pos read again for the max). Other kinds write nothing. */
void phys_static_box_bounds(rx_addr self, rx_addr q_p, rx_addr pos_p, rx_addr out_min, rx_addr out_max) {
    const uint32_t kind = U32(self, 0x30);
    rx_v4 bound;
    if (kind == 1) {
        bound = v4_ld(self + 0x60);
    } else if (kind == 2) {
        const rx_v4 q = v4_ld(q_p), a = v4_xyz(v4_ld(self + 0x40));
        const rx_v4 r = coll_quat_mul(q, coll_quat_mul(a, v4_mul(q, COLL_QCONJ)));
        bound = v4_add(coll_v4_abs_max(r), v4_ld(self + 0x60));
    } else if (kind == 3) {
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 a1 = v4_xyz(v4_ld(self + 0x40));
        const rx_v4 r1 = coll_quat_mul(q, coll_quat_mul(a1, v4_mul(q, COLL_QCONJ)));
        const rx_v4 a2 = v4_xyz(v4_ld(self + 0x50));
        const rx_v4 r2 = coll_quat_mul(q, coll_quat_mul(a2, v4_mul(q, COLL_QCONJ)));
        const rx_v4 margin = v4_ld(self + 0x60);
        bound = v4_max(v4_add(coll_v4_abs_max(r1), margin), v4_add(coll_v4_abs_max(r2), margin));
    } else {
        return;
    }
    v4_st(out_min, v4_sub(v4_ld(pos_p), bound));
    v4_st(out_max, v4_add(v4_ld(pos_p), bound));
}
RX_FUNCTION(0x00846380, phys_static_box_bounds)

/* 00848ef0 CollisionShapeStaticComposite::vf8 (thiscall, stack name; RET 4): the index of the child whose name (in the pool, at the
 * child's offset) equals the string, or -1. */
int32_t phys_static_composite_find(rx_addr self, rx_addr name) {
    const uint32_t count = (uint32_t)(I32(self, 0xa8) - I32(self, 0xa4)) >> 2;
    for (uint32_t i = 0; i < count; i++) {
        const int pool_empty = (I32(self, 0x90) - I32(self, 0x8c)) == 0;
        rx_addr s = pool_empty ? 0 : U32(self, 0x8c) + U32(U32(self, 0xa4), i * 4);
        rx_addr n = name;
        int equal = 1;
        for (;;) {                                           /* strcmp */
            const uint8_t c = U8(n, 0);
            if (c != U8(s, 0)) { equal = 0; break; }
            if (c == 0) break;
            n++; s++;
        }
        if (equal) return (int32_t)i;
    }
    return -1;
}
RX_FUNCTION(0x00848ef0, phys_static_composite_find)

/* 00847f60 CollisionShapeComposite::vf2 (thiscall: q*, pos*, out min*, out max*; RET 0x10): the union of the children's bounds (their
 * vf2, vtable slot 2), the first child's written straight into the outputs, the others merged with min / max. Child 0's world
 * position uses conj * q, the others q * conj. Frame (sub esp, 0x64; push ebx / esi / edi): +0x20 rotation, +0x30 position, +0x40 /
 * +0x50 the child's min / max. Returns EAX: the child count. */
uint32_t phys_composite_vf2(rx_addr self, rx_addr q_p, rx_addr pos_p, rx_addr out_min, rx_addr out_max) {
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x64 - 12;
    rx_set_sp(F);
    const rx_addr rot = F + 0x20, pos = F + 0x30, mn = F + 0x40, mx = F + 0x50;
    {
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 offset = v4_xyz(v4_ld(U32(self, 0x5c)));
        v4_st(pos, v4_add(coll_quat_mul(q, coll_quat_mul(offset, v4_mul(COLL_QCONJ, q))), v4_ld(pos_p)));
        v4_st(rot, coll_quat_mul(v4_ld(q_p), v4_ld(U32(self, 0x44))));
        const rx_addr child = U32(U32(self, 0x2c), 0);
        phys_shape_bounds(U32(U32(child, 0), 8), child, rot, pos, out_min, out_max);
    }
    for (uint32_t i = 1; i < (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2; i++) {
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 offset = v4_xyz(v4_ld(U32(self, 0x5c) + i * 16));
        v4_st(pos, v4_add(coll_quat_mul(q, coll_quat_mul(offset, v4_mul(q, COLL_QCONJ))), v4_ld(pos_p)));
        v4_st(rot, coll_quat_mul(v4_ld(q_p), v4_ld(U32(self, 0x44) + i * 16)));
        const rx_addr child = U32(U32(self, 0x2c), i * 4);
        phys_shape_bounds(U32(U32(child, 0), 8), child, rot, pos, mn, mx);
        v4_st(out_min, v4_min(v4_ld(out_min), v4_ld(mn)));
        v4_st(out_max, v4_max(v4_ld(out_max), v4_ld(mx)));
    }
    rx_set_sp(entry_sp);
    return (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2;
}
RX_FUNCTION(0x00847f60, phys_composite_vf2)

/* x87 fld m32 / fstp m32: a copy, except that a signaling NaN comes back quiet. */
static float copy_through_x87(float f) { return f != f ? rx_bf(rx_fb(f) | 0x400000u) : f; }

/* 00848af0 CollisionShapeComposite::vf4 (thiscall, stack scale; RET 4): scales the shape: for every child its vf4(scale) (vtable slot 4;
 * the scale goes through the x87 stack), then the child's offset (+0x5c array) is saved to the +0x74 array and multiplied by the
 * scale splat. Returns EAX: the address of the last child's offset (0 without children). */
rx_addr phys_composite_scale(rx_addr self, float scale) {
    const uint32_t count = (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2;
    if (count == 0) return count;
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x14 - 12;
    rx_set_sp(F);
    const rx_v4 splat = v4_splat(scale);
    rx_addr last = 0;
    for (uint32_t i = 0; i < (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2; i++) {
        const rx_addr child = U32(U32(self, 0x2c), i * 4);
        const rx_addr slot = U32(U32(child, 0), 0x10);
        void (*f)(rx_addr, float) = RX_FN(slot, void (*)(rx_addr, float));
        if (f) f(child, copy_through_x87(scale));
        else { const uint32_t args[1] = {rx_fb(copy_through_x87(scale))}; rx_call_original(slot, RX_THISCALL, child, 1, args); }
        const rx_addr offset = U32(self, 0x5c) + i * 16;
        v4_st(U32(self, 0x74) + i * 16, v4_ld(offset));
        last = U32(self, 0x5c) + i * 16;
        v4_st(last, v4_mul(v4_ld(last), splat));
    }
    rx_set_sp(entry_sp);
    return last;
}
RX_FUNCTION(0x00848af0, phys_composite_scale)

/* 00848bd0 CollisionShapeComposite::vf5 (thiscall): undoes vf4: every child's vf5 (slot 5), then the offsets are restored from the +0x74
 * array. Returns EAX: the child count. */
uint32_t phys_composite_restore(rx_addr self) {
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x14 - 12;
    rx_set_sp(F);
    for (uint32_t i = 0; i < (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2; i++) {
        const rx_addr child = U32(U32(self, 0x2c), i * 4);
        const rx_addr slot = U32(U32(child, 0), 0x14);
        void (*f)(rx_addr) = RX_FN(slot, void (*)(rx_addr));
        if (f) f(child); else rx_call_original(slot, RX_THISCALL, child, 0, 0);
        v4_st(U32(self, 0x5c) + i * 16, v4_ld(U32(self, 0x74) + i * 16));
    }
    rx_set_sp(entry_sp);
    return (uint32_t)(I32(self, 0x30) - I32(self, 0x2c)) >> 2;
}
RX_FUNCTION(0x00848bd0, phys_composite_restore)

/* ---- vertex tables: pairs of (position, vertex) vec4s, 0x20 bytes each, in globals */
#define BOX_TABLE 0xd6d910u       /* 12 entries: 8 corners, then 4 vertices of the face perpendicular to the longest axis */
#define CYL_TABLE 0xd6d5e0u       /* 24 entries: 3 rings of 8 points (bottom, middle, top) */
#define ZERO4 0xd61670u           /* writable global vec4 (zero) */

/* 00845f90 FUN_00845f90 (ECX box): fills the box table with the local corners (CollisionShapeBox_Corners of the box at +0x20 half
 * extents, as 8 table entries 0..7 with the zero vector as position) and four vertices of the face through the box's middle
 * perpendicular to its longest half extent (entries 8..11, position zero vector): with half extents (h0, h1, h2),
 *   h0 largest (h0 > h1, h0 > h2): (0,h1,h2,h2) (0,-h1,h2,h2) (0,-h1,-h2,-h2) (0,h1,-h2,-h2)
 *   else h1 > h2:                  (h0,0,h2,h2) (-h0,0,h2,h2) (-h0,0,-h2,-h2) (h0,0,-h2,-h2)
 *   else:                          (h0,h1,0,0) (h0,-h1,0,0) (-h0,-h1,0,0) (-h0,h1,0,0). */
void phys_box_vertex_table(rx_addr box) {
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x80;     /* the corners are built on the frame (push ebp; and esp, -16; sub esp, 0x80) */
    rx_set_sp(F);
    phys_box_corners(box, F);
    const rx_v4 zero4 = v4_ld(ZERO4);
    for (int k = 0; k < 8; k++) v4_st(BOX_TABLE + 0x10 + 0x20 * k, v4_ld(F + 0x10 * k));
    for (int k = 0; k < 8; k++) v4_st(BOX_TABLE + 0x20 * k, zero4);
    const float h0 = F32(box, 0x20), h1 = F32(box, 0x24), h2 = F32(box, 0x28);
    const float zero = 0.0f /* 0x92e12c */, neg0 = -0.0f /* 0x9774a8 */;
    for (int k = 8; k < 12; k++) v4_st(BOX_TABLE + 0x20 * k, zero4);
    rx_v4 v[4];
    if (h0 > h1 && h0 > h2) {
        const float n1 = phys_sub(neg0, h1), n2 = phys_sub(neg0, h2);
        v[0] = v4(zero, h1, h2, h2); v[1] = v4(zero, n1, h2, h2); v[2] = v4(zero, n1, n2, n2); v[3] = v4(zero, h1, n2, n2);
    } else if (h1 > h2) {
        const float n0 = phys_sub(neg0, h0), n2 = phys_sub(neg0, h2);
        v[0] = v4(h0, zero, h2, h2); v[1] = v4(n0, zero, h2, h2); v[2] = v4(n0, zero, n2, n2); v[3] = v4(h0, zero, n2, n2);
    } else {
        const float n0 = phys_sub(neg0, h0), n1 = phys_sub(neg0, h1);
        v[0] = v4(h0, h1, zero, zero); v[1] = v4(h0, n1, zero, zero); v[2] = v4(n0, n1, zero, zero); v[3] = v4(n0, h1, zero, zero);
    }
    for (int k = 0; k < 4; k++) v4_st(BOX_TABLE + 0x10 + 0x20 * (8 + k), v[k]);
    rx_set_sp(entry_sp);
}
RX_FUNCTION(0x00845f90, phys_box_vertex_table)

/* 00846230 FUN_00846230 (ESI q*, EDX pos*, ECX box): the box table (phys_box_vertex_table) transformed to the world: every entry's
 * position slot gets the position and its vertex q * v * conj(q) + position (the quaternion and position are read again per entry). */
void phys_box_world_vertices(rx_addr box, rx_addr q_p, rx_addr pos_p) {
    const rx_addr entry_sp = rx_sp();
    rx_set_sp(entry_sp - 4);                              /* the call to 00845f90 pushes its return address */
    phys_box_vertex_table(box);
    rx_set_sp(entry_sp);
    for (rx_addr e = BOX_TABLE + 0x10; e < 0xd6daa0u; e += 0x20) {
        v4_st(e - 0x10, v4_ld(pos_p));
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 v = v4_xyz(v4_ld(e));
        v4_st(e, v4_add(coll_quat_mul(q, coll_quat_mul(v, v4_mul(COLL_QCONJ, q))), v4_ld(pos_p)));
    }
}
RX_FUNCTION(0x00846230, phys_box_world_vertices)

/* 008472d0 FUN_008472d0 (stack cylinder; RET 4): fills the cylinder table: 3 rings (y = -h/2, 0, +h/2) of 8 points
 * (x, y, z, z) with (x, z) at angles 0, 45, ..., 315 degrees: radius r = cyl+0x20 and r * 0.70710677 (0x977588), h = cyl+0x24;
 * position slots are zero. */
void phys_cylinder_vertex_table(rx_addr cyl) {
    const float r = F32(cyl, 0x20), h = F32(cyl, 0x24);
    const float zero = 0.0f /* 0x92e12c */, neg0 = -0.0f /* 0x9774a8 */;
    const float y_top = phys_mul(h, 0.5f /* 0x9322cc */), y_bottom = phys_sub(neg0, y_top);
    const float rc = phys_mul(r, 0.707106769f /* 0x977588 */);
    const float nr = phys_sub(neg0, r), nrc = phys_sub(neg0, rc);
    const float xs[8] = {r, rc, zero, nrc, nr, nrc, zero, rc};
    const float zs[8] = {zero, rc, r, rc, zero, nrc, nr, nrc};
    const float ys[3] = {y_bottom, zero, y_top};
    for (int ring = 0; ring < 3; ring++) {
        for (int a = 0; a < 8; a++) {
            const rx_addr e = CYL_TABLE + 0x20 * (uint32_t)(ring * 8 + a);
            v4_st(e, v4(zero, zero, zero, zero));
            v4_st(e + 0x10, v4(xs[a], ys[ring], zs[a], zs[a]));
        }
    }
}
RX_FUNCTION(0x008472d0, phys_cylinder_vertex_table)

/* 00847630 FUN_00847630 (ECX pos*, EDX q*, stack cylinder; RET 4): the cylinder table transformed to the world (as for the box). */
void phys_cylinder_world_vertices(rx_addr cyl, rx_addr pos_p, rx_addr q_p) {
    phys_cylinder_vertex_table(cyl);
    for (rx_addr e = CYL_TABLE + 0x10; e < 0xd6d8f0u; e += 0x20) {
        v4_st(e - 0x10, v4_ld(pos_p));
        const rx_v4 q = v4_ld(q_p);
        const rx_v4 v = v4_xyz(v4_ld(e));
        v4_st(e, v4_add(coll_quat_mul(q, coll_quat_mul(v, v4_mul(COLL_QCONJ, q))), v4_ld(pos_p)));
    }
}
RX_FUNCTION(0x00847630, phys_cylinder_world_vertices)

/* 00845b50 FUN_00845b50 (ESI point*, EDI box, stack normal out*; RET 4): moves the point to the box surface and gives the surface normal
 * (xyz of *normal; w untouched except in the inside case).
 * Inside the box (CollisionShapeBox_ContainsPoint): the point goes to the nearest face -- the smallest of the six squared distances
 * (x -+ hx)^2, (y -+ hy)^2, (z -+ hz)^2, ties to the first -- and the normal is that face's axis (zero vector with one +-1.0; the
 * -1 is the runtime global 0x9cd9e8).
 * Outside: the point is clamped into the box, the normal starts as the globals 0xa95d50.. (0) and gets +1 / -1 per axis where the
 * clamped point lies on a face from the original's tests, then it is normalised (nothing when its length^2 is not above zero). */
void phys_box_clamp_point(rx_addr box, rx_addr point, rx_addr normal) {
    const float minus_one = F32(0, 0x9cd9e8), neg0 = -0.0f /* 0x9774a8 */, one = 1.0f /* 0x931f24 */;
    /* the test runs at the original's ESP (push ebp; and esp, -16; sub esp, 0x1c; push ebx; push edi = its box argument; call): its
     * frame keeps the last lane mask (phys_coll_shape.c contains_frame) */
    const rx_addr sp0 = rx_sp(), arg = ((sp0 - 4) & ~15u) - 0x1c - 8;
    SU32(arg, 0, box); SU32(arg, -4, 0x00845b65u);
    rx_set_sp(arg - 4);
    const uint32_t inside = phys_box_contains_point(point, box);
    rx_set_sp(sp0);
    if (inside) {
        v4_st(normal, v4_ld(0xd61670));
        const float x = F32(point, 0), y = F32(point, 4), z = F32(point, 8);
        const float hx = F32(box, 0x20), hy = F32(box, 0x24), hz = F32(box, 0x28);
        const float a1 = phys_mul(phys_add(hx, x), phys_add(hx, x));
        const float y_minus = phys_sub(y, hy), y_plus = phys_add(hy, y);
        const float a2 = phys_mul(y_minus, y_minus), a3 = phys_mul(y_plus, y_plus);
        const float z_minus = phys_sub(z, hz), z_plus = phys_add(hz, z);
        const float a4 = phys_mul(z_minus, z_minus), a5 = phys_mul(z_plus, z_plus);
        float best = phys_mul(phys_sub(x, hx), phys_sub(x, hx));
        int face = 0;
        if (best > a1) { face = 1; best = a1; }
        if (best > a2) { face = 2; best = a2; }
        if (best > a3) { face = 3; best = a3; }
        if (best > a4) { face = 4; best = a4; }
        if (best > a5) face = 5;
        switch (face) {
        case 0: SF32(point, 0, hx); SF32(normal, 0, one); break;
        case 1: SF32(point, 0, phys_sub(neg0, hx)); SF32(normal, 0, minus_one); break;
        case 2: SF32(point, 4, copy_through_x87(F32(box, 0x24))); SF32(normal, 4, one); break;
        case 3: SF32(point, 4, phys_sub(neg0, F32(box, 0x24))); SF32(normal, 4, minus_one); break;
        case 4: SF32(point, 8, copy_through_x87(F32(box, 0x28))); SF32(normal, 8, one); break;
        default: SF32(point, 8, phys_sub(neg0, F32(box, 0x28))); SF32(normal, 8, minus_one); break;
        }
        return;
    }
    SF32(normal, 0, F32(0, 0xa95d50)); SF32(normal, 4, F32(0, 0xa95d54)); SF32(normal, 8, F32(0, 0xa95d58));
    /* clamp each coordinate to [-h, h]: first min(c, h), stored; then max against -h (the stored value if not below) */
    float c[3];
    for (int k = 0; k < 3; k++) {
        const uint32_t o = 4u * (uint32_t)k, ho = 0x20 + o;
        float v = F32(point, o);
        if (v > F32(box, ho)) v = F32(box, ho);
        SF32(point, o, v);
        const float h = F32(box, ho);
        float r = v;
        if (phys_sub(neg0, v) > h) r = phys_sub(neg0, h);
        SF32(point, o, r);
        c[k] = r;
    }
    const float zero = 0.0f;
    float nx, ny, nz;
    {   /* x: out.x + a, then + b (a, b = 1 above +hx, -1 below -hx) */
        const float a = c[0] > F32(box, 0x20) ? one : zero;
        nx = phys_add(F32(normal, 0), a);
        SF32(normal, 0, nx);
        const float b = phys_sub(neg0, F32(point, 0)) > F32(box, 0x20) ? minus_one : zero;
        nx = phys_add(nx, b);
        SF32(normal, 0, nx);
    }
    {   /* y: a + out.y, then + b */
        const float a = F32(point, 4) > F32(box, 0x24) ? one : zero;
        ny = phys_add(a, F32(normal, 4));
        SF32(normal, 4, ny);
        const float b = phys_sub(neg0, F32(point, 4)) > F32(box, 0x24) ? minus_one : zero;
        ny = phys_add(ny, b);
        SF32(normal, 4, ny);
    }
    {   /* z: out.z + a, then + b */
        const float a = F32(point, 8) > F32(box, 0x28) ? one : zero;
        nz = phys_add(F32(normal, 8), a);
        SF32(normal, 8, nz);
        const float b = phys_sub(neg0, F32(point, 8)) > F32(box, 0x28) ? minus_one : zero;
        nz = phys_add(nz, b);
        SF32(normal, 8, nz);
    }
    const float len2 = phys_add(phys_add(phys_mul(nz, nz), phys_mul(ny, ny)), phys_mul(nx, nx));
    if (len2 > zero) {
        const float inv = phys_div(one, phys_sqrtf(copy_through_x87(len2)));
        SF32(normal, 0, phys_mul(nx, inv));
        SF32(normal, 4, phys_mul(ny, inv));
        SF32(normal, 8, phys_mul(nz, inv));
    }
}
RX_FUNCTION(0x00845b50, phys_box_clamp_point)
