/* rx_pthread_shim.c -- Windows only (core/tools/build_windows.sh). The seven POSIX thread calls that GCC's emulated
 * thread-locals need, over plain Win32, so rxcore.dll does not link mingw's winpthreads.
 *
 * Why: this MinGW GCC has no native TLS, so every _Thread_local / thread_local in the core becomes a call to libgcc's
 * __emutls_get_address (libgcc_eh.a, emutls.o). emutls.o calls pthread_once, pthread_key_create, pthread_getspecific,
 * pthread_setspecific, pthread_mutex_init, pthread_mutex_lock and pthread_mutex_unlock. Without this file the linker
 * takes them from libwinpthread, and winpthreads' thread.o / sched.o / clock.o bring in thread-suspend, thread-context,
 * process-open and system-time imports the core never uses (antivirus heuristics read that set as process injection).
 * Defined here, the linker never opens libwinpthread; if anything else ever needs another winpthreads symbol the link
 * fails with a duplicate definition, and build_windows.sh's import check fails the build.
 *
 * ABI: the prototypes come from mingw's own <pthread.h> (winpthreads), so the compiler checks every signature, and the
 * static asserts below check the types: pthread_once_t is a long (PTHREAD_ONCE_INIT 0), pthread_key_t an unsigned,
 * pthread_mutex_t an intptr_t whose static initializer PTHREAD_MUTEX_INITIALIZER is -1. An SRWLOCK is one pointer and
 * SRWLOCK_INIT is 0, so the lock lives inside the pthread_mutex_t itself: init stores 0, and a lock that still holds
 * the static initializer -1 is switched to 0 first. Only plain (non-recursive) mutexes are supported; emutls uses one.
 *
 * Keys are fiber-local storage slots: FlsAlloc with the key's destructor, so Windows frees each thread's emutls block
 * when that thread exits (the destructor gets the slot's value; it is not called for a NULL value). Each key with a
 * destructor needs its own callback, because the FLS callback receives only the value: RX_SHIM_KEYS trampolines.
 * When the DLL is unloaded while the process keeps running (FreeLibrary), a TLS callback frees the slots, so Windows
 * never calls back into an unmapped DLL. getspecific / setspecific keep the thread's last-error value, as winpthreads does.
 *
 * The shim's own bookkeeping is in .rxkeep (RX_RESET_KEEP): a session reset must not forget the keys. emutls' own
 * once flag, key and mutex live in libgcc_eh's sections, which the reset never covers. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include "rx_reset.h"

_Static_assert(sizeof(pthread_once_t) == sizeof(long), "pthread_once_t is winpthreads' long");
_Static_assert(PTHREAD_ONCE_INIT == 0, "PTHREAD_ONCE_INIT is 0");
_Static_assert(sizeof(pthread_key_t) == sizeof(unsigned) && sizeof(pthread_key_t) == sizeof(DWORD), "pthread_key_t is an unsigned");
_Static_assert(sizeof(pthread_mutex_t) == sizeof(intptr_t), "pthread_mutex_t is winpthreads' intptr_t");
_Static_assert(sizeof(SRWLOCK) == sizeof(pthread_mutex_t), "an SRWLOCK fits in a pthread_mutex_t");
_Static_assert(PTHREAD_MUTEX_INITIALIZER == (pthread_mutex_t)-1, "winpthreads' static mutex initializer is -1");

/* ---- pthread_once: 0 not run, 1 running, 2 done (in the caller's long) */
int pthread_once(pthread_once_t *o, void (*func)(void)) {
    if (!o || !func) return EINVAL;
    if (__atomic_load_n(o, __ATOMIC_ACQUIRE) == 2) return 0;
    long expect = 0;
    if (__atomic_compare_exchange_n(o, &expect, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        func();
        __atomic_store_n(o, 2, __ATOMIC_RELEASE);
        return 0;
    }
    while (__atomic_load_n(o, __ATOMIC_ACQUIRE) != 2) SwitchToThread();   /* another thread runs func (a few calls) */
    return 0;
}

/* ---- keys: FLS slots; one trampoline per key with a destructor */
#define RX_SHIM_KEYS 4
static void (*volatile g_key_dtor[RX_SHIM_KEYS])(void *) RX_RESET_KEEP;
static volatile DWORD g_key_index[RX_SHIM_KEYS] RX_RESET_KEEP;
static volatile LONG g_key_used RX_RESET_KEEP;   /* trampolines handed out */

#define RX_SHIM_TRAMPOLINE(n) static VOID NTAPI fls_dtor##n(PVOID v) { void (*d)(void *) = g_key_dtor[n]; if (v && d) d(v); }
RX_SHIM_TRAMPOLINE(0) RX_SHIM_TRAMPOLINE(1) RX_SHIM_TRAMPOLINE(2) RX_SHIM_TRAMPOLINE(3)
static PFLS_CALLBACK_FUNCTION const g_trampoline[RX_SHIM_KEYS] = {fls_dtor0, fls_dtor1, fls_dtor2, fls_dtor3};

int pthread_key_create(pthread_key_t *key, void (*dest)(void *)) {
    if (!key) return EINVAL;
    if (!dest) {
        DWORD i = FlsAlloc(NULL);
        if (i == FLS_OUT_OF_INDEXES) return EAGAIN;
        *key = i;
        return 0;
    }
    LONG n = InterlockedIncrement(&g_key_used) - 1;
    if (n >= RX_SHIM_KEYS) { InterlockedDecrement(&g_key_used); return EAGAIN; }
    g_key_dtor[n] = dest;
    DWORD i = FlsAlloc(g_trampoline[n]);
    if (i == FLS_OUT_OF_INDEXES) { g_key_dtor[n] = NULL; return EAGAIN; }   /* the trampoline stays unused */
    g_key_index[n] = i;
    *key = i;
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    DWORD err = GetLastError();
    void *v = FlsGetValue(key);
    SetLastError(err);
    return v;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    DWORD err = GetLastError();
    BOOL ok = FlsSetValue(key, (PVOID)value);
    SetLastError(err);
    return ok ? 0 : EINVAL;
}

/* ---- plain mutexes: an SRWLOCK in place */
static PSRWLOCK srw(pthread_mutex_t *m) {
    intptr_t v = __atomic_load_n(m, __ATOMIC_ACQUIRE);
    if (v == (intptr_t)PTHREAD_MUTEX_INITIALIZER) {   /* statically initialized, first use */
        intptr_t expect = v;
        __atomic_compare_exchange_n(m, &expect, 0, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    } else if (v < 0) {
        abort();   /* PTHREAD_RECURSIVE_MUTEX_INITIALIZER / ERRORCHECK (-2 / -3): not supported here */
    }
    return (PSRWLOCK)m;
}

int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
    if (!m) return EINVAL;
    if (a && (*a & 3) != PTHREAD_MUTEX_NORMAL) return ENOTSUP;   /* plain mutexes only (winpthreads keeps the type in bits 0-1) */
    __atomic_store_n(m, 0, __ATOMIC_RELEASE);   /* SRWLOCK_INIT */
    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *m) {
    if (!m) return EINVAL;
    AcquireSRWLockExclusive(srw(m));
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *m) {
    if (!m) return EINVAL;
    ReleaseSRWLockExclusive((PSRWLOCK)m);
    return 0;
}

/* ---- unload: free the FLS slots that have destructors so Windows never calls into an unmapped DLL. At process exit
 * (Reserved != NULL) the process is going away and nothing is unmapped first, so nothing is done. */
static VOID NTAPI rx_shim_tls_callback(PVOID module, DWORD reason, PVOID reserved) {
    (void)module;
    if (reason != DLL_PROCESS_DETACH || reserved) return;
    LONG n = g_key_used < RX_SHIM_KEYS ? g_key_used : RX_SHIM_KEYS;
    for (LONG k = 0; k < n; k++)
        if (g_key_dtor[k]) { FlsFree(g_key_index[k]); g_key_dtor[k] = NULL; }
}
/* mingw's tlssup.o owns the TLS directory (__xl_a .. __xl_z in .CRT$XLA / .CRT$XLZ); .CRT$XLF sorts between them,
 * the slot winpthreads itself uses. */
__attribute__((used, section(".CRT$XLF"))) const PIMAGE_TLS_CALLBACK rx_shim_xl_f = rx_shim_tls_callback;
#endif
