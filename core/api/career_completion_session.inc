/* Explicit finite owned completion boundaries. Canonical game owners retain all
 * standings/reward algorithms; this file validates host actions and observes
 * genuine dispatch. No elapsed time, forced phase or fake frontend state. */
static rx_career_host_request* cp_completion_request(struct rx_session*s,uint64_t id){
 rx_career_host_queue*q=&g_career_public_queue;
 for(uint32_t i=0;i<q->count;i++){
  rx_career_host_request*r=&q->records[(q->first+i)%RX_CAREER_HOST_QUEUE_CAPACITY];
  if(r->id==id&&r->generation==s->career_presentation.generation)return r;
 }
 return NULL;
}
static int cp_completion_identity(struct rx_session*s){
 const rx_addr m=0x9e2188,e=s->career_completion_event,a=s->career_completion_activity;
 if(s->career_phase!=RX_CAREER_PHASE_STARTED||!e||!a||!U32(m,0)||U32(m,0x20)!=s->career_completion_series||U32(m,0x190)!=1||U32(m,0x1c)!=s->career_controller||U32(m,0x18c)!=e||cp_rules_world_activity(s)!=a)return 0;
 if(U32(e,0)==CP_EVENT_MOTO_VTABLE)return cp_moto_final_identity(s,e,a);
 if(U32(e,0)==CP_EVENT_HEAT_VTABLE)return cp_heat_final_identity(s,e,a);
 /* The Single event of any Single-class format (SingleEvent, Overtake, Eliminator, SpeednStyle, Outrun, Faceoff, SectionRace):
  * the manager's format (+0x10) is the event's (+8), its race phase 5 is over and Post-complete, the activity is the one it set up. */
 return cp_manager_event_is(CP_EVENT_SINGLE_VTABLE) &&
        U32(e, 8) == U32(m, 0x10) &&
        U32(e, 0xc) == 5 &&
        U32(e, 0x14) == a &&
        U32(e, 0x30) != 0 &&
        cp_activity_layout(a, e) &&
        U32(a, 0x21c) == 4;
}
static void cp_completion_witness(void*context,uint64_t generation,rx_addr activity){
 struct rx_session*s=context;
 if(s!=g_career_live_session||s->state==RX_SESSION_ERROR||
    generation!=s->career_presentation.generation||!s->career_continue_submitted||
    !s->career_completion.ticket||activity!=s->career_completion_activity||
    !cp_completion_identity(s))return;
 s->career_completion.post_state_observed=1;
}
static int cp_completion_arm(struct rx_session*s,rx_addr activity){
 if(s->career_completion.ticket)return RX_CAREER_BAD_STATE;
 const rx_addr e=U32(0x9e2188,0x18c),channel=U32(activity,0x290);
 if(!rx_career_completion_bind(s,s->career_presentation.generation,channel,activity,cp_completion_witness))
  return RX_CAREER_UNSUPPORTED;
 s->career_completion_event=e;s->career_completion_activity=activity;
 s->career_completion_series=U32(0x9e2188,0x20);
 /* This finite session supports exactly one launch; generation disambiguates
  * ticket1 across destroyed/recreated sessions without exposing guest identity. */
 s->career_completion.ticket=1;return RX_CAREER_OK;
}
static uint32_t cp_completion_property(rx_addr name){
 RX_FRAME_ENTER(F,4);SU32(F,0,0);uint32_t have=rx_owned_property_get_int(name,F,0);
 uint32_t value=have?U32(F,0):0;RX_FRAME_LEAVE(F,4);return value;
}
static int cp_completion_main_event(void){
 RX_FRAME_ENTER(F,64);uint32_t have=rx_owned_property_get_string(0x94c99c,F,64);
 int result=have&&!crt_strcmp(F,0x94c118);RX_FRAME_LEAVE(F,64);return result;
}
/* SeriesEventStatus "Failed" (0x0094beac): the Single event's vf13 005affb0 sets it for an Outrun race the local rider did not
 * finish (its driver's + 0x1128). */
