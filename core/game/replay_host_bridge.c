/* Product presentation transport; NOT an original game algorithm or render sink.
 * Serialized session-worker ownership; no locks/concurrent PInvoke supported. */
#include "replay_host_bridge.h"
#include "rx_career_api.h"
#include "rx_mem.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
_Static_assert(sizeof(rx_replay_request)==72,"replay copied request ABI");
_Static_assert(sizeof(rx_replay_view)==104,"replay copied view ABI");
struct rx_replay_request_node {rx_replay_request value;uint32_t delivered,applied;struct rx_replay_request_node*next;};
static int append(rx_replay_host_bridge*q,rx_replay_request*v){
 if(!q||!q->bound||!q->generation||!q->next_id||q->count==UINT32_MAX)return -1;
 struct rx_replay_request_node*n=calloc(1,sizeof *n);if(!n)return -1;
 v->size=sizeof *v;v->version=RX_REPLAY_API_VERSION;v->generation=q->generation;v->id=q->next_id;v->frame=q->frame;
 n->value=*v;if(q->tail)q->tail->next=n;else q->head=n;q->tail=n;q->count++;
 q->next_id=q->next_id==UINT64_MAX?0:q->next_id+1;return 0;
}
static int viewport(void*context,uint64_t generation,rx_addr game,int32_t count){
 rx_replay_host_bridge*q=context;if(!q||generation!=q->generation||!game||count!=1)return -1;
 rx_replay_request r={0};r.operation=RX_REPLAY_VIEWPORT;r.viewport_count=(uint32_t)count;return append(q,&r);
}
static int fade(void*context,uint64_t generation,uint32_t kind,float duration,const uint32_t*rgba){
 rx_replay_host_bridge*q=context;if(!q||generation!=q->generation||(kind!=2&&kind!=3)||(kind==2&&rgba)||(kind==3&&!rgba))return -1;
 rx_replay_request r={0};r.operation=RX_REPLAY_FADE;r.fade_kind=kind;memcpy(&r.duration_bits,&duration,4);
 if(rgba){r.color_mask=15;memcpy(r.rgba_bits,rgba,sizeof r.rgba_bits);}
 else {
  /* Read exactly the parameter block preceding original FadeIn. This is not
   * a synthetic renderer allocation/default; missing actual state rejects. */
  rx_addr context=U32(0,0x00a95844);if(!context)return -1;
  rx_addr scene=U32(context,0x1c+4*U32(context,4));if(!scene)return -1;
  rx_addr view=U32(scene,0x19b8);if(!view)return -1;
  rx_addr parameters=U32(view,0x70);if(!parameters)return -1;
  for(uint32_t i=0;i<4;i++)r.rgba_bits[i]=U32(parameters,0x30+4*i);
 }
 r.flags=1;return append(q,&r);
}
int rx_replay_host_begin(rx_replay_host_bridge*q,uint64_t generation){
 if(!q||!generation||q->bound||q->head)return RX_CAREER_BAD_STATE;
 rx_replay_presentation h={q,generation,viewport,fade};
 if(rx_replay_presentation_install(&h))return RX_CAREER_CONFLICT;
 memset(q,0,sizeof *q);q->bound=1;q->generation=generation;q->next_id=1;return RX_CAREER_OK;
}
void rx_replay_host_end(rx_replay_host_bridge*q){
 if(!q)return;if(q->bound)rx_replay_presentation_clear();
 struct rx_replay_request_node*n=q->head;while(n){struct rx_replay_request_node*next=n->next;free(n);n=next;}memset(q,0,sizeof *q);
}
void rx_replay_host_frame(rx_replay_host_bridge*q,uint64_t frame){if(q&&q->bound)q->frame=frame;}
static int identity(rx_replay_host_bridge*q,uint64_t generation){return !q||!q->bound?RX_CAREER_INACTIVE:generation!=q->generation?RX_CAREER_STALE:RX_CAREER_OK;}
int rx_replay_host_peek(rx_replay_host_bridge*q,uint64_t generation,uint32_t index,rx_replay_request*out){
 int rc=identity(q,generation);if(rc)return rc;if(!out||out->size!=sizeof *out||out->version!=RX_REPLAY_API_VERSION)return RX_CAREER_INVALID;
 struct rx_replay_request_node*n=q->head;while(n&&index){n=n->next;index--;}
 if(!n)return RX_CAREER_EMPTY;n->delivered=1;*out=n->value;return RX_CAREER_OK;
}
int rx_replay_host_next(rx_replay_host_bridge*q,uint64_t generation,rx_replay_request*out){return rx_replay_host_peek(q,generation,0,out);}
int rx_replay_host_ack(rx_replay_host_bridge*q,uint64_t generation,const rx_replay_request*copy,uint32_t milestone){
 int rc=identity(q,generation);if(rc)return rc;
 if(!copy||copy->size!=sizeof *copy||copy->version!=RX_REPLAY_API_VERSION||copy->generation!=generation||(milestone!=1&&milestone!=2))return RX_CAREER_INVALID;
 if(copy->id==q->last.id&&q->last.id){return memcmp(copy,&q->last,sizeof *copy)?RX_CAREER_CONFLICT:RX_CAREER_REPEAT;}
 if(!q->head)return RX_CAREER_NOT_FOUND;
 struct rx_replay_request_node*n=q->head;
 if(copy->id!=n->value.id)return RX_CAREER_OUT_OF_ORDER;
 if(memcmp(copy,&n->value,sizeof *copy))return RX_CAREER_CONFLICT;
 if(!n->delivered)return RX_CAREER_NOT_DELIVERED;
 if(milestone==RX_REPLAY_APPLIED){if(n->applied)return RX_CAREER_REPEAT;n->applied=1;return RX_CAREER_OK;}
 if(!n->applied)return RX_CAREER_OUT_OF_ORDER;
 q->last=n->value;q->head=n->next;if(!q->head)q->tail=NULL;q->count--;free(n);return RX_CAREER_OK;
}
