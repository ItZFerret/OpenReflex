/* phys_coll_terrain.c -- the terrain height-field queries of the collision system (native/game/notes/collision.md, _re/notes/
 * terrain-query-original-math.md). The terrain object: base heights (float grid) +0x1c, deformation surface (D3D texture) +0x240,
 * deformation gain +0xbc, row stride +0x44bc, cells +0x44c8 / +0x44cc (floats), world size +0x44d0 / +0x44d4, spacing +0x44e8, normal
 * y +0x44ec. Height = ((deform - 0.5) * 2 * gain) + base * 2048.
 *
 * "Cell work" (0x40 bytes, ESI in the LTCG functions): +0x00 cell (floor of the grid position, clamped), +0x10 fraction, +0x20 cell
 * counts minus the bias, +0x30 window origin (cells -1..+2 around the cell). */
#include "phys_coll_terrain.h"
#include "phys_coll_grid.h"
#include "phys_math.h"
#include "../src/rx_d3d.h"
#include "terrain_height_frames.h"
#include "terrain_cell_frames.h"
#include "terrain_window_frames.h"
#ifdef RX_TERRAIN_HEIGHT_COMPARE
extern void rx_height_c_enter(unsigned);
#endif

/* Writable runtime vec4s of the exe: 0xd61660 zero, 0xd61690 cell bias, 0xd615c0 column step, 0xd61680 row step, 0xd616a0 up normal, and
 * the float 0x9cd9e8 (-1.0f): read from memory each time. */
#define RT_ZERO4   0xd61660u
#define RT_BIAS    0xd61690u
#define RT_COLSTEP 0xd615c0u
#define RT_ROWSTEP 0xd61680u
#define RT_UP      0xd616a0u
#define RT_MINUS1  0x9cd9e8u

/* The sum of the four lane masks of a cmpltps (-4 when all lanes are set). */
static uint32_t mask_sum4(rx_v4 m) { return v4_lb(m, 3) + v4_lb(m, 2) + v4_lb(m, 1) + v4_lb(m, 0); }

/* 005452d0 Vec4_FromScalars (inlined, a pure lane arrangement): (x3, x2, x1, x0). */
static rx_v4 vec_from_scalars(float x0, float x1, float x2, float x3) { return v4(x3, x2, x1, x0); }

/* x87 fld m32 / fstp m32: a copy, except that a signaling NaN comes back quiet. */
static float copy_through_x87(float f) { return f != f ? rx_bf(rx_fb(f) | 0x400000u) : f; }

/* the D3D texture's LockRect (vtable slot 13: this, rect*, NULL, flags; stdcall) and UnlockRect (slot 14) */
static void texture_lock(rx_addr tex, rx_addr rect) {
    const uint32_t args[4] = {tex, rect, 0, 0x810};
    rx_d3d_call(tex, 13, 4, args);
}
static void texture_unlock(rx_addr tex) {
    const uint32_t args[1] = {tex};
    rx_d3d_call(tex, 14, 1, args);
}

/* 007c51e0 TerrainCellSetup (ESI work, ECX dims*, EDX position*, stack cells*; cdecl): the grid position of the point
 * t = ((0.5 * dims + pos) * (cells - bias)) / dims; cell = floor(t + (0,0,1,1)), fraction t - cell. Returns 1 when the cell lies
 * inside the grid (0 < cell < cells - bias on every lane), else 0 with the cell clamped to [0, cells - bias]. The EDX the original
 * leaves is the lane-mask sum of the first test. */
static uint32_t terrain_cell_setup(rx_addr work,rx_addr dims,rx_addr pos,rx_addr cells,uint32_t*edx_out,int exact,rx_v4*live){
 const rx_addr entry=rx_sp();fx_frame frame;rx_addr F=0;
 if(exact){
  if(!live||fx_reg(FX_ESI)!=work||fx_reg(FX_ECX)!=dims||fx_reg(FX_EDX)!=pos||U32(entry,4)!=cells)abort();
  #ifdef RX_TERRAIN_HEIGHT_COMPARE
  if(U32(entry,0)!=0x7c5f8b)abort();rx_height_c_enter(0);
#endif
  fx_frame_enter0(&frame,0x20,16);F=frame.E;
  cell_disjoint(work,0x40,F,entry);cell_disjoint(dims,16,F,entry);cell_disjoint(pos,16,F,entry);cell_disjoint(cells,16,F,entry);cell_constants(1);
  SU32(F,0x1c,U32(0,0x9b45e4)^F);fx_set_reg(FX_EAX,cells);
 }
 const rx_v4 counts=v4_sub(v4_ld(cells),v4_ld(RT_BIAS));v4_st(work+0x20,counts);
 const rx_v4 a=v4_ld(dims),b=v4_ld(pos);
 const rx_v4 t=v4_div(v4_mul(v4_add(v4_mul(v4_splat(F32(0,0x9322cc)),a),b),v4_ld(work+0x20)),a);
 v4_st(work+0x10,t);
 rx_v4 bias=vec_from_scalars(F32(0,0x931f24),F32(0,0x931f24),0.f,0.f);
 if(exact){fx_set_reg(FX_EAX,F);cell_vec_ctor(F,bias,0x7c5248);bias=v4_ld(F);}
 const rx_v4 sum=v4_add(bias,v4_ld(work+0x10));
 const rx_v4 cell=exact?cell_floor_frame(sum):phys_vec4_floor(sum);
 v4_st(work,cell);v4_st(work+0x10,v4_sub(v4_ld(work+0x10),cell));
 const rx_v4 zero=v4_ld(RT_ZERO4),c=v4_ld(work),mask=v4_cmplt(zero,c);
 const uint32_t above=mask_sum4(mask);if(edx_out)*edx_out=above;
 if(exact){rx_st4(F,mask);fx_set_reg(FX_ECX,v4_lb(mask,2));fx_set_reg(FX_EDX,above);}
 if(above==0xfffffffcu){
  const rx_v4 inside=v4_cmplt(c,v4_ld(work+0x20));const uint32_t total=mask_sum4(inside);
  if(exact){rx_st4(F,inside);fx_set_reg(FX_EAX,v4_lb(inside,2));fx_set_reg(FX_ECX,total);}
  if(total==above){
   const uint32_t result=above+5;if(exact){*live=c;fx_set_reg(FX_EAX,result);cell_cookie_leave(&frame,0x7c52c2);}return result;
  }
 }
 const rx_v4 clamped=v4_min(v4_ld(work+0x20),v4_max(zero,c));v4_st(work,clamped);
 if(exact){*live=clamped;fx_set_reg(FX_EAX,0);cell_cookie_leave(&frame,0x7c52e0);}return 0;
}
uint32_t phys_terrain_cell_setup(rx_addr work,rx_addr dims,rx_addr pos,rx_addr cells,uint32_t*edx_out){return terrain_cell_setup(work,dims,pos,cells,edx_out,0,0);}
uint32_t phys_terrain_cell_setup_exact(rx_addr work,rx_addr dims,rx_addr pos,rx_addr cells,rx_v4*live_xmm0){return terrain_cell_setup(work,dims,pos,cells,0,1,live_xmm0);}
RX_FUNCTION(0x007c51e0, phys_terrain_cell_setup)

