# MX paired-stick whip/takeoff investigation

User reports weak initial whip snap/flick on steep jump faces with a controller
using both sticks; a30FPS render cap did not resolve it. No handling multiplier
or arbitrary torque change has been made.

Source chain: controller sampling004308a0 (`bike_sim.c`) reads four analog actions
into raw S+2c/30/34/38. Controls0044c170 (`bike_controls.c`) produce
C+68/6c/70/74. Ground stunt latches00440da0 (`bike_air.c`) measure outward stick
rates and preserve strongest flick/side. Slot1200443610 (`bike_slot12.c`)
chooses grounded correction or airborne0043f2e0. Contact slot1300444b90 sets
S+8d98 airborne and S+8d9c first-air-step from actual wheel contacts.

The typed native action vf4 functions (`world_node_input.c`) return float,
matching `bike_sim_action_value`; this is not the ATV rx_f80-struct ABI bug.
Air/ground-control helper calls are direct typed native calls. There is no
unported branch identified in this audited chain. This is a source audit, not
new original-comparison acceptance.

Steep takeoff has an explicit original fallback: normalized velocity's vertical
component is splatted, squared, reduced (three equal lanes), square-rooted and
compared to009cd9ac. Below it S+8bf0 is normalized up-cross-velocity; otherwise
it is the takeoff quaternion's rotated X axis. Original004438e2..0044392f and
004439a4 onward match the native expression, including its unusual repeated
vertical-component reduction. Do not replace it with an intuitive slope angle.

Transient sensitivity: air control computes outward rates `(now-before)/dt`,
curve-shapes them, mixes steer/reflex with takeoff fades, and adds angular
increments per physics step. Final angular increments are not multiplied by dt;
first-air-step scaling uses S+56e0 times S+5680. Fades/filters and rates do use dt.
Thus render FPS alone cannot establish matching simulation/input cadence. Timing
ownership is investigating scheduler and real controller sampling separately.

`core/oracle/bike_whip_probe.cpp`, hook`BikeWhipProbeHook`,
`RX_BIKE_WHIP_PROBE=1`, frame900, supplies bounded synthetic branch stimuli on
an actual MX simulation. It restores S(8fc0bytes), actual controls(88bytes),
three external ground filters, all registers/XMM/flags/FPU. Cases:24 paired-stick
air combinations (level/steep velocity, left/right, same/opposite sticks,
first-air-step on/off), two grounded takeoff-axis alternatives, eight flick-latch
cases at.0075/.015. It retains actual tuning/curves/body hierarchy; it does not
simulate a naturally ridden jump or assert user-perceived handling parity.

Central comparison should independently select existing rx_bike
`Simulation_AirControl`, `RavenSimulationMotocross_vf12`, and
`Simulation_StuntLatches` adapters so nested native calls do not hide coverage.
Verify each; then run the same deterministic stimulus in On/reference and compare
whole state/memory. No builds or runs performed by this investigation yet.

Further native-init audit: parameter offsets are consistent with the original
record layout. CourseCorrectionForce is P+26d0 scalar+44 => S+5624;
AngularRateDamping P+2720 vector+50 => S+5680; OnTakeoffDamping
P+2780 vector+50 => S+56e0. Negative construction defaults are sentinel values,
so loaded BXML data must be compared rather than treating defaults as tuning.
Class1..4 selects mx125/mx250/mx250f/mx450f through params_load_file0043a3f0.
Setup0043b750 initializes roll/pitch/yaw filter times from S+5950/54/58 and
steering time from S+3634. No incorrect offset found in this source read.

An additional steep-face branch in controls0044c170 requires world velocity y>15
and a real terrain-ahead query miss. It enables S+8c24, adds reflexX*1.5 to C+40,
and sets S+8c20 when both steering/reflex X magnitudes exceed.6. The probe now
includes four paired-stick control calls with level/steep velocity and both
sides, and logs actual assist flags instead of claiming the terrain branch hit.
The existing slot9 adapter is RavenSimulationMotocross_vf9.

Before synthetic stimuli, the probe emits pointer-free [mx-param] records from
actual registered/loaded tuning and [mx-param-meta] fixed dt/max steps, initial
air-filter time constants, steering time and the slope/vertical/nose-down global
constants. Curves include only their active samples plus slopes, avoiding dead
uninitialized array tails. Timing ownership adds the same read-only dump to its
isolated native diagnostic copy. Comparing those outputs will test separate
native construction, unlike the original-arena per-function comparisons. Probe
source is frozen pending central build; no handling conclusion yet.


