# ATV startup methods (2026-10-21)

Source: original RavenSimulationATV vtable 0093d054; `_re/out/decomp/reflex_001_00459400.c`, functions 0048cde0 (vf35), 0049c8d0 (vf2), 004997f0 (vf16). Ports are `game/vehicle_atv_methods.c/.h`, registered through RX_FUNCTION and oracle group `rx_AtvMethods`.

vf35 scales tire/suspension and engine fields in original SSE operation order, including delayed final stores. Ranged values preserve FLT_MAX sentinel comparisons (unordered NaN does not equal sentinel). vf2 retains all six body placements, ranged reset records, rear vertical/lateral offset construction, history copies, contact draining and virtual calls. vf16 derives the four suspension frames: front half-track radius intersection with .1 fallback; rear YZ direction normalization and -.7 sector test, replacing forward directions by (0,+/-.15,-1,-1). Original four-lane shuffle results, including nonstandard fourth lanes, are retained. Shared quaternion and masked-normalization helpers implement the original inlined operation sequences.

`tools/atv_methods_test.c` covers finite/FLT_MAX/NaN/negative-zero tuning and isolated reset pose/flags/canaries. It substitutes fixture callbacks at vf16/vf32; it does not establish complete production startup. Parent runs all compilation and tests centrally.

`oracle/vehicle_atv_probe.cpp` uses a real original ATV world (garage `0,0,-1,8,2,4`, PresetVehicle 2 -> model 0). It saves/restores the complete live simulation plus 12 pointed-to ranged records, invokes 3 tuning cases and 3 vf16 pose stimuli, then invokes reset with actual six bodies while clearing external contact/ray queues for isolation. Enable `REFLEX_PORT=rx_AtvMethods,rx_AtvMethodsProbeHook REFLEX_PORT_VERIFY=1 RX_ATV_METHODS_PROBE=1 RX_ATV_METHODS_FRAME=650`. The forced rear heights are stimuli, not proof both fallback branches ran; inspect original Verify results before acceptance. Probe is developer-only and not product code.

Remaining boundaries: reset's contact helper reaches ATV vf32 004a1a10. Subsequent startup/runtime slots include vf19 0047c460, vf20 0048f9e0, vf23 0049ee00, vf33 004862e0, vf60 0047e850, vf62 0047cfa0 and vf29 0047d870. These ports do not establish playable ATV simulation. See vehicle-import-inventory-2026-10-21.md for full family inventory and monster-truck remnants.

## Latest pending validation and next slices

004a1a10 (S vf32, 2777 bytes) is now implemented and registered. It builds 13 probes, raises three by speed (capped at 1.5 at speed 10), and compares them to an on-stack baseline: first eight copied probes, four negative inverse-rotated forward axes, then the raised rear midpoint. This differs materially from MX's twelve-probe persistent-copy baseline. Oracle probe adds three cases with all 13 rays (live velocity, zero velocity, X velocity 20). Compile/oracle validation pending centrally; this supersedes the preceding remaining-vf32 statement.

Current next boundaries from original function census (byte counts, not source lines):

| Owner | Address/slot | Bytes | Dependencies / scope |
|---|---|---:|---|
| Landed / original verified | 004862e0 V33 |575| Four TerrainTracks_CutStrip calls then field resets |
| Other agent, placement | 0047cfa0 V62 |1741| Composite child pose, DynObj_SetPose, nearby statics, 0042b000/0042bd90/004603f0 |
| Other agent, placement | 0047e850 V60 |24449| Large full hierarchy pose; node world position/rotation, matrix, trig/math. Major slice |
| Unported | 0047d870 V29 |4058| Per-frame update/replay/world nodes plus 00487b30/00489c80 and rider helper006243d0 |
| Unported | 004847e0 V31 |1127| Track allocation/free, physics/proxy setup, locks and containers |
| Unported | 00484c50 V35 |5283| Rider animation/state publication, locator references and 19 calls |
| Unported | 00486100 V34 |473| TerrainMaterialAt +00402dc0 |
| Unported | 00486520 V61 |51| Scheduled third-frame call0049d7c0 |
| Unported leaves |00486560/80/a0/c0/e0 V30/36/37/38/40|31 each| Copy simulation pose/velocity fields to output |
| Staged only |0047c460 S19|7| Steering FLD S+5898; exact x87 adapter required |
| Staged only |0049ee00 S23|39| Existing Simulation_ReadControllerInput then shift-input copies |
| Unported |0048f9e0 S20|4487| Terrain/ray setup, PhysicsWorld_RayCastBodies,0075c580/007c5aa0 |
| Unported |00490b80 S11|247| Dispatches seven large ATV tire/control helpers |
| Unported |00490c80 S13|5784| Contact dispatch/body contacts and five ATV-specific helpers |
| Unported |00497a70 S12|1160| Controls,00494910/00496880, angular clamp and stick mapping |
| Unported |0049ee30 S9|7294| Force/control update, curves/terrain and00499220/004a0ab0 |
| Unported |004a0e60 S10|993| Angular/force math, atan2 |
| Unported |0048d570 S15|4897| Contact response,0048d430/0048ef80, mechanism kernels |
| Unported |004931a0 S17|1154|004a17b0 helper |

