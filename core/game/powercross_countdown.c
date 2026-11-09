/* Owned presentation adapter for the original BeginningCountdownComponent.
 * The managed tree owns UIText/UIComponent base rendering and animations. This
 * owner ports its actual typed activity subscriptions and callback state, rather
 * than synthesizing countdown text from activity clocks. Original object offsets
 * +3a0/+3a4 retain their exact flag/timer meanings. Single session/thread only. */
#include "powercross_countdown.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "world_flag_holders.h"
#include "world_misc.h"
#include "race_activity_misc.h"
#include "crt.h"
#include <string.h>
#include <float.h>
static struct {rx_addr component,activity,channel;uint32_t wanted,subscribed,visible,events;int32_t step;} pc;
static void require_component(rx_addr self){if(!pc.component||self!=pc.component)act_unreached(0x5fb830,"countdown presentation component lifetime mismatch");}
/*005fb880 full widget-specific body: the managed adapter resolves STR_GO for0
 * and invariant decimal for nonzero, then UIText's SetText on the actual node.*/
void rx_powercross_countdown_step(rx_addr self,rx_addr activity,int32_t step){
 require_component(self);(void)activity;pc.step=step;pc.events++;
}
#ifndef RX_ORACLE   /* product-only presentation adapter (needs the managed component): the oracle runs the original */
RX_FUNCTION(0x005fb880,rx_powercross_countdown_step)
#endif
/*005fb830,RET8 (the old-state argument is ignored by the original).*/
void rx_powercross_countdown_enter(rx_addr self,rx_addr activity,uint32_t old){
 (void)old;rx_powercross_countdown_step(self,activity,3);
 pc.visible=U32(activity,0x238)!=6;SU32(self,0x3a0,0);
}
#ifndef RX_ORACLE   /* product-only presentation adapter (needs the managed component): the oracle runs the original */
RX_FUNCTION(0x005fb830,rx_powercross_countdown_enter)
#endif
/*005fb990,RET8: does not change text or visibility.*/
void rx_powercross_countdown_exit(rx_addr self,rx_addr activity,uint32_t next){
 require_component(self);(void)activity;(void)next;SU32(self,0x3a0,1);SF32(self,0x3a4,2.0f);pc.events++;
}
#ifndef RX_ORACLE   /* product-only presentation adapter (needs the managed component): the oracle runs the original */
RX_FUNCTION(0x005fb990,rx_powercross_countdown_exit)
#endif
/*005fb920 widget-specific tail, after base component/animation update. Test
 * before subtract, do not clamp or clear the exit flag after expiration.*/
void rx_powercross_countdown_component_tick(rx_addr self,float dt){
 require_component(self);
 if(U32(self,0x3a0)){if(F32(self,0x3a4)<=0.0f)pc.visible=0;SF32(self,0x3a4,F32(self,0x3a4)-dt);}
}
/* This adapter is not registered as005fb920: its UIComponent base call remains
 * managed OriginalUiTree.Tick. Never replace a full original function by a tail.*/

/*0060ba80 uses EventHandlerProxy<BeginningCountdownComponent,Activity*,int>:
 *9577f0 vf2=60f530, vf3=610ab0, vf4=610b40; +14 is the this adjustment.*/
void rx_powercross_proxy_call(rx_addr h,rx_addr event){
 /* the original's frame: push ebp; mov ebp, esp; sub esp, 0x10 (its callee's entry ESP is 0x20 below this one's; the
  * earlier ACT_FRAME(F, 0, 2) called it 0xc higher: the UI callbacks' stack strings moved, w-freestyle) */
 ACT_FRAME(F,0x10,1);rx_addr payload=U32(event,0);rx_addr self=U32(h,0xc)+U32(h,0x14);
 rx_addr callback=U32(h,0x10);typedef void(*fn)(rx_addr,rx_addr,uint32_t);fn f=RX_FN(callback,fn);
 if(f){ACT_CALL(2,f(self,U32(payload,0),U32(payload,4)));}
 else{uint32_t args[2]={U32(payload,0),U32(payload,4)};rx_call_original(callback,RX_THISCALL,self,2,args);}
 ACT_END(F);
}
RX_FUNCTION(0x0060f530,rx_powercross_proxy_call)
rx_addr rx_powercross_proxy_clone(rx_addr h){
 rx_addr out=rx_reflex_aligned_alloc(0x18);if(out){SU32(out,0,0x9577f0);SU32(out,4,0);SU32(out,8,crt_type_info_name(0x9cf5b4,0x9e1a7c));for(uint32_t x=0xc;x<=0x14;x+=4)SU32(out,x,U32(h,x));}return out;
}
RX_FUNCTION(0x00610ab0,rx_powercross_proxy_clone)
uint32_t rx_powercross_proxy_equal(rx_addr self,rx_addr key){
 rx_addr cast=crt_rtdynamiccast(key,0,0x9cbc8c,0x9d6718,0);
 return cast&&U32(self,0x10)==U32(cast,0x10)&&(!U32(self,0x10)||U32(self,0x14)==U32(cast,0x14))&&U32(self,0xc)==U32(cast,0xc);
}
RX_FUNCTION(0x00610b40,rx_powercross_proxy_equal)
rx_addr rx_powercross_proxy_delete(rx_addr self,uint32_t flags){
 /*00402060 EventHandler::~EventHandler restores its base vtable.*/
 SU32(self,0,0x937244);SU32(self,4,0);if(flags&1)rx_reflex_aligned_free(self);return self;
}
RX_FUNCTION(0x005f1180,rx_powercross_proxy_delete)