/* 007c52f0 TerrainCellOffsets (ESI work; cdecl): the window origin at +0x30 = cell + (-1,-1,1,1) (lanes x0 z0 x1 z1), pulled back into
 * the grid: minus max(0, origin.hi - counts) minus min(0, origin.lo). */
static void terrain_cell_offsets(rx_addr work,int exact,rx_v4*live){
 const rx_addr entry=rx_sp();fx_frame frame;rx_addr F=0;
 if(exact){
  if(!live||fx_reg(FX_ESI)!=work)abort();
#ifdef RX_TERRAIN_HEIGHT_COMPARE
  if(U32(entry,0)!=0x7c5fc2)abort();rx_height_c_enter(1);
#endif
  fx_frame_enter0(&frame,0x20,16);F=frame.E;
  cell_disjoint(work,0x40,F,entry);cell_constants(0);SU32(F,0x1c,U32(0,0x9b45e4)^F);
 }
 rx_v4 delta=vec_from_scalars(F32(0,0x931f24),F32(0,0x931f24),F32(0,RT_MINUS1),F32(0,RT_MINUS1));
 if(exact){fx_set_reg(FX_EAX,F);cell_vec_ctor(F,delta,0x7c5323);delta=v4_ld(F);}
 v4_st(work+0x30,v4_add(v4_ld(work),delta));
 const rx_v4 v=v4_ld(work+0x30),v2=v4_ld(work+0x30),zero=v4_ld(RT_ZERO4);
 const rx_v4 over=v4_max(zero,v4_sub(v4_shuf(v,v,0xee),v4_ld(work+0x20)));
 const rx_v4 low=v4_shuf(v2,v2,0x44),under=v4_min(zero,low);
 v4_st(work+0x30,v4_sub(v4_sub(v4_ld(work+0x30),over),under));
 if(exact){*live=low;cell_cookie_leave(&frame,0x7c5373);}
}
void phys_terrain_cell_offsets(rx_addr work){terrain_cell_offsets(work,0,0);}
void phys_terrain_cell_offsets_exact(rx_addr work,rx_v4*live_xmm0){terrain_cell_offsets(work,1,live_xmm0);}
RX_FUNCTION(0x007c52f0, phys_terrain_cell_offsets)

/* One bilinear sample of a 2x2 patch p (p0 p1 / p2 p3): the two rows lerped by fx, then the rows lerped by fz. */
/* The same canonical bilinear arithmetic in packed form. Every lane follows
 * the original SUBPS/MULPS/ADDPS operand order, including NaN and signed zero. */
static rx_v4 terrain_patch_rows(rx_v4 patch,float fraction){
 const rx_v4 low=v4_shuf(patch,patch,0x88),high=v4_shuf(patch,patch,0xdd);
 return v4_add(v4_mul(v4_sub(high,low),v4_splat(fraction)),low);
}
static rx_v4 terrain_patch_finish(rx_v4 rows,float fraction){
 const rx_v4 low=v4_splat(rows.v[0]),high=v4_splat(rows.v[1]);
 return v4_add(v4_mul(v4_sub(high,low),v4_splat(fraction)),low);
}
static float bilerp(const float p[4],float fx,float fz){return terrain_patch_finish(terrain_patch_rows(v4(p[0],p[1],p[2],p[3]),fx),fz).v[0];}

/* 007c5440 TerrainSampleWindow (ESI work, EDI center out; stack neighbours out*, grid*, scale*; cdecl): samples the 4x4 window of
 * the height grid around the cell (row r starts at index floor(scale * (w34, w04, w0c, w3c) + w30)[r], rows A..D, columns 0..3) with
 * bilinear interpolation (fx = work+0x10, fz = work+0x14) at the centre patch (out_center, splat) and the four neighbour patches
 * (out_nbr = left, right, up, down). */
