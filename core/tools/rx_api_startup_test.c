/* Read-only startup metadata contract. No legacy world or CRT cache is used.
 * clang -std=c11 -Icore/include core/tools/rx_api_startup_test.c -o <test>
 * RX_FULL_FRAME=1 <test> <librxcore.dylib> <install>
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define LOAD(n) __typeof__(&n) f_##n = (__typeof__(&n))dlsym(lib,#n); if (!f_##n) return 2;
int main(int argc,char **argv) {
 if(argc!=3)return 2;
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){puts(dlerror());return 2;}
 LOAD(rx_session_create);LOAD(rx_session_destroy);LOAD(rx_session_event_default);LOAD(rx_session_event_load);
 LOAD(rx_session_advance_dt);LOAD(rx_session_slots);LOAD(rx_session_appearance);LOAD(rx_session_sizeof);
 LOAD(rx_session_terrain);LOAD(rx_session_terrain_heights);LOAD(rx_session_last_error);
 if(sizeof(rx_appearance)!=272 || f_rx_session_sizeof("rx_appearance")!=272)return 1;
 for(int run=0;run<2;run++) {
  struct rx_session *s=f_rx_session_create(argv[2]);if(!s)return 1;
  rx_event_spec spec;f_rx_session_event_default(&spec);if(f_rx_session_event_load(s,&spec))return 1;
  rx_slot slots[16];int count=0;
  for(int i=0;i<60;i++){
   if(f_rx_session_advance_dt(s,1.0f/60)){puts(f_rx_session_last_error(s));return 1;}
   count=f_rx_session_slots(s,slots,16);if(count==spec.riders)break;
  }
  if(count!=spec.riders)return 1;
  for(int i=0;i<count;i++) {
   struct{uint64_t a;rx_appearance value;uint64_t b;} got;
   memset(&got,0xa5,sizeof got);rx_appearance again;
   if(f_rx_session_appearance(s,slots[i].slot,&got.value) || f_rx_session_appearance(s,slots[i].slot,&again))return 1;
   if(got.a!=UINT64_C(0xa5a5a5a5a5a5a5a5)||got.b!=got.a||memcmp(&got.value,&again,sizeof again))return 1;
   if(again.vehicle_model_id<0||again.part_count<=0||again.part_count>17||again.gear_count<=0||again.gear_count>16)return 1;
   printf("slot=%d local=%d model=%d owner=%d rider=%d parts=",slots[i].slot,slots[i].local,again.vehicle_model_id,again.rider_owner,again.owner_rider_id);
   for(int k=0;k<again.part_count;k++)printf("%d,",again.part_ids[k]);printf(" gear=");
   for(int k=0;k<again.gear_count;k++)printf("%d,",again.gear_ids[k]);puts("");
  }
  rx_appearance missing;if(f_rx_session_appearance(s,-1,&missing)!=2)return 1;
  rx_terrain_info info;float heights[16];
  if(f_rx_session_terrain(s,&info)||f_rx_session_terrain_heights(s,heights,0,0,4,4))return 1;
  printf("PASS run %d: %d slots, stable appearance/canaries, terrain %u first %.3f\n",run+1,count,info.samples,heights[0]);
  f_rx_session_destroy(s);
 }
 return 0;
}
