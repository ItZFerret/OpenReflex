/* rx_reset.h -- returning the core's process-level state to its loaded condition (session destroy / create, rx_api.h).
 *
 * The game's own globals live in the guest arena: rx_mem_shutdown unmaps it and the next rx_image_load rewrites .data / .rdata from
 * the EXE (.bss is the fresh zero mapping), so they are pristine by construction. What is left is the HOST's state: the file-scope
 * and function-local statics of the core (core/game, core/src, core/api: registers, caches, counters, install pointers, one-shot
 * guards ...). Instead of a hand-kept reset hook per static (every new static would need one), the core records its image's
 * writable static data once, before the first session writes anything, and copies it back when a session ends:
 *   rx_reset_capture()   first session create: records __DATA,__data / __bss / __common of the image that holds this code
 *   rx_reset_restore()   session destroy: copies the record back (call it with no game code running)
 * Host memory that statics point to (malloc'd) must be released BEFORE the restore (rx_heap_reset, rx_image_reset, the session's
 * own handles): the restore only overwrites the pointers. Statics that must SURVIVE a restore (the create error text) are marked
 * RX_RESET_KEEP; so is this file's own bookkeeping. Thread-local statics are not covered (they are scratch or self-initializing:
 * core/notes/session-reset.md).
 *
 * Platform: Mach-O sections on macOS; isolated .rxdata/.rxbss PE sections on Windows
 * (core/tools/build_windows.sh). Other platforms refuse a second session create. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#if defined(__APPLE__) && defined(__MACH__)
#  define RX_RESET_AVAILABLE 1
#  define RX_RESET_KEEP __attribute__((section("__DATA,__rxkeep")))   /* a section the capture does not cover */
#elif defined(_WIN32)
#  define RX_RESET_AVAILABLE 1
#  define RX_RESET_KEEP __attribute__((section(".rxkeep")))
#else
#  define RX_RESET_AVAILABLE 0
#  define RX_RESET_KEEP
#endif

int  rx_reset_available(void);
/* Records the pristine statics (once per process; later calls do nothing). Returns 1 when a record exists. Call it before the first
 * session touches any core state; it first warms the lazily built C++ constants (rx_x87_warm) so the record holds them. */
int  rx_reset_capture(void);
/* Copies the record back over the statics. No-op without a record. Returns the number of bytes that differed (0 = the statics were
 * already pristine), for the tests; -1 without a record. */
long rx_reset_restore(void);
/* Number of bytes of the statics that differ from the record, without changing anything (tests / audits); -1 without a record. */
long rx_reset_dirty(void);

#ifdef __cplusplus
}
#endif