No claim of playable ATV support follows from the startup ports. Full V60 and S9/S11/S13/S15 control/contact closures are substantial remaining work.

## Original acceptance result (supersedes pending-Verify statements above)

Parent completed all five original comparisons: 0048cde0 tuning 3 calls, 0049c8d0 reset 1, 004997f0 suspension frames 1,559, 004a1a10 probes 1,559, and 004862e0 V ground reset 4. Every method reported zero mismatches. On/reference state hash `a01204e338e8cd97` and memory hash `021d09ba5952f78a` match. V ground reset is in vehicle_atv_place.c/.h (placement owner).

The vf16 oracle stimuli were corrected to actual body-position Y offsets234/2d4/374/414 before this run; previous240/2e0/380/420 were velocity fields. The accepted run covers the corrected source. Final native product build and startup test remain pending centrally. This accepts the five bounded methods only; it does not establish playable ATV support or close the remaining V60/V62 and main simulation paths.

Final product startup check: all native fixtures pass; real public profile/event load now reaches unported V60 `0047e850` on frame1. It has passed the initial simulation tuning/reset/probe and vehicle ground-reset boundaries, but is not yet a playable ATV. See `builds/sol_core_features/vehicles-final.json`.


## Current pose closure work

V62 0047cfa0 is implemented in `vehicle_atv_pose.c/.h` with adapter group `rx_AtvPose`; compile/original comparison remains pending. It retains the actual four wheel-node/child indices, named handle composite child, dynamic-body pose, authored bounds, nearby-statics refresh and final debug-input branch. It does not substitute MX placement. V60 0047e850 now has a complete source candidate in the same owner: original root pose, independent mirrored front linkage, rear-frame solve, steering and three evaluated shock triples. Scalar cosine fits and matrix-to-quaternion branch ties retain the source operation structure, including right-side direction reversal and the alternate trace sums. Both methods are frozen for initial original Verify; source completeness is not acceptance and playable ATV status is still pending.

## Runtime and wheel publication closure, current comparison status

`vehicle_atv_runtime.c/.h` adds V29 (0047d870), V31 (004847e0), V35 rider publication (00484c50), V34 safe pose (00486100), V61 scheduled post-step (00486520), five MOVAPS vector getters, and the exact predicate leaves they reach. Adapter group is `rx_AtvRuntime`. The original-world probe group `rx_AtvRuntimeProbeHook` exercises ten live/edge-bit getter cases, three safe-pose branches, and four scheduled phases with the actual ATV state saved/restored. It deliberately does not synthesize V31 world transfer: that method mutates scene/listener/track/heap state outside the vehicle.

Parent's first original comparison produced positive zero-mismatch getter, V34 and V61 cases. V29 failed at the security-report routine (emulator rejected segment register SS at 008fa46a): this was a real port error, not an emulator workaround. The epilogue had retained the MX cookie offset +bc while the ATV frame stores/reads +cc. The cookie read is corrected to +cc, and the divergence scratch snapshot is corrected to original +20. The next original comparison is pending; no V29 acceptance is claimed here.

