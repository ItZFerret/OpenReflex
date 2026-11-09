/* Included after career_public_session.inc: reuse existing identity/guard policy.
 * Product worker bridge, not original UI/renderer emulation. */
/* Exhibition presenter identity: the same checks as cp_entry, against the
 * presenter's own generation instead of the career's. */
static int rp_event_entry(struct rx_session*s,uint64_t generation){
 if(s->career_busy||t_guard_env)return RX_CAREER_BUSY;
 if(s->state==RX_SESSION_ERROR)return RX_CAREER_FAULT;
 if(!s->replay_host.bound)return RX_CAREER_INACTIVE;
 return s->replay_host.generation==generation?RX_CAREER_OK:RX_CAREER_STALE;
}
static int rp_entry(struct rx_session*s,uint64_t generation){
 if(s&&s==g_career_live_session&&session_ok(s)&&s->replay_event)return rp_event_entry(s,generation);
 int rc=cp_entry(s,generation,0);if(rc)return rc;
 if(!s->replay_host.bound)return RX_CAREER_INACTIVE;
 return s->replay_host.generation==generation?RX_CAREER_OK:RX_CAREER_STALE;
}
/* Exhibition events (rx_session_event_load). The original starts the end-of-race
 * replay for EVERY race that reaches Post: MXActivityRace Post sub-state 2
 * (00569a40) -> 006aa8d0 with ClipRecorderEnabled [0xd5a9a4] set by the engine
 * config, and the Waypoint finish (waypoint_activity_runtime.c). The replay is not
 * career-specific, so the session installs the same host bridge (ReplayWorker on
 * the Unity side) for a plain event. The generation is the presenter's own (one
 * per begin, never reused in the process), so a stale managed handle of an
 * earlier session is rejected exactly as a stale career generation. */
static uint64_t g_replay_event_generation RX_RESET_KEEP;
int rx_session_replay_begin_event(struct rx_session*s,uint64_t*generation){
 if(!generation)return RX_CAREER_INVALID;
 *generation=0;
 if(!s||s!=g_career_live_session||!session_ok(s))return RX_CAREER_INVALID;
 if(s->career_busy||t_guard_env)return RX_CAREER_BUSY;
 if(s->state==RX_SESSION_ERROR)return RX_CAREER_FAULT;
 /* Before the event exists: the host must be installed before the first frame. */
 if(s->career_phase||s->spec_loaded||s->frames||s->records_finalized||s->replay_event||s->replay_host.bound)
  return RX_CAREER_BAD_STATE;
 if(g_replay_event_generation==UINT64_MAX)return RX_CAREER_EXHAUSTED;
 const uint64_t next=g_replay_event_generation+1;
 int rc=rx_replay_host_begin(&s->replay_host,next);
 if(rc)return rc;
 g_replay_event_generation=next;
 s->replay_event=1;
 rx_replay_host_frame(&s->replay_host,s->frames);
 *generation=next;
 return RX_CAREER_OK;
}
int rx_session_replay_end_event(struct rx_session*s,uint64_t generation){
 if(!s||s!=g_career_live_session||!session_ok(s)||!s->replay_event)return RX_CAREER_INVALID;
 if(s->career_busy||t_guard_env)return RX_CAREER_BUSY;
 if(!s->replay_host.bound)return RX_CAREER_REPEAT;
 if(s->replay_host.generation!=generation)return RX_CAREER_STALE;
 /* Unpainted requests are dropped with the presenter; rx_session_advance_dt
  * refuses further frames (the replay would need the host again). */
 rx_replay_host_end(&s->replay_host);
 return RX_CAREER_OK;
}
int rx_session_replay_begin(struct rx_session*s,uint64_t generation){
 int rc=cp_entry(s,generation,0);if(rc)return rc;
 if(s->career_phase!=RX_CAREER_PHASE_CONFIGURED)return RX_CAREER_BAD_STATE;
 rc=rx_replay_host_begin(&s->replay_host,generation);
 if(!rc)rx_replay_host_frame(&s->replay_host,s->frames);
 return rc;
}
int rx_session_replay_next(struct rx_session*s,uint64_t generation,rx_replay_request*out){
 int rc=rp_entry(s,generation);return rc?rc:rx_replay_host_next(&s->replay_host,generation,out);
}
int rx_session_replay_peek(struct rx_session*s,uint64_t generation,uint32_t index,rx_replay_request*out){
 int rc=rp_entry(s,generation);return rc?rc:rx_replay_host_peek(&s->replay_host,generation,index,out);
}
int rx_session_replay_ack(struct rx_session*s,uint64_t generation,const rx_replay_request*copy,uint32_t milestone){
 int rc=rp_entry(s,generation);return rc?rc:rx_replay_host_ack(&s->replay_host,generation,copy,milestone);
}
struct rp_view_call{struct rx_session*s;rx_replay_view out;};
static void rp_view_run(void*opaque){
 struct rp_view_call*c=opaque;struct rx_session*s=c->s;rx_replay_view*out=&c->out;
 out->size=sizeof *out;out->version=RX_REPLAY_API_VERSION;out->generation=s->replay_host.generation;
 out->frame=s->frames;out->engine_dt_bits=U32(0,RX_ENGINE_GLOBAL+0x18);out->pending_requests=s->replay_host.count;
 out->active=U32(0,0x00a942b4)==2;
 if(!out->active)return;
 const rx_addr activity=U32(0,0x00a942b8);if(!activity)return;
 const rx_addr world=U32(activity,0x234);if(!world)return;
 /* Read the actual view Camera after7678b0: the director may win first,
  * then selected vehicle camera overrides only if world21de4 is set.
  * Canonical63dba0 stores that final Camera at U32(view,0). */
 const rx_addr scene=U32(world,0x20138);if(!scene)return;
 const rx_addr view=U32(scene,0x19b8);if(!view)return;
 out->camera_valid=camera_read(U32(view,0),&out->camera);
}
int rx_session_replay_view(struct rx_session*s,uint64_t generation,rx_replay_view*out){
 if(!out||out->size!=sizeof *out||out->version!=RX_REPLAY_API_VERSION)return RX_CAREER_INVALID;
 int rc=rp_entry(s,generation);if(rc)return rc;
 struct rp_view_call c={0};c.s=s;char captured[4096];s->career_busy=1;
 int sig=run_guarded(rp_view_run,&c,captured,sizeof captured);s->career_busy=0;
 if(sig){char message[512];describe_fault(message,sizeof message,"rx_session_replay_view",sig,captured);session_error(s,"%s",message);return RX_CAREER_FAULT;}
 *out=c.out;return RX_CAREER_OK;
}
