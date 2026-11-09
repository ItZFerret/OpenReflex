/* Verify imported collision vertices and broadphase membership through the loaded product world. */
#include "rx_api.h"
#include "rx_game.h"
#include "fmt_modtrack.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv) {
    if(argc<3||argc>5)return 2;
    float scale=argc>3?strtof(argv[3],NULL):1.f;
    rx_session* s=rx_session_create(argv[1]);if(!s)return 3;
    if(rx_session_mount_level(s,"NAT_Concord",argv[2]))return 4;
    if(rx_session_set_map_scale(s,scale))return 13;
    rx_event_spec e;rx_session_event_default(&e);
    snprintf(e.event_type,sizeof e.event_type,"FreeRide");e.event_type_id=9;
    snprintf(e.location,sizeof e.location,"NAT_Concord");e.location_id=-1;e.track_id=0;e.vehicle_class=-1;e.riders=1;e.ai_difficulty=0;e.duration=0;e.duration_type[0]=e.event_activation[0]=0;
    if(rx_session_event_load(s,&e))return 5;
    for(int i=0;i<3000&&rx_session_get_state(s)!=RX_SESSION_RACING;i++)if(rx_session_advance_dt(s,1.f/60))return 6;
    rx_slot slots[32];int count=rx_session_slots(s,slots,32);rx_addr vehicle=0;
    for(int i=0;i<count;i++)if(slots[i].local)vehicle=slots[i].vehicle;
    if(!vehicle)return 7;
    rx_addr handle=U32(vehicle+0x90,0xec),world=handle?U32(handle,4):0;if(!world)return 8;
    rx_addr vector=world+0x19b4;uint32_t n=(U32(vector,0x10)-U32(vector,0xc))/0x40;
    if(n!=fmt_modtrack_collision_count()||!n)return 9;
    unsigned char* seen=calloc(n,1);rx_addr grid=U32(U32(vector,0x18),0x30);
    for(uint32_t c=0;c<U32(grid,0x410);c++)for(uint32_t j=0;j<U32(U32(grid,0x474),4*c);j++){
        uint32_t id=U32(U32(grid,0x470),4*(U32(grid,0x414)*c+j));if(id<n)seen[id]=1;
    }
    for(uint32_t j=0;j<U32(grid,0x478);j++){uint32_t id=U32(U32(grid,0x47c),4*j);if(id<n)seen[id]=1;}
    uint32_t triangles=0;
    for(uint32_t i=0;i<n;i++){
        if(!seen[i])return 10;
        rx_addr record=U32(vector,0xc)+i*0x40,shape=U32(record,0x28);uint32_t nv;
        const float* points=fmt_modtrack_collision_chunk(i,&nv);
        if(U32(shape,8)!=0x10 || U32(shape,0x20)!=nv/3 || !(U32(shape,0x10)&4))return 11;
        if(!U32(record,0x30)||U32(U32(record,0x30),0)!=0x00974f84)return 15;
        rx_addr data=U32(shape,0x24);
        const float* anchors=fmt_modtrack_collision_anchors(i);
        for(uint32_t v=0;v<nv;v++) {
            float expected[3];fmt_modtrack_scale_point(points+v*3,anchors?anchors+v*4:NULL,scale,expected);
            for(int k=0;k<3;k++)if(fabsf(F32(data+v*16,k*4)+F32(record,k*4)-expected[k])>.001f)return 12;
        }
        triangles+=nv/3;
    }
    printf("PASS: %u static groups in broadphase; all %u collision triangles match package coordinates\n",n,triangles);
    free(seen);
    if(argc>4)for(uint32_t chunk=0;chunk<n;chunk++){
        uint32_t nv;const float* points=fmt_modtrack_collision_chunk(chunk,&nv);
        float p[3]={0,0,0},dir[3]={1,0,0};
        const float* anchors=fmt_modtrack_collision_anchors(chunk);
        for(int v=0;v<3;v++){float q[3];fmt_modtrack_scale_point(points+v*3,anchors?anchors+v*4:NULL,scale,q);for(int k=0;k<3;k++)p[k]+=q[k]/3;}

        p[1]+=3.f;
        int rc=rx_session_spawn_at_instant(s,p,dir);
        for(int retry=0;rc==3 && retry<600;retry++){
            if(rx_session_set_input(s,(uint8_t[256]){0},NULL,0)||rx_session_advance_dt(s,1.f/60))return 16;
            rc=rx_session_spawn_at_instant(s,p,dir);
        }
        if(rc){fprintf(stderr,"Cannot reach contact probe %u: %d\n",chunk,rc);return 17;}
        fprintf(stderr,"contact probe chunk %u spawn %d at %.2f %.2f %.2f\n",chunk,rc,p[0],p[1],p[2]);
        for(int j=0;j<240;j++){
            uint8_t keys[256]={0};keys[0x11]=0x80;
            if(rx_session_set_input(s,keys,NULL,0)||rx_session_advance_dt(s,1.f/60)){
                fprintf(stderr,"FAIL chunk %u step %d: %s\n",chunk,j,rx_session_last_error(s));return 14;
            }
        }
    }
    if(argc>4)printf("PASS: impact simulation at all %u scenery groups, scale %.2f\n",n,scale);
    rx_session_destroy(s);return 0;
}
