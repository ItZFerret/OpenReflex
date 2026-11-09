#include "career_transition_host.h"
#ifndef RX_ORACLE
#include "build_activity_util.h"
static rx_owned_loading_host loading;
void rx_owned_loading_bind(const rx_owned_loading_host*h){
 if(!h||!h->context||!h->entered||loading.context)act_unreached(0x5a8030,"owned loading binding invalid");
 loading=*h;
}
void rx_owned_loading_unbind(void*context){if(loading.context==context)memset(&loading,0,sizeof loading);}
void rx_owned_loading_native_entry_completed(void){
 if(!loading.context)act_unreached(0x5a8030,"owned loading page entry has no host");
 loading.entered(loading.context);
}
#endif
