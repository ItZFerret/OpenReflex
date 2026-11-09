/* phys_contact.c -- the narrow phase: 0085ddd0 Collision_ContactDispatch, contact generation between two collision shapes
 * dispatched on both shapes' type flags (shape +0x08: 2 sphere, 4 box, 8 cylinder, 0x10 triangles, 0x20 ?, 0x40 / 0x80 /
 * 0x100 circle-like shapes, 0x200 height-field terrain, 0x400 ray set, 0x8000 composite) after the collision-group masks
 * (shape +0x0c against the other's +0x08, +0x10 against the other's +0x14).
 * Contact set ("manifold", M): count +0, records of 0x30 bytes (point, normal, depth) at +0x10, feature ids at +0x5b0 / +0x628,
 * touching flag +0x6a0 (native/game/notes/world.md). */
#include "phys_contact.h"
#include "phys_pair.h"
#include "phys_pair2.h"
#include "phys_pair3.h"
#include "phys_pair_rodbox.h"
#include "phys_pair_segments.h"
#include "phys_pair_sphere_terrain.h"
#include "phys_pair_circle.h"
#include "phys_pair_rays.h"
#include "phys_pair_query.h"
#include "phys_coll_pair.h"
#include "loop_vehicle.h"
#include "collision_dispatch_observed.h"
#include "collision_live.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>

/* The pair generators have LTCG conventions: each case of the original passes a different choice of the dispatcher's
 * arguments in registers and on the stack (the swapped cases pass B first and the inverted swap flag). The table lists them
 * exactly as the original's code does (derived from its case blocks, tools/disp.py in the rx_phys scratchpad). */