static void terrain_sample_window(rx_addr work,rx_addr out_center,rx_addr out_nbr,rx_addr grid,rx_addr scale,int exact,rx_v4*live){
 const rx_addr entry=rx_sp();fx_frame frame;rx_addr F=0;
 if(exact){
  if(!live||fx_reg(FX_ESI)!=work||fx_reg(FX_EDI)!=out_center||U32(entry,4)!=out_nbr||U32(entry,8)!=grid||U32(entry,12)!=scale)abort();
  #ifdef RX_TERRAIN_HEIGHT_COMPARE
  uint32_t caller=U32(entry,0);if(caller!=0x7c5fd6&&caller!=0x7c601a)abort();rx_height_c_enter(caller==0x7c5fd6?2:3);
#endif
  fx_frame_enter(&frame,0xcc,16,FX_EBX);F=frame.E;
  cell_disjoint(work,0x40,F,entry);cell_disjoint(out_center,16,F,entry);cell_disjoint(out_nbr,16,F,entry);cell_disjoint(scale,16,F,entry);cell_constants(1);
  SU32(F,0xcc,U32(0,0x9b45e4)^(F+4));SU32(F,0x4c,out_nbr);
 }
 /* Actual prologue assembles (w34,w04,w0c,w3c), retaining raw bits. No output
  * can modify work before this capture or the four subsequent raw gathers. */
 const float row_b=F32(work,4),row_a=F32(work,0x34),row_c=F32(work,0xc),row_d=F32(work,0x3c);
 const rx_v4 positions=v4(row_a,row_b,row_c,row_d),scaled=v4_mul(v4_ld(scale),positions);
 const rx_v4 coordinate=v4_add(scaled,v4_splat(F32(work,0x30)));rx_v4 indices;
 if(exact){
  fx_set_reg(FX_EAX,rx_fb(row_d));fx_set_reg(FX_ECX,F+0x10);fx_set_reg(FX_EDX,scale);fx_set_reg(FX_EBX,grid);
  rx_st4(F+0x10,positions);rx_st4(F+0x20,v4_splat(row_d));
  /* Two pending pointer pushes precede floor. ADD4 consumes only the top
   * dummy; Store RET0 leaves the genuine destination until later ADD4. */
  SU32(F-4,0,F+0x10);SU32(F-8,0,F+0x10);SU32(F-12,0,0x7c54ee);rx_set_sp(F-12);
  indices=window_floor_to_int_entered(coordinate,F+0x10);if(rx_sp()!=F-8)abort();rx_set_sp(F-4);
  SU32(F-8,0,0x7c54f6);rx_set_sp(F-8);window_store_entered(indices,F+0x10);if(rx_sp()!=F-4)abort();
 }else indices=phys_vec4_floor_to_int(coordinate);
 rx_v4 rows[4];for(unsigned r=0;r<4;r++){
  const rx_addr address=grid+4u*v4_lb(indices,r);if(exact)cell_disjoint(address,16,F,entry);
  rows[r]=v4_ld(address); /* original MOVUPS: unaligned grid row is valid */
 }
 const rx_v4 up=v4(rows[0].v[1],rows[0].v[2],rows[1].v[1],rows[1].v[2]);
 const rx_v4 left=v4(rows[1].v[0],rows[1].v[1],rows[2].v[0],rows[2].v[1]);
 const rx_v4 right=v4(rows[1].v[2],rows[1].v[3],rows[2].v[2],rows[2].v[3]);
 const rx_v4 down=v4(rows[2].v[1],rows[2].v[2],rows[3].v[1],rows[3].v[2]);
 const rx_v4 center=v4(rows[1].v[1],rows[1].v[2],rows[2].v[1],rows[2].v[2]);
 /* Original FX is loaded before FZ, cached for all four neighbours. The
  * centre alone reads FX after neighbour publication and FZ after its row
  * output, preserving real out/work aliases outside the private frame. */
 const float fx=F32(work,0x10),fz=F32(work,0x14);
 const rx_v4 result_up=terrain_patch_finish(terrain_patch_rows(up,fx),fz);
 const rx_v4 result_left=terrain_patch_finish(terrain_patch_rows(left,fx),fz);
 const rx_v4 result_right=terrain_patch_finish(terrain_patch_rows(right,fx),fz);
 const rx_v4 result_down=terrain_patch_finish(terrain_patch_rows(down,fx),fz);
 const rx_v4 neighbors=v4(result_left.v[0],result_right.v[0],result_up.v[0],result_down.v[0]);
 if(exact){
  /* No calls occur between these original copy/shuffle stores and the final
   * cookie. These are the same computed raw patches/results, not re-evaluation
   * or output replay. Unwritten holes F04..0C/F40..48/Fc0..c8 stay untouched. */
  rx_st4(F+0x10,center);rx_st4(F+0x20,neighbors);rx_st4(F+0x30,result_down);
  rx_st4(F+0x50,left);rx_st4(F+0x60,right);rx_st4(F+0x70,up);
  rx_st4(F+0x80,result_up);rx_st4(F+0x90,result_left);rx_st4(F+0xa0,result_right);rx_st4(F+0xb0,result_down);
  fx_set_reg(FX_EAX,rx_fb(result_down.v[0]));fx_set_reg(FX_EDX,rx_fb(result_up.v[0]));fx_set_reg(FX_ECX,out_nbr);
  rx_set_sp(F); /* actual ADD ESP4 at7c55ca consumes retained store argument */
 }
 v4_st(out_nbr,neighbors);const uint32_t cookie=exact?U32(F,0xcc):0;
 const float center_fx=F32(work,0x10);v4_st(out_center,terrain_patch_rows(center,center_fx));
 const float low=F32(out_center,0),high=F32(out_center,4),center_fz=F32(work,0x14);
 if(exact){fx_set_reg(FX_ECX,cookie^(F+4));if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort();fx_set_reg(FX_EBX,frame.in.ebx);rx_set_sp(F+4);}
 const rx_v4 result=terrain_patch_finish(v4(low,high,low,high),center_fz);
 if(exact){
  /* Original POP EBX precedes final packed mul/add/store. The cookie CALL
   * then overwrites the consumed EBX save at F with its actual return PC. */
  v4_st(out_center,result);*live=result;
  SU32(F,0,0x7c5940);rx_set_sp(F);phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(F+4);fx_frame_leave(&frame);
 }else v4_st(out_center,result);
}
void phys_terrain_sample_window(rx_addr work,rx_addr center,rx_addr neighbors,rx_addr grid,rx_addr scale){terrain_sample_window(work,center,neighbors,grid,scale,0,0);}
void phys_terrain_sample_window_exact(rx_addr work,rx_addr center,rx_addr neighbors,rx_addr grid,rx_addr scale,rx_v4*live_xmm0){terrain_sample_window(work,center,neighbors,grid,scale,1,live_xmm0);}
RX_FUNCTION(0x007c5440, phys_terrain_sample_window)

