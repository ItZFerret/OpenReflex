/* Original00457070, isolated handwritten family source preparation.
 * Complete1998-instruction body source draft; unreviewed/unrun/unregistered. */
#include "truck_tyre.h"
#include "phys_vec.h"
#include "fx_util.h"
#include "weather_grip.h"
static rx_v4 product_terms(rx_v4 a,rx_v4 b,unsigned lane_left,rx_addr x_spill,rx_addr y_spill,rx_addr z_spill){
 rx_v4 shuffle=v4_shuf(b,b,0x1b),ax=v4_lane(a,0);
 rx_v4 x=v4_mul((lane_left&1)?ax:shuffle,(lane_left&1)?shuffle:ax);
 if(x_spill)rx_st4(x_spill,x);x=(lane_left&32)?v4_mul(rx_ld4(0x93a1f0),x):v4_mul(x,rx_ld4(0x93a1f0));shuffle=v4_shuf(shuffle,shuffle,0xb1);
 rx_v4 ay=v4_lane(a,1),y=v4_mul((lane_left&2)?ay:shuffle,(lane_left&2)?shuffle:ay);
 if(y_spill)rx_st4(y_spill,y);y=(lane_left&64)?v4_mul(rx_ld4(0x93a200),y):v4_mul(y,rx_ld4(0x93a200));shuffle=v4_shuf(shuffle,shuffle,0x1b);
 rx_v4 az=v4_lane(a,2),z=v4_mul((lane_left&4)?az:shuffle,(lane_left&4)?shuffle:az);
 z=v4_mul(z,rx_ld4(0x93a210));if(z_spill)rx_st4(z_spill,z);rx_v4 aw=v4_lane(a,3);
 rx_v4 w=v4_mul((lane_left&8)?aw:b,(lane_left&8)?b:aw);
 return v4_add(v4_add(z,y),(lane_left&16)?v4_add(x,w):v4_add(w,x));
}
static rx_v4 product_spill(rx_v4 a,rx_v4 b,unsigned flags,rx_addr x,rx_addr y){return product_terms(a,b,flags,x,y,0);}
static rx_v4 product(rx_v4 a,rx_v4 b,unsigned flags){return product_spill(a,b,flags,0,0);}
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