enum pair_src {
    S_M, S_A, S_QA, S_PA, S_B, S_QB, S_PB, S_FLAG,
    S_NOTFLAG,       /* byte(flag) == 0 (sete) */
    S_TB,            /* B's type word (left in ECX by the switch) */
    S_TB_LOWNOT,     /* sete cl over the type word: (type & ~0xff) | not flag */
    S_TB_LOWFLAG,    /* mov cl, byte flag over the type word */
    S_PB_LOWFLAG,    /*85df9c/85dfa0: PB upper bits, CL=byte flag */
    S_A10,           /* A+0x10 (left in EDX by the mask checks) */
};
struct pair_case {
    uint32_t type_a, type_b, fn;
    uint8_t reg[5];                  /* EAX, ECX, EDX, ESI, EDI */
    uint8_t nargs, arg[7];           /* stack arguments, first = lowest address */
};
static const struct pair_case k_pairs[] = {
    {0x0002, 0x0002, 0x00861a60, {S_A, S_PA, S_A, S_B, S_M}, 4, {S_M, S_B, S_PB, S_FLAG}},
    {0x0002, 0x0004, 0x00861ba0, {S_A, S_A, S_PA, S_B, S_M}, 6, {S_M, S_QA, S_B, S_QB, S_PB, S_FLAG}},
    {0x0002, 0x0008, 0x00862da0, {S_A, S_PB, S_A, S_B, S_M}, 5, {S_M, S_PA, S_B, S_QB, S_FLAG}},
    {0x0002, 0x0010, 0x00863f70, {S_A, S_PB, S_A, S_B, S_B}, 4, {S_M, S_PA, S_QB, S_FLAG}},
    {0x0002, 0x0020, 0x00864460, {S_A, S_PB, S_A, S_M, S_M}, 4, {S_PA, S_B, S_QB, S_FLAG}},
    {0x0002, 0x0100, 0x00864dd0, {S_A, S_TB, S_FLAG, S_A, S_PA}, 3, {S_M, S_B, S_FLAG}},
    {0x0002, 0x0200, 0x00864ec0, {S_A, S_A, S_A10, S_B, S_PA}, 3, {S_M, S_B, S_FLAG}},
    {0x0002, 0x0400, 0x008650c0, {S_A, S_PB_LOWFLAG, S_QB, S_B, S_M}, 5, {S_M, S_A, S_PA, S_B, S_PB}},
    {0x0002, 0x8000, 0x008656c0, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0004, 0x0002, 0x00861ba0, {S_A, S_B, S_PB, S_B, S_M}, 6, {S_M, S_QB, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x0004, 0x0004, 0x008657d0, {S_A, S_B, S_QB, S_B, S_M}, 6, {S_M, S_A, S_QA, S_PA, S_PB, S_FLAG}},
    {0x0004, 0x0008, 0x00867ca0, {S_A, S_QB, S_B, S_B, S_M}, 6, {S_M, S_A, S_QA, S_PA, S_PB, S_FLAG}},
    {0x0004, 0x0010, 0x00869d40, {S_A, S_A, S_QB, S_B, S_M}, 6, {S_M, S_QA, S_PA, S_B, S_PB, S_FLAG}},
    {0x0004, 0x0020, 0x0086a1e0, {S_A, S_B, S_PB, S_B, S_M}, 5, {S_A, S_QA, S_PA, S_QB, S_FLAG}},
    {0x0004, 0x0100, 0x0086b360, {S_A, S_A, S_PA, S_B, S_M}, 4, {S_M, S_QA, S_B, S_FLAG}},
    {0x0004, 0x0200, 0x0086b4d0, {S_A, S_B, S_A, S_B, S_M}, 4, {S_M, S_QA, S_PA, S_FLAG}},
    {0x0004, 0x0400, 0x0086b7f0, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_A, S_QA, S_PA, S_B, S_FLAG}},
    {0x0004, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0008, 0x0002, 0x00862da0, {S_A, S_PA, S_B, S_B, S_M}, 5, {S_M, S_PB, S_A, S_QA, S_NOTFLAG}},
    {0x0008, 0x0004, 0x00867ca0, {S_A, S_QA, S_A, S_B, S_M}, 6, {S_M, S_B, S_QB, S_PB, S_PA, S_NOTFLAG}},
    {0x0008, 0x0008, 0x0086bd80, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_A, S_QA, S_PA, S_B, S_FLAG}},
    {0x0008, 0x0010, 0x0086e090, {S_A, S_PA, S_QB, S_B, S_M}, 5, {S_A, S_QA, S_B, S_PB, S_FLAG}},
    {0x0008, 0x0020, 0x0086e750, {S_A, S_B, S_PB, S_B, S_M}, 6, {S_M, S_A, S_QA, S_PA, S_QB, S_FLAG}},
    {0x0008, 0x0100, 0x0086f150, {S_A, S_PA, S_QA, S_B, S_M}, 3, {S_A, S_B, S_FLAG}},
    {0x0008, 0x0200, 0x0086f2a0, {S_A, S_PA, S_QA, S_B, S_M}, 3, {S_A, S_B, S_FLAG}},
    {0x0008, 0x0400, 0x0086f3d0, {S_A, S_PB, S_QA, S_B, S_M}, 6, {S_M, S_A, S_PA, S_B, S_QB, S_FLAG}},
    {0x0008, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0010, 0x0002, 0x00863f70, {S_A, S_PA, S_B, S_B, S_A}, 4, {S_M, S_PB, S_QA, S_NOTFLAG}},
    {0x0010, 0x0004, 0x00869d40, {S_QB, S_B, S_QA, S_B, S_M}, 6, {S_M, S_QB, S_PB, S_A, S_PA, S_NOTFLAG}},
    {0x0010, 0x0008, 0x0086e090, {S_A, S_PB, S_QA, S_B, S_M}, 5, {S_B, S_QB, S_A, S_PA, S_NOTFLAG}},
    {0x0010, 0x0020, 0x0086fac0, {S_A, S_PB, S_QA, S_M, S_M}, 5, {S_A, S_PA, S_B, S_QB, S_FLAG}},
    {0x0010, 0x0400, 0x008716b0, {S_A, S_QB, S_A, S_B, S_M}, 6, {S_M, S_QA, S_PA, S_B, S_PB, S_FLAG}},
    {0x0010, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0020, 0x0002, 0x00864460, {S_PB, S_PA, S_B, S_M, S_M}, 4, {S_PB, S_A, S_QA, S_NOTFLAG}},
    {0x0020, 0x0004, 0x0086a1e0, {S_A, S_A, S_PA, S_B, S_M}, 5, {S_B, S_QB, S_PB, S_QA, S_NOTFLAG}},
    {0x0020, 0x0008, 0x0086e750, {S_A, S_A, S_PA, S_B, S_M}, 6, {S_M, S_B, S_QB, S_PB, S_QA, S_NOTFLAG}},
    {0x0020, 0x0010, 0x0086fac0, {S_A, S_PA, S_QB, S_M, S_M}, 5, {S_B, S_PB, S_A, S_QA, S_NOTFLAG}},
    {0x0020, 0x0100, 0x00872970, {S_A, S_PA, S_B, S_A, S_M}, 2, {S_QA, S_FLAG}},
    {0x0020, 0x0200, 0x00872c90, {S_A, S_A, S_PA, S_B, S_M}, 4, {S_M, S_QA, S_B, S_FLAG}},
    {0x0020, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0040, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0080, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0100, 0x0002, 0x00864dd0, {S_A, S_TB_LOWNOT, S_NOTFLAG, S_B, S_PB}, 3, {S_M, S_A, S_NOTFLAG}},
    {0x0100, 0x0004, 0x0086b360, {S_QB, S_B, S_PB, S_B, S_M}, 4, {S_M, S_QB, S_A, S_NOTFLAG}},
    {0x0100, 0x0008, 0x0086f150, {S_A, S_PB, S_QB, S_B, S_M}, 3, {S_B, S_A, S_NOTFLAG}},
    {0x0100, 0x0020, 0x00872970, {S_A, S_PB, S_A, S_B, S_M}, 2, {S_QB, S_NOTFLAG}},
    {0x0100, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x0200, 0x0002, 0x00864ec0, {S_A, S_B, S_NOTFLAG, S_B, S_PB}, 3, {S_M, S_A, S_NOTFLAG}},
    {0x0200, 0x0004, 0x0086b4d0, {S_A, S_A, S_B, S_B, S_M}, 4, {S_M, S_QB, S_PB, S_NOTFLAG}},
    {0x0200, 0x0008, 0x0086f2a0, {S_A, S_PB, S_QB, S_B, S_M}, 3, {S_B, S_A, S_NOTFLAG}},
    {0x0200, 0x0020, 0x00872c90, {S_QB, S_B, S_PB, S_B, S_M}, 4, {S_M, S_QB, S_A, S_NOTFLAG}},
    {0x0200, 0x8000, 0x00873010, {S_A, S_A, S_QA, S_B, S_M}, 6, {S_M, S_PA, S_B, S_QB, S_PB, S_FLAG}},
    {0x0400, 0x0002, 0x008650c0, {S_PB, S_TB_LOWNOT, S_QA, S_B, S_M}, 5, {S_M, S_B, S_PB, S_A, S_PA}},
    {0x0400, 0x0004, 0x0086b7f0, {S_PB, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_B, S_QB, S_PB, S_A, S_NOTFLAG}},
    {0x0400, 0x0008, 0x0086f3d0, {S_PB, S_PA, S_QB, S_B, S_M}, 6, {S_M, S_B, S_PB, S_A, S_QA, S_NOTFLAG}},
    {0x0400, 0x0010, 0x008716b0, {S_QB, S_QA, S_B, S_B, S_M}, 6, {S_M, S_QB, S_PB, S_A, S_PA, S_NOTFLAG}},
    {0x0400, 0x8000, 0x00872860, {S_A, S_QA, S_PA, S_B, S_M}, 6, {S_M, S_A, S_B, S_QB, S_PB, S_FLAG}},
    {0x8000, 0x0002, 0x008656c0, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0004, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0008, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0010, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0020, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0040, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0080, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0100, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0200, 0x00873010, {S_A, S_B, S_QB, S_B, S_M}, 6, {S_M, S_PB, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x0400, 0x00872860, {S_A, S_QB, S_PB, S_B, S_M}, 6, {S_M, S_B, S_A, S_QA, S_PA, S_NOTFLAG}},
    {0x8000, 0x8000, 0x008732d0, {S_A, S_M, S_PA, S_B, S_M}, 7, {S_A, S_QA, S_PA, S_B, S_QB, S_PB, S_FLAG}},
};

