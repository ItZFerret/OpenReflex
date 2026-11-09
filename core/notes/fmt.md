# File readers (`core/src/fmt_*`)

The layer that reads MX vs ATV Reflex's install files directly (the way Open Unleashed's libmxacore reads its game), in plain C11 with
its own data structures. These are readers, not game code: no `rx_mem` model, no registry, no original code. Everything is read
from the player's install (`game files/Archive` in the repository: the folder with `MXReflex.exe` and `Database/`); nothing is ever
written there. Product build: `core/build.sh` globs `src/*.c`, so the readers are in `librxcore.dylib` (hidden visibility, like
everything else: `rx_api.h` or the lead's glue decides what Unity sees). They compile clean with `-Wall -Wextra -Wpedantic -Wshadow
-Wmissing-prototypes`, as C++ headers too.

Conventions (`fmt_common.h`): `int` functions return 0 / -1, pointer functions NULL on failure, the text is `fmt_error()` (per
thread). `fmt_xxx_open/_parse/_load` allocate, `fmt_xxx_free` releases (NULL ok). Strings / arrays reachable from an object belong to
it. Little-endian host assumed (fields are still read byte-wise). Everything read-only after creation is thread safe
(`fmt_install_db`, `fmt_scene_entities` lock internally; reads use `pread` / overlapped `ReadFile` on a small handle cache).

## Modules

| file | what | main API |
| --- | --- | --- |
| `fmt_common` | errors, `fmt_buf`, LE readers, `fmt_f16`, case-insensitive helpers | `fmt_error`, `fmt_u32/i32/f32`, `fmt_buf_free` |
| `fmt_vfs` | the install as a read-only file system, case and `/` `\` insensitive, POSIX + Windows (UTF-8 root, `fmt_vfs_open_w` for wide) | `fmt_vfs_open`, `_find`, `_exists`, `_size`, `_read_at`, `_read_file`, `_list` |
| `fmt_inflate` | zlib / deflate (no zlib dependency) | `fmt_zlib_decode`, `fmt_inflate`, `fmt_adler32` |
| `fmt_lzx` | LZX decompressor written from the format (window 15..21, E8 translation, per-frame alignment) + the XMem resource wrapper | `fmt_lzx_new/_decode/_reset/_free`, `fmt_lzx_decode_once`, `fmt_xmem_resource` |
| `fmt_db` | `.dx9.database` / `.dx9.package`: packages, assets by (name, type), decompression; `fmt_install` = vfs + database cache | `fmt_install_open`, `_db`, `_read`, `_read_named`, `_database_names`; `fmt_db_find`, `fmt_package_find`, `fmt_db_read` |
| `fmt_bxml` | BXML (0x103EA zlib and 0x3EA raw), all value types incl. the scene ones, lookup helpers, `/Root/Child[2]/@a` paths | `fmt_bxml_parse`, `_tag`, `_child`, `_find_child`, `_find_attr`, `_attr_str/_i32/_f32`, `_node_value`, `_attr_value`, `fmt_bx_f32/_floats`, `_resolve` |
| `fmt_scene` | `.dx9.level`, the `.dx9.scene` container (subscenes, chunks, entities, RSData fields), and `fmt_level` (level + database + scene + the asset lookup order) | `fmt_level_open`, `fmt_level_scene`, `fmt_scene_entities`, `fmt_entity_field/_string/_float/_int/_matrix/_asset/_enum`, `fmt_level_find_asset/_read_asset`, `fmt_level_activity_subscene` |
| `fmt_table` | tbldata tables (`fmt_tables`, with the DLC merge the game does) and `localiz` string tables (`fmt_strings`) | `fmt_tables_load_merged`, `fmt_tables_find`, `fmt_table_int/_float/_str/_find_row/_cell`; `fmt_strings_parse/_get` |
| `fmt_texture` | texture header + R32F copy + RGBA8 decode of A8R8G8B8 / X8R8G8B8 / DXT1 / DXT3 / DXT5 | `fmt_texture_parse`, `_read_r32f`, `_decode_rgba8` |
| `fmt_material` | material BXML: shader names, textures + sampler states, parameters, pass states | `fmt_material_parse`, `fmt_material_param_find`, `fmt_state_get` |
| `fmt_terrain` | the level's HFTerrain: parameters, height map, coarse bounds, softness mask, material, friction zones | `fmt_terrain_params_read`, `fmt_terrain_load`, `fmt_terrain_coarse_bounds`, `fmt_terrain_load_softness`, `fmt_terrain_load_material`, `fmt_zones_load/_sample` |
| `fmt_collis` | `.collis` kinds 22 (triangles), 15 (shapes), others raw; entity matrix + placement rules; the level's CollisionRSData placements | `fmt_collis_parse`, `fmt_collis_entity_matrix`, `_transform_point`, `_chunk_position`, `fmt_collis_load_placements` |
| `fmt_model` | model 0x88aabb00 (nodes, local + world matrices, LODs, bindings) and surface 0x99bbcc00 (vertices, strips -> triangles, skin palette) | `fmt_model_parse`, `fmt_model_find_node`, `fmt_surfaces_parse` |

Typical use:

```c
fmt_install* in = fmt_install_open("…/game files/Archive");
fmt_level*   lv = fmt_level_open(in, "FR_Alpine");
fmt_terrain* t  = fmt_terrain_load(lv);              // t->heights (2048², raw R32F), t->coarse (128² min/max pairs), spacing …
fmt_collis_set set; const char* subs[] = { "FR_AlpineBase", "FR_AlpineFreeRide" };
fmt_collis_load_placements(lv, subs, 2, &set);      // each: asset, scene + entity matrix, parsed .collis
fmt_buf b; fmt_level_read_asset(lv, "MX_Hierarchy", "model", &b);   // any asset of the level (Base package first, then the others, then EngineDependencies)
fmt_model* m = fmt_model_parse(b.data, b.size);
```

## Format facts the readers rely on (all verified against the files; sources in the header comments)

- **Database**: root `Database` (one file spells it `DataBase`), `Handles`, `Packages/Package{name,file,offset,blockOffset,heapOffset}`
  / `Asset{type,name}/Heap{offset,size}/Block{offset,size}/Compress{enabled,codec}`. Bytes at `package offset + heap offset` in
  `Database/<file>.dx9.package`; LZX when `Compress enabled` and codec LZX, otherwise stored (the two `bxml` assets of RiderPhysics
  say `Zlib` with `enabled=0`: stored). `Block size != 0` (block resources) is unobserved and rejected on read.
- **XMem wrapper** exactly as `package_extract.py` documents, with the two leniencies of `Rfx.Lzx` (C#): the container may end before
  the heap region (`limit <= size - 4`, MXUI `PublisherVideo.lyr`), and a chunk whose packed size equals its allocation is stored.
- **LZX**: the frames of a chunk are one stream (frame headers removed, 16-bit alignment at frame ends), window 17, new stream per chunk.
- **BXML**: types 0 empty, 1 string, 2 opaque bytes, 3 int, 4 uint, 5 float, 7 vec4, 8 matrix, 9 vec2, 10 vec3, 11 bool; `count` is the
  element count (arrays occur: friction-zone data has 98305 uints).
- **Scene**: `u32 0xC0FFFEEE, u32 n`, `n * {char[256], u32 0, u32 offset}`, sections `{u32 chunks, u32 entities, BXML chunks…}`.
  Entities are `Entity{entityId}/RSData{namespace}/<Field name=..>/<typed value>`. The Python and C# readers agree with this reader on
  all entities of every game level (see Tests).
- **Terrain**: spacing `1 << GridResolution.EnumValue`, `x = col*spacing - half`, `z = half - row*spacing`, `y = raw*2048`, coarse
  bounds = min/max of raw*2048 over 17x17 shared-edge blocks (the 128x128 table of the live terrain object, byte for byte).
  Neutral deformation sample 128/255 (`0x3F008081`), gain 2.0 (live-verified on Alpine only, assumed elsewhere).
- **Collision** entity matrix: each rotation row scaled by `f32(1/sqrt((x*x+y*y)+z*z))`; chunk position = center * matrix with the
  float32 operation order `((x*m0 + y*m4) + z*m8) + t` (bit exact on the 10 Alpine entities per `_re/notes/alpine-terrain-world-live.md`).
  The orientation quaternion is the original routine 0x401210 (the core ports it); `fmt_collis_quat_approx` is a double Shepperd
  conversion, one ulp off, only for tools.

## New findings while writing the readers

1. **`.collis` has more kinds than 22 and 15** (survey of the level databases + MXs / ATVs / EngineDependencies / RiderPhysics, 236
   files): kind 22 triangles (86), 15 primitive shapes (77: trees, signs, gates, bales, the named vehicle part files), 2 (41:
   `Editor_Default` and the vehicle bodies `MX_Collision*`; the word at 0x14 is a float 1.0, not a count), 11 (2, 36 bytes), 16 (1,
   24 bytes). Kinds 2 / 11 / 16 are kept raw.
2. **Kind 15 layout**: `u32 pool size` at 0x18, the name pool, then `count` records `{u32 type, f32 matrix[16], i32 name offset (-1
   none), params}`: type 7 one f32 (76 bytes), type 8 two f32 (80), type 5 three f32 + a u32 (88). The named vehicle files
   (`Ron_BikeCollisionExperiment`: Tail, Handles, FrontTire, RearTire, FrontFender, Body) make 5 = box (half extents + flags) and 8 =
   wheel / cylinder (radius, half width) probable; 7 (trees) is a one-radius shape. NOT verified against the original's shape builder.
   `_re/notes/alpine-terrain-world-live.md` only said "kind 15 not recovered".
3. **Friction zones (`packdata`)** are a BXML with `Interleaved` and four 1024x1024 (Alpine / Concord; 512x512 Kingston) maps
   `Width/Height/NumBits/NumWords/Data<1..4>` of 3 / 2 / 3 / 3 bit codes packed LSB-first into u32 words (+1 spare word). Alpine's map 1
   holds the values {2, 1, 4}, map 3 {0, 6, 3, 4, 1, 7}: 93% / 90% of horizontal neighbours are equal, so the bit order is right, but
   what the codes mean (surface types / friction) and `Interleaved` is open (`fmt_zones_sample` documents the assumption).
4. **Model node record**: the nine floats after `name offset, parent` are exposed as `extra[9]` (e.g. `1.68 0 0 0 1 .97 .97 .97 1`); their
   meaning is unknown.
5. **`decode_block_texture` (level_export.py) has a DXT5 8-alpha bug**: it interpolates `((6-i)*a0 + i*a1)//7` for table entries 2..7, the
   standard (and Rfx.DecodeBlocks) is `((7-i)*a0 + i*a1)/7`. The core uses the standard one (self-checked in `fmt_test.c`); the
   Python previews of DXT5 textures are slightly off. Not compared against it.
6. **Install facts**: `SX_FresnoB`'s package is cut short (218 of 675 assets lie past its end; the level cannot be loaded from this
   install); the eight `Ctrl_*` levels are tool levels (empty or big-endian scene section headers, not game levels);
   `SX_*` / `NAT_*` base subscene = package name `<level>Base`.
