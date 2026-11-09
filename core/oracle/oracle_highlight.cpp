/* oracle_highlight.cpp -- ORACLE BUILD ONLY: game/highlight_progress.c refuses the oracle build (it needs a Steam provider), so its
 * two entry points run the ORIGINAL functions here (MXHighlightTracking_AddProgress 006d7980 / IsUnlocked 006d7d60). */
extern "C" {
#include "highlight_progress.h"
#include "rx_mem.h"
uint32_t rx_highlight_add_progress(rx_addr t, uint32_t c, uint32_t i) { const uint32_t a[3] = {t, c, i}; return rx_call_original(0x006d7980, RX_STDCALL, 0, 3, a); }
uint32_t rx_highlight_is_unlocked(rx_addr t, uint32_t c, uint32_t i) { const uint32_t a[3] = {t, c, i}; return rx_call_original(0x006d7d60, RX_STDCALL, 0, 3, a); }
}
