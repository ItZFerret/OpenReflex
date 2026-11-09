/* phys_coll_pair.c -- shape pairs with composites (native/game/notes/collision.md). A composite shape (+0x2c / +0x30 children vector)
 * is paired child by child with the other shape through the contact dispatcher; the contacts each child pair adds are tagged with the
 * child's index.
 * Contact manifold M: +0 contact count, +0x5b0 / +0x628 per-contact child index of shape A / shape B (swapped when `swap`). The
 * children's world rotation / position come from phys_composite_child_rotation / _position (008486f0 / 00848650).
 * Each function keeps the original's frame so the dispatcher and the children run at the original's ESP (the garbage they read from
 * the dead stack is then the original's: native/game/notes/dead-stack.md). */
#include "phys_coll_pair.h"
#include <stdlib.h>
#include "phys_coll_shape.h"
#include "phys_contact.h"

/* 0085ddd0 Collision_ContactDispatch (phys_contact.c): thiscall M; (shapeA, rotA*, posA*, shapeB, rotB*, posB*, swap) */
/* called as the original calls it: seven stack arguments and the return address below the caller's ESP (the dispatcher and the
 * pair generators then run at the original's stack addresses) */
static void dispatch_pair(rx_addr M, rx_addr shapeA, rx_addr rotA, rx_addr posA, rx_addr shapeB, rx_addr rotB, rx_addr posB, uint32_t swap) {
    const rx_addr sp = rx_sp();
    const uint32_t args[7] = {shapeA, rotA, posA, shapeB, rotB, posB, swap};
    for (int i = 0; i < 7; i++) SU32(sp - 28, 4u * (uint32_t)i, args[i]);
    rx_set_sp(sp - 32);
    phys_contact_dispatch(M, shapeA, rotA, posA, shapeB, rotB, posB, swap);
    rx_set_sp(sp);
}

/* Tags the contacts added since `from` (indices from .. M.count - 1; the count is re-read per contact) with `child` in the id array
 * `ids`. Returns the manifold's contact count. */
static uint32_t tag_new_contacts(rx_addr M, rx_addr ids, uint32_t from, uint32_t child) {
    for (int32_t k = (int32_t)from; k < I32(M, 0); k++) SU32(ids, (uint32_t)k * 4, child);
    return U32(M, 0);
}

/* the size in bytes of a composite's children vector, and its element count (arithmetic shift) */
static int32_t child_bytes(rx_addr comp) { return I32(comp, 0x30) - I32(comp, 0x2c); }
static int32_t child_count(rx_addr comp) { return child_bytes(comp) >> 2; }
static rx_addr child_at(rx_addr comp, uint32_t i) { return U32(U32(comp, 0x2c), i * 4); }

/* box overlap: minB <= maxA and minA <= maxB on x, y, z (cmpleps; the three lane masks summed as -3) */
static int boxes_overlap(rx_addr minA, rx_addr maxA, rx_addr minB, rx_addr maxB) {
    const rx_v4 a = v4_cmple(v4_ld(minB), v4_ld(maxA));
    const uint32_t sum = v4_lb(a, 2) + v4_lb(a, 1) + v4_lb(a, 0);
    if (sum != 0xfffffffdu) return 0;
    const rx_v4 b = v4_cmple(v4_ld(minA), v4_ld(maxB));
    return v4_lb(b, 2) + v4_lb(b, 1) + v4_lb(b, 0) == sum;
}

/* 008732d0 CollisionPair_CompositeVsComposite: every pair of children (one of each composite) whose world boxes overlap is sent through
 * the dispatcher; the contacts a child pair adds are tagged with the child indices of both composites (A: ids at +0x5b0, B: +0x628;
 * swapped with `swap`). Frame (sub esp, 0xd4; push ebx / esi / edi): +0x50 posA, +0x60 posB, +0x70 rotB, +0x80 rotA, +0x90 maxA,
 * +0xa0 minA, +0xb0 minB, +0xc0 maxB. Returns EAX: the manifold's count after the last A child (the B ids array without children). */