7. Cube-like reflection maps (`NAT_CO_ReflectionMap`, A16B16G16R16F) carry other values in header words 4..9 and 6 faces, each with its
   own mip table: `fmt_texture` handles them (`faces`, `face_stride`).

## Tests (`core/tools/fmt_test.sh`)

`core/tools/fmt_test.sh [scratch dir] [archive]` builds the readers with the product flags, runs `fmt_test.py` (the Python parsers
of `_re/formats`: bxml, scene_read, package_extract + the libmspack bridge, texture_decode, bike_asset_export, alpine_collision_objects,
level_export, terrain_deform_reference, ui_export) and `fmt_test` (C), and diffs the two text dumps. `FMT_DUMP=1` also writes the full
per-document dumps for diffing a failing hash. Result at the time of writing: **identical, 9715 lines**, covering for FR_Alpine,
NAT_Concord, SX_Kingston (plus MXTables_Default, EngineDependencies, RiderPhysics, MXs, ATVs, MXUI):

| section | compared |
| --- | --- |
| `vfs` | file list + sizes of the whole install (619 files) |
| `db`, `pkg`, `assets` | every package / asset record of 6 databases |
| `a` (5254 lines) | the decompressed bytes (size + SHA-256) of **every asset** of those 6 databases: LZX / XMem / inflate exact |
| `bxml` (929) | every root `.bxml`, the three `.level` / `.database`, every material asset: node count, value counts and the SHA-256 of the full typed tree dump (all value types) |
| `resolve` | `/Root/Child[n]/@attr` paths |
| `scene`, `sub`, `anchor` | the container table, every entity of every subscene of the 3 levels (full RSData trees, hashed), free-ride anchors via the entity API |
| `table`, `mtable`, `dlc` | all 92 default tables cell by cell, and the merge with the DLC databases (rows + origins) |
| `loc` (125) | every `localiz` table of MXUI (keys, UTF-8 values, EMB_ strings, capacities) |
| `tparams`, `height`, `coarse`, `soft`, `tmat`, `zones` | terrain entity fields, the R32F height map bytes, the 128x128 / 64x64 coarse bounds (byte identical to the Python and, for Alpine, to the live capture), the softness mask RGB8, the terrain material, the zone words and samples |
| `collis`, `place`, `chunk` | header + content summary of all 236 collis assets, the CollisionRSData placements (scene matrix, entity matrix, chunk centers, triangle hashes, transformed positions: float32 bit patterns) |
| `model`, `surf`, `tex` | 173 models (nodes / matrices incl. world, LODs, bindings), 1383 surfaces (positions, repaired normals, UVs, triangles), 120 texture headers and the RGBA8 of the DXT1 / A8R8G8B8 ones |