`vehicle_atv_wheels.c/.h` and distinct `rx_AtvWheels` adapter now cover original 00487b30. The four original deform parameter blocks and static splat guards, source-ordered terrain-grip averages, four wheel-lift force publications, water/wake state, and body/rider terrain-track contacts are translated. It uses the original aligned frame and four-wheel data, with no MX offsets or skipped live branch. A tiny C syntax check passes. Original Verify is still required. Conditional water-entry calls 0045f500 and 00424340 remain explicit unported boundaries, as they currently are in the MX path; dry-terrain acceptance would not prove those branches closed.

A targeted source scan did not establish any deliberate disabled-contact shortcut in existing MX/ATV pair dispatch. Unsupported pair cases retain explicit boundaries. The wheel-publication helper is materially relevant to physics: it supplies grip to all four suspensions and publishes four lift forces, in addition to drawing tracks. These ports do not by themselves establish playable ATV support while the remaining simulation/control closures are outstanding.

Follow-up: parent isolated original V35 rider publication and 00489c80 contact filters on the actual racing drive: 1,400 calls each, zero mismatches (`builds/sol_atv/logs/rider-isolated-verify.log`). V29's corrected cookie eliminates its exception, but its 700 current comparisons still fail (first frame 497 words, including inherited pose nodes and a two-ULP db46e4 difference); V60 is being corrected independently before attributing the residual.

The bounded water leaves have now been translated in `vehicle_atv_wheels.c`: 00424340 guarded singleton lookup/init and 0045f500 add-to-S+2f00. The new ATV helper invokes them with typed direct calls. Original water-branch comparison is pending. Separate existing water-wake/splat code still has original bridges (including a NULL-context WORLD_CTX0 singleton call), so registration alone does not establish that whole water interaction closure is product-native.

## Pose/control closure follow-up (current batch)

V62 passed 1,400 original comparisons with zero mismatches (`builds/sol_atv/logs/fourth-pose-drive.log`). V60's startup comparison passed after correcting the rear-frame fitted sine/cosine slots and last shock's rotated-X cross basis. The longer 1,400-call run still found 68 ULP-only mismatches, beginning in the right third front-link node. Source review then corrected its alternate quaternion trace association: original 004823ed..00482425 (and rear 00482d26..00482d5a) computes `row1.y + (row0.x + 1)`, followed by `+row2.z`, rather than `(row1.y + row0.x) + 1`. Repeat original comparison is pending; V60 is not accepted from the short startup result.

`vehicle_atv_controls.c/.h` now contains the complete source candidates for S12 00497a70 and its airborne 00494910 / grounded 00496880 helpers. They retain the distinct ATV fields, trick counters/phase thresholds, all six mechanism bodies, curve-based stick rates, exact flight basis/quaternion normalization sums, contact damping, and front/rear tipping limits. Existing math helpers are reused only for matching original instruction idioms. C warnings/syntax check passes. Adapter group `rx_AtvControls` preserves ECX/RET4, stack S+dt/RET8, and ESI/RET4 respectively. The development-only `rx_AtvControlsProbeHook` can exercise dispatcher3, ground5 and air7 cases on a genuine original ATV and restore its complete simulation state between cases. Original comparison remains required before acceptance; probe stimuli are branch coverage, not evidence of a natural jump/ride.

Original comparison update (`builds/sol_atv/logs/wheels-water-verify.log`): AtvWheels 1,401 calls, warm singleton 1, water-force leaf 5, all zero mismatches. Probe found an authored queryable patch and compared its full-helper stimulus, but logged `wet=0`; this does not establish the immersed-state transition branch. Shared water-wake singleton bridge now uses a typed native call, while Wake_AddSplatQuad 007ec280 and Wake_AddSplat 007ecd80 remain explicit original boundaries.

`vehicle_atv_response.c/.h` adds S15 0048d570 and wheel-slip limiter 0048ef80, plus actual rest-state query leaves 0048ee30/60/d0. New adapter group `rx_AtvResponse`; opt-in actual-S probe `rx_AtvResponseProbeHook` / `RX_ATV_RESPONSE_PROBE=1` tests contact flag branches, normal zero/finite/Inf/NaN cases, and brake/curve slip branches with S restored. Main S15 uses natural per-frame calls to avoid manufacturing mechanism side effects. Syntax passes; original comparisons are pending. Independent peer review confirmed lock-entry quaternion chains and body/wheel rotation direction; this is source review, not oracle acceptance.

