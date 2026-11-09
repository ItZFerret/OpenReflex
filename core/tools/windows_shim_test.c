/* windows_shim_test.c -- run on Windows (core/tools/build_windows_shim_test.sh builds it with src/win/rx_pthread_shim.c and
 * no winpthreads). Exercises the shim the way GCC's emulated TLS uses it, plus the DLL-unload path:
 *   once      pthread_once raced by 16 threads runs its function exactly once
 *   mutex     a PTHREAD_MUTEX_INITIALIZER mutex and an initialized one keep 16 threads x 100000 increments exact
 *   keys      key destructors run once per exiting thread with a non-NULL value (never for NULL), keys without one work
 *   lasterror pthread_getspecific / setspecific keep the thread's GetLastError value
 *   tls       _Thread_local scalars and arrays: each new thread sees the initial values, threads never see each other's
 *   tlsfree   4000 threads each touching 64 KiB of thread-locals: the process does not keep their blocks
 *   unload    a DLL with thread-locals is unloaded while a thread that used them is alive; the thread then exits cleanly
 * Prints one PASS / FAIL line per check and a summary; exit code 0 only when everything passed. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void report(const char *name, int ok, const char *detail) {
    printf("%s %-9s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (!ok) g_fail = 1;
    fflush(stdout);
}
static void run_threads(int n, LPTHREAD_START_ROUTINE fn, void *arg) {
    HANDLE h[64];
    for (int i = 0; i < n; i++) {
        h[i] = CreateThread(NULL, 0, fn, arg ? arg : (void *)(intptr_t)i, 0, NULL);
        if (!h[i]) { printf("FAIL CreateThread %lu\n", GetLastError()); exit(2); }
    }
    WaitForMultipleObjects((DWORD)n, h, TRUE, INFINITE);
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
}

/* ---- once */
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static volatile LONG g_once_runs, g_once_go;
static void once_fn(void) { InterlockedIncrement(&g_once_runs); Sleep(20); }
static DWORD WINAPI once_thread(void *p) {
    (void)p;
    while (!g_once_go) YieldProcessor();
    pthread_once(&g_once, once_fn);
    return g_once_runs == 1 ? 0 : 1;   /* after pthread_once returns, the function has finished */
}

/* ---- mutex */
static pthread_mutex_t g_static_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_init_mutex;
static volatile long g_count_a, g_count_b;
static DWORD WINAPI mutex_thread(void *p) {
    (void)p;
    for (int i = 0; i < 100000; i++) {
        pthread_mutex_lock(&g_static_mutex); g_count_a = g_count_a + 1; pthread_mutex_unlock(&g_static_mutex);
        pthread_mutex_lock(&g_init_mutex); g_count_b = g_count_b + 1; pthread_mutex_unlock(&g_init_mutex);
    }
    return 0;
}

/* ---- keys */
static pthread_key_t g_key_a, g_key_b, g_key_plain;
static volatile LONG g_dtor_a, g_dtor_b, g_dtor_bad;
static void dtor_a(void *v) { if (!v || *(int *)v != 0xA) InterlockedIncrement(&g_dtor_bad); InterlockedIncrement(&g_dtor_a); free(v); }
static void dtor_b(void *v) { if (!v || *(int *)v != 0xB) InterlockedIncrement(&g_dtor_bad); InterlockedIncrement(&g_dtor_b); free(v); }
static DWORD WINAPI key_thread(void *p) {
    int i = (int)(intptr_t)p;
    if (pthread_getspecific(g_key_a) || pthread_getspecific(g_key_b) || pthread_getspecific(g_key_plain)) return 1;   /* new thread: NULL */
    int *a = malloc(sizeof *a); *a = 0xA;
    if (pthread_setspecific(g_key_a, a)) return 2;
    if (i % 2 == 0) { int *b = malloc(sizeof *b); *b = 0xB; if (pthread_setspecific(g_key_b, b)) return 3; }
    else if (pthread_setspecific(g_key_b, NULL)) return 4;   /* NULL: no destructor call */
    if (pthread_setspecific(g_key_plain, (void *)(intptr_t)(i + 1))) return 5;
    Sleep(5);
    if (pthread_getspecific(g_key_a) != a || pthread_getspecific(g_key_plain) != (void *)(intptr_t)(i + 1)) return 6;
    return 0;
}

/* ---- thread-locals (emutls through the shim) */
static _Thread_local int tl_scalar = 1234;
static _Thread_local unsigned tl_array[256] = {7, 8, 9};
static _Thread_local unsigned char tl_big[64 * 1024];
static volatile LONG g_tls_bad;
static DWORD WINAPI tls_thread(void *p) {
    unsigned id = (unsigned)(intptr_t)p + 1;
    if (tl_scalar != 1234 || tl_array[0] != 7 || tl_array[1] != 8 || tl_array[2] != 9 || tl_array[255] != 0) InterlockedIncrement(&g_tls_bad);
    for (int round = 0; round < 200; round++) {
        tl_scalar = (int)(id * 1000 + round);
        for (int k = 0; k < 256; k++) tl_array[k] = id ^ (unsigned)k;
        SwitchToThread();
        if (tl_scalar != (int)(id * 1000 + round)) InterlockedIncrement(&g_tls_bad);
        for (int k = 0; k < 256; k++) if (tl_array[k] != (id ^ (unsigned)k)) { InterlockedIncrement(&g_tls_bad); break; }
    }
    return 0;
}
static DWORD WINAPI tls_big_thread(void *p) {
    (void)p;
    memset(tl_big, 0x5a, sizeof tl_big);
    tl_scalar++;
    return tl_big[12345] == 0x5a ? 0 : 1;
}
static SIZE_T private_bytes(void) {
    PROCESS_MEMORY_COUNTERS_EX c;
    memset(&c, 0, sizeof c);
    c.cb = sizeof c;
    K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&c, sizeof c);
    return c.PrivateUsage;
}

