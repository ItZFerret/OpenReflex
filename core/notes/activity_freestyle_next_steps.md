# Freestyle and activity factory: next port scope

Updated October 22 after the initial read-only investigation. The three constructor ports and restored oracle probe are now integrated; lifecycle, scoring and factory activation remain pending. Shipping October 22 Mac/Windows artifacts were frozen before this constructor slice and remain unchanged.

## Actual failure and dispatch

The retained manual session `builds/sol_player_camera/evidence/manual-session-20261021-235100.log` starts Freestyle/FMX_BuenaVista at 23:51:14 and fails at frame 3 at the deliberate core boundary `0055ef98`. This is an interior factory dispatch boundary, not a separately callable original function. The original factory is `0055ef80` (1424 bytes, stdcall manager/type, RET8). Its authoritative dispatch is `_re/out/decomp/reflex_003_0050c500.c:56891`; current core supports only its race cases and FreeRide8. It appends the new object to the manager array even when allocation yields null; array growth `007cff20` is another current explicit boundary.

| Internal activity type | Original allocation / constructor |
|---|---|
| 0,1,2,3,4,7,17,18,19 | 0x13e660 /00565830, race vtable |
| 5,20 | 0x13e610 /00561910, Freestyle |
| 6 | 0x13ec70 /00578260, waypoint race |
| 8 | 0x114530 /0055fdb0, FreeRide (ported) |
| 9 /10 /11 | 0xff3b0 /0056d550; 0xff220 /0056b050; 0xff090 /00563d90 |
| 12 /13 | 0x13e680 /0056a210; 0x13e8e0 /00571000 |
| 14 /15 /16 | 0xff1f0 /00572d00; 0x100130 /00574730; 0x1014b0 /00576290 |
| 21,22,27,29 | challenge family0057aa30, distinct BigAir/HillClimb/TrailBlazer/TargetPractice vtables and fields |
| 24 /25 | 0xff3e0 /0057b740; 0xff400 /0057c3e0 |
| 31 | race championship intro, distinct vtable |
| 32 | Freestyle championship intro, base00562280, distinct vtable |
| 33 | waypoint championship intro, base00578260 |
| 34,38,42,46,50,54 | race championship practice |
| 35,39,43,51,55 | race championship heat |
| 36,37,40,44,47,52,53,56 | race championship main event |
| 41,45,57 | race championship LCQ |
| 48 /49 | 0x13ec90 /0057ea60 /0057e920 |
| 58 /59,60 | 0x13e600 /00562280, Freestyle championship practice / main event |

Types17 etc here are internal activity enums, not front-end event-type IDs. The source switch has no constructor cases23,26,28,30: preserve its actual null/default semantics rather than invent classes. Championship variants reuse a base constructor but require their own vtable methods and progression; changing only the vtable cannot establish career parity.

## Accepted constructor slice

Ported `005623b0` (53 bytes, FreestyleActivityAudioLogic), `00562280` (263 bytes, MXActivityFreestyleStub constructor), then `00561910` (68 bytes, concrete Freestyle constructor). Existing `005562c0` Activity construction is already registered and supplies the large player/scenario infrastructure. `00562280` builds three embedded scenario objects: FMXIntro at+0xff080, FMXFinish at+0x114250, FMXResults at+0x129420; installs vtables0094410c/0094414c/0094418c; initializes their owner pointers; allocates0x98 audio logic and stores it at+0xfd8. The concrete constructor installs009442cc, flags+0x2c8=0/+0x22c=1 and values+0x13e600=0,+0x13e604=100,+0x13e608=5. Assembly confirms audio construction directly calls00581030 (the already ported audio cue reset), not a virtual dispatch, before installing Freestyle audio vtable00942bbc. Exact ESI/ECX/EAX and stack contracts are recorded in the adapters.

