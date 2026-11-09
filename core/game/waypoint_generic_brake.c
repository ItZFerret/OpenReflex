/* Original GenericBrake control policy, hand translated from complete spans.
 * The common geometry/ray owners are reused through their authoritative header.
 * SSE arithmetic uses x86 helpers; x87 float loads/stores quiet signaling NaNs.
 * This is source-only until the separate native/original fixtures are run. */
#include "waypoint_generic_brake.h"
#include "build_activity_util.h"
#include "race.h"
#include "phys_crt.h"

static rx_addr brake_vehicle(rx_addr driver) { return U32(U32(driver, 0x74), 0x1ac); }
static float brake_x87_copy(float x) { return rx_f80_to_f32(rx_f80_from_f32(x), 0); }
/* SqrtF receives an SSE-written stack argument. This source helper adds no
 * extra original frame; it preserves the caller's single cdecl argument slot. */
static float brake_sqrt(float square) {
    SF32(rx_sp()-4, 0, square);
    float result; ACT_CALLR(result, 1, phys_sqrtf(square)); return result;
}

/*00530ff0 ECX=behavior, stackdt (unused), RET4. Probe center/left/right rays,
 * then accumulate braking against the other actual activity drivers. */
uint32_t rx_generic_brake_update(rx_addr behavior, float dt) {
    (void)dt;
    ACT_FRAME_EBP(f, 16, 4, 3);
    if (U32(behavior, 4)) {
        ACT_CALL(2, rx_race_probe_rays(U32(behavior, 8), behavior+0x10, behavior+0x30, behavior+0x50));
        ACT_CALL(0, rx_generic_brake_scan(behavior));
    }
    ACT_END(f); return 0;
}
RX_FUNCTION(0x00530ff0, rx_generic_brake_update)

/*00531030 EAX=behavior. A NaN vertical separation is NOT rejected by JAE;
 * a NaN forward coordinate is likewise not behind. Do not turn those original
 * comparisons into a positive finite-only inclusion test. */
void rx_generic_brake_scan(rx_addr behavior) {
    ACT_FRAME_EBP(f, 16, 0x24, 3);
    rx_addr activity=U32(U32(U32(behavior,8),0x74),0x94);
    SU32(f,0x1c,U32(activity,0x26c));
    for (int32_t i=0; i<I32(f,0x1c); ++i) {
        rx_addr self=U32(behavior,8), player=U32(self,0x74);
        rx_addr other=U32(U32(U32(player,0x94),0x268),4u*(uint32_t)i);
        if (other==self) continue;
        rx_addr ov=brake_vehicle(other);
        if (!ov) continue;
        rx_addr own=U32(player,0x1ac);
        float vertical=rx_subs(F32(own,0xc4),F32(ov,0xc4));
        vertical=rx_bf(rx_fb(vertical)&U32(0,0x00977570));
        if (vertical>=F32(0,0x0092fe4c)) continue;
        own=brake_vehicle(U32(behavior,8));
        for (uint32_t k=0;k<3;++k) SF32(f,0x20+4*k,rx_subs(F32(ov,0xc0+4*k),F32(own,0xc0+4*k)));
        ACT_CALL(0,rx_race_to_vehicle_frame(own,f+0x20));
        if (0.0f>F32(f,0x28)) continue;
        float width=brake_x87_copy(F32(0,0x0093582c+4u*U32(brake_vehicle(other),0x194)));
        SF32(rx_sp()-4,0,width);
        float time; ACT_CALLR(time,1,rx_generic_brake_contact_time(behavior,other,width));
        SU32(f,0x18,0);
        float amount;
        if (time!=0.0f) amount=rx_subs(F32(0,0x00931f24),rx_muls(time,F32(0,0x009776dc)));
        else {
            ACT_CALLR(time,1,rx_generic_brake_crossing_time(other,behavior));
            amount=time!=F32(0,0x0092e12c) ? rx_subs(F32(0,0x00931f24),rx_muls(time,F32(0,0x00977994))) : F32(f,0x18);
        }
        self=U32(behavior,8);
        amount=rx_muls(amount,F32(0,0x009d789c));
        SF32(self,0x64,rx_adds(amount,F32(self,0x64)));
    }
    ACT_END(f);
}
RX_FUNCTION(0x00531030, rx_generic_brake_scan)

/*005311d0 EDI=behavior, EAX=other, stackmax_move, RET4 -> XMM0.
 * Contact on either side ray must be less than .75 seconds away and the other
 * rider must travel less than max_move in that time. Prediction retains the
 * add/subtract at the projected point, which may round differently than dx*k. */