/* The pair generator of address `fn` (every generator of k_pairs is a core port) entered at rx_sp(): the return address there,
 * its stack arguments `a` above it, the registers the dispatcher loaded in `in` (their LTCG parameters, see the adapters in
 * oracle/phys_pair*.cpp). The generators save and restore EBX / EBP / ESI / EDI themselves; their EAX / ECX / EDX / XMM0
 * leftovers are not reproduced here (see phys_contact_pair_observed for why no caller of the dispatcher reads them). */
static void pair_kernel(rx_addr fn, const rx_regs* in, const uint32_t* a) {
    const uint32_t ecx = in->r[1], edx = in->r[2], esi = in->r[6], edi = in->r[7];
    rx_regs caller = *in;
    caller.r[4] = rx_sp();
    uint32_t edx_out = edx;
    switch (fn) {
    case 0x00861a60: { uint32_t ecx_out; (void)phys_pair_sphere_sphere(ecx, edx, a[0], a[1], a[2], a[3], &ecx_out, &edx_out); break; }
    case 0x00861ba0: phys_pair_sphere_box(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x00862da0: phys_pair_sphere_cylinder(ecx, edx, a[0], a[1], a[2], a[3], a[4]); break;
    case 0x00863f70: phys_pair_sphere_triangles(ecx, edx, edi, a[0], a[1], a[2], a[3]); break;
    case 0x00864ec0: phys_pair_sphere_hfterrain(ecx, edi, a[0], a[1], a[2]); break;
    case 0x008656c0: (void)phys_pair_shape_vs_composite(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x00872860: (void)phys_pair_shape_vs_composite_b(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x008657d0: phys_pair_box_box(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5], &edx_out); break;
    case 0x00867ca0: (void)phys_pair_box_cylinder(edx, ecx, a[0], a[1], a[2], a[3], a[4], a[5], &caller, &edx_out); break;
    case 0x0086b7f0: phys_pair_0400_box(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5], &edx_out); break;
    case 0x0086bd80: (void)phys_pair_cylinder_cylinder(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5], &caller, &edx_out); break;
    case 0x0086e750: (void)phys_pair_cylinder_segments(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5], &edx_out); break;
    case 0x00869d40: phys_pair_box_triangles(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x0086a1e0: rx_collision_rays_vs_box(ecx, a[0], a[1], a[2], a[3], a[4] & 0xff, edx, edi); break;
    case 0x0086b4d0: phys_pair_box_hfterrain(ecx, edx, a[0], a[1], a[2], a[3]); break;
    case 0x0086f2a0: phys_pair_cylinder_hfterrain(ecx, edx, edi, a[0], a[1], a[2]); break;
    case 0x0086e090: phys_pair_cylinder_triangles(ecx, edx, edi, a[0], a[1], a[2], a[3], a[4]); break;
    case 0x0086fac0: phys_pair_triangles_segments(ecx, edx, esi, a[0], a[1], a[2], a[3], a[4]); break;
    case 0x008716b0: (void)phys_pair_triangles_circle(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x00873010: phys_pair_composite_hfterrain(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5]); break;
    case 0x008732d0: (void)phys_pair_composite_vs_composite(ecx, a[0], a[1], a[2], a[3], a[4], a[5], a[6]); break;
    case 0x00864460: (void)phys_pair_sphere_rays(ecx, edx, esi, a[0], a[1], a[2], a[3], &edx_out); break;
    case 0x00864dd0: (void)phys_pair_sphere_query(esi, edi, a[0], a[1], a[2]); break;
    case 0x008650c0: (void)phys_pair_sphere_circle(ecx, edx, a[0], a[1], a[2], a[3], a[4], &edx_out); break;
    case 0x0086b360: phys_pair_box_query(ecx, edx, a[0], a[1], a[2], a[3]); break;
    case 0x0086f150: phys_pair_cylinder_query(ecx, edx, edi, a[0], a[1], a[2]); break;
    case 0x0086f3d0: (void)phys_pair_cylinder_circle(ecx, edx, a[0], a[1], a[2], a[3], a[4], a[5], &edx_out); break;
    case 0x00872970: phys_pair_rays_query(ecx, edx, esi, edi, a[0], a[1]); break;
    case 0x00872c90: (void)phys_pair_rays_hfterrain(ecx, edx, a[0], a[1], a[2], a[3], &edx_out); break;
    default:
        /* k_pairs is a closed table of the original's 68 cases, and every generator it names is ported */
        abort();
    }
}

