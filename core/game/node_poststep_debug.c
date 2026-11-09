/* Node vf4 closure. Hand-read decompile and disassembly, 00859b30 / 00859e40 /
 * 0085b180 / 0085bed0. These are dormant visual-debug evaluations, not physics:
 * every original vector store is relative to the node callback's own ESP.
 * No scratch pointer escapes. Preserve actual calls, original frames, final
 * useful debug vectors and EAX/EDX. Pure overwritten arithmetic can collapse. */
#include "node_poststep_debug.h"
#include "world_veh_util.h"
#include "phys_mech_kernel.h"
#include "phys_crt.h"
rx_node_poststep_regs rx_node_debug_axes_regs(rx_addr node){
 world_frame f=world_frame_enter(0xb0,0,1);rx_addr S=f.esp;
 rx_addr position=U32(node,0x30),rotation=U32(node,0x34);
 rx_v4 p=v4_ld(position+0x90),q=v4_ld(rotation+0x90),scale=v4_splat(F32(0,0x009322a4));
 v4_st(S+0x50,p);v4_st(S+0x20,scale);
 const rx_addr axes[]={0x00d615c0,0x00d616a0,0x00d61680};
 for(unsigned i=0;i<3;i++){
  rx_v4 axis=v4_xyz(v4_ld(axes[i]));
  rx_v4 inner=veh_quat_mul(axis,v4_mul(q,v4_ld(0x0093a220)));
  rx_v4 point=v4_add(v4_mul(veh_quat_mul(q,inner),scale),p);
  v4_st(S+(i==0?0x60:0x40),point);
 }
 world_frame_leave(&f);return (rx_node_poststep_regs){node,rotation};
}
uint32_t rx_node_debug_axes(rx_addr n){return rx_node_debug_axes_regs(n).eax;}
RX_FUNCTION(0x00859b30,rx_node_debug_axes)
rx_node_poststep_regs rx_node_mainbody_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx){
 rx_node_poststep_regs r={eax,edx};
 if(U32(node,4))WORLD_CALL_SP(4,r=rx_node_debug_axes_regs(node));
 return r;
}
uint32_t rx_node_mainbody_poststep(rx_addr n){return rx_node_mainbody_poststep_regs(n,0,0).eax;}
RX_FUNCTION(0x00859e40,rx_node_mainbody_poststep)
rx_node_poststep_regs rx_node_rotor_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx){
 world_frame f=world_frame_enter(0xac,1,1);
 rx_node_poststep_regs r={eax,edx};
 if(U32(node,4)){
  WORLD_CALL_SP(4,r=rx_node_debug_axes_regs(node));
  /* 0085b19f..0085b58f performs only SSE debug-frame arithmetic. Every
   * store is to ESP+00..a0, all inside this callback's own frame; no calls
   * follow and no pointer to this frame escapes. The decompiler eliminates
   * this complete region. Its only live outputs are these pointer registers. */
  r.eax=U32(node,0x2c);r.edx=U32(node,0x30);
 }
 world_frame_leave(&f);return r;
}
uint32_t rx_node_rotor_poststep(rx_addr n){return rx_node_rotor_poststep_regs(n,0,0).eax;}
RX_FUNCTION(0x0085b180,rx_node_rotor_poststep)
rx_node_poststep_regs rx_node_fixed_poststep_regs(rx_addr node,uint32_t eax,uint32_t edx){
 (void)eax;
 world_frame f=world_frame_enter(0x9c,1,1);rx_addr S=f.esp;
 uint32_t cookie=U32(0,0x009b45e4)^(S+4);SU32(S,0x9c,cookie);
 rx_node_poststep_regs r={cookie,edx};
 if(U32(node,4)){
  WORLD_CALL_SP(4,r=rx_node_debug_axes_regs(node));
  rx_v4 q=v4_ld(U32(node,0x2c)+0x90),p=v4_ld(U32(node,0x28)+0x90);
  v4_st(S+0x10,v4_ld(U32(node,0x30)+0x90));
  v4_st(S+0x10,v4_add(veh_rotate(q,v4_ld(node+0x70)),p));
  SU32(S,0x50,0x0093ca04);SU32(S,0x54,3);SU32(S,0x58,3);
  WORLD_CALL_SP(4,phys_matrix3x3_from_quaternion(S+0x50,U32(node,0x34)+0x90));
  rx_v4 scale=v4_splat(F32(0,0x009d78c4));
  v4_st(S+0x20,v4_mul(scale,v4_ld(0x00d615c0)));
  WORLD_CALL_SP(4,phys_mat3_mulvec(S+0x40,S+0x20,S+0x50));
  v4_st(S+0x20,v4_mul(scale,v4_ld(0x00d616a0)));
  WORLD_CALL_SP(4,phys_mat3_mulvec(S+0x30,S+0x20,S+0x50));
  v4_st(S+0x20,v4_mul(scale,v4_ld(0x00d61680)));
  WORLD_CALL_SP(4,phys_mat3_mulvec(S+0x10,S+0x20,S+0x50));
  r.eax=S+0x50;r.edx=S+0x20;
 }
 /* Original POP ESI precedes the cookie check: the callee sees ESP S+4. */
 rx_set_sp(S+4);WORLD_CALL_SP(0,phys_security_check_cookie(U32(S,0x9c)^(S+4)));
 world_frame_leave(&f);return r;
}
uint32_t rx_node_fixed_poststep(rx_addr n){return rx_node_fixed_poststep_regs(n,0,0).eax;}
RX_FUNCTION(0x0085bed0,rx_node_fixed_poststep)
