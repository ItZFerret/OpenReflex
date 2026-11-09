#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private controlled one-world record:55756 input,39188 output bytes.
 * No supplied hit Boolean. Geometry is the actual original ray's input.
 * See bike_controls_terrain_model.py for the complete normalized mapping.
 * Status0 success,1 argument/size,2 unsupported FP,3 unsupported execution.
 * Every error is atomic; input/output may overlap at arbitrary byte offsets. */
uint32_t reflex_draft_bike_controls_terrain(const void*,uint32_t,void*,uint32_t);
#ifdef __cplusplus
}
#endif
