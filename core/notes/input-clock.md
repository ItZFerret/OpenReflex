# Input and frame clock in the product session (inputclock, 2026-10-19)

Two parts of the session API were stubs: `rx_session_set_input` stored the device state and `rx_session_advance_dt` recorded dt. Both now reach the
game through the game's own paths. Files: `game/loop_hostinput.[ch]` (new), `game/loop_platform.[ch]` (host mode for the oracle, `tick_ms`),
`game/build_globals_engine.c` (engine timing settings, counter stamps), `api/rx_session.c`, `include/rx_api.h`, `oracle/loop_hostinput.cpp`
(differential check), `tools/rx_input_clock_test.[c|sh]` (product test).

## Principle

The original talks to the operating system at fixed points of `Engine::Update` (0084ed20) and reads the devices there; the game code does the rest. The
product's host (Unity, through the API) takes the operating system's place: it keeps the device state and a clock and **serves those OS calls**.
Nothing writes a game record except the game's own poll code (all ported and verified before: `loop_input.c`). Dead zones, edges, held times,
KeyMaps and the vehicle's input block are the game's.

| OS call in the original | where | product: served by |
|---|---|---|
| `IDirectInputDevice8::GetDeviceState(256)` | `Keyboard_Poll` 00816ef0 (called by `XenonInputManager::vf43` 008150b0, once per frame) | `rx_loop_host.keyboard_state` |
| `XInputGetCapabilities` / `XInputGetState` | `Pad_Update` 008157a0 (vf43, 4 pads) | `xinput_get_capabilities` / `xinput_get_state` |
| `XInputSetState` | `Pad_UpdateVibration` 008169f0 | `xinput_set_state` (kept; `rx_session_rumble`) |
| `GetCursorPos` (`GetClientRect`, `ClientToScreen`) | `Mouse_UpdateCursor` 00817400 | `cursor` (client = screen origin) |
| `WM_INPUT` -> `GetRawInputData` | `MXWndProc` 00880dc0 -> `Mouse_AccumulateRawInput` 00817cb0 | `rx_host_mouse_post` -> `rx_wndproc` (ported) |
| `QueryPerformanceCounter` / `Frequency` | `Engine::UpdateTiming` 0084ea80 | `qpc` / `qpf` (virtual clock, 10 MHz) |
| `GetTickCount`, `timeGetTime` | package cache stamps, the random seed, `world_node_audio.c` | `tick_ms` / `rx_os_set_time_source` (the same clock in ms) |

`rx_session_create` installs the callbacks (`rx_host_input_install`) and the time source before the startup runs, so the startup reads the same clock.

## rx_pad / keys / mouse -> the game's records (exact)

All offsets are guest offsets. `im` = the input manager (`[[0x00a95844] + 0x34]`, built by `XenonCore::vf6` 00843a10).

**Keyboard** (`keys[256]`, indexed by DirectInput scan code, DIK_W = 0x11, DIK_LEFT = 0xcb; bit 0x80 = down, the other bits are ignored as DirectInput
defines the state). `Keyboard_Poll` reads the 256 bytes, maps each scan code through the game's table `[0x00a95848 + scan]` to a key id `0x23..0x87`
(table in `build_globals_engine.c`: W -> 0x39, A -> 0x23, S -> 0x35, D -> 0x26 (scan 0x20), UP 0xc8 -> 0x84 ...) and updates the key record
`kb + 0x18 * (id - 0x23)`, `kb = im + 0xb74` (keyboard 0; +0x990 device read ok, +0x994 any key down): `+0` the edge of this frame (2 pressed, 1 released, 0
none), `+4` the level (1 down), `+8` modifiers (shift / ctrl / alt bits), `+0xc` value (1.0 down), `+0x10` / `+0x14` times. Only keyboard 0 exists. The
KeyMaps (`rx_keymaps_update` 00813de0: held queries through `XenonInputManager::vf27` 00814a70) read these records; the player's controller key maps
(`Controller_BuildKeyMaps` 00547f30) turn them into the vehicle's input block (`simulation + 0x1c`: throttle, brake, steering, rider reflex ... see
`_re/notes/simulation-input.md`). The default bindings (ControllerRemapping 005b80b0): W throttle, S brake, A / D steer, arrows rider reflex, Space
trick, left Shift clutch, C camera.

