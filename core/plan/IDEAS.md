# Ideas for later

Ideas that have been looked into but are parked. Each has what we found, what it would take, and where any started work lives.

## LEGEND AI difficulty: better riding (community vote, 2026-11-01)

A fifth AI DIFFICULTY step above ALL TIME, in the race setup toggle like the original's four. The community voted for **better
riding**: same bikes, physics and power as the player, no boosts.

### What we measured (11 AI, ALL TIME, 3 laps; NAT_Concord, NAT_Renne, SX_Kingston)

- **The original's knobs are used up at ALL TIME.** Every rider is already on the top tier (bucket 0) with the Pro riders, and the
  catch-up boost runs all race. Each track's lap pacing targets are far faster than the AI can ride (Concord's Pro target is 37 s
  against real laps of about 64 s), so the AI is always "behind pace". Raising the pace target past Pro, section speed limits up to
  +20 % and forcing the best personality level (+0x8f8 = 2) each moved race times by less than the run-to-run noise.
- **Crashes are where the time goes.**
  - Each AI crashes about 3 times a race, and each crash costs about 4 s (2 s down, 2 s back to speed).
  - About 70 % of crashes come within 0.5 s of a jump landing. The most common is wreck code 23 (0x17), the impact/landing type.
    Rider-to-rider contact is rare.
  - On Concord every AI had the same tier, rider type and bike. Their best laps ran from 63.8 to 76.3 s, ordered almost exactly by
    crash count. The AI's clean pace is about 64 s, and crashes cost the field about 8 % of race time.
  - On Kingston one AI was stuck for 208 s after a crash.

### What LEGEND riding would be

The AI lands its jumps and rarely crashes, so the whole field stays near clean-lap pace in a tight pack. That's roughly 8 % faster
than ALL TIME's median on Concord. LEGEND only; every other difficulty stays the original.

### What needs changing

1. **Landing control (most of the gain, the least certain part).**
   - Today the AI only leans in the air at jumps the designers marked: node threshold +0x2e8 starts a lean recovery (0050b590).
   - Give LEGEND riders their own control in the air: predict the landing point and its slope, then lean the bike to match it.
   - First check whether the landing crashes come from casing or overshooting jumps; if so, the fix is takeoff speed instead.
   - About 2 to 4 days.
2. **The other ~30 % of crashes.** Decode wreck codes 10, 15, 16 and 19 (tipping over in turns? hitting objects?) and fix the common
   ones. About 1 to 2 days.
3. **Stuck recovery.** An AI that makes no progress for several seconds is put back on the track. About half a day.
4. **Tuning and validation.** National, Supercross, ATV and UTV with the crash tool, then a playtest. About 1 to 2 days.

Total: about a week of focused work.

### Started work: branch `w-legend` (worktree REFLEX-w-legend), not merged

- `core/game/ai_legend.h/.c`: the LEGEND switch (AIDifficulty >= 130; the front end would send 135), read in MXUIManager::SetupEvent.
  It never eases off toward a slower human (the positive lap gap is dropped in rx_behavior_lap_pacing_gap), has no "too fast" state,
  and has a pace target one step past Pro (floored at 94 % of Pro).
- `core/tools/ai_pace_test.c`: AI race and lap times at a difficulty, headless, with the player idle at the gate.
- `core/tools/ai_crash_test.c` with the TEMP `rx_debug_ai_probe` in `core/api/rx_session.c`: per-AI crashes (code, landing,
  contact, node), time down and recovering, and crash hot spots. Both TEMP pieces must not ship.
- Not done: the LEGEND menu entry (ReflexWorldFiles.AiSkills ("LEGEND", 135), OpenContent.AiName, ReflexEventSelect,
  FrontEndScripts) and all of the riding work above.

## Smaller ghosts (RXG2) (2026-11-02, parked by the user)

RXG1 ghosts (core/plan/LEADERBOARDS.md 3.2) measure about 825 kB a minute: a 1:46 lap is 1.49 MB, and laps over about 2:40 thin to 15 Hz.
The $5 Workers plan isn't at risk: ghosts live in R2, which has about 10 GB a month free, then about $0.015 per GB a month, and no
egress charge. The Worker only reads a ghost's header. Smaller ghosts would still download faster and store less. The user wants this
later, as a new format **RXG2, with RXG1 still supported** for reading, uploading and serving (the Worker accepts and serves both; the
game decodes both).