First central comparison: air108, controls2226 and grounded821 calls all zero
mismatches. Latch1008 calls exposed eight EAX-only discrepancies in synthetic
flick cases (`builds/sol_whip/logs/whip-latch-verify.log`); no simulation-memory
difference reported. Original remap helper00424370 executes UCOMISD(lo,hi);LAHF
at00424378/7c, leaving AH03 for every fixed lo<hi flick remap. The latch port
tracked its own prior LAHF but omitted the remap helper's later AH overwrite.
`bike_air.c` now preserves that exact EAX side effect after each of its three
remap branches. Adapter output mask and all stimuli remain unchanged; no
handling arithmetic was altered. Central rerun pending.

Next investigation after matching tuning/cadence: S+8c28/2c is an original debug
pose-freeze/rearm path (frame-update input codes23/33); when enabled, slot7
restores saved position/velocity and air control holds assist fades until an
active stick clears rearm. No source-only reason found for a native-only mode.
S+8d9c is set only on the wheel-contact transition (both8890/89e0 zero and
previous8d98 zero), then consumed by slot12 on the following physics step;
S14/18 are scheduler contact summaries, not those wheel flags. Derived body
mass/inverse-mass/inertia/inverse-inertia are now emitted as four [mx-body]
records for independent initialization comparison. Separate observer stage
records track angular velocity across control, air, contact response, poses and
new contacts. Product remains unchanged.

`WhipPadHook` in the same probe source now supplies a genuine emulated XInput
provider when RX_BIKE_WHIP_PAD=1, selected explicitly instead of PadSimHook.
Pad0 connected, RT255 during actual racing, no buttons/LT; both raw stick pairs
follow the native fixture's deterministic500ms phases with ±26214/±19660.
First actual racing clock anchors the phase. Original controller/action mapping
runs normally; this does not directly write simulation input or invent contacts.
The next central comparison selects rx_RavenSimulation_IntegrateStep00431d60
with this provider to compare the entire physical step at identical actual
entry states. This goes beyond isolated air math and includes constraint solve,
velocity integration, contact response and wheel-contact classification.


### Completed original capture and cornering investigation (October 21 continuation)

The user finished riding stock James Stewart MX on Sugar Ridge (first Nationals) and the loading track, and reported recording was "Mostly smooth, some emulation issues but playable." They clarified the custom client loses control more during turning/cornering. Capture200735 is stopped, configs restored, no original game remains running. user-session.json updated. Use metadata-audit.md and solver-summary.json beside the capture; analysis separates outer context, simulation and thread, excluding AI. Local integration step is ~0.0075s throughout: loading20,665 steps and event11,573. Event both-horizontal-stick >0.8 coverage1,310 steps. Loading ~60fps; 12-rider event median outer dt34.464ms (~29fps). Exclude50.449s focus-loss gap. Recorder lacks wheel contacts/angular velocity/render pose: cannot quantify visual whip snap or traction from these records alone.

Completed full IntegrateStep oracle under deterministic XInput two-stick provider:2,222 comparisons, zero mismatches. On/reference state hash285668c8a3665f9f and memory31df9a12c9070c6b (25,074pages) match. Logs builds/sol_whip/logs/whip-step-{verify,on,ref}.log. Independent native30/60Hz fixtures both pass with ~133.3physicsHz, fixed7.5ms step, zero max-step capacity hits.108 parameter records, metadata and4body mass/inertia records match original initialization exactly. These are controlled fixtures, not replay of the user's Sugar Ridge ride. No handling multiplier/traction or stepping change justified.

Three Sol audits: Sugar Ridge=NAT_Concord noninterleaved material zone path is implemented and constructor comparisons already matched. Missing interleaved007f8d10 registration is a real gap but not this track. Native steering/reflex axis mapping/raw deadzone path found no substantial mismatch. Queued-input overwrite initially looked suspicious, but DEFAULT OriginalRealClock explicitly runs1frame/job; latest-state coalescing is normal sampling, not established lost-input bug. UI captured0merged frames. Fixed-mode merge remains separate. Native test was model11 MarsF_Lite; original James Stewart MX class has NOT been independently identified, so do not claim a class mismatch.