Continued V29 source audit found a definite inherited defect in its S+2de0 alternate rider frame: original first forms vehicle quaternion multiplied by global 00ad66b0, rotates the inverse offset by that combined quaternion, then forms conjugate(sim+2e00) multiplied by the combined quaternion. The port omitted the global and reversed the final product. ATV V29 is corrected; existing MX source is unchanged pending accepted ATV evidence. Current long V29 comparison still has failures before that fix; no acceptance is claimed.

## Accepted original comparisons for pose, controls and contacts

The corrected V60 and V62 each passed 1,400 calls with zero mismatches in `builds/sol_atv/logs/fifth-pose-drive.log`; this supersedes the pending pose result above. Controls passed 1,963 dispatcher calls plus seven positive airborne-helper and five grounded-helper probe calls, all zero mismatches (`builds/sol_atv/logs/fifth-controls.log`). The original S13 contact dispatcher passed 3,111 calls with zero mismatches (`builds/sol_atv/logs/fifth-contacts.log`). These are original-code comparisons, not assertions of a completed native product ride or a natural airborne test.

`vehicle_atv_contacts_probe.cpp` adds bounded additional helper stimuli: two rest-pin cases, eight correction contact masks and seven rolling modes. It saves/restores the complete six-body simulation and all twelve external rolling filters. The rolling stimuli require the actual node callbacks to be the known 0085a8f0 projection with both output records inside the saved simulation; an unsupported callback closure logs a skip. Full S13 world/terrain stimuli remain natural-drive coverage because restoring only simulation/filter memory would not reverse shared world effects. Positive results for this new probe are still pending centrally.

S15 first original comparison: 3,111 calls had only an ESP mismatch, caused by adapter RET0 versus actual original RET4 at 0048e88e (decompile omitted the unused stack argument). Adapter now uses RET4. Its slip helper and predicate/normal probe cases each had zero mismatches. Next S15 comparison is pending.

Correction to the preceding V29 audit: the final quaternion order was initially misread from decompile substitutions. Full register tracing at 0047e3cc..0047e481 confirms combined R is the left operand, hence riderRotation = R * conjugate(sim+2e00). The omitted global multiplication was a real defect; the subsequently introduced reversal was erroneous and is now removed. Current source retains R = vehicleQuaternion * global00ad66b0 and uses R * conjugate(q0). The intermediate run cleaned position globals but retained 518 mismatched V29 calls with small rider differences; the next comparison must establish acceptance.

The isolated natural contact-leaf run also passed: 12,444 terrain queries and 3,111 calls each to correction, rest pin and rolling orientation, all zero mismatches (`builds/sol_atv/logs/fifth-contact-leaves.log`). The newer branch-stimulus probe remains separately pending.

The previously explicit wet-contact wake queue boundaries now have source candidates in `vehicle_water_wake.c/.h`: quad 007ec280, impact splat 007ecd80, center append 007f9510 and float append 007f9550, with `rx_WaterWake` oracle adapters. Both vehicle callers use typed native calls with original stack depth. The quad retains CW53 horizontal speed-square, fitted asin plus existing original axis rotation, original six-vertex order, queue cap and quality-dependent secondary queues. The impact secondary copy retains its x87 signaling-NaN quieting, which differs from the quad's raw float append. Warnings/syntax checks pass; positive original comparison is pending, so this does not yet establish wet riding acceptance.

The remaining V57/V51/V52 source batch is now implemented in `vehicle_atv_telemetry.c/.h`, with `rx_AtvTelemetry` adapters. V57 00486850 is the four-wheel axle grip factor; V51 00486cc0 and V52 00487510 publish diagnostic snapshots, rather than performing network transport. Its directly required missing V47 RPM getter 00486620 is included as the exact x87 FLD/FMUL extended result. Source review retains V57's front-left-only normal-weight asymmetry, snapshot output holes/raw integer flags, scalar x87 NaN quieting, actual four-wheel quaternion/field offsets and source vector-add association. C syntax/warnings check passes. The optional real-state `rx_AtvTelemetryProbeHook` exercises rpm1/grip9/vector-snapshot2/value-snapshot2 cases, restores the actual 6000-byte vehicle, complete simulation and aligned scratch output. Original comparison is pending.

