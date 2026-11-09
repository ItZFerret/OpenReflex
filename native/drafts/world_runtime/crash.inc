// Crash capture for libreflexworld (additive exports; see reflex_world.h "crash reports"). Included by plugin_api.cpp.
// Adapted from Open Unleashed's native/src/crash.c (the same user's code), for the Reflex plugin:
//
// reflex_world_crash_install(dir, session, build) installs process-wide handlers once the player has loaded the plugin:
//   macOS / POSIX: sigaction for SIGSEGV SIGBUS SIGILL SIGFPE SIGABRT SIGTRAP (SA_SIGINFO | SA_ONSTACK)
//   Windows:       a first vectored exception handler, SetUnhandledExceptionFilter and the CRT's SIGABRT
// and, on a crash, writes <dir>/crash-<session>.txt (Windows also crash-<session>.dmp) with the signal, the fault address,
// the registers, a frame-pointer backtrace with module + offset (symbolicate with REFLEX/scripts/symbolicate_crash.sh and
// the build's symbols under REFLEX/builds/symbols/<build id>/), the plugin's load address and LC_UUID, the context the game
// set (reflex_world_crash_set_context: scene / event / world / pacing ...) and a ring of the last notes
// (reflex_world_crash_note). Afterwards it chains to the handler installed before (Mono's / Unity's), so the crash is never
// swallowed.
//
// Only faults whose stack passes through libreflexworld are reported from SIGSEGV / SIGBUS / SIGILL / SIGFPE / SIGTRAP: Mono
// turns faults in managed code into NullReferenceException through the same signals. The JIT's AArch64 blocks (jit.cpp) live
// in an anonymous MAP_JIT region, but they never touch x29, so the frame chain from a fault in translated code still reaches
// the plugin's own frames (vm.cpp's dispatch). SIGABRT is always reported (once): Unity's / Mono's crash path ends there.
//
// POSIX writing is async-signal-safe: the directory is opened at install (openat at crash time), the text is formatted by
// hand into a stack buffer and written with write(); dladdr / backtrace_symbols_fd are the usual compromises (no malloc).
#if defined(__APPLE__)
#  define _DARWIN_C_SOURCE 1
#elif !defined(_WIN32)
#  define _GNU_SOURCE 1
#endif


#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#  if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0602
#    undef _WIN32_WINNT
#    define _WIN32_WINNT 0x0602 /* GetCurrentThreadStackLimits */
#  endif
#  include <windows.h>
#  include <dbghelp.h>
#else
#  include <dlfcn.h>
#  include <errno.h>
#  include <execinfo.h>
#  include <fcntl.h>
#  include <pthread.h>
#  include <sys/stat.h>
#  if defined(__APPLE__)
#    include <sys/ucontext.h>
#  else
#    include <ucontext.h>
#  endif
#  include <unistd.h>
#  if defined(__APPLE__)
#    include <mach-o/dyld.h>
#    include <mach-o/loader.h>
#    include <sys/sysctl.h>
#  endif
#endif

