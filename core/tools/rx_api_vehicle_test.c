/* Public-API vehicle acceptance using an exported garage profile and existing extracted assets.
 * No internal memory writes: selected model, factory, bodies, poses and real input-driven motion are checked.
 * usage: test <core> <game files> <profile.bin> <model id> <factory id> <body count> [racing frames] [location] [location id]
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n=(__typeof__(&n))dlsym(lib,#n); if(!f_##n){fprintf(stderr,"missing API %s\n",#n);return 2;}
#define FAIL(why) do {fprintf(stderr,"FAIL %s: %s\n",why,f_rx_session_last_error(s));f_rx_session_destroy(s);return 1;} while(0)
int main(int argc,char **argv) {
    if(argc<7)return 2;
    setvbuf(stdout,NULL,_IONBF,0);alarm(240);
    char marker[4096];snprintf(marker,sizeof marker,"%s/reflex-extracted-v1.txt",argv[2]);if(access(marker,R_OK))return 2;
    void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
    LOAD(rx_session_create);LOAD(rx_session_destroy);LOAD(rx_session_last_error);LOAD(rx_session_profile_import);
    LOAD(rx_session_event_default);LOAD(rx_session_event_load);LOAD(rx_session_advance_dt);LOAD(rx_session_get_state);
    LOAD(rx_session_set_input);LOAD(rx_session_vehicle_info);LOAD(rx_session_appearance);LOAD(rx_session_slots);LOAD(rx_session_rider_pose);
    LOAD(rx_session_bike_pose);LOAD(rx_session_camera);LOAD(rx_session_frames);
    FILE *file=fopen(argv[3],"rb");if(!file)return 2;
    fseek(file,0,SEEK_END);long bytes=ftell(file);rewind(file);
    if(bytes<=0||bytes%(long)sizeof(rx_profile_cell)||bytes>65536*(long)sizeof(rx_profile_cell)){fclose(file);return 2;}
    rx_profile_cell *cells=malloc((size_t)bytes);if(!cells){fclose(file);return 2;}
    if(fread(cells,1,(size_t)bytes,file)!=(size_t)bytes){free(cells);fclose(file);return 2;}fclose(file);
    rx_session *s=f_rx_session_create(argv[2]);if(!s){free(cells);FAIL("create");}
    if(f_rx_session_profile_import(s,cells,(int)(bytes/sizeof *cells))){free(cells);FAIL("profile import");}free(cells);
    const int model=atoi(argv[4]),factory=atoi(argv[5]),bodies=atoi(argv[6]),target=argc>7?atoi(argv[7]):600;
    rx_event_spec e;f_rx_session_event_default(&e);
    strcpy(e.event_type,"FreeRide");e.event_type_id=9;e.event_activation[0]=0;strcpy(e.location,"FR_Alpine");e.location_id=45;
    if(argc>9){snprintf(e.location,sizeof e.location,"%s",argv[8]);e.location_id=atoi(argv[9]);}
    e.track_id=0;e.duration=0;e.duration_type[0]=0;e.riders=1;e.ai_difficulty=0;e.vehicle_class=-1;
    if(getenv("VEHICLE_ACTIVITY")){
      snprintf(e.event_type,sizeof e.event_type,"%s",getenv("VEHICLE_ACTIVITY"));e.event_type_id=atoi(getenv("VEHICLE_ACTIVITY_ID"));
      e.vehicle_class=1;e.riders=4;e.ai_difficulty=95;e.duration=2;strcpy(e.duration_type,"Laps");
    }
    printf("BEGIN model=%d factory=%d bodies=%d %s/%s riders=%d\n",model,factory,bodies,e.event_type,e.location,e.riders);
    if(f_rx_session_event_load(s,&e))FAIL("event load");
    int racing=0,local_slot=-1;float peak=0,start[3]={0};
    for(int i=0;i<3600+target;i++) {
        uint8_t keys[256]={0};if(racing){keys[0x11]=0x80;if(racing>240&&racing<300)keys[0x1e]=0x80;if(racing>420&&racing<480)keys[0x20]=0x80;if(racing>600&&racing<660){keys[0x11]=0;keys[0x1f]=0x80;}}
        if(f_rx_session_set_input(s,keys,NULL,0)||f_rx_session_advance_dt(s,1.f/60))FAIL("advance");
        if(f_rx_session_get_state(s)==RX_SESSION_RACING) {
            rx_vehicle_info v,again;rx_appearance appearance;
            if(local_slot<0){rx_slot slots[16];int n=f_rx_session_slots(s,slots,16);for(int k=0;k<n;k++)if(slots[k].local)local_slot=slots[k].slot;if(local_slot<0)FAIL("local slot missing");}
            int vi=f_rx_session_vehicle_info(s,-1,&v),ap=f_rx_session_appearance(s,local_slot,&appearance);
            if(vi||ap){fprintf(stderr,"GETTERS frame=%llu vehicle_info=%d appearance=%d type=%d bodies=%d nodes=%d\n",(unsigned long long)f_rx_session_frames(s),vi,ap,v.type,v.bodies,v.model_nodes);FAIL("vehicle getters");}
            if(v.type!=factory||v.bodies!=bodies||appearance.vehicle_model_id!=model)FAIL("selected model/factory/body count changed");
            if(!isfinite(v.speed)||!isfinite(v.rpm))FAIL("nonfinite speed/rpm");
            for(int k=0;k<4*bodies;k++)if(!isfinite(v.render_position[k])||!isfinite(v.render_orientation[k]))FAIL("nonfinite body pose");
            if(v.speed>peak)peak=v.speed;
            /* Catch a transient broken linkage during driving, not only the final pose. */
            float live_pose[128*16],live_rider[69*16];
            int live_nodes=f_rx_session_bike_pose(s,live_pose,128),live_bones=f_rx_session_rider_pose(s,live_rider,69);
            if(live_nodes!=v.model_nodes||live_nodes<1||live_nodes>128||live_bones!=69)FAIL("incomplete live pose");
            for(int k=0;k<live_nodes*16;k++)if(!isfinite(live_pose[k]))FAIL("nonfinite live suspension/model pose");
            for(int k=0;k<live_bones*16;k++)if(!isfinite(live_rider[k]))FAIL("nonfinite live driver pose");
            if(racing%300==0&&e.riders>1){rx_slot slots[16];int n=f_rx_session_slots(s,slots,16);if(n!=e.riders)FAIL("incomplete roster");for(int k=0;k<n;k++){rx_vehicle_info ai;if(f_rx_session_vehicle_info(s,slots[k].slot,&ai)||ai.type!=factory||ai.bodies!=bodies||!isfinite(ai.speed))FAIL("AI vehicle mismatch");}}
            if(racing%300==0)printf("SAMPLE race=%d speed=%.6f rpm=%.6f gear=%d pos=%.6f,%.6f,%.6f\n",racing,v.speed,v.rpm,v.gear,v.render_position[0],v.render_position[1],v.render_position[2]);
            if(!racing){memcpy(start,v.render_position,sizeof start);printf("RACING frame=%llu model=%d factory=%d bodies=%d nodes=%d\n",(unsigned long long)f_rx_session_frames(s),model,v.type,v.bodies,v.model_nodes);}
            if(++racing>=target) {
                if(f_rx_session_vehicle_info(s,-1,&again)||memcmp(&v,&again,sizeof v))FAIL("same-frame getter changed state");
                float vehicle_pose[128*16],rider_pose[69*16];rx_camera camera;
                int nodes=f_rx_session_bike_pose(s,vehicle_pose,128),bones=f_rx_session_rider_pose(s,rider_pose,69);
                if(nodes!=v.model_nodes||nodes<1||nodes>128||bones!=69||f_rx_session_camera(s,&camera))FAIL("presentation getters");
                for(int k=0;k<nodes*16;k++)if(!isfinite(vehicle_pose[k]))FAIL("nonfinite model node");
                for(int k=0;k<bones*16;k++)if(!isfinite(rider_pose[k]))FAIL("nonfinite rider bone");
                float distance=0;for(int k=0;k<3;k++){float d=v.render_position[k]-start[k];distance+=d*d;}
                if(peak<1||distance<1){fprintf(stderr,"MOTION peak=%.6f distance=%.6f\n",peak,sqrtf(distance));FAIL("vehicle did not move");}
                printf("PASS model=%d factory=%d racing_frames=%d total_frames=%llu bodies=%d nodes=%d bones=%d max_speed=%.3f distance=%.3f\n",model,factory,racing,(unsigned long long)f_rx_session_frames(s),bodies,nodes,bones,peak,sqrtf(distance));
                f_rx_session_destroy(s);return 0;
            }
        }
        if(i>=3600&&!racing)FAIL("no racing within 60 game seconds");
        usleep(3000);
    }
    FAIL("racing target not reached");
}