/* The same, called from the dispatcher's frame: the arguments and a 0 return address written below the stack pointer, the
 * generator entered there. */
static void pair_call_core(rx_addr fn, const rx_regs* in, const uint32_t* a, int nargs) {
    const rx_addr sp0 = rx_sp(), entry = sp0 - 4u * (uint32_t)nargs - 4u;
    SU32(entry, 0, 0);
    for (int i = 0; i < nargs; i++) SU32(entry + 4, 4u * (uint32_t)i, a[i]);
    rx_set_sp(entry);
    pair_kernel(fn, in, a);
    rx_set_sp(sp0);
}

/* 0085ddd0 Collision_ContactDispatch (thiscall M; A, rotation A*, position A*, B, rotation B*, position B*, swap flag; RET
 * 0x1c): contacts of shape A against shape B into M. Frame: push ebp; and esp, -8; push ecx / ebx / esi / edi -- the pair
 * generators run at the original's ESP and EBP (they leave uninitialized stack words in their results: the dead-stack trap). */
/* Real return sites in canonical k_pairs order; no synthetic return word.
 * Each entry was individually checked against the complete original selector. */
static const uint32_t pair_return_sites[] = {
 0x85de7b,0x85dea0,0x85dec1,0x85dee3,0x85df05,0x85df40,0x85df59,0x85dfaf,0x85df90,
 0x85e007,0x85e02c,0x85e051,0x85e076,0x85e09a,0x85e0d9,0x85e0f8,0x85e152,0x85e12f,
 0x85e1a6,0x85e1d2,0x85e1f5,0x85e217,0x85e23c,0x85e278,0x85e292,0x85e2ec,0x85e2c9,
 0x85e341,0x85e36d,0x85e396,0x85e3ba,0x85e422,0x85e3fd,
 0x85e477,0x85e4a2,0x85e4ce,0x85e4f9,0x85e538,0x85e555,0x85e590,
 0x85e5ff,0x85e790,
 0x85e673,0x85e697,0x85e6b8,0x85e6da,0x85e721,
 0x85e7dc,0x85e802,0x85e823,0x85e847,0x85e890,
 0x85eb25,0x85eb4f,0x85eb79,0x85eba5,0x85ebe0,
 0x85e8fe,0x85e928,0x85e952,0x85e97c,0x85e9a6,0x85e9d0,0x85ea46,0x85ea1c,0x85ea72,0x85ead6,0x85eaac
};
/* A pair role of the observed dispatcher (phys_contact_dispatch_observed(_live) below) that the finite closures of
 * collision_triangle_ray.c do not model: the generator `target` already entered by the dispatcher's real call, rx_sp() at the
 * return address `return_pc` with its `nargs` stack arguments above it and the registers it loads in the fx model. The role is
 * checked against the case table (the return site names the case), the generator runs as the core port, and the original's
 * RET is modelled (ESP = `expected`).
 * Not modelled: the generator's EAX / ECX / EDX / XMM0 leftovers. The observed dispatcher's callers do not read them: the
 * UTV / buggy / truck vf13 wheel pass reloads ECX from the manifold and EAX / EDX in its count helper right after the call
 * (fourwheel_contact_dispatch.c), the BODY probe (bike_slot13.c body_probe_kernel) reloads ECX from the manifold and EAX / EDX
 * before reading them, and 00872860 after its child dispatch at 0087292b reloads EAX from its frame, ECX in its loop and EDX
 * from M (disassembly 0087292b..00872957); their XMM0 is overwritten or reaches only the cookie / bounds roles. */
