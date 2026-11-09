#include "rx_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
int main(int argc,char**argv){
 if(argc<3||argc>5)return 2;
 float scale=argc>3?strtof(argv[3],NULL):1.f; int frames=argc>4?atoi(argv[4]):1200;
 rx_session*s=rx_session_create(argv[1]); if(!s){fprintf(stderr,"%s\n",rx_session_last_error(NULL));return 1;}
 if(rx_session_mount_level(s,"NAT_Concord",argv[2]))return 3;
 if(rx_session_set_map_scale(s,scale))return 13;
 rx_event_spec e;rx_session_event_default(&e);snprintf(e.event_type,sizeof e.event_type,"FreeRide");e.event_type_id=9;snprintf(e.location,sizeof e.location,"NAT_Concord");e.location_id=-1;e.track_id=0;e.vehicle_class=-1;e.riders=1;e.ai_difficulty=0;e.duration=0;e.duration_type[0]=0;e.event_activation[0]=0;
 if(rx_session_event_load(s,&e)){fprintf(stderr,"%s\n",rx_session_last_error(s));return 4;}
 for(int i=0;i<3000&&rx_session_get_state(s)!=RX_SESSION_RACING;i++)if(rx_session_advance_dt(s,1.f/60))return 5;
 if(rx_session_get_state(s)!=RX_SESSION_RACING){fprintf(stderr,"state %d %s\n",rx_session_get_state(s),rx_session_last_error(s));return 6;}
 rx_terrain_info info;if(rx_session_terrain(s,&info))return 7;
 size_t n=2048u*2048u;float*a=malloc(n*4),*b=malloc(n*4);if(!a||!b)return 8;
 rx_session_terrain_deform(s,a,0,0,2048,2048);
 rx_bike_state start;rx_session_bike(s,&start);float maxdistance=0;
 for(int i=0;i<frames;i++){uint8_t keys[256]={0};keys[0x11]=0x80;if(rx_session_set_input(s,keys,NULL,0)||rx_session_advance_dt(s,1.f/60)){fprintf(stderr,"frame %d: %s\n",i,rx_session_last_error(s));return 9;} if(i%120==0){rx_bike_state bb;rx_session_bike(s,&bb);float dx=bb.position[0][0]-start.position[0][0],dz=bb.position[0][2]-start.position[0][2];float distance=sqrtf(dx*dx+dz*dz);if(distance>maxdistance)maxdistance=distance;printf("frame %d speed %f pos %f %f %f\n",i,bb.speed,bb.position[0][0],bb.position[0][1],bb.position[0][2]);}}
 rx_bike_state bike;rx_session_bike(s,&bike);rx_session_terrain_deform(s,b,0,0,2048,2048);
 size_t changed=0;for(size_t i=0;i<n;i++)if(fabsf(a[i]-b[i])>1e-6f)changed++;
 printf("RACING: bike %f %f %f; deformed samples %zu\n",bike.position[0][0],bike.position[0][1],bike.position[0][2],changed);
 int result=changed>100&&maxdistance>100&&isfinite(bike.position[0][1])?0:10;
 if(rx_session_terrain_reset(s)||rx_session_set_input(s,(uint8_t[256]){0},NULL,0)||rx_session_advance_dt(s,1.f/60))result=11;
 rx_session_terrain_deform(s,b,0,0,2048,2048);
 size_t remaining=0;for(size_t i=0;i<n;i++)if(fabsf(a[i]-b[i])>1e-6f)remaining++;
 printf("reset remaining samples %zu; maximum displacement %.2f feet\n",remaining,maxdistance);
 if(remaining>100)result=12;
 free(a);free(b);rx_session_destroy(s);return result;
}
