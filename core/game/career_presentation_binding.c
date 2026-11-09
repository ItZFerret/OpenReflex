#include "career_presentation_binding.h"
#ifndef RX_ORACLE
#include "build_ui_world.h"
#include "build_activity_util.h"
#include "race_activity_misc.h"
#include "race.h"
static rx_career_presentation* bound;
uint32_t rx_career_presentation_bind(rx_career_presentation*p,rx_career_host*out){
 if(!p||!p->active||!out||bound)return 0;
 /* Real canonical worker dispatch is mandatory, not a supplied fake presenter. */
 if(p->services.fade_complete!=rx_career_presentation_dispatch_fade_complete||
    p->services.page_wipe_transition!=rx_career_presentation_dispatch_page_wipe_transition||
    p->services.page_wipe_complete!=rx_career_presentation_dispatch_page_wipe_complete)return 0;
 *out=(rx_career_host){p,rx_career_presentation_ui_event,rx_career_presentation_ui_route,
  rx_career_presentation_hub,rx_career_presentation_route_accept,rx_career_presentation_checkpoint};
 bound=p;return 1;
}
void rx_career_presentation_unbind(rx_career_presentation*p){
 if(p&&bound==p){
  if(p->dispatching)act_unreached(0x5bda00,"cannot unbind during owned presentation dispatch");
  bound=NULL;
 }
}
void rx_career_presentation_observe_fade(uint32_t bits,uint32_t color){
 if(!bound||rx_career_presentation_fade(bound,bits,color)!=RX_CAREER_HOST_OK)
  act_unreached(0x5bdaa0,"owned FadeOut notification rejected");
}
void rx_career_presentation_dispatch_fade_complete(void*unused){
 (void)unused;
 const rx_addr channel=rx_ui_event_channel();
 if(!channel)act_unreached(0x5a9dd0,"owned FadeComplete channel unavailable");
 /* Registered void(const char*) payload: synchronous empty guest string.
  * Actual Single callback ignores it. No host pointer enters guest dispatch. */
 ACT_FRAME(B,4,0);
 SU32(B,0,0);
 const uint32_t text=B;
 race_raise_event(channel,0x0094d174,0x009cdb68,&text,1);
 ACT_END(B);
}
void rx_career_presentation_observe_page_wipe(uint32_t guest_name){
 if(!bound||rx_career_presentation_page_wipe(bound,guest_name)!=RX_CAREER_HOST_OK)
  act_unreached(0x5bda00,"owned ShowPageWipe notification rejected");
}
static void dispatch_page_wipe(rx_addr event,const char*name){
 size_t n=0;
 if(!name||!name[0])act_unreached(event,"owned PageWipe has no name");
 while(n<RX_CAREER_HOST_TEXT_CAPACITY&&name[n])n++;
 if(n==RX_CAREER_HOST_TEXT_CAPACITY)act_unreached(event,"owned PageWipe name is not bounded");
 const rx_addr channel=rx_ui_event_channel();
 if(!channel)act_unreached(event,"owned PageWipe channel unavailable");
 /* Shared.ENG/PageWipe.lua passes the same name via AddString/SendEvent.
  * 005999bac/bbc/bcc register all three events through 00592820 using
  * 009cdb68. The copied name lives on the guest stack for synchronous dispatch;
  * no host pointer, animation duration or callback address enters the payload. */
 ACT_FRAME(B,RX_CAREER_HOST_TEXT_CAPACITY,0);
 for(size_t i=0;i<=n;i++)SU8(B,(uint32_t)i,(uint8_t)name[i]);
 const uint32_t text=B;
 race_raise_event(channel,event,0x009cdb68,&text,1);
 ACT_END(B);
}
void rx_career_presentation_dispatch_page_wipe_transition(void*unused,const char*name){
 (void)unused;dispatch_page_wipe(0x00941d7c,name);
}
void rx_career_presentation_dispatch_page_wipe_complete(void*unused,const char*name){
 (void)unused;dispatch_page_wipe(0x0094c5ec,name);
}
#endif