void phys_contact_pair_observed(rx_addr target, rx_addr return_pc, unsigned nargs, rx_addr expected) {
    const rx_addr entry = rx_sp();
    size_t k = 0;
    while (k < sizeof pair_return_sites / sizeof pair_return_sites[0] && pair_return_sites[k] != return_pc) k++;
    if (k == sizeof pair_return_sites / sizeof pair_return_sites[0] || k_pairs[k].fn != target || k_pairs[k].nargs != nargs ||
        U32(entry, 0) != return_pc || expected != entry + 4u + 4u * nargs) {
        fprintf(stderr, "rxcore: observed pair role target %08x caller %08x nargs %u: not a case of 0085ddd0\n", target, return_pc,
                nargs);
        abort();
    }
    rx_regs in = phys_regs();
    for (int r = 0; r < 8; r++) in.r[r] = r == FX_ESP ? entry : fx_reg(r);
    uint32_t args[7];
    for (unsigned i = 0; i < nargs; i++) args[i] = U32(entry, 4u + 4u * i);
    pair_kernel(target, &in, args);
    if (rx_sp() != entry || U32(entry, 0) != return_pc) abort();
    rx_set_sp(expected);
}
static int observed_type(uint32_t t){
 switch(t){case 2:case 4:case 8:case 0x10:case 0x20:case 0x40:case 0x80:case 0x100:case 0x200:case 0x400:case 0x8000:return 1;default:return 0;}
}
/* Only reached when the unique canonical table found no pair. Known finite
 * no-pair selectors still leave their real switch arithmetic/index values.
 * A type outside the dispatcher's cases (e.g. CollisionShapeUnknown, type 1)
 * makes no contact either; its ECX / EDX switch leftovers are not modelled
 * (no caller of the observed dispatcher reads them: phys_contact_pair_observed). */
