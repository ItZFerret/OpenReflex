# Core track coverage — 2026-10-21

Status: grouped native build and targeted real-arena contract passed. All eight serial track sessions reached RACING and completed 900 riding frames. Full new oracle/construction comparison remains unrun; runtime acceptance is not a claim of complete core or rendering coverage.

## Exact changes

- `game/anim_scenario_rider.c`: ported stance-2 output setter `00621640` as `rx_rider_animation_stance2_outputs`, and wired the stance entry directly under the existing guest call frame. Mode at `+0x5bab8` selects the original motorcycle/ATV constants or zero outputs. The two ordered upper-then-lower clamps retain COMISS/JB unordered behavior, signed-zero selection, nullable output pointers, and bitwise controller-parameter assignments. Evidence: `_re/out/decomp/reflex_005_005c8e90.c`, function `00621640` (line 65501), checked against disassembly at that address. No generic min/max replacement.
- Same owner: `00616f30` now resolves each of four child vf7 setters using its actual `(rx_addr,float)` signature. The guest stack float and existing callsite frame remain intact. Unregistered targets retain the explicit original-call boundary. This is dispatch wiring, not new animation math.
- `game/race_beh1_air.c`: ported gate target getter `0050ed70` as `rx_gate_rule_target` and wired AirLeveling directly. Original instructions are `fld [ecx+48]; fstp [eax]; fld [ecx+4c]; fstp [eax+4]; ret`. The implementation preserves sequential overlap and masked x87 signaling-NaN quieting. Evidence: `_re/out/decomp/reflex_003_0050c500.c`, function `0050ed70` (line 1672), plus its five-instruction disassembly. The adjacent line-rule target accessor `00508c00` already uses `rx_copy_pair_20` from `race_driver.c`.
- Owner declarations added to `game/anim_scenario.h` and `game/race_beh1.h`; oracle adapters added to `oracle/anim_scenario_rider.cpp` and `oracle/race_beh1.cpp`.

## Native validation

`tools/rider_stance_test.c` uses the real arena and literal source-evidenced expectations: 11 stance cases covering modes, clamps, inverted bounds, unordered NaNs, signed zero, null outputs, controller bits, and canaries; gate cases cover finite values, zero signs, infinities, signaling-NaN payload quieting, canaries, and overlapping output. Run it with `python3 builds/sol_tracks/run_contract.py` after the core build; this selects current source objects rather than stale historical object files. All cases passed (`builds/sol_tracks/logs/rider_stance_test.log`). This fixture is not an independent original-oracle comparison.

Dalton baseline evidence: `builds/sol_tracks/baseline/native_NAT_DLC05_Dalton.log` reached racing at step 1167 with four riders, then failed at frame 1463 on `0050ed70`. The fixed run reaches 2067 frames / 900 racing frames without that boundary. Kingston, Manchester, and Freight Yard previously failed at frame 3 on scene type 0x1057; all now complete. Stillwater, Bedford, Cheyenne, and Gibson retain the same recorded summary values as baseline. See `builds/sol_tracks/native-results.json` and per-track logs. All runs use only extracted `builds/sol_garage/game files` assets.

## Remaining boundaries and audit limits

Source-only review found genuine unported BehaviorBrakeTurn state handlers `00522fc0`, `00523230`, `005232a0`, and `00523300`, called by `rx_behavior_brake_turn_update` in `race_beh1_small.c`. These are state-machine work, not analogous trivial accessors; no approximation was added. They are candidates if the scripted four-rider track run reaches them, not a claim that the current routes require them.

The AirLeveling landing predictor `00461980` and corner helpers `00526b90`, `00526bf0`, `005380f0` already have native registrations; their original fallbacks do not establish missing core functionality. Dynamic vehicle virtual dispatch still has explicit fallback boundaries for unregistered concrete targets.

The earlier scan of constant direct-original targets against native registrations was a bounded dispatch audit, not a complete census: register-selected calls, virtual calls, macro expansion, signatures/calling conventions, and runtime reachability require separate inspection. Presence of a native registration alone does not prove a correct call frame. This work does not establish support for all tracks, broad AI behavior, replay, career progression, or every scene teardown path. Existing prior Alpine acceptance is separate evidence and does not validate these new changes.

Next steps: longer/more varied rides, additional installed events and original-oracle/construction comparisons. Inspect actual next blockers with original source/disassembly before adding further ports.

## Scene type 0x1057 — stadium roaming characters

Source closure added in `game/build_scene_anim_roaming.c`, with the factory case in `build_scene.c` and constructor declaration in `build_scene_int.h`. Type0x1057 is `LandAnimalProximity` / `AnimEntityRoamingProximityRSData`; stadium assets use it for camera operators and walking pit/crowd people as well as animals. Sources were the original decompile and disassembly, not lifted code. Factory instruction0040528b allocates0x890 bytes and jumps to006cfbe0. Base006b8d90 constructs CharacterEntity, sets the proximity interfaces and zeroes +0x18 in35 records at+0x39c. Derived006cfbe0 installs vtables00960dec/00960e6c/00960eac and initializes active/audio/scratch fields.

