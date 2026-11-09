/* phys_crt.h -- the exe's runtime helpers of the physics frame (phys_crt.c). */
#pragma once
#include "phys_vec.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif
int64_t phys_ftol2(rx_f80 x, int precision);                       /* 008fa106 _ftol2 (x87 st0 -> EDX:EAX) */
int32_t phys_ftol2_sse(rx_f80 x, int precision);                   /* 008fa0d0 _ftol2_sse (x87 st0 -> EAX) */
void    phys_security_check_cookie(uint32_t cookie);/* 008f9efe __security_check_cookie (ECX) */
void    rx_report_gsfailure(void);                  /* 008fa43c ___report_gsfailure (cdecl; fatal: records the exception, ends the process) */
void    rx_rtc_stack_failure(rx_addr return_address, rx_addr name);   /* 008faa92 _RTC_StackFailure (cdecl; fatal) */
float   phys_sqrtf(float x);                        /* 0042c230 SqrtF (cdecl; xmm0 lane 0) */
float   phys_ceilf(float x);                        /* 0042c1d0 CeilF (xmm0 lane 0 -> x87) */
float   phys_powf(float base, float exponent);      /* 0042c1f0 PowF (xmm0, xmm1 lane 0 -> xmm0 lane 0) */
#ifdef __cplusplus
}
#endif