UI FPS candidate was tested windowed on Sugar Ridge with12riders and existing extracted assets: menu switched30→60→30, save reflected choices, race sustained29.9fps with target30/vsync0. Screenshots client-options-30fps.png, client-options-60fps.png, client-sugar-ridge-30fps.png in screenshots/sol-2026-10-21/whip. Preserved logs/ui-fps-session.log. In-race paused Options interactions did not change the value under CUA before watchdog ended; live60fps/menu persistence across restart not yet accepted. Stable Mac and Windows ZIP not promoted. Test watchdog stopped original PID36283; CUA later reopened candidate without launch environment, then closed via Cmd-Q.

Optional cornering diagnostics now extend REFLEX_PACING_LOG CSV (first15 columns preserved) with submission/frame start/main-thread collection/presentation observation clocks, submitted raw pad0 values/mask, and initial actual local model/eight tuning values. Metadata read worker-only and only with logging enabled. Timing is scheduling observation, not photon latency; inputs belong to Newest while A/B/alpha identify any interpolated display. No handling changes. Root review corrected RxPad trigger field names and rejected appearance slot -1, using enumerated local slot. Build/runtime verification follows below.


Diagnostic acceptance: candidate Mac build succeeded with0 Editor errors. Two bounded native-only Sugar Ridge runs completed at29.8fps/59.7fps, one rider,7.5ms core stepping unchanged. CSV parsed28columns,867/1753rows, no clock-order errors, initialModel11 and tuning5;15;30;20;20;30;50;50. Both rawsticks/triggers had live nonzero coverage. Median submission→core start0.106ms/0.178ms;0merged. This does NOT show a large default queue delay. Whole-CSV timing includes loading; summary files cornering-timing-summary.json and pacing-cornering-{30,60}.json. Automated W was held by smoke/probe, so these runs validate logging/pacing ONLY, not a controlled human traction comparison.

Correction to earlier cleanup note: Cmd-Q did not terminate the automatically reopened PID36483. It remains active with controller input and fullscreen view; likely user is riding. Do not kill it without checking. Later owned smoke PIDs36716/36732 exited0 normally. CUA bound to36483 rather than the bounded smoke window, so its12rider/lap2 screenshot must NOT be attributed to the one-rider diagnostic runs. No screenshot from those runs was saved. Earlier verified client-sugar-ridge-30fps.png remains valid. Stable bundle and Windows ZIP remain unchanged; candidate is builds/sol_whip/OpenReflex.app, rebuilt with diagnostics/FPSchanges.

Prepared builds/sol_whip/run_cornering_capture.py for the NEXT manual session: real controls, regular profile, player-controlled FPS Options, windowed1280x720,2workers, existing assets, no smoke/probe/automatic throttle,10minute watchdog scoped to owned process. Not launched while current client is in use. Recording requires exiting/restarting into this candidate; do not claim current visible old process has the new recorder. The cause of harder cornering and weak whip feel remains unresolved. Next: matched bike/tuning, manual corner/countersteer recordings and contact/material comparison rather than arbitrary grip changes.

### 2026-10-22 follow-up: fresh current native checks, original comparison pending

**The October 21 equivalence results above are historical, not proof that today's installed client matches the original.** Fresh tests below use current main native sources/objects and two explicitly matched own profiles. They establish native observations only; they do not replay the user's ride or establish original equivalence. The fresh original build/comparison is tracked separately at `builds/sol_bike_physics_verification/original-comparison`; its actual result remains pending at this follow-up.

The matched profiles in `builds/sol_bike_physics_verification/profiles/manifest.json` are model10 (authored James Stewart MX mapping, effective tuning55,35,15,35,25,25,50,50) and current model14 NeptuneF450F (40,50,60,40,40,50,50,50). Both use neutral own rider0/playerVehicle0 and actual authored parts. The Stewart identification is authored/report-derived, not independently decoded from old capture200735. Each binary has58 typed cells; two camera-float columns unsupported by the existing five-table importer are omitted, retaining native camera defaults. Initial incorrectly typed60-cell binaries were preserved and not used in successful runs. Correct profile SHA256s: model10 `af536d42aa5bbbacd5237e856dfb695ecf4cdc007c98950a2defe34a8a87d30f`; model14 `374b75a33bc8d64ed6369a2075713de404e6ba74fbd58d062476ce3434b4dc43`. Actual appearance, factory0, four bodies and all eight effective tuning values are checked after the local vehicle exists.