/* 007c5380 TerrainSampleCell (ESI grid, EDI work, ECX scale*, stack out*; cdecl): the bilinear sample of the cell's 2x2 corners, the
 * indices floor((w1, w1, w3, w3) * scale + (w0, w2, w0, w2)) of work vec w, splat into *out. Returns the EDX the original leaves (the
 * first corner's index). */
uint32_t phys_terrain_sample_cell(rx_addr grid, rx_addr work, rx_addr scale, rx_addr out) {
    const rx_v4 w = v4_ld(work);
    const rx_v4 idx = phys_vec4_floor_to_int(v4_add(v4_mul(v4_shuf(w, w, 0xf5), v4_ld(scale)), v4_shuf(w, w, 0x88)));
    const float c3 = F32(grid, 4 * v4_lb(idx, 3)), c2 = F32(grid, 4 * v4_lb(idx, 2)), c1 = F32(grid, 4 * v4_lb(idx, 1)), c0 = F32(grid, 4 * v4_lb(idx, 0));
    const float p[4] = {c0, c1, c2, c3};
    v4_st(out, v4_splat(bilerp(p, F32(work, 0x10), F32(work, 0x14))));
    return v4_lb(idx, 0);
}
RX_FUNCTION(0x007c5380, phys_terrain_sample_cell)

/* 007c5e80 TerrainHeightNormal (thiscall: ECX normal out*; stack terrain*, point* in / out; RET 8): the terrain height at the point's
 * (x, z) is put into point.y and the surface normal into *normal (default: the up vector 0xd616a0); returns 1, or 0 when the terrain
 * has no base grid, the deformation surface cannot be locked or the point is outside the grid. The normal is (left - right,
 * normal.y, up - down) of the neighbour samples (base + deformation * 2 * gain), normalised.
 * Frame (push ebp; and esp, -16; sub esp, 0xb4; push ebx / esi / edi; F = ESP): +0x18 lock rect (pitch, bits), +0x20 scaled
 * position / base neighbours, +0x30 extents / deformation neighbours, +0x40 cells / base centre, +0x50 signs / deformation centre
 * / normal-y splat, +0x60 stride splat, +0x70 cell work (0x40 bytes). */
/* One canonical height/normal algorithm, with optional exact caller effects.
 * Cell/window math remains in its existing owners behind explicit roles. */