static const rx_addr names[3]={0x9427bc,0x941bdc,0x9427d0};
static const rx_addr callbacks[3]={0x5fb830,0x5fb880,0x5fb990};
void rx_powercross_countdown_unbind(void){
 if(pc.subscribed){ACT_FRAME(F,0x18,2);
  for(int i=0;i<3;i++){rx_addr channel;ACT_CALLR(channel,2,rx_world_registry_find(pc.channel,names[i]));
   SU32(F,0,0x9577f0);SU32(F,4,0);SU32(F,8,crt_type_info_name(0x9cf5b4,0x9e1a7c));SU32(F,0xc,pc.component);SU32(F,0x10,callbacks[i]);SU32(F,0x14,0);
   ACT_CALL(1,rx_channel_remove_listener(channel,F));}
  ACT_END(F);
 }
 pc.subscribed=0;pc.visible=0;pc.activity=pc.channel=0;
 if(pc.component){rx_reflex_aligned_free(pc.component);pc.component=0;}
}
void rx_powercross_countdown_bind(rx_addr activity){
 if(pc.activity==activity&&pc.component)return;
 rx_powercross_countdown_unbind();
 if(!activity||U32(activity,0)!=0x943554||U32(activity,0x23c)!=19||U32(activity,0x238)!=1)
  act_unreached(0x5fb4b0,"countdown requires actual offline Powercross activity");
 if(U32(activity,0x21c)!=0&&U32(activity,0x21c)!=5)act_unreached(0x5fb4b0,"countdown dashboard must bind before Countdown Entered");
 pc.activity=activity;pc.channel=U32(activity,0x290);pc.component=rx_reflex_aligned_alloc(0x3a8);
 if(!pc.component)act_unreached(0x5fb450,"countdown backing allocation failed");
 SU32(pc.component,0x3a0,0);SF32(pc.component,0x3a4,-FLT_MAX);pc.step=0;pc.events=0;pc.visible=0;
 /*00545330 is mode==8, not a network test.005fb4b0 subscribes when
  * mode!=8 OR the mode8 board is absent. Finite Single68 is mode1.*/
 if(U32(activity,0x238)!=8||U32(activity,0x2cc)==0){ACT_FRAME(F,0,4);
  for(int i=0;i<3;i++)ACT_CALL(4,rx_event_register_proxy(pc.channel,names[i],0x9577f0,0x9cf5b4,pc.component,callbacks[i],0));
  ACT_END(F);pc.subscribed=1;
 }
}
void rx_powercross_countdown_tick(float dt){if(pc.component)rx_powercross_countdown_component_tick(pc.component,dt);}
void rx_powercross_countdown_reset(void){memset(&pc,0,sizeof pc);}
void rx_powercross_countdown_prepare(uint32_t enabled){if(!enabled)rx_powercross_countdown_unbind();pc.wanted=enabled!=0;}
int rx_powercross_countdown_copy(rx_addr activity,uint32_t*visible,int32_t*step,uint32_t*exited,float*remaining,uint32_t*events){
 if(!pc.component||pc.activity!=activity)return 0;
 *visible=pc.visible;*step=pc.step;*exited=U32(pc.component,0x3a0);*remaining=F32(pc.component,0x3a4);*events=pc.events;return 1;
}

/* Actual activity lifecycle, before canonical global listeners. The installed
 * HUD route has been accepted before the level request; bind as its real
 * Activity Init arrives, while Inactive5 or Pre0, before Countdown Entered.
 * Activity UnInit removes its listeners while the old channel remains alive. */
void rx_powercross_countdown_activity_lifecycle(rx_addr channel,rx_addr name,rx_addr payload,uint32_t size){
 if(channel!=U32(0,0xa93f74)||!payload||size!=4)return;
 rx_addr activity=U32(payload,0);
 if(pc.wanted&&activity&&crt_strcmp(name,0x935ea4)==0&&U32(activity,0)==0x943554&&U32(activity,0x23c)==19&&U32(activity,0x238)==1)
  rx_powercross_countdown_bind(activity);
 else if(pc.activity==activity&&crt_strcmp(name,0x941d90)==0)rx_powercross_countdown_unbind();
}
