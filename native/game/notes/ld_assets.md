# ld_assets (0x7f0000..0x810000): wave 4, native loading

Status 2026-10-18: all 292 unported rows of plan/loading-waves.tsv for ld_assets are ported (lifted with tools/lift, captured
backend): ports/ld_assets_7f.cpp (129), ports/ld_assets_80.cpp (163); 490 M boot instructions (Concord, plan TSV). Nothing in the
range is left unported. Regenerate with:

    awk ... plan/loading-waves.tsv -> scratchpad list (addr, name, group, file; names: FUN_x -> Fn_x, "::" -> "__")
    tools/lift/gen.py --list <list> --out ports/ld_assets_<7f|80>.cpp --group ld_assets

Acceptance (2026-10-18): `loading_accept.sh <boot> ld_assets 1` and `... alpine`: canonical dump IDENTICAL, world images 0 pages
differ, race_compare IDENTICAL, race state / memory hashes identical. 286 (Concord) / 289 (Alpine) of the ports are called in the boot.

## What is in the range

TextIcon / ImageIcon / Icon / IconProperties vfuncs (UI icon text layout: TextIcon::vf5 and FUN_00801830 are most of the 490 M,
main thread), model submission (Model_Submit / Model_SubmitNodes / Model_SubmitSkinned / Model_RenderInstance / Model_Bounds /
Model_LodForDistance, the render-queue front of the loading page), asset IO classes (SoundAssetIO, SoundBankAssetIO,
TextureAtlasAssetIO, IconGeometryAssetIO, CellAssetIO ...), CompressedInputStream::vf1 / vf2 (loader thread: reads through the
zlib / LZX inflaters of ld_inflate), the audio command layer (Audio_ExecuteCommand, Audio_FrameUpdate, Audio_UpdateListeners, ...),
sky / ambient-light entity vfuncs.

## Traps found (both fixed in tools/lift, see notes/ld_tables.md for the other one)

- **Signed-overflow UB in the lifter's `imul`** (Model_Submit 007f1b00): the lifter emitted the 2/3-operand `imul` as
  `uint32_t(int32_t(a) * int32_t(b))`. That is undefined behaviour on overflow, and clang -O3 used it: for `imul edx, eax` with
  edx == eax it folded "sign of x*x" to "not negative", so the `fild` + 2^32 unsigned-correction branch (`test edx, edx; jge`) was
  dropped. Model_Submit computes the fade byte `[edi]` from `[lod + 0x120]` = 0xffff (no limit): 0xffff^2 = 0xfffe0001 is negative as
  int32; the native lost the correction, faded models (byte 0x03 instead of 0xff) and changed the render-queue order of 1,548 of
  5,936 calls in the boot (11 canonical nodes differed). Fixed: the lifter now multiplies as uint32_t (same low 32 bits).
  **Every port lifted before the fix still has the UB form** (831 occurrences in ports/*.cpp of other groups at the time of writing):
  re-lift them, or build with `-fwrapv`.
- **Jumps to Ghidra "functions" inside a function**: 00803040 jumps to 0080310b and 00803790 to 008038ca (continuations after an SEH
  catch funclet, which Ghidra makes separate functions). The lifter used to emit LIFT_TAILJMP for them, which assumes a return
  address at [esp]: the native then ran the continuation as a nested call and returned through the cookie / saved registers. The
  walker now tracks the stack depth and lifts such jumps inline (ld_tables.md).
- `rol eax, cl` (Fn_007f7df0): the lifter refused variable rotates; added (count masked to 5 bits, 0 leaves the operand; the flags
  must be dead, as for the immediate form).

## Debugging notes

- Per-call Verify inside the boot (REFLEX_PORT_VERIFY=1 with REFLEX_BOOT_SCHED=baton) works for functions that do not allocate
  host heap: without the host snapshot (loading.md section 3 item 2) the native's allocations are not undone, so every allocating
  function shows false "heap address" mismatches (native lower addresses than the original's). Model_Submit allocates from a guest
  bump arena (0xd6daf8), so Verify located its bug directly.
- Bisect: scratchpad ldat/cset.sh / cbis.sh boot a port subset and world_compare its canonical dump against a saved reference.
