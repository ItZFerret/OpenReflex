#include "race.h"
/* Owned equivalents of RulesPopup.lua and AAALoading.lua. Canonical native
 * registry dispatch still runs for each Show/In/Out; these script states are
 * host presentation state, not fabricated original callback addresses. No Lua
 * UIContainer, localization/audio renderer, or synthetic native listener is
 * claimed. The copied required_ui_event is processed at In, before Out. */
static void cp_transition_reset(struct rx_session*s){
 memset(&s->career_transitions,0,sizeof s->career_transitions);
 s->career_transitions.size=sizeof s->career_transitions;
 s->career_transitions.version=RX_CAREER_TRANSITIONS_VERSION;
 s->career_transitions.generation=s->career_presentation.generation;
 s->career_rules_activity=0;
}
static uint64_t cp_script_wipe(struct rx_session*s,const char*name){
 RX_FRAME_ENTER(F,64);cp_text(F,name);
 const uint32_t text=F;
 rx_addr channel=rx_ui_event_channel();cp_require(channel!=0,"script ShowPageWipe channel missing");
 /*Same real typed emission as Single's ShowPageWipe. Only one projection is
  * enqueued, and no completion callback is invoked here. */
 race_raise_event(channel,0x941e9c,0x9cdb68,&text,1);
 RX_FRAME_LEAVE(F,64);
 const rx_career_host_input input={RX_CAREER_HOST_PAGE_WIPE,name,0,0,0};uint64_t id;
 cp_require(rx_career_host_queue_enqueue(&g_career_public_queue,s->career_presentation.generation,0,&input,&id)==RX_CAREER_HOST_OK,
  "script ShowPageWipe queue rejected; session must be aborted");
 return id;
}
static uint32_t cp_rules_show(void*context,rx_addr activity,rx_addr text){
 struct rx_session*s=context;rx_career_transition_status*t=&s->career_transitions;
 if(s!=g_career_live_session||!(s->career_capabilities&RX_CAREER_PRESENT_RULES)||
    (t->rules_state!=RX_CAREER_RULES_NONE&&t->rules_state!=RX_CAREER_RULES_FINISHED)||
    (t->loading_state!=RX_CAREER_LOADING_INACTIVE&&t->loading_state!=RX_CAREER_LOADING_FINISHED)||
    !U32(activity,0xff068))return 0;
 char name[64];if(!cp_copy_guest(text,name,sizeof name)||!name[0])return 0;
 rx_career_host_input input={RX_CAREER_HOST_UI_EVENT,"ShowRaceRulesPopup",0,0,0};uint64_t id;
 if(rx_career_host_queue_enqueue(&g_career_public_queue,s->career_presentation.generation,0,&input,&id))return 0;
 t->rules_state=RX_CAREER_RULES_REQUESTED;t->rules_request_id=id;t->rules_wipe_id=0;t->do_not_show_chosen=0;
 memset(t->rules_type,0,sizeof t->rules_type);memcpy(t->rules_type,name,strlen(name)+1);
 t->required_ui_event[0]=0;s->career_rules_activity=activity;return 1;
}
static void cp_loading_entered(void*context){
 struct rx_session*s=context;rx_career_transition_status*t=&s->career_transitions;
 cp_require(s==g_career_live_session&&s->career_phase==RX_CAREER_PHASE_STARTED&&
  (t->loading_state==RX_CAREER_LOADING_INACTIVE||t->loading_state==RX_CAREER_LOADING_FINISHED),
  "authentic loading page entry in incompatible session");
 /*No currently selected owner calls this. It is the explicit extension point
  * after the full005a8030 + game states9/10 closure, not a public UI toggle. */
 t->loading_state=RX_CAREER_LOADING_ACTIVE;t->loading_wipe_id=0;
}
static uint32_t cp_loading_goto_hub(struct rx_session*s){
 rx_career_transition_status*t=&s->career_transitions;
 if(t->loading_state!=RX_CAREER_LOADING_ACTIVE)return 0;
 cp_require(t->rules_state==RX_CAREER_RULES_NONE||t->rules_state==RX_CAREER_RULES_FINISHED,
  "AAALoading and Rules cannot own the same page transition");
 t->loading_state=RX_CAREER_LOADING_WIPING;t->loading_wipe_id=cp_script_wipe(s,"AAALoading");return 1;
}
static rx_addr cp_rules_world_activity(struct rx_session*s){
 rx_addr context=U32(0,0xa95844);if(!context)return 0;
 rx_addr alternate=U32(context,0x20),normal=U32(context,0x1c),world=0;
 if(alternate&&U32(alternate,0x34)==1)world=U32(s->game,0x5a54);
 else if(normal&&U32(normal,0x34)==1)world=U32(s->game,0x20c8);
 if(!world||I32(world,0x21da8)<=0||!U32(world,0x21da4))return 0;
 return U32(U32(world,0x21da4),0);
}
static int cp_rules_accept(struct rx_session*s,uint64_t id,uint32_t skip){
 rx_career_transition_status*t=&s->career_transitions;
 if(!id||id!=t->rules_request_id)return RX_CAREER_NOT_FOUND;
 if(t->rules_state>=RX_CAREER_RULES_WIPING&&t->rules_state<=RX_CAREER_RULES_FINISHED)
  return skip==t->do_not_show_chosen?RX_CAREER_REPEAT:RX_CAREER_CONFLICT;
 if(t->rules_state!=RX_CAREER_RULES_SHOWN||rx_career_host_queue_pending(&g_career_public_queue))return RX_CAREER_BAD_STATE;
 if(cp_rules_world_activity(s)!=s->career_rules_activity||!s->career_rules_activity||
    !U32(s->career_rules_activity,0xff068))return RX_CAREER_BAD_STATE;
 /*Consume explicit user choice before native mutation: a fault cannot replay
  * an incomplete table change or a partially emitted wipe. */
 t->do_not_show_chosen=skip;t->rules_state=RX_CAREER_RULES_WIPING;
 if(skip){
  RX_FRAME_ENTER(F,80);cp_text(F,"Rule_");cp_text(F+5,t->rules_type);
  ACT_CALL(1,rx_player_event_status_set(s->career_controller,F,1));RX_FRAME_LEAVE(F,80);
 }
 t->rules_wipe_id=cp_script_wipe(s,"RulesPopup");return RX_CAREER_OK;
}
static int cp_rules_ended(struct rx_session*s,uint64_t id){
 rx_career_transition_status*t=&s->career_transitions;
 if(!id||id!=t->rules_wipe_id)return RX_CAREER_NOT_FOUND;
 if(t->rules_state==RX_CAREER_RULES_ENDED||t->rules_state==RX_CAREER_RULES_FINISHED)return RX_CAREER_REPEAT;
 if(t->rules_state!=RX_CAREER_RULES_HIDE_REQUESTED)return RX_CAREER_OUT_OF_ORDER;
 if(!s->career_rules_activity||cp_rules_world_activity(s)!=s->career_rules_activity||
    !U32(s->career_rules_activity,0xff068))return RX_CAREER_BAD_STATE;
 t->rules_state=RX_CAREER_RULES_ENDED;t->required_ui_event[0]=0;
 ACT_CALL(0,rx_app_pre_activity_rules_page_done());return RX_CAREER_OK;
}
static int cp_loading_hub_entered(struct rx_session*s,uint64_t id){
 rx_career_transition_status*t=&s->career_transitions;
 if(!id||id!=t->loading_wipe_id)return RX_CAREER_NOT_FOUND;
 if(t->loading_state==RX_CAREER_LOADING_HUB_ENTERED||t->loading_state==RX_CAREER_LOADING_FINISHED)return RX_CAREER_REPEAT;
 if(t->loading_state!=RX_CAREER_LOADING_ROUTE_HUB)return RX_CAREER_OUT_OF_ORDER;
 /*AAALoading.OnPageWipeTransition sends RoutedGotoHub; there is no native
  * cleanup/CompleteEvent call in this handler. Hub rendering is the caller's
  * job. Do not run destructive005a8030 again on this acknowledgement. */
 t->loading_state=RX_CAREER_LOADING_HUB_ENTERED;t->required_ui_event[0]=0;
 /* RoutedGotoHub: the in-game graph's hub page, which routes by UIRoutingMessage (the Heat format's next gate selection). The
  * product's hub is a host request, as the series event's own GotoHub (cp_hub). */
 if(cp_manager_heat())cp_require(rx_career_presentation_hub(&s->career_presentation)!=0,"RoutedGotoHub hub request rejected");
 return RX_CAREER_OK;
}
static int cp_transition_wipe(struct career_public_call*c){
 struct rx_session*s=c->s;rx_career_transition_status*t=&s->career_transitions;
 int rules=c->id&&c->id==t->rules_wipe_id,loading=c->id&&c->id==t->loading_wipe_id;
 if(c->value==RX_CAREER_HOST_WIPE_OUT_COMPLETE){
  if(rules&&t->rules_state!=RX_CAREER_RULES_ENDED&&t->rules_state!=RX_CAREER_RULES_FINISHED)return RX_CAREER_OUT_OF_ORDER;
  if(loading&&t->loading_state!=RX_CAREER_LOADING_HUB_ENTERED&&t->loading_state!=RX_CAREER_LOADING_FINISHED)return RX_CAREER_OUT_OF_ORDER;
 }
 int rc=rx_career_presentation_wipe_milestone(&s->career_presentation,c->generation,c->id,c->kind,c->name,c->value);
 if(rc!=RX_CAREER_OK)return rc; /*repeat never replays a script handler*/
 if(rules){
  if(c->value==RX_CAREER_HOST_WIPE_IN_COMPLETE){
   cp_require(t->rules_state==RX_CAREER_RULES_WIPING,"Rules transition subscription state");
   t->rules_state=RX_CAREER_RULES_HIDE_REQUESTED;strcpy(t->required_ui_event,"HideRaceRulesPopup");
  }else t->rules_state=RX_CAREER_RULES_FINISHED;
 }else if(loading){
  if(c->value==RX_CAREER_HOST_WIPE_IN_COMPLETE){
   cp_require(t->loading_state==RX_CAREER_LOADING_WIPING,"AAALoading transition subscription state");
   t->loading_state=RX_CAREER_LOADING_ROUTE_HUB;strcpy(t->required_ui_event,"RoutedGotoHub");
  }else t->loading_state=RX_CAREER_LOADING_FINISHED;
 }
 return RX_CAREER_OK;
}
static void cp_transition_ack(struct rx_session*s,uint64_t id,uint32_t accepted){
 rx_career_transition_status*t=&s->career_transitions;
 if(id==t->rules_request_id&&t->rules_state==RX_CAREER_RULES_REQUESTED)
  t->rules_state=accepted?RX_CAREER_RULES_SHOWN:RX_CAREER_RULES_REJECTED;
 if(id==t->rules_wipe_id&&!accepted)t->rules_state=RX_CAREER_RULES_REJECTED;
 /*Rejected loading wipe leaves the page active only as an explicit terminal
  * failure: no second GotoHub can retry a partially handled native request. */
 if(id==t->loading_wipe_id&&!accepted){t->loading_state=RX_CAREER_LOADING_REJECTED;t->required_ui_event[0]=0;}
}
