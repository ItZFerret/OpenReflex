#ifndef REFLEX_PRIVATE_BIKE_STEP_BRIDGES_H
#define REFLEX_PRIVATE_BIKE_STEP_BRIDGES_H
#include <stdint.h>
#define REFLEX_DRAFT_BIKE_STATE_BYTES 38656u
#define REFLEX_DRAFT_BIKE_BRIDGE_INPUT_BYTES 39096u
typedef struct reflex_draft_bike_bridge_globals {
    float axis_x[4],axis_y[4],axis_z[4],g[4];
    float sign_x[4],sign_y[4],sign_z[4],conjugate[4];
    uint32_t mask[4];
    float one,two,negative_two,negative_zero;
    float extra[4];
} reflex_draft_bike_bridge_globals;
typedef struct reflex_draft_bike_bridge_input {
    uint8_t state[38656],control[256];
    uint32_t simulation,dt_bits;
    reflex_draft_bike_bridge_globals globals;
} reflex_draft_bike_bridge_input;
enum reflex_draft_bike_bridge_stage {
    REFLEX_DRAFT_BIKE_POSE=1,REFLEX_DRAFT_BIKE_STEERING_ANCHORS=2,
    REFLEX_DRAFT_BIKE_NODE6=3,REFLEX_DRAFT_BIKE_POST_CONTACT=4,
    REFLEX_DRAFT_BIKE_FILTERED_SPEED=5
};
#ifdef __cplusplus
extern "C" {
#endif
/* Private composition bridge; not registered in reflex_core.h.
 * Exact input39096/output38656 bytes. Status0=success,1=arguments,
 * 2=unsupported FP,3=invalid snapshot/numerical range.
 * Full input snapshot permits unaligned and overlapping buffers. Output is
 * untouched on failure; all caller FP controls and exception flags restore.
 */
__attribute__((visibility("default"))) uint32_t reflex_draft_bike_step_bridge(
    uint32_t stage,const void* input,uint32_t input_size,void* output,uint32_t output_size);
#ifdef __cplusplus
}
#endif
#endif
