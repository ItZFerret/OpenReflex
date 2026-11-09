/* Complete structured control-flow/arithmetic draft of004d50e0, assembled from
 * original evidence, not an ATV alias. Surviving-frame audit/proof is pending;
 * no original-address adapter/registration or build has been activated. */
#include "tyre_prefix.h"
#include "phys_vec.h"
#include "fx_util.h"
#include "weather_grip.h"
/* The four products retain original MULPS operand direction. Bit n means the
 * a-lane is the left input of term n; otherwise the shuffled b vector is left.
 * Sign multiplies and the (Z+Y)+(W+X) additions are always kept separately. */
static rx_v4 product_spill(rx_v4 a,rx_v4 b,unsigned lane_left,rx_addr x_spill,rx_addr y_spill){
 rx_v4 shuffle=v4_shuf(b,b,0x1b),ax=v4_lane(a,0);
 rx_v4 x=v4_mul((lane_left&1)?ax:shuffle,(lane_left&1)?shuffle:ax);
 if(x_spill)rx_st4(x_spill,x);x=v4_mul(x,rx_ld4(0x93a1f0));shuffle=v4_shuf(shuffle,shuffle,0xb1);
 rx_v4 ay=v4_lane(a,1),y=v4_mul((lane_left&2)?ay:shuffle,(lane_left&2)?shuffle:ay);
 if(y_spill)rx_st4(y_spill,y);y=v4_mul(y,rx_ld4(0x93a200));shuffle=v4_shuf(shuffle,shuffle,0x1b);
 rx_v4 az=v4_lane(a,2),z=v4_mul((lane_left&4)?az:shuffle,(lane_left&4)?shuffle:az);
 z=v4_mul(z,rx_ld4(0x93a210));rx_v4 aw=v4_lane(a,3);
 rx_v4 w=v4_mul((lane_left&8)?aw:b,(lane_left&8)?b:aw);
 return v4_add(v4_add(z,y),(lane_left&16)?v4_add(x,w):v4_add(w,x));
}
static rx_v4 product(rx_v4 a,rx_v4 b,unsigned flags){return product_spill(a,b,flags,0,0);}
static float indexed(rx_addr b,rx_addr table,rx_addr ra,float input){
 fx_set_reg(FX_ESI,table);SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_utv_tyre_dependency(0x4390d0,b,input,0);
}
void rx_utv_tyre_airborne_phase(rx_addr w,float dt){
 /*004d5136 COMISS tau,dt/JBE: unordered keeps tau, not dt. */
 SU32(w,0xc0,1);rx_st4(w+0x80,rx_ld4(0xd61670));
 float tau=F32(w,0xe4),selected=tau;if(tau>dt)selected=dt;
 fx_set_reg(FX_EAX,w+0xe0);float factor=phys_div(selected,tau);SF32(w,0xe8,factor);
 float old=F32(w,0xe0);SF32(w,0xe0,phys_add(phys_mul(phys_sub(0,old),factor),old));
 /* Cookie/callee-save epilogue is the owner's responsibility; EDX unchanged. */
}
void rx_utv_tyre_friction_phase(rx_addr b,rx_addr s,rx_addr w){
 uint32_t body=fx_reg(FX_ESI);int front=body==1||body==2;
 rx_addr first=front?0x44fc:0x499c,second=front?0x43bc:0x485c;
 rx_addr third=front?0x45ec:0x4a8c,fourth=front?0x44ac:0x494c;
 float a=indexed(b,s+first,front?0x4d5533:0x4d54da,F32(w,0x110));
 if(g_rx_weather_grip_on)a=phys_mul(a,g_rx_weather_grip);   /* WEATHER GRIP (port, weather_grip.h): rain / snow */
 /* Original LEA of the next table precedes the retained result spill. */
 fx_set_reg(FX_ESI,s+second);rx_st4(b+0x160,v4_splat(a));
 SU32(b-4,0,front?0x4d5552:0x4d54f9);rx_set_sp(b-4);
 float c=rx_utv_tyre_dependency(0x4390d0,b,F32(w,0x110),0);
 if(g_rx_weather_grip_on)c=phys_mul(c,g_rx_weather_grip);
 fx_set_reg(FX_ESI,s+third);rx_st4(b+0xe0,v4_splat(c));
 SU32(b-4,0,front?0x4d5571:0x4d5518);rx_set_sp(b-4);
 float rolling=rx_utv_tyre_dependency(0x4390d0,b,F32(w,0x110),0);
 fx_set_reg(FX_ESI,s+fourth);SF32(b,0x14,rolling);
 SU32(b-4,0,0x4d558a);rx_set_sp(b-4);
 SF32(b,0x1c,rx_utv_tyre_dependency(0x4390d0,b,F32(w,0x110),0));
}
void rx_utv_tyre_root_point_phase(rx_addr b,rx_addr s,rx_addr w,float dt){
 rx_st4(b+0x40,v4_splat(phys_div(F32(0x931f24,0),dt)));
 fx_set_reg(FX_EAX,1);SU32(w,0xc0,1);SU32(w,0xc4,1);
 rx_v4 root=rx_ld4(s+0x730);rx_st4(b+0x300,rx_ld4(s+0x100));
 rx_st4(b+0x2f0,rx_ld4(s+0x760));rx_st4(b+0x240,v4_div(rx_ld4(0xd61650),rx_ld4(s+0x110)));
 rx_v4 point=rx_ld4(w+0x60);rx_st4(b+0xf0,point);
 rx_v4 world_arm=v4_sub(point,rx_ld4(s+0xf0));rx_st4(b+0x280,world_arm);
 rx_v4 first=product(v4_and(world_arm,rx_ld4(0x92db80)),root,11);
 rx_st4(b+0x20,v4_shuf(root,root,0x1b));rx_v4 inverse=v4_mul(root,rx_ld4(0x93a220));
 rx_v4 local_arm=product(inverse,first,11);rx_st4(b+0x320,local_arm);
 rx_st4(b+0x1c0,root);rx_v4 r1=v4_shuf(local_arm,local_arm,0xd2),r2=v4_shuf(r1,r1,0xd2);
 rx_v4 omega1=v4_shuf(rx_ld4(s+0x740),rx_ld4(s+0x740),0xc9),omega2=v4_shuf(omega1,omega1,0xc9);
 rx_v4 cross=v4_and(v4_sub(v4_mul(r1,omega1),v4_mul(r2,omega2)),rx_ld4(0x92db80));
 /* Both ANDPS occur; the second is bitwise idempotent, with no FP side effect. */
 cross=v4_and(cross,rx_ld4(0x92db80));rx_v4 spin_first=product(cross,inverse,11);
 fx_set_reg(FX_ESI,U32(b,0x18));rx_st4(b+0x90,v4_lane(root,0));
 rx_st4(b+0xa0,v4_lane(root,1));rx_st4(b+0x270,v4_lane(root,3));
 rx_v4 world_spin=product(root,spin_first,11);
 fx_set_reg(FX_EAX,fx_reg(FX_ESI)*5);rx_st4(b+0x1b0,v4_add(world_spin,rx_ld4(b+0x300)));
 /* EAX SHL5, wheel-body loads, and remaining normal/friction phases follow in
  * the active owner body; no return from original004d50e0 is claimed here. */
}
void rx_utv_tyre_publish_phase(rx_addr b,rx_addr s,rx_addr w,float dt,rx_v4 rolling,rx_v4 lateral){
 /* Four wheel stores are intentionally distinct from the force call's B+f0
  * argument. The preceding steep-surface gate may have zeroed B+f0. */
 rx_v4 total=v4_add(v4_add(rolling,rx_ld4(b+0x150)),lateral);
 SU32(b-4,0,fx_reg(FX_ESI));fx_set_reg(FX_EDX,s+0xd0);
 fx_set_reg(FX_ESI,b+0x310);fx_set_reg(FX_ECX,b+0xf0);rx_st4(w+0xd0,total);
 SU32(b-8,0,0x4d7539);rx_set_sp(b-8);(void)rx_utv_tyre_dependency(0x85c770,b,0,0);
 SU32(b-4,0,0);fx_set_reg(FX_ESI,b+0x320);fx_set_reg(FX_ECX,b+0x2d0);
 SU32(b-8,0,0x4d754e);rx_set_sp(b-8);(void)rx_utv_tyre_dependency(0x85c770,b,0,0);
 rx_v4 square=v4_mul(rx_ld4(b+0x150),rx_ld4(b+0x150));
 float magnitude=phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]));SF32(b,0x14,magnitude);
 /* Real FLD/FSTP roundtrip, then a RET4 filter call; no host-lag substitute. */
 fx_x87_copy(b-4,b+0x14);fx_set_reg(FX_EAX,w+0xe0);SU32(b-8,0,0x4d7594);rx_set_sp(b-8);
 (void)rx_utv_tyre_dependency(0x42d6b0,b,0,dt);
 /* The original loads the cookie BEFORE popping ESI/EBX. The owner performs
  * those pops, XORs with B+8, then issues security call RA004d75a4 at B+4. */
 fx_set_reg(FX_ECX,U32(b,0x33c));
}
static rx_v4 bare_x(rx_v4 a,rx_v4 b){return v4_mul(v4_lane(a,0),v4_shuf(b,b,0x1b));}
void rx_utv_tyre_wheel_point_phase(rx_addr b,rx_addr s,rx_addr w){
 /*57e9..5b33. Wheel ownership is the actual mechanism body indexed by W0;
  * no new mechanism or substituted rigid body is allocated. */
 uint32_t index=fx_reg(FX_ESI);rx_addr body=s+160u*index;fx_set_reg(FX_EAX,body);
 rx_v4 angular=rx_ld4(body+0x740),q=rx_ld4(body+0x730);
 rx_st4(b+0x210,angular);rx_st4(b+0x230,rx_ld4(body+0x100));
 rx_st4(b+0x290,rx_ld4(body+0x760));rx_st4(b+0x100,v4_div(rx_ld4(0xd61650),rx_ld4(body+0x110)));
 rx_v4 arm=v4_sub(rx_ld4(b+0xf0),rx_ld4(body+0xf0));rx_st4(b+0x1a0,arm);
 rx_st4(b+0xb0,v4_lane(rx_ld4(s+0x730),2));rx_st4(b+0x2c0,q);rx_st4(b+0x70,v4_shuf(q,q,0x1b));
 rx_v4 first=product(v4_and(arm,rx_ld4(0x92db80)),q,11),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_st4(b+0xf0,bare_x(inverse,first));rx_v4 local=product(inverse,first,11);rx_st4(b+0x310,local);
 rx_v4 r1=v4_shuf(local,local,0xd2),r2=v4_shuf(r1,r1,0xd2);
 rx_v4 o1=v4_shuf(angular,angular,0xc9),o2=v4_shuf(o1,o1,0xc9);
 rx_v4 cross=v4_and(v4_sub(v4_mul(r1,o1),v4_mul(r2,o2)),rx_ld4(0x92db80));cross=v4_and(cross,rx_ld4(0x92db80));
 rx_st4(b+0xf0,bare_x(cross,inverse));rx_v4 spin=product(cross,inverse,11);
 rx_st4(b+0x80,v4_lane(q,0));rx_st4(b+0x220,v4_lane(q,1));
 rx_st4(b+0x50,v4_lane(q,2));rx_st4(b+0x30,v4_lane(q,3));
 /*59e8/5a01/5a23/5a34 multiply with the shuffled spin on the left. */
 rx_v4 velocity=v4_add(product(q,spin,0),rx_ld4(b+0x230));
 rx_v4 rolling=rx_ld4(w+0x140),normal=rx_ld4(w+0x130),lateral=rx_ld4(w+0x120);
 rx_st4(b+0x60,rolling);float roll=phys_dot3(velocity,rolling),vertical=phys_dot3(velocity,normal),side=phys_dot3(velocity,lateral);
 rx_st4(b+0x210,lateral);rx_v4 frame_velocity=v4(roll,vertical,side,side);
 rx_st4(w+0x80,frame_velocity);rx_st4(b+0x110,frame_velocity);
 rx_st4(b+0x2d0,rx_ld4(0xd61670));rx_st4(b+0x1d0,rx_ld4(0xd61670));
 rx_st4(b+0x260,velocity);rx_st4(b+0x180,normal);
 /*5b33 starts the optional ROOT normal impulse, not a wheel-active earlyexit. */
}
static rx_v4 cross_shuffled(rx_v4 first,rx_v4 second){
 rx_v4 a=v4_shuf(first,first,0xd2),aa=v4_shuf(a,a,0xd2),b=v4_shuf(second,second,0xc9),bb=v4_shuf(b,b,0xc9);
 return v4_and(v4_sub(v4_mul(a,b),v4_mul(aa,bb)),rx_ld4(0x92db80));
}
static uint32_t first_three_mask(rx_addr p){return U32(p,8)+U32(p,4)+U32(p,0);}
void rx_utv_tyre_root_normal_phase(rx_addr b,rx_addr w){
 /*5b33..5e0c. Wheel geometry initialized B2d0 to zero. A root response exists
  * only for actual W108 and negative root contact speed along W130. */
 if(!U32(w,0x108))return;
 rx_v4 normal=rx_ld4(b+0x180),speed=v4_splat(phys_dot3(normal,rx_ld4(b+0x1b0)));
 rx_st4(b+0x120,speed);rx_v4 negative=v4_cmplt(speed,rx_ld4(0xd61670));rx_st4(b+0x140,negative);
 fx_set_reg(FX_ECX,U32(b,0x144));fx_set_reg(FX_EDX,first_three_mask(b+0x140));
 if(fx_reg(FX_EDX)!=0xfffffffdu)return;
 rx_v4 arm=rx_ld4(b+0x280),normal_zxy=v4_shuf(normal,normal,0xd2);rx_st4(b+0xf0,normal_zxy);
 rx_v4 cross=v4_and(cross_shuffled(normal,arm),rx_ld4(0x92db80));
 rx_v4 q=rx_ld4(b+0x1c0),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 first=product_spill(cross,q,11,b+0x150,b+0xf0);
 rx_v4 local=product_spill(inverse,first,11,b+0x150,b+0xf0);
 local=v4_and(v4_mul(local,rx_ld4(b+0x2f0)),rx_ld4(0x92db80));
 rx_v4 local_first=product_spill(local,inverse,11,b+0x150,b+0xf0);
 /*004d5d74 ADDPS X,W differs from the preceding W,X products. */
 rx_v4 world=product_spill(q,local_first,27,0,b+0xf0);
 rx_v4 arm_zxy=v4_shuf(arm,arm,0xd2),arm_yzx=v4_shuf(arm_zxy,arm_zxy,0xd2);
 rx_v4 response_yzx=v4_shuf(world,world,0xc9),response_zxy=v4_shuf(response_yzx,response_yzx,0xc9);
 rx_v4 response=v4_and(v4_sub(v4_mul(arm_zxy,response_yzx),v4_mul(arm_yzx,response_zxy)),rx_ld4(0x92db80));
 float rotational=phys_dot3(response,normal);
 rx_v4 negative_n=v4_sub(v4_zero(),v4_add(rx_ld4(b+0x1f0),rx_ld4(0xd61650)));
 rx_v4 impulse=v4_mul(negative_n,rx_ld4(b+0x120));impulse=v4_mul(impulse,rx_ld4(b+0x40));
 impulse=v4_div(impulse,v4_add(v4_splat(rotational),rx_ld4(b+0x240)));
 rx_st4(b+0x2d0,v4_mul(impulse,normal));
}
void rx_utv_tyre_wheel_normal_phase(rx_addr b,rx_addr w){
 /*5e14..617e. Negative speed yields a wheel impulse; the root impulse was
  * independently formed by the preceding W108 branch. */
 rx_v4 normal=rx_ld4(b+0x180),speed=v4_splat(phys_dot3(normal,rx_ld4(b+0x260)));
 rx_st4(b+0x120,speed);rx_st4(b+0x140,v4_cmplt(speed,rx_ld4(0xd61670)));
 fx_set_reg(FX_EAX,U32(b,0x144));fx_set_reg(FX_ECX,first_three_mask(b+0x140));
 rx_v4 impulse=rx_ld4(b+0x1d0);
 if(fx_reg(FX_ECX)==0xfffffffdu){
  rx_v4 arm=rx_ld4(b+0x1a0);rx_st4(b+0xf0,v4_shuf(normal,normal,0xd2));
  rx_v4 cross=v4_and(cross_shuffled(normal,arm),rx_ld4(0x92db80));
  rx_v4 q=rx_ld4(b+0x2c0),inverse=v4_mul(q,rx_ld4(0x93a220));
  rx_v4 first=product_spill(cross,q,11,b+0x150,b+0xf0);rx_st4(b+0xf0,first);
  rx_v4 local=product_spill(inverse,first,11,b+0x150,0);
  local=v4_and(v4_mul(local,rx_ld4(b+0x290)),rx_ld4(0x92db80));
  rx_v4 inner=product_spill(local,inverse,11,b+0x150,b+0xf0);
  rx_v4 world=product_spill(q,inner,0,0,b+0xf0);
  rx_v4 response=cross_shuffled(arm,world);float rotational=phys_dot3(response,normal);
  rx_v4 negative_n=v4_sub(v4_zero(),v4_add(rx_ld4(b+0x1f0),rx_ld4(0xd61650)));
  impulse=v4_mul(negative_n,rx_ld4(b+0x120));impulse=v4_mul(impulse,rx_ld4(b+0x40));
  impulse=v4_div(impulse,v4_add(v4_splat(rotational),rx_ld4(b+0x100)));
  impulse=v4_mul(impulse,normal);rx_st4(b+0x1d0,impulse);
 }
 rx_v4 total=v4_add(impulse,rx_ld4(b+0x2d0));rx_st4(b+0x150,total);
 rx_v4 squares=v4_mul(total,total);float magnitude=phys_sqrt(phys_add(phys_add(squares.v[0],squares.v[2]),squares.v[1]));
 rx_v4 lateral_mu=v4_mul(rx_ld4(w+0xb0),rx_ld4(b+0xe0));
 rx_st4(b+0xf0,v4_mul(v4_splat(magnitude),lateral_mu));
 /*6152 sums the squared total, not an un-squared normal vector. */
 rx_v4 rolling_mu=v4_mul(rx_ld4(w+0xa0),rx_ld4(b+0x160));
 rx_st4(b+0x140,v4_mul(v4_splat(magnitude),rolling_mu));
}
/* Scalar SSE2 double operations: preserve x86's first-NaN priority and explicit
 * CVTSS2SD/CVTSD2SS points in the original rational approximation. */