**Pad `i`** (`rx_pad` = XINPUT_GAMEPAD: `buttons, left_trigger, right_trigger, lx, ly, rx, ry`; XInput's own values, y up positive; `pad_mask` bit i =
connected). Record `pad = im + 4 + 0x2dc * i`. The host answers `XInputGetCapabilities` for a connected pad with the same 20 caps bytes the oracle's emulator
reports (the game reads only the vibration words +0x10 / +0x12; real wButtons would be 0xf3ff, not read), `XInputGetState` with the XINPUT_STATE
{dwPacketNumber (counts state changes), the gamepad}; an unconnected pad answers ERROR_DEVICE_NOT_CONNECTED (0x48f).
- `+0x29c` connected, `+0x2a0` connected this frame, `+0x2a4` lost this frame, `+0x27c` the caps copy, `+0x290` the previous gamepad (12 bytes: buttons, LT, RT,
  LX, LY, RX, RY).
- buttons: `+0x244` held word, `+0x246` pressed this frame, `+0x260` released this frame, and the 14 button records (0x18 each: `+0` state 2 down / 1 up, `+4`
  pressed, `+0xc` value, `+0x10` time down, `+0x14` time up) at `pad + 0xc4 (0x1000 A), 0xdc (0x2000 B), 0xf4 (0x4000 X), 0x10c (0x8000 Y), 0x124 (0x100
  LB), 0x13c (0x200 RB), 0x184 (0x40 LThumb), 0x19c (0x80 RThumb), 0x1e4 (0x01 up), 0x1fc (0x08 right), 0x214 (0x02 down), 0x22c (0x04 left), 0x154 (0x10
  Start), 0x16c (0x20 Back)`.
- triggers (`rx_pad_triggers` 00816400): a byte above 30 is "held" (`+0x1b8` LT / `+0x1d0` RT = 1) with the value / 255 (`+0x1c0` / `+0x1d8`); edges
  (`+0x248` / `+0x24c` pressed, `+0x264` / `+0x268` released) compare the old and new bytes with 30 as the threshold (below counts as 0).
- sticks (`rx_pad_axis` 008156e0): dead zone |v| <= 0x1eb8 = 7864 -> 0; beyond it (v - 7864) / 24903 (positive) or (v + 7864 + 1) / 24903 (negative), so
  +-32767 / -32768 reach +-1. Values `+0x2a8` RX, `+0x2ac` RY, `+0x2b0` LX, `+0x2b4` LY (and the axis records' `+0xc`: RX `+0x34`, RY `+0x4c`, LX `+0x04`, LY
  `+0x1c`, with state 3 / 4 / 2 and times). Edge flags use the dead zone [-0x1eb7, 0x1eb7]: left the centre `+0x250` RX, `+0x254` RY, `+0x258` LX, `+0x25c` LY;
  back to the centre `+0x26c` RX, `+0x270` RY, `+0x274` LX, `+0x278` LY.
