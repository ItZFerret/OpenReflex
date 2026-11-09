#include "career_host_queue.h"
#include <string.h>
#include <math.h>
static rx_career_host_request* at(rx_career_host_queue*q,uint32_t i){return &q->records[(q->first+i)%RX_CAREER_HOST_QUEUE_CAPACITY];}
static rx_career_host_result current(const rx_career_host_queue*q,uint64_t g){if(!q)return RX_CAREER_HOST_INVALID;if(g!=q->generation)return RX_CAREER_HOST_STALE;if(!q->active)return RX_CAREER_HOST_INACTIVE;return RX_CAREER_HOST_OK;}
void rx_career_host_queue_init(rx_career_host_queue*q){if(q)memset(q,0,sizeof(*q));}
size_t rx_career_host_queue_pending(const rx_career_host_queue*q){if(!q)return 0;size_t n=0;for(uint32_t i=0;i<q->count;i++)n+=q->records[(q->first+i)%RX_CAREER_HOST_QUEUE_CAPACITY].state==RX_CAREER_HOST_PENDING;return n;}
rx_career_host_result rx_career_host_queue_begin(rx_career_host_queue*q,uint64_t*g){if(!q||!g)return RX_CAREER_HOST_INVALID;if(q->active)return RX_CAREER_HOST_BUSY;if(q->generation==UINT64_MAX)return RX_CAREER_HOST_EXHAUSTED;q->generation++;q->active=1;q->first=q->count=0;memset(q->records,0,sizeof q->records);*g=q->generation;return RX_CAREER_HOST_OK;}
rx_career_host_result rx_career_host_queue_end(rx_career_host_queue*q){if(!q)return RX_CAREER_HOST_INVALID;if(!q->active)return RX_CAREER_HOST_INACTIVE;if(rx_career_host_queue_pending(q))return RX_CAREER_HOST_BUSY;q->active=0;return RX_CAREER_HOST_OK;}
rx_career_host_result rx_career_host_queue_abort(rx_career_host_queue*q){if(!q)return RX_CAREER_HOST_INVALID;if(!q->active)return RX_CAREER_HOST_INACTIVE;for(uint32_t i=0;i<q->count;i++)if(at(q,i)->state==RX_CAREER_HOST_PENDING)at(q,i)->state=RX_CAREER_HOST_CANCELLED;q->active=0;return RX_CAREER_HOST_OK;}
static int payload(rx_career_host_request*r,const rx_career_host_input*i){
 if(!i||i->kind<RX_CAREER_HOST_UI_EVENT||i->kind>RX_CAREER_HOST_CHECKPOINT)return 0;r->kind=i->kind;
 const int named=i->kind==RX_CAREER_HOST_UI_EVENT||i->kind==RX_CAREER_HOST_UI_ROUTE||i->kind==RX_CAREER_HOST_PAGE_WIPE;
 if(named){if(!i->name||!i->name[0])return 0;size_t n=0;while(n<RX_CAREER_HOST_TEXT_CAPACITY&&i->name[n])n++;if(n==RX_CAREER_HOST_TEXT_CAPACITY)return 0;memcpy(r->name,i->name,n+1);}else if(i->name&&i->name[0])return 0;
 if(i->kind==RX_CAREER_HOST_FADE_OUT){if(!isfinite(i->seconds)||i->seconds<0||i->controller)return 0;r->seconds=i->seconds;r->color=i->color;}
 else if(i->seconds!=0||i->color)return 0;
 if(i->kind==RX_CAREER_HOST_CHECKPOINT){if(i->controller>=4)return 0;r->controller=i->controller;}else if(i->controller)return 0;
 return 1;
}
static int same(const rx_career_host_request*a,const rx_career_host_request*b){return a->kind==b->kind&&!strcmp(a->name,b->name)&&!memcmp(&a->seconds,&b->seconds,sizeof a->seconds)&&a->color==b->color&&a->controller==b->controller;}
rx_career_host_result rx_career_host_queue_enqueue(rx_career_host_queue*q,uint64_t g,uint64_t token,const rx_career_host_input*i,uint64_t*id){
 if(!id)return RX_CAREER_HOST_INVALID;rx_career_host_result status=current(q,g);if(status)return status;rx_career_host_request r;memset(&r,0,sizeof r);if(!payload(&r,i))return RX_CAREER_HOST_INVALID;
 if(token)for(uint32_t n=0;n<q->count;n++)if(at(q,n)->token==token){if(!same(at(q,n),&r))return RX_CAREER_HOST_CONFLICT;*id=at(q,n)->id;return RX_CAREER_HOST_REPEAT;}
 if(q->next_id==UINT64_MAX)return RX_CAREER_HOST_EXHAUSTED;
 if(q->count==RX_CAREER_HOST_QUEUE_CAPACITY){if(at(q,0)->state==RX_CAREER_HOST_PENDING)return RX_CAREER_HOST_FULL;memset(at(q,0),0,sizeof r);q->first=(q->first+1)%RX_CAREER_HOST_QUEUE_CAPACITY;q->count--;}
 r.generation=g;r.id=++q->next_id;r.token=token;r.state=RX_CAREER_HOST_PENDING;*at(q,q->count++)=r;*id=r.id;return RX_CAREER_HOST_OK;
}
rx_career_host_result rx_career_host_queue_next(rx_career_host_queue*q,uint64_t g,rx_career_host_request*out){if(!out)return RX_CAREER_HOST_INVALID;rx_career_host_result status=current(q,g);if(status)return status;for(uint32_t n=0;n<q->count;n++){rx_career_host_request*r=at(q,n);if(r->state==RX_CAREER_HOST_PENDING){r->delivered=1;*out=*r;return RX_CAREER_HOST_OK;}}return RX_CAREER_HOST_EMPTY;}
rx_career_host_result rx_career_host_queue_ack(rx_career_host_queue*q,uint64_t g,uint64_t id,uint32_t kind,uint32_t accepted){if(!id||accepted>1)return RX_CAREER_HOST_INVALID;rx_career_host_result status=current(q,g);if(status)return status;for(uint32_t n=0;n<q->count;n++){rx_career_host_request*r=at(q,n);if(r->id!=id)continue;if(kind!=r->kind)return RX_CAREER_HOST_WRONG_KIND;if(kind==RX_CAREER_HOST_PAGE_WIPE){if(accepted)return RX_CAREER_HOST_INVALID;if(r->wipe_milestone)return RX_CAREER_HOST_CONFLICT;}const uint32_t target=accepted?RX_CAREER_HOST_ACCEPTED:RX_CAREER_HOST_REJECTED;if(r->state!=RX_CAREER_HOST_PENDING)return r->state==target?RX_CAREER_HOST_REPEAT:RX_CAREER_HOST_CONFLICT;if(!r->delivered)return RX_CAREER_HOST_NOT_DELIVERED;r->state=target;return RX_CAREER_HOST_OK;}return RX_CAREER_HOST_NOT_FOUND;}

