/* Starting-grid row geometry. Complete semantic ports of 0055ab60/0055b060.
 * Scalar operations retain the original finite SSE arithmetic grouping. Row slots
 * are 16 bytes (only xyz written); d0=count, e0=lateral unit vector,
 * f0=next-row separation, f4=available width, f8=track progress.
 * This native composition does not claim incidental original register/dead
 * stack equivalence. Product execution uses only imported tables and core math. */
#include "race_start_rows.h"
#include "phys_vec.h"

int32_t rx_race_start_fill_row(rx_addr assignments, rx_addr previous, uint32_t first, uint32_t maximum, rx_addr row) {
    float cursor[3] = {F32(row,0x10),F32(row,0x14),F32(row,0x18)};
    float remaining=F32(row,0xf4), correction=0.0f;
    uint32_t added=0;
    int overlaps=0;
    for (int32_t i=0; i<(int32_t)maximum; ++i) {
        const rx_addr assignment=assignments+(first+(uint32_t)i)*8u;
        const uint32_t type=U32(assignment,4);
        const float width=F32(0,0x93567c+4u*type); /* 00555520 */
        const float occupied=rx_adds(width,F32(0,0x9ca294));
        const float available=rx_subs(remaining,occupied);
        if (0.0f>=available) break; /* unordered continues, as COMISS/JAE */
        const float half=rx_adds(rx_muls(width,F32(0,0x9322cc)),F32(0,0x9ca294));
        remaining=rx_subs(remaining,occupied);
        const rx_addr slot=row+0x10+16u*(uint32_t)i;
        for (unsigned k=0;k<3;++k) {
            const float p=rx_adds(rx_muls(F32(row,0xe0+4*k),half),cursor[k]);
            SF32(slot,4*k,p);
        }
        const rx_addr player=U32(assignment,0);
        if(player) SF32(player,0x140,rx_muls(F32(row,0xf8),F32(0,0x931d94)));
        const float trailing_half=rx_muls(F32(0,0x93567c+4u*type),F32(0,0x9322cc));
        for(unsigned k=0;k<3;++k)
            cursor[k]=rx_adds(rx_muls(F32(row,0xe0+4*k),trailing_half),F32(slot,4*k));
        SU32(row,0xd0,U32(row,0xd0)+1u); ++added;
        const float length=F32(0,0x93569c+4u*type); /* 00555530 */
        if(length>F32(row,0xf0)) {
            float spacing=rx_adds(length,F32(0,0x9ca2c0));
            SF32(row,0xf0,spacing);
            if(type==2u) {
                /* Original converts the rounded sum to double for this add. */
                const double extra=F64(0,0x977838);
                spacing=(float)((double)spacing+extra);
                SF32(row,0xf0,spacing);
            }
        }
        if(previous) {
            const uint32_t count=U32(previous,0xd0);
            for(uint32_t j=0;j<count;++j) {
                const rx_addr other=previous+0x10+16u*j;
                const float y=rx_subs(F32(slot,4),F32(other,4));
                const float x=rx_subs(F32(slot,0),F32(other,0));
                const float z=rx_subs(F32(slot,8),F32(other,8));
                const float squared=rx_adds(rx_adds(rx_muls(y,y),rx_muls(x,x)),rx_muls(z,z));
                const float d=rx_sqrts(squared), clearance=F32(previous,0xf0);
                if(clearance>d) {
                    float amount=rx_subs(clearance,d);
                    const float one=F32(0,0x931f24);
                    if(!(amount>one)) amount=one;
                    if(amount>correction) correction=amount;
                    overlaps=1;
                }
            }
        }
    }
    if(overlaps) { SF32(row,0xf0,correction); return -1; }
    /* The original unrolls four slots at a time, then the identical tail. */
    const int32_t count=(int32_t)U32(row,0xd0);
    const float gap=rx_divs(remaining,(float)(int32_t)(added+1u));
    for(int32_t i=0;i<count;++i) {
        const float shift=rx_muls((float)(i+1),gap);
        const rx_addr slot=row+0x10+16u*(uint32_t)i;
        const int32_t unrolled=count>=4 ? (count/4)*4 : 0;
        for(unsigned k=0;k<3;++k) {
            const float offset=rx_muls(F32(row,0xe0+4*k),shift);
            /* Preserve operand priority even for unordered imported values. */
            const int product_first=i<unrolled ?
                (k==1 && ((i&3)==1 || (i&3)==3)) || (k==0 && (i&3)==2) : k==0;
            const float old=F32(slot,4*k);
            SF32(slot,4*k,product_first ? rx_adds(offset,old) : rx_adds(old,offset));
        }
    }
    return count;
}
RX_FUNCTION(0x0055ab60,rx_race_start_fill_row)