namespace rwcrash {

static char s_error[256];
static void rwc_strlcpy(char* d, const char* s, size_t n) { if (!n) return; size_t k = 0; while (s && s[k] && k + 1 < n) { d[k] = s[k]; k++; } d[k] = 0; }
static void rwc_clear_error() { s_error[0] = 0; }
static int rwc_fail(const char* what, const char* arg = nullptr) { rwc_strlcpy(s_error, what, sizeof s_error); if (arg) { size_t n = strlen(s_error); rwc_strlcpy(s_error + n, arg, sizeof s_error - n); } return -1; }
// the reflex_world_* string convention: returns the full length; copies (NUL-terminated, truncated) when out / size allow
static int rwc_copy_out(char* out, int size, const char* t, size_t n) { if (out && size > 0) { size_t k = n < size_t(size - 1) ? n : size_t(size - 1); memcpy(out, t, k); out[k] = 0; } return int(n); }

/* ---------------------------------------------------------------------------------------------- state */

#define CTX_SLOTS 16
#define RING 48
#define BT_MAX 64

typedef struct { char key[24]; char val[200]; } ctx_slot;
typedef struct { const char *name; char text[64]; uint32_t count; uint32_t ms; } ring_entry;

static ctx_slot s_ctx[CTX_SLOTS];
static ring_entry s_ring[RING];
static volatile uint32_t s_ring_next;     /* entries written so far */
static int s_installed;
static volatile int s_in_handler, s_written;
static char s_session[64] = "unknown", s_build[160] = "unknown";
static char s_home[512];                  /* the user's home folder, written as "~" */
static uintptr_t s_mod_base, s_mod_end;   /* the core's image (text) range */
static char s_mod_id[64] = "?";           /* macOS LC_UUID / Windows PE timestamp + size */
static uint64_t s_t0_ms;

#if defined(_WIN32)
static wchar_t s_txt_path[1024], s_dmp_path[1024];
static PVOID s_veh;
static LPTOP_LEVEL_EXCEPTION_FILTER s_prev_uef;
typedef BOOL(WINAPI *minidump_fn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
	PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
static minidump_fn s_minidump;
static void (*s_prev_abrt)(int);
#else
static int s_dirfd = -1;
static char s_txt_name[128];
static const int s_signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};
#define NSIG_HANDLED ((int)(sizeof s_signals / sizeof s_signals[0]))
static struct sigaction s_prev[NSIG_HANDLED];
#endif

static uint64_t now_ms(void)
{
#if defined(_WIN32)
	return GetTickCount64();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
#endif
}

/* ---------------------------------------------------------------------------------------------- breadcrumbs */

void rwc_trace(const char *fn)
{
	uint32_t n = s_ring_next;
	uint32_t ms = s_t0_ms ? (uint32_t)(now_ms() - s_t0_ms) : 0;
	if (n > 0) {
		ring_entry *last = &s_ring[(n - 1) % RING];
		if (last->name == fn && !last->text[0]) { last->count++; last->ms = ms; return; } /* repeated frame calls */
	}
	ring_entry *e = &s_ring[n % RING];
	e->name = fn; e->text[0] = 0; e->count = 1; e->ms = ms;
	s_ring_next = n + 1;
}

extern "C" REFLEX_API void reflex_world_crash_note(const char *text)
{
	uint32_t n = s_ring_next;
	ring_entry *e = &s_ring[n % RING];
	e->name = "note";
	rwc_strlcpy(e->text, text ? text : "", sizeof e->text);
	e->count = 1; e->ms = s_t0_ms ? (uint32_t)(now_ms() - s_t0_ms) : 0;
	s_ring_next = n + 1;
}

extern "C" REFLEX_API void reflex_world_crash_set_context(const char *key, const char *value)
{
	if (!key || !*key) return;
	int free_slot = -1;
	for (int i = 0; i < CTX_SLOTS; i++) {
		if (s_ctx[i].key[0] && strcmp(s_ctx[i].key, key) == 0) {
			if (!value || !*value) { s_ctx[i].val[0] = 0; s_ctx[i].key[0] = 0; }
			else rwc_strlcpy(s_ctx[i].val, value, sizeof s_ctx[i].val);
			return;
		}
		if (!s_ctx[i].key[0] && free_slot < 0) free_slot = i;
	}
	if (!value || !*value || free_slot < 0) return;
	rwc_strlcpy(s_ctx[free_slot].val, value, sizeof s_ctx[free_slot].val);
	rwc_strlcpy(s_ctx[free_slot].key, key, sizeof s_ctx[free_slot].key);
}

/* ---------------------------------------------------------------------------------------------- the writer */

typedef struct {
#if defined(_WIN32)
	HANDLE h;
#else
	int fd;
#endif
	char buf[2048];
	int n;
} writer;

static void w_flush(writer *w)
{
	if (w->n <= 0) return;
#if defined(_WIN32)
	DWORD done;
	if (w->h != INVALID_HANDLE_VALUE) WriteFile(w->h, w->buf, (DWORD)w->n, &done, NULL);
#else
	int off = 0;
	while (off < w->n) {
		ssize_t r = write(w->fd, w->buf + off, (size_t)(w->n - off));
		if (r < 0 && errno == EINTR) continue;
		if (r <= 0) break;
		off += (int)r;
	}
#endif
	w->n = 0;
}

static void w_ch(writer *w, char c)
{
	if (w->n >= (int)sizeof w->buf) w_flush(w);
	w->buf[w->n++] = c;
}

static void w_str(writer *w, const char *s)
{
	if (!s) s = "(null)";
	while (*s) w_ch(w, *s++);
}

/* a path with the home folder as "~" */
static void w_path(writer *w, const char *s)
{
	if (!s) { w_str(w, "(null)"); return; }
	size_t hl = strlen(s_home);
	if (hl > 1 && strncmp(s, s_home, hl) == 0 && (s[hl] == '/' || s[hl] == '\\' || s[hl] == 0)) { w_ch(w, '~'); s += hl; }
	w_str(w, s);
}

static void w_hex(writer *w, uint64_t v, int digits)
{
	char t[17];
	int n = 0;
	do { t[n++] = "0123456789abcdef"[v & 15]; v >>= 4; } while (v && n < 16);
	while (n < digits && n < 16) t[n++] = '0';
	w_str(w, "0x");
	while (n) w_ch(w, t[--n]);
}

static void w_dec(writer *w, int64_t v)
{
	char t[24];
	int n = 0;
	uint64_t u = v < 0 ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
	if (v < 0) w_ch(w, '-');
	do { t[n++] = (char)('0' + u % 10); u /= 10; } while (u);
	while (n) w_ch(w, t[--n]);
}

static void w_dec2(writer *w, int v) { w_ch(w, (char)('0' + v / 10 % 10)); w_ch(w, (char)('0' + v % 10)); }

/* UTC wall time without localtime (not async-signal-safe): days -> civil date */
static void w_utc(writer *w, int64_t t)
{
	int64_t days = t / 86400, sec = t % 86400;
	if (sec < 0) { sec += 86400; days--; }
	days += 719468;
	int64_t era = (days >= 0 ? days : days - 146096) / 146097;
	int64_t doe = days - era * 146097;
	int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int64_t y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
	int d = (int)(doy - (153 * mp + 2) / 5 + 1), m = (int)(mp < 10 ? mp + 3 : mp - 9);
	if (m <= 2) y++;
	w_dec(w, y); w_ch(w, '-'); w_dec2(w, m); w_ch(w, '-'); w_dec2(w, d); w_ch(w, ' ');
	w_dec2(w, (int)(sec / 3600)); w_ch(w, ':'); w_dec2(w, (int)(sec / 60 % 60)); w_ch(w, ':'); w_dec2(w, (int)(sec % 60));
	w_str(w, " UTC");
}

static int in_core(uintptr_t a) { return s_mod_base && a >= s_mod_base && a < s_mod_end; }

static void w_header(writer *w, const char *what)
{
	w_str(w, "MX vs ATV Reflex (Unity port) - native crash report\n");
	w_str(w, "report:   "); w_str(w, what); w_ch(w, '\n');
	w_str(w, "time:     "); w_utc(w, (int64_t)time(NULL)); w_ch(w, '\n');
	w_str(w, "session:  "); w_str(w, s_session); w_ch(w, '\n');
	w_str(w, "build:    "); w_str(w, s_build); w_ch(w, '\n');
	w_str(w, "plugin:   libreflexworld API "); w_dec(w, REFLEX_WORLD_API_VERSION); w_str(w, ", id "); w_str(w, s_mod_id);
	w_str(w, ", loaded at "); w_hex(w, s_mod_base, 0); w_str(w, " .. "); w_hex(w, s_mod_end, 0); w_ch(w, '\n');
	w_str(w, "uptime:   "); w_dec(w, (int64_t)(s_t0_ms ? now_ms() - s_t0_ms : 0)); w_str(w, " ms since the plugin's crash handler was installed\n");
}

static void w_context(writer *w)
{
	w_str(w, "\n-- last game context\n");
	int any = 0;
	for (int i = 0; i < CTX_SLOTS; i++) {
		if (!s_ctx[i].key[0]) continue;
		any = 1;
		w_str(w, "  "); w_str(w, s_ctx[i].key); w_str(w, ": "); w_path(w, s_ctx[i].val); w_ch(w, '\n');
	}
	if (!any) w_str(w, "  (none set)\n");
	w_str(w, "\n-- recent native calls (oldest first; xN = repeated, t = ms since install)\n");
	uint32_t n = s_ring_next, first = n > RING ? n - RING : 0;
	if (n == 0) w_str(w, "  (none)\n");
	for (uint32_t i = first; i < n; i++) {
		const ring_entry *e = &s_ring[i % RING];
		w_str(w, "  t="); w_dec(w, e->ms); w_str(w, "  "); w_str(w, e->name ? e->name : "?");
		if (e->text[0]) { w_str(w, ": "); w_path(w, e->text); }
		if (e->count > 1) { w_str(w, "  x"); w_dec(w, e->count); }
		w_ch(w, '\n');
	}
}

/* ================================================================================================ POSIX */
#if !defined(_WIN32)

static const char *sig_name(int s)
{
	switch (s) {
	case SIGSEGV: return "SIGSEGV"; case SIGBUS: return "SIGBUS"; case SIGILL: return "SIGILL";
	case SIGFPE: return "SIGFPE"; case SIGABRT: return "SIGABRT"; case SIGTRAP: return "SIGTRAP";
	default: return "signal";
	}
}

static const char *code_name(int s, int c)
{
	if (s == SIGSEGV) return c == SEGV_MAPERR ? "SEGV_MAPERR (address not mapped)" : c == SEGV_ACCERR ? "SEGV_ACCERR (no access)" : "";
	if (s == SIGBUS) return c == BUS_ADRALN ? "BUS_ADRALN" : c == BUS_ADRERR ? "BUS_ADRERR" : c == BUS_OBJERR ? "BUS_OBJERR" : "";
	if (s == SIGFPE) return c == FPE_INTDIV ? "FPE_INTDIV (integer divide by zero)" : c == FPE_FLTDIV ? "FPE_FLTDIV" : c == FPE_FLTINV ? "FPE_FLTINV" : "";
	if (s == SIGILL) return c == ILL_ILLOPC ? "ILL_ILLOPC" : c == ILL_ILLOPN ? "ILL_ILLOPN" : c == ILL_PRVOPC ? "ILL_PRVOPC" : "";
	return "";
}

static uintptr_t strip_pac(uintptr_t a)
{
#if defined(__aarch64__)
	return a & 0x00007fffffffffffull;
#else
	return a;
#endif
}

typedef struct { uintptr_t pc, lr, fp, sp; } regs4;

static regs4 uc_regs(void *ucv)
{
	regs4 r = {0, 0, 0, 0};
	ucontext_t *uc = (ucontext_t *)ucv;
	if (!uc) return r;
#if defined(__APPLE__) && defined(__aarch64__)
	if (!uc->uc_mcontext) return r;
	r.pc = (uintptr_t)uc->uc_mcontext->__ss.__pc; r.lr = (uintptr_t)uc->uc_mcontext->__ss.__lr;
	r.fp = (uintptr_t)uc->uc_mcontext->__ss.__fp; r.sp = (uintptr_t)uc->uc_mcontext->__ss.__sp;
#elif defined(__APPLE__) && defined(__x86_64__)
	if (!uc->uc_mcontext) return r;
	r.pc = (uintptr_t)uc->uc_mcontext->__ss.__rip; r.fp = (uintptr_t)uc->uc_mcontext->__ss.__rbp;
	r.sp = (uintptr_t)uc->uc_mcontext->__ss.__rsp;
#elif defined(__linux__) && defined(__aarch64__)
	r.pc = (uintptr_t)uc->uc_mcontext.pc; r.lr = (uintptr_t)uc->uc_mcontext.regs[30];
	r.fp = (uintptr_t)uc->uc_mcontext.regs[29]; r.sp = (uintptr_t)uc->uc_mcontext.sp;
#elif defined(__linux__) && defined(__x86_64__)
	r.pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP]; r.fp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
	r.sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
#endif
	return r;
}