static double dnan(double result,double a,double c){
 if(result==result)return result;
 if(a!=a)return rx_bd(rx_db(a)|UINT64_C(0x0008000000000000));
 if(c!=c)return rx_bd(rx_db(c)|UINT64_C(0x0008000000000000));
 return rx_bd(UINT64_C(0xfff8000000000000));
}
static double dmul(double a,double c){volatile double v=c;return dnan(a*v,a,c);}
static double dsub(double a,double c){return dnan(a-c,a,c);}
static double dadd(double a,double c){return dnan(a+c,a,c);}
static double ddiv(double a,double c){return dnan(a/c,a,c);}
static double widen(float f){
 if(f==f)return (double)f;uint32_t u=rx_fb(f);
 return rx_bd(((uint64_t)(u&0x80000000u)<<32)|UINT64_C(0x7ff0000000000000)|((uint64_t)(u&0x7fffffu)<<29)|UINT64_C(0x0008000000000000));
}
static float narrow(double d){
 if(d==d){volatile float f=(float)d;return f;}uint64_t u=rx_db(d);
 return rx_bf((uint32_t)(u>>32)&0x80000000u|0x7f800000u|(uint32_t)((u>>29)&0x7fffffu)|0x400000u);
}
static double dc(rx_addr a){return rx_bd(rx_rd64(a));}
static float unary(rx_addr b,rx_addr target,rx_addr ra,float input){
 SU32(b-4,0,ra);rx_set_sp(b-4);return rx_utv_tyre_dependency(target,b,input,0);
}
void rx_utv_tyre_lean_phase(rx_addr b,rx_addr s,rx_addr w){
 /*5174..54b5. Root x-axis/up dot; this is not a heading angle or a wheel
  * orientation. Preserve all root splats before calls reuse their registers. */
 rx_v4 q=rx_ld4(s+0x730),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 first=product(v4_and(rx_ld4(0x92db80),rx_ld4(0xd615c0)),inverse,11);
 rx_st4(b+0x2b0,v4_lane(q,0));rx_st4(b+0x2a0,v4_lane(q,1));rx_st4(b+0x250,q);
 rx_st4(b+0xd0,v4_lane(q,2));rx_st4(b+0xc0,v4_lane(q,3));
 float side=phys_dot3(product(q,first,0),rx_ld4(0xd616a0));uint32_t index=U32(w,0);
 fx_set_reg(FX_ESI,index);SU32(b,0x18,index);SF32(b,0x10,side);
 float absolute=unary(b,0x401040,0x4d52a1,side);SF32(b,0x14,absolute);
 float input=absolute,threshold=F32(0x9ddfc8,0);int complement=absolute>=threshold;
 if(complement){
  input=narrow(dsub(dc(0x977638),widen(narrow(dmul(widen(side),widen(side))))));
  input=unary(b,0x401040,0x4d52de,input);SF32(b-4,0,input);SU32(b-8,0,0x4d52e9);rx_set_sp(b-8);
  input=rx_utv_tyre_dependency(0x42c230,b-4,input,0);rx_set_sp(b);
 }
 float square=narrow(dmul(widen(input),widen(input))),fourth=narrow(dmul(widen(square),widen(square)));
 double denominator_first=widen(narrow(dsub(dc(0x9777b0),dmul(widen(square),dc(0x9777a8)))));
 denominator_first=dmul(denominator_first,widen(fourth));
 double denominator_last=widen(narrow(dsub(dc(0x977638),dmul(widen(square),dc(0x9777a0)))));
 float reciprocal=narrow(ddiv(dc(0x977638),dadd(denominator_first,denominator_last)));
 double numerator_first=widen(narrow(dsub(dc(0x977798),dmul(widen(square),dc(0x977790)))));
 numerator_first=dmul(numerator_first,widen(fourth));
 double numerator_last=widen(narrow(dsub(dc(0x977638),dmul(widen(square),dc(0x977788)))));
 float numerator=narrow(dmul(dadd(numerator_first,numerator_last),widen(input)));
 float angle=narrow(dmul(widen(reciprocal),widen(numerator)));
 if(complement)angle=narrow(dsub(dc(0x977510),widen(angle)));
 /* COMISS raw,zero/JB negates for negative OR unordered raw input. */
 if(!(side>=F32(0x92e12c,0)))angle=phys_sub(F32(0x9774a8,0),angle);
 angle=unary(b,0x401000,0x4d5418,angle);
 float lean=phys_div(phys_sub(F32(0x977498,0),angle),F32(0x977498,0));
 float one_minus=phys_sub(F32(0x931f24,0),lean),blend=phys_sub(F32(0x931f24,0),phys_mul(one_minus,one_minus));
 rx_addr pair=index==1?0x82d0:index==2?0x8180:index==3?0x8570:index==4?0x8420:0;
 if(pair&&U32(s,pair))lean=phys_mul(lean,F32(0x9322cc,0));
 rx_st4(b+0x2e0,v4_splat(lean));rx_st4(b+0x170,v4_splat(blend));rx_st4(b+0x1f0,v4_splat(F32(0x92e12c,0)));
}
static rx_v4 dot_live(rx_v4 a,rx_v4 c){rx_v4 p=v4_mul(a,c);p.v[0]=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);return p;}
/* World rotational response to an axis impulse at a point. The four distinct
 * product flags are derived from this UTV instruction range, not from ATV math.
 * Final spill addresses, when present, are actual raw term stores. */