static float unary(rx_addr b,rx_addr target,rx_addr ra,float value){
 SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_truck_tyre_dependency(target,b,value,0);
}
static float indexed(rx_addr b,rx_addr table,rx_addr ra,float value){
 fx_set_reg(FX_ESI,table);return unary(b,0x4390d0,ra,value);
}
static rx_v4 cross_arm(rx_v4 a,rx_v4 c){
 rx_v4 a1=v4_shuf(a,a,0xd2),a2=v4_shuf(a1,a1,0xd2);
 rx_v4 c1=v4_shuf(c,c,0xc9),c2=v4_shuf(c1,c1,0xc9);
 return v4_and(v4_sub(v4_mul(a1,c1),v4_mul(a2,c2)),rx_ld4(0x92db80));
}
static float length021(rx_v4 x){rx_v4 z=v4_mul(x,x);return phys_sqrt(phys_add(phys_add(z.v[0],z.v[2]),z.v[1]));}
static uint32_t mask012(rx_addr p){return U32(p,8)+U32(p,4)+U32(p,0);}
void rx_truck_tyre_airborne_phase(rx_addr w,float dt){
 SU32(w,0xc0,1);rx_st4(w+0x80,rx_ld4(0xd61670));
 float tau=F32(w,0xe4),chosen=tau;if(tau>dt)chosen=dt;
 fx_set_reg(FX_EAX,w+0xe0);float f=phys_div(chosen,tau);SF32(w,0xe8,f);
 float old=F32(w,0xe0);SF32(w,0xe0,phys_add(phys_mul(phys_sub(0,old),f),old));
}
void rx_truck_tyre_lean_phase(rx_addr b,rx_addr s,rx_addr w){
 rx_v4 q=rx_ld4(s+0x730),inv=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 axis=v4_and(rx_ld4(0x92db80),rx_ld4(0xd615c0));
 float side=phys_dot3(product(q,product(axis,inv,11),15|32|64),rx_ld4(0xd616a0));
 uint32_t index=U32(w,0);fx_set_reg(FX_ESI,index);SU32(b,0x18,index);SF32(b,0x10,side);
 float absolute=unary(b,0x401040,0x457206,side);SF32(b,0x14,absolute);
 float input=absolute;int complement=absolute>=F32(0x9ddfc8,0);
 if(complement){
  input=narrow(dsub(dc(0x977638),widen(narrow(dmul(widen(side),widen(side))))));
  input=unary(b,0x401040,0x457243,input);SF32(b-4,0,input);SU32(b-8,0,0x45724e);rx_set_sp(b-8);
  input=rx_truck_tyre_dependency(0x42c230,b-4,input,0);rx_set_sp(b);
 }
 float square=narrow(dmul(widen(input),widen(input))),fourth=narrow(dmul(widen(square),widen(square)));
 double d1=widen(narrow(dsub(dc(0x9777b0),dmul(widen(square),dc(0x9777a8)))));
 d1=dmul(d1,widen(fourth));
 double d2=widen(narrow(dsub(dc(0x977638),dmul(widen(square),dc(0x9777a0)))));
 float reciprocal=narrow(ddiv(dc(0x977638),dadd(d1,d2)));
 double n1=widen(narrow(dsub(dc(0x977798),dmul(widen(square),dc(0x977790)))));
 n1=dmul(n1,widen(fourth));
 double n2=widen(narrow(dsub(dc(0x977638),dmul(widen(square),dc(0x977788)))));
 float numerator=narrow(dmul(dadd(n1,n2),widen(input)));
 float angle=narrow(dmul(widen(reciprocal),widen(numerator)));
 if(complement)angle=narrow(dsub(dc(0x977510),widen(angle)));
 if(!(side>=F32(0x92e12c,0)))angle=phys_sub(F32(0x9774a8,0),angle);
 angle=unary(b,0x401000,0x45737d,angle);
 float lean=phys_div(phys_sub(F32(0x977498,0),angle),F32(0x977498,0));
 float remain=phys_sub(F32(0x931f24,0),lean);
 float blend=phys_sub(F32(0x931f24,0),phys_mul(remain,remain));
 rx_addr pair=index==1?0x82d0:index==2?0x8180:index==3?0x8570:index==4?0x8420:0;
 if(pair&&U32(s,pair))lean=phys_mul(lean,F32(0x9322cc,0));
 rx_st4(b+0x270,v4_splat(lean));rx_st4(b+0x200,v4_splat(blend));rx_st4(b+0x160,v4_splat(F32(0x92e12c,0)));
}
void rx_truck_tyre_friction_phase(rx_addr b,rx_addr s,rx_addr w){
 uint32_t index=fx_reg(FX_ESI);int front=index==1||index==2;
 rx_addr first=front?0x44fc:0x499c,second=front?0x43bc:0x485c,third=front?0x45ec:0x4a8c,fourth=front?0x44ac:0x494c;
 float a=indexed(b,s+first,front?0x457498:0x45743f,F32(w,0x110));
 if(g_rx_weather_grip_on)a=phys_mul(a,g_rx_weather_grip);   /* WEATHER GRIP (port, weather_grip.h): rain / snow */
 fx_set_reg(FX_ESI,s+second);rx_st4(b+0x1c0,v4_splat(a));
 SU32(b-4,0,front?0x4574b7:0x45745e);rx_set_sp(b-4);float c=rx_truck_tyre_dependency(0x4390d0,b,F32(w,0x110),0);
 if(g_rx_weather_grip_on)c=phys_mul(c,g_rx_weather_grip);
 fx_set_reg(FX_ESI,s+third);rx_st4(b+0xe0,v4_splat(c));SU32(b-4,0,front?0x4574d6:0x45747d);rx_set_sp(b-4);
 float rolling=rx_truck_tyre_dependency(0x4390d0,b,F32(w,0x110),0);fx_set_reg(FX_ESI,s+fourth);SF32(b,0x14,rolling);
 SU32(b-4,0,0x4574ef);rx_set_sp(b-4);SF32(b,0x1c,rx_truck_tyre_dependency(0x4390d0,b,F32(w,0x110),0));
}
void rx_truck_tyre_root_point_phase(rx_addr b,rx_addr s,rx_addr w,float dt){
 rx_st4(b+0x190,v4_splat(phys_div(F32(0x931f24,0),dt)));fx_set_reg(FX_EAX,1);SU32(w,0xc0,1);SU32(w,0xc4,1);
 rx_v4 q=rx_ld4(s+0x730);rx_st4(b+0x210,rx_ld4(s+0x100));rx_st4(b+0x2a0,rx_ld4(s+0x760));
 rx_st4(b+0x2b0,v4_div(rx_ld4(0xd61650),rx_ld4(s+0x110)));rx_v4 point=rx_ld4(w+0x60);rx_st4(b+0x100,point);
 rx_v4 arm=v4_sub(point,rx_ld4(s+0xf0));rx_st4(b+0x250,arm);
 rx_v4 first=product(v4_and(arm,rx_ld4(0x92db80)),q,11);rx_st4(b+0x50,v4_shuf(q,q,0x1b));
 rx_v4 inverse=v4_mul(q,rx_ld4(0x93a220)),local=product(inverse,first,11);rx_st4(b+0x2c0,local);rx_st4(b+0x110,q);
 rx_v4 cross=cross_arm(local,rx_ld4(s+0x740));cross=v4_and(cross,rx_ld4(0x92db80));rx_v4 spin=product(cross,inverse,11);
 fx_set_reg(FX_ECX,U32(b,0x18));rx_st4(b+0xb0,v4_lane(q,0));rx_st4(b+0xc0,v4_lane(q,1));rx_st4(b+0x230,v4_lane(q,3));
 rx_st4(b+0x170,v4_add(product(q,spin,27),rx_ld4(b+0x210)));fx_set_reg(FX_EAX,fx_reg(FX_ECX)*5u);
}
void rx_truck_tyre_wheel_point_phase(rx_addr b,rx_addr s,rx_addr w){
 rx_addr body=s+160u*fx_reg(FX_ECX);fx_set_reg(FX_EAX,body);
 rx_v4 q=rx_ld4(body+0x730),angular=rx_ld4(body+0x740);rx_st4(b+0x240,angular);rx_st4(b+0x220,rx_ld4(body+0x100));
 rx_st4(b+0x280,rx_ld4(body+0x760));rx_st4(b+0x1f0,v4_div(rx_ld4(0xd61650),rx_ld4(body+0x110)));
 rx_v4 arm=v4_sub(rx_ld4(b+0x100),rx_ld4(body+0xf0));rx_st4(b+0x150,arm);rx_st4(b+0xa0,v4_lane(rx_ld4(b+0x110),2));
 rx_st4(b+0x80,v4_shuf(q,q,0x1b));rx_v4 first=product(v4_and(arm,rx_ld4(0x92db80)),q,11),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 local=product_spill(inverse,first,11,b+0x100,0);rx_st4(b+0x2d0,local);
 rx_v4 cross=cross_arm(local,angular);cross=v4_and(cross,rx_ld4(0x92db80));rx_v4 spin=product_spill(cross,inverse,11,b+0x100,0);
 rx_st4(b+0x90,v4_lane(q,0));rx_st4(b+0x70,v4_lane(q,1));rx_st4(b+0x30,v4_lane(q,2));rx_st4(b+0x40,v4_lane(q,3));
 rx_v4 velocity=v4_add(product(q,spin,11),rx_ld4(b+0x220)),rolling=rx_ld4(w+0x140),normal=rx_ld4(w+0x130),lateral=rx_ld4(w+0x120);
 rx_st4(b+0x60,rolling);rx_st4(b+0x20,lateral);
 float roll=phys_dot3(velocity,rolling),vertical=phys_dot3(velocity,normal),side=phys_dot3(velocity,lateral);
 rx_v4 frame=v4(roll,vertical,side,side);
 rx_st4(w+0x80,frame);rx_st4(b+0x1d0,frame);rx_st4(b+0x240,velocity);rx_st4(b+0x120,normal);
 rx_st4(b+0x1a0,rx_ld4(0xd61670));rx_st4(b+0x260,rx_ld4(0xd61670));fx_set_reg(FX_EAX,0);
}
static rx_v4 saved_wheel_q(rx_addr b){return v4(F32(b,0x90),F32(b,0x70),F32(b,0x30),F32(b,0x40));}
void rx_truck_tyre_root_normal_phase(rx_addr b,rx_addr w){
 if(!U32(w,0x108))return;
 rx_v4 normal=rx_ld4(b+0x120),speed=v4_splat(phys_dot3(normal,rx_ld4(b+0x170)));
 rx_st4(b+0x130,speed);rx_st4(b+0xf0,v4_cmplt(speed,rx_ld4(0xd61670)));fx_set_reg(FX_EDX,U32(b,0xf4));fx_set_reg(FX_ESI,mask012(b+0xf0));
 if(fx_reg(FX_ESI)!=0xfffffffdu)return;
 rx_v4 arm=rx_ld4(b+0x250);rx_st4(b+0x100,v4_shuf(normal,normal,0xd2));
 rx_v4 q=rx_ld4(b+0x110),inv=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 first=product_spill(cross_arm(normal,arm),q,11,b+0xf0,b+0x100);
 rx_v4 local=product_spill(inv,first,11,b+0xf0,b+0x100);local=v4_and(v4_mul(local,rx_ld4(b+0x2a0)),rx_ld4(0x92db80));
 rx_v4 inner=product_spill(local,inv,11,b+0xf0,b+0x100),world=product_spill(q,inner,16,0,b+0x100);
 float rotational=phys_dot3(cross_arm(arm,world),normal);
 rx_v4 impulse=v4_mul(v4_sub(v4_zero(),v4_add(rx_ld4(b+0x160),rx_ld4(0xd61650))),rx_ld4(b+0x130));
 impulse=v4_mul(impulse,rx_ld4(b+0x190));impulse=v4_div(impulse,v4_add(v4_splat(rotational),rx_ld4(b+0x2b0)));
 rx_st4(b+0x1a0,v4_mul(impulse,normal));
}
void rx_truck_tyre_wheel_normal_phase(rx_addr b,rx_addr w){
 rx_v4 normal=rx_ld4(b+0x120),speed=v4_splat(phys_dot3(normal,rx_ld4(b+0x240)));
 rx_st4(b+0x130,speed);rx_st4(b+0xf0,v4_cmplt(speed,rx_ld4(0xd61670)));fx_set_reg(FX_EDX,U32(b,0xf4));fx_set_reg(FX_ESI,mask012(b+0xf0));
 rx_v4 impulse=rx_ld4(b+0x260);
 if(fx_reg(FX_ESI)==0xfffffffdu){
  rx_v4 arm=rx_ld4(b+0x150),q=saved_wheel_q(b),inv=v4_mul(q,rx_ld4(0x93a220));rx_st4(b+0x100,v4_shuf(normal,normal,0xd2));
  rx_v4 first=product_spill(cross_arm(normal,arm),q,11,b+0xf0,b+0x100);rx_st4(b+0xf0,first);
  rx_v4 local=product_spill(inv,first,11,b+0x180,b+0x100);local=v4_and(v4_mul(local,rx_ld4(b+0x280)),rx_ld4(0x92db80));
  rx_st4(b+0xf0,inv);rx_v4 inner=product_spill(local,inv,11,b+0x180,b+0x100),world=product_spill(q,inner,11,b+0xf0,b+0x100);
  rx_st4(b+0x100,v4_shuf(arm,arm,0xd2));float rotational=phys_dot3(cross_arm(arm,world),normal);
  impulse=v4_mul(v4_sub(v4_zero(),v4_add(rx_ld4(b+0x160),rx_ld4(0xd61650))),rx_ld4(b+0x130));
  impulse=v4_mul(impulse,rx_ld4(b+0x190));impulse=v4_div(impulse,v4_add(v4_splat(rotational),rx_ld4(b+0x1f0)));
  impulse=v4_mul(impulse,normal);rx_st4(b+0x260,impulse);
 }
 rx_v4 total=v4_add(impulse,rx_ld4(b+0x1a0));rx_st4(b+0x180,total);rx_v4 square=v4_mul(total,total);rx_st4(b+0xd0,square);
 float magnitude=phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]));
 rx_st4(b+0x100,v4_mul(v4_splat(magnitude),v4_mul(rx_ld4(w+0xb0),rx_ld4(b+0xe0))));
 rx_st4(b+0x1c0,v4_mul(v4_splat(magnitude),v4_mul(rx_ld4(w+0xa0),rx_ld4(b+0x1c0))));
}
static rx_v4 dot_live(rx_v4 a,rx_v4 c){rx_v4 p=v4_mul(a,c);p.v[0]=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);return p;}
void rx_truck_tyre_response_phase(rx_addr b){
 rx_v4 lateral=rx_ld4(b+0x20),arm=rx_ld4(b+0x150),q=saved_wheel_q(b),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 inertia=rx_ld4(b+0x280),speed=dot_live(lateral,rx_ld4(b+0x240));rx_st4(b+0x130,v4_splat(speed.v[0]));
 rx_st4(b+0x1e0,v4_shuf(arm,arm,0xc9));
 rx_v4 first=product_spill(cross_arm(lateral,arm),q,11,0,b+0xe0);
 rx_v4 local=product_spill(inverse,first,11,b+0xf0,b+0xe0);local=v4_and(v4_mul(local,inertia),rx_ld4(0x92db80));
 rx_v4 inner=product_spill(local,inverse,11,b+0xf0,b+0xe0),world=product_spill(q,inner,11,b+0xf0,b+0xe0);
 rx_st4(b+0x120,v4_shuf(arm,arm,0xd2));rx_v4 rotation=dot_live(cross_arm(arm,world),lateral);
 rx_v4 negative=v4_sub(v4_zero(),v4_add(rx_ld4(b+0x160),rx_ld4(0xd61650)));
 rx_st4(b+0xf0,v4_zero());rx_st4(b+0x20,negative);
 rx_v4 cancellation=v4_mul(v4_splat(speed.v[0]),negative);cancellation=v4_mul(cancellation,rx_ld4(b+0x190));
 cancellation=v4_div(cancellation,v4_add(v4_splat(rotation.v[0]),rx_ld4(b+0x1f0)));rx_st4(b+0x150,cancellation);SF32(b,0xe8,speed.v[0]);
 rx_v4 rolling=rx_ld4(b+0x60),root_arm=rx_ld4(b+0x250),root_q=rx_ld4(b+0x110),root_inverse=v4_mul(root_q,rx_ld4(0x93a220));
 rx_v4 root_speed=dot_live(rolling,rx_ld4(b+0x170));rx_st4(b+0x290,root_speed);rx_st4(b+0x170,v4_shuf(rolling,rolling,0xd2));
 first=product_spill(cross_arm(rolling,root_arm),root_q,11,b+0x130,b+0x170);rx_st4(b+0x1b0,first);
 local=product_spill(root_inverse,first,15,b+0x140,b+0x130);rx_st4(b+0x170,root_inverse);
 local=v4_and(v4_mul(local,rx_ld4(b+0x2a0)),rx_ld4(0x92db80));
 inner=product_spill(local,root_inverse,11,b+0x1b0,b+0x140);world=product_spill(root_q,inner,27,b+0x1b0,b+0x140);
 rx_st4(b+0x140,v4_shuf(root_arm,root_arm,0xd2));rx_v4 root_rot=dot_live(cross_arm(root_arm,world),rolling);rx_st4(b+0x230,root_rot);
 rx_v4 wheel_speed=dot_live(rolling,rx_ld4(b+0x240));rx_st4(b+0x1b0,wheel_speed);rx_st4(b+0x140,v4_shuf(rolling,rolling,0xd2));
 first=product_spill(cross_arm(rolling,arm),q,11,b+0x140,b+0x1e0);
 local=product_spill(inverse,first,15,b+0x140,b+0x1e0);local=v4_and(v4_mul(local,inertia),rx_ld4(0x92db80));
 rx_st4(b+0x1e0,inverse);inner=product(local,inverse,11);world=product(q,inner,27);
 rx_v4 wheel_rot=dot_live(cross_arm(arm,world),rolling),scale=v4_mul(rx_ld4(b+0x190),rx_ld4(b+0x270));
 rx_v4 wheel_roll=v4_mul(v4_mul(v4_splat(wheel_speed.v[0]),negative),scale);
 wheel_roll=v4_div(wheel_roll,v4_add(v4_splat(wheel_rot.v[0]),rx_ld4(b+0x1f0)));
 rx_v4 root_roll=v4_mul(v4_mul(v4_splat(root_speed.v[0]),negative),scale);
 root_roll=v4_div(root_roll,v4_add(v4_splat(root_rot.v[0]),rx_ld4(b+0x2b0)));
 rx_v4 impulse=v4_add(v4_mul(v4_sub(root_roll,wheel_roll),rx_ld4(b+0x200)),wheel_roll);
 SF32(b,0xe0,phys_dot3(rolling,rx_ld4(b+0x220)));
 rx_v4 denominator=v4_add(rx_ld4(b+0x1c0),rx_ld4(0xd61650));
 rx_v4 lateral_scaled=v4_div(rx_ld4(b+0x150),denominator),rolling_scaled=v4_div(impulse,denominator);
 rx_v4 candidate=v4(rolling_scaled.v[0],lateral_scaled.v[0],F32(0xd61670,0),F32(0xd61670,0));
 rx_st4(b+0x220,candidate);rx_st4(b+0x290,rolling_scaled);rx_st4(b+0x270,v4_splat(length021(candidate)));
 rx_st4(b+0x120,impulse);rx_st4(b+0x1f0,rx_ld4(b+0x160));
}
static float unit_clamp(float x){if(x>=F32(0x931f24,0))x=F32(0x931f24,0);else if(x<=0)x=0;return x;}
static void cmp_mask(rx_addr p,rx_v4 value,int lane,int sum){rx_st4(p,value);fx_set_reg(lane,U32(p,4));fx_set_reg(sum,mask012(p));}
void rx_truck_tyre_rear_slip_phase(rx_addr b,rx_addr s,rx_addr w){
 uint32_t index=fx_reg(FX_ECX);float slip=F32(b,0x1d8);if(!(0>slip)||(index!=3&&index!=4))return;
 rx_v4 q=rx_ld4(s+0x730),velocity=v4_and(rx_ld4(s+0x100),rx_ld4(0x92db80));
 rx_v4 first=product_spill(velocity,q,11,b+0x200,b+0x1d0);
 rx_v4 local=product_spill(v4_mul(q,rx_ld4(0x93a220)),first,11,b+0x200,b+0x1d0);rx_st4(b+0x200,local);
 float maximum=F32(0x9cc37c,0),forward=unit_clamp(phys_div(F32(b,0x208),maximum));
 float backward=unit_clamp(phys_div(phys_sub(phys_sub(F32(0x9774a8,0),slip),F32(0x931f24,0)),maximum));
 float factor=phys_mul(phys_mul(backward,phys_sub(F32(0x9774a8,0),F32(s,0x2c))),phys_sub(F32(0x931f24,0),forward));
 SF32(b,0x10,factor);float curve=indexed(b,s+0x4bcc,0x458bc1,F32(w,0x110));factor=F32(b,0x10);SU32(s,0x8708,0);
 rx_st4(b+0x160,v4_splat(phys_mul(curve,factor)));float absolute=unary(b,0x401000,0x458be9,factor);if(absolute>F32(0x931da8,0))SU32(s,0x8708,1);
 q=rx_ld4(b+0x110);velocity=v4_and(rx_ld4(b+0x210),rx_ld4(0x92db80));first=product(velocity,q,11);
 local=product(v4_mul(q,rx_ld4(0x93a220)),first,11);
 rx_v4 scaled=v4_div(rx_ld4(b+0x150),v4_add(rx_ld4(b+0x1c0),rx_ld4(0xd61650)));
 rx_v4 local_x=v4_lane(local,0),zero=rx_ld4(0xd61670),nonnegative=v4_cmple(zero,local_x),negative=v4_cmplt(local_x,zero);
 cmp_mask(b+0x110,nonnegative,FX_EAX,FX_ECX);rx_st4(b+0x190,scaled);
 float control=F32(s,0x2c),t=0;rx_addr endpoint=0;
 if(fx_reg(FX_ECX)==0xfffffffdu&&0>control){t=phys_sub(F32(0x9774a8,0),control);endpoint=0x9322a4;}
 else{cmp_mask(b+0x110,negative,FX_EDX,FX_EAX);
  if(fx_reg(FX_EAX)==0xfffffffdu&&control>0){t=control;endpoint=0x9322a4;}
  else{cmp_mask(b+0x110,nonnegative,FX_ECX,FX_EDX);
   if(fx_reg(FX_EDX)==0xfffffffdu&&control>0){t=control;endpoint=0x9d789c;}
   else{cmp_mask(b+0x110,negative,FX_EAX,FX_ECX);
    if(fx_reg(FX_ECX)==0xfffffffdu&&0>control){t=phys_sub(F32(0x9774a8,0),control);endpoint=0x9d789c;}}}}
 rx_v4 limit=v4_splat(F32(0x92e330,0));
 if(endpoint){fx_x87_copy(b-4,endpoint);fx_x87_copy(b-8,0x92e330);SU32(b-12,0,0x458e13);rx_set_sp(b-12);
  limit=v4_splat(rx_truck_tyre_dependency(0x430810,b-8,0,t));rx_set_sp(b);}
 rx_v4 clamped=v4_max(v4_sub(rx_ld4(b+0xf0),limit),v4_min(rx_ld4(b+0x290),limit));
 rx_st4(b+0x220,v4(clamped.v[0],scaled.v[0],F32(0xd61670,0),F32(0xd61670,0)));fx_set_reg(FX_ECX,U32(b,0x18));fx_set_reg(FX_EAX,0);
}
static rx_v4 normalize_original(rx_v4 v,rx_v4 zero){
 rx_v4 square=v4_mul(v,v),sum=v4_splat(phys_hsum3(square)),len=v4_sqrt(sum);
 rx_v4 finite=v4_mask(sum.v[0]!=F32(0x92dc20,0),sum.v[1]!=F32(0x92dc20,4),sum.v[2]!=F32(0x92dc20,8),sum.v[3]!=F32(0x92dc20,12));
 rx_v4 nonzero=v4_mask(len.v[0]!=zero.v[0],len.v[1]!=zero.v[1],len.v[2]!=zero.v[2],len.v[3]!=zero.v[3]);
 return v4_or(v4_and(v4_and(nonzero,v4_div(v,len)),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
rx_truck_tyre_parts rx_truck_tyre_limits_phase(rx_addr b,rx_addr s,rx_addr w){
 rx_v4 zero=rx_ld4(b+0xf0),rolling_bound=rx_ld4(b+0x1c0),lateral_bound=rx_ld4(b+0x100),extra=rx_ld4(b+0x1f0),rolling=rx_ld4(b+0x120);
 cmp_mask(b+0x110,v4_cmplt(rx_ld4(0xd61650),rx_ld4(b+0x270)),FX_EDX,FX_ESI);
 rx_st4(b+0x210,rolling_bound);rx_st4(b+0x130,lateral_bound);
 if(fx_reg(FX_ESI)==0xfffffffdu){
  uint32_t index=fx_reg(FX_ECX);int front=index==1||index==2;
  float speed=unary(b,0x401000,front?0x458f1f:0x458ee5,F32(b,0xe0));fx_set_reg(FX_ESI,s+(front?0x468c:0x4b2c));
  float c=unary(b,0x438ee0,front?0x458f2a:0x458ef0,speed);SF32(b,0x10,c);
  float c2=indexed(b,s+(front?0x463c:0x4adc),front?0x458f43:0x458f09,F32(w,0x110));c=phys_mul(F32(b,0x10),c2);
  rx_v4 unit=normalize_original(rx_ld4(b+0x220),zero);float lat=F32(b,0x1c),roll=F32(b,0x14);
  lateral_bound=v4_mul(v4_splat(lat),rx_ld4(b+0x100));rx_st4(b+0x130,lateral_bound);
  rx_v4 scaled=v4_mul(v4(roll,lat,roll,roll),unit),signed_limit=v4_mul(v4_lane(scaled,0),rx_ld4(b+0x1c0));
  rolling_bound=v4_max(v4_sub(zero,signed_limit),signed_limit);rx_st4(b+0x210,rolling_bound);
  extra=v4_mul(v4_lane(unit,0),v4_splat(c));extra=v4_sub(extra,v4_mul(v4_splat(phys_mul(F32(s,0x24),F32(0x935aa8,0))),extra));
  rx_st4(b+0x1f0,extra);fx_set_reg(FX_ECX,U32(b,0x18));fx_set_reg(FX_EAX,0);
 }
 rx_v4 lateral=rx_ld4(b+0x150);cmp_mask(b+0xe0,v4_cmplt(lateral_bound,lateral),FX_EDX,FX_ESI);
 if(fx_reg(FX_ESI)==0xfffffffdu){lateral=v4_min(lateral,lateral_bound);SU32(w,0xc0,fx_reg(FX_EAX));rx_st4(b+0x150,lateral);}
 else{rx_v4 lower=v4_sub(zero,lateral_bound);cmp_mask(b+0xe0,v4_cmplt(lateral,lower),FX_EDX,FX_ESI);
  if(fx_reg(FX_ESI)==0xfffffffdu){lateral=v4_max(lateral,lower);SU32(w,0xc0,fx_reg(FX_EAX));rx_st4(b+0x150,lateral);}}
 rx_v4 world_lateral=v4_mul(lateral,rx_ld4(w+0x120));cmp_mask(b+0xe0,v4_cmplt(rolling_bound,rolling),FX_EDX,FX_ESI);
 if(fx_reg(FX_ESI)==0xfffffffdu){rx_v4 extended=v4_add(rolling_bound,extra);cmp_mask(b+0xe0,v4_cmplt(extended,rolling),FX_EDX,FX_ESI);
  if(fx_reg(FX_ESI)==0xfffffffdu)rolling=extended;SU32(w,0xc4,fx_reg(FX_EAX));}
 else{rx_v4 lower=v4_sub(zero,rolling_bound);cmp_mask(b+0xe0,v4_cmplt(rolling,lower),FX_EDX,FX_ESI);
  if(fx_reg(FX_ESI)==0xfffffffdu){rx_v4 extended=v4_add(lower,extra);cmp_mask(b+0xe0,v4_cmplt(rolling,extended),FX_EDX,FX_ESI);
   if(fx_reg(FX_ESI)==0xfffffffdu)rolling=extended;SU32(w,0xc4,fx_reg(FX_EAX));}}
 rolling=v4_add(rolling,rx_ld4(b+0x160));rx_st4(b+0x120,rolling);
 rx_truck_tyre_parts result={v4_mul(rx_ld4(w+0x140),rolling),world_lateral};return result;
}
static void publish_debug(rx_addr b,rx_addr s,uint32_t index){
 rx_v4 zero=rx_ld4(b+0xf0),extra=rx_ld4(b+0x1f0);
 rx_v4 envelope=v4_add(v4_max(v4_sub(zero,extra),extra),rx_ld4(b+0x210));
 rx_v4 first=v4_movss(v4_shuf(rx_ld4(b+0x120),envelope,0),v4_sub(zero,envelope));
 rx_v4 lat_bound=rx_ld4(b+0x130),second=v4_movss(v4_shuf(rx_ld4(b+0x150),lat_bound,0),v4_sub(zero,lat_bound));
 rx_v4 third=v4_movss(v4_shuf(rx_ld4(b+0x1a0),rx_ld4(b+0x180),0),rx_ld4(b+0x160));
 rx_addr a,c,d;if(index==3){a=0x8750;c=0x8790;d=0x87d0;}else if(index==4){a=0x8770;c=0x87b0;d=0x87f0;}
 else if(index==1){a=0x8760;c=0x87a0;d=0x87e0;}else{a=0x8780;c=0x87c0;d=0x8800;}
 rx_st4(s+a,first);rx_st4(s+c,second);rx_st4(s+d,third);
}
static uint32_t publish(rx_addr b,rx_addr s,rx_addr w,float dt,rx_truck_tyre_parts p){
 rx_v4 total=v4_add(v4_add(p.rolling,rx_ld4(b+0x180)),p.lateral),force=v4_add(v4_add(p.rolling,rx_ld4(b+0x260)),p.lateral);
 SU32(b-4,0,fx_reg(FX_ECX));rx_st4(w+0xd0,total);fx_set_reg(FX_EDX,s+0xd0);fx_set_reg(FX_ESI,b+0x2d0);fx_set_reg(FX_ECX,b+0x160);rx_st4(b+0x160,force);
 SU32(b-8,0,0x4591ea);rx_set_sp(b-8);(void)rx_truck_tyre_dependency(0x85c770,b,0,0);
 SU32(b-4,0,0);fx_set_reg(FX_ESI,b+0x2c0);fx_set_reg(FX_ECX,b+0x1a0);SU32(b-8,0,0x4591ff);rx_set_sp(b-8);(void)rx_truck_tyre_dependency(0x85c770,b,0,0);
 rx_v4 square=rx_ld4(b+0xd0);SF32(b,0x14,phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1])));
 fx_x87_copy(b-4,b+0x14);fx_set_reg(FX_EAX,w+0xe0);SU32(b-8,0,0x459242);rx_set_sp(b-8);(void)rx_truck_tyre_dependency(0x42d6b0,b,0,dt);
 uint32_t index=U32(b,0x18);fx_set_reg(FX_EAX,index);
 if(index!=1&&index!=3&&index!=4)fx_set_reg(FX_ECX,U32(b,0x2ec));
 publish_debug(b,s,index);return index;
}
void rx_truck_tyre_step_exact(rx_addr w,rx_addr s,float dt){
 fx_frame f;fx_frame_enter(&f,0x2e8,16,FX_EBX,FX_ESI);rx_addr b=f.E;
 uint32_t cookie=U32(0,0x9b45e4)^(b+8);fx_set_reg(FX_EAX,cookie);SU32(b,0x2ec,cookie);fx_set_reg(FX_EBX,s);
 if(!U32(w,0x100)){
  rx_truck_tyre_airborne_phase(w,dt);fx_set_reg(FX_ESI,f.in.esi);fx_set_reg(FX_EBX,f.in.ebx);
  fx_set_reg(FX_ECX,U32(b,0x2ec)^(b+8));SU32(b+4,0,0x4570fe);rx_set_sp(b+4);(void)rx_truck_tyre_dependency(0x8f9efe,b+8,0,0);fx_frame_leave(&f);return;
 }
 rx_truck_tyre_lean_phase(b,s,w);rx_truck_tyre_friction_phase(b,s,w);rx_truck_tyre_root_point_phase(b,s,w,dt);
 rx_truck_tyre_wheel_point_phase(b,s,w);rx_truck_tyre_root_normal_phase(b,w);rx_truck_tyre_wheel_normal_phase(b,w);
 rx_truck_tyre_response_phase(b);rx_truck_tyre_rear_slip_phase(b,s,w);rx_truck_tyre_parts p=rx_truck_tyre_limits_phase(b,s,w);uint32_t index=publish(b,s,w,dt,p);
 /* Three branches load cookie after both pops; default loads before either.
  * All paths use the same actual B2ec cookie and original XOR base B+8. */
 fx_set_reg(FX_ESI,f.in.esi);fx_set_reg(FX_EBX,f.in.ebx);
 if(index==1||index==3||index==4)fx_set_reg(FX_ECX,U32(b,0x2ec));
 fx_set_reg(FX_ECX,fx_reg(FX_ECX)^(b+8));uint32_t ra=index==3?0x4592df:index==4?0x45933f:index==1?0x45939f:0x4593fa;
 SU32(b+4,0,ra);rx_set_sp(b+4);(void)rx_truck_tyre_dependency(0x8f9efe,b+8,0,0);fx_frame_leave(&f);
}
