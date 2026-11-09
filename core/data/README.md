# Immutable startup data

The product restores this compiled payload into its 32-bit arena. It does not open `MXReflex.exe`, either CRT DLL, or `Installers/vcredist_x86.exe`. Original content packages and install tables remain runtime assets. All executable behavior remains hand-written native core functions.

`initial_data.inc` contains zlib-compressed **data bytes**, original section metadata, and numeric import-slot bindings. It contains no PE headers, EXE `.text`, DLL instruction ranges, PE exports, or redistributable. `initial_data.json` records each source SHA-256, address, length, decoded SHA-256, rationale, and resolved import symbol. Compression reduces 850,014 data bytes to 215,121 bytes before C source encoding.

Sources are the supported PC 1.1.0.11 executable and Microsoft VC90 CRT 9.0.30729.1 at their preferred bases:

| Source | SHA-256 |
| --- | --- |
| MXReflex.exe | `917d14e7ef7ce492b665fea7d0c87c9a39b549e2a43b09883d2a786988e7c02c` |
| msvcr90.dll | `cf420aced0d810e1d75f6811dd986f2d9fded2fbb8d61fc9a7024520c475febb` |
| msvcp90.dll | `91fd46d7335c9990a20f215b9f6f53bc59551420a9c99ad8110ae2f9ff7598f0` |

EXE `.rdata`, initialized `.data`, and `.tls` are copied exactly, clamping file padding to virtual size as the prior PE loader did. Fresh arena virtual tails remain zero. Both CRT initialized `.data` sections are retained. The CRTs combine read-only data and code in `.text`; only these individually identified data objects are extracted:

| Address | Bytes | Reader |
| --- | --- | --- |
| `785219b4` | 58 | EncodePointer / KERNEL32.DLL / DecodePointer literals (`crt_thread.c`; oracle helpers) |
| `78521bf0` | 160 | printf character classes and state transitions (`crt_printf.c`) |
| `78522d98` | 120 | ten exception-action records; per-thread `_pxcptacttab` points here |
| `785230c8` | 512 | C locale uint16 character classification table (`crt_string.c`, `crt_printf.c`) |

The CRT math constants already live as literal data in `game/crt_tables.h`; they need no DLL data extraction. The core's string functions implement C-locale case conversion directly. CRT OS calls dispatch by original import-slot identifiers. 304 CRT function bindings (EXE and msvcp90 imports) are resolved from DLL export tables **during generation**, so runtime does not need either export table or any imported machine code. Original function addresses remain identifiers for the native function registry.

Regenerate from the repository's read-only reference files:

```sh
python3 core/tools/gen_initial_data.py
python3 core/tools/gen_initial_data.py --check
```

Or supply `--exe`, `--msvcr90`, `--msvcp90`, and optionally `--out`. These are development inputs only. The generator uses Python's standard library, writes deterministic ordered C / JSON (no timestamps or absolute source paths), and never modifies input files. Generation requires the exact supported hashes; `--check` checks the committed provenance, payload, every decoded data range, and import resolution against those files. zlib compression is deterministic for a given zlib implementation; byte hashes describe the uncompressed source data.

`tools/initial_data_test.c` independently restores the product arena without a VFS, snapshots its retained ranges, then calls the preserved PE reference loaders on the source bytes. It compares every retained arena byte and bound import slot, clears the reference-only code mappings, repeats embedded startup, and verifies EXE code remains absent. Its header contains the small standalone compile/run command; it does not build or run gameplay.

When adding a CRT port that reads another original static data object, add an explicit bounded data span and reader rationale to `CRT_RANGES` in the generator, regenerate, and rerun both checks. Do not broaden a span to contain instructions.
