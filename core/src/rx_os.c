/* rx_os.c -- see rx_os.h. Slot addresses: msvcr90 9.0.30729.1's import table at its base 0x78520000 and MXReflex.exe's kernel32
 * imports (0x009160xx), both checked with pefile. Handles and thread ids follow the reference boot (boot.cpp): one handle
 * counter from 0x100 in steps of 4, thread ids from 0x1004 (0x1000 = the main thread). */
#include "rx_os.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

static uint32_t g_last_error;
static uint32_t g_tls[64];
static uint32_t g_tls_next;
static uint32_t g_next_handle = 0x100;
static uint32_t g_next_tid = 0x1004;
static uint32_t g_suspended[64];   /* handles of threads created suspended and not resumed (they never run: the core runs the
                                      game single-threaded) */
static int g_nsuspended;

static uint32_t (*g_time_source)(void);   /* NULL: the host's monotonic clock */
void rx_os_set_time_source(uint32_t (*ms)(void)) { g_time_source = ms; }

uint32_t rx_os_call(uint32_t slot, int n, const uint32_t* a) {
    (void)n;
    switch (slot) {
    case 0x78521054: return g_last_error;                                         /* GetLastError */
    case 0x78521070: g_last_error = a[0]; return 0;                               /* SetLastError */
    case 0x78521030: return a[0] < 64 ? g_tls[a[0]] : 0;                          /* TlsGetValue */
    case 0x78521064: if (a[0] < 64) { g_tls[a[0]] = a[1]; return 1; } return 0;   /* TlsSetValue */
    case 0x78521060: return g_tls_next < 64 ? g_tls_next++ : 0xffffffffu;        /* TlsAlloc */
    case 0x7852106c: { uint32_t v = rx_rd32(a[0]) + 1; rx_wr32(a[0], v); return v; }   /* InterlockedIncrement */
    case 0x78521074: { uint32_t v = rx_rd32(a[0]) - 1; rx_wr32(a[0], v); return v; }   /* InterlockedDecrement */
    case 0x7852102c: return 0x1000;                                               /* GetCurrentThreadId: the main thread's id */
    case 0x78521008: return 0x7c800000;                                           /* GetModuleHandleW: a fixed kernel32 handle */
    case 0x78521048: case 0x78521044: return 0;                                   /* Enter / LeaveCriticalSection: single thread */
    /* MXReflex.exe's kernel32 imports */
    case 0x00916078: return g_last_error;                                         /* GetLastError */
    case 0x00916038: g_last_error = 0; { uint32_t h = g_next_handle; g_next_handle += 4; return h; }   /* CreateEventA (names:
                                                                                     no shared named events in the race) */
    case 0x00916030: {                                                            /* CreateThread(sa, stack, fn, param, flags, &tid) */
        uint32_t h = g_next_handle; g_next_handle += 4;
        if (a[5]) rx_wr32(a[5], g_next_tid);
        g_next_tid += 4;
        if ((a[4] & 4) && g_nsuspended < 64) g_suspended[g_nsuspended++] = h;
        return h;
    }
    case 0x0091602c:                                                              /* ResumeThread: the previous suspend count */
        for (int i = 0; i < g_nsuspended; i++) if (g_suspended[i] == a[0]) { g_suspended[i] = g_suspended[--g_nsuspended]; return 1; }
        return 0;
    case 0x00916028: case 0x00916024: return 1;                                   /* SetThreadAffinityMask / SetThreadPriority */
    case 0x00916014:                                                              /* InitializeCriticalSection: {0, -1, 0, 0, 0, 0} */
        rx_wr32(a[0], 0); rx_wr32(a[0] + 4, 0xffffffffu);
        for (uint32_t k = 8; k < 24; k += 4) rx_wr32(a[0] + k, 0);
        return 0;
    /* Candidate import additions: product services have no live worker/OS section;
     * Enter/Leave already use the single-thread model. Oracle uses original imports. */
    case 0x00916010: return 0; /* DeleteCriticalSection: no host CS exists in product. */
    case 0x00916018: /* CloseHandle: remove tracked suspended synthetic thread, if any. */
        for (int i = 0; i < g_nsuspended; i++)
            if (g_suspended[i] == a[0]) { g_suspended[i] = g_suspended[--g_nsuspended]; break; }
        return 1;
    case 0x0091600c: case 0x009160f8: return 0;                                   /* Enter / LeaveCriticalSection: single thread */
    case 0x00916034: case 0x00916090: return 1;                                   /* SetEvent / ResetEvent */
    case 0x0091607c: return 0x1000;                                               /* GetCurrentThreadId: the main thread */
    case 0x00916020: return 0;                                                    /* Sleep: nothing to wait for (one thread) */
    case 0x00916080: { uint32_t v = rx_rd32(a[0]) + 1; rx_wr32(a[0], v); return v; }   /* InterlockedIncrement */
    case 0x00916084: { uint32_t v = rx_rd32(a[0]) - 1; rx_wr32(a[0], v); return v; }   /* InterlockedDecrement */
    case 0x0091609c: { uint32_t v = rx_rd32(a[0]); if (v == a[2]) rx_wr32(a[0], a[1]); return v; }   /* InterlockedCompareExchange */
    case 0x009160a8: {                                                            /* WideCharToMultiByte(cp, flags, src, n, dst, size, 0, 0) */
        if (a[0] != 65001u || a[1] != 0 || a[6] || a[7]) break;                   /* CP_UTF8 only, as the game calls it */
        uint32_t src = a[2], n = a[3], dst = a[4], size = a[5], out = 0, i = 0;
        for (;;) {
            if (n != 0xffffffffu && i >= n) break;
            uint32_t c = rx_rd16(src + 2 * i++);
            if (c >= 0xd800 && c < 0xdc00 && (n == 0xffffffffu || i < n)) {
                uint32_t d = rx_rd16(src + 2 * i);
                if (d >= 0xdc00 && d < 0xe000) { i++; c = 0x10000 + ((c - 0xd800) << 10) + (d - 0xdc00); } else c = 0xfffd;
            } else if (c >= 0xd800 && c < 0xe000) c = 0xfffd;
            uint8_t b[4]; uint32_t k;
            if (c < 0x80) { b[0] = (uint8_t)c; k = 1; }
            else if (c < 0x800) { b[0] = (uint8_t)(0xc0 | c >> 6); b[1] = (uint8_t)(0x80 | (c & 0x3f)); k = 2; }
            else if (c < 0x10000) { b[0] = (uint8_t)(0xe0 | c >> 12); b[1] = (uint8_t)(0x80 | (c >> 6 & 0x3f)); b[2] = (uint8_t)(0x80 | (c & 0x3f)); k = 3; }
            else { b[0] = (uint8_t)(0xf0 | c >> 18); b[1] = (uint8_t)(0x80 | (c >> 12 & 0x3f)); b[2] = (uint8_t)(0x80 | (c >> 6 & 0x3f)); b[3] = (uint8_t)(0x80 | (c & 0x3f)); k = 4; }
            if (size) {
                if (out + k > size) { g_last_error = 122; return 0; }   /* ERROR_INSUFFICIENT_BUFFER */
                for (uint32_t j = 0; j < k; j++) rx_wr8(dst + out + j, b[j]);
            }
            out += k;
            if (n == 0xffffffffu && c == 0) break;
        }
        return out;
    }
    case 0x009164b8: {                                                            /* winmm timeGetTime: milliseconds */
        if (g_time_source) return g_time_source();
#ifdef _WIN32
        {   /* the performance counter, as mingw's clock_gettime(CLOCK_MONOTONIC) reads it (winpthreads clock.c), without linking
               winpthreads: whole seconds, then the nanoseconds rounded to nearest, then milliseconds as below */
            LARGE_INTEGER pf, pc; QueryPerformanceFrequency(&pf); QueryPerformanceCounter(&pc);
            uint64_t sec = (uint64_t)(pc.QuadPart / pf.QuadPart);
            uint64_t ns = (uint64_t)(((pc.QuadPart % pf.QuadPart) * 1000000000LL + (pf.QuadPart >> 1)) / pf.QuadPart);
            if (ns >= 1000000000u) { sec++; ns -= 1000000000u; }
            return (uint32_t)(sec * 1000u + ns / 1000000u);
        }
#else
        struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
        return (uint32_t)((uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u);
#endif
    }
    case 0x00916008: return 0;                                                    /* WaitForSingleObject: WAIT_OBJECT_0 */
    default: break;
    }
    {
        fprintf(stderr, "rxcore: OS service at import slot %08x not provided (core/src/rx_os.c)\n", slot);
        abort();
    }
}