/* ---- unload */
typedef int (*touch_fn)(int);
static volatile LONG g_unload_stage;
static touch_fn g_touch;
static DWORD WINAPI unload_thread(void *p) {
    (void)p;
    int r = g_touch(41);   /* sets the DLL's thread-locals on this thread */
    InterlockedExchange(&g_unload_stage, 1);
    while (g_unload_stage != 2) Sleep(1);   /* the DLL is unloaded meanwhile */
    return r == 42 ? 0 : 1;                 /* exiting must not call into the unloaded DLL */
}

int main(void) {
    char detail[160];
    printf("OPEN REFLEX Windows pthread-shim test (rxcore.dll's emulated-TLS support without winpthreads)\n");

    { HANDLE h[16];
      for (int i = 0; i < 16; i++) h[i] = CreateThread(NULL, 0, once_thread, NULL, 0, NULL);
      InterlockedExchange(&g_once_go, 1);
      WaitForMultipleObjects(16, h, TRUE, INFINITE);
      int bad = 0; for (int i = 0; i < 16; i++) { DWORD c = 1; GetExitCodeThread(h[i], &c); bad += c != 0; CloseHandle(h[i]); }
      pthread_once(&g_once, once_fn);
      snprintf(detail, sizeof detail, "16 racing threads, function ran %ld time(s), %d early return(s)", g_once_runs, bad);
      report("once", g_once_runs == 1 && !bad, detail); }

    { pthread_mutex_init(&g_init_mutex, NULL);
      run_threads(16, mutex_thread, NULL);
      snprintf(detail, sizeof detail, "16 threads x 100000: static %ld, initialized %ld (want 1600000)", g_count_a, g_count_b);
      report("mutex", g_count_a == 1600000 && g_count_b == 1600000, detail); }

    { int ok = !pthread_key_create(&g_key_a, dtor_a) && !pthread_key_create(&g_key_b, dtor_b) && !pthread_key_create(&g_key_plain, NULL);
      HANDLE h[16]; int bad = 0;
      for (int i = 0; i < 16; i++) h[i] = CreateThread(NULL, 0, key_thread, (void *)(intptr_t)i, 0, NULL);
      WaitForMultipleObjects(16, h, TRUE, INFINITE);
      for (int i = 0; i < 16; i++) { DWORD c = 1; GetExitCodeThread(h[i], &c); bad += c != 0; CloseHandle(h[i]); }
      snprintf(detail, sizeof detail, "16 threads: key A destructor ran %ld (want 16), key B %ld (want 8: half set NULL), wrong values %ld, thread errors %d",
               g_dtor_a, g_dtor_b, g_dtor_bad, bad);
      report("keys", ok && g_dtor_a == 16 && g_dtor_b == 8 && !g_dtor_bad && !bad, detail); }

    { SetLastError(12345); void *v = pthread_getspecific(g_key_a); DWORD e1 = GetLastError();
      SetLastError(54321); int r = pthread_setspecific(g_key_plain, (void *)1); DWORD e2 = GetLastError();
      snprintf(detail, sizeof detail, "GetLastError after getspecific %lu (want 12345), after setspecific %lu (want 54321)", e1, e2);
      report("lasterror", v == NULL && r == 0 && e1 == 12345 && e2 == 54321, detail); }

    { tl_scalar = 99;
      run_threads(16, tls_thread, NULL);
      snprintf(detail, sizeof detail, "16 threads x 200 rounds: %ld bad reads, main thread's value %d (want 99)", g_tls_bad, tl_scalar);
      report("tls", !g_tls_bad && tl_scalar == 99, detail); }

    { for (int i = 0; i < 64; i++) run_threads(1, tls_big_thread, NULL);   /* warm the heap */
      SIZE_T before = private_bytes();
      for (int i = 0; i < 4000; i++) run_threads(1, tls_big_thread, NULL);
      SIZE_T after = private_bytes();
      long long grew = (long long)after - (long long)before;
      snprintf(detail, sizeof detail, "4000 threads x 64 KiB thread-locals: private bytes grew %lld KiB (a leak would be ~256000 KiB)", grew / 1024);
      report("tlsfree", grew < 32LL * 1024 * 1024, detail); }

    { char path[MAX_PATH]; DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
      while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
      snprintf(path + n, MAX_PATH - n, "windows_shim_test_dll.dll");
      HMODULE m = LoadLibraryA(path);
      g_touch = m ? (touch_fn)(void *)GetProcAddress(m, "rx_shim_test_touch") : NULL;
      if (!g_touch) report("unload", 0, "windows_shim_test_dll.dll not found or not loadable (keep it next to the exe)");
      else {
          HANDLE t = CreateThread(NULL, 0, unload_thread, NULL, 0, NULL);
          while (g_unload_stage != 1) Sleep(1);
          BOOL freed = FreeLibrary(m);
          HMODULE still = GetModuleHandleA("windows_shim_test_dll.dll");
          InterlockedExchange(&g_unload_stage, 2);
          DWORD w = WaitForSingleObject(t, 10000), code = 1; GetExitCodeThread(t, &code); CloseHandle(t);
          snprintf(detail, sizeof detail, "DLL freed %s, unmapped %s, thread that used its thread-locals exited %s",
                   freed ? "yes" : "no", still ? "no" : "yes", w == WAIT_OBJECT_0 && code == 0 ? "cleanly" : "with an error");
          report("unload", freed && !still && w == WAIT_OBJECT_0 && code == 0, detail);
      } }

    printf(g_fail ? "RESULT: FAIL (please send this whole window's text)\n" : "RESULT: PASS (all checks)\n");
    return g_fail;
}