float rx_generic_brake_contact_time(rx_addr behavior,rx_addr other,float max_move) {
    ACT_FRAME_EBP(f,16,0x48,2);
    rx_addr own=brake_vehicle(U32(behavior,8)), ov=brake_vehicle(other);
    SU32(f,0x40,U32(own,0xc0)); SU32(f,0x48,U32(own,0xc8));
    float dx=F32(ov,0x160), dz=F32(ov,0x168);
    float length2=rx_adds(rx_adds(rx_muls(dx,dx),rx_muls(0.0f,0.0f)),rx_muls(dz,dz));
    SF32(f,0x10,dx); SU32(f,0x14,0); SF32(f,0x18,dz); SF32(f,0xc,length2);
    if (length2>0.0f) {
        /* This SqrtF argument is an actual FLD/FSTP pair. */
        float length=brake_sqrt(brake_x87_copy(F32(f,0xc)));
        float inv=rx_divs(F32(0,0x00931f24),length);
        SF32(f,0x10,rx_muls(inv,F32(f,0x10)));
        SF32(f,0x14,rx_muls(inv,0.0f));
        SF32(f,0x18,rx_muls(F32(f,0x18),inv));
    }
    ov=brake_vehicle(other);
    SU32(f,0x30,U32(ov,0xc0)); SU32(f,0x34,0); SU32(f,0x38,U32(ov,0xc8));
    const float radius=brake_x87_copy(F32(0,0x00935890));
    int hit;
    ACT_CALLR(hit,2,rx_race_point_near_segment(f+0x30,behavior+0x30,behavior+0x40,radius,f+0x20));
    if (!hit) ACT_CALLR(hit,2,rx_race_point_near_segment(f+0x30,behavior+0x50,behavior+0x60,radius,f+0x20));
    float result=0.0f;
    if (hit) {
        float ey=rx_subs(0.0f,F32(f,0x24)),ex=rx_subs(F32(f,0x40),F32(f,0x20)),ez=rx_subs(F32(f,0x48),F32(f,0x28));
        float square=rx_adds(rx_adds(rx_muls(ey,ey),rx_muls(ex,ex)),rx_muls(ez,ez));
        float distance=brake_sqrt(square);
        float time=rx_divs(distance,F32(brake_vehicle(U32(behavior,8)),0x150));
        SF32(f,0xc,time);
        if (F32(0,0x00935aa8)>time) {
            float scale=rx_muls(F32(ov,0x150),time);
            float z=rx_muls(F32(f,0x18),scale),x=rx_muls(F32(f,0x10),scale),y=rx_muls(F32(f,0x14),scale);
            x=rx_subs(rx_adds(x,F32(f,0x20)),F32(f,0x20));
            y=rx_subs(rx_adds(y,F32(f,0x24)),F32(f,0x24));
            z=rx_subs(rx_adds(z,F32(f,0x28)),F32(f,0x28));
            float displacement=brake_sqrt(rx_adds(rx_adds(rx_muls(y,y),rx_muls(x,x)),rx_muls(z,z)));
            if (max_move>displacement) result=F32(f,0xc);
        }
    }
    ACT_END(f);return result;
}
RX_FUNCTION(0x005311d0, rx_generic_brake_contact_time)

/*00531430 ECX=other, stackbehavior, RET4 -> XMM0. The other trajectory is
 * offset backwards by its class width and extends along its unnormalized flat
 * velocity. Only a crossing reached by the other first and ourselves within
 * the original .45-second limit contributes. */
float rx_generic_brake_crossing_time(rx_addr other,rx_addr behavior) {
    ACT_FRAME_EBP(f,16,0x64,3);
    rx_addr own=brake_vehicle(U32(behavior,8)),ov=brake_vehicle(other);
    SU32(f,0x30,U32(own,0xc0)); SU32(f,0x38,U32(own,0xc8));
    float ox=F32(ov,0xc0),oz=F32(ov,0xc8),vx=F32(ov,0x160),vz=F32(ov,0x168);
    ov=brake_vehicle(other); SU32(f,0x18,ov);
    float width=F32(0,0x0093582c+4u*U32(ov,0x194));
    float backx=rx_muls(vx,width),backz=rx_muls(vz,width),backy=rx_muls(width,F32(0,0x0092e12c));
    SF32(f,0x40,ox); SF32(f,0x48,oz);
    float x0=rx_subs(ox,backx),z0=rx_subs(oz,backz),y0=rx_subs(0.0f,backy);
    float limit=F32(0,0x009cf5b0);
    float x1=rx_adds(rx_muls(vx,limit),x0),y1=rx_adds(rx_muls(0.0f,limit),y0),z1=rx_adds(rx_muls(vz,limit),z0);
    SF32(f,0x50,x0);SF32(f,0x54,y0);SF32(f,0x58,z0);
    SF32(f,0x60,x1);SF32(f,0x64,y1);SF32(f,0x68,z1);
    int hit; ACT_CALLR(hit,1,rx_race_segments_cross_xz(f+0x50,f+0x60,behavior+0x10,behavior+0x20,f+0x20));
    float result=0.0f;
    if (hit) {
        float ix=F32(f,0x20),iy=rx_subs(F32(f,0x24),F32(0,0x0092e12c)),iz=F32(f,0x28);
        float otherx=rx_subs(ix,F32(f,0x40)),selfx=rx_subs(ix,F32(f,0x30));
        float selfz=rx_subs(iz,F32(f,0x38)),otherz=rx_subs(iz,F32(f,0x48));
        SF32(f,0x20,otherx);SF32(f,0x24,iy);SF32(f,0x28,otherz);
        float distance=brake_sqrt(rx_adds(rx_adds(rx_muls(iy,iy),rx_muls(selfx,selfx)),rx_muls(selfz,selfz)));
        float time=rx_divs(distance,F32(brake_vehicle(U32(behavior,8)),0x150));SF32(f,0x1c,time);
        /* The original leaves the first cdecl argument on the stack, overwrites
         * it for the second SqrtF, then removes it only after both square roots. */
        rx_set_sp(f-4);
        float square=rx_adds(rx_adds(rx_muls(F32(f,0x24),F32(f,0x24)),rx_muls(F32(f,0x20),F32(f,0x20))),rx_muls(F32(f,0x28),F32(f,0x28)));
        SF32(f-4,0,square);
        rx_set_sp(f-8);distance=phys_sqrtf(square);rx_set_sp(f-4);
        float other_time=rx_divs(distance,F32(U32(f,0x18),0x150));
        time=F32(f,0x1c);rx_set_sp(f);
        if (time>other_time && F32(0,0x009cd960)>time) result=time;
    }
    ACT_END(f);return result;
}
RX_FUNCTION(0x00531430, rx_generic_brake_crossing_time)
