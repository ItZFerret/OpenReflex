# ld_tables (0x4d0000..0x540000): wave 4, native loading

Status 2026-10-18: all 356 unported rows of plan/loading-waves.tsv for ld_tables are ported (lifted with tools/lift, captured
backend): ports/ld_tables_4d.cpp .. ld_tables_53.cpp (one file per 64 KiB); 15 M boot instructions. Nothing in the range is left
unported.

Acceptance (2026-10-18): `loading_accept.sh <boot> ld_tables 1` and `... alpine`: canonical dump IDENTICAL, world images 0 pages
differ, race_compare IDENTICAL, race hashes identical. 345 (Concord) / 354 (Alpine) of the ports are called in the boot.

## What is in the range

MXTable helpers and table rows: schema creation (ActivityResults_CreateSchema, PlayerList_CreateSchema and the other
FUN_004de*/004e1*/004e2* schema builders called from the static-table init 004e6280), std::basic_string helpers (004f48e0 is
basic_string::_Copy: grow, allocate through 004f5290, copy with _Copy_s, with a try/catch), std::vector / tree helpers, table row
readers on the loader thread (004ef8f0, 00539720, 00536ef0 ...), the network driver vfuncs (NetworkDriver::vf1, 0050e380).

## Trap: Ghidra entries that are labels inside a function (fixed in tools/lift)

004f48e0 (basic_string::_Copy) ends its try block with `jmp 004f498c`; Ghidra made 004f498c a function of its own (the catch funclet
`mov eax, 0x4f4983; ret` returns there), but it is the tail of 004f48e0: it reads [ebp + 8] / [ebp + 0x10] of 004f48e0's frame and
returns with 004f48e0's epilogue. The lifter treated every `jmp` to a functions.tsv entry as a tail call (LIFT_TAILJMP: pop a
return address from [esp], call the target, return to that address), so the native popped the security cookie as a return address
and the boot died at `execution outside mapped code bfa0b27d` (the cookie xor esp). Fix in tools/lift/lift.py (walk / stack_step /
ret_cleanup): the walker carries the stack state (esp offset from the entry, ebp offset, "ebp is this function's frame pointer"),
and a `jmp` to another entry is a tail call only when that is not known to be wrong: if the frame pointer is still set up or the
known esp offset is not 0, the jump is internal and the target is lifted inline (always exact: the same instructions on the same
state). Calls with a known single `ret N` keep the offset; indirect / import calls make it unknown (then the old tail-call rule
applies). Changed jumps in these groups: 004f48e0 -> 004f498c, 00803040 -> 0080310b, 00803790 -> 008038ca. The targets keep their
own ports (they run when original code jumps there: a native at a pc runs whether it is reached by call or jmp).
lift_fl.py (the fl backend) still has the old rule (`is_other_entry`).

## Other lifter additions

- `wait` / `fwait` (Fn_00501bf0): no-op, as vm.cpp.
