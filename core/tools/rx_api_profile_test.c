/* Final public ABI contract, using a captured real profile snapshot.
 * clang -std=c11 -Icore/include core/tools/rx_api_profile_test.c -o <test>
 * RX_FULL_FRAME=1 <test> <librxcore> <asset-only-install> <profile-cells.bin>
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOAD(n) __typeof__(&n) f_##n=(__typeof__(&n))dlsym(lib,#n); if(!f_##n)return 2;
#define CHECK(v,msg) do { if(!(v)){fprintf(stderr,"FAIL %s: %s\n",msg,f_rx_session_last_error(s));return 1;}puts("PASS " msg); }while(0)
int main(int argc,char**argv){
 if(argc!=4)return 2;
 void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){puts(dlerror());return 2;}
 LOAD(rx_session_create);LOAD(rx_session_destroy);LOAD(rx_session_last_error);LOAD(rx_session_sizeof);
 LOAD(rx_session_profile_import);LOAD(rx_session_tuning_get);LOAD(rx_session_tuning_set);
 LOAD(rx_session_event_default);LOAD(rx_session_event_load);LOAD(rx_session_advance_dt);LOAD(rx_session_appearance);
 FILE*file=fopen(argv[3],"rb");if(!file)return 2;fseek(file,0,SEEK_END);long bytes=ftell(file);rewind(file);
 if(bytes<=0||bytes%(long)sizeof(rx_profile_cell))return 2;int n=(int)(bytes/(long)sizeof(rx_profile_cell));
 rx_profile_cell*c=malloc(bytes),*bad=malloc(bytes+2*sizeof(rx_profile_cell));if(!c||!bad||fread(c,1,bytes,file)!=(size_t)bytes)return 2;fclose(file);
 struct rx_session*s=f_rx_session_create(argv[2]);CHECK(s,"create without executable files");
 CHECK(sizeof(rx_profile_cell)==204 && f_rx_session_sizeof("rx_profile_cell")==204,"profile ABI");
 CHECK(f_rx_session_profile_import(s,NULL,0)==2,"empty snapshot rejected");
 memcpy(bad,c,bytes);bad[0].row=-1;CHECK(f_rx_session_profile_import(s,bad,n)==2,"negative row rejected before mutation");
 memcpy(bad,c,bytes);memset(bad[0].table,'x',32);CHECK(f_rx_session_profile_import(s,bad,n)==2,"unterminated field rejected");
 int gear_cell=-1,part_vehicle=-1,part_id=-1,gear_max=-1,part_max=-1;
 for(int i=0;i<n;i++){
  if(!strcmp(c[i].table,"PlayerRiderGear")){if(c[i].row>gear_max)gear_max=c[i].row;if(!strcmp(c[i].column,"RiderGearId"))gear_cell=i;}
  if(!strcmp(c[i].table,"PlayerVehicleParts")){if(c[i].row>part_max)part_max=c[i].row;if(c[i].row==0){if(!strcmp(c[i].column,"PlayerVehicleId"))part_vehicle=i;if(!strcmp(c[i].column,"VehiclePartId"))part_id=i;}}
 }
 if(gear_cell>=0){memcpy(bad,c,bytes);bad[n]=c[gear_cell];bad[n].row=gear_max+1;CHECK(f_rx_session_profile_import(s,bad,n+1)==2,"duplicate equipped rider type rejected");}
 if(part_vehicle>=0&&part_id>=0){memcpy(bad,c,bytes);bad[n]=c[part_vehicle];bad[n+1]=c[part_id];bad[n].row=bad[n+1].row=part_max+1;CHECK(f_rx_session_profile_import(s,bad,n+2)==2,"duplicate equipped vehicle part type rejected");}
 CHECK(f_rx_session_profile_import(s,c,n)==0,"valid complete profile imported after rejected inputs");
 int selected=-1;for(int i=0;i<n;i++)if(!strcmp(c[i].table,"PlayerRider")&&!strcmp(c[i].column,"LastEquippedVehicle"))selected=c[i].value;
 int tune_row=-1;for(int i=0;i<n;i++)if(!strcmp(c[i].table,"PlayerVehicleTuning")&&!strcmp(c[i].column,"PlayerVehicleId")&&c[i].value==selected)tune_row=c[i].row;
 const char*names[8]={"Engine1","Engine2","Suspension1","Suspension2","Tire1","Tire2","Braking","Steering"};int vals[8]={0};
 for(int i=0;i<n;i++)if(!strcmp(c[i].table,"PlayerVehicleTuning")&&c[i].row==tune_row)for(int k=0;k<8;k++)if(!strcmp(c[i].column,names[k]))vals[k]=c[i].value;
 rx_event_spec ev;f_rx_session_event_default(&ev);
 if(getenv("RX_TEST_FREERIDE")){strcpy(ev.event_type,"FreeRide");ev.event_type_id=9;ev.event_activation[0]=0;strcpy(ev.location,"FR_Alpine");ev.location_id=45;ev.track_id=0;ev.duration=0;ev.duration_type[0]=0;ev.riders=1;ev.ai_difficulty=0;ev.vehicle_class=-1;}
 CHECK(f_rx_session_event_load(s,&ev)==0,"event constructed with profile");
 CHECK(f_rx_session_profile_import(s,c,n)==2,"mid-event profile replacement rejected");
 rx_appearance a={0};for(int i=0;i<120;i++){CHECK(f_rx_session_advance_dt(s,1.f/60)==0,"loading frame");if(f_rx_session_appearance(s,0,&a)==0)break;}
 CHECK(a.vehicle_owner==0&&a.owner_vehicle_id==selected,"constructed vehicle matches actual profile ID");
 int guarded[10]={0x12345678,0,0,0,0,0,0,0,0,0x76543210};CHECK(f_rx_session_tuning_get(s,guarded+1)==0,"effective tuning read");CHECK(guarded[0]==0x12345678&&guarded[9]==0x76543210,"output canaries");
 int initial[8];memcpy(initial,guarded+1,sizeof initial);int invalid[8];memcpy(invalid,vals,sizeof invalid);invalid[0]=101;
 CHECK(f_rx_session_tuning_set(s,invalid)==2,"out-of-range tuning rejected");memcpy(invalid,vals,sizeof invalid);invalid[0]++;
 CHECK(f_rx_session_tuning_set(s,invalid)==2,"bonus-point creation rejected");CHECK(f_rx_session_tuning_get(s,guarded+1)==0&&!memcmp(initial,guarded+1,sizeof initial),"rejected tuning leaves state unchanged");
 vals[6]=(vals[6]+50)%150;CHECK(f_rx_session_tuning_set(s,vals)==0,"valid live tuning applies");CHECK(f_rx_session_tuning_get(s,guarded+1)==0&&guarded[7]==vals[6],"live braking readback");
 if(getenv("RX_TEST_FREERIDE")) {
  LOAD(rx_session_get_state);LOAD(rx_session_set_input);LOAD(rx_session_bike);LOAD(rx_session_frames);LOAD(rx_session_game_time);
  uint8_t keys[256]={0};keys[0x11]=0x80; /* DirectInput W, the existing throttle mapping. */
  CHECK(f_rx_session_set_input(s,keys,NULL,0)==0,"FreeRide throttle input installed");
  for(int i=0;i<1200;i++) {
   if(getenv("RX_TEST_LOOK")) {
    keys[0xcb]=(i%240<30)?0x80:0;keys[0xc8]=(i%240>=60&&i%240<90)?0x80:0;
    if(f_rx_session_set_input(s,keys,NULL,0))return 1;
   }
   if(f_rx_session_advance_dt(s,1.f/60)){fprintf(stderr,"FAIL FreeRide frame %d: %s\n",i,f_rx_session_last_error(s));return 1;}
  }
  CHECK(f_rx_session_get_state(s)==RX_SESSION_RACING,"FreeRide reaches in-progress state");
  rx_bike_state bike;CHECK(f_rx_session_bike(s,&bike)==0,"FreeRide bike state readable after 1200 frames");
  CHECK(f_rx_session_tuning_get(s,guarded+1)==0&&guarded[7]==vals[6],"live tuning retained while FreeRide advances");
  printf("FreeRide speed=%g frame=%llu game_time=%g\n",bike.speed,(unsigned long long)f_rx_session_frames(s),f_rx_session_game_time(s));
 }
 f_rx_session_destroy(s);s=f_rx_session_create(argv[2]);CHECK(s,"second session recreated");CHECK(f_rx_session_profile_import(s,c,n)==0,"profile reimported into fresh session");f_rx_session_destroy(s);free(c);free(bad);puts("PASS profile contract complete");return 0;
}