/* the calling thread's stack bounds (the frame walk only reads inside them) */
static void stack_bounds(uintptr_t *lo, uintptr_t *hi)
{
#if defined(__APPLE__)
	pthread_t t = pthread_self();
	uintptr_t top = (uintptr_t)pthread_get_stackaddr_np(t);
	size_t size = pthread_get_stacksize_np(t);
	*hi = top; *lo = top - size;
#else
	*lo = 0; *hi = 0;
	pthread_attr_t a;
	if (pthread_getattr_np(pthread_self(), &a) == 0) {
		void *addr; size_t size;
		if (pthread_attr_getstack(&a, &addr, &size) == 0) { *lo = (uintptr_t)addr; *hi = (uintptr_t)addr + size; }
		pthread_attr_destroy(&a);
	}
#endif
}

/* frame-pointer walk from the faulting context: pc, lr (arm64 leaf), then the fp chain */
static int walk(void *uc, uintptr_t *out, int max)
{
	regs4 r = uc_regs(uc);
	int n = 0;
	if (!r.pc) return 0;
	out[n++] = strip_pac(r.pc);
	if (r.lr && n < max) out[n++] = strip_pac(r.lr);
	uintptr_t lo, hi, fp = r.fp;
	stack_bounds(&lo, &hi);
	while (n < max && fp && (fp & 7) == 0 && (!hi || (fp >= lo && fp + 16 <= hi))) {
		uintptr_t next = ((uintptr_t *)fp)[0], ret = strip_pac(((uintptr_t *)fp)[1]);
		if (!ret) break;
		if (!(n == 2 && ret == out[1])) out[n++] = ret; /* the arm64 lr is often the first saved return */
		if (next <= fp) break;
		fp = next;
	}
	return n;
}