| Change | Rough saving | Notes |
|---|---|---|
| Rider bones store their local rotation only (no position) | ~40 % | Bone lengths are constant; positions are rebuilt from the skeleton's bind offsets. Check first that the original's animation never stretches a bone. |
| Skip bones that don't show on a translucent ghost (eyes, blink, jersey flaps, collars) | ~15-20 % | Rebuilt from their parent bone. |
| Lower rotation precision (smallest-three, about 10 bits) | ~15-20 % | |
| Record at 20 Hz instead of 30 | ~33 % | Hermite interpolation for positions, slerp for rotations. Unleashed records at 15 Hz. |
| Store the raw gzip bytes in R2, not the base64 wire text | 25 % | Worker side only. Serve it base64 again, or binary under a new protocol. |

Together: roughly 150-200 kB a minute, 4-5x smaller (a 1:46 lap about 300 kB), and no thinning on long laps.

What it takes:
- an RXG2 codec beside RXG1 in RxGhost, picked by the magic;
- the Worker's `ghostHead` reading both;
- a protocol bump (api 2) only if the wire format changes;
- the cross-language fixture for both formats.

About a day plus a small Worker update.

## NVIDIA Reflex low latency + boost (2026-11-02, parked by the user)

A Windows-only option for NVIDIA cards: **NVIDIA REFLEX: OFF / ON / ON + BOOST** in OPTIONS > VIDEO, shown only when the driver
reports support. On NVIDIA it replaces LOW LATENCY (one queued frame, ReflexLatencyGuard): Reflex paces when each frame starts
instead of shrinking the queue, so it doesn't halve the frame rate when a frame misses the refresh. LOW LATENCY and the guard stay
for AMD, Intel and Mac.

What we found:
- Unity 6000.6.1f1 has no Reflex API. Its NVIDIA module only exposes DLSS (NVIDIA.DLSS*, NVIDIA.GraphicsDevice).
- It would be our own native plugin, a small x64 DLL cross-compiled on the Mac with clang like rxcore.dll. It takes the D3D11
  device from IUnityGraphicsD3D11 and loads the driver's nvapi64.dll at runtime (nothing extra ships; it does nothing elsewhere).
- Three NVAPI calls: NvAPI_D3D_SetSleepMode (low latency, boost), NvAPI_D3D_Sleep once a frame just before input is read, and
  NvAPI_D3D_SetLatencyMarker (simulation, render submit, present). Render-thread markers go through Unity's plugin events.
- The input and simulation markers must sit where the core really reads input (the worker-thread simulation and the newer input
  path), or Reflex gains little. With Reflex on, the queue stays at Unity's two frames.
- Verification is by Windows NVIDIA testers: a log line (available / active, the driver's latency report) and the NVIDIA App
  overlay's "PC Latency", which shows once the markers work.

What it takes: the NVAPI headers (github.com/NVIDIA/nvapi, needs the user's OK to download; check its license and NVIDIA's terms
for using the "NVIDIA Reflex" name in a menu), about a day of work, then a couple of tester rounds.

## Starter routine at the gate (2026-11-03, parked by the user)

Make the start feel like a real moto. Ideas from the 20+ rider start gate brainstorm; the user wants them for later.
- **The starter's board:** a 30-second board, then the board turns sideways and the gate drops at a random moment within
  a few seconds (about 3-10 s), instead of a fixed countdown. Online, the host picks the moment and sends it.
- **Jump starts:** a rider who leaves before the drop gets a penalty (time added, or put back behind the gate).
- **Holeshot award:** shown on the HUD and in the results. Alive races still have no holeshot cue (ALIVE_RACING.md).
- **Gate-drop camera:** a short shot along the gate as it drops, skippable, off for online if it costs input.

Related ideas from the same brainstorm, not picked yet:
- wider gates, built by repeating the gate model's sections sideways;
- class waves (two drops a few seconds apart);
- gate pick by qualifying time;
- heats, LCQ and a main event;
- see-through ghost riders past the rider cap;
- raising the 16-rider cap;
- a gate tool for track makers.
The second row was picked and is being built (w-secondrow).