static int cp_completion_failed_event(void) {
    RX_FRAME_ENTER(F, 64);
    const uint32_t have = rx_owned_property_get_string(0x94c99c, F, 64);
    const int result = have && !crt_strcmp(F, 0x94beac);
    RX_FRAME_LEAVE(F, 64);
    return result;
}
static int cp_completion_unowned_vehicle(struct rx_session*s){
 const rx_addr pending=0x9e2188+0x78+0x78,models=cp_table(RX_TBL_DEFAULT_DB,0x93e180);
 for(uint32_t r=0;r<rx_tbl_row_count(pending);r++){
  const uint32_t asset=cp_word(pending,0x93e63c,r);uint32_t purchased;
  ACT_CALLR(purchased,1,rx_career_shop_is_purchased((int32_t)s->career_controller,(int32_t)asset));
  if(purchased)continue;
  for(uint32_t v=0;v<rx_tbl_row_count(models);v++)if(cp_word(models,0x93e63c,v)==asset)return 1;
 }
 return 0;
}
static int cp_completion_event_allowed(struct rx_session*s){
 if((s->career_capabilities&12u)!=12u||!s->career_completion.post_state_observed||s->career_completion.complete_event_state||
    s->records_finalized||!cp_completion_identity(s)||(!cp_completion_main_event()&&!cp_completion_failed_event())||
    !U32(0x9e2188,0x188)||!cp_completion_property(0x94d6a4)||
    rx_career_host_queue_pending(&g_career_public_queue))return 0;
 return 1;
}
static int cp_completion_frontend_allowed(struct rx_session*s,uint64_t id){
 rx_career_completion_status*t=&s->career_completion;
 if(t->complete_event_state!=2||t->records_state||t->frontend_ready||
    s->records_finalized||id!=t->frontend_request_id||!cp_completion_identity(s)||
    rx_career_host_queue_pending(&g_career_public_queue))return 0;
 rx_career_host_request*r=cp_completion_request(s,id);
 return r&&r->kind==RX_CAREER_HOST_UI_ROUTE&&r->delivered&&
  r->state==RX_CAREER_HOST_ACCEPTED&&!strcmp(r->name,t->frontend_route);
}
static int cp_completion_series_allowed(struct rx_session*s){
 rx_career_completion_status*t=&s->career_completion;
 return t->complete_event_state==2&&t->frontend_ready&&t->records_state==2&&
  s->records_finalized&&!t->complete_series_state&&cp_completion_identity(s)&&
  !rx_career_host_queue_pending(&g_career_public_queue);
}
static void cp_completion_reset(struct rx_session*s){
 memset(&s->career_completion,0,sizeof s->career_completion);
 s->career_completion.size=sizeof s->career_completion;s->career_completion.version=RX_CAREER_COMPLETION_VERSION;
 s->career_completion.generation=s->career_presentation.generation;
 s->career_completion_event=s->career_completion_activity=0;s->career_completion_receipts=0;s->career_completion_series=0;
}
static int cp_completion_ticket(struct rx_session*s,uint64_t ticket){
 return ticket&&ticket==s->career_completion.ticket?RX_CAREER_OK:RX_CAREER_NOT_FOUND;
}
/* The final Moto may complete: its identity holds and the series is still admitted, unlocked and without an unowned reward
 * vehicle (the same preflight as the Single formats: cp_series). */