static void w_frame(writer *w, int i, uintptr_t a)
{
	w_str(w, "  #"); w_dec2(w, i); w_ch(w, ' '); w_hex(w, a, 16); w_ch(w, ' ');
	Dl_info di;
	memset(&di, 0, sizeof di);
	if (dladdr((void *)(i > 0 ? a - 1 : a), &di) && di.dli_fbase) {
		const char *f = di.dli_fname ? di.dli_fname : "?", *b = strrchr(f, '/');
		w_str(w, b ? b + 1 : f); w_str(w, " + "); w_hex(w, a - (uintptr_t)di.dli_fbase, 0);
		if (di.dli_sname && di.dli_saddr) {
			/* the nearest symbol the image's symbol table has (inlined / static functions may show their caller) */
			w_str(w, "   ("); w_str(w, di.dli_sname); w_str(w, " + ");
			w_dec(w, (int64_t)(a - (uintptr_t)di.dli_saddr)); w_ch(w, ')');
		}
		if (in_core(a)) w_str(w, "   [libreflexworld]");
	} else w_str(w, "???");
	w_ch(w, '\n');
}

static int stack_has_core(void *uc)
{
	uintptr_t f[24];
	int n = walk(uc, f, 24);
	for (int i = 0; i < n; i++) if (in_core(f[i])) return 1;
	return 0;
}

static void write_report_posix(int sig, siginfo_t *si, void *uc, int ours)
{
	if (s_dirfd < 0) return;
	int fd = openat(s_dirfd, s_txt_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) return;
	writer w;
	w.fd = fd; w.n = 0;
	w_header(&w, ours ? "a crash inside libreflexworld" : "the process aborted (Unity / Mono crash handling or abort())");
	w_str(&w, "signal:   "); w_str(&w, sig_name(sig)); w_str(&w, " ("); w_dec(&w, sig); w_str(&w, "), code ");
	w_dec(&w, si ? si->si_code : 0); w_ch(&w, ' '); w_str(&w, si ? code_name(sig, si->si_code) : ""); w_ch(&w, '\n');
	w_str(&w, "fault:    address "); w_hex(&w, si ? (uint64_t)(uintptr_t)si->si_addr : 0, 0); w_ch(&w, '\n');
	w_str(&w, "process:  pid "); w_dec(&w, getpid());
#if defined(__APPLE__)
	uint64_t tid = 0;
	pthread_threadid_np(NULL, &tid);
	w_str(&w, ", thread "); w_dec(&w, (int64_t)tid);
	w_str(&w, pthread_main_np() ? " (main)" : "");
#endif
	w_ch(&w, '\n');
	regs4 r = uc_regs(uc);
	w_str(&w, "pc "); w_hex(&w, r.pc, 16); w_str(&w, "  lr "); w_hex(&w, r.lr, 16);
	w_str(&w, "  fp "); w_hex(&w, r.fp, 16); w_str(&w, "  sp "); w_hex(&w, r.sp, 16); w_ch(&w, '\n');
#if defined(__APPLE__) && defined(__aarch64__)
	if (uc && ((ucontext_t *)uc)->uc_mcontext) {
		const __uint64_t *x = ((ucontext_t *)uc)->uc_mcontext->__ss.__x;
		for (int i = 0; i < 29; i++) {
			w_ch(&w, 'x'); w_dec(&w, i); w_str(&w, i < 10 ? "  " : " "); w_hex(&w, x[i], 16);
			w_str(&w, (i % 4 == 3 || i == 28) ? "\n" : "  ");
		}
	}
#endif
	uintptr_t f[BT_MAX];
	int n = walk(uc, f, BT_MAX);
	w_str(&w, "\n-- backtrace of the crashed thread (frame pointers; module + offset)\n");
	for (int i = 0; i < n; i++) w_frame(&w, i, f[i]);
	if (!n) w_str(&w, "  (no context)\n");
	w_context(&w);
	w_str(&w, "\n-- handler stack (backtrace_symbols)\n");
	w_flush(&w);
	void *bt[BT_MAX];
	int m = backtrace(bt, BT_MAX);
	backtrace_symbols_fd(bt, m, fd);
	w_str(&w, "\nSymbolicate: REFLEX/scripts/symbolicate_crash.sh <this file> (uses REFLEX/builds/symbols/<build id>/).\n");
	w_flush(&w);
	close(fd);
}