static uint32_t terrain_height_normal(rx_addr normal,rx_addr terrain,rx_addr point,int exact,rx_v4*live){
 const rx_addr entry=rx_sp();fx_frame frame;rx_addr F;if(exact&&!live)abort();
 if(exact){
  fx_frame_enter(&frame,0xb4,16,FX_EBX,FX_ESI,FX_EDI);F=frame.E;
  /* Prologue's arguments and normal initialization precede the first branch. */
  SU32(F,0xbc,U32(0,0x9b45e4)^(F+12));SU32(F,0x14,point);SU32(F,0x10,normal);
  fx_set_reg(FX_EAX,point);fx_set_reg(FX_EBX,terrain);
  const rx_addr addresses[]={normal,point,terrain};const uint32_t sizes[]={16,16,0x44f0};
  for(unsigned i=0;i<3;i++)if(addresses[i]<entry&&addresses[i]+sizes[i]>F-0x200)abort();
 }else F=((entry-4)&~15u)-0xb4-12;
 v4_st(normal,v4_ld(RT_UP));if(exact)*live=v4_ld(RT_UP);
 if(!U32(terrain,0x1c)){if(exact)height_cookie_leave(&frame,F,0);return 0;}
 if(!exact)rx_set_sp(F);
 v4_st(F+0x20,v4_ld(point));
 if(exact){height_vec_pair(F+0x50,F32(0,RT_MINUS1),F32(0,0x931f24),0x7c5ee7);fx_set_reg(FX_EDI,U32(terrain,0x1c));fx_set_reg(FX_ESI,U32(terrain,0x44bc));}
 else v4_st(F+0x50,vec_from_scalars(F32(0,RT_MINUS1),1.f,F32(0,RT_MINUS1),1.f));
 rx_v4 p=v4_ld(F+0x20);v4_st(F+0x20,v4_mul(v4_shuf(p,p,0x88),v4_ld(F+0x50)));
 if(exact)*live=v4_ld(F+0x50);
 const rx_addr base=exact?fx_reg(FX_EDI):U32(terrain,0x1c);
 const int32_t stride=exact?(int32_t)fx_reg(FX_ESI):I32(terrain,0x44bc);
 rx_addr tex=U32(terrain,0x240);
 if(exact){
  fx_set_reg(FX_EAX,U32(U32(tex,0),0x34));fx_set_reg(FX_ECX,U32(tex,0));fx_set_reg(FX_EDX,F+0x18);
  uint32_t args[]={tex,F+0x18,0,0x810};(void)fx_push(4,args,0x7c5f21);rx_terrain_height_texture(tex,13,F);
 }else texture_lock(tex,F+0x18);
 if(!U32(F,0x1c)){if(exact)height_cookie_leave(&frame,F,0);else rx_set_sp(entry);return 0;}
 if(exact){
  fx_set_reg(FX_ECX,F+0x30);height_vec_pair(F+0x30,F32(terrain,0x44d4),F32(terrain,0x44d0),0x7c5f48);
  fx_set_reg(FX_EDX,F+0x40);height_vec_pair(F+0x40,F32(terrain,0x44cc),F32(terrain,0x44c8),0x7c5f68);
 }else{
  v4_st(F+0x30,vec_from_scalars(F32(terrain,0x44d4),F32(terrain,0x44d0),F32(terrain,0x44d4),F32(terrain,0x44d0)));
  v4_st(F+0x40,vec_from_scalars(F32(terrain,0x44cc),F32(terrain,0x44c8),F32(terrain,0x44cc),F32(terrain,0x44c8)));
 }
 v4_st(F+0x60,v4_splat((float)stride));uint32_t valid;
 if(exact){
  fx_set_reg(FX_EAX,F+0x40);fx_set_reg(FX_EDX,F+0x20);fx_set_reg(FX_ECX,F+0x30);fx_set_reg(FX_ESI,F+0x70);
  uint32_t arg=F+0x40;(void)fx_push(1,&arg,0x7c5f8b);*live=rx_terrain_height_dependency(RX_HEIGHT_CELL_SETUP,F-4);rx_set_sp(F);valid=fx_reg(FX_EAX);
 }else{uint32_t edx;valid=phys_terrain_cell_setup(F+0x70,F+0x30,F+0x20,F+0x40,&edx);}
 if(!valid){
  tex=U32(terrain,0x240);
  if(exact){fx_set_reg(FX_EBX,tex);fx_set_reg(FX_ECX,U32(tex,0));fx_set_reg(FX_EDX,U32(U32(tex,0),0x38));uint32_t arg=tex;(void)fx_push(1,&arg,0x7c5fa0);rx_terrain_height_texture(tex,14,F);height_cookie_leave(&frame,F,0);}
  else{texture_unlock(tex);rx_set_sp(entry);}return 0;
 }
 if(exact){fx_set_reg(FX_ESI,F+0x70);(void)fx_push(0,0,0x7c5fc2);*live=rx_terrain_height_dependency(RX_HEIGHT_CELL_OFFSETS,F);}
 else phys_terrain_cell_offsets(F+0x70);
 if(exact){
  fx_set_reg(FX_EAX,F+0x60);fx_set_reg(FX_ECX,F+0x20);uint32_t args[]={F+0x20,base,F+0x60};fx_set_reg(FX_EDI,F+0x40);
  (void)fx_push(3,args,0x7c5fd6);*live=rx_terrain_height_dependency(RX_HEIGHT_SAMPLE_BASE,F-12);
 }else phys_terrain_sample_window(F+0x70,F+0x40,F+0x20,base,F+0x60);
 /* Original re-reads lock bits after first sample while its3args are pending. */
 const rx_addr bits=U32(F,0x1c);if(exact)fx_set_reg(FX_EAX,bits);rx_set_sp(F);
 const rx_v4 k2048=v4_splat(F32(0,0x92e22c));v4_st(F+0x20,v4_mul(k2048,v4_ld(F+0x20)));v4_st(F+0x40,v4_mul(k2048,v4_ld(F+0x40)));
 if(exact){
  fx_set_reg(FX_EDX,F+0x60);fx_set_reg(FX_ECX,F+0x30);fx_set_reg(FX_EDI,F+0x50);
  uint32_t args[]={F+0x30,bits,F+0x60};(void)fx_push(3,args,0x7c601a);*live=rx_terrain_height_dependency(RX_HEIGHT_SAMPLE_DEFORM,F-12);
 }else phys_terrain_sample_window(F+0x70,F+0x50,F+0x30,bits,F+0x60);
 rx_v4 h=v4_sub(v4_ld(F+0x50),v4_splat(F32(0,0x9322cc)));
 h=v4_mul(h,v4_splat(F32(0,0x92e330)));h=v4_mul(h,v4_splat(F32(terrain,0xbc)));h=v4_add(h,v4_ld(F+0x40));
 const rx_addr outputPoint=exact?U32(F,0x14):point;if(exact)fx_set_reg(FX_EAX,outputPoint);
 const rx_v4 pt=v4_ld(outputPoint);v4_st(outputPoint,v4_movss(v4_shuf(h,pt,0xe5),pt));
 const uint32_t normalY=exact?fx_x87_bits(U32(terrain,0x44ec)):rx_fb(copy_through_x87(F32(terrain,0x44ec)));
 /* Original FSTP overwrites oldest pending arg at F-4; ADD ESP8 retains it
  * as the scalar argument, then PUSH out and CALL Splat RET8 restore F. */
 const float gain=phys_mul(F32(terrain,0xbc),F32(0,0x92e330));
 v4_st(F+0x30,v4_mul(v4_splat(gain),v4_ld(F+0x30)));
 if(exact){SU32(F-4,0,normalY);rx_set_sp(F-4);fx_set_reg(FX_EDX,F+0x50);SU32(F-8,0,F+0x50);SU32(F-12,0,0x7c609b);rx_set_sp(F-12);height_splat_entered(F+0x50,F);}
 else v4_st(F+0x50,v4_splat(rx_bf(normalY)));
 const float nz=phys_add(phys_sub(F32(F,0x3c),F32(F,0x38)),phys_sub(F32(F,0x2c),F32(F,0x28)));
 const float nx=phys_add(phys_sub(F32(F,0x30),F32(F,0x34)),phys_sub(F32(F,0x20),F32(F,0x24)));
 rx_v4 n=v4_shuf(v4_ld(F+0x50),v4_splat(nz),0);n=v4_movss(n,v4_splat(nx));rx_v4 sq=v4_mul(n,n);
 const float len2=phys_add(phys_add(sq.v[0],sq.v[1]),sq.v[2]);const rx_addr outputNormal=exact?U32(F,0x10):normal;
 const rx_v4 divisor=v4_splat(phys_sqrt(len2));if(exact){fx_set_reg(FX_EAX,outputNormal);*live=divisor;}v4_st(outputNormal,v4_div(n,divisor));
 tex=U32(terrain,0x240);
 if(exact){fx_set_reg(FX_EBX,tex);fx_set_reg(FX_ECX,U32(tex,0));fx_set_reg(FX_EDX,U32(U32(tex,0),0x38));uint32_t arg=tex;(void)fx_push(1,&arg,0x7c6143);rx_terrain_height_texture(tex,14,F);height_cookie_leave(&frame,F,1);}
 else{texture_unlock(tex);rx_set_sp(entry);}return 1;
}
uint32_t phys_terrain_height_normal(rx_addr normal,rx_addr terrain,rx_addr point){return terrain_height_normal(normal,terrain,point,0,0);}
uint32_t phys_terrain_height_normal_exact(rx_addr normal,rx_addr terrain,rx_addr point,rx_v4*live_xmm0){return terrain_height_normal(normal,terrain,point,1,live_xmm0);}
RX_FUNCTION(0x007c5e80, phys_terrain_height_normal)

