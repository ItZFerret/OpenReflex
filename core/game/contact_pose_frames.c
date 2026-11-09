/* Exact private frames; all matrices/quaternion/pose math is single-owned by
 * canonical phys_constraint / phys_mech_kernel / phys_mech_matrix. */
#include "contact_pose_frames.h"
#include "phys_constraint.h"
#include "phys_mech.h"
#include "phys_mech_kernel.h"
#include "phys_mech_matrix.h"
#include "phys_crt.h"
#include "fx_util.h"
#include <stdlib.h>
typedef struct pose_frame{struct pose_frame*previous;rx_addr b,self;unsigned role;}pose_frame;
typedef struct matrix_frame{struct matrix_frame*previous;rx_addr b;unsigned kind,seen;}matrix_frame;
static _Thread_local pose_frame*pose;
static _Thread_local matrix_frame*matrix;
void* rx_contact_pose_bind(void*p){void*old=pose;pose=(pose_frame*)p;return old;}
void* rx_contact_matrix_bind(void*p){void*old=matrix;matrix=(matrix_frame*)p;return old;}
int rx_contact_pose_active(void){return pose!=NULL;}
rx_addr rx_contact_pose_base(void){if(!pose)abort();return pose->b;}
void rx_contact_matrix_q1_spills(float w,float z2,float xx,float wx,float y2,float wy,float wz){
 if(!matrix)return;if(matrix->kind!=0||matrix->seen++)abort();rx_addr b=matrix->b;
 SF32(b,0x2c,w);SF32(b,0x14,xx);SF32(b,0xc,z2);SF32(b,0x1c,wx);SF32(b,0x18,y2);SF32(b,0x10,wy);SF32(b,0xc,wz);
}
void rx_contact_matrix_q2_spills(float wz,float xx,float wx,float zy){
 if(!matrix)return;if(matrix->kind!=2||matrix->seen++)abort();SF32(matrix->b,0,wz);SF32(matrix->b,4,xx);SF32(matrix->b,0xc,wx);SF32(matrix->b,8,zy);
}
static void quaternion_matrix(unsigned kind,rx_addr out,rx_addr q){
 rx_addr entry=rx_sp();fx_frame f;fx_frame_enter0(&f,kind?0x10:0x30,16);matrix_frame c={matrix,f.E,kind,0};matrix=&c;
 if(kind)phys_matrix3x3_from_quaternion2(out,q);else phys_matrix3x3_from_quaternion(out,q);
 if(c.seen!=1)abort();matrix=c.previous;fx_set_reg(FX_EAX,q);fx_frame_leave(&f);if(rx_sp()!=entry)abort();
}
static void transpose(rx_addr source,rx_addr out){fx_frame f;fx_frame_enter0(&f,0,16);phys_matrix3x3_transpose(source,out);fx_set_reg(FX_EAX,out);fx_frame_leave(&f);}
static void mulvec(rx_addr out,rx_addr input,rx_addr source){fx_frame f;fx_frame_enter0(&f,0,16);phys_mat3_mulvec(out,input,source);fx_set_reg(FX_EAX,source);fx_frame_leave(&f);}
void rx_contact_pose_matrix(unsigned role,rx_addr out,rx_addr input,rx_addr source){
 if(!pose||pose->role++!=role)abort();rx_addr b=pose->b;static const uint32_t ra[]={0x85a951,0x85a95f,0x85a96f,0x85a983,0x85a997};
 if(role>=5)abort();uint32_t arg=role==1?out:role<3?input:source;
 /* Previous matrix-vector call is cdecl; its actual argument remains until
  * the source's explicit ADD ESP,4 before preparing the second call. */
 if(role==4){if(rx_sp()!=b-4)abort();rx_set_sp(b);}else if(rx_sp()!=b)abort();
 if(role==0||role==2){fx_set_reg(FX_EAX,role?fx_reg(FX_EAX):input);if(role==2)fx_set_reg(FX_EDX,input);fx_set_reg(FX_ECX,out);}
 else if(role==1)fx_set_reg(FX_ECX,input);
 else{fx_set_reg(role==3?FX_EAX:FX_ECX,source);fx_set_reg(FX_EDX,input);fx_set_reg(FX_ECX,out);}
 rx_addr caller=rx_sp();(void)fx_push(1,&arg,ra[role]);rx_addr entry=rx_sp();if(entry!=caller-8)abort();
 if(role==0)quaternion_matrix(0,out,input);else if(role==1)transpose(input,out);else if(role==2)quaternion_matrix(2,out,input);else mulvec(out,input,source);
 if(rx_sp()!=entry)abort();rx_set_sp(entry+(role<3?8:4));
}
void rx_contact_pose_relative(rx_v4 qb){if(!pose||pose->role!=5||rx_sp()!=pose->b-4)abort();rx_st4(pose->b+0x10,qb);rx_set_sp(pose->b);}
void rx_contact_pose_normalization(rx_v4 u,rx_v4 finite){
 if(!pose||rx_sp()!=pose->b)abort();rx_st4(pose->b+0x20,u);rx_st4(pose->b+0x10,finite);fx_set_reg(FX_EDX,U32(0,0xd61668));
}
rx_addr rx_contact_rotor_pose_exact(rx_addr self){
 rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=self)abort();fx_frame f;fx_frame_enter(&f,0xfc,16,FX_ESI);pose_frame c={pose,f.E,self,0};pose=&c;
 SU32(f.E,0xfc,U32(0,0x9b45e4)^(f.E+4));fx_set_reg(FX_ESI,self);
 rx_addr result=phys_rotor_project_pose(self);if(c.role!=5||rx_sp()!=f.E)abort();fx_set_reg(FX_EAX,result);
 fx_set_reg(FX_ESI,f.in.esi);rx_set_sp(f.E+4);fx_set_reg(FX_ECX,U32(f.E,0xfc)^(f.E+4));SU32(f.E,0,0x85ac00);rx_set_sp(f.E);phys_security_check_cookie(fx_reg(FX_ECX));if(rx_sp()!=f.E)abort();
 pose=c.previous;fx_frame_leave(&f);if(rx_sp()!=entry)abort();return result;
}
