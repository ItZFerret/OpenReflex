/* Complete17-byte0055eb90: actual tail-dispatch to embedded FINISH scenario
 * vf10. This must not inspect the RESULTS scenario instead. */
#include "activity_finish_stage.h"
#include "build_activity_util.h"
uint32_t rx_activity_finish_stage_is_b(rx_addr activity) {
 const rx_addr scenario=activity+0x114260;
 const rx_addr target=U32(U32(scenario,0),0x28);
 uint32_t (*owner)(rx_addr)=RX_FN(target,uint32_t(*)(rx_addr));
 if(!owner)act_unreached(target,"unported actual finish scenario vf10 tail target");
 /* Original JMP adds no return/argument slot; preserve current guest ESP. */
 return owner(scenario);
}
RX_FUNCTION(0x0055eb90,rx_activity_finish_stage_is_b)
