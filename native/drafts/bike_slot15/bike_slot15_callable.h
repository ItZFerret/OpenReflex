#ifndef REFLEX_DRAFT_BIKE_SLOT15_CALLABLE_H
#define REFLEX_DRAFT_BIKE_SLOT15_CALLABLE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private bridge; not a registered public Reflex ABI.
 * Exact 45,900-byte input/output record from bike_slot15_layout.json.
 * Returns 0 success, 1 bad arguments, 2 unsupported FP/environment, 3 rejected
 * snapshot/execution. Output is unchanged on failure. Input/output may overlap.
 * Successful output includes the complete updated S/globals/regions and the
 * unchanged final dt/profile words. All incoming FP controls/status restore.
 */
uint32_t reflex_draft_bike_slot15(const void *input,uint32_t input_size,
                                void *output,uint32_t output_size);
#ifdef __cplusplus
}
#endif
#endif
