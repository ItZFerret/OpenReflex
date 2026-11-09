/* race_results_text.h -- the UI text helpers of the results table's fillers (w-wpfill): App_FormatCredits 005dc430 (+ 005dc3c0,
 * 007900c0) and the localized ordinal 005dc7a0 / 005dc720 (a UI boundary in the product: race_results_text.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr  rx_wstr_reverse(rx_addr s);                                         /* 007900c0 (EAX = UTF-16 string) -> EAX */
uint32_t rx_results_wcmb(rx_addr src, rx_addr dst, uint32_t size);             /* WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, size, 0, 0) (import 0x009160a8) */
rx_addr  rx_format_thousands_w(uint32_t value, rx_addr w);                    /* 005dc3c0 (ECX = value, EDI = UTF-16 buffer) -> EAX */
uint32_t rx_app_format_credits(int32_t value, int32_t size, rx_addr buf);     /* App_FormatCredits 005dc430 (ECX value, EDX size; cdecl buffer) -> EAX */
uint32_t rx_ui_ordinal_w(rx_addr w, uint32_t size, int32_t place);            /* 005dc720 (cdecl UTF-16 buffer, size, place) -> EAX */
uint32_t rx_ui_ordinal_text(rx_addr buf, int32_t size, int32_t place);        /* 005dc7a0 (ECX = size, EDI = buffer; cdecl place) -> EAX */
#ifdef __cplusplus
}
#endif