uint32_t phys_pair_composite_vs_composite(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {
    const int sw = (swap & 0xff) != 0;
    const rx_addr ids_a = M + (sw ? 0x628 : 0x5b0), ids_b = M + (sw ? 0x5b0 : 0x628);
    uint32_t count_a = U32(M, 0), count_b = count_a;
    uint32_t eax = ids_b;
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0xd4 - 12;
    rx_set_sp(F);
    const rx_addr rot_a = F + 0x80, pos_a = F + 0x50, max_a = F + 0x90, min_a = F + 0xa0;
    const rx_addr rot_b = F + 0x70, pos_b = F + 0x60, min_b = F + 0xb0, max_b = F + 0xc0;
    if ((child_bytes(A) & ~3) > 0) {
        for (uint32_t i = 0; (int32_t)i < child_count(A); i++) {
            const rx_addr child_a = child_at(A, i);
            phys_composite_child_rotation(A, i, qA, rot_a);
            phys_composite_child_position(A, i, qA, pA, pos_a);
            /* the bounds vcalls at the original's ESP (4 arguments and the return address below the frame, 008733b6 / 00873436):
             * their frames (00846700 for a cylinder) are the dead stack the child pairs' generators read below their entry
             * (0086bd80's kernels: core/notes/phys.md "The bounds frames of CompositeVsComposite") */
            phys_shape_bounds_call(U32(U32(child_a, 0), 8), child_a, rot_a, pos_a, min_a, max_a);
            if ((child_bytes(B) & ~3) > 0) {
                for (uint32_t j = 0; (int32_t)j < child_count(B); j++) {
                    const rx_addr child_b = child_at(B, j);
                    phys_composite_child_rotation(B, j, qB, rot_b);
                    phys_composite_child_position(B, j, qB, pB, pos_b);
                    phys_shape_bounds_call(U32(U32(child_b, 0), 8), child_b, rot_b, pos_b, min_b, max_b);
                    if (!boxes_overlap(min_a, max_a, min_b, max_b)) continue;
                    dispatch_pair(M, child_a, rot_a, pos_a, child_b, rot_b, pos_b, swap);
                    count_b = tag_new_contacts(M, ids_b, count_b, j);
                }
            }
            count_a = tag_new_contacts(M, ids_a, count_a, i);
            eax = count_a;
        }
    }
    rx_set_sp(entry_sp);
    return eax;
}
RX_FUNCTION(0x008732d0, phys_pair_composite_vs_composite)

/* 008656c0 CollisionPair_ShapeVsComposite: shape A against every child of composite B (no box test; the child count read once),
 * the new contacts tagged with the child index in B's id array (+0x5b0 when swapped, else +0x628). Frame (sub esp, 0x54; push
 * ebx / esi / edi): +0x30 child position, +0x40 child rotation. Returns EAX: the child count, then the manifold's count after each
 * child. */
uint32_t phys_pair_shape_vs_composite(rx_addr rotA, rx_addr posA, rx_addr M, rx_addr shapeA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap) {
    const int32_t n = child_count(B);
    const rx_addr ids = M + ((swap & 0xff) ? 0x5b0 : 0x628);
    uint32_t count = U32(M, 0), eax = (uint32_t)n;
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x54 - 12;
    rx_set_sp(F);
    const rx_addr rot = F + 0x40, pos = F + 0x30;
    for (int32_t i = 0; i < n; i++) {
        const rx_addr child = child_at(B, (uint32_t)i);
        phys_composite_child_rotation(B, (uint32_t)i, qB, rot);
        phys_composite_child_position(B, (uint32_t)i, qB, pB, pos);
        dispatch_pair(M, shapeA, rotA, posA, child, rot, pos, swap);
        count = tag_new_contacts(M, ids, count, (uint32_t)i);
        eax = count;
    }
    rx_set_sp(entry_sp);
    return eax;
}
RX_FUNCTION(0x008656c0, phys_pair_shape_vs_composite)

/* 00872860 CollisionPair_ShapeVsCompositeB: as 008656c0, with the child count read again after every child (children added during the
 * dispatch are visited) and no iteration without children. Returns EAX: the child count (the byte size when there are no children). */
#include "composite_pair_observed.h"
#include "fx_util.h"
static void composite_call(rx_addr fn,rx_addr ra,unsigned n,const uint32_t*args,rx_v4*live){
 const rx_addr before=rx_sp();
 for(unsigned i=0;i<n;i++)SU32(before-4*n,4*i,args[i]);
 SU32(before-4*n-4,0,ra);rx_set_sp(before-4*n-4);
 rx_composite_pair_dependency(fn,ra,n,before,live);
 if(rx_sp()!=before)abort();
}
/* One semantic loop for ordinary and observed entry. Optional observation
 * exposes actual existing intermediate register/stack state, not new math. */
static uint32_t composite_b_kernel(rx_addr rotA,rx_addr posA,rx_addr M,rx_addr shapeA,rx_addr B,rx_addr qB,rx_addr pB,uint32_t swap,rx_v4*live){
 const rx_addr entered=rx_sp();fx_frame f;uint32_t shadow[40]={0},regs[8];for(unsigned r=0;r<8;r++)regs[r]=fx_reg(r);
 if(live)fx_frame_enter(&f,0x54,16,FX_EBX,FX_ESI,FX_EDI);
 else{f.sp0=entered;f.ebp=entered-4;f.A=f.ebp&~15u;f.E=f.A-0x60;rx_set_sp(f.E);}
 const rx_addr E=f.E;
 /* Ordinary C callers supply typed arguments, not a fabricated guest ABI.
  * Private scalar observer spills use host shadow slots in that path. Only
  * actual child vectors and the pre-existing dispatch argument writes touch
  * guest RAM; ordinary entry does not add saved-register/cookie writes. */
#define CR(r) (live?fx_reg(r):regs[(r)])
#define CS(r,v) do{uint32_t cv_=(v);if(live)fx_set_reg((r),cv_);else regs[(r)]=cv_;}while(0)
#define CW(addr,off,val) do{rx_addr ca_=(addr)+(off);uint32_t cv_=(val);if(live)SU32(ca_,0,cv_);else shadow[(ca_-(E-32))/4]=cv_;}while(0)
#define CG(addr,off) (live?U32((addr),(off)):shadow[((addr)+(off)-(E-32))/4])
 if(live)CW(E,0x5c,U32(0x9b45e4,0)^(f.A-0x54));
 CS(FX_EAX,shapeA);CS(FX_EBX,M);CS(FX_EDI,B);
 CW(E,0x18,shapeA);CW(E,0x14,rotA);CW(E,0x20,posA);
 const rx_addr ids=M+((swap&255)?0x5b0:0x628);
 if(swap&255)CS(FX_EAX,ids);else CS(FX_ECX,ids);
 CW(E,0x10,ids);
 uint32_t eax=U32(B,0x30)-U32(B,0x2c);CS(FX_EAX,eax);
 CS(FX_EDX,U32(M,0));CS(FX_ESI,0);CW(E,0x2c,CR(FX_EDX));
 if((int32_t)(eax&~3u)>0){
  for(;;){
   const uint32_t i=CR(FX_ESI);CS(FX_ECX,(uint32_t)((int32_t)(U32(B,0x30)-U32(B,0x2c))>>2));
   if(i>=CR(FX_ECX)){
    if(live)composite_call(0x916224,0x8728d4,0,0,live);else abort();
   }
   CS(FX_EDX,U32(B,0x2c));CS(FX_EAX,U32(CR(FX_EDX),i*4));
   CS(FX_EDX,(live?U32(f.ebp,0x14):qB));CS(FX_ECX,E+0x40);
   CW(E-4,0,CR(FX_ECX));CW(E-8,0,CR(FX_EDX));
   CS(FX_ECX,i);CW(E,0x1c,CR(FX_EAX));
   if(live){const uint32_t args[]={CG(E-8,0),CG(E-4,0)};composite_call(0x848650,0x8728ee,2,args,live);}
   else phys_composite_child_rotation(B,i,qB,E+0x40);
   CS(FX_ECX,(live?U32(f.ebp,0x18):pB));CS(FX_EDX,(live?U32(f.ebp,0x14):qB));CS(FX_EAX,E+0x30);
   CW(E-4,0,CR(FX_EAX));CW(E-8,0,CR(FX_ECX));CW(E-12,0,CR(FX_EDX));CS(FX_ECX,i);
   if(live){const uint32_t args[]={CG(E-12,0),CG(E-8,0),CG(E-4,0)};composite_call(0x8486f0,0x872902,3,args,live);}
   else phys_composite_child_position(B,i,qB,pB,E+0x30);
   CS(FX_EAX,(live?U32(f.ebp,0x1c):swap));CW(E-4,0,CR(FX_EAX));
   CS(FX_EAX,CG(E,0x1c));CS(FX_ECX,E+0x30);CW(E-8,0,CR(FX_ECX));
   CS(FX_ECX,CG(E,0x20));CS(FX_EDX,E+0x40);CW(E-12,0,CR(FX_EDX));
   CS(FX_EDX,CG(E,0x14));CW(E-16,0,CR(FX_EAX));CS(FX_EAX,CG(E,0x18));
   CW(E-20,0,CR(FX_ECX));CW(E-24,0,CR(FX_EDX));CW(E-28,0,CR(FX_EAX));CS(FX_ECX,M);
   if(live){uint32_t args[7];for(unsigned k=0;k<7;k++)args[k]=CG(E-28,k*4);composite_call(0x85ddd0,0x87292b,7,args,live);}
   else dispatch_pair(M,shapeA,rotA,posA,CG(E,0x1c),E+0x40,E+0x30,swap);
   eax=CG(E,0x2c);CS(FX_EAX,eax);
   while((int32_t)eax<I32(M,0)){CS(FX_ECX,CG(E,0x10));SU32(CR(FX_ECX),eax*4,i);eax++;CS(FX_EAX,eax);}
   eax=U32(B,0x30)-U32(B,0x2c);CS(FX_EAX,eax);CS(FX_EDX,U32(M,0));
   CS(FX_ESI,i+1);eax=(uint32_t)((int32_t)eax>>2);CS(FX_EAX,eax);CW(E,0x2c,CR(FX_EDX));
   if((int32_t)CR(FX_ESI)>=(int32_t)eax)break;
  }
 }
 if(live){
 CS(FX_ECX,U32(E,0x5c));CS(FX_EDI,U32(E,0));CS(FX_ESI,U32(E,4));CS(FX_EBX,U32(E,8));
 rx_set_sp(E+12);CS(FX_ECX,CR(FX_ECX)^rx_sp());composite_call(0x8f9efe,0x872965,0,0,live);fx_frame_leave(&f);
 }else rx_set_sp(entered);
#undef CR
#undef CS
#undef CW
#undef CG
 return eax;
}
uint32_t phys_pair_shape_vs_composite_b(rx_addr rotA,rx_addr posA,rx_addr M,rx_addr shapeA,rx_addr B,rx_addr qB,rx_addr pB,uint32_t swap){return composite_b_kernel(rotA,posA,M,shapeA,B,qB,pB,swap,0);}
uint32_t phys_pair_shape_vs_composite_b_observed(rx_addr rotA,rx_addr posA,rx_addr M,rx_addr shapeA,rx_addr B,rx_addr qB,rx_addr pB,uint32_t swap,rx_v4*live){if(!live)abort();return composite_b_kernel(rotA,posA,M,shapeA,B,qB,pB,swap,live);}
RX_FUNCTION(0x00872860, phys_pair_shape_vs_composite_b)
