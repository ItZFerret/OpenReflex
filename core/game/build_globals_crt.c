/* build_globals_crt.c -- unit 12b: the startup of msvcr90.dll (DllMain -> _CRT_INIT, VC9 SP1 9.0.30729.1) as far as the core's CRT ports
 * (game/crt_*.c) read its data. The product loads the DLL's .data section from the install's redistributable (rx_image_load_crt); this
 * sets what the DLL's startup code writes there before the exe's entry point runs. The values are the reference run's (the emulator's
 * kernel32 stand-ins: GetProcessHeap, TlsAlloc, GetStringTypeA / LCMapStringA for code page 1252); heap addresses of course are the
 * core's own. What is NOT set, because no core port reads it: the stack-cookie seed (0x785ba2a8), the stdio tables (__pioinfo
 * 0x785bc658), the environment / command line strings (0x785bd660, 0x785bd664, 0x785bd6a0), the program name, the encoded kernel32
 * entry points (0x785bbc98.., the OS layer core/src/rx_os.c answers those calls) and the whole of msvcp90.dll's data (facet ids and the
 * locale registry: std::string does not touch them).
 * Checked by tools/build_globals_cmp.py --dll against the DLL's data at the exe's entry point (oracle tracer). */
#include "build_globals_int.h"

#define MSVCR90_DATA 0x785b7000u

void rx_crt_startup(void) {
    /* __heap_init (system heap mode): __crtheap = GetProcessHeap(), __active_heap = 1 (_HEAP_MAXREQ stays); __sse2_available (__set_sse2_available) = 1 */
    SU32(0, 0x785bc160, 0x00150000);
    SU32(0, 0x785bd680, 1);
    SU32(0, 0x785bd7b0, 1);
    SU32(0, 0x785bd7b4, 1);
    /* _mtinit: the FLS fallback to TLS (FlsAlloc is not in the reference kernel32): __tlsindex, __flsindex = the TlsAlloc results */
    SU32(0, 0x785b71e4, 1);
    SU32(0, 0x785b71e0, 2);
    /* _mtinitlocks: every entry of _locktable {lock, kind} of kind lkPrealloc (1) gets the next critical section of lclcritsects
     * (0x785bba40, 0x18 bytes each), initialized with InitializeCriticalSectionAndSpinCount(cs, 4000) */
    rx_addr cs = 0x785bba40;
    for (rx_addr e = 0x785b70c0; e < 0x785b71e0; e += 8) {                 /* (36 entries) */
        if (U32(e, 4) != 1) continue;
        SU32(e, 0, cs);
        rx_critical_section_init(cs); SU32(cs, 0x14, 4000);
        cs += 0x18;
    }
    /* _setmbcp(_MB_CP_ANSI) for code page 1252: no lead bytes; the Latin-1 letters 0xc0..0xff get _SBUP (0x10) / _SBLOW (0x20) in
     * _mbctype (0x785b7980, index c + 1) and the other-case mapping in _mbcasemap (0x785b7a88) */
    for (uint32_t c = 0xc0; c < 0x100; c++) {
        int up = c <= 0xde && c != 0xd7, low = c >= 0xdf && c != 0xf7;
        SU8(0, 0x785b7980 + 1 + c, up ? 0x10 : low ? 0x20 : 0);
        uint32_t m = up ? (c == 0xd7 ? 0 : c + 0x20) : (c == 0xdf || c == 0xff) ? c : (c >= 0xe0 && c != 0xf7) ? c - 0x20 : 0;   /* (d7 and f7, the multiplication and division signs, stay 0) */
        SU8(0, 0x785b7a88 + c, m);
    }
    /* the new threadmbcinfo (544 bytes, a copy of __initialmbcinfo 0x785b7760): refcount 2, code page 1252, its own copies of the tables;
     * __initialmbcinfo's refcount is -1 (never freed); __ptmbcinfo = the block */
    SU32(0, 0x785b7760, 0xffffffffu);
    rx_addr mb = rx_crt_malloc(544);
    for (uint32_t o = 0; o < 544; o += 4) SU32(mb, o, U32(0, 0x785b7760 + o));
    SU32(mb, 0, 2); SU32(mb, 4, 0x4e4);
    for (uint32_t c = 0xc0; c < 0x100; c++) { SU8(mb, 0x1d + c, U8(0, 0x785b7980 + 1 + c)); SU8(mb, 0x11d + c, U8(0, 0x785b7a88 + c)); }
    SU32(0, 0x785b7b88, mb);
    SU32(0, 0x785bc0d0, 0x4e4);                      /* __mbcodepage */
    SU32(0, 0x785b7ffc, 1);                          /* __ismbcodepage */
    /* the thread locale: a copy (216 bytes) of __initiallocinfo 0x785ba2c0 with refcount 2; __initiallocinfo drops to 0 */
    rx_addr li = rx_crt_malloc(216);
    for (uint32_t o = 0; o < 216; o += 4) SU32(li, o, U32(0, 0x785ba2c0 + o));
    SU32(li, 0, 2);
    SU32(0, 0x785ba2c0, 0);
    SU32(0, 0x785ba398, li);                         /* __ptlocinfo */
}
