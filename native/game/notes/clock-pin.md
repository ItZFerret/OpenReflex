# clockpin: the opt-in pinned frame clock (oracle)

Owner: agent `clockpin` (2026-10-20). Files: `native/drafts/world_runtime/clock_pin.h` (new),
`boot.cpp` / `boot.h` (the baton boot), `runtime.cpp` / `runtime.h` (the world-image runtime),
`boot_main.cpp` (one diagnostic line), build dir `builds/clockpin`.

## Why

The game's `dt` is `Engine::UpdateTiming` 0084ea80 reading `QueryPerformanceCounter`
(10 MHz) and dividing by `QueryPerformanceFrequency`. `REFLEX_AFTER` counts **frames**,
not simulated time, so a verification run is only meaningful if a frame is defined.
Two oracle paths existed:

| path | clock before this change |
|---|---|
| world-image runtime (`REFLEX_WORLD_TEST`, what `verify.sh` uses) | the CLI calls `World::engine_frame(166667)` for every frame: already an exact 60 Hz clock |
| baton boot (`REFLEX_EVENT...`, `loading_accept.sh`) | instruction-driven virtual time under the cooperative scheduler (`advance_time`, 1 GIPS); under baton, QPC-read / yield logical time; or `frame_dt` when a caller passes one. No fixed frame length |

## The option

```
REFLEX_FIXED_CLOCK=1     pin the clock at 60 Hz   (166667 x 100 ns per frame)
REFLEX_FIXED_CLOCK=60    the same, explicit Hz
REFLEX_FIXED_CLOCK=120   pin another rate (83333 x 100 ns)
REFLEX_CLOCK=<hz>        alias
```

Unset / empty / `0` is the old path, byte for byte. The value is read once when the
`Boot` is constructed and once in `World::load`; nothing else consults the host.

In fixed mode:

- the frame boundary advances virtual time by exactly `10,000,000 / hz` 100 ns units
  (`166667` at 60 Hz); instruction-driven time is off (`advance_time` returns early
  because `frame_dt` is set, `boot.cpp`);
- `QueryPerformanceFrequency` stays the game's 10 MHz (Wine's value), and
  `QueryPerformanceCounter` returns the frame's counter plus 100 ns per repeated read
  **inside** the frame — the original's "first read is the frame, later reads see a
  real wait". `GetTickCount` / `timeGetTime` are derived from the same counter, exactly
  as before. The frame-cap busy wait in `Engine::UpdateTiming` therefore terminates
  (it advances on each counter query); in practice the frame's first `dt` already
  exceeds the 1/60 s cap, so the wait is not entered;
- `World::engine_frame_at(counter, ...)` (absolute-counter replay) opts out of the pin
  (`replay` flag): it keeps the counter it is given.

No time value used by the game is read from the host in either mode. The pin makes the
amount of **simulated** time per frame explicit: one frame = 1/60 s, independent of how
many instructions the frame executes.

## The 3600 / 5400 observation, re-measured

`core/notes/known-differences.md` 13 recorded `tracks version 3600` (state `1f6611fa`,
deform 74) four times and `tracks version 5400` (state `a936214c`, deform 397) under
load. With one binary and one world image this is **not** the host clock: the simulator
never reads it. It is the **frame count**:

| frames | tracks version | state hash | deform |
|---|---|---|---|
| 1200 | 3600 | `1f6611fa80b80831` | 74 |
| 1800 | 5400 | `a936214c3ad9ccf1` | 397 |

`tracks version` is bumped three times per rendered frame (three `TerrainTracks`
buffers unlock in the render path, `runtime.cpp` vf12 of the stand-in), so it is a frame
counter, not a time counter. The chain-4 "idle vs loaded" pair was the 1200-frame quick
run vs the 1800-frame full run (`builds/chain4/verify_rx_anim.log` vs
`final2_rx_anim.log`). `deform version` and the state hash differ for the same reason:
they are different points of the same deterministic run.

The pin is still worth keeping: it turns the boot path's instruction-rate clock into a
defined 60 Hz timeline, so a boot capture can be compared across machines / code
changes, and it removes the only remaining place where "how long a frame took" was an
emergent property. It changes **nothing** for `verify.sh` (world runtime): that path
already advanced exactly 166667 per frame; the pinned and unpinned five-world matrices
are identical (see below).

## Determinism proof (`builds/clockpin/proof.sh`, `boot_proof.sh`)

No-port world command of `verify.sh` (`REFLEX_WORLD_TEST=National_NAT_Concord`,
`REFLEX_DRIVE=1`, `REFLEX_JIT=1`, `REFLEX_MEMHASH=nojobs`, 1200 frames), 4 runs idle and
4 with three CPU-bound loops running (`builds/clockpin/logs/`):

```
idle 1..4 and load 1..4, every run:
  sim 14c50a30 state hash 1f6611fa80b80831 tracks version 3600 deform version 74
  memory hash 1487a52f208b4b26 over 27272 pages
unset (old path), one run: exactly the same line
pre-change CLI (`builds/chain4/snapcli`), unset: exactly the same line
```