static int cp_moto_final_reward_policy(struct rx_session* s) {
    if (!cp_moto_final_identity(s, s->career_completion_event, s->career_completion_activity)) return 0;
    rx_career_series_info info;
    cp_series(s, s->career_completion_series, &info);
    return info.supported && !info.locked && !info.unowned_reward_vehicle_count;
}
static int cp_complete_event(struct rx_session*s,uint64_t ticket){
 int rc=cp_completion_ticket(s,ticket);if(rc)return rc;
 rx_career_completion_status*t=&s->career_completion;
 if(t->complete_event_state)return t->complete_event_state==2?RX_CAREER_REPEAT:RX_CAREER_BAD_STATE;
 if(!cp_completion_event_allowed(s))return RX_CAREER_BAD_STATE;
 /* Both possible genuine result routes must have an owned presenter before
  * reward calculation mutates anything. Configure never grants these bits. */
 if((s->career_capabilities&(RX_CAREER_PRESENT_REWARDS|RX_CAREER_PRESENT_CAREER))!=
    (RX_CAREER_PRESENT_REWARDS|RX_CAREER_PRESENT_CAREER))return RX_CAREER_UNSUPPORTED;
 rx_career_series_info info;cp_series(s,s->career_completion_series,&info);
 if(U32(s->career_completion_event,0)==CP_EVENT_MOTO_VTABLE){if(!cp_moto_final_reward_policy(s)||cp_completion_unowned_vehicle(s))return RX_CAREER_UNSUPPORTED;}
 else if(!info.supported||info.unowned_reward_vehicle_count||cp_completion_unowned_vehicle(s))return RX_CAREER_UNSUPPORTED;
 if(g_career_public_queue.next_id==UINT64_MAX)return RX_CAREER_EXHAUSTED;
 const uint64_t before=g_career_public_queue.next_id;
 t->complete_event_state=1;
 if (cp_completion_failed_event()) {
  /* PostEventOverlay:OnPostStateComplete (InGame PostEventOverlay.lua), the Single formats' "Failed": CompleteSeriesEvent is NOT
   * called (no rewards); SwitchToFrontEnd, App.SendUIEvent("GotoCareerRaceAndRidePage") and FrontEndRoutingMessage
   * "GotoRaceAndRideEvents". The front end's CareerFrontEndLoadingPage:DoneLoading then runs CompleteSeries as for a finished event
   * (cp_complete_series). The route goes through the same owned presentation as CompleteSeriesEvent's. */
  RX_FRAME_ENTER(F, 64);
  cp_text(F, "FrontEndRoutingMessage");
  cp_text(F + 32, "GotoRaceAndRideEvents");
  rx_owned_property_set_string(F, F + 32);
  RX_FRAME_LEAVE(F, 64);
  cp_require(rx_career_presentation_ui_route(&s->career_presentation, 0x94d9bc /* "GotoCareerRaceAndRidePage" */) != 0,
             "Failed event's career route rejected");
 } else {
  ACT_CALL(0,rx_career_composition_complete_event());
 }
 cp_require(g_career_public_queue.next_id==before+1,"CompleteEvent expected one real route request");
 rx_career_host_request*r=cp_completion_request(s,before+1);
 cp_require(r&&r->kind==RX_CAREER_HOST_UI_ROUTE&&r->state==RX_CAREER_HOST_PENDING&&
  (!strcmp(r->name,"GotoRewardsPage")||!strcmp(r->name,"GotoCareerRaceAndRidePage")),"CompleteEvent actual route outside finite contract");
 cp_require(strlen(r->name)<sizeof t->frontend_route,"completion route exceeds copied DTO");
 t->frontend_request_id=r->id;strcpy(t->frontend_route,r->name);t->complete_event_state=2;
 return RX_CAREER_OK;
}
static int cp_frontend_ready(struct rx_session*s,uint64_t ticket,uint64_t route){
 int rc=cp_completion_ticket(s,ticket);if(rc)return rc;
 rx_career_completion_status*t=&s->career_completion;
 if(route!=t->frontend_request_id)return RX_CAREER_CONFLICT;
 if(t->records_state)return t->records_state==2?RX_CAREER_REPEAT:RX_CAREER_BAD_STATE;
 if(!cp_completion_frontend_allowed(s,route))return RX_CAREER_BAD_STATE;
 if(cp_completion_unowned_vehicle(s))return RX_CAREER_UNSUPPORTED;
 /* Explicit host attestation of real owned frontend readiness. This does not
  * call or pretend to execute original SwitchToFrontEnd/loading states. */
 t->frontend_ready=1;t->records_state=1;
 rx_career_completion_unbind(s);
 struct finalize_records_call call={s,2};finalize_records_run(&call);
 cp_require(call.rc==0,"owned final-leave record settlement failed");
 s->records_finalized=1;t->records_state=2;return RX_CAREER_OK;
}
static int cp_complete_series(struct rx_session*s,uint64_t ticket){
 int rc=cp_completion_ticket(s,ticket);if(rc)return rc;
 rx_career_completion_status*t=&s->career_completion;
 if(t->complete_series_state)return t->complete_series_state==2?RX_CAREER_REPEAT:RX_CAREER_BAD_STATE;
 if(!cp_completion_series_allowed(s))return RX_CAREER_BAD_STATE;
 if(cp_completion_unowned_vehicle(s))return RX_CAREER_UNSUPPORTED;
 if(g_career_public_queue.next_id>UINT64_MAX-2)return RX_CAREER_EXHAUSTED;
 const uint64_t before=g_career_public_queue.next_id;
 t->complete_series_state=1;
 ACT_CALL(0,rx_career_composition_complete_series());
 cp_require(g_career_public_queue.next_id==before+2,"CompleteSeries must retain both actual checkpoint requests");
 for(uint32_t i=1;i<=2;i++){
  rx_career_host_request*r=cp_completion_request(s,before+i);
  cp_require(r&&r->kind==RX_CAREER_HOST_CHECKPOINT&&r->state==RX_CAREER_HOST_PENDING&&
   r->controller==s->career_controller,"CompleteSeries checkpoint identity mismatch");
 }
 cp_require(!U32(0x9e2188,0x18c)&&!U32(0x9e2188,0x190),"actual EndSeries cleanup did not complete");
 t->checkpoint_one_id=before+1;t->checkpoint_two_id=before+2;
 /* CareerFrontEndLoadingPage.DoneLoading no-vehicle branch: CompleteSeries,
  * then genuine EnableTourMode semantic body. No frontend route is invented. */
 rx_career_completion_enable_tour(0x9e2188);t->tour_enabled=1;
 t->complete_series_state=2;t->in_memory_settled=1;return RX_CAREER_OK;
}
static void cp_completion_receipt(struct rx_session*s,uint64_t id,int result){
 if(result!=RX_CAREER_OK&&result!=RX_CAREER_REPEAT)return;
 if(id==s->career_completion.checkpoint_one_id&&id)s->career_completion_receipts|=1;
 if(id==s->career_completion.checkpoint_two_id&&id)s->career_completion_receipts|=2;
}
static void cp_completion_status(struct rx_session*s,rx_career_completion_status*out){
 *out=s->career_completion;
 out->complete_event_allowed=(uint32_t)cp_completion_event_allowed(s);
 out->frontend_ready_allowed=(uint32_t)cp_completion_frontend_allowed(s,out->frontend_request_id);
 out->complete_series_allowed=(uint32_t)cp_completion_series_allowed(s);
 out->checkpoint_error=s->career_checkpoint_error;
 out->durable=out->in_memory_settled&&s->career_completion_receipts==3&&!s->career_checkpoint_error;
}