The fresh isolated diagnostic is `builds/sol_bike_physics_verification/native-comparison`. It retains526 current normal objects and replaces only four semantic owners with read-only instrumentation: bike_sim plus three tyre/surface/material observers. No handling, geometry, pose or contact writes were added. Pulse input is generated at the actual XInput callback using original timing+3c, first actual racing callback anchor, raw-clock packet word, both sticks and RT255. The original-style500ms paired-stick pulse is distinct from the new steady-corner input. Initial pulse60 binary SHA256 was `c9ef794626563c55a6a20a4cc2ab348e702e6402a865d03a03f5dc8abb2e3b4b`; its complete source/binary/build receipt is preserved in `history/pulse60-before-corner-mode`. The successor used for corner60 and pulse30 is `933d8e15f515b611059d2956950474c212fa2ff9d73e63705c6ac4bcca04ac13`.

Both pulse60 runs passed600RACING samples with1,331 observed local integrations at exactly0.00749999983s, maxSteps6 (histogram: first uninstrumented sample0, then466frames×2 and133×3). Fresh pulse30 runs passed300 samples: model10 had1,324 integrations, model14 had1,325, predominantly4/5 per frame. All108 parameter-map records, four mass/inertia records and metadata exactly match each respective60 run. Parameter-map records also match between the two models; these are common curve records and do not imply equal selected tuning or equal handling. No observed frame exceeded six steps. Requested dt switches only after RACING: original five-frame smoothing ramps16.667→20→23.333→26.667→30→33.333ms at30Hz. Thus accumulated engine dt is9.9500194s for300 samples versus10.0000162s for600 at60Hz. These are **not identical-effective-time trajectory comparisons**; the first RACING sample also precedes local observer activation.

Exact pulse receipts/logs, relative to that native-comparison directory:

| Run | Path | Log SHA256 |
|---|---|---|
| model10/60 | `attempts/20261022T202129Z/stock-stewart-mx.log` | `1b4a677e45ec2e0bc10b8aab45da4f5f8e8816e52ee9005b93bd8f34cd58a692` |
| model14/60 | `attempts/20261022T202129Z/current-neptune-mx.log` | `adec14c6ecc32494ef47b7380e6ed4d098a9b5763ab6fc14dcda4d11a6ab2452` |
| model10/30 | `attempts/20261022T203118Z/stock-stewart-mx.log` | `f4dc89636943962af079fdf7f66a1738f1c54427a61b5b5140a063c7c211dc55` |
| model14/30 | `attempts/20261022T203118Z/current-neptune-mx.log` | `b2cc9a8ee9c13423a4781b34a7ff4be6c5519f14bc30deb2c09a175075143fab` |

`attempts/FRESH_30_60_COMPARISON.json` SHA256 `4c6fb256894bcc709fc6425761f8ee45c8e6aaefed9290b9df6d2a8921f44802` records the fresh comparison and its timing limits.

Steady-corner60 uses RT128, zero other axes/buttons, straight3s, LX+16384 for2s, neutral1s, LX-16384 for2s, neutral2s, with the same original-clock anchor. Both passed600 samples and1,331 integrations. Actual raw and processed steering span approximately−0.342087..+0.342127 after stock deadzone; no extra native pad attenuation appears here. Model10 had1,112 active rear FINAL calls/219 inactive; model14 had1,066/265, with no entry/final/exit pairing errors or normal-length outliers. The first positive-steer interval (logged elapsed3.01662064→5.01659012) has266/266 active rear calls per model: lateralGrip clipping is1/266 stock and0/266 current; demandLength>1 is4/266 and0/266 respectively. This is **almost no lateral clipping in that observed first turn**, not a claim of no saturation throughout the run. The later opposite turn has42/175 and23/154 clipped active calls; contact gaps and differing trajectories prevent a matched handling conclusion. See brightness's `REAR_CORNER_AUDIT.md` and `CORNER_PHASE_ANALYSIS.json` in `attempts/20261022T202627Z`. Corner log SHA256s: model10 `04b99c869a1d42544d5250c3d7f0761f088056a24b5e8f220fbea1836c09c5f4`; model14 `c9de1f65f722532166c6644b35cfc70556ca03dae247d2cb696aae7a03188017`.

Current source corrections to older notes: water sampler007f8d10 is now registered. Startup remapping applies38 saved values only if a controls file exists; otherwise it leaves native defaults. No controls file was found under this user's Application Support during this audit (an explicit REFLEX_SAVE_DIR override still needs its own check). Default camera is button16, not a stick axis. The optional stick-drive branch tests input+28 = simulation+44; construction initializes it to zero, but these logs omit that field, so its actual runtime value is not proved. Current cadence/input source findings are in `builds/sol_bike_physics_verification/input-audit`. No arbitrary feel tuning or physics edit is justified by these native-only observations.
