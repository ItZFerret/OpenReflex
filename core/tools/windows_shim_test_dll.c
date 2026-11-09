/* windows_shim_test_dll.c -- the test DLL of windows_shim_test.c's "unload" check: thread-locals (emulated TLS through
 * src/win/rx_pthread_shim.c, as in rxcore.dll), used on a thread that outlives FreeLibrary of this DLL. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static _Thread_local int tl_value = 1;
static _Thread_local char tl_block[4096];
__declspec(dllexport) int rx_shim_test_touch(int v) {
    tl_block[100] = (char)v;
    tl_value += v;
    return tl_value + (tl_block[100] != (char)v);
}