Load006cfd80 now creates the authentic model/character, resolves idle/run/mid animation handles with the original name equality/Editor_Default choices, retains durations and scratch allocations, reads proximity/home/shadow fields, computes matrix-column scales and normalized initial direction in the original SSE operation order, initializes heading history, and uses the original CRT rand stream for three animation phases. AssetHandle vf2 exposes the inline name at+4; this is the same handle invariant used by existing scene animation loaders. Activity callback006cfb40 queries the activity world's scene with original list capacity/grow300, disables characters for activity category3, and is registered on Activity Basic Essentials Loaded. Unload006d0670 calls the existing ModelEntity unload and removes the free-function handler with004197c0's actual key shape. Destruction006cfc80/006cfc30/006d3260 releases scratch/character state and preserves the renderable pointer adjustment.

Existing readable owners already implement the active simulation: loop_entity.c006d06a0(vf19), fx_animal.c006d09f0(vf20), fx_world.c006d07a0(vf29) and006d2eb0(steering), Character_SampleBlend3 and CharacterEntity matrix methods. No animation or proximity updates are disabled.

Read-only inspection of the extracted Base/Supercross scene sections counted92 roamers inSX_Kingston,127 inSX_Manchester and7 inSX_DLC01_FreightYard. None has an AudioEmitter field; all have the distinct AvoidSoundEvent field. No assets were rewritten or extracted.

Boundaries remain explicit: an actual AudioEmitter field reaches original helper006d05a0, whose sound-emitter construction is not ported by this scene change. Sound-owned destruction retains unportedcallee0041c850 after the existing stop/queue calls; there is no recursive fallback to this newly registered destructor and no silent audio dummy. Threaded animation vf30(006d1c80) is not added; the existing original threaded-animals global00a94324 is default0. These optional branches are not exercised by the inspected stadium characters. Other unbuilt scene variants are unchanged.

Central validation passed: complete native build and the three affected Supercross sessions, each with four riders and 900 RACING frames. Constructor/load behavior remains source-reviewed and runtime-tested, without a new bit-exact original-oracle construction comparison. Optional audio/threaded branches remain outside this acceptance.

## Stillwater rendering audit (read-only)

`builds/sol_tracks/logs/player_national.log:609`–`:613` reports missing materials `Nat_Sponsor_02`, `Nat_Sponsor_01`, `GL_Rainbowcolor`, `Nat_SW_OverhangSP`, and `MG_WP_Finish`. This is a rendering follow-up, separate from native track/frame acceptance and the known unported shader-program warnings.

The managed lookup in `Unity/Reflex/Assets/Scripts/Content/ReflexLevelContent.cs:193` searches the level's Base/other packages, then EngineDependencies only (`:196`–`:198`); `ReadAsset` rejects an unresolved name at `:207`. `ReflexScenery.cs:213` loads a material through this lookup. Ambient `EnsureMaterialData` logs failures at `ReflexAmbientContent.cs:114`; `AmbientMaterials` allocates the material array at `:224` and continues on unresolved definitions at `:231`, leaving null slots. The affected extracted Stillwater model bindings are Standard_Flag → GL_Rainbowcolor, NAT_CO_FinishGate_Flag → Nat_Sponsor_02/01, Nat_SW_Overhang_Flag → Nat_SW_OverhangSP, and MG_WP_Finish → MG_WP_Finish.

Inventory and actual-file checks under `builds/sol_garage/game files` found:

- Nat_Sponsor_01/02 and GL_Rainbowcolor definitions in `Ctrl_AmbientAnimation/Ctrl_AmbientAnimationBase`.
- Sponsor definitions also in NAT_Bedford/NAT_BedfordBase; Nat_Sponsor_02 in NAT_ConcordB/NAT_ConcordBBase; GL_Rainbowcolor in SX_KingstonB/SX_KingstonBBase. These are not interchangeable by name: the Ctrl sponsor materials are 698/693 bytes, while Bedford's are 1223/1214 bytes.
- MG_WP_Finish material in FR_Alpine/Shared, FR_Cheyenne/Shared and FR_Gibson/Shared; each is 879 bytes with identical SHA-256 prefix `c6b6c67604198902`.
- Nat_SW_OverhangSP has no material entry anywhere in the 123,555-asset inventory. Wood/Rope variants exist, but there is no evidence that either is an equivalent replacement.

A bounded original `game files/Archive/Database/NAT_Stillwater.dx9.database` decode confirmed 3,918 asset rows / 790 material entries and **none** of the five material names above. Its 92,454 bytes match the retained extracted-folder descriptor exactly. Thus these five local descriptor absences were not introduced by extraction; the global absence of Nat_SW_OverhangSP is specifically an inventory finding, not a claim about every possible game release or missing source package.

Next step: establish the original asset manager's dependency/package precedence and scene material-list override behavior for these models, then implement and test that scoped resolution using the matching package identities. Existing definitions elsewhere indicate a dependency-resolution gap but do not prove which variant is correct. Do not add arbitrary aliases or a global first-name-match search. Successful racing does not establish authored flag/sponsor/finish-object appearance. No asset mutation, extraction, build, or rendering fix was performed for this audit.
