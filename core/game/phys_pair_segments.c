/* phys_pair_segments.c -- 0086e750: the contact generator of the dispatcher's shape type 0x20 (a segment list: count at +0x20,
 * 0x20-byte entries at +0x24, each two vec4 end points) against a cylinder (type 0x0008: radius +0x20, height +0x24), both
 * directions (the dispatcher's 0x0008 * 0x0020 and its swapped case 0x0020 * 0x0008, with the inverted swap flag).
 *
 * For every entry of the list: the segment's two ends into the cylinder's frame (the cylinder's rotation rows applied to the
 * list's frame product, then subtracted from the cylinder's position), then 0081beb0 (the segment against the cylinder: side and
 * caps) at the original's ESP. Per hit: the point back in world space (the local point's x / y / z lanes times the cylinder's
 * rows plus its position; w 1), the surface point's normal (00846f10 on the cylinder, which moves the local hit onto the surface,
 * then the same rows; w 0, negated with the flag) and the depth |segment end - surface point| (the sqrt of (x^2 + z^2) + y^2,
 * broadcast to the four lanes). Contact set M: count +0, records at M + 0x30 * c (point +0x10, normal +0x20, depth +0x30), the
 * touching flag +0x6a0.
 *
 * Convention (LTCG): ECX = the segment list, EDX = its position; stack: M, cylinder, cylinder's rotation*, cylinder's position*,
 * the list's rotation*, swap flag; RET 0x18. EAX / ESI / EDI hold the dispatcher's redundant copies (never read). EBP / EBX /
 * ESI of this frame (the cylinder's and the list's registers) are handed to 0081beb0, which saves them in its own frame.
 * Frame: push ebp; and esp, -16; sub esp, 0x1b4; push ebx / esi / edi; +0x30 / +0x80 the two rotation matrices (3 rows, w 0),
 * +0x60 / +0xa0 their positions (w 1 / unused), +0xcc the half height, +0x24 the height, +0x1c / +0x28 the hit flags, +0x18 /
 * +0x20 the loop's offset / remaining count, +0x70 / +0xd0 the segment's ends in the cylinder's frame, +0x100 the surface
 * normal, +0x180 / +0x190 the two hits, +0x1a0 the hit saved for the world point.
 * Returns ECX (the security cookie, as the original's epilogue leaves it); *edx = the EDX the loop leaves (the last record's
 * 0x30 * count, or the last entry's offset when neither end hit). */
#include "phys_pair_segments.h"
#include "phys_cylinder.h"
#include "world_node.h"
#include "world_util.h"

/* a direct call of an original stdcall kernel at the original's ESP: its arguments and the 0 return address on the stack (as
 * phys_pair3.c) */
#define KERNEL_CALL(n, args, stmt) do { const rx_addr sp_ = rx_sp(); \
    for (int i_ = 0; i_ < (n); i_++) SU32(sp_ - 4u * (n) + 4u * (uint32_t)i_, 0, (args)[i_]); \
    SU32(sp_ - 4u * (n) - 4u, 0, 0); rx_set_sp(sp_ - 4u * (n) - 4u); stmt; rx_set_sp(sp_); } while (0)
#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

