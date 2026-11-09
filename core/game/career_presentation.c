#include "career_presentation.h"
#include <string.h>
static int ready(const rx_career_presentation*p){return p&&p->active;}
static int name_copy(rx_career_presentation*p,uint32_t address,char*out){
 if(!ready(p)||!address)return 0;
 memset(out,0xff,RX_CAREER_HOST_TEXT_CAPACITY);
 return p->services.copy_name(p->services.context,address,out,RX_CAREER_HOST_TEXT_CAPACITY)&&
  memchr(out,0,RX_CAREER_HOST_TEXT_CAPACITY)&&out[0];
}
static uint32_t enqueue(rx_career_presentation*p,const rx_career_host_input*input){
 if(!ready(p))return 0;
 uint64_t id;return rx_career_host_queue_enqueue(p->queue,p->generation,0,input,&id)==RX_CAREER_HOST_OK;
}
rx_career_host_result rx_career_presentation_begin(rx_career_presentation*p,rx_career_host_queue*q,const rx_career_presentation_services*s){
 if(!p||!q||!s||!s->copy_name||!s->route_available||!s->fade_complete||
    !s->page_wipe_transition||!s->page_wipe_complete)return RX_CAREER_HOST_INVALID;
 if(p->active)return RX_CAREER_HOST_BUSY;
 uint64_t generation;rx_career_host_result r=rx_career_host_queue_begin(q,&generation);if(r)return r;
 p->queue=q;p->generation=generation;p->services=*s;p->active=1;p->dispatching=0;return RX_CAREER_HOST_OK;
}
rx_career_host_result rx_career_presentation_end(rx_career_presentation*p,uint32_t abort_pending){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;
 if(p->dispatching)return RX_CAREER_HOST_BUSY;
 if(abort_pending>1)return RX_CAREER_HOST_INVALID;
 rx_career_host_result r=abort_pending?rx_career_host_queue_abort(p->queue):rx_career_host_queue_end(p->queue);
 if(!r){p->active=0;memset(&p->services,0,sizeof p->services);p->queue=NULL;}return r;
}
uint32_t rx_career_presentation_ui_event(void*context,uint32_t address){
 rx_career_presentation*p=context;char name[RX_CAREER_HOST_TEXT_CAPACITY];if(!name_copy(p,address,name))return 0;
 /* These carry typed animation payloads; a name-only callback cannot fulfill them. */
 if(!strcmp(name,"ShowPageWipe")||!strcmp(name,"FadeOut"))return 0;
 rx_career_host_input in={RX_CAREER_HOST_UI_EVENT,name,0,0,0};return enqueue(p,&in);
}
uint32_t rx_career_presentation_route_accept(void*context,uint32_t address){
 rx_career_presentation*p=context;char name[RX_CAREER_HOST_TEXT_CAPACITY];
 return name_copy(p,address,name)&&p->services.route_available(p->services.context,name)!=0;
}
uint32_t rx_career_presentation_ui_route(void*context,uint32_t address){
 rx_career_presentation*p=context;char name[RX_CAREER_HOST_TEXT_CAPACITY];if(!name_copy(p,address,name))return 0;
 if(!p->services.route_available(p->services.context,name))return 0;
 rx_career_host_input in={RX_CAREER_HOST_UI_ROUTE,name,0,0,0};return enqueue(p,&in);
}
uint32_t rx_career_presentation_hub(void*context){rx_career_host_input in={RX_CAREER_HOST_HUB,NULL,0,0,0};return enqueue(context,&in);}
uint32_t rx_career_presentation_checkpoint(void*context,uint32_t controller){rx_career_host_input in={RX_CAREER_HOST_CHECKPOINT,NULL,0,0,controller};return enqueue(context,&in);}
rx_career_host_result rx_career_presentation_fade(rx_career_presentation*p,uint32_t bits,uint32_t color){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;
 rx_career_host_input in={RX_CAREER_HOST_FADE_OUT,NULL,0,color,0};memcpy(&in.seconds,&bits,sizeof bits);
 uint64_t id;return rx_career_host_queue_enqueue(p->queue,p->generation,0,&in,&id);
}
rx_career_host_result rx_career_presentation_next(rx_career_presentation*p,uint64_t generation,rx_career_host_request*out){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;return rx_career_host_queue_next(p->queue,generation,out);
}
rx_career_host_result rx_career_presentation_ack(rx_career_presentation*p,uint64_t generation,uint64_t id,uint32_t kind,uint32_t accepted){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;
 if(p->dispatching)return RX_CAREER_HOST_BUSY;
 rx_career_host_result r=rx_career_host_queue_ack(p->queue,generation,id,kind,accepted);
 if(r==RX_CAREER_HOST_OK&&accepted&&kind==RX_CAREER_HOST_FADE_OUT){
  p->dispatching=1;p->services.fade_complete(p->services.context);p->dispatching=0;
 }
 return r;
}
rx_career_host_result rx_career_presentation_page_wipe(rx_career_presentation*p,uint32_t guest_name){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;
 char name[RX_CAREER_HOST_TEXT_CAPACITY];
 if(!name_copy(p,guest_name,name))return RX_CAREER_HOST_INVALID;
 rx_career_host_input in={RX_CAREER_HOST_PAGE_WIPE,name,0,0,0};uint64_t id;
 return rx_career_host_queue_enqueue(p->queue,p->generation,0,&in,&id);
}
rx_career_host_result rx_career_presentation_wipe_milestone(rx_career_presentation*p,
 uint64_t generation,uint64_t id,uint32_t kind,const char*name,uint32_t milestone){
 if(!ready(p))return RX_CAREER_HOST_INACTIVE;
 if(p->dispatching)return RX_CAREER_HOST_BUSY;
 rx_career_host_request copy;
 rx_career_host_result r=rx_career_host_queue_wipe_milestone(p->queue,
  generation,id,kind,name,milestone,&copy);
 if(r==RX_CAREER_HOST_OK){
  p->dispatching=1;
  if(milestone==RX_CAREER_HOST_WIPE_IN_COMPLETE)
   p->services.page_wipe_transition(p->services.context,copy.name);
  else p->services.page_wipe_complete(p->services.context,copy.name);
  p->dispatching=0;
 }
 return r;
}
