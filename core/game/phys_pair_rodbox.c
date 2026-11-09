/* Original0086b7f0 mask0x0400/box contact: twelve box-face intersections,
 * averaged point, box clamp and one manifold record. Decompile is checked
 * against SSE instruction order, including all four quaternion lanes.
 * Face kernels use SSE arithmetic; x87 only copies arguments (quieting SNaN).
 * These functions do not run retained x87 arithmetic. */
#include "phys_pair_rodbox.h"
#include "phys_coll_quat.h"
#include "phys_coll_shape2.h"
#include "phys_crt.h"

static float x87copy(float x){uint32_t b=rx_fb(x);if((b&0x7f800000u)==0x7f800000u&&(b&0x7fffffu))b|=0x400000u;return rx_bf(b);}
/* Each face kernel solves its two roots, stores both xyz points, then applies
 * strict face-interior tests. Unused w lanes are deliberately untouched.
 * Axis0 solves z then y; axis1 solves z then x; axis2 solves y then x. */
static void face_intersections(int axis,rx_addr d,rx_addr c,float radius,float face,float la,float lb,rx_addr plus,rx_addr minus,rx_addr vp,rx_addr vm){
    const rx_addr entry=rx_sp();rx_set_sp(entry-0x14); /*sub0xc,push ebx,ebp*/
    const uint32_t other=axis?0:4, solve=axis==2?4:8, fixed=4u*(uint32_t)axis;
    const float den=F32(d,other), ds=F32(d,solve);
    const float len2=phys_add(phys_mul(den,den),phys_mul(ds,ds));
    const float oldfixed=F32(c,fixed), delta=phys_sub(face,oldfixed);
    float disc=phys_sub(phys_mul(phys_mul(len2,radius),radius),phys_mul(delta,delta));
    disc=phys_mul(phys_mul(disc,den),den);
    const float absdisc=rx_bf(rx_fb(disc)&0x7fffffffu);
    /* Float_Abs then FLD/FSTP into SqrtF's stack argument. */
    const rx_addr saved=rx_sp();rx_set_sp(saved-8);const float root=phys_sqrtf(x87copy(absdisc));rx_set_sp(saved);
    const float cross_a=F32(d,axis==0?0:4), cross_b=F32(d,8);
    const float inv=phys_div(1.0f,len2);
    float t=phys_mul(phys_sub(root,phys_mul(phys_mul(cross_a,cross_b),delta)),inv);
    const float value=phys_add(phys_mul(phys_add(phys_mul(F32(d,solve),t),phys_mul(F32(d,fixed),delta)),phys_div(-1.0f,F32(d,other))),F32(c,other));
    const float solved=phys_add(F32(c,solve),t),fixed_value=phys_add(oldfixed,delta);
    if(axis==0){SF32(plus,other,value);SF32(plus,solve,solved);SF32(plus,fixed,fixed_value);}
    else {SF32(plus,other,value);if(axis==1){SF32(plus,fixed,fixed_value);SF32(plus,solve,solved);}else{SF32(plus,solve,solved);SF32(plus,fixed,fixed_value);}}
    /* The original reloads direction/center after the first point stores. */
    const float a=F32(d,axis==0?8:4),b=F32(d,axis==0?0:8);
    t=phys_mul(phys_sub(phys_sub(-0.0f,phys_mul(phys_mul(a,b),delta)),root),inv);
    const float mvalue=phys_add(phys_mul(phys_add(phys_mul(F32(d,solve),t),phys_mul(F32(d,fixed),delta)),phys_div(-1.0f,F32(d,other))),F32(c,other));
    const float msolve=phys_add(F32(c,solve),t),mfixed=phys_add(F32(c,fixed),delta);
    SF32(minus,0,axis==0?mfixed:mvalue);
    SF32(minus,4,axis==0?mvalue:axis==1?mfixed:msolve);
    SF32(minus,8,axis==2?mfixed:msolve);
    const float nLa=phys_sub(-0.0f,la),nLb=phys_sub(-0.0f,lb);
    SF32(vp,0,(la>F32(plus,other)&&F32(plus,other)>nLa&&lb>F32(plus,solve)&&F32(plus,solve)>nLb&&disc>0.0f)?1.0f:0.0f);
    SF32(vm,0,(la>F32(minus,other)&&F32(minus,other)>nLa&&lb>F32(minus,solve)&&F32(minus,solve)>nLb&&disc>0.0f)?1.0f:0.0f);
    rx_set_sp(entry);
}
void phys_rodbox_face_x(rx_addr d,rx_addr c,float r,float f,float a,float b,rx_addr p,rx_addr m,rx_addr vp,rx_addr vm){face_intersections(0,d,c,r,f,a,b,p,m,vp,vm);}
void phys_rodbox_face_y(rx_addr d,rx_addr c,float r,float f,float a,float b,rx_addr p,rx_addr m,rx_addr vp,rx_addr vm){face_intersections(1,d,c,r,f,a,b,p,m,vp,vm);}
void phys_rodbox_face_z(rx_addr d,rx_addr c,float r,float f,float a,float b,rx_addr p,rx_addr m,rx_addr vp,rx_addr vm){face_intersections(2,d,c,r,f,a,b,p,m,vp,vm);}
RX_FUNCTION(0x0081da00,phys_rodbox_face_x)
RX_FUNCTION(0x0081dc30,phys_rodbox_face_y)
RX_FUNCTION(0x0081de70,phys_rodbox_face_z)