## Wake and contact-probe original acceptance

Water wake queue methods passed the original positive probe in `builds/sol_atv/logs/sixth-wake.log`: eight quad calls, six impact splats, one center-append and four float-append calls, all zero mismatches. The cases used the actual wake object/queues, emitted the original 30-float splats and covered both quality1 and quality2 paths. This accepts the queue closure and typed caller integration; it does not establish a natural wet ATV ride or visual wake rendering acceptance.

The additional contact-helper stimuli also passed: correction8, rest-pin2 and rolling7 cases, zero mismatches, together with the already accepted 3,111-call natural S13 run. The complete six-body state and twelve external rolling filters were restored between helper stimuli. Telemetry comparison remains pending the next centrally built binary.

Sixth original acceptance update (supersedes V29/S15 pending statements): parent reports runtime V29 1,400 calls with zero mismatches; S15 3,111 calls, slip limiter 5, contact predicates and normal edge cases all zero. The exact S15 C prototype now includes its unused stack float and adapter RET4. Deep authored-water comparisons: wheels 1,403 calls zero; deep case returned normalized depth1, wet1, entered1, rider gesture0x16 and force40. Logs: `builds/sol_atv/logs/sixth-runtime.log`, `sixth-response.log`, `sixth-water.log` (use actual parent log naming if suffix differs). This establishes the tested runtime/contact/water branches, not full ATV playability or destruction closure.

Bounded lifecycle source landed in `vehicle_atv_lifecycle.c/.h` with oracle group `rx_AtvLifecycle`: V+90 listener pre-step0047d670, V9 detach0047cbc0, deleting wrapper0047c780, and shared base destructor0045d3c0/base detach00460230. Source preserves explicit unported simulation-destruction and optional effect boundaries. The opt-in lifecycle probe uses the actual original base constructor and allocator for flags0/1/2 deleting-wrapper comparisons; it never reads freed memory. Full V9 is reserved for natural cleanup because restoring only V would corrupt external world/heap state. Syntax passes; lifecycle original comparison pending.

Simulation destruction closure is staged outside compiled inputs: exact0048c730 scalar deleting wrapper and0048cfc0 body, plus0042f9f0/0042fd50/0085c390 MOV-only subobject destructor leaves and0081f6e0 vector buffer/proxy release. The sparse reset sequences were independently checked against original instructions (36/60/13 offsets). Existing native registry removal, allocator and tuning initialization cover all direct callees. Staged original-constructor probe uses newly allocated S objects with tail canaries for flags0/1/2; it does not destroy live S or inspect flags1-freed storage, and restores the shared tuning callback. Syntax passes; no original acceptance yet. Optional animation timer effect00476340 remains an explicit unported boundary in listener pre-step.

Seventh original acceptance: lifecycle deleting-wrapper flags0/1/2 (3 calls) and actual V+90 pre-step (1,400 calls) report zero mismatches. Simulation deleting-wrapper flags0/1/2 (3 original-constructed S cases) report zero mismatches and canaryErrors0; active-world S was not destroyed. This accepts the tested destructor bodies/flags and pre-step, not actual complete V9 teardown: original racing run did not call V9 naturally. Handle callback closure0042af20/0042b090 has subsequently landed with exact reference decrement, conditional record removal/free, clear-on-release and deleting flags. Its new probe covers original-constructed empty handles only; nonempty/refcount-zero record removal remains for natural teardown. All direct V9 cleanup callees are now translated; optional timer effect00476340 is still an explicit listener boundary. No complete V9 original comparison is claimed.

Telemetry original acceptance: `builds/sol_atv/logs/seventh-telemetry.log` passed RPM1, grip1,409, vector-snapshot1,402 and value-snapshot1,402 calls, all zero mismatches. This includes the bounded positive probe cases and natural-drive calls. The x87 RPM getter dependency and both telemetry records are accepted for that run.

### Native wreck-checker ABI integration correction