These three constructors are registered in core/game/build_activity_freestyle.c with adapters in core/oracle/build_activity_freestyle.cpp. Ten restored direct cases passed: audio2, stub4, concrete4, zero mismatches. On/reference state hash fc092b3cdac8235c and memory hash262c4c467ea69cef over67648pages match exactly. Evidence: builds/sol_full_parity/freestyle-candidate/constructor-reference-acceptance.json and integration.json. The integrated files are identical to the verified candidate. Genuine allocation-failure coverage remains pending. This constructor-only slice does NOT make an FMX event playable. Keep the factory boundary until its execution closure is ready, or explicitly label a constructor-only intermediate implementation as incomplete. Do not substitute MXActivityRace, fake drivers, skip scoring, or hide Freestyle in the menu.

## Required playable closure after construction

* Lifecycle: concrete vf2=00561990 calls stub vf2=00562560 then event subscriptions00587930/005879f0/00587ab0 and mode-specific setup00561a70 or00562800. Stub startup queries actual authored start entities via007d2920/007d2820, including fallback positions. These are different from race-grid construction.
* Driver: stub vf44=00562ac0 allocates0xf50 and calls0050c900 (FreestyleDriver), which calls0050e1b0 (NetworkDriver) and00508c10 (base driver). Audit this complete chain and the FreestyleDriver vtable, including player binding and scoring ownership; constructor-only stubs are insufficient.
* Running/scoring: concrete vf28=00561e30, vf29=00561f10, vf46=00562170 and vf52=005620e0 manage judging, score/time, event completion and driver results. Driver-related vf47=00563740 subscribes00587690..00587e50 handlers. Concrete/stub update and these handlers are unregistered; port original stunt scoring and timer transitions.
* Intro/results and cleanup: stub vf25=00562e00 runs actual intro/rules flow, with scenario virtual methods plus existing007473f0 intro update. vf39/40/41=00562f70/005633a0/00563090 involve clip selection, records, score and placement. vf3/destructors00561d30/005627b0/005623f0/00561960 and event unsubscription00587b70/00587bd0/00587c30 must accompany startup to avoid persistent callbacks on event exit/reload. Shared intro helpers being ported does not imply FMX scenario vtables are ported.

This is a direct-call / vtable entry inventory, not a claim that the full transitive closure is resolved. `_re/out/functions.tsv`, `_re/names/10_rtti_vfuncs.tsv` and `_re/names/00_rtti_labels.tsv` supply original addresses and class identities. Beyond the three accepted constructors, the Freestyle-specific lifecycle/scoring entries in00561910..00563d00 inspected by the initial scan remain pending. Indirect call targets need actual vtable-word inspection plus assembly ABI verification. The existing _re/.venv interpreter provides disassembly support; no dependency installation was needed.

## Validation sequence

1. Restore actual saved-world guest pages, allocation host state and CPU for constructor comparisons; compare all constructed bytes, allocation order/addresses and live outputs. Use real settings/world objects and cover allocation failure only if an existing allocator fault mechanism preserves original semantics. Never manufacture a replacement world or synthesize fake game objects.
2. Obtain or locate an existing original FMX captured world, with real authored start entities, drivers, embedded scenarios and UI settings. No FMX saved fixture has been established by this investigation; Concord race captures can prove construction only, not natural FMX running/scoring parity.
3. Add restored probes for startup mode0/1/2/3, driver construction, successful/failed stunts, score/time limits, intro completion, finish/results, event leave/reload and allocation cleanup. Require positive exact Verify counts and On/reference full state+memory equality per slice.
4. Run the real FMX_BuenaVista event through start, riding, scoring, finish and exit, then championship intro/practice/main transitions with real career records. Other activity families in the dispatch table need their own corresponding closure and reference evidence.

The shared native factory boundary can cause a Windows Freestyle race-start failure as well, but the Mac evidence alone does not diagnose any particular Windows report. Full activity and career parity remains open beyond this first FMX workstream.