Baton boot (`loading_accept.sh`'s no-port boot, `REFLEX_BOOT_SCHED=baton`, event 1),
world and canonical images compared byte for byte (`builds/clockpin/boot/`):

| run | world md5 | canonical md5 | vs old CLI |
|---|---|---|---|
| pre-change CLI, unset (`builds/chain4/snapcli`) | `5c8c8523…` | `c1e55b1a…` | — |
| this CLI, unset, idle | `5c8c8523…` | `c1e55b1a…` | **identical** (default path unchanged) |
| this CLI, unset, 3 loops | `5c8c8523…` | `c1e55b1a…` | identical |
| this CLI, pinned, idle | `75d2b986…` | `6b34d71c…` | different (expected: fixed 60 Hz timeline) |
| this CLI, pinned, 3 loops | `75d2b986…` | `6b34d71c…` | identical to pinned idle |

The unset boot was already reproducible on this machine; the pinned boot is
reproducible too, and both differ from each other because the unpinned boot's logical
clock (QPC reads under baton) and one fixed 1/60 s per frame are different (both
deterministic) timelines.

## Five-world re-runs under the pin (`REFLEX_FIXED_CLOCK=1`)

`verify.sh <builds/clockpin/cli/reflex_boot> <group>` full runs, logs
`builds/clockpin/logs/verify_<group>_{pin,unpinned}.log` (CLI built 2026-10-20 14:02;
`core/build.sh` clean). The pinned and unpinned logs are identical for all three groups
(same counts, same hashes): the world runtime was already on the exact 60 Hz grid. The
differences below are therefore **not** clock artifacts; the pin did not close any of
them.

### rx_anim

| world | ports | mismatching ports | On |
|---|---|---|---|
| Concord (1800) | 54 | 007473f0 3/780, 0074cf70 17/3040 (`95e90000`), + aliases | **DIFFERENT**: native stops, `WORLD STOPPED: execution outside mapped code 95e90000` |
| Concord_ai11 (1500) | 52 | 007473f0 3/789, 0074cf70 17/3076 | **DIFFERENT**: same stop |
| FreeRide_FR_Alpine (1500) | 32 | 0 | **identical** (`1ef28933aa2c03c7`, 25174 pages) |
| SX_Springfield_ai11 (1500) | 54 | 007473f0 3/920, 0074cf70 17/3717 | **DIFFERENT**: same stop |
| Terracross_TRX_Verde_ai11 (1500) | 54 | 007473f0 3/530 | state identical, memory hash differs (`a333caa8…`) = documented mud/particle dead data |

Remaining, real: the **2-dword intro cast matrix** in 007473f0 (calls 137/357/538 at
`[1821f674]`/`[1821f678]`: the port writes `y = Y - 0.24, z = Y`, the original
`y = Y, z = Z`; known-differences 14c) and the **`95e90000` dispatch** in 0074cf70
(call 2101, every world): a script handler slot that is not mapped code. The latter is
what ends the native On run on Concord/ai11/SX. Neither is a clock issue and neither
changed under the pin.

### rx_race

| world | ports | mismatching ports | On |
|---|---|---|---|
| Concord | 50 | 0055f700 775/1800 | **identical** (`a936214c…`, 27563 pages) |
| Concord_ai11 | 48 | 0055f700 784/1500 | **identical** (`fd1d4b45…`, 29081) |
| FreeRide_FR_Alpine | 42 | 0 | **identical** (`1ef28933…`, 25174) |
| SX_Springfield_ai11 | 48 | 0055f700 915/1500 | **identical** (`fe8060ee…`, 26335) |
| Terracross_TRX_Verde_ai11 | 50 | 0055f700 525/1500 | **identical** (`0f1c6fa1…`, 29376) |

All five On comparisons are identical under the pin, and the same holds for the unpinned
run of this tree. The earlier four-world state difference from chain round 4 does not
reproduce here and was **not** a clock artifact (the oracle's clock is deterministic);
it depended on the tree/run at the time and is superseded by this matrix. The remaining
Verify difference is the real `0055f700 ActivityManager_Update` residue: two
`RiderCharacter` words (`+0x70` 0 vs 1, `+0x10` 0x…01ff vs 0x…01ff with bit 0x10000
set), present from call 1 on every race world and in `0084ed20 Engine::Update` too
(cleanup2's finding). It does not propagate to state (On identical).

### rx_loop

| world | ports | mismatching ports (all are 0084ed20 aliases) | On |
|---|---|---|---|
| Concord | 7 | 0084ed20 776/1800 | **identical** (`a936214c…`, 27563) |
| Concord_ai11 | 7 | 0084ed20 784/1500 | **one page**: 29080 vs 29081 (known-differences 2, ConVar page 0x1002b000) |
| FreeRide_FR_Alpine | 7 | 0 | **identical** (`1ef28933…`, 25174) |
| SX_Springfield_ai11 | 7 | 0084ed20 916/1500 | **one page**: 26334 vs 26335 (entry 2) |
| Terracross_TRX_Verde_ai11 | 7 | 0084ed20 1495/1500 | same pages (29376); memory hash differs = entry 1 (mud `+0x6c`) |

The 7 "ports" are the same `0084ed20` under seven registry spellings (engine_mt,
camschema/handlers/misc/race probe hooks, rx_loop). The 776/… mismatches are the same
two `RiderCharacter` words as 0055f700; the one-page differences are the documented
ConVar residency, the Terracross hash difference is the documented mud record. None is
clock-related.

## Usage

```
# one deterministic boot capture at exactly 60 Hz
REFLEX_FIXED_CLOCK=1 native/game/tools/loading_accept.sh <cli> <ports> 1 1500 1

# the five-world matrix, pinned
REFLEX_FIXED_CLOCK=1 native/game/tools/verify.sh <cli> rx_race

# proofs
builds/clockpin/proof.sh <cli> 1200
builds/clockpin/boot_proof.sh <new cli> <pre-change cli>
```

Build: `native/game/tools/build_cli.sh /Users/payton/Documents/Development/REFLEX/builds/clockpin/cli`.
Backups of the pre-change runtime files: `builds/clockpin/backup/*.pre`.