void phys_rodbox_face_sum(rx_addr ext,rx_addr center,rx_addr dir,float radius,rx_addr sum,rx_addr count){
    const rx_addr entry=rx_sp(),F=((entry-4)&~15u)-0x104-12;rx_set_sp(F);
    /* x/y/z extents pass through the original FLD/FSTP argument copies. */
    const float ex=x87copy(F32(ext,0)),ey=x87copy(F32(ext,4)),ez=x87copy(F32(ext,8));
    const float face[6]={ex,phys_sub(-0.0f,F32(ext,0)),ey,phys_sub(-0.0f,F32(ext,4)),ez,phys_sub(-0.0f,F32(ext,8))};
    for(int k=0;k<6;k++){
        const int axis=k/2;
        const float la=axis==0?ey:ex,lb=axis==2?ey:ez;
        const rx_addr p=F+0x50+0x20u*(uint32_t)k,m=p+0x10,vp=F+0x20+8u*(uint32_t)k,vm=vp+4;
        /* Eight original stack args and return address; their game addresses
         * matter because each callee writes these face records by address. */
        SF32(F,-0x20,x87copy(radius));SF32(F,-0x1c,x87copy(face[k]));SF32(F,-0x18,la);SF32(F,-0x14,lb);
        SU32(F,-0x10,p);SU32(F,-0xc,m);SU32(F,-8,vp);SU32(F,-4,vm);SU32(F,-0x24,0);
        rx_set_sp(F-0x24);
        face_intersections(axis,dir,center,x87copy(radius),x87copy(face[k]),la,lb,p,m,vp,vm);
        rx_set_sp(F);
    }
    /* Both six-weight batches preserve sequential stores: NaN*0 is not
     * skipped, and the old sum/count participate in the exact add order. */
    for(uint32_t batch=0;batch<2;batch++){
        const rx_addr weights=F+0x20+0x18*batch,vertices=F+0x50+0x60*batch;
        const float first_weight=F32(weights,0);
        for(uint32_t k=0;k<5;k++)for(uint32_t lane=0;lane<3;lane++)
            SF32(sum,4*lane,phys_add(F32(sum,4*lane),phys_mul(F32(vertices,0x10*k+4*lane),F32(weights,4*k))));
        /*0081ec31..38 captures count before the sixth point's xyz stores.
         * Preserve this when count aliases one of sum's three components. */
        float n=phys_add(first_weight,F32(count,0));
        for(uint32_t lane=0;lane<3;lane++)
            SF32(sum,4*lane,phys_add(F32(sum,4*lane),phys_mul(F32(vertices,0x50+4*lane),F32(weights,20))));
        for(uint32_t k=1;k<6;k++)n=phys_add(n,F32(weights,4*k));
        SF32(count,0,n);
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x0081e8e0,phys_rodbox_face_sum)

void phys_pair_0400_box(rx_addr qa,rx_addr pa,rx_addr M,rx_addr box,rx_addr qb,rx_addr pb,rx_addr radial,uint32_t flip,uint32_t* edx){
    const rx_addr entry=rx_sp(),F=((entry-4)&~15u)-0x84-12;rx_set_sp(F);
    v4_st(F+0x40,v4_ld(box+0x20));
    for(uint32_t i=0;i<3;i++)SU32(F,0x50+4*i,U32(0,0x00a95d50+4*i));
    SF32(F,0x10,0.0f);
    const rx_v4 qA=v4_ld(qa),qB=v4_ld(qb);
    rx_v4 axis=coll_quat_mul(qA,coll_quat_mul(v4_xyz(v4_ld(0x00d615c0)),v4_mul(qA,COLL_QCONJ)));
    axis=v4_xyz(axis);
    axis=coll_quat_mul(v4_mul(qB,COLL_QCONJ),coll_quat_mul(axis,qB));
    /* cmpeqps/andps/andnps/orps replaces exact zero lanes with1e-5. */
    for(int k=0;k<4;k++)if(axis.v[k]==F32(0,0x00d61670+4*(uint32_t)k))axis.v[k]=rx_bf(0x3727c5ac);
    v4_st(F+0x20,axis);
    rx_v4 center=coll_quat_mul(v4_mul(qB,COLL_QCONJ),coll_quat_mul(v4_xyz(v4_sub(v4_ld(pa),v4_ld(pb))),qB));
    v4_st(F+0x60,center);v4_st(F+0x30,center);
    const rx_v4 radius=v4_ld(radial+0x20);v4_st(F+0x70,radius);
    const uint32_t args[4]={F+0x20,rx_fb(x87copy(radius.v[0])),F+0x50,F+0x10};
    for(uint32_t i=0;i<4;i++)SU32(F,-16+4*i,args[i]);SU32(F,-20,0);
    rx_set_sp(F-20);phys_rodbox_face_sum(F+0x40,F+0x30,F+0x20,x87copy(radius.v[0]),F+0x50,F+0x10);rx_set_sp(F);
    *edx=F+0x10;
    const float count=F32(F,0x10);
    if(count>0.0f){
        for(uint32_t i=0;i<3;i++)SF32(F,0x50+4*i,phys_div(F32(F,0x50+4*i),count));
        const rx_v4 delta=v4_sub(v4_ld(F+0x50),v4_ld(F+0x60)),squares=v4_mul(delta,delta);
        const float len=phys_sqrt(phys_add(phys_add(squares.v[0],squares.v[2]),squares.v[1]));
        v4_st(F+0x60,v4_sub(v4_ld(F+0x70),v4_splat(len)));
        SU32(F,-4,F+0x70);SU32(F,-8,0);rx_set_sp(F-8);phys_box_clamp_point(box,F+0x50,F+0x70);rx_set_sp(F);
        const rx_v4 q=v4_ld(qb);
        const rx_v4 point=coll_quat_mul(q,coll_quat_mul(v4_xyz(v4_ld(F+0x50)),v4_mul(q,COLL_QCONJ)));
        const rx_v4 worldpoint=v4_add(point,v4_ld(pb));
        rx_v4 normal=coll_quat_mul(q,coll_quat_mul(v4_xyz(v4_ld(F+0x70)),v4_mul(q,COLL_QCONJ)));
        if(flip&0xff)normal=v4_sub(v4_zero(),normal);
        int32_t index=I32(M,0);if(index>=29)index=29;
        const rx_addr record=M+0x30u*(uint32_t)index;
        v4_st(record+0x10,worldpoint);v4_st(record+0x20,normal);v4_st(record+0x30,v4_ld(F+0x60));
        SU32(M,0,(uint32_t)index+1);SU32(M,0x6a0,1);*edx=M;
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x0086b7f0,phys_pair_0400_box)
