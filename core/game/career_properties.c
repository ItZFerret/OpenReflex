/* PRODUCT boundary, not a port of original UI registry/events/packages. */
#include "career_properties.h"
#ifndef RX_ORACLE
#include "build_activity_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
typedef struct owned_property {struct owned_property*next;uint32_t type,value;size_t key_length,text_length;char data[];} owned_property;
static owned_property* entries;
static uint32_t active;
/* The Race & Ride challenges Free Ride (and a challenge started in it) run with the UI property InCareerMode set (MainMenu:CareerMode sets
 * it before RaceAndRideChallenges:OnAcceptChallenge starts the Free Ride). An exhibition session has no career store: the host declares
 * it before the event load (rx_session_event_in_career_mode) and the inactive read answers it as it answers the spec's values. */
static uint32_t exhibition_in_career_mode;
void rx_owned_property_set_in_career_mode(uint32_t value) { exhibition_in_career_mode = value; }
/* The UI property WaitForUI the host holds while one of its own pages is up over the running world: PreChallengePopup:Start sets it to 1
 * ("Tell game it has to wait for the UI (activities are not allowed to start)"), PreChallengePopup:End to 0; Activity::vf25 (Pre, 00558ce0)
 * is its only reader. Exhibition sessions only (a career session's own store answers); cleared with the store. */
static uint32_t exhibition_wait_for_ui;
void rx_owned_property_set_wait_for_ui(uint32_t value) { exhibition_wait_for_ui = value; }
static unsigned fold(unsigned c){return c>='A'&&c<='Z'?c+32:c;}
static size_t game_length(rx_addr a){size_t n=0;while(U8(a,(uint32_t)n)){if(n==UINT32_MAX-a)act_unreached(0x5dc290,"property key address overflow");++n;}return n;}
static int equal(rx_addr key,const char*s){for(size_t i=0;;i++){unsigned a=U8(key,(uint32_t)i),b=(unsigned char)s[i];if(fold(a)!=fold(b))return 0;if(!a)return 1;}}
static int exact(rx_addr key,const char*s){for(size_t i=0;;i++){unsigned a=U8(key,(uint32_t)i),b=(unsigned char)s[i];if(a!=b)return 0;if(!a)return 1;}}
static owned_property** locate(rx_addr key){owned_property**p=&entries;while(*p&&!equal(key,(*p)->data))p=&(*p)->next;return p;}
static char* text_of(owned_property*p){return p->data+p->key_length+1;}
void rx_owned_properties_reset(void){while(entries){owned_property*p=entries;entries=p->next;free(p);}}
void rx_owned_properties_dispose(void){rx_owned_properties_reset();active=0;exhibition_wait_for_ui=0;}
void rx_owned_properties_begin(void){rx_owned_properties_dispose();active=1;}
uint32_t rx_owned_properties_active(void){return active;}
static void store(rx_addr key,uint32_t type,uint32_t value,rx_addr text){
 const size_t k=game_length(key);size_t n=0;if(type==2)while(n<31&&U8(text,(uint32_t)n))n++;
 if(k>SIZE_MAX-sizeof(owned_property)-n-2)act_unreached(0x5dc290,"property allocation overflow");
 owned_property*p=malloc(sizeof(*p)+k+1+n+1);if(!p)act_unreached(0x5dc290,"property allocation failed");
 p->type=type;p->value=value;p->key_length=k;p->text_length=n;
 for(size_t i=0;i<k;i++)p->data[i]=(char)U8(key,(uint32_t)i);p->data[k]=0;
 char*s=text_of(p);for(size_t i=0;i<n;i++)s[i]=(char)U8(text,(uint32_t)i);s[n]=0;
 owned_property**slot=locate(key);owned_property*old=*slot;p->next=old?old->next:0;*slot=p;free(old);
}
void rx_owned_property_set_int(rx_addr key,uint32_t value){
 if(active){store(key,1,value,0);return;}
 /*Retain exactly the existing exhibition setter behavior while inactive.*/
 rx_event_spec e=*rx_event_spec_get();
 if(exact(key,"EventRiders"))e.riders=(int32_t)value;
 else if(exact(key,"AIDifficulty"))e.ai_difficulty=(int32_t)value;
 else return;
 rx_event_set_spec(&e);
}
void rx_owned_property_set_string(rx_addr key,rx_addr value){if(active)store(key,2,0,value);}
void rx_owned_property_erase(rx_addr key){if(!active)return;owned_property**p=locate(key);if(*p){owned_property*old=*p;*p=old->next;free(old);}}
uint32_t rx_owned_property_get_int(rx_addr key,rx_addr out,uint32_t include_wait){
 if(active){owned_property*p=*locate(key);if(!p)return 0;uint32_t value=p->value;
  if(p->type==2){ACT_FRAME(F,32,0);for(size_t i=0;i<=p->text_length;i++)SU8(F,(uint32_t)i,(uint8_t)text_of(p)[i]);value=(uint32_t)crt_atoi(F);ACT_END(F);}
  SU32(out,0,value);return 1;
 }
 const rx_event_spec*e=rx_event_spec_get();int32_t v;
 if(exact(key,"AIDifficulty"))v=e->ai_difficulty;
 else if(exact(key,"EventRiders"))v=e->riders;
 else if(exact(key,"VehicleClass"))v=e->vehicle_class;
 else if(exact(key,"TrackId"))v=e->track_id;
 else if(exact(key,"EventLocationId"))v=e->location_id;
 else if(exact(key,"EventTypeId"))v=e->event_type_id;
 else if(exact(key,"EventDuration")){if(!e->duration_type[0])return 0;v=e->duration;}
 else if(exact(key,"EventHasLaps"))v=strcmp(e->duration_type,"Minutes")!=0;
 else if(exact(key,"EventHasTimer"))v=strcmp(e->duration_type,"Minutes")==0;
 else if(exact(key,"EventHasRounds"))v=0;
 else if(include_wait&&exact(key,"WaitForUI"))v=U32(U32(0xa946b8,0),0x18)!=2||exhibition_wait_for_ui!=0;
 else if(exact(key,"InCareerMode")){
  if(!exhibition_in_career_mode)return 0;   /* absent, as before a front-end page sets it */
  v=1;
 }
 else return 0;
 SU32(out,0,(uint32_t)v);return 1;
}
uint32_t rx_owned_property_get_string(rx_addr key,rx_addr out,uint32_t capacity){
 const char*s;char numeric[12];owned_property*p=0;
 if(active){p=*locate(key);if(!p)return 0;if(p->type==2)s=text_of(p);else{snprintf(numeric,sizeof numeric,"%d",(int32_t)p->value);s=numeric;}}
 else{const rx_event_spec*e=rx_event_spec_get();
  if(exact(key,"EventType"))s=e->event_type;
  else if(exact(key,"EventTypeActivation"))s=e->event_activation[0]?e->event_activation:"FreeRide";
  else if(exact(key,"EventLocation"))s=e->location;
  else if(exact(key,"EventDurationType")){if(!e->duration_type[0])return 0;s=e->duration_type;}
  else if(exact(key,"PlayerMode"))s=e->player_mode;else return 0;
 }
 /*Original requires positive capacity; reject invalid zero without underwrite.*/
 if(!capacity)return 0;
 uint32_t i=0;if(active){size_t n=strlen(s);for(;i+1<capacity;i++)SU8(out,i,i<n?(uint8_t)s[i]:0);}
 else{for(;i+1<capacity&&s[i];i++)SU8(out,i,(uint8_t)s[i]);}
 SU8(out,active?capacity-1:i,0);return 1;
}
#endif