rx_career_host_result rx_career_host_queue_wipe_milestone(rx_career_host_queue*q,
 uint64_t g,uint64_t id,uint32_t kind,const char*name,uint32_t milestone,
 rx_career_host_request*out){
 if(!id||!out||!name||!name[0]||
    (milestone!=RX_CAREER_HOST_WIPE_IN_COMPLETE&&
     milestone!=RX_CAREER_HOST_WIPE_OUT_COMPLETE))return RX_CAREER_HOST_INVALID;
 size_t length=0;
 while(length<RX_CAREER_HOST_TEXT_CAPACITY&&name[length])length++;
 if(length==RX_CAREER_HOST_TEXT_CAPACITY)return RX_CAREER_HOST_INVALID;
 rx_career_host_result status=current(q,g);if(status)return status;
 for(uint32_t n=0;n<q->count;n++){
  rx_career_host_request*r=at(q,n);if(r->id!=id)continue;
  if(kind!=r->kind||kind!=RX_CAREER_HOST_PAGE_WIPE)return RX_CAREER_HOST_WRONG_KIND;
  if(strcmp(name,r->name))return RX_CAREER_HOST_CONFLICT;
  if(r->state==RX_CAREER_HOST_REJECTED||r->state==RX_CAREER_HOST_CANCELLED)
   return RX_CAREER_HOST_CONFLICT;
  if(!r->delivered)return RX_CAREER_HOST_NOT_DELIVERED;
  if(milestone<=r->wipe_milestone){*out=*r;return RX_CAREER_HOST_REPEAT;}
  if(milestone!=r->wipe_milestone+1)return RX_CAREER_HOST_OUT_OF_ORDER;
  r->wipe_milestone=milestone;
  if(milestone==RX_CAREER_HOST_WIPE_OUT_COMPLETE)r->state=RX_CAREER_HOST_ACCEPTED;
  *out=*r;return RX_CAREER_HOST_OK;
 }
 return RX_CAREER_HOST_NOT_FOUND;
}
