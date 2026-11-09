/* Integration test: actual native FreeRide frames and mapped keyboard input. */
#include "rx_api.h"
#include "rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s: %s\n",__LINE__,#x,rx_session_last_error(s)); return 1; } } while(0)
int main(int argc,char **argv) {
 if(argc!=2)return 2; alarm(300); setvbuf(stdout,NULL,_IONBF,0);
 rx_session *s=rx_session_create(argv[1]); CHECK(s);
 rx_event_spec e;rx_session_event_default(&e);strcpy(e.event_type,"FreeRide");strcpy(e.location,"FR_DLC_Hills");e.event_type_id=9;e.location_id=-1;e.vehicle_class=-1;e.riders=1;e.ai_difficulty=0;e.duration=0;e.duration_type[0]=0;
 CHECK(rx_session_event_load(s,&e)==0);
 int loading=0;while(rx_session_get_state(s)==RX_SESSION_LOADING && loading++<1000)CHECK(rx_session_advance_dt(s,1.f/60)==0);
 for(int i=0;i<600 && rx_session_get_state(s)!=RX_SESSION_RACING;i++)CHECK(rx_session_advance_dt(s,1.f/60)==0);
 CHECK(rx_session_get_state(s)==RX_SESSION_RACING);
 rx_remap_table maps;CHECK(rx_session_remap_get(s,&maps)==0);
 int throttle=maps.keyboard[0],reset=maps.keyboard[12];
 printf("bindings throttle=%d reset=%d\n",throttle,reset);
 rx_bike_state b;CHECK(rx_session_bike(s,&b)==0);float start[3];memcpy(start,b.position[0],sizeof(start));
 rx_addr g=U32(0,0x00a946b8),player=g+0x7460+U32(g,0x153ec)*0x12a0; float priorReset=0; int grounded=0,input=0;float maxspeed=0,maxdistance=0;uint32_t frames=b.frames;
 for(int i=0;i<1800;i++) {
  unsigned char keys[256]={0}; if(i>=180 && i<1080)keys[0x11]=0x80;
  /* no manual reset in diagnostic */
  CHECK(rx_session_set_input(s,keys,NULL,0)==0);CHECK(rx_session_advance_dt(s,1.f/60)==0);CHECK(rx_session_bike(s,&b)==0);
  float timer=F32(player,0x116c); if(timer>priorReset+.1f) printf("RESET frame=%d reason=%u timer=%f pos=%.3f,%.3f,%.3f\n",i,U32(player,0x1170),timer,b.position[0][0],b.position[0][1],b.position[0][2]); priorReset=timer; CHECK(b.frames>frames);frames=b.frames;
  for(int j=0;j<3;j++)CHECK(isfinite(b.position[0][j]));
  CHECK(b.position[0][1]>450 && b.position[0][1]<1100);
  rx_onboard_state o;CHECK(rx_session_onboard_state(s,&o)==0);
  if(o.contacts_valid&&(o.front_grounded||o.rear_grounded))grounded++;
  if(b.inputs[0]>.1f)input++;
  float dx=b.position[0][0]-start[0],dz=b.position[0][2]-start[2],d=hypotf(dx,dz);
  if(d>maxdistance)maxdistance=d;if(b.speed>maxspeed)maxspeed=b.speed;
  if(i%180==0)printf("frame=%d pos=%.3f,%.3f,%.3f speed=%.3f throttle=%.3f grounded=%d/%d wreck=%d\n",i,b.position[0][0],b.position[0][1],b.position[0][2],b.speed,b.inputs[0],o.front_grounded,o.rear_grounded,o.wrecked);
 }
 CHECK(input>300);CHECK(grounded>300);CHECK(maxspeed>3);CHECK(maxdistance>20);
 printf("PASS ride frames=1800 input=%d grounded=%d maxSpeed=%.3f displacement=%.3f\n",input,grounded,maxspeed,maxdistance);
 rx_session_destroy(s);return 0;
}