static rx_v4 rotational(rx_v4 axis,rx_v4 arm,rx_v4 q,rx_v4 inverse_inertia,unsigned inverse_flags,unsigned outer_flags,rx_addr third_x,rx_addr final_x,rx_addr final_y){
 rx_v4 cross=v4_and(cross_shuffled(axis,arm),rx_ld4(0x92db80));
 rx_v4 inverse=v4_mul(q,rx_ld4(0x93a220)),first=product(cross,q,11);
 rx_v4 local=product(inverse,first,inverse_flags);
 local=v4_and(v4_mul(local,inverse_inertia),rx_ld4(0x92db80));
 rx_v4 inner=product_spill(local,inverse,11,third_x,0);
 rx_v4 world=product_spill(q,inner,outer_flags,final_x,final_y);
 return dot_live(cross_shuffled(arm,world),axis);
}
void rx_utv_tyre_response_draft(rx_addr b,rx_addr w){
 /* The operand/frame audit for this complete arithmetic section is ongoing.
  * Do not wire an original target or claim exact dead-stack acceptance yet. */
 rx_v4 lateral=rx_ld4(b+0x210),wheel_arm=rx_ld4(b+0x1a0),wheel_q=rx_ld4(b+0x2c0);
 rx_v4 projected=dot_live(lateral,rx_ld4(b+0x260));rx_st4(b+0x120,v4_splat(projected.v[0]));
 rx_v4 axis_zxy=v4_shuf(lateral,lateral,0xd2);rx_st4(b+0x160,axis_zxy);
 rx_v4 arm_yzx=v4_shuf(wheel_arm,wheel_arm,0xc9);rx_st4(b+0x190,arm_yzx);
 rx_v4 wheel_lat=rotational(lateral,wheel_arm,wheel_q,rx_ld4(b+0x290),15,0,0,b+0xe0,b+0x160);
 rx_st4(b+0x160,v4_shuf(wheel_arm,wheel_arm,0xd2));
 rx_v4 negative_n=v4_sub(v4_zero(),v4_add(rx_ld4(b+0x1f0),rx_ld4(0xd61650)));
 rx_v4 cancellation=v4_mul(v4_splat(projected.v[0]),negative_n);
 cancellation=v4_mul(cancellation,rx_ld4(b+0x40));cancellation=v4_div(cancellation,v4_add(v4_splat(wheel_lat.v[0]),rx_ld4(b+0x100)));
 rx_st4(b+0x180,cancellation);SF32(b,0xe8,projected.v[0]);rx_st4(b+0x1a0,v4_zero());rx_st4(b+0x210,negative_n);
 rx_v4 rolling=rx_ld4(b+0x60),root_arm=rx_ld4(b+0x280),root_q=rx_ld4(b+0x1c0);
 rx_v4 root_speed=dot_live(rolling,rx_ld4(b+0x1b0));rx_st4(b+0x1e0,root_speed);
 rx_v4 root_rot=rotational(rolling,root_arm,root_q,rx_ld4(b+0x2f0),15,27,0,0,b+0x130);
 rx_st4(b+0x1b0,v4_mul(root_q,rx_ld4(0x93a220)));
 rx_st4(b+0x130,v4_shuf(root_arm,root_arm,0xd2));rx_st4(b+0x270,root_rot);
 rx_v4 wheel_speed=dot_live(rolling,rx_ld4(b+0x260));rx_st4(b+0x200,wheel_speed);
 rx_v4 wheel_rot=rotational(rolling,wheel_arm,wheel_q,rx_ld4(b+0x290),11,8,b+0x130,0,b+0x190);
 rx_v4 scale=v4_mul(rx_ld4(b+0x40),rx_ld4(b+0x2e0));
 rx_v4 wheel_roll=v4_mul(v4_mul(v4_splat(wheel_speed.v[0]),negative_n),scale);
 wheel_roll=v4_div(wheel_roll,v4_add(v4_splat(wheel_rot.v[0]),rx_ld4(b+0x100)));
 rx_v4 root_roll=v4_mul(v4_mul(v4_splat(root_speed.v[0]),negative_n),scale);
 root_roll=v4_div(root_roll,v4_add(v4_splat(root_rot.v[0]),rx_ld4(b+0x240)));
 rx_v4 impulse=v4_add(v4_mul(v4_sub(root_roll,wheel_roll),rx_ld4(b+0x170)),wheel_roll);
 SF32(b,0xe0,phys_dot3(rolling,rx_ld4(b+0x230)));rx_st4(b+0x220,rx_ld4(b+0x1f0));
 rx_v4 denominator=v4_add(rx_ld4(b+0x140),rx_ld4(0xd61650));
 rx_v4 lateral_scaled=v4_div(rx_ld4(b+0x180),denominator),rolling_scaled=v4_div(impulse,denominator);
 rx_st4(b+0x2e0,rolling_scaled);
 rx_v4 candidate=v4(rolling_scaled.v[0],lateral_scaled.v[0],F32(0xd61670,0),F32(0xd61670,0));rx_st4(b+0x240,candidate);
 rx_v4 square=v4_mul(candidate,candidate);rx_st4(b+0x190,v4_splat(phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]))));
 rx_st4(b+0x230,rx_ld4(b+0x110));rx_st4(b+0x120,impulse);
 /* Original COMISS zero,B238/JBE at6a8f is the next branch, still unjoined. */
}
static float unit_clamp(float x){if(x>=F32(0x931f24,0))x=F32(0x931f24,0);else if(x<=0)x=0;return x;}
void rx_utv_tyre_negative_slip_phase(rx_addr b,rx_addr s,rx_addr w){
 /*6a8f..6c7c. Unordered COMISS zero,side takes JBE and skips every mutation,
  * including the apparent S8708 clear. That clear must not be hoisted. */
 if(!(0>F32(b,0x238)))return;
 rx_v4 q=rx_ld4(s+0x730),velocity=v4_and(rx_ld4(s+0x100),rx_ld4(0x92db80));
 rx_v4 first=product_spill(velocity,q,11,b+0x170,b+0x110);
 rx_v4 local=product_spill(v4_mul(q,rx_ld4(0x93a220)),first,11,b+0x170,b+0x110);rx_st4(b+0x110,local);
 float maximum=F32(0x9cc37c,0),forward=unit_clamp(phys_div(F32(b,0x118),maximum));
 float backward=unit_clamp(phys_div(phys_sub(phys_sub(F32(0x9774a8,0),F32(b,0x238)),F32(0x931f24,0)),maximum));
 float factor=phys_mul(phys_mul(backward,phys_sub(F32(0x9774a8,0),F32(s,0x2c))),phys_sub(F32(0x931f24,0),forward));
 SF32(b,0x10,factor);float curve=indexed(b,s+0x4bcc,0x4d6c12,F32(w,0x110));
 factor=F32(b,0x10);SU32(s,0x8708,0);rx_st4(b+0x1f0,v4_splat(phys_mul(curve,factor)));
 float absolute=unary(b,0x401000,0x4d6c3a,factor);fx_set_reg(FX_ESI,U32(b,0x18));
 if(absolute>F32(0x931da8,0))SU32(s,0x8708,1);
}
static void cmp_mask(rx_addr destination,rx_v4 mask,int lane_register,int sum_register){
 rx_st4(destination,mask);fx_set_reg(lane_register,U32(destination,4));fx_set_reg(sum_register,first_three_mask(destination));
}
void rx_utv_tyre_rear_slip_phase(rx_addr b,rx_addr s){
 /*6c81..6f7c. Front wheels bypass this entire rear-only local-velocity/Lerp
  * path. Every sign-mask store/GPR assignment precedes its actual branch. */
 uint32_t index=fx_reg(FX_ESI);if(index!=3&&index!=4)return;
 rx_v4 q=rx_ld4(b+0x1c0),velocity=v4_and(rx_ld4(b+0x300),rx_ld4(0x92db80));
 rx_v4 first=product_spill(velocity,q,11,b+0x170,b+0x110);rx_st4(b+0x170,first);
 rx_v4 local=product_spill(v4_mul(q,rx_ld4(0x93a220)),first,15,b+0x1e0,b+0x110);
 rx_v4 scaled=v4_div(rx_ld4(b+0x180),v4_add(rx_ld4(b+0x140),rx_ld4(0xd61650)));
 rx_v4 nonnegative=v4_cmple(rx_ld4(0xd61670),v4_lane(local,0));
 cmp_mask(b+0x100,nonnegative,FX_EDX,FX_EAX);rx_st4(b+0x1e0,scaled);
 rx_v4 fallback=v4_splat(F32(0x92fe4c,0)),local_x=v4_lane(local,0);rx_st4(b+0x170,fallback);rx_st4(b+0x110,local_x);
 float control=F32(s,0x2c),t=0;rx_addr endpoint=0;
 if(fx_reg(FX_EAX)==0xfffffffdu&&0>control){
  t=phys_sub(F32(0x9774a8,0),control);endpoint=0x9322a4;
 }else{
  rx_v4 negative=v4_cmplt(local_x,rx_ld4(0xd61670));cmp_mask(b+0x100,negative,FX_ECX,FX_EDX);rx_st4(b+0x110,negative);
  if(fx_reg(FX_EDX)==0xfffffffdu&&control>0){t=control;endpoint=0x9322a4;}
  else{
   cmp_mask(b+0x100,nonnegative,FX_EAX,FX_ECX);
   if(fx_reg(FX_ECX)==0xfffffffdu&&control>0){t=control;endpoint=0x97785c;}
   else{
    cmp_mask(b+0x100,negative,FX_EDX,FX_EAX);
    if(fx_reg(FX_EAX)==0xfffffffdu&&0>control){t=phys_sub(F32(0x9774a8,0),control);endpoint=0x97785c;}
   }
  }
 }
 rx_v4 limit=fallback;
 if(endpoint){
  fx_x87_copy(b-4,endpoint);fx_x87_copy(b-8,0x92fe4c);SU32(b-12,0,0x4d6ec0);rx_set_sp(b-12);
  limit=v4_splat(rx_utv_tyre_dependency(0x430810,b-8,0,t));rx_set_sp(b);
 }
 rx_v4 lower=v4_sub(rx_ld4(b+0x1a0),limit),clamped=v4_max(lower,v4_min(rx_ld4(b+0x2e0),limit));
 rx_v4 candidate=v4(scaled.v[0],scaled.v[0],F32(0xd61670,0),F32(0xd61670,0));rx_st4(b+0x110,candidate);
 candidate.v[0]=clamped.v[0];rx_st4(b+0x240,candidate);
}
static rx_v4 normalize_original(rx_v4 vector,rx_v4 zero,rx_v4 *length,rx_v4 *finite){
 rx_v4 square=v4_mul(vector,vector),sum=v4_splat(phys_hsum3(square));*length=v4_sqrt(sum);
 *finite=v4_mask(sum.v[0]!=F32(0x92dc20,0),sum.v[1]!=F32(0x92dc20,4),sum.v[2]!=F32(0x92dc20,8),sum.v[3]!=F32(0x92dc20,12));
 rx_v4 nonzero=v4_mask(length->v[0]!=zero.v[0],length->v[1]!=zero.v[1],length->v[2]!=zero.v[2],length->v[3]!=zero.v[3]);
 return v4_or(v4_and(v4_and(nonzero,v4_div(vector,*length)),*finite),v4_andn(*finite,rx_ld4(0x92dc30)));
}
rx_utv_tyre_published_parts rx_utv_tyre_force_limits_draft(rx_addr b,rx_addr s,rx_addr w){
 /*6f7c..750d. This is a source draft pending the full surviving-frame audit.
  * Limits deliberately use per-lane x86 min/max and summed first-three masks,
  * preserving the original cases that a scalar clamp would erase. */
 rx_v4 lateral_bound=rx_ld4(b+0xf0),rolling_bound=rx_ld4(b+0x140),rolling=rx_ld4(b+0x120);
 rx_st4(b+0x1c0,lateral_bound);cmp_mask(b+0x100,v4_cmplt(rx_ld4(0xd61650),rx_ld4(b+0x190)),FX_ECX,FX_EDX);
 if(fx_reg(FX_EDX)==0xfffffffdu){
  uint32_t index=fx_reg(FX_ESI);int front=index==1||index==2;
  float speed=unary(b,0x401000,front?0x4d701e:0x4d6fe4,F32(b,0xe0));
  fx_set_reg(FX_ESI,s+(front?0x468c:0x4b2c));float coefficient=unary(b,0x438ee0,front?0x4d7029:0x4d6fef,speed);SF32(b,0x10,coefficient);
  float indexed_coefficient=indexed(b,s+(front?0x463c:0x4adc),front?0x4d7042:0x4d7008,F32(w,0x110));
  coefficient=phys_mul(F32(b,0x10),indexed_coefficient);
  rx_v4 length,finite,unit=normalize_original(rx_ld4(b+0x240),rx_ld4(b+0x1a0),&length,&finite);
  float lateral_scale=F32(b,0x1c),rolling_scale=F32(b,0x14);fx_set_reg(FX_ESI,U32(b,0x18));
  lateral_bound=v4_mul(v4_splat(lateral_scale),rx_ld4(b+0xf0));rx_st4(b+0x1c0,lateral_bound);
  rx_v4 anisotropic=v4(rolling_scale,lateral_scale,rolling_scale,rolling_scale),scaled=v4_mul(anisotropic,unit);
  rx_v4 signed_limit=v4_mul(v4_lane(scaled,0),rx_ld4(b+0x140));rolling_bound=v4_max(v4_sub(rx_ld4(b+0x1a0),signed_limit),signed_limit);
  rx_v4 extra=v4_mul(v4_lane(unit,0),v4_splat(coefficient));
  rx_v4 rider=v4_mul(v4_splat(phys_mul(F32(s,0x24),F32(0x935aa8,0))),extra);
  rx_st4(b+0x220,v4_sub(extra,rider));rolling=rx_ld4(b+0x120);
 }
 rx_v4 lateral=rx_ld4(b+0x180),mask=v4_cmplt(lateral_bound,lateral);
 cmp_mask(b+0xe0,mask,FX_EAX,FX_ECX);
 if(fx_reg(FX_ECX)==0xfffffffdu){lateral=v4_min(lateral,lateral_bound);fx_set_reg(FX_EAX,0);SU32(w,0xc0,0);}
 else{
  rx_v4 negative_bound=v4_sub(rx_ld4(b+0x1a0),lateral_bound);mask=v4_cmplt(lateral,negative_bound);
  cmp_mask(b+0xe0,mask,FX_EDX,FX_EAX);
  if(fx_reg(FX_EAX)==0xfffffffdu){lateral=v4_max(lateral,negative_bound);fx_set_reg(FX_EAX,0);SU32(w,0xc0,0);}
  else fx_set_reg(FX_EAX,0);
 }
 rx_v4 world_lateral=v4_mul(lateral,rx_ld4(w+0x120));
 mask=v4_cmplt(rolling_bound,rolling);cmp_mask(b+0xe0,mask,FX_ECX,FX_EDX);rx_st4(b+0x1e0,world_lateral);
 if(fx_reg(FX_EDX)==0xfffffffdu){
  rx_v4 extended=v4_add(rolling_bound,rx_ld4(b+0x220));mask=v4_cmplt(extended,rolling);cmp_mask(b+0xe0,mask,FX_ECX,FX_EDX);
  if(fx_reg(FX_EDX)==0xfffffffdu)rolling=extended;SU32(w,0xc4,fx_reg(FX_EAX));
 }else{
  rx_v4 negative_bound=v4_sub(rx_ld4(b+0x1a0),rolling_bound);mask=v4_cmplt(rolling,negative_bound);cmp_mask(b+0xe0,mask,FX_ECX,FX_EDX);
  if(fx_reg(FX_EDX)==0xfffffffdu){
   rx_v4 extended=v4_add(negative_bound,rx_ld4(b+0x220));mask=v4_cmplt(rolling,extended);cmp_mask(b+0xe0,mask,FX_ECX,FX_EDX);
   if(fx_reg(FX_EDX)==0xfffffffdu)rolling=extended;SU32(w,0xc4,fx_reg(FX_EAX));
  }
 }
 uint32_t index=fx_reg(FX_ESI);rolling=(index==3||index==4)?v4_add(rolling,rx_ld4(b+0x1f0)):v4_sub(rolling,rx_ld4(b+0x1f0));
 rx_v4 world_rolling=v4_mul(rx_ld4(w+0x140),rolling);rx_st4(b+0x170,world_rolling);
 rx_v4 total=v4_add(v4_add(world_rolling,rx_ld4(b+0x1d0)),world_lateral);
 rx_v4 q=rx_ld4(b+0x250),inverse=v4_mul(q,rx_ld4(0x93a220));rx_st4(b+0x110,inverse);
 rx_v4 first=product_spill(v4_and(rx_ld4(0xd616a0),rx_ld4(0x92db80)),inverse,11,0,b+0x250);
 rx_v4 reverse=v4_shuf(first,first,0x1b);reverse=v4_shuf(reverse,reverse,0xb1);rx_st4(b+0x2b0,reverse);
 rx_v4 up=product_spill(q,first,27,0,b+0x2a0);
 rx_v4 length,finite,unit=normalize_original(total,rx_ld4(b+0x1a0),&length,&finite);
 rx_st4(b+0x110,finite);rx_st4(b+0x250,length);rx_st4(b+0xf0,total);
 float dot=phys_dot3(up,unit);mask=v4_cmplt(v4_splat(dot),v4_splat(F32(0x9776ec,0)));cmp_mask(b+0x1d0,mask,FX_EAX,FX_ECX);
 rx_utv_tyre_published_parts parts={world_rolling,world_lateral};
 if(fx_reg(FX_ECX)==0xfffffffdu){rx_st4(b+0x150,rx_ld4(0xd61670));rx_st4(b+0xf0,rx_ld4(0xd61670));parts.rolling=rx_ld4(0xd61670);parts.lateral=parts.rolling;}
 return parts;
}
void rx_utv_tyre_step_draft(rx_addr w,rx_addr s,float dt){
 fx_frame frame;fx_frame_enter(&frame,0x338,16,FX_EBX,FX_ESI);rx_addr b=frame.E;
 uint32_t cookie=U32(0,0x9b45e4)^(b+8);fx_set_reg(FX_EAX,cookie);SU32(b,0x33c,cookie);fx_set_reg(FX_EBX,s);
 if(!U32(w,0x100)){
  rx_utv_tyre_airborne_phase(w,dt);fx_set_reg(FX_ESI,frame.in.esi);fx_set_reg(FX_EBX,frame.in.ebx);
  fx_set_reg(FX_ECX,U32(b,0x33c)^(b+8));SU32(b+4,0,0x4d516e);rx_set_sp(b+4);
  (void)rx_utv_tyre_dependency(0x8f9efe,b+8,0,0);fx_frame_leave(&frame);return;
 }
 rx_utv_tyre_lean_phase(b,s,w);rx_utv_tyre_friction_phase(b,s,w);rx_utv_tyre_root_point_phase(b,s,w,dt);
 rx_utv_tyre_wheel_point_phase(b,s,w);rx_utv_tyre_root_normal_phase(b,w);rx_utv_tyre_wheel_normal_phase(b,w);
 rx_utv_tyre_response_draft(b,w);rx_utv_tyre_negative_slip_phase(b,s,w);rx_utv_tyre_rear_slip_phase(b,s);
 rx_utv_tyre_published_parts parts=rx_utv_tyre_force_limits_draft(b,s,w);
 rx_utv_tyre_publish_phase(b,s,w,dt,parts.rolling,parts.lateral);
 fx_set_reg(FX_ESI,frame.in.esi);fx_set_reg(FX_EBX,frame.in.ebx);fx_set_reg(FX_ECX,fx_reg(FX_ECX)^(b+8));
 SU32(b+4,0,0x4d75a4);rx_set_sp(b+4);(void)rx_utv_tyre_dependency(0x8f9efe,b+8,0,0);fx_frame_leave(&frame);
}
