/* Gate-less race starting rows, complete0055a7e0..0055ab57.
 * This is a functional typed port; incidental original GPR/private scratch parity
 * remains a separate original-comparison obligation. Real guest row/assignment
 * storage is used because the row helpers retain and read these exact records. */
#include "gateless_race_start.h"
#include "race_start_rows.h"
#include "build_activity_util.h"
#include "phys_vec.h"

static int32_t fill(rx_addr assignments,rx_addr previous,uint32_t first,uint32_t count,rx_addr row,uint32_t ra) {
    const rx_addr sp=rx_sp();
    SU32(sp-16,0,ra);SU32(sp-16,4,previous);SU32(sp-16,8,first);SU32(sp-16,12,count);
    rx_set_sp(sp-16);
    const int32_t n=rx_race_start_fill_row(assignments,previous,first,count,row);
    if(rx_sp()!=sp-16)act_unreached(ra,"row helper changed entered guestSP");
    rx_set_sp(sp);return n;
}
static void next(rx_addr position,rx_addr node,float distance,rx_addr row,uint32_t ra) {
    const rx_addr sp=rx_sp();SU32(sp-8,0,ra);SU32(sp-8,4,row);rx_set_sp(sp-8);
    rx_race_start_next_row(position,node,distance,row);
    if(rx_sp()!=sp-8)act_unreached(ra,"next-row helper changed entered guestSP");
    rx_set_sp(sp);
}
void rx_gateless_race_start(rx_addr A,rx_addr track,uint32_t capacity,uint32_t preview) {
    ACT_FRAME_EBP(B,16,0x1124,3);
    const rx_addr assignments=B+0x50,rows=B+0x130,position=B+0x30;
    for(uint32_t i=0;i<0xe0;i++)SU8(assignments,i,0);
    if(!preview) {
        SU32(A,0x884,1);
        const int32_t count=I32(A,0x26c);rx_addr slots=U32(A,0x268);
        for(int32_t i=0;i<count;i++,slots+=4) {
            const rx_addr slot=U32(slots,0),player=U32(slot,0x74);const int32_t index=I32(player,0x1dc);
            if(index<0)SU32(assignments,(uint32_t)index*8+4,1);
            else {SU32(assignments,(uint32_t)index*8,slot);SU32(assignments,(uint32_t)index*8+4,U32(U32(player,0x1ac),0x194));}
        }
    } else for(uint32_t i=0;i<28;i++)SU32(assignments,8*i+4,1);
    for(uint32_t i=0;i<0x1000;i++)SU8(rows,i,0);
    const rx_addr node=U32(track,0x2c);
    for(uint32_t i=0;i<3;i++)SU32(position,4*i,U32(node,4*i));
    float z=F32(node,0x28)-F32(node,0x18),x=F32(node,0x20)-F32(node,0x10),y=0.0f;
    const float zz=z*z,xx=x*x,l2=zz+xx;
    /* The first square root is the first row's width, not the normalized vector. */
    SF32(rows,0xf4,phys_sqrt(l2));
    if(l2>0.0f) {const float inv=F32(0,0x00931f24)/phys_sqrt(l2);x=inv*x;y=inv*0.0f;z=z*inv;}
    SU32(rows,0,node);
    for(uint32_t i=0;i<3;i++)SU32(rows,0x10+4*i,U32(node,0x10+4*i));
    SF32(rows,0xe0,x);SF32(rows,0xe4,y);SF32(rows,0xe8,z);SF32(rows,0xf8,0.0f);
    uint32_t total=(uint32_t)fill(assignments,0,0,capacity,rows,0x0055aa03),first=total,row_count=1;
    next(position,node,F32(rows,0xf0),rows+0x100,0x0055aa2d);
    SF32(rows+0x100,0xf8,F32(rows,0xf0));
    rx_addr distance_cell=rows+0xf8;
    while(total<16) {
        uint32_t n=16-total;if(n>=capacity)n=capacity;
        const int32_t added=fill(assignments,distance_cell-0xf8,first,n,distance_cell+8,0x0055aa7c);
        const float distance=F32(distance_cell,0xf8);
        if(added>-1) {row_count++;first+=(uint32_t)added;total+=(uint32_t)added;distance_cell+=0x100;if(total>=16)break;}
        next(position,node,distance,distance_cell+8,0x0055aabb);
        SF32(distance_cell,0x100,F32(distance_cell,0)+distance);
    }
    uint32_t index=0;
    for(uint32_t r=0;r<row_count;r++) {
        const rx_addr row=rows+0x100*r;const uint32_t count=U32(row,0xd0),source_node=U32(row,0);
        for(uint32_t i=0;i<count;i++,index++) {
            const rx_addr out=A+0x340+0x30*index,p=row+0x10+0x10*i;
            for(uint32_t k=0;k<3;k++)SU32(out,4*k,U32(p,4*k));
            SU32(out,0x24,source_node);
            for(uint32_t k=0;k<3;k++)SU32(out,0x10+4*k,U32(source_node,0x50+4*k));
        }
    }
    ACT_END(B);
}
RX_FUNCTION(0x0055a7e0,rx_gateless_race_start)