static int sig_index(int sig)
{
	for (int i = 0; i < NSIG_HANDLED; i++) if (s_signals[i] == sig) return i;
	return -1;
}

static void chain(int sig, siginfo_t *si, void *uc)
{
	int i = sig_index(sig);
	struct sigaction prev = s_prev[i < 0 ? 0 : i];
	if (i < 0) return;
	/* hand the signal over for good: the previous disposition becomes the installed one */
	sigaction(sig, &prev, NULL);
	if (prev.sa_flags & SA_SIGINFO) {
		if (prev.sa_sigaction) prev.sa_sigaction(sig, si, uc);
		return;
	}
	if (prev.sa_handler == SIG_IGN) { signal(sig, SIG_DFL); }
	else if (prev.sa_handler != SIG_DFL && prev.sa_handler) { prev.sa_handler(sig); return; }
	/* default disposition: returning re-executes the fault (synchronous signals) and kills the process; an abort()
	 * or a raised signal is raised again */
	if (sig == SIGABRT || (si && si->si_code <= 0)) raise(sig);
}

static void on_signal(int sig, siginfo_t *si, void *uc)
{
	int saved_errno = errno;
	if (s_in_handler || s_written) {
		/* a second fault inside the handler, or the report is out: straight to the previous handler */
		chain(sig, si, uc);
		errno = saved_errno;
		return;
	}
	s_in_handler = 1;
	int core = stack_has_core(uc);
	if (core || sig == SIGABRT) {
		write_report_posix(sig, si, uc, core);
		s_written = 1;
		s_in_handler = 0;
		chain(sig, si, uc);
	} else {
		s_in_handler = 0;
		/* not the core's: Mono / Unity decide (a managed NullReferenceException resumes from here). The handler stays
		 * installed: call the previous one without uninstalling ours */
		int i = sig_index(sig);
		if (i >= 0) {
			struct sigaction *p = &s_prev[i];
			if (p->sa_flags & SA_SIGINFO) { if (p->sa_sigaction) p->sa_sigaction(sig, si, uc); }
			else if (p->sa_handler != SIG_DFL && p->sa_handler != SIG_IGN && p->sa_handler) p->sa_handler(sig);
			else chain(sig, si, uc);
		}
	}
	errno = saved_errno;
}

static void core_range(void)
{
	Dl_info di;
	if (!dladdr((void *)&core_range, &di) || !di.dli_fbase) return;
	s_mod_base = (uintptr_t)di.dli_fbase;
	s_mod_end = s_mod_base + 1;
#if defined(__APPLE__)
	const struct mach_header_64 *mh = (const struct mach_header_64 *)di.dli_fbase;
	if (mh->magic != MH_MAGIC_64) return;
	const uint8_t *p = (const uint8_t *)(mh + 1);
	for (uint32_t i = 0; i < mh->ncmds; i++) {
		const struct load_command *lc = (const struct load_command *)p;
		if (lc->cmd == LC_SEGMENT_64) {
			const struct segment_command_64 *sc = (const struct segment_command_64 *)p;
			if (strcmp(sc->segname, "__TEXT") == 0) s_mod_end = s_mod_base + sc->vmsize;
		} else if (lc->cmd == LC_UUID) {
			const uint8_t *u = ((const struct uuid_command *)p)->uuid;
			char *o = s_mod_id;
			for (int k = 0; k < 16; k++) {
				*o++ = "0123456789ABCDEF"[u[k] >> 4]; *o++ = "0123456789ABCDEF"[u[k] & 15];
				if (k == 3 || k == 5 || k == 7 || k == 9) *o++ = '-';
			}
			*o = 0;
		}
		p += lc->cmdsize;
	}
#else
	s_mod_end = s_mod_base + (64u << 20);
#endif
}