static float row_dot(float y0,float y1,float x0,float x1,float z0,float z1) {
    return rx_adds(rx_adds(rx_muls(y0,y1),rx_muls(x0,x1)),rx_muls(z0,z1));
}
static void row_normalize(float v[3]) {
    const float length2=row_dot(v[1],v[1],v[0],v[0],v[2],v[2]);
    if(length2>F32(0,0x92e12c)) {
        const float inv=rx_divs(F32(0,0x931f24),rx_sqrts(length2));
        for(unsigned k=0;k<3;++k) v[k]=rx_muls(v[k],inv);
    }
}

void rx_race_start_next_row(rx_addr position,rx_addr node,float distance,rx_addr row) {
    const float zero=F32(0,0x9774a8);
    rx_addr behind=U32(node,0x60);
    float moved[3];
    for(unsigned k=0;k<3;++k)
        moved[k]=rx_adds(F32(position,4*k),rx_muls(rx_subs(zero,F32(behind,0x50+4*k)),distance));
    float projection=row_dot(F32(behind,0x54),rx_subs(moved[1],F32(behind,4)),
                            F32(behind,0x50),rx_subs(moved[0],F32(behind,0)),
                            F32(behind,0x58),rx_subs(moved[2],F32(behind,8)));
    while(0.0f>projection) {
        node=behind; behind=U32(node,0x60);
        /* The original tests the previous candidate against the next segment,
         * while calculating the new candidate from the unchanged input point. */
        const float dx=rx_subs(moved[0],F32(behind,0));
        const float dy=rx_subs(moved[1],F32(behind,4));
        const float dz=rx_subs(moved[2],F32(behind,8));
        for(unsigned k=0;k<3;++k) {
            const float offset=rx_muls(rx_subs(zero,F32(behind,0x50+4*k)),distance);
            moved[k]=k==0 ? rx_adds(F32(position,4*k),offset) : rx_adds(offset,F32(position,4*k));
        }
        projection=row_dot(F32(behind,0x54),dy,F32(behind,0x50),dx,F32(behind,0x58),dz);
    }
    for(unsigned k=0;k<3;++k) SF32(position,4*k,moved[k]);
    float left[3],right[3];
    for(unsigned k=0;k<3;++k) {
        left[k]=rx_subs(F32(node,0x10+4*k),F32(behind,0x10+4*k));
        right[k]=rx_subs(F32(node,0x20+4*k),F32(behind,0x20+4*k));
    }
    row_normalize(left); row_normalize(right);
    /* Edge-plane normals and the crossing direction intentionally preserve the
     * original y component; do not flatten slopes before intersecting. */
    const float cross[3]={F32(behind,0x58),F32(behind,0x54),rx_subs(zero,F32(behind,0x50))};
    const float left_normal[3]={left[2],left[1],rx_subs(zero,left[0])};
    const float right_normal[3]={right[2],right[1],rx_subs(zero,right[0])};
    const float left_constant=row_dot(F32(behind,0x14),left_normal[1],F32(behind,0x10),left_normal[0],F32(behind,0x18),left_normal[2]);
    const float left_at=row_dot(left_normal[1],moved[1],left_normal[0],moved[0],left_normal[2],moved[2]);
    const float left_den=row_dot(left_normal[1],cross[1],left_normal[0],cross[0],left_normal[2],cross[2]);
    const float left_t=rx_divs(rx_subs(left_constant,left_at),left_den);
    float first[3];
    for(unsigned k=0;k<3;++k) first[k]=rx_adds(k==0 ? rx_muls(left_t,cross[k]) : rx_muls(cross[k],left_t),moved[k]);
    const float right_constant=row_dot(F32(behind,0x24),right_normal[1],F32(behind,0x20),right_normal[0],F32(behind,0x28),right_normal[2]);
    const float right_at=row_dot(right_normal[1],moved[1],right_normal[0],moved[0],right_normal[2],moved[2]);
    const float right_den=row_dot(right_normal[1],cross[1],right_normal[0],cross[0],right_normal[2],cross[2]);
    const float right_t=rx_divs(rx_subs(right_constant,right_at),right_den);
    const float dx=rx_subs(rx_adds(rx_muls(right_t,cross[0]),moved[0]),first[0]);
    const float dz=rx_subs(rx_adds(rx_muls(cross[2],right_t),moved[2]),first[2]);
    const float length2=rx_adds(rx_adds(rx_muls(dx,dx),rx_muls(0.0f,0.0f)),rx_muls(dz,dz));
    SF32(row,0xf4,rx_sqrts(length2));
    if(length2>F32(0,0x92e12c)) {
        const float inv=rx_divs(F32(0,0x931f24),rx_sqrts(length2));
        SF32(row,0xe0,rx_muls(dx,inv));
        SF32(row,0xe4,rx_muls(inv,F32(0,0x92e12c)));
        SF32(row,0xe8,rx_muls(dz,inv));
    }
    SU32(row,0xd0,0);
    for(unsigned k=0;k<3;++k) SF32(row,0x10+4*k,first[k]);
    SU32(row,0,node);
}
RX_FUNCTION(0x0055b060,rx_race_start_next_row)
