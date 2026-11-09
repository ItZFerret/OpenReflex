#include "replay_follow.h"
#include "rider_cap.h"
#include "replay_decode.h"
#include "replay_clip.h"
#include "build_activity_util.h"
#include "build_vehicle.h"
#include "race_activity.h"
#include "phys_vec.h"
#include "phys_crt.h"
#include "build_series_init.h"
#include "career_activity.h"

void rx_replay_pose_vehicles(rx_addr activity, int32_t first, int32_t last) {
    ACT_FRAME_EBP(f,16,0xf4,3);
    SU32(f,0xfc,U32(0,0x009b45e4)^(f+12));
    const int32_t count=I32(activity,0x26c);
    SU32(f,0x18,activity);SU32(f,0x1c,count);
    for(int32_t i=0;i<count;++i) {
        rx_addr driver=U32(U32(activity,0x268),4u*(uint32_t)i);
        rx_addr player=U32(driver,0x74);
        uint32_t kind=U32(activity,0x23c);
        if(kind>=0x1f) {
            if(!(U8(0,0x009e275c)&1)) {
                SU32(0,0x009e275c,U32(0,0x009e275c)|1u);
                ACT_CALL(0,(void)rx_series_manager_construct());
                const uint32_t destroy=0x00911da0;
                (void)act_call_fn(0x008f9fbf,RX_CDECL,0,1,&destroy);
            }
            ACT_CALLR(kind,1,rx_career_current_event_type(0x009e2188));
        }
        if(kind==5 && (!U32(player,4)||U32(player,0x10))) continue;
        rx_addr vehicle=U32(player,0x1ac);
        int32_t sample_index=-1;
        if(first<13 && (uint32_t)i<12)
            sample_index=(int32_t)((uint32_t)U32(0,0x00a942dc)*(uint32_t)i + U32(0,0x00a942d8)*(uint32_t)first);
        else if(first>=0 && first<13 && (uint32_t)i<RX_RC_MAX)   /* (w-rc-core) riders 12..31: the side buffer's indices */
            sample_index=(int32_t)rx_clip_index((uint32_t)first,(uint32_t)i,0);
        replay_sample d=rx_replay_decode_record(sample_index>=0?rx_clip_record((uint32_t)sample_index):U32(0,0x00a7f124)+(uint32_t)sample_index*0x50u);
        rx_st4(f+0x80,d.p);rx_st4(f+0x20,d.q);
        float x=d.q.v[0],y=d.q.v[1],z=d.q.v[2],w=d.q.v[3];
        float two=F32(0,0x0092e330),one=F32(0,0x00931f24);
        float z2=phys_mul(z,two),y2=phys_mul(y,two),x2=phys_mul(x,two);
        float zy=phys_mul(z2,y),xx=phys_mul(x,x2);
        float a0=phys_sub(phys_mul(x,y2),phys_mul(z2,w));
        float a1=phys_sub(one,phys_add(phys_mul(z2,z),xx));
        float xw=phys_mul(x2,w);
        float a2=phys_add(zy,xw);
        float b0=phys_add(phys_mul(x,z2),phys_mul(y2,w));
        float b1=phys_sub(zy,xw);
        float b2=phys_sub(one,phys_add(phys_mul(y2,y),xx));
        SF32(f,0x50,a0);SF32(f,0x54,a1);SF32(f,0x58,a2);
        SF32(f,0x60,b0);SF32(f,0x64,b1);SF32(f,0x68,b2);
        ACT_CALL(3,rx_vehicle_place(vehicle,f+0x80,f+0x60,f+0x50));
        ACT_CALL(1,rx_replay_follow_begin(U32(vehicle,0x234),(int32_t)((uint32_t)last+(uint32_t)sample_index),sample_index));
        activity=U32(f,0x18);
    }
    rx_set_sp(f+12);
    ACT_CALL(0,phys_security_check_cookie(U32(f,0xfc)^(f+12)));
    ACT_END(f);
}
RX_FUNCTION(0x006ac0e0,rx_replay_pose_vehicles)
