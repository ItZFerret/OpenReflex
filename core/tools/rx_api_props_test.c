/* Read-only props acceptance. No synthetic lap or trigger mutation.
 * rx_api_props_test <dylib> <extracted root> [racing frames (180)]
 * Default event is the API's original event. Observe only; natural transitions are reported, not required/faked. */
#include "rx_api.h"
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n=(__typeof__(&n))dlsym(lib,#n); if(!f_##n)return 2
#define CHECK(c) do { if(!(c)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#c);rc=1;goto done;} } while(0)
int main(int argc,char**argv){
 if(argc<3)return 2;alarm(180);int rc=0;rx_session*s=NULL;rx_prop *a=NULL,*b=NULL,*previous=NULL;
 void*lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());return 2;}
 LOAD(rx_session_create);LOAD(rx_session_destroy);LOAD(rx_session_event_default);LOAD(rx_session_event_load);
 LOAD(rx_session_advance_dt);LOAD(rx_session_get_state);LOAD(rx_session_props);LOAD(rx_session_frames);LOAD(rx_session_scene_objects);LOAD(rx_session_sizeof);
 CHECK(sizeof(rx_prop)==80);CHECK(f_rx_session_sizeof("rx_prop")==80);CHECK(f_rx_session_props(NULL,NULL,0)==-1);
 s=f_rx_session_create(argv[2]);CHECK(s);CHECK(f_rx_session_props(s,NULL,0)==0);
 rx_event_spec e;f_rx_session_event_default(&e);e.riders=4;CHECK(!f_rx_session_event_load(s,&e));
 int frames=argc>3?atoi(argv[3]):180,racing=0,initial=-1,visible=0,hidden=0,body=0,previous_count=0;
 int shown[4]={0},hidden_changes[4]={0},enabled[4]={0},disabled[4]={0},matrix_changes[4]={0};
 a=calloc(16384,sizeof *a);b=calloc(16384,sizeof *b);previous=calloc(16384,sizeof *previous);CHECK(a&&b&&previous);
 for(int frame=0;frame<3600+frames;frame++){
  usleep(3000); /* Yield during loading/countdown as well as racing. */
  CHECK(!f_rx_session_advance_dt(s,1.f/60));
  if(f_rx_session_get_state(s)<RX_SESSION_COUNTDOWN)continue;
  int n=f_rx_session_props(s,NULL,0);CHECK(n>0&&n<=16384);
  unsigned long long before=f_rx_session_frames(s);
  CHECK(f_rx_session_props(s,a,16384)==n);CHECK(f_rx_session_props(s,b,16384)==n);
  CHECK(before==f_rx_session_frames(s));
  for(int i=0;i<n;i++){
   CHECK(a[i].entity_id==b[i].entity_id&&a[i].vtable==b[i].vtable&&a[i].kind==b[i].kind);
   CHECK((a[i].flags&~4u)==(b[i].flags&~4u));CHECK(!memcmp(a[i].matrix,b[i].matrix,64));
   CHECK(!(a[i].flags&16u)||(a[i].flags&8u));CHECK(a[i].kind>=0&&a[i].kind<4);
   for(int j=0;j<16;j++)CHECK(isfinite(a[i].matrix[j]));
   if(initial<0){visible+=(a[i].flags&1)!=0;hidden+=(a[i].flags&1)==0;body+=(a[i].flags&8)!=0;}
   int old=-1;
   if(i<previous_count&&previous[i].entity_id==a[i].entity_id&&previous[i].vtable==a[i].vtable)old=i;
   else for(int j=0;j<previous_count;j++)if(previous[j].entity_id==a[i].entity_id&&previous[j].vtable==a[i].vtable){old=j;break;}
   if(old>=0){
    unsigned delta=previous[old].flags^a[i].flags;int k=a[i].kind;
    if(delta&1u){if(a[i].flags&1u)shown[k]++;else hidden_changes[k]++;}
    if(delta&16u){if(a[i].flags&16u)enabled[k]++;else disabled[k]++;}
    if(memcmp(previous[old].matrix,a[i].matrix,64))matrix_changes[k]++;
   }
  }
  if(initial<0){
   rx_scene_object* scene=calloc(16384,sizeof *scene);CHECK(scene);
   int ns=f_rx_session_scene_objects(s,scene,16384);CHECK(ns>0&&ns<16384);
   for(int i=0;i<n;i++){int found=0;for(int j=0;j<ns;j++)if(scene[j].vtable==a[i].vtable&&scene[j].entity_id==a[i].entity_id){CHECK(!memcmp(scene[j].matrix,a[i].matrix,64));found=1;break;}CHECK(found);}
   free(scene);
   initial=n;rx_prop guard[3];memset(guard,0xa5,sizeof guard);CHECK(f_rx_session_props(s,guard+1,1)==n);for(size_t j=0;j<sizeof(rx_prop);j++){CHECK(((unsigned char*)&guard[0])[j]==0xa5);CHECK(((unsigned char*)&guard[2])[j]==0xa5);}}
  memcpy(previous,a,(size_t)n*sizeof *a);previous_count=n;
  if(f_rx_session_get_state(s)==RX_SESSION_RACING&&++racing>=frames)break;
 }
 CHECK(racing>=frames);printf("PASS props=%d initial_visible=%d initial_hidden=%d with_body=%d racing_frames=%d (no lap/trigger mutation)\n",initial,visible,hidden,body,racing);
 for(int k=0;k<4;k++)printf("OBSERVED kind=%d shown=%d hidden=%d body_enabled=%d body_disabled=%d matrix_changes=%d\n",k,shown[k],hidden_changes[k],enabled[k],disabled[k],matrix_changes[k]);
 done:free(a);free(b);free(previous);if(s)f_rx_session_destroy(s);return rc;
}