static void observed_no_pair(uint32_t ta,uint32_t tb,uint32_t a10){
 if(!observed_type(ta)||!observed_type(tb))return;
 uint32_t ecx=tb,edx=a10;
 if(tb<=0x20){ecx=tb-2;if(ta==0x40)edx=0;
  if(tb==0x10&&(ta==0x10||ta==0x100||ta==0x200))edx=3;
  if(ta==0x20&&tb==0x20)ecx=4;
  if(ta==0x400&&tb==0x20)edx=4;
 }
 fx_set_reg(FX_ECX,ecx);fx_set_reg(FX_EDX,edx);
}
static void contact_dispatch_kernel(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag,int observe,rx_v4*live){
 fx_frame f;
 if(observe){
  rx_addr entry=rx_sp();const uint32_t input[]={A,QA,PA,B,QB,PB,flag};if(fx_reg(FX_ECX)!=M)abort();for(unsigned i=0;i<7;i++)if(U32(entry,4+4*i)!=input[i])abort();
  fx_frame_enter(&f,4,8,FX_EBX,FX_ESI,FX_EDI);SU32(f.A-4,0,M);fx_set_reg(FX_EAX,A);fx_set_reg(FX_ESI,B);fx_set_reg(FX_EDI,M);
 }
 const uint32_t ta=U32(A,8),tb=U32(B,8),right=U32(B,0xc)&ta;
 const uint32_t rejected=(U32(A,0xc)&tb)|right;
 if(observe){fx_set_reg(FX_ECX,ta);fx_set_reg(FX_EDX,right);fx_set_reg(FX_EBX,rejected);}
 if(rejected)goto done;
 if(observe)fx_set_reg(FX_EDX,U32(A,0x14));
 if((U32(B,0x10)&U32(A,0x14))!=0)goto done;
 if(observe)fx_set_reg(FX_EDX,U32(A,0x10));
 if((U32(B,0x14)&U32(A,0x10))!=0)goto done;
 for(size_t k=0;k<sizeof k_pairs/sizeof k_pairs[0];k++){
  const struct pair_case*c=&k_pairs[k];if(c->type_a!=ta||c->type_b!=tb)continue;
  const uint32_t notflag=(flag&0xff)==0;
  const uint32_t src[]={M,A,QA,PA,B,QB,PB,flag,notflag,tb,(tb&~0xffu)|notflag,(tb&~0xffu)|(flag&0xff),(PB&~0xffu)|(flag&0xff),U32(A,0x10)};
  const rx_addr sp0=rx_sp();rx_regs in=phys_regs();in.r[0]=src[c->reg[0]];in.r[1]=src[c->reg[1]];in.r[2]=src[c->reg[2]];in.r[6]=src[c->reg[3]];in.r[7]=src[c->reg[4]];in.r[3]=0;in.r[5]=observe?f.ebp:sp0-4;
  uint32_t args[7];for(unsigned i=0;i<c->nargs;i++)args[i]=src[c->arg[i]];
  if(observe){
   if(sizeof pair_return_sites/sizeof pair_return_sites[0]!=sizeof k_pairs/sizeof k_pairs[0])abort();
   for(unsigned i=0;i<8;i++)if(i!=FX_ESP)fx_set_reg(i,in.r[i]);
   rx_addr caller=rx_sp();(void)fx_push(c->nargs,args,pair_return_sites[k]);
   uint32_t compare_left=ta>tb?(flag&255u):(tb<=0x20?tb-2:tb),compare_right=ta>tb?0u:(tb<=0x20?0x1eu:tb);
   if(live)rx_collision_dispatch_pair_observed_live(c->fn,pair_return_sites[k],c->nargs,caller,compare_left,compare_right,ta>tb?8u:32u,live);else rx_collision_dispatch_pair_observed(c->fn,pair_return_sites[k],c->nargs,caller,compare_left,compare_right,ta>tb?8u:32u);if(rx_sp()!=caller)abort();
  }else{
   rx_set_sp(((sp0-4)&~7u)-16);pair_call_core(c->fn,&in,args,c->nargs);rx_set_sp(sp0);
  }
  goto done;
 }
 if(observe)observed_no_pair(ta,tb,U32(A,0x10));
done:
 if(observe)fx_frame_leave(&f);
}
void phys_contact_dispatch(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag){contact_dispatch_kernel(M,A,QA,PA,B,QB,PB,flag,0,0);}
void phys_contact_dispatch_observed(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag){contact_dispatch_kernel(M,A,QA,PA,B,QB,PB,flag,1,0);}
void phys_contact_dispatch_observed_live(rx_addr M,rx_addr A,rx_addr QA,rx_addr PA,rx_addr B,rx_addr QB,rx_addr PB,uint32_t flag,rx_v4*live){contact_dispatch_kernel(M,A,QA,PA,B,QB,PB,flag,1,live);}
RX_FUNCTION(0x0085ddd0, phys_contact_dispatch)