static int install_platform(const char *dir)
{
	if (s_dirfd >= 0) close(s_dirfd);
	s_dirfd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (s_dirfd < 0) return rwc_fail("crash: cannot open ", dir);
	snprintf(s_txt_name, sizeof s_txt_name, "crash-%s.txt", s_session);
	core_range();
	const char *h = getenv("HOME");
	if (h) rwc_strlcpy(s_home, h, sizeof s_home);
	{ void *bt[2]; backtrace(bt, 2); } /* loads what backtrace() needs now, not in the handler */
	for (int i = 0; i < NSIG_HANDLED; i++) {
		struct sigaction sa, old;
		memset(&sa, 0, sizeof sa);
		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = on_signal;
		sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
		if (sigaction(s_signals[i], NULL, &old) == 0 && (old.sa_flags & SA_SIGINFO) && old.sa_sigaction == on_signal)
			continue; /* already ours (re-install keeps the first previous handler) */
		if (sigaction(s_signals[i], &sa, &old) == 0) s_prev[i] = old;
	}
	return 0;
}

static int handlers_ours(void)
{
	int n = 0;
	for (int i = 0; i < NSIG_HANDLED; i++) {
		struct sigaction cur;
		if (sigaction(s_signals[i], NULL, &cur) == 0 && (cur.sa_flags & SA_SIGINFO) && cur.sa_sigaction == on_signal) n++;
	}
	return n;
}

/* ================================================================================================ Windows */
#else

static const char *exc_name(DWORD c)
{
	switch (c) {
	case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
	case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
	case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
	case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
	case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
	case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
	case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
	case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
	case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
	case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN";
	case 0xC0000374: return "STATUS_HEAP_CORRUPTION";
	default: return "exception";
	}
}

static int fatal_code(DWORD c)
{
	switch (c) {
	case EXCEPTION_ACCESS_VIOLATION: case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_INT_DIVIDE_BY_ZERO: case EXCEPTION_STACK_OVERFLOW: case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: case EXCEPTION_DATATYPE_MISALIGNMENT: case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	case EXCEPTION_FLT_INVALID_OPERATION: case EXCEPTION_NONCONTINUABLE_EXCEPTION: case 0xC0000409: case 0xC0000374:
		return 1;
	default: return 0;
	}
}

/* unwinds a copy of ctx with the x64 unwind tables (.pdata, which MinGW-w64 emits) */
static int walk_ctx(const CONTEXT *ctx0, uintptr_t *out, int max)
{
	int n = 0;
#if defined(_M_X64) || defined(__x86_64__)
	CONTEXT c = *ctx0;
	ULONG_PTR lo = 0, hi = 0;
	GetCurrentThreadStackLimits(&lo, &hi);
	while (n < max && c.Rip) {
		out[n++] = (uintptr_t)c.Rip;
		DWORD64 base = 0;
		PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &base, NULL);
		if (fn) {
			PVOID handler_data = NULL;
			DWORD64 establisher = 0;
			RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, fn, &c, &handler_data, &establisher, NULL);
		} else {
			/* a leaf (or code without unwind data): the return address is at rsp */
			if (c.Rsp < lo || c.Rsp + 8 > hi) break;
			c.Rip = *(DWORD64 *)c.Rsp;
			c.Rsp += 8;
		}
		if (c.Rsp < lo || c.Rsp > hi) break;
	}
#else
	(void)ctx0; (void)max;
#endif
	return n;
}

static void w_wide_base(writer *w, const wchar_t *p)
{
	const wchar_t *b = p;
	for (const wchar_t *q = p; *q; q++) if (*q == L'\\' || *q == L'/') b = q + 1;
	char tmp[260];
	int k = WideCharToMultiByte(CP_UTF8, 0, b, -1, tmp, sizeof tmp, NULL, NULL);
	w_str(w, k > 0 ? tmp : "?");
}

static void w_frame(writer *w, int i, uintptr_t a)
{
	w_str(w, "  #"); w_dec2(w, i); w_ch(w, ' '); w_hex(w, a, 16); w_ch(w, ' ');
	HMODULE m = NULL;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                       (LPCWSTR)(i > 0 ? a - 1 : a), &m) && m) {
		wchar_t path[MAX_PATH];
		DWORD k = GetModuleFileNameW(m, path, MAX_PATH);
		if (k) w_wide_base(w, path); else w_str(w, "?");
		w_str(w, " + "); w_hex(w, a - (uintptr_t)m, 0);
		if (in_core(a)) w_str(w, "   [libreflexworld]");
	} else w_str(w, "???");
	w_ch(w, '\n');
}

