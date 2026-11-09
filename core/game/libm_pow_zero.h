#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {int ok;double r;uint64_t hi;uint32_t eax,ecx,edx;} rx_libm_pow_zero_result;
/* Original7859e9d7 special branch, base±0 and finite nonzero exponent only.
 * Pure value/register semantic owner; adapter owns the genuine entry frame. */
int rx_libm_pow_zero_supported(uint64_t x,uint64_t y);
int rx_libm_pow_zero_try(uint64_t x,uint64_t y,rx_libm_pow_zero_result* result);
#ifdef __cplusplus
}
#endif