/* 007c6160 TerrainTriangles (ECX query point*; stack terrain*, triangles out*, count out*, capacity, extent*; RET 0x14): the two
 * triangles of every grid cell around the query point (within +-extent), as vec4 vertices (6 per cell, 0x60 bytes) in the order
 * (bottom-left, top-left, top-right), (bottom-left, top-right, bottom-right) -- bottom-left being x low, z high: heights
 * ((deform - 0.5) * gain) * 2 + base * 2048. *count = 0 without a base grid; the count is only written when the triangles fit the
 * capacity. The terrain is locked through its deformation surface for the duration.
 * Frame (sub esp, 0xc4; push ebx / esi / edi; F = ESP): the lock rect at +0x38 (pitch, bits at +0x3c). */
#include "terrain_triangles_exact.h"
#include "terrain_math_frames.h"
#define TS32(off,value) do{if(exact)SU32(F,off,value);}while(0)
#define TS4(off,value) do{if(exact)rx_st4(F+off,value);}while(0)
/* Three real original unlock call sites have distinct structural registers. */
static void terrain_unlock_observed(rx_addr tex,rx_addr F,unsigned path,int exact){
 if(!exact){texture_unlock(tex);return;}rx_addr vt=U32(tex,0),target=U32(vt,0x38),ra;
 fx_set_reg(FX_EBX,tex);
 if(path==0){fx_set_reg(FX_EAX,vt);fx_set_reg(FX_ECX,target);ra=0x7c66c2;}
 else if(path==1){fx_set_reg(FX_ECX,vt);fx_set_reg(FX_EDX,target);ra=0x7c63e7;}
 else{fx_set_reg(FX_EDX,vt);fx_set_reg(FX_EAX,target);ra=0x7c66d9;}
 SU32(F-4,0,tex);SU32(F-8,0,ra);rx_set_sp(F-8);rx_terrain_texture_exact(tex,14,F);
}
/* Exact caller closure admits ordinary argument/output aliasing outside its
 * own scratch frame. A caller alias into private dead-frame storage is outside
 * the genuine four-wheel contract and must not silently claim parity. */