static void write_minidump(EXCEPTION_POINTERS *ep)
{
	if (!s_minidump || !s_dmp_path[0]) return;
	HANDLE h = CreateFileW(s_dmp_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	MINIDUMP_EXCEPTION_INFORMATION mei;
	mei.ThreadId = GetCurrentThreadId();
	mei.ExceptionPointers = ep;
	mei.ClientPointers = FALSE;
	s_minidump(GetCurrentProcess(), GetCurrentProcessId(), h,
	           (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
	           ep ? &mei : NULL, NULL, NULL);
	CloseHandle(h);
}

static void write_report_win(EXCEPTION_POINTERS *ep, const char *what)
{
	HANDLE h = CreateFileW(s_txt_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) return;
	writer w;
	w.h = h; w.n = 0;
	w_header(&w, what);
	if (ep && ep->ExceptionRecord) {
		EXCEPTION_RECORD *er = ep->ExceptionRecord;
		w_str(&w, "exception: "); w_str(&w, exc_name(er->ExceptionCode)); w_str(&w, " ("); w_hex(&w, er->ExceptionCode, 8);
		w_str(&w, ") at "); w_hex(&w, (uint64_t)(uintptr_t)er->ExceptionAddress, 16); w_ch(&w, '\n');
		if ((er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || er->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && er->NumberParameters >= 2) {
			w_str(&w, "fault:    ");
			w_str(&w, er->ExceptionInformation[0] == 0 ? "read of " : er->ExceptionInformation[0] == 1 ? "write to " : "execute at ");
			w_hex(&w, er->ExceptionInformation[1], 0); w_ch(&w, '\n');
		}
	}
	w_str(&w, "process:  pid "); w_dec(&w, (int64_t)GetCurrentProcessId()); w_str(&w, ", thread ");
	w_dec(&w, (int64_t)GetCurrentThreadId()); w_ch(&w, '\n');
	uintptr_t f[BT_MAX];
	int n = 0;
	CONTEXT here;
	const CONTEXT *ctx = ep ? ep->ContextRecord : NULL;
	if (!ctx) { RtlCaptureContext(&here); ctx = &here; }
#if defined(_M_X64) || defined(__x86_64__)
	w_str(&w, "rip "); w_hex(&w, ctx->Rip, 16); w_str(&w, "  rsp "); w_hex(&w, ctx->Rsp, 16); w_str(&w, "  rbp "); w_hex(&w, ctx->Rbp, 16); w_ch(&w, '\n');
	w_str(&w, "rax "); w_hex(&w, ctx->Rax, 16); w_str(&w, "  rbx "); w_hex(&w, ctx->Rbx, 16); w_str(&w, "  rcx "); w_hex(&w, ctx->Rcx, 16); w_ch(&w, '\n');
	w_str(&w, "rdx "); w_hex(&w, ctx->Rdx, 16); w_str(&w, "  rsi "); w_hex(&w, ctx->Rsi, 16); w_str(&w, "  rdi "); w_hex(&w, ctx->Rdi, 16); w_ch(&w, '\n');
	w_str(&w, "r8  "); w_hex(&w, ctx->R8, 16); w_str(&w, "  r9  "); w_hex(&w, ctx->R9, 16); w_str(&w, "  r10 "); w_hex(&w, ctx->R10, 16); w_ch(&w, '\n');
	w_str(&w, "r11 "); w_hex(&w, ctx->R11, 16); w_str(&w, "  r12 "); w_hex(&w, ctx->R12, 16); w_str(&w, "  r13 "); w_hex(&w, ctx->R13, 16); w_ch(&w, '\n');
	w_str(&w, "r14 "); w_hex(&w, ctx->R14, 16); w_str(&w, "  r15 "); w_hex(&w, ctx->R15, 16); w_ch(&w, '\n');
#endif
	n = walk_ctx(ctx, f, BT_MAX);
	w_str(&w, "\n-- backtrace of the crashed thread (module + RVA)\n");
	for (int i = 0; i < n; i++) w_frame(&w, i, f[i]);
	if (!n) w_str(&w, "  (no frames)\n");
	w_context(&w);
	w_str(&w, "\nA minidump of the crash: "); w_str(&w, s_minidump ? "crash-" : "(dbghelp unavailable)");
	if (s_minidump) { w_str(&w, s_session); w_str(&w, ".dmp"); }
	w_str(&w, "\nSymbolicate: REFLEX/scripts/symbolicate_crash.sh <this file> (uses REFLEX/builds/symbols/<build id>/).\n");
	w_flush(&w);
	CloseHandle(h);
}

static int stack_has_core_ctx(const CONTEXT *ctx)
{
	uintptr_t f[24];
	int n = walk_ctx(ctx, f, 24);
	for (int i = 0; i < n; i++) if (in_core(f[i])) return 1;
	return 0;
}

static LONG CALLBACK on_veh(EXCEPTION_POINTERS *ep)
{
	if (!ep || !ep->ExceptionRecord || s_written || s_in_handler) return EXCEPTION_CONTINUE_SEARCH;
	DWORD code = ep->ExceptionRecord->ExceptionCode;
	if (!fatal_code(code)) return EXCEPTION_CONTINUE_SEARCH;
	if (!in_core((uintptr_t)ep->ExceptionRecord->ExceptionAddress) && !(ep->ContextRecord && stack_has_core_ctx(ep->ContextRecord)))
		return EXCEPTION_CONTINUE_SEARCH; /* Mono's NullReferenceException and everything else: not ours */
	s_in_handler = 1;
	write_report_win(ep, "a crash inside libreflexworld");
	write_minidump(ep);
	s_written = 1;
	s_in_handler = 0;
	return EXCEPTION_CONTINUE_SEARCH; /* Unity's crash handler and Windows Error Reporting go on */
}

static LONG WINAPI on_uef(EXCEPTION_POINTERS *ep)
{
	if (!s_written && !s_in_handler) {
		s_in_handler = 1;
		int core = ep && ep->ExceptionRecord && (in_core((uintptr_t)ep->ExceptionRecord->ExceptionAddress) ||
		                                         (ep->ContextRecord && stack_has_core_ctx(ep->ContextRecord)));
		write_report_win(ep, core ? "a crash inside libreflexworld (unhandled)" : "an unhandled exception (outside libreflexworld)");
		write_minidump(ep);
		s_written = 1;
		s_in_handler = 0;
	}
	return s_prev_uef ? s_prev_uef(ep) : EXCEPTION_CONTINUE_SEARCH;
}

static void on_abort(int sig)
{
	if (!s_written && !s_in_handler) {
		s_in_handler = 1;
		write_report_win(NULL, "abort() inside the core's C runtime");
		write_minidump(NULL);
		s_written = 1;
		s_in_handler = 0;
	}
	signal(sig, s_prev_abrt && s_prev_abrt != SIG_ERR ? s_prev_abrt : SIG_DFL);
	raise(sig);
}

static void core_range(void)
{
	HMODULE m = NULL;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        (LPCWSTR)(void *)&core_range, &m) || !m) return;
	s_mod_base = (uintptr_t)m;
	const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)m;
	const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const uint8_t *)m + dos->e_lfanew);
	s_mod_end = s_mod_base + nt->OptionalHeader.SizeOfImage;
	snprintf(s_mod_id, sizeof s_mod_id, "pe-%08lX-%lX", (unsigned long)nt->FileHeader.TimeDateStamp,
	         (unsigned long)nt->OptionalHeader.SizeOfImage);
}

