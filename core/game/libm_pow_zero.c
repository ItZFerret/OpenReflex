/* Hand C from original7859ed11..7859ef40/7859efe2. This is variant1's
 * actual zero route, not variant0's similar but different CRT/error path. */
#include "libm_pow_zero.h"
int rx_libm_pow_zero_supported(uint64_t x,uint64_t y){
 const uint64_t absolute=UINT64_C(0x7fffffffffffffff),infinity=UINT64_C(0x7ff0000000000000);
 return (x&absolute)==0&&(y&absolute)!=0&&(y&absolute)<infinity;
}
/* The original PSLLQ/PCMPEQD/PMOVMSKB sequence recognizes integer/odd parity
 * without converting to a bounded host integer. All exponents above52 make
 * representable doubles even; values with magnitude below1 are non-integers.
 * This is also valid for finite exponent operands that are themselves denormal. */
static int odd_integer(uint64_t absolute_y){
 unsigned encoded=(unsigned)(absolute_y>>52);if(encoded<1023||encoded>1075)return 0;
 unsigned fractional=1075-encoded;uint64_t mantissa=(absolute_y&UINT64_C(0x000fffffffffffff))|UINT64_C(0x0010000000000000);
 uint64_t mask=(UINT64_C(1)<<fractional)-1;
 return !(mantissa&mask)&&((mantissa>>fractional)&1)!=0;
}
int rx_libm_pow_zero_try(uint64_t x,uint64_t y,rx_libm_pow_zero_result* out){
 if(!out||!rx_libm_pow_zero_supported(x,y))return 0;
 const uint64_t sign=UINT64_C(0x8000000000000000),absolute=UINT64_C(0x7fffffffffffffff);
 uint32_t negative=(x&sign)&&odd_integer(y&absolute)?0x80000000u:0;
 uint32_t result_high=negative|((y&sign)?0x7ff00000u:0);
 out->ok=1;out->r=rx_bd((uint64_t)result_high<<32);out->hi=0;
 /*7859ef2b/ef00 shifts ECX13 then masks EAX by it. Negative y builds
  * infinity in EDX; positive y retains the zero from7859eef5. */
 out->eax=negative;out->ecx=negative;out->edx=(y&sign)?result_high:0;
 return 1;
}
