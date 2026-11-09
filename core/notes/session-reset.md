# Session reset: create -> destroy -> create in one process

Why: the Unity editor keeps the process (and the loaded `librxcore.dylib`) alive between Play sessions. Before this change a second
`rx_session_create` after a destroy stopped at a garbage virtual call (`unported game function 7855471e`: an msvcr90 address, because
the CRT import table and many other statics still held the first session's values). Now the second session starts exactly like the
first one.

Verification: `core/tools/rx_api_recreate_test.sh <out dir> <install root> [iterations (3)] [frames (10)] [level]` (see "Test").

## What a session owns, and how each part is reset

`rx_session_destroy` (and a failed or faulted `rx_session_create`) runs `core_release` in `api/rx_session.c`, in this order:

| # | State | Reset |
|---|---|---|
| 1 | The event's level (`fmt_level`) | `fmt_level_free` |
| 2 | Terrain-deformation model cache (`fx_deform.c g_model`: malloc'd mask / brush mips, keyed by guest texture addresses) | `rx_fx_deform_release()` (the only game-file edit: frees the buffers; the pointers would otherwise be lost by step 7) |
| 3 | The install (`fmt_install`: vfs, database handles) | `fmt_install_close` |
| 4 | `rx_image` import slot list (`g_imports`, malloc'd) | `rx_image_reset()` |
| 5 | The guest heap's block table (`rx_heap.c g_blocks`, malloc'd; bump top) | `rx_heap_reset()` now also frees the table (it only emptied it before) |
| 6 | **The guest arena**: everything the original keeps in its image: the EXE's `.data` / `.rdata` / `.tls`, the CRT DLLs' data, the heap, the game stack, the objects built by the startup / session | `rx_mem_shutdown()` unmaps the 4 GB reservation. The next create maps a fresh zero arena and `rx_image_load` / `rx_image_load_crt` re-read the sections from the EXE / vcredist, so `.data` is the pristine loaded state and `.bss` is zero by construction. Host code keeps guest addresses only in statics (step 7), never host pointers into the arena across sessions (a host pointer into the old mapping would dangle: none found in the census; the hash test would show a stale cache as a divergence). |
| 7 | **Host statics** of the core: every file-scope and function-local writable static in `core/src`, `core/game`, `core/api` (the list below) | `rx_reset_restore()` (`src/rx_reset.c`): copies back the image's `__DATA,__data / __bss / __common`, recorded by `rx_reset_capture()` at the first `rx_session_create` (before anything wrote to them) |

The statics are restored from a recording instead of one hand-written reset per variable on purpose: other agents add statics to
game files every day, and a missed one is exactly this bug again (a stale install pointer, a half-built class registry, a counter that
must restart at its initial value). The recording catches them all, including ones added later. What it needs from the code:

- Host memory that statics point to (malloc'd) must be released BEFORE the restore, which only overwrites the pointers
  (steps 1-5). A new static that owns host memory needs a release call in `core_release` (the test cannot see a leak).
- A static that must SURVIVE the restore is marked `RX_RESET_KEEP` (`src/rx_reset.h`: it places the variable in the
  `__DATA,__rxkeep` section, outside the recorded ones). Used for: `g_live`, `g_dirty`, `g_create_error`, `g_guard_old`
  (`api/rx_session.c`) and the recording itself (`rx_reset.c`).
- C++ statics with destructors must not be restored half-way: the only one is `rxf80`'s lazily built constants (`consts()`, three
  `std::vector`s). `rx_reset_capture()` builds them first (`rx_x87_warm()`), so the recording holds them built with their guard
  set. A new C++ static with a constructor / destructor needs the same treatment (or is a bug: the core is C).

### The statics that are restored (census 2026-10-19, `nm -m` of the product objects; all writable `__DATA` symbols)

- Infrastructure: `rx_mem.c g_sp` (the game stack pointer), `rx_base`, `rx_fn.c g_tab / g_count` (the function registry is filled by
  constructors at load: recorded populated, kept), `rx_heap.c` (`g_blocks g_count g_cap g_top`), `rx_image.c` (sections, DLL
  sections, imports), `rx_os.c` (`g_last_error`, TLS slots, `g_next_handle`, `g_next_tid`, suspended-thread list, the time source),
  `rx_d3d.c` (`g_device`, `g_init`), `fmt_lzx.c` (decode tables, rebuilt on demand).
- Game registers and counters: `anim_node.c g_anim_regs`, `fx_particle.c g_fx_regs`, `loop_platform.c g_clock` (the deterministic QPC
  counter), `build_rider_init.c os_create_event.next` (handle counter), `build_scene_anim.c g_holdrand`, `build_alloc.c g_fill`
  (RX_BUILD_FILL cache), `crt_thread.c crt_ptd.ptd`, `crt_printf.c crt_format.o`.
- Session inputs and caches: `build_activity_setup.c g_spec / g_spec_set` (the event spec), `build_activity_race.c event_level` and the
  `host_str` / `track_cstr` scratch buffers of `build_activity_race / world / track.c`, `build_asset.c` (`g_types[].loader`
  registrations, `g_req_install`, trace / between-loads hooks), `build_load.c g_db`, `build_mxgame_init.c g_in`, `build_world.c
  g_install`, `build_scene.c` (`g_rs` = the level's entity reader, class registry `g_classes / g_nclasses`, the "seen" list of
  `rx_scene_create_entity`), `build_globals.c g_unported / g_nunported`, `fx_deform.c g_model / tiles`.
- API: `api/rx_session.c` (`g_live`, `g_dirty`, `g_create_error`, `g_guard_old` are KEEP; nothing else is static).

The static data of the process image is restored as a whole. Consequence for a HOST that links the core statically (the test
executables do): the host's own writable statics are restored too. Keep a test's results in malloc memory or on the stack. In the Unity
editor the core is its own dylib, so only the core's statics are touched.

## Determinism: the one wall clock

`timeGetTime` (winmm, import slot `0x009164b8`, read by `world_node_audio.c 008b98c0` and stored in guest memory) was the only host
wall clock the core read: two runs of the same session differed in those words and in the stack. `rx_os_set_time_source(fn)`
(`src/rx_os.[ch]`) installs a millisecond source; the default (NULL) is still the host's monotonic clock, unchanged behaviour. The
test installs a counter (16 ms per read) after create. The source is a static, so the restore clears it with the session: a host
that wants reproducible runs sets it after every create. Update (inputclock): `rx_session_create` now installs the session's virtual clock (`rx_host_clock_ms`: the same
10 MHz counter `advance_dt` moves, `notes/input-clock.md`) as this source and the platform callbacks in `rx_loop_host`; both are statics, so the restore clears them and the next create installs them again.
The loop's own clock (`loop_qpc` / `loop_get_tick_count`) is already a deterministic counter (`g_clock`).

## Test

`tools/rx_api_recreate_test.c` + `.sh` (build: product link shape like `rx_api_test.sh`). In ONE process, 3 times:
create -> event load -> load frames (at most 10000, until the state leaves LOADING or errors) -> 10 race frames -> destroy. After each
step it hashes the guest arena (every touched non-zero 16 KB page: address + words; split per region image / heap / dll / stack /
high; touched pages come from `mach_vm_page_range_query`, never by faulting the reservation in) and records the event-load result,
session state, load / race / session frame counts and the error text. Iterations 2 and 3 must equal iteration 1 in all of it; after
each destroy `rx_base` is NULL and the heap table is empty. Before the iterations: a create from a missing install returns NULL, and a
second create while a session is open returns NULL; neither may disturb the iterations. The `.sh` then runs a FRESH process with one
iteration and requires its iteration 1 to equal the multi-iteration process's iteration 1 (the reset machinery does not change the
first create either).