uint32_t phys_pair_cylinder_segments(rx_addr seg, rx_addr pseg, rx_addr M, rx_addr cyl, rx_addr qcyl, rx_addr pcyl, rx_addr qseg,
                                     uint32_t flag, uint32_t* edx) {
    const rx_addr sp0 = rx_sp();
    const rx_addr L = ((sp0 - 4u) & ~15u) - 0x1b4u - 12u;
    rx_set_sp(L);

    /* the cylinder's rotation rows (w lanes 0) */
    const float ax = F32(qcyl, 0), ay = F32(qcyl, 4), az = F32(qcyl, 8), aw = F32(qcyl, 0xc);
    const float ax2 = phys_mul(ax, 2.0f /* 0x92e330 */), ay2 = phys_mul(ay, 2.0f), az2 = phys_mul(az, 2.0f);
    const float axx = phys_mul(ax2, ax), ayy = phys_mul(ay2, ay), azz = phys_mul(az2, az);
    const float awx = phys_mul(aw, ax2), awy = phys_mul(aw, ay2), awz = phys_mul(aw, az2);
    const float axy = phys_mul(ax2, ay), axz = phys_mul(ax2, az), ayz = phys_mul(ay2, az);
    const rx_v4 ar0 = v4(phys_sub(1.0f, phys_add(azz, ayy)), phys_add(axy, awz), phys_sub(axz, awy), 0.0f);
    const rx_v4 ar1 = v4(phys_sub(axy, awz), phys_sub(1.0f, phys_add(azz, axx)), phys_add(ayz, awx), 0.0f);
    const rx_v4 ar2 = v4(phys_add(axz, awy), phys_sub(ayz, awx), phys_sub(1.0f, phys_add(ayy, axx)), 0.0f);

    /* the segment list's rotation rows */
    const float bx = F32(qseg, 0), by = F32(qseg, 4), bz = F32(qseg, 8), bw = F32(qseg, 0xc);
    const float bx2 = phys_mul(bx, 2.0f), by2 = phys_mul(by, 2.0f), bz2 = phys_mul(bz, 2.0f);
    const float bxx = phys_mul(bx2, bx), byy = phys_mul(by2, by), bzz = phys_mul(bz2, bz);
    const float bwx = phys_mul(bx2, bw), bwy = phys_mul(by2, bw), bwz = phys_mul(bz2, bw);
    const float bxy = phys_mul(bx2, by), bxz = phys_mul(bx2, bz), byz = phys_mul(by2, bz);
    const rx_v4 br0 = v4(phys_sub(1.0f, phys_add(bzz, byy)), phys_add(bxy, bwz), phys_sub(bxz, bwy), 0.0f);
    const rx_v4 br1 = v4(phys_sub(bxy, bwz), phys_sub(1.0f, phys_add(bzz, bxx)), phys_add(byz, bwx), 0.0f);
    const rx_v4 br2 = v4(phys_add(bxz, bwy), phys_sub(byz, bwx), phys_sub(1.0f, phys_add(byy, bxx)), 0.0f);

    const float pax = F32(pcyl, 0), pay = F32(pcyl, 4), paz = F32(pcyl, 8);
    const rx_v4 pa4 = v4(pax, pay, paz, 1.0f);              /* the w lane the world point's add gives 1 */
    const float pbx = F32(pseg, 0), pby = F32(pseg, 4), pbz = F32(pseg, 8);
    const float h = phys_mul(F32(cyl, 0x24), 0.5f), r = F32(cyl, 0x20);
    const uint32_t h_bits = world_x87_pass(rx_fb(h)), r_bits = world_x87_pass(rx_fb(r));   /* fld / fstp copies of the arguments */
    const float h_arg = rx_bf(h_bits), r_arg = rx_bf(r_bits);

    SU32(L, 0x1c, 0); SU32(L, 0x28, 0);
    *edx = (uint32_t)seg;                                   /* EDX = the list until the loop sets it */
    const int32_t count = I32(seg, 0x20);
    if (count > 0) {
        uint32_t off = 0;
        int32_t remaining = count;
        do {
            *edx = off;                                     /* mov edx, [esp + 0x18] */
            const rx_addr s = U32(seg, 0x24) + off;
            const float p0x = F32(s, 0), p0y = F32(s, 4), p0z = F32(s, 8);
            const float p1x = F32(s, 0x10), p1y = F32(s, 0x14), p1z = F32(s, 0x18);
            /* the ends into the cylinder's frame */
            const float l0x = phys_sub(phys_add(phys_add(phys_add(phys_mul(p0z, br0.v[2]), phys_mul(p0y, br0.v[1])),
                                                          phys_mul(br0.v[0], p0x)), pbx), pax);
            const float l0y = phys_sub(phys_add(phys_add(phys_add(phys_mul(br1.v[0], p0x), phys_mul(br1.v[2], p0z)),
                                                          phys_mul(p0y, br1.v[1])), pby), pay);
            const float l0z = phys_sub(phys_add(phys_add(phys_add(phys_mul(br2.v[0], p0x), phys_mul(p0z, br2.v[2])),
                                                          phys_mul(p0y, br2.v[1])), pbz), paz);
            const float l1x = phys_sub(phys_add(phys_add(phys_add(phys_mul(p1z, br0.v[2]), phys_mul(p1y, br0.v[1])),
                                                          phys_mul(br0.v[0], p1x)), pbx), pax);
            const float l1y = phys_sub(phys_add(phys_add(phys_add(phys_mul(br1.v[0], p1x), phys_mul(p1z, br1.v[2])),
                                                          phys_mul(p1y, br1.v[1])), pby), pay);
            const float l1z = phys_sub(phys_add(phys_add(phys_add(phys_mul(br2.v[0], p1x), phys_mul(p1z, br2.v[2])),
                                                          phys_mul(p1y, br2.v[1])), pbz), paz);
            /* and through the cylinder's rows */
            const float q0x = phys_add(phys_add(phys_mul(l0x, ar0.v[0]), phys_mul(l0y, ar0.v[1])), phys_mul(l0z, ar0.v[2]));
            const float q0y = phys_add(phys_add(phys_mul(l0x, ar1.v[0]), phys_mul(l0y, ar1.v[1])), phys_mul(l0z, ar1.v[2]));
            const float q0z = phys_add(phys_add(phys_mul(l0x, ar2.v[0]), phys_mul(l0y, ar2.v[1])), phys_mul(l0z, ar2.v[2]));
            const float q1x = phys_add(phys_add(phys_mul(l1x, ar0.v[0]), phys_mul(l1y, ar0.v[1])), phys_mul(l1z, ar0.v[2]));
            const float q1y = phys_add(phys_add(phys_mul(l1x, ar1.v[0]), phys_mul(l1y, ar1.v[1])), phys_mul(l1z, ar1.v[2]));
            const float q1z = phys_add(phys_add(phys_mul(l1x, ar2.v[0]), phys_mul(l1y, ar2.v[1])), phys_mul(l1z, ar2.v[2]));
            SF32(L, 0xd0, q0x); SF32(L, 0xd4, q0y); SF32(L, 0xd8, q0z);
            SF32(L, 0x70, q1x); SF32(L, 0x74, q1y); SF32(L, 0x78, q1z);
            const uint32_t args[6] = {h_bits, r_bits, L + 0x180, L + 0x190, L + 0x1c, L + 0x28};
            KERNEL_CALL(6, args, rx_world_segment_cylinder(L + 0xd0, L + 0x70, h_arg, r_arg, L + 0x180, L + 0x190, L + 0x1c,
                                                           L + 0x28, sp0 - 4, M, cyl));              /* 0081beb0 */
            for (int hit = 0; hit < 2; hit++) {
                const rx_addr HF = hit ? 0x190 : 0x180;         /* the hit's slot */
                if (U32(L, hit ? 0x28 : 0x1c) == 0) continue;
                const rx_v4 saved = v4(F32(L, HF), F32(L, HF + 4), F32(L, HF + 8), F32(L, HF + 0xc));
                CALL_AT_SP(L, 1, (void)phys_cylinder_closest_point_normal(cyl, L + HF, L + 0x100));   /* 00846f10 */
                const rx_v4 nl = v4_ld(L + 0x100);
                rx_v4 wp = v4_add(v4_add(v4_mul(v4_splat(saved.v[1]), ar1), v4_mul(v4_splat(saved.v[0]), ar0)),
                                  v4_mul(v4_splat(saved.v[2]), ar2));
                rx_v4 wn = v4_add(v4_add(v4_mul(v4_splat(nl.v[0]), ar0), v4_mul(v4_splat(nl.v[1]), ar1)),
                                  v4_mul(v4_splat(nl.v[2]), ar2));
                wp = v4_add(wp, pa4);
                if ((flag & 0xffu) != 0) wn = v4_sub(v4_zero(), wn);
                int32_t c = I32(M, 0);
                if (c >= 0x1d) c = 0x1d;
                *edx = 0x30u * (uint32_t)c;
                const rx_addr rec = M + 0x30u * (uint32_t)c;
                v4_st(rec + 0x10, wp);
                v4_st(rec + 0x20, wn);
                const rx_v4 d = v4_sub(v4_ld(L + 0x70), v4_ld(L + HF));
                const rx_v4 p = v4_mul(d, d);
                v4_st(rec + 0x30, v4_splat(phys_sqrt(phys_add(phys_add(p.v[0], p.v[2]), p.v[1]))));
                SU32(M, 0, (uint32_t)c + 1);
                SU32(M, 0x6a0, 1);
            }
            off += 0x20;
        } while (--remaining != 0);
    }
    rx_set_sp(sp0);
    return U32(0, 0x009b45e4);                              /* ECX at the original's epilogue: the security cookie */
}
RX_FUNCTION(0x0086e750, phys_pair_cylinder_segments)