static int install_platform(const char *dir)
{
	wchar_t wdir[900];
	if (!MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, 900)) return rwc_fail("crash: bad directory");
	wchar_t wses[64];
	MultiByteToWideChar(CP_UTF8, 0, s_session, -1, wses, 64);
	_snwprintf(s_txt_path, 1024, L"%ls\\crash-%ls.txt", wdir, wses);
	_snwprintf(s_dmp_path, 1024, L"%ls\\crash-%ls.dmp", wdir, wses);
	s_txt_path[1023] = s_dmp_path[1023] = 0;
	core_range();
	const char *up = getenv("USERPROFILE");
	if (up) rwc_strlcpy(s_home, up, sizeof s_home);
	if (!s_minidump) {
		HMODULE dh = LoadLibraryW(L"dbghelp.dll");
		if (dh) s_minidump = (minidump_fn)(void *)GetProcAddress(dh, "MiniDumpWriteDump");
	}
	if (!s_veh) s_veh = AddVectoredExceptionHandler(1, on_veh);
	LPTOP_LEVEL_EXCEPTION_FILTER prev = SetUnhandledExceptionFilter(on_uef);
	if (prev != on_uef) s_prev_uef = prev;
	void (*pa)(int) = signal(SIGABRT, on_abort);
	if (pa != on_abort) s_prev_abrt = pa;
	return 0;
}

static int handlers_ours(void)
{
	LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(on_uef);
	if (cur != on_uef) { s_prev_uef = cur; } /* someone replaced it: ours again, chaining to theirs */
	return (s_veh ? 1 : 0) + 1;
}

#endif

/* ---------------------------------------------------------------------------------------------- API */

extern "C" REFLEX_API int reflex_world_crash_install(const char *dir, const char *session, const char *build)
{
	rwc_clear_error();
	if (!dir || !*dir) return rwc_fail("crash: no directory");
	if (session && *session) {
		size_t k = 0;
		for (const char *p = session; *p && k + 1 < sizeof s_session; p++) {
			char c = *p;
			s_session[k++] = (c == '/' || c == '\\' || c == ':' || c == ' ') ? '_' : c;
		}
		s_session[k] = 0;
	}
	if (build && *build) rwc_strlcpy(s_build, build, sizeof s_build);
	if (!s_t0_ms) s_t0_ms = now_ms();
	if (install_platform(dir) < 0) return -1;
	s_installed = 1;
	rwc_trace("reflex_world_crash_install");
	return 0;
}

extern "C" REFLEX_API int reflex_world_crash_status(char *buf, int size)
{
	char t[400];
	snprintf(t, sizeof t, "installed=%d handlers=%d id=%s base=0x%llx session=%s%s%s", s_installed, s_installed ? handlers_ours() : 0,
	         s_mod_id, (unsigned long long)s_mod_base, s_session, s_error[0] ? " error=" : "", s_error);
	return rwc_copy_out(buf, size, t, strlen(t));
}

#if defined(__GNUC__) || defined(__clang__)
#  define RWC_NOINLINE __attribute__((noinline))
#else
#  define RWC_NOINLINE
#endif

static volatile int *volatile s_crash_ptr; /* NULL: the test's bad write */

static RWC_NOINLINE void crash_test_null_write(int depth)
{
	if (depth > 0) { crash_test_null_write(depth - 1); return; }
	*(s_crash_ptr + 2) = 0x4d58; /* writes to address 8: SIGSEGV / EXCEPTION_ACCESS_VIOLATION inside the core */
}

/* kind 1: a bad write (SIGSEGV / access violation), 2: abort(), 3: integer divide by zero. Debug only: the player's
 * hidden -reflexCrashTest native switch. Never returns for a valid kind. */
extern "C" REFLEX_API int reflex_world_crash_test(int kind)
{
	rwc_trace("reflex_world_crash_test");
	if (kind == 1) { crash_test_null_write(3); }
	else if (kind == 2) { abort(); }
	else if (kind == 3) {
		volatile int z = 0;
#if defined(__aarch64__)
		raise(SIGFPE); /* arm64 integer division does not trap */
#endif
		volatile int r = 1 / z;
		(void)r;
	}
	return -1;
}

}  // namespace rwcrash