Result at the time of writing (product stops during the load frames at whatever game function is unported at that moment; this is
part of the compared state: same address, same frame count, same arena): arena hashes after create / event load / frames identical
in all three iterations, `IDENTICAL`, and equal to the fresh process. Negative control: with the restore disabled the second create
fails with `unported game function 7855471e reached` (the original symptom).

Diagnostics: `RX_RESET_DEBUG=1` prints the byte ranges a restore changes (section offset + address; match against `nm -n` of the
binary). The count in the test output ("static bytes dirtied") is informative only: it includes host pointer values (malloc
addresses) that differ per run.

When a future change breaks the repeatability, the test names the step (create / event load / frames) and region that diverged; then
`RX_RESET_DEBUG` and the per-region hashes locate it. A divergence only in `heap` / `image` after the frames is usually a new wall-clock
or address-dependent value (host pointers stored in the arena would show immediately: none are).

## What is NOT reset (known limits)

- Non-macOS platforms: `rx_reset_available()` is 0 (the recording is Mach-O only: `getsectiondata`); a second create is refused with
  "restart the process" (the old behaviour). A PE (`.data` / `.bss` via the module's section headers) or ELF (`dl_iterate_phdr`)
  walker behind the same two calls is the port; the `RX_RESET_KEEP` macro needs the matching section attribute there.
- Thread-local statics: `fmt_common.c g_error` (last reader error text: only read right after a failing call), `rxf80` `g_P / g_Pw /
  g_check_bits / g_amb` (set on entry of every operation that reads them and restored on exit; only a signal inside the math library
  could leave them changed, and the guard only catches game faults), `api g_guard env pointer` (NULL outside guarded calls).
- Host memory leaked by a game function that was aborted in the middle (a fault / unported function longjmps out of the game code):
  e.g. the temporary inflate buffers of `build_xml.c` / `build_registry.c` when the stop happens inside them. Bounded by one session's
  temporaries and only on the error path.
- Anything the process does outside the core (stdio buffers, signal dispositions: the guard restores its own).
- Concurrency: destroy / create must be called from the simulation thread with no other call in flight (the API's threading rule); the
  restore rewrites statics without locks.