The native race fixture reached 900 race frames but reported wreck type V+280=19
(0x13), zero processed throttle, and a rider separated from the parked ATV.
`world_wreck_checker.c::vehicle_rate` had typed return handling for MX only;
its generic native fallback cast registered ATV V56 004867b0 and V57 00486850
to `float (*)(rx_addr,uint32_t)`, although both C owners return `rx_f80`.
This is an incompatible C ABI, not a tuning discrepancy. The grip return scales
pitch wreck thresholds; zero garbage there triggers the original forced0x13
branch. The RX_ORACLE path calls the original instead and therefore did not
exercise this integration defect despite positive isolated runtime comparisons.

Both ATV methods now use their owner prototypes, preserve the original SP-8
call frame, and round rx_f80 to float at the original FSTP boundary. Unknown
vehicle method prototypes retain an explicit boundary instead of being guessed
as float returns. `core/tools/atv_wreck_rate_test.c` includes the actual checker
source, exercises native wheel/grip dispatch and stack preservation, and must
replace world_wreck_checker.o in the fixture link. Syntax checked only by this
agent; parent owns execution and integrated race/reset acceptance. No claim of
playable ATV is made from the source correction alone.

Parent subsequently ran the native wreck-rate fixture successfully after fixture
CRT startup and both-contact initialization corrections. Native Alpine ran 900
race frames, peak79.132ft/s and519.543ft displacement, 6 bodies/50 nodes/69 bones;
Gibson, Cheyenne, Concord4ATV and FreightYard4ATV also passed900 race frames.
These runs do not prove every water/recovery path: later preview logs exposed
0054e470 reset eligibility and007efed0 water-wake wrapper boundaries.

0054e470 now has an exact native read-only predicate in player_reset_gate.c,
typed reset-input caller and independent oracle group rx_PlayerResetGate.
The opt-in PlayerResetGateProbeHook (RX_PLAYER_RESET_GATE_PROBE=1) restores
actual player/activity/UI/global state while checking null/state/substate/timer
boundaries, NaN/Inf, UI blocking and global override. Source and native contract
fixture syntax checked; original comparison/execution remains parent's work.
The gate itself has no water dependency: it governs recovery input after a
crash. Successful eligibility can next reach00550400, which remains an explicit
unported manual-reset boundary at this point; existing automatic00550650 and
shared reset placement005501f0 already have native owners.

## Reported water-entry crash: guarded rider splash closure

The user-reported native water crash identified explicit boundary007efed0 in the rider/ragdoll wet-entry path, distinct from the previously accepted vehicle quad/splat calls. Original007efed0 is a25-byte wrapper: EAX points to the water singleton's wake slot, ECX points to the entry position, XMM0 supplies size, and the stack supplies strength (RET4). It returns when the wake pointer is null; otherwise its two saved-register pushes and two outgoing arguments call007ecd80. `vehicle_water_wake.c/.h` now implements that exact guard/frame and its strength FLD/FSTP signaling-NaN quieting. `rx_WaterWake` adds the adapter and six positive null/live/NaN-strength cases across both quality levels; comparison is pending centrally.

Source review of `phys_rider_water` typed dispatch confirmed constructor0042b410's stack(vec,&zero)/RET8, erase007cf450's ESI vector plus five stack arguments/RET20 (the native equivalent consumes out/first/last and retains all five original argument slots), and guarded-splash stack depth. The original0042aa38..0042aa4f explicitly loads each contact sphere's radius fromA+a7b8+4k immediately before the splash. That value is now reconstructed in the oracle register context and passed to the typed native wrapper, replacing the stale tracked XMM0. Product branches also call the existing native water initialization, vector constructor/erase, singleton and CRT exit-policy functions directly.

This is source confirmation, not runtime acceptance of a complete wet ride. The shared singleton's guard-false `_atexit` call in `vehicle_atv_wheels.c` still used WORLD_ORIG at this review and was reported to its owner; the current rider wet loop has set the guard before reaching that helper, but a separate first singleton call must retain a fully native CRT path. Core promotion remains on hold pending positive guarded-entry comparison and the parent's complete native water fixture.

