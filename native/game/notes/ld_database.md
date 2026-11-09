# ld_database (0x810000..0x840000): Database, XMLReader, ZLib / LZX compression wrappers

Wave 4 (native loading), 2026-10-18. Files: `ports/ld_database_81.cpp`, `ld_database_82.cpp`, `ld_database_83.cpp` (lifted with
`tools/lift/gen.py --list`, captured backend; lists in the scratchpad `ldb/ld_database_8?.tsv`).

- All 271 unported functions of plan/loading-waves.tsv for the group are ported (271 lifted, 0 refused, 0 hand-written), 210 M boot
  instructions (loader thread 95 %). Top: FUN_008252c0 (77.8 M, a 10.9 KB function with critical sections and aligned allocs),
  ZLibCompression::vf11 (38.8 M), FUN_008237e0 (22 M), LZXCompression::vf11 (20 M), XMLReader::vf7 (16 M).
- Lifter extensions needed (tools/lift/lift.py, captured backend): `shld` / `shrd r/m32, r32, imm8|cl` (FUN_008252c0 at 00825a63;
  zero count leaves the destination; refused when a consumer reads its flags) and 8 / 16-bit shifts (`shr ax, 1` in FUN_0087c000,
  ld_engine; refused when flags are read).
- Imports (EnterCriticalSection / LeaveCriticalSection, _aligned_malloc / free / realloc, strchr / strtol / atof, _stricmp) are called
  through the same IAT slots in the same order (LIFT_CALL of `[IAT]`), so the boot's stand-ins and the baton schedule see the same calls.
- Named (9e_ld_database.tsv): 00829560 ParseCommaIntList, 00829640 ParseCommaFloatList, 008296e0 ParseCommaFloat2List, 008294e0
  ParseCommaBoolList (XML attribute value lists: EAX string, n comma-separated values).
- Acceptance: see ld_engine.md (the two groups were accepted together and separately in the race verify).
