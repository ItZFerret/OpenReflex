/* Serial extracted-asset track acceptance. Uses only the public session API.
 * rx_api_track_test <core library> <extracted root> <activity> <type id> <level> <location id> [riders] [racing frames]
 * Event IDs come from the installed EventTypeLocations tables, never engine enum guesses.
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n=(__typeof__(&n))dlsym(lib,#n); if(!f_##n){fprintf(stderr,"missing API %s\n",#n);return 2;}
#define FAIL(msg) do{fprintf(stderr,"FAIL %s: %s\n",msg,f_rx_session_last_error(s));f_rx_session_destroy(s);return 1;}while(0)
int main(int argc,char**argv){
 if(argc<7)return 2;setvbuf(stdout,NULL,_IONBF,0);alarm(240);
 char marker[4096];snprintf(marker,sizeof marker,"%s/reflex-extracted-v1.txt",argv[2]);if(access(marker,R_OK)){fprintf(stderr,"extracted marker required\n");return 2;}
 void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
 LOAD(rx_session_create);LOAD(rx_session_destroy);LOAD(rx_session_last_error);LOAD(rx_session_event_default);LOAD(rx_session_event_load);
 LOAD(rx_session_advance_dt);LOAD(rx_session_get_state);LOAD(rx_session_set_input);LOAD(rx_session_bike);LOAD(rx_session_race);
 LOAD(rx_session_rider_pose);LOAD(rx_session_bike_pose);LOAD(rx_session_camera);LOAD(rx_session_terrain);LOAD(rx_session_frames);
 rx_session*s=f_rx_session_create(argv[2]);if(!s)FAIL("create");
 rx_event_spec e;f_rx_session_event_default(&e);snprintf(e.event_type,sizeof e.event_type,"%s",argv[3]);e.event_type_id=atoi(argv[4]);
 snprintf(e.location,sizeof e.location,"%s",argv[5]);e.location_id=atoi(argv[6]);e.track_id=0;e.riders=argc>7?atoi(argv[7]):4;e.ai_difficulty=95;e.vehicle_class=3;e.duration=2;strcpy(e.duration_type,"Laps");
 if(!strcmp(e.event_type,"FreeRide")){e.riders=1;e.ai_difficulty=0;e.vehicle_class=-1;e.duration=0;e.duration_type[0]=0;e.event_activation[0]=0;}
 printf("BEGIN %s/%s type=%d location=%d riders=%d class=%d\n",e.event_type,e.location,e.event_type_id,e.location_id,e.riders,e.vehicle_class);
 if(f_rx_session_event_load(s,&e))FAIL("event load");
 int racing=0,first=-1,target=argc>8?atoi(argv[8]):900;float maxspeed=0,firstpos[3]={0};
 for(int i=0;i<3600+target;i++){
  uint8_t keys[256]={0};if(racing){keys[0x11]=0x80;if(racing>240&&racing<300)keys[0x1e]=0x80;if(racing>420&&racing<480)keys[0x20]=0x80;if(racing>600&&racing<660){keys[0x11]=0;keys[0x1f]=0x80;}}
  if(f_rx_session_set_input(s,keys,NULL,0))FAIL("input");
  if(f_rx_session_advance_dt(s,1.f/60))FAIL("advance");
  if(f_rx_session_get_state(s)==RX_SESSION_RACING){
   rx_bike_state b;if(f_rx_session_bike(s,&b))FAIL("bike");
   for(int j=0;j<4;j++){for(int k=0;k<3;k++)if(!isfinite(b.position[j][k])||!isfinite(b.velocity[k]))FAIL("nonfinite bike");for(int k=0;k<4;k++)if(!isfinite(b.orientation[j][k]))FAIL("nonfinite orientation");}
   if(!isfinite(b.speed))FAIL("nonfinite speed");if(b.speed>maxspeed)maxspeed=b.speed;
   if(first<0){first=i;for(int k=0;k<3;k++)firstpos[k]=b.position[0][k];printf("RACING first_step=%d\n",i);}racing++;
   if(racing>=target){
    float rp[69*16],bp[64*16];rx_camera cam;rx_terrain_info terrain;rx_race_state race;
    int bones=f_rx_session_rider_pose(s,rp,69),nodes=f_rx_session_bike_pose(s,bp,64);
    if(bones!=69||nodes<=0||f_rx_session_camera(s,&cam)||f_rx_session_terrain(s,&terrain)||f_rx_session_race(s,&race))FAIL("presentation getters");
    float dist=0;for(int k=0;k<3;k++){float d=b.position[0][k]-firstpos[k];dist+=d*d;}
    if(maxspeed<1||dist<1)FAIL("bike did not move");
    printf("PASS %s/%s racing_frames=%d total_frames=%llu max_speed=%.3f distance=%.3f riders=%d bones=%d nodes=%d terrain=%u\n",e.event_type,e.location,racing,(unsigned long long)f_rx_session_frames(s),maxspeed,sqrtf(dist),race.riders,bones,nodes,terrain.samples);
    f_rx_session_destroy(s);return 0;
   }
  }
  if(first<0&&i>=3600)FAIL("no racing state within60game seconds");
  usleep(3000); /* Yield three milliseconds per frame; never run more than one process. */
 }
 FAIL("racing frame target not reached");
}