00550400 manual reset is now ported separately in player_reset_manual.c with
existing native activity vf19 and005501f0 dependencies. It preserves saved-spot
priority, exact original quaternion-to-axis operation order for both saved and
vehicle branches, original frame/untouched lanes, and reset reason1. Caller is
typed. Group rx_PlayerResetManual, hook PlayerResetManualProbeHook and env
RX_PLAYER_RESET_MANUAL_PROBE=1 exercise three original calls on an actual local
racing player at frame900: delayed guard, saved actual vehicle pose, and real
activity/fallback reset. The probe intentionally leaves normal reset state in
place, rather than cancelling its timer; original comparison has not yet run.
Both ATV lazy water initialization sites now use existing native rx_crt_atexit
in product builds while retaining original calls under RX_ORACLE. Syntax checks
passed for all changed C files; parent owns native build and oracle runs.

Native water-placement fixture next exposed00550a20 at frame892 (MX). This is
out-of-bounds reason7 reset, not fade. It is now included in the same bounded
manual-reset module/group with exact original fallback axis arithmetic, existing
activity vf19/reset_to dependencies and a typed out-of-bounds caller. The real
manual-reset probe additionally calls its positive-delay guard and a legitimate
reset, leaving reason7/reset timer to advance normally. Syntax passes; no original
comparison or native post-fix water completion is claimed here.

The MX full-water fixture next exposed0045f500, already ported as the shared base vehicle water-force accumulator. `world_veh_wheels.c` now calls it through the actual typed EAX-vehicle/stack-float equivalent at SP4; original0045f500 reads V+234 and updates S+2f00, so this is shared by MX and ATV despite its existing symbol's ATV prefix. Its subsequent singleton query is now typed with SP8, preserving the original pending-point plus dummy push at004699a0/004699ac (singleton entry S-12), and the following height query remains at SP4. First-use `_atexit` guards in both `world_veh_wheels.c` and `world_veh_vehicle.c` use the native product exit-policy function; oracle builds retain original CRT calls. No water-specific original-only bridge remains in these audited world_veh water paths. This is source closure; complete native MX/ATV water fixture results are still pending centrally.


### Guarded splash NaN comparison limitation

`builds/sol_atv/logs/water-wake-verify.log` reports six guarded-entry calls,
with two mismatches confined to the injected signaling-NaN strength `0x7fa12345`
(quality1/2). Native produces the hardware FLD/FSTP quieted payload `0x7fe12345`;
the reference produces canonical `0x7fffffff`. All affected words are repeated
vertex strengths, including the quality2 secondary queue. The four other guard
cases and all other wake methods compare without mismatches.

This is a reference-emulator limitation: `native/drafts/world_runtime/jit.cpp`
FLD/FSTP falls back to `reflex_f80::from_f32`/`to_f32` for NaNs, and
`native/drafts/world_runtime/f80/f80.cpp:109` discards their payload into kind2;
its `to_bits` returns signed all-ones NaN. The same behavior exists in the core
F80 library. Replacing the wrapper's `world_x87_pass` with that library would
copy this artifact into the product. The native payload-preserving operation
and all six probe cases remain unchanged. Full edge-case original comparison
is therefore **not accepted** until the oracle preserves NaN payloads; finite
wet-ride behavior and NaN-payload fidelity must be reported separately.
Parent reports the latest native MX and ATV each survive over600 real immersion
frames, with wet depth1 and peak wake queues806/961 respectively; this note does
not promote that report into complete original-code NaN verification.

`RX_WATER_WAKE_FINITE_ONLY=1` explicitly omits only the two injected NaN-strength
guarded-entry cases and prints the omitted coverage. It retains four finite/null
guard cases and all other wake stimuli. Default execution retains the failing
NaN regression without any mismatch suppression or reclassification. Central
finite-only Verify and On/reference results remain pending.


### Final water product acceptance

See `HANDOFF/SESSION_2026-10-21_ATV.md` for completed finite wake comparison (allpositivezero), reset gate60/manual3/bounds2zero, matching On/reference state+memory, native MX/ATV601-frame immersion checks, actual windowed automatic remount and resumed riding for both classes, and signed Mac promotion. Known NaN emulator limitation remains documented above. The strict developer-teleport recovery diagnostic is not a passing test; normal player-driven recovery was verified separately. The shared singleton native CRT exit branch and MX shared water-force call are now wired.
