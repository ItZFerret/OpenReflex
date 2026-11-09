/* fmt_sys.h -- the few operating system pieces the readers need (private to src/fmt_*.c): a mutex. */
#ifndef FMT_SYS_H
#define FMT_SYS_H
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
typedef SRWLOCK fmt_mutex;
static inline void fmt_mutex_init(fmt_mutex* m) { InitializeSRWLock(m); }
static inline void fmt_mutex_destroy(fmt_mutex* m) { (void)m; }
static inline void fmt_mutex_lock(fmt_mutex* m) { AcquireSRWLockExclusive(m); }
static inline void fmt_mutex_unlock(fmt_mutex* m) { ReleaseSRWLockExclusive(m); }
#else
#  include <pthread.h>
typedef pthread_mutex_t fmt_mutex;
static inline void fmt_mutex_init(fmt_mutex* m) { pthread_mutex_init(m, NULL); }
static inline void fmt_mutex_destroy(fmt_mutex* m) { pthread_mutex_destroy(m); }
static inline void fmt_mutex_lock(fmt_mutex* m) { pthread_mutex_lock(m); }
static inline void fmt_mutex_unlock(fmt_mutex* m) { pthread_mutex_unlock(m); }
#endif
#endif