Floats are compared as bit patterns. Besides the diff the tool has self checks: inflate stored / fixed / dynamic vectors (Python zlib),
corrupted zlib, vfs case / separator / parent-folder / bounds, table helpers, the DXT5 alpha table, `fmt_f16`, a survey of **every level
of the install** (49 game levels: scene, all subscenes, terrain, masks incl. 2 DXT5 masks, zones, material, collision placements: 128,619
entities, 8,190 placements, 414,817 triangles), and a mutation fuzz of every parser (`FMT_FUZZ=40` for a long run). The whole thing was also
run under ASan + UBSan (`clang -fsanitize=address,undefined`, also with `FMT_FUZZ=40`): clean.

One-off (not in the script): all **124,230 assets of all databases of the install (14.5 GB decompressed)** decoded by the C reader and by the Python +
libmspack path: same sizes and Adler-32 for every asset, except `PublisherVideo.lyr` (5 MXUI copies, container longer than the declared
region; also one SX_FresnoB shader: Python rejects, C and C# accept) and the truncated `SX_FresnoB` package (both fail).

Speed (arm64 -O2): LZX ≈ 250 MB/s; opening FR_Alpine's terrain (22 MB height map) ≈ 0.1 s; the 3-level dump takes ≈ 5 s in C, ≈ 25 s in Python.

## Not done / not verified

- Windows (`fmt_vfs_open_w`, the `_WIN32` branches of `fmt_vfs.c` / `fmt_sys.h`) is written but was not compiled (no toolchain here).
- DXT3 / A16B16G16R16F / G16R16F / R32F-cube decoding to RGBA8 (only headers and sizes); rendering textures stay in Unity's `ReflexTex`.
- `.collis` kinds 2 / 11 / 16 and the exact meaning of kind 15 types; friction-zone semantics; model `extra[9]`.
- Skinned surface layouts other than the observed ones are accepted when declared (as `ParseSurfaces` in C#); the Python parser is stricter.
- `anim`, `sound`, `shader`, `forest`, `tdf`, `uicmpnt`, `script` assets are returned as bytes by `fmt_db_read` and not interpreted here
  (`script` = u32 length + UTF-8; `forest` / `tdf` have their own notes in `_re/notes/forest.md` and the C# readers).