- vibration: the game decides (`+0x2b8` on, the UI setting Vibration, default 0 on PC) and calls `XInputSetState` with `pad + 0x2cc` / `+0x2d4` x 65535;
  `rx_session_rumble(pad, &left, &right, &calls)` returns the last pair (the old plugin's rumble).

**Mouse** (`rx_session_set_mouse(dx, dy, wheel, buttonFlags)`: Raw Input semantics, +y down, wheel 120 per notch, RAWMOUSE usButtonFlags bit 2k down / 2k + 1
up of left, right, middle, button 4, button 5). The host builds the RAWINPUT (RIM_TYPEMOUSE) and calls the game's window procedure with WM_INPUT
(`rx_wndproc`, port of the WM_INPUT branch of MXWndProc, other messages stay original). The mouse block `mouse = im + 0x3234` gets: held bits `+0xf8`,
pressed / released sums `+0x118` / `+0x11c`, motion sums `+0xfc` / `+0x100`, wheel sum (word) `+0x104` (`Mouse_AccumulateRawInput`, ported as
`rx_mouse_accumulate_raw_input`). `Mouse_Update` 00817340 (in vf43) consumes the sums once per frame (so the mouse is delivered at the first step after the
call and not repeated) and computes RiderReflexMouseX / Y at `+0x10` / `+0x28` = clamp(1/30 s / dt x (dx / 1280 or -dy / 720) x S, -1, 1) with the sensitivity
S [0x009cb7b0] (the UI setting MouseSensitivity). `rx_session_set_cursor(x, y)` sets what GetCursorPos returns (UI cursor only; the product's display
viewport block (context + 0x3c) is not set up, so the UI cursor coordinates are degenerate; gameplay does not read them).

Differences between the product's manager and the original's that the poll does not see: no DirectInput object (`im + 0x33a0` = 0, keyboard device
`+0x9ac` = 0): the host answers GetDeviceState whatever the device argument is.

## The frame clock

Original: `Engine::Update` -> `UpdateTiming` (0084ea80) reads QPC, `dt = (now - [engine + 0x68]) / freq` (x87, 53-bit), waits (spins on QPC) while `dt < cap`
(engine + 0x14), then smooths (`dt = 0.2 x` the sum of the last 5 frame times, ring `+0x24..+0x34` index `+0x20` sum `+0x38`; the constructor
leaves the ring / sum warm: the first frames' dt is higher than the real one, as in the original), clamps at `+0x48` and adds to `+0x3c`.

Product:
- The session owns a virtual QPC (10 MHz, `loop_hostinput.c`). `rx_session_advance_dt(dt)` moves it on by `ceil(dt x 1e7)` ticks at the frame start (dt <= 0 / NaN
  = nothing, limited to an hour); the first read of the frame returns that value, each further read 100 ticks (10 us) more. So a dt shorter than the cap makes
  the game's own busy wait run (it ends within 10 us of the cap), a longer one is measured, smoothed and clamped by the game. `rx_session_advance(s)`
  is `advance_dt(0)`: the frame cap's dt (1/60 s). `GetTickCount` / `timeGetTime` are the same clock in ms and never advance it
  (`rx_os_set_time_source`), so runs are reproducible.
- Engine settings the startup lacked (`build_globals_engine.c`, from Engine::Startup 0084e52c..0084e734): `Engine::vf0(60.0)` (0084e070, ported) = the cap
  `+0x14` = 1/60 (the static constructor leaves FLT_MAX), `DeltaSecondsClamp` from the Registry ("Game" / "DeltaSecondsClamp", 0.1 s) into `+0x48`, the FPS
  display's registry id `+0x84`, and the counter stamps `+0x60 / +0x68 / +0x70` (the first dt is measured from the startup's counter).
- Frame composition (`rx_session.c frame_call_run`): **BOUNDARY.** The real frame is `Engine::Update`; the product session still ran only `MXGame::Update`. It
  now runs the head of Engine::Update (`rx_engine_frame_head`: UpdateTiming, the input poll vf43, the key maps) and then `MXGame::Update`. `RX_FULL_FRAME=1` runs
  the whole `rx_engine_update`: with the current core it stops in `SceneManager_Tick` (`rx_scene_manager_tick` -> `rx_vector_assign` -> unported `004fb0e0`)
  right after `MXGame::Update` has run its first frame, i.e. the scene tickers, the job pool drain and the render-camera copies are what the product frame
  still lacks. Remove the boundary (call `rx_engine_update`) when those run.
- Pacing for a host at another rate: `rx_session_step_dt(s)` = the game's cap read from the engine (0.016666668 s); `rx_session_pace(s, real_dt, max_steps)`
  accumulates real time and returns the whole steps due (a backlog beyond `max_steps` is dropped, the fraction stays); run that many
  `rx_session_advance_dt(s, step_dt)`; `rx_session_alpha(s)` is the fraction of a step left in the accumulator. The game has no inter-frame
  alpha of its own: the vehicle's fixed-step scheduler (h = 0.0075 s, lead `sim + 0xc0`) already interpolates inside the frame, `rx_session_bike`'s
  `render_position` is that pose. A host interpolates between the last two frames' poses with alpha, or shows the last one. `rx_session_engine_dt` is
  the game's own dt (after smoothing / clamp); `rx_session_frame_dt` the dt the host passed. The C# bridge's `RealClock` path (spin to the cap, pass the
  measured dt) stays valid: the game now does the waiting itself when the host passes less.
- The input is latched: every `advance_dt` of a loop sees the last `set_input` state; the mouse (a message) is consumed by the first.

## Verification

1. **Oracle, differential** (`oracle/loop_hostinput.cpp`, group `rx_inputclock`; run `builds/inputclock/verify_inputclock.sh <reflex_boot> [quick]`, enabled by
   `RX_INPUTCLOCK=1`). Verify mode (native first, then the original, memory writes and registers compared) on the five race worlds, driven by REFLEX_DRIVE
   (random W / S / A / D / arrows), REFLEX_PADSIM (pad 0 buttons, triggers around 30, sticks across the dead zone) and a frame-start hook
   (`rx_FrameStartHook`: a synthetic mouse with motion, wheel notches and button edges posted through `MXWndProc`, and frame times jittered by up to 0.24 s
   so the clamp and smoothing run):
   - `XenonInputManager::vf43`: the native reads the emulator's keyboard and pad as the host would hand them to `rx_session_set_input` (`rx_host_check_poll`),
     serves the game's platform calls from the host layer (`rx_loop_host_mode`), runs the game's poll; the original runs the poll on the emulator's imports.
     Every record the poll writes (pads, keyboards, mouse, idle timer) matches.
   - `Engine::UpdateTiming`: the same with the host clock (the counter continues at the emulator's).
   - `MXWndProc` WM_INPUT: the ported window procedure + `Mouse_AccumulateRawInput` against the original, reading the same RAWINPUT.
   Result (full run, 5 worlds; Concord 1800 frames, the others 1500): MXWndProc, vf43 and UpdateTiming **1800 / 1500 calls, all verified, 0 mismatches** on every world
   (log `builds/inputclock/verify_full2.log`); On mode (the host-layer
   ports alone) against the original with only the hooks: state and whole-memory hashes **identical** on all five. Negative controls
   (`RX_INPUTCLOCK_BREAK=keys|pad|mouse|clock` perturb what the host layer hands over): each is reported by exactly the port it breaks (600 / 600 mismatches).
   `native/game/tools/verify.sh <boot> rx_loop quick` still 0 mismatches / On identical after the `loop_platform.c` change.
2. **Product test** `tools/rx_input_clock_test.sh <out dir>` + `<out>/rx_input_clock_test <install root>`: the public API (set_input / set_mouse / advance_dt /
   pace) and the game's records after one frame (pad edges, dead zone, trigger threshold, keyboard high-bit rule, mouse axes, `UpdateTiming` fields) against
   values computed from the mapping above; then several frames of the frame head with varying input and real time (held, released, a pad that goes away,
   the cap's busy wait, the clamp, the once-per-second FPS call).
3. Not verified: the keyboard / pad / mouse inside a product race frame (the product does not run past its first load frames yet; where it stops is in the
   agent report), the DirectInput-less keyboard record set-up against the original manager (`+0x9ac` = 0 is read by nobody in the poll), the real
   XInput packet-number semantics (not stored by the game), key repeat (the KeyMap repeat uses the game's dt: same code).

## Race input under load: the ride test (inputrace, 2026-10-20)

`builds/inputrace/` (`ride.c`, `cmp_ride.py`, README) drives the session through `rx_session_set_input` on the last runnable
product (the camsnap/presencefix object set) for 5400 frames with a known sequence (W, A/D from race frame 200, brake windows).
It answers point 3 above for the product race frame:

- **The host path is proved up to the green light**: the simulation input block (`S+0x1c`) is written from the host state at the
  first InProgress frames (the gate's brake hold, then the first `REFLEX_DRIVE` draw: W plus the drawn arrow key), and the same deterministic
  drive (`--drive N` = the oracle world test's `REFLEX_DRIVE=N`, same LCG/pool/holds) is fed to both sides for comparison.
- **The product cannot be ridden yet, and the input layer is not the cause**: on the second InProgress frame the local rider is
  thrown (vehicle `V+0x280` = 0x13, 19) - the chain-round-3 start crash, now executed instead of aborting at `00426d00`. The vehicle
  listener's control word `L+0x1f0` becomes 19, and `0045d510`'s local-player branch requires it to be 0 (no pose override is
  active, mode 1); its fallback needs a mode-2 override, so the game's own `veh_clear_inputs` zeroes `S+0x1c` every step. The bike
  coasts (speed <= 5.4, gear 0, 125 units of path, no lap); the camera follows it. The same with a pad (`--pad`) and with W held
  from frame 1, so it is neither the keyboard path nor the sequence.
- **Oracle, same event and input**: not wrecked, block live (throttle in 241 / 331 racing frames with drive 7; W held reaches
  76.8 ft/s and gears 0->1->2->3). When the oracle itself enters a mode-1 pose override (f=1281), its block also zeroes (18/19
  frames) and its speed decays - the branch the product is stuck in.
- **First difference** (`cmp_ride.py`, 202 frames): input block race+1 word 2 (front brake 0x3f800000 vs 0) and race+2 word 0
  (throttle 0 vs 0x3f800000); speed/velocity from race+0 (inherited from the known grid slot 7 vs 5: position word 0 615.678 vs
  605.903); race state / laps / riders identical, place 6 vs 5, race time differs by one dt ulp from race+2.
- **Pacing**: `rx_session_pace(1/30)` returns 2 every call (650 calls -> 1300 steps), alpha stays 0, the game's dt word is the cap
  `3c888897` on every racing frame, and the paced per-frame log is byte-identical to the 1/60 run (and across processes: state hash
  `4689c205e4ea6775`). The 1/30 accumulator drains exactly because 1/30f == 2 x 1/60f.
- Remaining: the start crash (ride blocker), the live tree (mid-edit; preserved objects used), and a re-run once loadfix settles.

## The ride after the wreck-checker fix (inputrace2, 2026-10-20)

`builds/inputrace2/` (README with the tables, logs, driver additions) reruns inputrace's harness on the build startdiag left:
the preserved camsnap/presencefix objects **plus the fixed `world_wreck_checker.c`** (`builds/startdiag/eobj_fixed`; its checker
object is byte-identical to compiling the current fixed source, md5 `f66eb339...`). The rider is no longer thrown and both grids
ride; every run ends at an unported function, not at the input layer. `RX_FULL_FRAME=1`, 1/60 s steps, 5400 requested:

| grid / input | steps | racing rows | max speed | gear | path | stop |
|---|---|---|---|---|---|---|
| 12, W held | 1452 | 354 | 78.33 | 3 | 281.5 | `00529190` (AI offtrack aim), race+353 |
| 4, W held | 1766 | 505 | 87.43 | 4 | 507.0 | `00550b70` (player reset / terrain mat 7), race+504 |
| 12, clean (W, A/D after +200, brake windows) | 1462 | 364 | 83.66 | 4 | 359.5 | `00529190`, race+363 |
| 4, clean | 1689 | 428 | 87.23 | 4 | 448.3 | `00848af0` on the next step, race+427 |

No laps in any run (`lap_time` = `race_time` ~6..8 s, `last_lap` 0, `best_lap` `7f7fffff` = unset), no wreck, state racing to
the end; the camera follows at 6.78..21.4 units (26.8 on the clean routes); the preserved build's particle managers are empty
(`live` 0 = no dust). **Oracle, same W-held input** (`builds/inputrace/logs/oracle_hw_input.txt`, 331 racing frames,
`cmp_ride.py`): the simulation input block is **identical in all 331 frames (2648/2648 words)**. The first differing word is
speed at race+0 (`3df9ecd9` 0.1220 vs `3ec4b012` 0.3842) and it is the **known grid slot 7 vs 5**, not input or physics: the
product starts at `615.359 114.322 -385.692`, the oracle at `605.903 114.406 -389.569` (10.22 units, one gate), and the speed
curves are one curve shifted by 10 race frames (product behind; RMS 0.19 / 0.38 / 0.51 ft/s over race 0..60 / 60..150 / 150..250,
mean +0.13 / +0.07 / +0.17). Gear shifts follow the same offset: product 143/168/204, oracle 135/159/194 (8..10 frames later).
At +150/+180/+200: product 47.27/58.50/65.10 (gear 1/2/2) vs oracle 50.75/61.85/67.86 (1/2/3) - startdiag's 50.8/1, 61.8/2,
67.9/3. From race+250 the lines part (RMS 5.8: product drops to gear 2/1 and 65.7 at +300, the oracle holds ~74) once the
10-unit offset and the still-unported AIData accumulate; the first differing field is still the inherited race+0 speed.

**After ridepath landed** (`00529190` at 18:49, `00550b70` at 18:50) the current tree was rebuilt (`build_current.sh`; it first
needed a patched copy of the mid-edit `loop_cam_firstperson.c`, then the live file compiled again at 19:07 and the rebuild from it
gave a byte-identical 12-rider W log). The 12-rider W ride now runs **456 racing rows** (max 87.71, gear 4, 433.4 units) and stops at the next AI behaviour
`005278e0` (race+455); 4-rider W: 471 rows, stop `005278e0`; 12-rider clean: 477 rows, stop `00592b60` (stunt audio handler);
4-rider clean is unchanged (same 428 rows, stop `00848af0`, same hash `f14d8b00b15ae994`). The current build matches the fixed
build's input block for all 1452 rows, and its speed / position / velocity through race+234 (an AI interaction then changes
the local velocity; the first difference of any field is the AI-driven `place` at race+19). Dust now
appears (up to 2725 live particles, 606 normal, 0 additive).

**Two input-path findings.** (1) `A` / `D` reach the simulation's **rider reflex** word (`+0x34`), not `steering_x` (`+0x2c`):
`[0xd92efc]` (the UI settings block) is **0**, so `Controller_BuildKeyMaps` gets preset/handedness/motion 0/0/0, i.e. handedness
*left* instead of the original default *right* (`UISettings_Defaults` 005b8030); the left/off routing in
`_re/notes/simulation-input.md` sends the keyboard steer keys to reflex and ignores the arrow reflex keys. Measured with a
single-key hold: A -> word 6 -1, D -> +1, arrows -> nothing; the key-map slots themselves are correct (steer 0x26 / esi 0x23,
reflex 0x87 / 0x86, up/down 0x84 / 0x85). `W` / `S` are unaffected, which is why the W-held comparison is exact. The missing
piece is the profile bootstrap's settings block (`_re/notes/profile-bootstrap.md`: `00743030` then `005b80b0` on
`*(u32*)0xd92efc`) - construction, not gameplay. (2) inputrace's `--drive N` applies its first draw one racing row earlier
than the oracle's `REFLEX_DRIVE` (`race >= 2`); after that shift the remaining difference is the A/D -> reflex routing above.
The W-held run is the valid same-input test.

**Pacing/determinism** on the fixed build: `rx_session_pace(1/30)` 726 calls, all `due2`, alpha 0, 1452 steps; the paced log is
**byte-identical** to the 1/60 log and two processes give state hash `73cfdd96c3eb51a0`. The current build repeats it: 777
calls, 1554 steps, hash `987d9fb87dcf2e4a`. Engine dt is the cap `3c888897` on every racing frame.

**Next stops**: `005278e0` (AI navigate mode 1), `00592b60` (stunt audio handler), `00848af0` (a registered
`CollisionShapeComposite::vf4` dispatched through `world_call_ctx`'s integer-only path from `Grid_ReportOverlaps`, so it aborts
as "unported"), `0076ee60` (static contact) and on the fixed build the two ridepath ports. A 5400-frame ride needs those.

## The profile settings block: handedness, the steer keys and the remap table (steerfix, 2026-10-20)

**Root cause (inputrace2's finding, fixed).** The product had no UI settings object: `[0x00d92efc]` (`g_UISettings`) was 0, so
`PlayerSlot_FromPlayerList` 0054a7b0 read preset / handedness / motion from address 0 and handed `handedness = 0` (*left*) to
`Controller_BuildKeyMaps` 00547f30. With the left routing the keyboard steer keys (`ControllerRemapping` SteerLeft / SteerRight)
feed the *rider reflex* and the arrow reflex keys are ignored (`_re/notes/simulation-input.md`); `W` / `S` are unaffected. The
original's default is handedness 1 (*right*), motion 0: A / D steer, the arrows are the rider reflex.

**How the original derives it (ported).** The settings object is created by `MXUIManager::Construct` 00598640 while
`MXGame::Startup` 00756e10 runs - `if (g_UISettings == 0) g_UISettings = UISettings_Construct(alloc(0x170))` - and the shipping
guest profile route runs later, after `MXRegistry_Load`: `ProfileReminderPopup.lua` restores defaults and calls
`UI_SetPrimaryPlayerDeviceIndex` 00743030 with the session's primary player device, then opens MainMenu (`_re/notes/profile-bootstrap.md`).
That call is the chain the block comes from:

| original | what it does |
|---|---|
| 005b7cd0 `FUN_005b7cd0` | vtable `0x0094db70`; rows 0..3 at +4 + 0x48*i via Fn_005b7d50 (+0x40 = -1, +0x44 = 0); +0x164 = -1, +0x168 = 0, +0x16c = -1; each row +0x40 = its device index, then `UISettings_Defaults`; the +0x124 "no device" row gets `UISettings_Defaults` too |
| 005b8030 `UISettings_Defaults` | the 16 defaults: DrivingCamera 0, Brightness 50, Gamma 1, OnlinePlayerName 0, MeasurementSystem 0, ControllerPreset `[0xa93f5c]`, Effects/Music/Engine/Crowd 100, ControllerMotion 0, **ControllerHandedness 1**, CareerDifficulty 80, HUDVisibility 0, MouseSensitivity 50, Vibration 0 |
| 00743030 `UI_SetPrimaryPlayerDeviceIndex` | ui+0x2a0 = controller; `[0xd92f10] + 0x3c + 4*controller = 1` (controller used); `UISettings_Reset(g_UISettings, controller)` |
| 005b80b0 `UISettings_Reset` | device -1: the four rows' loaded flags and +0x16c = -1; else: `+0x16c = device` when it was -1, `FUN_005b7d70(row)` then `UISettings_Apply`, then the ControllerRemapping seed |
| 005b7d70 | `UISettings_Defaults(row)`; field `Setting` (index < 0x10) of the player database `device`'s `PlayerSettings` rows overrides `Value` (empty in the shipped MXTables_Default); +0x44 = 1 |
| 005b9af0 `UISettings_Apply` | the active device's row into the renderer gamma / brightness, the audio volumes, every player's measurement flag, the device player's driving camera, the pad's vibration enable (008151b0), MouseSensitivity 0x009cb7b0 = value x 0.05 clamped [0.1, 5] |
| 005b80b0 (tail) | with the `ControllerRemapping` table empty (the shipped MXTables_Default has 0 rows): clear, add the two built-in rows - keyboard 0x39 W, 0x35 S, 0x23 A, 0x26 D, 0x65 Space, 0x62 LShift, arrows 0x86/0x87/0x84/0x85, 0x25 C, 0x28 F, 0x34 R, 0x38 V, 0x3a X, J/L/I/K 0x2c/0x2e/0x2b/0x2d; pad 0x14/0x13, stick 1/1, 0xd/0xe, 3/3, 4/4, 0x10, 0xc, 0xb, 0xa, 9, and the keyboard's J/L/I/K - and register the table in player database 0 (+0x38 list / +0x1e8 count / the +0x18 name map) |

The five worlds' reference state matches exactly: +0x16c = 0, row 0 = preset 0 / handedness 1 / motion 0 / device 0 / flag 1,
rows 1..3 flag 0, the +0x124 row defaults, and the table's 2 x 19 cells (compared field by field, see below).

**Port.** New `game/build_uiprofile.[ch]`: `rx_uisettings_defaults` (005b8030), `rx_uisettings_row_construct` (005b7d50),
`rx_uisettings_construct` (005b7cd0), `rx_uisettings_row_defaults` (005b7d70), `rx_uisettings_apply` (005b9af0),
`rx_uisettings_reset` (005b80b0, with the seed rows through the core's table helpers 004f6d80 / 004175e0 / 004ec950 / 004ecdb0 /
004eb830 / 004f0540), `rx_ui_set_primary_player_device_index` (00743030), `rx_ui_settings_create` (00598640's settings part) and
`rx_ui_profile_bootstrap`. `loop_input.c` adds 008151b0 (vf53, the pad's vibration-enable store). Hooks: the settings object is
created at the UI-manager step of `mxgame_startup` (its gameplay reader is 00547f30 / 0054a7b0); the profile step
`UI_SetPrimaryPlayerDeviceIndex(0)` runs at the end of `rx_globals_run` after `rx_globals_game` (the tables are loaded, no local
player exists yet - the original's front-end timing; the product has no front end). Controller 0 is the single-player session's
primary device (the event spec's local player; the reference boot's +0x16c = 0). The settings block is now allocated at startup,
so every later product allocation shifts: the W-held ride log and state hash are unchanged (below).

`UISettings_Apply`'s side effects outside the core are the documented render / audio / camera boundaries: `Renderer_BuildGammaRamp`
007aff40 and the audio mixer callback 008073c0 run the original in the oracle build and do nothing in the product (the renderer
brightness word and every audio volume word are still written); `Player_ApplyDrivingCamera` 0054aef0 is the camera unit's (as in
`build_activity_player.c`). The original reads the per-player flag from the row's **+0x0c** word (the OnlinePlayerName field, not
MeasurementSystem at +0x10 - both 0 in the shipped data): transliterated as the original does it.

**Verification.**
1. **Construction, against the original's world image** (`builds/steerfix/cmp_uiprofile.py`; product probe `builds/steerfix/probe
   <install> --dump`): the settings vtable / +0x164 / +0x168 / +0x16c, the five rows' preset / handedness / motion / device /
   loaded flag, the remap table's 19 columns / 2 rows x 19 cells and the player-database-0 registration are **IDENTICAL** on all
   five reference worlds (`boot/worlds/*`) and on the framecmp pc1 world; the table counts (92 default / 25 player database 0)
   also match.
2. **The core's own input path** (`builds/steerfix/ride`, `RX_FULL_FRAME=1`, one key held from frame 1; the simulation input block
   S+0x1c after the gate, `--debug`): `remap = 10cdbf30 rec = 10cdbf30 preset 0 hand 1 motion 0`; A (DIK 0x1e) -> word 4
   `steering_x = bf800000` (-1), D (0x20) -> `3f800000` (+1); left arrow (0xcb) -> word 6 `rider_reflex_x = -1`, right arrow
   (0xcd) -> +1, up arrow (0xc8) -> word 7 `rider_reflex_y = +1`; the key-map slots are the defaults (throttle 0x39, steer
   0x26 / secondary 0x23, reflex 0x87 / 0x86, reflex Y 0x84 / 0x85). W / S words unchanged.
3. **Same A/D sequence as the oracle** (`--replay builds/inputrace/logs/oracle_drive7_input.txt`: the oracle's sampled 8-word
   blocks fed as keyboard keys through `rx_session_set_input`): in the 100 race frames in which both sides have a live block
   (the product's block-dead windows are the known input-suspension / pose-override branch: `S+0x2de0 = 1` from race 69, the
   game's own `veh_clear_inputs` zeroes `S+0x1c`; product dead 69..281, oracle 76..146 and 313..330),
   **800 / 800 input words identical**, including all 99 frames with `steering_x != 0` (the oracle's A = -1) and the reflex X
   word. First differing input word: none in the live window; the product's early block death (race 69 vs 76) is the vehicle's
   input-suspension branch, not the input layer.
4. **The bike turns** (12 riders, 1/60 s, same driver): clean sequence (W; A/D from race 200) vs the same sequence with A/D
   suppressed (`--no-steer`): identical until race 210 (A/D's first sine crossing), then the position diverges - 0.05 units at
   race 220, 2.4 at 240, 9.6 at 260, 29.2 at 300; path 350.8 vs 409.2 units, max speed 78.4 vs 86.4, gear 3 vs 4, place 6 vs 1
   (both 402 racing rows). The run with steering has `steer |max| = 3f800000` and 96 steering frames; the W-only A/B has
   `steer 0`.
5. **No regression**: the W-held 12-rider ride (1600 requested, stops at `005278e0`, step 1554) is **byte-identical** to
   inputrace2's current-build log (`builds/inputrace2/logs/cur12_holdw5400.tsv`, 1555 lines) and the state hash is the same
   `987d9fb87dcf2e4a`; `rx_loop` quick (12 ports) / `rx_world` quick (132 ports) 0 mismatches, On identical; API test
   `builds/steerfix/api/rx_api_test` runs.

**Remaining.** The settings object's *heap address* differs from the original's (the core does not build the UI manager, so its
allocation order around the object differs); the compared content is identical. The UI paths that change the settings at run time
(App.SetUISetting 005b9fa0 / UISettings_Save 005b7e70 / RemapTable_InitDefaults 005b9340, the Lua bindings) are not called by the
product and not ported; the defaults + the guest route are what a race reads. `UISettings_Apply`'s renderer / audio / camera
callees are boundaries as above. The ride still stops on the same unported functions (005278e0, 00592b60, 00848af0, ...).
