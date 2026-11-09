# ui group: input, user interface, cameras, sound logic (wave 2)

Range: the functions of `plan/nonphysics.tsv` at 0x00810000..0x0081ffff (input), 0x00590000..0x0060ffff (UI), 0x00630000..0x0065ffff
(cameras) and 0x00670000..0x0068ffff + 0x00800000..0x0080ffff (sound logic): 393 functions.

## How they are ported

Every function is an instruction-level transliteration generated from the original code by the scratchpad's `ui_tools/gen.py`
(a copy of the world group's rider generator, `rider_tools/gen.py`, extended; `ui_tools/genfiles.sh` regenerates all five files):

| file | range |
|---|---|
| `ports/ui_input.cpp` | 0x0081xxxx: KeyMap / AnalogKeyMap / CumulativeKeyMap / CompoundKeyMap, XenonInputManager, Keyboard_Poll, Pad_*, Mouse_* |
| `ports/ui_pages.cpp` | 0x0059xxxx..0x0060xxxx: MXUIManager, UIManager, UIPageGraph / UIPageStack, UIContainer / UIComponent, UIProperty_*, MXUI* |
| `ports/ui_camera.cpp` | 0x0063xxxx..0x0065xxxx: MXCameraEntity and the MX*Camera classes, camera paths |
| `ports/ui_audio.cpp` | 0x0067xxxx..0x0068xxxx: SoundEmitterEntity, (Crowd)LineSoundEmitterEntity, MusicPlayer_*, VehicleAudioManager, engine audio |
| `ports/ui_audio_queue.cpp` | 0x0080xxxx: the audio command queue (AudioEvent_IsPlaying, Sound_PauseAll, Audio_PostCommandWait ...) |
| `ports/ui_util.h` | helpers (copy of `world_rider_util.h`, namespace `uix`, macros `UI_*`) |

The registers live in C++ locals (EAX..EDI, EFLAGS as vm.cpp keeps them, xmm0..7), the stack frame stays on the guest stack at
the original's addresses, and every call goes through `Machine::call_function` with the registers synchronized, so callees (ported
or not, other groups' too) see exactly the original's state. Imports are reached the same way the original reaches them (the IAT
slot read and called, DirectInput / FMOD through their vtables), in the same order, so the world runtime's stand-ins see the same calls.
Outputs (the lead's Outputs rule): per function, the registers its callers read after the call, computed by the scratchpad's
`ui_tools/outs2.py` (result `ui_tools/outs_ui.tsv`, emitted by genall.py): whole-program register dataflow on the original code:
must-define and entry liveness of every function (fixpoints), then the least fixpoint of live-out over the ui functions and their
callers (10 levels; beyond: everything). A call to a known function kills only what it defines on every path (LTCG callers keep values in
volatile registers across known calls); indirect calls and imports kill every volatile register. Address-taken functions (vtable slots,
callbacks, by the relocations) keep the standard convention: at most EAX / EDX / st0, from every `call [reg + slot]` site of their slot.
Result over the 392: mostly EAX | EDX | FPU (139), some LTCG ones with ECX / xmm, 28 with none, 5 conservative (everything).
The analysis errs on the side of declaring more (unknown paths count as reads).

Generator extensions over the rider version: the FS segment (TEB at 0x7ffd0000: the SEH frames of the C++ UI code), movq,
pxor / por / pand / pandn, cwde, cmovcc, fcomi / fcomip / fucomi(p) (capstone's "fcompi"), fcom / fcomp / fcompp / ftst, fnstsw,
sahf, leave, xchg, cld.

## Status (2026-10-18)

392 of the 393 listed functions ported (857,562 of 857,706 instructions per frame of the range); `verify.sh ui`: 0 mismatches and
"on: identical" on all five worlds. Not ported: 00630065 (see below). Names identified so far: `_re/names/9e_ui.tsv` (the ports
of those use them; the rest keep their Ghidra names or `Fn_<addr>`).

Next steps toward readable native code: the generated bodies are a faithful first step; the hot ones (KeyMap::vf5 / vf6,
XenonInputManager::vf27, the audio emitters 0068b6c0 / 0068be30 / 00680470, MXThirdPersonCamera::vf19) are the candidates for a
hand-written rewrite against the same verify.

## Traps

- **Return addresses on the dead stack.** `call_function` pushes 0 as the callee's return address (that ends the nested run).
  The original leaves the real return address in that slot, below ESP after the call, and unported code later reads such
  uninitialized stack and copies it into live memory: with 0 in the slots, Verify showed 0 mismatches on every world but the On run's
  memory hash differed (Concord, Alpine, SX, TRX; bike state identical). `UI_CALL(target, ra)` therefore writes the original return
  address into the slot after the call returns, and `UI_TAILJMP` restores the slot it borrowed. With that the memory hashes are identical.
  (Any instruction-level port that calls through `call_function` from inside its body has the same issue.)
- Host-side stand-in state (heap, events, FMOD handles, worker switches) reached from these functions: SetEvent (sound emitters and
  MusicPlayer kick audio jobs), HeapAlloc / HeapFree (UI strings, page events, VehicleAudioManager voices), CreateEventA / CloseHandle,
  LockRect / UnlockRect of the deformation texture (the third-person and quick cameras' terrain queries), XInputGetCapabilities and the
  DirectInput keyboard GetDeviceState (XenonInputManager::vf43). Verify snapshots that state since the lead's 2026-10-18 change; before it,
  the double run made the world stop ("HeapFree of an unknown block").
- **00630065 is not a function** (left original, `ui_tools/skip.txt`): FUN_00630065 (9963 bytes, no callers) is the tail of
  FUN_0062fa90 (0x62fa90 + 1493 = 0x630065, a branch target Ghidra split off; world range). A native there catches a jump in the middle
  of 0062fa90. On mode still works (the transliteration runs on to 0062fa90's own ret), but Verify takes [esp] (a local) as the return
  address, overwrites it with 0 and lets the original run on past it: Concord 12 riders stopped in FUN_0062b710 (null 2nd argument,
  0062b826) a frame later. The coverage plan lists it as a function; other groups' plans may contain such split tails too (check
  unaligned entries with no callers that start where the previous function ends).
- Verify counts only the outermost ported call; nested ports run as natives inside it. Many ui functions are only ever reached
  from another ui function, so the "ports called" count (60..85 per world) is far below the number of ports executed.