static void terrain_frame_disjoint(rx_addr p,uint64_t n,rx_addr F,rx_addr entry){
 if((uint64_t)p<entry&&(uint64_t)p+n>(uint64_t)F-0x40u)abort();
}
static void terrain_triangles_kernel(rx_addr q_p, rx_addr terrain, rx_addr out, rx_addr count_p, int32_t capacity, rx_addr extent_p, int exact) {
    const rx_addr entry_sp=rx_sp();fx_frame frame;rx_addr F=((entry_sp-4)&~15u)-0xd0;
    if(exact){
        if(U32(entry_sp,4)!=terrain||U32(entry_sp,8)!=out||U32(entry_sp,12)!=count_p||U32(entry_sp,16)!=(uint32_t)capacity||U32(entry_sp,20)!=extent_p)abort();
        terrain_frame_disjoint(terrain,0x44f0,F,entry_sp);terrain_frame_disjoint(count_p,4,F,entry_sp);
        fx_frame_enter(&frame,0xc4,16,FX_EBX,FX_ESI,FX_EDI);F=frame.E;
        SU32(F,0x34,out);SU32(F,0x24,terrain);SU32(F,0x1c,count_p);fx_set_reg(FX_EBX,terrain);fx_set_reg(FX_EAX,count_p);
    }
    SU32(count_p, 0, 0);
    const rx_addr base = U32(terrain, 0x1c);
    if(exact)fx_set_reg(FX_ESI,base);
    if (base == 0) goto done;
    if(exact){terrain_frame_disjoint(q_p,16,F,entry_sp);terrain_frame_disjoint(extent_p,16,F,entry_sp);}
    if(exact){const uint32_t c[][2]={{0x931f24,0x3f800000},{0x9322cc,0x3f000000},{0x92e330,0x40000000},{0x9774a8,0x80000000},{0x9774f0,0x4b000000}};for(unsigned i=0;i<5;i++)if(U32(c[i][0],0)!=c[i][1])abort();}
    const float one = 1.0f /* 0x931f24 */, minus_one = F32(0, RT_MINUS1), half = 0.5f /* 0x9322cc */;
    const rx_v4 q = v4_ld(q_p);
    const rx_v4 q_masked = v4(q.v[0], q.v[1], q.v[2], F32(0, RT_ZERO4 + 12));    /* w replaced by the zero vector's w */
    const int32_t stride = I32(terrain, 0x44bc);
    const rx_v4 scaled = v4_mul(v4_shuf(q_masked, q_masked, 0x88), v4(one, minus_one, one, minus_one));
    const rx_addr tex = U32(terrain, 0x240);
    if(exact){TS4(0x70,v4_ld(RT_ZERO4));TS4(0x60,q);TS32(0x6c,U32(RT_ZERO4,12));TS32(0x28,(uint32_t)stride);TS4(0x90,scaled);fx_set_reg(FX_EDX,F+0x38);fx_set_reg(FX_ECX,U32(tex,0));fx_set_reg(FX_EAX,U32(U32(tex,0),0x34));}
    rx_set_sp(F);
    if(exact){SU32(F-4,0,0x810);SU32(F-8,0,0);SU32(F-12,0,F+0x38);SU32(F-16,0,tex);SU32(F-20,0,0x7c6208);rx_set_sp(F-20);rx_terrain_texture_exact(tex,13,F);}else texture_lock(tex,F+0x38);
    const rx_addr bits = U32(F, 0x3c);
    if(exact)fx_set_reg(FX_EDI,bits);
    if (bits == 0) goto done;
    {
        const float world_x = F32(terrain, 0x44d0), world_z = F32(terrain, 0x44d4), cells_x = F32(terrain, 0x44c8), cells_z = F32(terrain, 0x44cc);
        const rx_v4 dims = v4(world_x, world_z, world_x, world_z), cells = v4(cells_x, cells_z, cells_x, cells_z);
        const rx_v4 ext = v4_ld(extent_p);
        const rx_v4 counts = v4_sub(cells, v4_ld(RT_BIAS));
        const rx_v4 neg_ext = v4_sub(v4_zero(), v4(ext.v[0], ext.v[1], ext.v[2], F32(0, RT_ZERO4 + 12)));
        const rx_v4 window = v4(neg_ext.v[0], neg_ext.v[2], ext.v[0], ext.v[2]);       /* (-ex, -ez, ex, ez) */
        rx_v4 t = v4_div(v4_mul(v4_add(v4_mul(dims, v4_splat(half)), scaled), counts), dims);
        t = v4_add(v4_add(t, window), v4(0.0f, 0.0f, one, one));
        if(exact){TS4(0x80,cells);TS4(0xa0,counts);TS4(0x60,window);TS4(0x70,ext);TS4(0x50,v4_splat(half));fx_set_reg(FX_ECX,rx_fb(ext.v[2]));fx_set_reg(FX_EAX,rx_fb(ext.v[0]));SU32(F-4,0,0x7c6318);rx_set_sp(F-4);}
        const rx_v4 fl=exact?terrain_floor_frame(t,0):phys_vec4_floor(t);if(exact)rx_set_sp(F);
        const rx_v4 zero4 = v4_ld(RT_ZERO4);
        rx_v4 lower=v4_cmplt(fl,zero4);if(exact){TS4(0x60,lower);fx_set_reg(FX_EDX,v4_lb(lower,2));fx_set_reg(FX_EAX,mask_sum4(lower));}
        if(mask_sum4(lower)==0xfffffffcu){terrain_unlock_observed(U32(terrain,0x240),F,2,exact);goto done;}
        rx_v4 upper=v4_cmplt(counts,fl);if(exact){TS4(0x60,upper);fx_set_reg(FX_ECX,v4_lb(upper,2));fx_set_reg(FX_EDX,mask_sum4(upper));}
        if(mask_sum4(upper)==0xfffffffcu){terrain_unlock_observed(U32(terrain,0x240),F,2,exact);goto done;}
        const rx_v4 cl = v4_min(v4_max(fl, zero4), v4_sub(cells, v4_splat(2.0f /* 0x92e330 */)));
        if(exact){TS4(0x80,cl);TS4(0xa0,v4_splat(2.f));SU32(F-4,0,F+0x60);SU32(F-8,0,fx_reg(FX_ECX));SU32(F-12,0,0x7c63aa);rx_set_sp(F-12);}
        const rx_v4 cell=exact?terrain_floor_frame(cl,1):phys_vec4_floor_to_int(cl);
        if(exact){rx_set_sp(F-4);SU32(F-8,0,0x7c63b2);rx_set_sp(F-8);(void)terrain_store_frame(cell,F+0x60);rx_set_sp(F);}
        const uint32_t rows = v4_lb(cell, 3) - v4_lb(cell, 1), cols = v4_lb(cell, 2) - v4_lb(cell, 0);
        const uint32_t n = rows * cols * 2;
        if(exact){TS32(0x30,cols);TS32(0x18,rows);fx_set_reg(FX_ECX,rows);fx_set_reg(FX_EDX,cols);fx_set_reg(FX_EAX,n);}
        if ((int32_t)n > capacity) { terrain_unlock_observed(U32(terrain,0x240),F,1,exact); goto done; }
        if(exact){terrain_frame_disjoint(out,(uint64_t)n*48,F,entry_sp);fx_set_reg(FX_EDX,count_p);}SU32(count_p, 0, n);
        /* constants of the vertices: half the world size (x, z), the spacing, the negated spacing */
        const float half_x = phys_mul(F32(terrain,0x44d0), half), half_z = phys_mul(F32(terrain,0x44d4), half), zero = F32(0, 0x92e12c);
        const float spacing = F32(terrain, 0x44e8), neg_spacing = phys_sub(-0.0f /* 0x9774a8 */, spacing);
        const rx_v4 half_size = v4(half_x, zero, half_z, half_z);
        const rx_v4 spacing_scale = v4(spacing, one, spacing, spacing);
        const rx_v4 offset_z = v4(zero, zero, neg_spacing, neg_spacing), offset_x = v4(spacing, zero, zero, zero);
        rx_v4 row = v4(cl.v[0], cl.v[1], cl.v[1], cl.v[1]);                        /* grid position of the first cell */
        if(exact){fx_set_reg(FX_EDX,0);TS32(0x2c,0);TS4(0x40,v4_splat(one));TS4(0xb0,half_size);TS4(0x90,spacing_scale);TS4(0xc0,offset_z);TS4(0x70,row);}
        if ((int32_t)rows <= 0) { terrain_unlock_observed(U32(terrain,0x240),F,0,exact); goto done; }
        uint32_t row_base = v4_lb(cell, 1) * (uint32_t)stride, tri = 0;
        if(exact){fx_set_reg(FX_EAX,(uint32_t)stride);fx_set_reg(FX_ECX,rows);TS32(0x14,row_base);TS32(0x1c,rows);}
        for (uint32_t r = rows; r != 0; r--) {
            if ((int32_t)cols > 0) {
                const uint32_t next = row_base + (uint32_t)stride;
                const rx_v4 signs = v4(one, one, F32(0, RT_MINUS1), F32(0, RT_MINUS1));
                const rx_v4 k2048 = v4_splat(F32(0, 0x92e22c));
                rx_addr p = out + tri * 48;
                uint32_t c = v4_lb(cell, 0);
                rx_v4 pos = row;
                if(exact){TS32(0x20,next);TS32(0x18,cols);TS4(0x80,signs);fx_set_reg(FX_ECX,c);fx_set_reg(FX_EDX,cols);}
                for (uint32_t k = cols; k != 0; k--, c++) {
                    const rx_v4 d = v4(F32(bits, 4 * (row_base + c)), F32(bits, 4 * (row_base + c + 1)), F32(bits, 4 * (next + c)), F32(bits, 4 * (next + c + 1)));
                    const rx_v4 b = v4(F32(base, 4 * (row_base + c)), F32(base, 4 * (row_base + c + 1)), F32(base, 4 * (next + c)), F32(base, 4 * (next + c + 1)));
                    const rx_v4 gain = v4_splat(F32(terrain, 0xbc));
                    /* heights of the cell's four corners */
                    const rx_v4 H = v4_add(v4_mul(v4_mul(v4_sub(d, v4_splat(half)), gain), v4_splat(2.0f)), v4_mul(b, k2048));
                    const rx_v4 v00 = v4_mul(v4_sub(v4_mul(v4(pos.v[0], H.v[0], pos.v[2], pos.v[3]), spacing_scale), half_size), signs);
                    const rx_v4 v01 = v4_add(v4(v00.v[0], H.v[1], v00.v[2], v00.v[3]), offset_x);
                    const rx_v4 v10 = v4_add(v4(v00.v[0], H.v[2], v00.v[2], v00.v[3]), offset_z);
                    const rx_v4 v11 = v4_add(v4(v10.v[0], H.v[3], v10.v[2], v10.v[3]), offset_x);
                    v4_st(p, v10); v4_st(p + 0x10, v00); v4_st(p + 0x20, v01);
                    v4_st(p + 0x30, v10); v4_st(p + 0x40, v01); v4_st(p + 0x50, v11);
                    p += 0x60; tri += 2;
                    if(exact){fx_set_reg(FX_EBX,row_base+c+1);fx_set_reg(FX_EAX,p);fx_set_reg(FX_ECX,c+1);fx_set_reg(FX_EDX,tri);TS32(0x2c,tri);TS32(0x18,k-1);}
                    pos = v4_add(pos, v4_ld(RT_COLSTEP));
                }
            }
            if(exact&&((int32_t)cols>0)){fx_set_reg(FX_EBX,terrain);fx_set_reg(FX_EAX,(uint32_t)stride);}
            row_base += (uint32_t)stride;
            row = v4_add(row, v4_ld(RT_ROWSTEP));
            if(exact){TS32(0x14,row_base);TS32(0x1c,r-1);TS4(0x70,row);}
        }
        terrain_unlock_observed(U32(terrain,0x240),F,0,exact);
    }
done:
    if(exact)fx_frame_leave(&frame);else rx_set_sp(entry_sp);
}
void phys_terrain_triangles(rx_addr q,rx_addr t,rx_addr o,rx_addr c,int32_t n,rx_addr e){terrain_triangles_kernel(q,t,o,c,n,e,0);}
void phys_terrain_triangles_exact(rx_addr q,rx_addr t,rx_addr o,rx_addr c,int32_t n,rx_addr e){terrain_triangles_kernel(q,t,o,c,n,e,1);}
#undef TS32
#undef TS4

RX_FUNCTION(0x007c6160, phys_terrain_triangles)

/* 007cda90 FUN_007cda90 (EAX list, xmm0 value): appends the value to a float list (count at +0x1e00); returns the old count (ECX). */
uint32_t phys_float_list_push(rx_addr list, uint32_t value_bits) {
    const uint32_t n = U32(list, 0x1e00);
    SU32(list, 4 * n, value_bits);
    SU32(list, 0x1e00, U32(list, 0x1e00) + 1);
    return n;
}
RX_FUNCTION(0x007cda90, phys_float_list_push)

/* 007cda50 FUN_007cda50 (EAX list, stack two floats; RET 8): appends the pair to a pair list (count at +0x200); returns the entry address (ECX). */
rx_addr phys_float_pair_list_push(rx_addr list, uint32_t a_bits, uint32_t b_bits) {
    const rx_addr at = list + U32(list, 0x200) * 8;
    SU32(at, 0, a_bits);
    SU32(at, 4, b_bits);
    SU32(list, 0x200, U32(list, 0x200) + 1);
    return at;
}
RX_FUNCTION(0x007cda50, phys_float_pair_list_push)
