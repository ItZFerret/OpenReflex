# MX vs ATV Untamed converter in C#: the contract for stage ports

The Python reference converter (`untamed/tools/*.py`, design in its `docs/DESIGN.md`) converts an Untamed stadium into an OPEN REFLEX
custom track in stages that talk through files in `work/<Level>/`. The C# port keeps that shape: one class per stage, the same manifests and
binary files in a work store, each stage compared with the Python output byte for byte. This file is what a stage port builds on (the plan
is `UNTAMED_TRACKS.md`). Everything below is in `Unity/Reflex/Assets/Scripts/Content/UntamedImport/` (namespace
`Reflex.Port.Content.UntamedImport`), is Unity-free ("System only", C# 9, .NET Standard 2.1) and is tested headless by
`tests/untamed_convert/check`: every file it writes for the 14 stadiums and 4 freestyle arenas against SHA-256 hashes in the repo
("Tests" at the end).

## Rules for a stage port

- New files `Ux<Stage>*.cs` in the folder above, each with a `.meta` (`fileFormatVersion: 2` / `guid: <32 hex>`). No UnityEngine, no
  unsafe, no native code, nothing newer than .NET Standard 2.1 (no `Math.FusedMultiplyAdd`, `MathF` is fine, no `System.Text.Json`,
  `Half`, `BitOperations`, `Convert.ToHexString`). Do not edit the foundation files or the Alive importer; report a foundation bug.
- A stage is `static void Convert(UxContext ctx, <options>)`: it reads `ctx.disc` (the Untamed level), `ctx.stock` / `ctx.library` (stock
  Reflex content), `ctx.work` (upstream manifests) and writes into `ctx.work` the paths the Python stage writes below `work/<Level>/`.
  Failures throw `UxException`. Logs go to `ctx.log`. No time stamps, absolute paths or machine names in outputs unless Python writes
  them (then say so in your report: those fields cannot match).
- Parity is byte for byte with the current Python code. `work/<Level>/` may be older than the tools: when a file differs, check what the
  current Python tool writes before changing the port. Never loosen a comparison; list every deviation with numbers.
- No game bytes in the repo: names, sizes, hashes and small constants only.

## Build and run a part

`tests/untamed_convert/build.sh <scratch> <part>` compiles (`-langversion:9`, no unsafe code: what compiles here compiles in Unity 6)
`foundation.txt` + `<part>/sources.txt` + `UxTest.cs` + `<part>/Program.cs` into `<scratch>/bin/<name>.dll` with its runtimeconfig.
The part in the repo is `check` (the converter end to end, "Tests" at the end); a local, git-ignored `reference/` folder may hold
more parts (`reference/<part>`, which also get `reference/UxTestReference.cs`). Run with `"C:/Program Files/dotnet/dotnet.exe"
<scratch>/bin/<name>.dll ...` (Git Bash; paths hold spaces). Scratch: a folder outside the repo; never write test output into the repo.

`UxTest` (namespace `...UntamedImport.Tests`):

| Member | Use |
|---|---|
| `OpenEnv(args)` -> `Env` | opens `--folder`/`UX_DISC_FOLDER` or `--iso`/`UX_DISC_ISO`, `--reflex`/`UX_REFLEX` or `--client`/`UX_CLIENT_LIB`, `--scratch` |
| `Env.Level(lv, workName)` | a `UxContext` whose work store is `<scratch>/<workName>/<Level>` (emptied first) |
| `SameBytes`, `SameMd5`, `Difference`, `ReadMd5List` | byte comparisons (the first differing byte), hash lists |
| `Check`, `Report`, `Fail`, `Skip`, `Section`, `Result(name)` | counting; `Result` prints `RESULT <name>: PASS|FAIL|INCOMPLETE` and gives the exit code |

## The foundation

**Work store and JSON** (`UxCommon.cs`, `UxJson.cs`)

- `UxWork(root)` / `UxWork.ForLevel(workRoot, lv)`: `PathOf`, `Exists`, `ReadBytes`, `ReadText`, `ReadJson` / `ReadJsonObj` /
  `ReadJsonOrNull`, `WriteBytes` (atomic), `WriteText`, `WriteJson(rel, obj, indent = 1)` = `ux_common.write_json`,
  `WriteJsonPlain` = `json.dumps(indent=1)` to a file, `List(dir, pattern, recursive)`, `Sub(rel)`, `Delete`. Paths are `/`-separated,
  relative, and cannot leave the store.
- `UxJson.DumpsUx(obj, indent)` = `ux_common.dumps_json` (sorted keys, `ensure_ascii=False`, NaN refused, final newline);
  `UxJson.Dumps(obj, indent, sortKeys, ensureAscii, allowNan)` = `json.dumps` (`indent < 0` = None); `UxJson.Parse` / `ParseUtf8`;
  `ParseDouble` (exact). Values: `JObj` (insertion-ordered dict; the Alive importer's), `List<object>` or any `IEnumerable` / array
  (multi-dimensional arrays nest), `string`, `long` / `int` / ..., `double`, `float` (written as `float(np.float32)`), `bool`, `null`,
  `byte[]` (hex string, as `_json_default`), dictionaries with integer keys. Parsed numbers are `long` or `double` (the int / float
  distinction of Python is kept). `JObj.Set` scans the keys (quadratic on large dicts): append to `items` when keys are known new.
- Access helpers: `UxJson.Obj`, `Arr`, `Str`, `Int`, `Num`, `Bool`, `Get(v, "a", "b")`.

**ux_common** (`UxCommon.cs`, `UxCore.cs`)

- `UxCommon.Levels` (`UxLevelRow`: disc, shortName, mode, tdf, display, freeride, location, condition), `Level(name)`, `YLift`, `Y_LIFT`,
  `LevelYLift`, `HEIGHT_SCALE`, `HF_SIZE`, `Limits` / `LimitsDict()`, `SlotName`, `SlotPrefix`, `SlotFileName`, `DiscFileName`,
  `ChampionshipSubscene`, `NS_PREFIX`, `RSDATA_CORE_TYPES`, `CanonNamespace`, `CanonNamespaces(elems)`, `RsdataType(entity)`,
  `PyLower`, `Logger`.
- `UxBin`: `U16/U32/I32/F32/U64(b, p, big)`, `U32s`, `F32s`, `Put*`, `Slice`, `PySlice` (clamping), `Concat`, `Same`, `CStr`,
  `Latin1`, `Hex`, `FromHex`, `Md5`, `Sha256`, `Md5File`, `Crc32`, `Require`. `UxWriter`: `bytearray += struct.pack(...)`.
- `UxNp` (numpy / Python number rules): `F32`, `Finite`, `HalfToFloat`, `DoubleToHalf`, `FloatToHalf` (`astype('<f2')`), `Round`
  (ties to even), `Round(x, decimals)` (`np.round`), `PyRound(x, n)` (Python `round`), `NanToZero`, `Clip`, `FloorDiv`, `Mod`,
  `StableArgsort` / `StableSort`, `ArgQuickSort`, `NaN` / `NaNf`, `Sum` (pairwise), `Fma`, `Dot`, `Norm`, `MatMul`, `MatVec3`, `VecMat`.

**S0, the disc** (`UxDisc.cs`): `UxDisc.Open(path)` (image or extracted folder; `Level(lv)`, `Has(lv)`). `UxDiscLevel` = `uxpkg.UxLevel`:
`assets` (`UxDiscAsset`: name, type, package, offsets, `file` = the path below `extract/<Level>`), `packages`, `handles`, `Get` /
`TryGet(name, type, package)`, `Find(type, package)`, `Read(asset)` / `Read(name, type)` (logical bytes), `ReadPadded`, `Blob`,
`ReadExtractFile(file)` / `HasExtractFile` / `AssetOfFile` (the `'file'` strings of `index.json` and the stage manifests, either
separator, case ignored), `ReadExtract(name, type)` (`U.extract_asset`), `ReadFile("database"|"level"|"scene")`, `ReadTdf()`
(`AI/<level>-<mode>.tdf07`), `ExtractIndex(sizes)`, `TypeCounts()`. Reads go to the package by range (nothing but the database is held),
are thread safe, and decoded assets are cached up to `CacheBytes` (96 MB; `ClearCache()`). Do not modify returned arrays. `UxLzx`:
`XmemDecompress`, `DecodeBlob`, `Decode`.

**BXML and scenes** (`UxBxml.cs`, `UxScene.cs`, `UxZlib.cs`): `UxBxml` = `bxml.Bxml` (`Find`, `FindAll`, `Children`, `Attr`,
`AttrStr`, `AttrInt`, `AttrItems`, `AttrValue`, `AttrIndex`, `SetPoolAttr`, `NodeValue`, `Name`, `ReplaceStrings`, `ToBytes`, `ToXml`,
`ToTree`, `EncodeTree`, `Pack`, `StoredSize`, `FlagsWalk`, `ApplyFlags`; a missing node is -1, a missing attribute null), `UxElem` /
`UxAttr` = `bxml.Elem` (`Find`, `FindAll`, `Attr`, `AttrObj`, `Iter`, `DeepCopy`). `UxPcScene` = `scene.py` (`Load`, `Save`, `Ns`,
`Prop`, `GetMatrix`, `SetMatrix`, `RawChunks`, `EntityId`; `UxSubScene`). `UxXScene` = `uxscene.py` (`Load`, `MergedEntities`,
`Inventory`, `ValueOf`, `Jsonable`; `UxEntity` with `props`, `prop_types`, `Assets()`, `ToDict()`, `Matrix`, `Position`).
`UxZlib.Compress` is `zlib.compress` (level 6) byte for byte, so every BXML written through `ToBytes` / `EncodeTree` / `Save` has
Python's bytes; `stored: true` writes stored deflate instead (not Python's bytes).

**Readers** (function names follow Python's)

| Python | C# | Notes |
|---|---|---|
| `uxmesh.py` | `UxMesh`: `ReadSurface` -> `UxGroup`, `StripToTris`, `ReadModel`, `ReadModelRigid`, `RigidStaticProblems`, `ModelToReflex`, `ReadCollis` -> `UxCollis`, `SwapCollisKind2`, `CollisToReflexPrimitive`, `CollisTriangles`, `BoxTriangles`, `UnitNormals`, `ReflexVertices`, `ReflexCrowdVertices`, `XDECL`, `PrimSize`, `CROWD_DECL_PC` | arrays are flat row-major: `float[]` (float32), `double[]` (float64), `int[]` (indices); `ReflexVertices` returns the structured array's bytes (44 per vertex; `VTX_*` offsets) |
| `rmesh.py` | `UxRmesh`: `Stripify`, `StripToTris`, `Canon`, `ReadSurface` / `WriteSurface` (`UxSurfaceGroup`), `MakeVertices`, `ReadModel` / `WriteModel` (`UxPcModel`, `raw_*` = the `raw` dict), `Bounds`, `ReadCollis` / `WriteCollis`, `ClusterTriangles` (`Cluster`), `DECL`, `DECL_BLOCK` | `Bounds` is the float64 form |
| `uxtex.py` | `UxTexture` (`LevelBlocks`, `Rgba`, `FloatLevel`, `PcLevelBytes`, `DefaultPcTarget`, `ToPc`, `CheckSize`, `Info`, `level_loc`), `UxPcTexture`, `UxTex` (`BuildPc`, `PcPitch`, `DecodeBlocks`, `Xfmt`, `PC_*`, tiling functions, `Header`) | images are `UxImage` (h, w, c) / `UxBlocks` |
| `rtex.py` | `UxRtex` (= `Tex`), `UxTex.RtexBuild`, `RtexHdrSize`, `RtexPitch`, `RtexMakeR32F` (point sampled only), `UxPil.BcEncode` (= `bc_encode`) | |
| Pillow | `UxPil.Resize(img, w, h, Box|Bilinear|Lanczos)` (L / RGB / RGBA; RGBA premultiplied as `Image.resize` does), `UxPil.BcEncode(img, dxt5)` | NEAREST and drawing (previews) are not ported |
| `uxmat.py` | `UxMat`: `ReadMaterial(data, big)` -> `UxMaterial`, `BlobPairs`, `XenonStatesToPc`, `XenonSamplerToPc`, `NamedStates`, `TextureRole`, `GuessRoles`, `BlendClass`, `SuggestReflexTemplate`, `Xrs`, `Xss` | |
| `d3d9sh.py` | `UxD3d9sh`: `ReadContainer`, `Ctab`, `Decls`, `Summarize` | |
| `uxterrain.py` | `UxTerrain`: `ReadTerrain` -> `UxTerrainMesh`, `ReadTrncol` -> `UxTrnCol`, `TrncolSwap`, `TerrainToPc`, `StripTris`, `Dec3n`, `Rasterize` -> `UxRaster`, `CollisionMaterials`, `Heightfield` | |
| `scipy.ndimage` | `UxNd.DistanceTransformEdt` (indices, scipy's choice among equally near elements), `DistanceTransformEdtDistances`, `BinaryDilation` (3 x 3 ones or the cross) | `gaussian_filter`, `cKDTree`, `ConvexHull` are not ported; `binary_closing` (the cross, 3 iterations, border 0: dilations then erosions) is in `UxDirtLook.CorridorMask` |
| `pkg.py` | `UxPkg` (`DecodeBlob`, `Decode`, `DecodeHead`, `EncodeBlob`), `UxTrackSet` (`Get`, `TryGet`, `Has`, `Blob`, `Read`, `ReadLogical`, `ReadHead`, `Rebuild`, `RebuildWithAdditions`), `UxPcAsset` | see "Writing packages" |

**Stock Reflex content** (`UxPcPkg.cs`, `UxRxar.cs`, `UxStock.cs`)

- `IUxLibrary` (the player's Reflex content): `HasLevel`, `ReadLevelFile(level, "database"|"level"|"scene")`, `ReadRootFile`,
  `OpenLevel(level)` -> `UxTrackSet`, `Levels()`. `UxDirectoryLibrary(folder)` reads an original install (`Database/*.dx9.package`) and
  the Open Reflex client's "game files" (extracted assets in `Assets.rxdata` / `Tracks/*.rxtrack`; `UxVfs`, `UxRxar`, `UxZstd` are the
  Unity-free readers of that format). Use it for everything Python reads from `U.GAME_DB`: `library.OpenLevel("SX_Kingston")
  .ReadLogical(name, type)`.
- `UxStock(library)`: `Has` / `Get(name, type)` / `Record` / `Prototype(rsdata)` = `ux_stocklib` (`templates/stock-lib`);
  `LibShaderBytes(T)`, `LibMaterialBytes(T)`, `LibPrototype(rsdata)`, `LibTemplate(T)` (`UxLibTemplate`: the `index.json` facts
  `vs_entry`, `ps_entry`, `num_textures`, `textures`, `user_samplers_register_order`, `samplers_register_order`, `shaders_form`, the
  md5s) = `ux_common.lib_*` (`templates/reflex-lib`); `BaseFile("database"|"level"|"scene")`, `BaseSet()`, `StockPin` =
  `templates/stock-SX_FortDodge` (`ux_base.STOCK_PINS`). Every result is checked against the reference's md5 pins.
  `templateNames["sx"|"fr"]` (`UxTemplateNames`: `Of(type)`, `Find(type, name)`) = the `template_*_names()` tables of the stages; a
  `UxStock` carries the constant tables `UxTemplateNames.Pinned(kind)` (see "Template names"). `BASE_GAPS`, `WATER_KIT`, `FD_WRAPS` =
  `ux_stocklib`'s.

## Writing packages (for the builder)

The builder writes resources with `UxLzxEncoder` (real LZX chunks; see "The track builder"). The foundation's older form is still there:
`UxPkg.EncodeBlob(data)` writes a resource as STORED 64 KiB chunks (packed size = allocation, the last chunk zero padded, the whole size
after the container: the form `pkg.encode_blob` writes for an incompressible chunk). The C core reads it: `core/src/fmt_lzx.c`
`fmt_xmem_resource` ("a stored chunk"; `tests/untamed_convert/check/core_lzx_test.sh` compiles that file and decodes stored and LZX
blobs with it); the client's reader (`RfxData.Lzx`, copied Unity-free as
`UxPkg.RfxLzx`) does too. It pads every resource to a multiple of 64 KiB (SX_FortDodge: 45.6 MB packed -> 269 MB), so prefer an asset
WITHOUT a `Compress` element: its heap region is the raw bytes (`RebuildWithAdditions(..., compressed: false)`; core `fmt_db.c`
`if (!a->lzx)`, client `RfxData` the same). In an extracted library (the client's game files) the stored blobs of the base do not
exist; `UxTrackSet.Blob` then re-encodes (`Reencode = true` forces that for an install too, so both libraries give the same bytes).

## Assembly parts (the libraries `ux_build_track` calls)

Ported for the stock-base sx / ec builds of the 12 tracks; byte for byte Python's on all 12. Entity lists are `List<KeyValuePair<string, UxElem>>` = Python's `[(subscene, Entity Elem)]` (what
`UxEntities.Convert(write: false)` / `LoadElems` give); asset maps are `UxAssetMap` = `{(name, type): (bytes, source, origin)}` in
insertion order (= the order they enter the package); ordered dicts are `UxOrdered<K, V>`. The path TEXT Python records (`Path.as_posix()`
of its own folders) comes from `UxPaths`: `root = <the reference's folder>` reproduces the reference's strings, the default `""` writes
them relative to that root (`templates/stock-lib/assets/X`, `work/<Level>/...`, `extract/<Level>/...`). No file is read through them.

| Python | C# entry point |
|---|---|
| `ux_base.resolve('stock', level)` / `Base` | `UxBase.Resolve(level = "SX_FortDodge")` -> `UxBase.Descriptor` (`stem`, `level`, `roles`, `policy`, `Record(paths)`) |
| `ux_base.keep_entity` | `UxBase.KeepEntity(policy, suffix, entity)` -> `(keep, rule)` |
| `ux_base.prepare` / `load_prepared` | `UxBase.Prepare(base, subs, levelBxml, stock)` / `UxBase.LoadPrepared(base, stock)` -> `Prepared` (`ts`, `subs`, `level`, `record`; `Sub("Base")`) |
| `ux_base.hf_roles` | `UxBase.HfRoles(subs, stem, ts)` -> `HfRoleSet` (`Role("Custom_DISP")`, `heightmap`, `material`, `softness`, `zones`, `textures`, `Record()`) |
| `ux_base.intro_track_part` / `script_kinds` / `script_renames` | `UxBase.IntroTrackPart(slot)` / `ScriptKinds(base, which)` / `ScriptRenames(base, ts, slot, kinds)` |
| `ux_base.CORE_LITERALS`, `literal_rows`, `literal_roots`, `core_global_assets`, `LOAD_ORDER_DEPS`, `CORE_FIELDS`, `CORE_GLOBAL_DBS`, `ENGINE_ANIM_SETS`, `FOLLOW` | `UxBase.CORE_LITERALS`, `LiteralRows(types)`, `LiteralRoots(types)`, `CoreGlobalAssets(library)`, `LOAD_ORDER_DEPS`, `CORE_FIELDS`, `CORE_GLOBAL_DBS`, `ENGINE_ANIM_SETS`, `FOLLOW` / `FOLLOW_TYPES` |
| `ux_base.anim_sets` / `session_anims` | `UxBase.AnimSets(library, paths)` -> `AnimSetTable` (`sets`, `vehicle`, `md5`, `file`) / `UxBase.SessionAnims(library)` |
| `ux_base.asset_strings` / `entity_strings` / `closure` | `UxBase.AssetStrings(data)` / `EntityStrings(entity)` / `Closure(entities, roots, index, read, globalKeys, sets, setRoots, sessionAnims)` -> `ClosureResult` (`kept`, `unresolved`, `glob`, `sets`, `nowhere`); `UxBase.IndexOf(keys)` makes the index |
| `ux_base.tex_bc` (= `ux_build_track.tex_bc`) / `constant_texture` | `UxBase.TexBc(image, dxt5)` / `UxBase.ConstantTexture(colour, w, h, dxt5)` |
| `ux_build_assets.entity_refs` / `rename_refs` | `UxBuildAssets.EntityRefs(elem)` / `RenameRefs(elem, renames)` |
| `ux_build_assets.TemplateAssets(ts)` | `new UxTemplateAssets(ts)`: `Has`, `Clash(name, type)`, `Logical(asset)`, `keys`, `lower` |
| `ux_build_assets.global_assets()` | `UxBuildAssets.GlobalAssets(library)` (read it once) |
| `ux_build_assets.Sources(lv, dirs)` | `new UxSources(lv, work, dirs, paths)`: `assets`, `Read(key)`, `problems`, `manifests`, `mesh_parts`, `terrain`, `collision`, `materials`, `textures`, `material_entries`, `dropped_materials`, `texture_renames`, `template_textures`, `PyPath(rel)` |
| `ux_build_assets.filter_dropped` | `UxBuildAssets.FilterDropped(items, dropped, readPart, log, fallbackMaterial)` -> `Filtered` (`items`, `new_assets`, `report`) |
| `ux_build_assets.TYPE_ORDER` | `UxBuildAssets.TYPE_ORDER`, `TypeOrder(type)` |
| `ux_tuff.swap` / `assets` | `UxTuff.Swap(items, lv, collision, wraps, log)` -> `(record, need)` / `UxTuff.Assets(stock, need, paths)` |
| `ux_tuff.sponsor_map`, `untamed_sponsor`, `material_name`, `generate_material`, `material_texture` | `UxTuff.SponsorMap`, `UntamedSponsor`, `MaterialName`, `GenerateMaterial(stock, mat, wrap)`, `MaterialTexture` |
| `ux_water.build` | `UxWater.Build(ctx, hf, n, cap, kitOrder, paths, log, look, shore, ground)` -> `Result` (`record`, `assets`, `elems`); `hf = null` reads `terrain/hf.npy`; `look` = `untamed` or `arena` (`UxWater.DEFAULT_LOOK`, the builder's `--ec-water-look` default); `shore` = `untamed` or `ground` (`DEFAULT_SHORE`, `--ec-water-shore`), `ground` = `UxWater.Ground` (the built ground's near albedo; needed for `ground` on a level with water: `UxWater.HasWater(work)`) |
| `ux_water.parse` / `water_pc` / `material` / `set_entity` / `shore_blend` | `UxWater.Parse`, `WaterPc`, `Material(stock, props, look, shoreRgb)`, `SetEntity`, `ShoreBlend(depth, look)`; `ARENA_*` = `ARENA` |
| `ux_water.open_colour` / `wet_ground` / `ground_shore`, `SHORES`, `SHORE_GROUND` | `UxWater.OpenColour(props)`, `WetGround(rgb, openRgb)`, `GroundShore(ground, xs, zs, wet, N, res, openRgb)` -> `(shore, record)`; `SHORES`, `SHORE_RING_FT` / `SHORE_WET_DARK` / `SHORE_WET_SAT` / `SHORE_OPEN_SEP` |
| `ux_dirtlook.stock_look` | `UxDirtLook.StockLook(work, tdir, tj, tdf, means, S, log, threads, cancel)` -> `Maps` (`mask` RGBA, `chromlum` RGBA, `topdown` RGB: 2048 x 2048 bytes; `rec`) |
| `ux_dirtlook.material_textures` / `layer_means` / `corridor_mask` / `client_index` / `on_client` / `composite` | `UxDirtLook.MaterialTextures(bytes)` -> `(shader, textures, parameters)`, `LayerMeans(textures)` (12 float64), `CorridorMask(tdf, S, n)`, `ClientIndex(nSrc, S)`, `OnClientRgb(image, S)`, `Composite(mask bytes, line, alpha, means, rgb)`; `PROFILE_*`, `MASK_SCALE` ...; `ExpExact` / `LogExact` / `PowExact` / `Hypot` (see "numpy rules") |
| `ux_build_track.tex_bc(img, fmt, straight_alpha)` | `UxBase.TexBc(image, dxt5, straightAlpha)` (`straightAlpha`: `_resize_straight`, colour and alpha resized apart: the dirt-look maps) |
| `ux_water.splash` / `particle_manager_assets` / `retarget_particles` / `tire_in_water` | `UxWater.Splash(stock, particle, partattr, texture, baseName, log)` -> `(record, assets)` / `ParticleManagerAssets(entity)` / `RetargetParticles(entity, name)` / `TireInWater(emitter)`; `PM_ROLES`, `SPLASH_*` |
| `ux_screens.build` | `UxScreens.Build(ctx, disc, materialEntries, gain, paths, log)` -> `Result` (`record`; `materials`: asset -> bytes, textures, template; `textures`) |
| `ux_screens.layout` / `h4` / `still_reference` / `light_reference` / `mip_chain` / `encode` / `light_recipe` | `UxScreens.Layout()`, `H4`, `StillReference`, `LightReference`, `MipChain`, `Encode`, `LightRecipe`, `SourceRgb` |
| `ux_ads.build` | `UxAds.Build(ctx, mode, matOpts, sources, templateAssets, lift, groupNumber, paths, log)` -> `Result` (`record`, `assets`, `elems`) |
| `ux_freestyle.ramps` / `widen_carriers` / `ramp_materials` / `carried_world` | `UxFreestyle.Ramps(items, ctx, sources, lift, log, cancel)` -> `(record, assets)` / `WidenCarriers(entities, record, readModel, readCollis, log)` -> assets / `RampMaterials(ctx)` / `CarriedWorld(ctx)` |
| `ux_splitgate.build` | `UxSplitGate.Build(ctx, matOpts, sources, templateAssets, lift, groupNumber, paths, log)` -> `Result` (`record`, `assets` = `_assets` + `_sources`, `dropgate_props`, `elems`); `UxSplitGate.UntamedGateRecords`, `OutDir(matOpts)` |

What the builder must know:

- `UxBase.LoadPrepared` loads a fresh scene on every call (the builder edits it). The prepared scene is `UxPcScene.Save(prepared.subs)`,
  the level file `prepared.level.ToBytes()`. `Prepared.ts` is the base's `UxTrackSet` (see "Writing packages"). The stock files come from
  the library through `UxStock.BaseFile` / `BaseSet` with the md5 pins (`ensure_stock_copy` is not needed).
- The closure as `base_prune` runs it: `index = UxBase.IndexOf(base keys, then the build's additions)`, `read(key)` = the addition's /
  replacement's bytes, else `templateAssets.Logical(asset)`; `globalKeys = UxBuildAssets.GlobalAssets(library).Keys`,
  `sets = UxBase.AnimSets(library).sets`, `sessionAnims = UxBase.SessionAnims(library)`; `kept` keeps Python's order (a queue, first
  found first). Over the base alone: 685 kept of 2,545; with a level's converted content 1,211 to 1,656.
- `UxSources` reads the stage manifests from the work store; `dirs` are work-store folders (`meshes`, `terrain`, `collision-t9`,
  `materials`, `textures`). A generated texture's `path` (`work/<Level>/materials/textures/X.texture`, relative to Python's root) is
  taken as the work-store file when it exists there, else `<terrain dir>/<path>`. Its problem texts carry posix paths (Python:
  `str(Path)`, backslashes on Windows).
- `UxWater.Build` returns the kit first (`Water_Wake`, `WaterPressure`, `Water`, `WaterFoam`), then per surface the water asset and its
  material: the order matters (the core loads a package in record order and the water loader needs `Water_Wake` loaded). Pass
  `cap = 1e9` for `--ec-water-depth untamed` and then set `record["cap_ft"] = "untamed"` as the builder does.
- The water-look round (reference of 2026-11-11, `ux_water.py` md5 fcbe332c of 10:00: the arena constants were re-tuned after the first
  port, `refl_mix` 0.5, `deep_min_lum` 0.22, `sky` 0.50 / 0.53 / 0.58, and fairview-v5 / lincoln-v5 rebuilt with them; DESIGN 23.9; the builder's defaults became
  `--ec-water-look arena` and `--ec-water-splash on`): pass `look: "arena"` (the default call of the fairview / lincoln builds
  from now on; `untamed` = the fairview-v4 / lincoln-v4 bytes). For the spray the builder's `water_splash` takes the base Base's
  one ParticleManagerRSData, `UxWater.ParticleManagerAssets` (one kit name for the three roles: `SX_Fort_Dodge_A`), the kit's three
  base assets (`templateAssets.Logical`), `UxWater.Splash` -> `UXW_<kit>` texture / particle / partattr (added as water assets),
  `UxWater.RetargetParticles(entity, record["assets"])`, and sets `record["splash"] = splash record + entity id`. The source
  kit `FR_Gibson_Particles` (3 assets) is in `UxStock` (`SPLASH_KIT`). `UxBxml` reads value type 9 (two floats: the particle
  documents) for this. Not ported: the look twin of the client's water shader (`read_cube`, `cube_sample`, `look_colour`,
  `look_metrics`, `material_params`, `look_problems`) and `particle_info`: the verifier's W10 - W12 gates.
- The dirt-look rounds (reference of 2026-11-11 14:06: `ux_dirtlook.py` 566533af, `ux_water.py` a51b3d57, `ux_build_track.py`
  161ea48c; DESIGN 23.10 / 23.11; the builder's defaults became `--dirt-look stock`, with it `--dirt-tint template` and
  `--hf-params base`, and `--ec-water-shore ground`). `UxDirtLook.StockLook` takes the terrain stage folder of the work store
  (`footprint.npy`, `detailmask.png`, `topdown_occ.png`), its `terrain.json`, the BUILD's tdf (the race stage's bytes) and the mean
  colours of the base HF material's four detail textures (`MaterialTextures` of the base's `Custom_HFTRN` material: samplers 1 - 4;
  `LayerMeans` of their logical bytes). The builder (`dirt_look_maps`) checks the sampler names against the roles, adds `material`,
  `tool_md5` (`UxDirtLook.TOOL_MD5`) and `maps_md5` to the record, and writes the three maps with `UxBase.TexBc(..., straightAlpha:
  true)` over the base's TopDown, DetailMasks and ChromLum; the four detail textures, the HF material and its parameters stay the
  base's. For the shore colour (`water_ground`) the ground is the TopDown map / 255 on the 2048 client grid (`--dirt-look
  untamed`: `UxDirtLook.OnClientRgb` of the terrain stage's TopDown picture), `S = 1023`; the maps are computed once (the water
  asks first, the terrain textures later). The reference's previews (`assembly/dirtlook_*.png`) are not written.
- `UxScreens.Build` needs the whole disc (`UxDisc`), not only the level being converted: the stills are made from the textures
  `Weststone_AD04` of WestStone_STADIUM (SX screens) and `endurocrosslogo` of Fairview_STADIUM (the EC arena). Nothing is stored in the
  repo: the layout plan is data in `UxScreens` (the fields `ux_screens` reads of the reference's pinned `screens_layout.json`, 19
  materials on 11 levels), the pictures are generated from the disc, the flat lights are constants. Pass
  `name => sources.material_entries.Get(name)`.
- `UxAds.Build` and `UxSplitGate.Build` take the build's S4 options (`UxMaterialsOptions`), a `UxSources` and the base's
  `UxTemplateAssets`. `UxSplitGate.Build` writes `splitgate/materials[-tag]/` into the work store (as Python writes
  `work/<Level>/splitgate/`); `record.tool_md5` is the md5 of the reference's `ux_splitgate.py` (`UxSplitGate.TOOL_MD5`). The builder
  then sets the base drop gate's props from `Result.dropgate_props` (the role name is `base.roles.dropgate`).
- Left for the builder (constants and steps of `ux_build_track.py` itself): `DIRT_TINT_CONST`, `HF_PARAMS_TODAY`,
  `SOFTNESS_CONST_RGB`, `CHROMLUM_CONST_RGBA`, `COLORREF_NEUTRAL`, `ZONES_FILL_DEFAULT`, the `_extra` / `add_extra` /
  `add_splitgate` clash rules, `base_prune`'s report, and `ux_scenecam`, `ux_package`. On the stock base nothing reads
  `U.TEMPLATE_DIR['fr']` (the dirt tint is the base material's own, or the constant with `--dirt-tint const`).
- Not ported of these modules: the DE sketch base of `ux_base` (`SKETCH_ROLES`, `resolve('sketch')`, `for_folder`), `detect_base`,
  `ensure_stock_copy`, the provenance helpers `package_md5s` / `lib_md5s` / `foreign_bytes` / `constant_recipe` (verifier side),
  `ux_stocklib --fetch / --check` (the library is read on demand, md5 pinned), the `__main__` inspection of `ux_splitgate`.
- `UxRace.cs` keeps its private subset of the `ux_base` policy (`UxRaceTemplate`); the test checks that it keeps the same named
  entities, poses and roles as `UxBase.LoadPrepared`. It can be built from `UxBase.LoadPrepared(...).subs` and `UxBase.HfRoles` now.

## The track builder (`UxBuildTrack.cs`, `UxPackage.cs`, `UxLzxEncoder.cs`)

The port of `ux_build_track.py` (the Builder of the current stock-base sx builds) joined with `ux_package.py`. One call converts one
stadium into the release-form folder OPEN REFLEX loads from `Mods/Tracks`: `Beta_Sx_Track_Slot_1.dx9.{database,level,package,scene}`
(a freestyle arena: `Beta_Fmx_Track_Slot_1`), `mod.ini`, `preview.png`, `sha256.txt`.

**Which reference.** Since the reference's water-glare round (2026-11-12, DESIGN 23.12, worklog `work/combined/water-glare.md`) the
port mirrors `ux_build_track.py` md5 15fa7970 (`UxBuildTrack.REFERENCE_TOOL_MD5`) with `ux_water.py` 1fc7f0da: fairview-v7 and
lincoln-v7. The two arenas' water reflects a copy of the stock base's level cube with the stadium lights capped at 2.0
(`UxWater.ReflectionCube`, asset `UXW_SX_FD_A_Reflection`; the base AmbientDirectionalLight's `_ReflectionMap` points at it), which
removes the white bloom blobs on the water; `ecWaterReflection = "level"` writes fairview-v6 / lincoln-v6, `"nolights"` in-paints
the lights. The ten stadiums without water are unchanged. Before that round it mirrored `ux_build_track.py` md5 161ea48c
(2026-11-11 13:41) with `ux_water.py` a51b3d57 and `ux_dirtlook.py` 566533af: the builder of the releases capitalcity-v5, columbus-v6, denton-v5,
harrispark-v6, foothills-v6, weststone-v5, jefferson-v5, wilford-v5, bridgeport-v5, fairview-v6, lincoln-v6, and (through its
predecessor 67ffbe75: the same ground, before the shore colour, which Norton has no water for) norton-v7. These are the reference's
two rounds after the water look (DESIGN 23.10 "dirt look" and 23.11 "dirt-look rollout and EC shore colour"; worklog
`work/combined/dirt-look.md`):

- `--dirt-look stock` (the sx default on the stock base): the base's own HF material (rut tint, detail tiling, bump scale: `--dirt-tint
  template`, `--hf-params base` become the defaults) and its four detail textures stay as stock bytes; DetailMask, ChromLum and TopDown
  are generated around our track from SX_FortDodge's profile (`UxDirtLook`), with straight-alpha mips.
- `--ec-water-shore ground` (arena look): the shore colour of each water surface is a wet version of that ground within 4 ft of it.

The older values are options and are still compared: `dirtLook = "untamed"` with `ecWaterShore = "untamed"` (`dirtTint` / `hfParams`
follow the look: `const` / `today`) writes norton-v6, capitalcity-v4, columbus-v5, denton-v4, harrispark-v5, foothills-v5,
weststone-v4, jefferson-v4, wilford-v4, bridgeport-v4, fairview-v5, lincoln-v5 (`run.sh <scratch> variants`). The per-track
arguments did not change (`work/combined/commands.json` lists all 12 under the new versions). `UxBuildTrack.PORT` (the stage stamps
and the import manifest carry it) became `UxBuildTrack/2` then, `/3` with the water-glare round and `/4` with the freestyle round,
whose merged reference builder is md5 1ef081f1 (`REFERENCE_TOOL_MD5`; see "Freestyle arenas" below).

```csharp
using (var disc = UxDisc.Open(discImageOrFolder))
using (var library = new UxDirectoryLibrary(reflexInstallOrGameFiles))
{
    var stock = new UxStock(library);                       // carries the pinned template name tables
    var shared = new UxBuildShared(stock);                  // once per session: the base's assets, global tables, race template
    var ctx = new UxContext { lv = lv, disc = disc.Level(lv), library = library, stock = stock, work = UxWork.ForLevel(stagingRoot, lv), cancel = token };
    UxBuildResult r = UxBuildTrack.Build(ctx, disc, UxTrackOptions.ForLevel(lv), outFolder, (fraction, text) => ..., token, shared);
}
```

`UxBuildTrack.Build(ctx, options, outFolder, progress, cancel)` is the same call with the whole disc taken from the level's own
volume and no shared state (a session that converts several tracks should pass one `UxBuildShared`: it saves reading the base again).

- `ctx.work` is the staging folder: the stages write there what the Python stages write below `work/<Level>/` (90 - 155 MB per level:
  `level.json`, `terrain/`, `meshes/`, `collision-t9/`, `materials/`, `textures/`, `splitgate/` on Foothills) and the builder adds
  `build_info.json` (the reference's record of the build: `assembly`, `rollout`, `assets`, `renames`, `options` ...) and
  `build_stages.json`. Nothing of it is needed once the folder is written; with `options.reuseStages` a stage whose stamp in
  `build_stages.json` matches (port version, disc database hash, the stage's options) is not run again.
- `disc` is the whole disc: the jumbotron stills are made from textures of WestStone_STADIUM and Fairview_STADIUM (`UxScreens`).
- `outFolder` is created; a slot set of another name in it is refused. Write into a staging folder and move it into `Mods/Tracks`
  when the call returns: a failed or cancelled build leaves a partial folder (the package is written first, as `.tmp`).
- `progress(fraction, text)` is called between steps (stages 0.02 - 0.56, entities, race line, parts, assets, writing 0.86,
  packaging 0.97); `cancel` is checked between steps and inside the stages and the encoder (`OperationCanceledException`).
- `UxBuildResult`: `folder`, `slot`, `files` (name, bytes, sha256 of the six files `sha256.txt` lists), `steps` (seconds per step),
  `seconds`, `packageBytes`, `logicalBytes`, `assets`, `warnings`, `info` (= `build_info.json`).
- The seven files depend on the disc, the library content and the options only: no time stamp, path or machine name; the same bytes
  from a disc image or an extracted disc, from an original install or the client's game files, with one thread or many.

**Options** (`UxTrackOptions`). `ForLevel(lv)` gives a track's current build: the row of `UxTrackOptions.Table`, which is the
argument list of the reference's build command (`work/combined/commands.json`, the 12 stadiums; `Parse(args)` reads such a list,
the test compares the two and the `options` record of every reference build). Every track: `--type9 accept`.

| Level | Per-track options (besides `--type9 accept`) |
|---|---|
| Norton | anchor moves `Anchor_30SecGirl_CameraHound10`, `Anchor_StartLine` +7.5, `Anchor_WinFinish` +20.5 |
| Capital City | `--bale-edge-clear 4`; anchor moves `Anchor_LoseWalk_PlayerCC11`, `Anchor_StartLine` +7.75, `Anchor_WinFinish` +19.25 |
| Columbus | `--bale-edge-clear 4`; anchor moves `Anchor_LoseWalk_PlayerCC4`, `Anchor_StartLine` +7.5, `Anchor_WinFinish` +20.5 |
| Denton | `--bale-trigger-move 0,413.31`, `--bale-edge-clear 4`, `--bale-reseat 6`; anchor moves `Anchor_StartLine` +3.25, `Anchor_LoseWalk` +6.25, `Anchor_WinFinish` +18.5 |
| Harris Park | anchor moves `Anchor_Intro` +31.25, `Anchor_StartLine` +7.5, `Anchor_WinFinish` +18.5 |
| Foothills | `--split-gate untamed`, `--startgate-visual hide`; anchor moves `Anchor_StartLine` +7.25, `Anchor_WinFinish` +21.0 |
| West Stone | anchor moves `Anchor_LoseWalk_PlayerCC8`, `_PlayerCC9`, `_PlayerFinished6`, `_PlayerFinished8`, `Anchor_Intro` +6.5, `Anchor_StartLine` +7.25, `Anchor_WinFinish` +20.5 |
| Jefferson | anchor moves `Anchor_LoseWalk_PlayerCC10`, `_PlayerFinished10`, `Anchor_StartLine` +7.5, `Anchor_LoseWalk` +10.25, `Anchor_WinFinish` +21.75 |
| Wilford | `--bale-edge-clear 4`; anchor moves `Anchor_StartLine_CardGirl`, `Anchor_StartLine` +7.5, `Anchor_LoseWalk` +7.25, `Anchor_WinFinish` +21.0 |
| Bridgeport | anchor moves `Anchor_LoseWalk_PlayerFinished8`, `_PlayerFinished9`, `Anchor_Intro` +49.5, `Anchor_StartLine` +7.25, `Anchor_WinFinish` +20.0 |
| Fairview | 11 anchor moves (`Anchor_30SecGirl_CameraHound07`, six `Anchor_LoseWalk_Player*`, `Anchor_StartLine_CardGirl`, `Anchor_StartLine` +7.75, `Anchor_LoseWalk` +7.0, `Anchor_WinFinish` +17.0) |
| Lincoln | anchor moves `Anchor_30SecGirl_CameraHound05`, `_CameraHound09`, `Anchor_StartLine` +3.75, `Anchor_LoseWalk` +1.75, `Anchor_WinFinish` +20.5 |
| Dyson | anchor moves `Anchor_Intro` +6.5, `Anchor_StartLine` +7.5, `Anchor_LoseWalk` +6.75, `Anchor_WinFinish` +19.25 |
| Edgewater | anchor moves `Anchor_LoseWalk_PlayerCC10`, `Anchor_StartLine` +7.25, `Anchor_LoseWalk` +11.25, `Anchor_WinFinish` +19.75 |
| RBW Center, Southbend, THQ Spectrum, West Lake | none (no anchors in the free-ride kit) |

**Freestyle arenas** (mode `fmx`, 2026-11-12). The port mirrors the reference's final arena round, the releases westlake-v3,
southbend-v3, thqspectrum-v3 and rbwcenter-v3 (built by its merged tools `tools-fmx2`, which also built norton-v7 and dyson-v2 byte for
byte). A level of mode `fmx` switches everything (`UxCommon.IsFmx`); there is no option of its own:

- the slot is `Beta_Fmx_Track_Slot_1` (`UxCommon.RaceSlotName`): OPEN REFLEX lists it under FREE RIDE only, and `mod.ini` says
  `kind = freeride`, its description "free ride: the arena's ramps and dirt jumps, no race";
- the stock base is reduced to its free-ride kit (`UxBase.Resolve(level, "sx", "freeride")`: Base 16 entities, the 12 free-ride
  starts, no SectionRace), and S8 (`UxRace`) does not run: no tdf, gate, 30 s board, lap pacing, scenario scripts or engine animation
  sets; the dirt look has no race corridor (Untamed's painted lanes only);
- the terrain stage leaves Untamed's collision-only ramp faces out of the heightfield (`terrain/carried_tris.npy`) and takes the
  `-fmx.tdf07` freestyle lines as the lap of its ruts test; `UxFreestyle.Ramps` gives each carried face to the smallest ramp
  collision box holding it (a new kind-22 collis `UXR_<short>_<Untamed id>`, DriveOn 1, MXBike, MetalFrame, no soft collision) and
  moves a ramp whose drawn deck sits off its riding faces onto them (THQ Spectrum: four ramps; the fit is scipy's Powell, ported step
  for step); `UxFreestyle.WidenCarriers` writes bounds-only model copies where the riding faces stick out of the steel;
- the materials stage draws the ramps' perforated grating (generated `tiled_alpha` texture), the side panels' navy and red (`tint`,
  from `UxShaderProg.DiffuseAlbedo`) and RBW Center's funbox frame (`diffuse_albedo` literal);
- a dynamic prop standing more than 100 ft over the arena is left out (Southbend's three signs parked above the roof); the jumbotron
  shows the Untamed logo still (overlay `screens-fmx/screens_layout_fmx.json`); the tuff blocks may wear four more stock wraps
  (`SP_Scott`, `SP_FactoryConnection`, `SP_Acerbis`, `SP_IMSRoll`).

Two changes of the reference's merged tools apply to every level and change no stadium's output: exact duplicate collision triangles
are dropped (`UxCollision`), and the terrain-part check accepts a split collision triangle that lies on a source triangle.

The other fields are the reference's defaults of the current builds (base stock SX_FortDodge, bales tuff, EC water on / arena look /
shore ground / splash on / cap 1.25 ft, screens stills at 0.9, MassiveAd panels keep, zones fill 2,0,0,0, dirt look stock with dirt
tint template and HF params base, base scripts stock, base layout neutral, lap pacing none ...). `dirtTint` and `hfParams` are
null by default and follow `dirtLook` as the reference's parser does (`stock`: `template` / `base`; `untamed`: `const` / `today`;
`DirtTint` / `HfParams` give the value used). `Validate()` refuses a value whose code path is not ported (the safe variant,
`--base sketch`, kind fr, `--zones remap|stock`, `--dirt-tint fr`). Port-only fields: `packing` (`lzx` default, `stored`, `raw`),
`threads`, `reuseStages`, `date` (the date in mod.ini's description: null = none, the file must not depend on the clock), `author`,
`paths` (the path text of the records), `check`, `collect` (GC.Collect() at the step boundaries, on by default: it holds the live
managed data below 285 MB between steps). Tested: the values of the 12 current builds, and the older values `dirt-look untamed`
(with `dirt-tint const`, `hf-params today`), `ec-water-shore untamed`, `ec-water-look untamed`, `ec-water-splash off` against the
older releases that used them (`run.sh <scratch> variants`: all 12 against norton-v6 ... lincoln-v5, the two arenas also against
fairview-v4 / lincoln-v4, equal in the same way). The other values `Validate()` lets through (`bales untamed`, `ec-water off`,
`screens untamed`, `massive-ads model | drop`, `zones template`, `base-scripts all | none`, `base-layout keep`, `base-cameraflash
stock`, `lappacing untamed`, `split-gate` / `startgate-visual` on another level, and the mixed forms `dirt-look stock` with
`dirt-tint const` or `hf-params today`, `dirt-look untamed` with `ec-water-shore ground`) follow the Python branch but were never
compared with a build (`ec-water-shore ground` on the Untamed ground is compared with the Python function in memory:
`assembly_parts`).

**Package resources: the decision.** The reference compresses every resource with Microsoft's XMemCompress. The core (`core/src/fmt_db.c`,
`fmt_lzx.c`) and the client (`RfxData`) read three forms: an asset without a `Compress` element (the heap region is the resource), an
LZX resource whose chunk is stored (packed size = allocation), and LZX chunks. The first two are easy and cost 1.69 GB for the 12
tracks (Norton 131 MB against the reference's 46.4 MB). So `UxLzxEncoder` writes real LZX: the XMem container (`u32 end`, per 64 KiB
chunk `u32 65536, u32 packed`, `u32 size`), per chunk one LZX stream (window 2^17) cut into frames of 32768 bytes (`u16be packed`; the
last frame `0xFF, u16be output, u16be packed`; five zero bytes, as XMemCompress ends a chunk), one VERBATIM block per frame (main and
length trees as pre-tree coded deltas, repeated offsets R0 - R2), a hash-chain match finder with one-step lazy matching, canonical
length-limited Huffman codes (always complete: the core's `tree_build` refuses an incomplete code). A chunk that does not shrink is
stored without padding. 314 lines, no state between resources, the output depends on the input bytes only. Result: 608 MB for the
12 tracks against the reference's 572 MB (+6.4 %), encoded in about 1 s per track. `packing = "stored"` (stored chunks, 131 MB for
Norton) and `"raw"` (no `Compress` element) remain as options and pass the same content comparison.

Proof that it loads (all on the bytes the builder writes): every resource of the 12 tracks (16,901) decodes to the reference's
bytes through (1) the client's decoder `LzxDecoder` (`UxLzx.Decode`), (2) the client's resource reader (`UxPkg.RfxLzx`, the copy of
`RfxData.Lzx`), (3) the C core's `fmt_xmem_resource` compiled from `core/src/fmt_lzx.c` (`build/core_lzx_test.sh`), (4) Microsoft's
codec (the reference verifier's V2.2: "all compressed blobs decode (XMem helper)"), and (5) the installed core `rxcore.dll` 1.1.0
loads and races every track (`ux_racetest.py`). 616 synthetic blobs (sizes around the frame and chunk limits, one repeated byte to
noise) pass (1) - (3).

**What is ported of the Builder**, in its order: the stages S1 - S6 (`Stages.ensure_all`), S7 and S8 in memory, `combined_entities`
(tuff, water with `water_ground` + `water_splash`, ads), `split_gate`, `assemble_scene` (strip, GridResolution, DistanceFog values,
the race kit moves, `anchor_moves`, `base_layout_scene`, the 12 free-ride starts, `filter_dropped`, `y_offset`), `collect_assets`
(`need`, the clash / `UX_` rename rules, `add_splitgate`, `add_extra`, `add_cameraflash`, `add_lappacing`, the reference renames,
`terrain_replacements` with `dirt_look_maps`, `hf_material_patch`, `zones_replacement` / `zones_patch`), `write` (chunks of 30,
`canon_namespaces`, `base_prune`, script renames, `rename_scene`, `rebuild_database` with removals and the slot rename), `build_info`
(records only; `rollout.dirt_look`), and of `ux_package`: `patch_doc`, `scene_rebuild`, the flags-only proofs, `openreflex_rules`,
mod.ini, preview, sha256.txt. Constants: `DIRT_TINT_CONST`, `HF_PARAMS_TODAY`, `SOFTNESS_CONST_RGB`, `CHROMLUM_CONST_RGBA`,
`COLORREF_NEUTRAL`, `ZONES_FILL_DEFAULT`, `G_DL`.

Not ported: kind fr, the DE sketch base and `--legacy-defaults`, `--safe`, `--no-content`, `--twice`, `--topdown`, the acceptance /
proto / slot-check helpers, `zones_patch` modes `remap` and `stock`, `dirt_tint_material`, `orphan_removals` beyond the check that
the base holds no candidate, `references_to`, stage freshness by tool md5 (`Stages.stale`: replaced by `build_stages.json`),
`assembly/*.json`, `preview_height.png` and the dirt-look previews (`assembly/dirtlook_*.png`), `ux_package`'s verify.log and
orcheck gates and its `--name` / `--kind` / `--keep-flags`.

Deviations from the reference's release folder:

- `.package`: other stored bytes (own LZX), so `.database` differs in `Heap` offset / size and `Package` offset / heapOffset values.
  Everything else of the database, the whole `.scene` and `.level`, and every resource's decoded bytes are equal.
- `mod.ini`: equal when `date` is the reference's packaging date, apart from this port's own author (`UxPackage.AUTHOR`) and
  description wording (`UxPackage.CONVERTED`, without the reference's closing "Unofficial fan conversion."); the description is
  "<track> from MX vs ATV Untamed (Xbox 360), converted from the player's own disc by the OPEN REFLEX Untamed import."
- `preview.png`: the track's top-down ground bake (`terrain/topdown_occ.png`: Untamed's own ground colours, also with `--dirt-look
  stock`) scaled to 512 x 512 with LANCZOS and written as Pillow writes a PNG. The reference scales `verify_oblique.png`, a labelled
  render of its verifier (Pillow's bitmap font), which is not ported.
- `build_info.json` is written into the work store, not next to the files; it has no `argv` / `command` / `tool_md5s`, its `options`
  are the switches of `UxTrackOptions`, `assembly.output` describes the release-form files, the stored-size counters of
  `assembly.base_prune` are those of the library's own package.

**Tests**: `tests/untamed_convert/check` ("Tests" at the end).

## Template names

The stages `ux_textures`, `ux_materials` and `ux_collision` (and `ux_ads` / `ux_splitgate` through them) ask the DE sketch slot ('sx')
and the HuCkiT free-ride slot ('fr') whether an Untamed name is taken: a taken name gets the `UX_` prefix, an `Editor_Default*`
name both hold is used as the template's. The current stock-base Python build still names its stage outputs by those two tables; the
stock base's own names are only checked later, by the builder, against the SX_FortDodge package (`TemplateAssets.clash`). So the
tables are NOT irrelevant on the stock base (they decide asset names), and names derived from SX_FortDodge would change outputs: of
the 17 'sx' names that matter, 5 are not SX_FortDodge assets (`Water.shader`, the textures `WaterFoam`, `WaterPressure`,
`Editor_Default_Gray`, `Editor_Default_BlackAlpha`). A Reflex library has neither slot, so `UxStock` carries the answer as constants:
`UxTemplateNames.Pinned("sx")` (17 names of the sketch's 962) and `Pinned("fr")` (8 of HuCkiT's 341): every name of the two
databases that an asset of the 12 supported levels, or a fixed name the stages ask for, meets (all stock Reflex names). With them
every lookup the stages and parts can make on the 12 levels gives the full tables' answer (61,748 lookups, 157 hits, 0 differ), and
the three stages write byte-identical folders on either table set (`assembly_parts/run.sh <scratch> stages`: 5,924 files). `level.json`
still records the template folder text (`template.dir`, an option of `UxLevel`). A level outside the 12 needs its own check: the two
Minimoto stadiums (Dyson, Edgewater; 2026-11-12) were built on the pinned tables and match the reference's releases dyson-v1 /
edgewater-v1 (scene byte for byte, database as a document, every package resource decoded), and so were the four freestyle arenas,
which match westlake-v3, southbend-v3, thqspectrum-v3 and rbwcenter-v3 the same way.

## numpy rules that decide bytes

- dtypes: `float32` arithmetic is C# `float`, `float64` is `double`; a Python float is a double. `np.round`, `round()` and `.astype(int)`
  differ: `UxNp.Round` (ties to even), `UxNp.PyRound` (decimal digits), truncation.
- `astype('<f2')`: `UxNp.DoubleToHalf` / `FloatToHalf`. NaN: numpy's is positive (`UxNp.NaN`, `UxNp.NaNf`); `double.NaN` / `float.NaN`
  have the sign bit set and change written bytes.
- Reductions: `np.sum` / `np.mean` along a contiguous axis (a 1-D array, the last axis of a C-ordered array) use the pairwise sum of
  numpy: `UxNp.Sum(a, start, n, step)` (fewer than 8 elements: plain left to right; `mean` = sum / n in the array's dtype). Along another
  axis (`axis=0` of a 2-D array, `tris.mean(1)` of an (n, 3, 3) array) numpy adds slice by slice, left to right: write that loop.
  `np.linalg.norm(a, axis=1)` is sqrt of the plain left-to-right sum of squares.
- BLAS: on the reference machine `A @ B` (2-D @ 2-D, also stacked), `np.dot` and `np.linalg.norm(vector)` round as fused multiply-add
  chains. Use `UxNp.MatMul` (any shape up to 30 x 30 measured), `UxNp.Dot` / `Norm` (fewer than 16 elements), `UxNp.MatVec3` ((m, 3) @
  vec3), `UxNp.VecMat` (vec @ 3 x 3 or 4 x 4). NOT modelled: `np.dot` of 16 and more elements, float32 `np.dot` / `norm` of 3 and more,
  (m, 4) @ vec4, vec4 @ (4, 3), `np.einsum`, `np.linalg.inv` / `solve` / `lstsq` / `det` (LAPACK). `np.cross` and element-wise
  expressions are plain IEEE and need nothing. Where an unmodelled operation feeds bytes, measure the deviation and report it.
- `np.interp(x, xp, fp)` with more samples than points: the slopes `(fp[j+1] - fp[j]) / (xp[j+1] - xp[j])` are computed once and the
  value is `slope * (x - xp[j]) + fp[j]` (two roundings, no fused multiply-add); `fp[j]` itself when `x == xp[j]`, the end values
  outside (`UxDirtLook.Interp`). `np.median` = the middle element, or `(a + b) / 2` of the two middle ones; `np.std` = sqrt of the
  pairwise mean of `(x - mean)^2` with a pairwise mean. `2 * a` of a uint8 array stays uint8 and wraps (the dirt-look record's
  `chromlum_factor_footprint` is computed that way; mirrored).
- The C library through numpy. `np.hypot` on the reference machine is `sqrt(x * x + y * y)` with every step rounded (all of 200,000
  pairs; the correctly rounded value differs by one unit in the last place on 16 % of them): `UxDirtLook.Hypot` is that expression.
  `np.exp`, `np.log` and `v ** k` are the C library's too, correctly rounded on 99.5 %, 99.7 % and 99.9 % of 200,000 inputs each and
  one unit off on the rest. A port cannot reproduce another C library's last bit, and `Math.Exp` / `Log` / `Pow` differ between
  runtimes, so `UxDirtLook.ExpExact` / `LogExact` / `PowExact` compute them in double-double arithmetic (`+ - * /` and `Math.Sqrt`
  only) and round once: the correctly rounded value on every platform (50,000 inputs each against decimal arithmetic). See "Known
  gaps" for what that can change.
- Sorting: `sorted`, `list.sort`, `np.argsort(kind='stable')`, `np.lexsort` are stable: `UxNp.StableSort` / `StableArgsort` (`List.Sort`
  and `Array.Sort` are not). `np.argsort` of a structured array: `UxNp.ArgQuickSort`. Default `np.sort` / `np.argsort` / `np.unique` of
  plain numbers are fine when only the sorted VALUES matter; when the order of equal elements matters there is no model.
- Dict order is insertion order (`JObj`, or a `List` of pairs); `set` iteration order of strings is not reproducible: sort as Python does.
- `str.lower()` on asset names: `UxCommon.PyLower`. Path sorting on the reference machine ignores case (`UxWork.List`).

## Known gaps

- `U.TEMPLATE_DIR` (the DE sketch and HuCkiT slots): resolved with constant name tables (see "Template names"); the stage tests
  still load the full tables from the reference's `templates/` (`OpenEnv`).
- `ux_build_track` and `ux_package` are the builder (`UxBuildTrack`, `UxPackage`; "The track builder" above). `ux_scenecam` is not
  ported: only the verifier (V8.22) and the reference's probes use it. `ux_base` (stock base), `ux_build_assets`,
  `ux_tuff`, `ux_water`, `ux_screens`, `ux_ads`, `ux_splitgate` are the assembly parts above; `ux_startpack` is the race stage's.
  `ux_shaderprog.py` (materials), `tdf07_to_reflex.py`, `lappacing_to_reflex.py` (race) and `ux_collis_to_reflex.py` (collision)
  belong to their stages.
- `templates/reflex-lib/index.json` analysis lists (`entries`, `ps_ctab`, `render_states`, `sampler_states`, ...) and `sm3dis.py` are
  not ported: `UxLibTemplate` holds the fields `ux_materials.lib()` reads; the UV trace (`template_uv`) is the materials stage's.
- Half-float textures' `UxTexture.Rgba` (display tone mapping) goes through `pow`: equal on the test machine (27 textures), not
  guaranteed on another C library. Everything else in the readers is integer or exactly rounded.
- The dirt-look maps and the C library (the only deviation in principle of the dirt-look rounds; no byte differs on the 12 tracks).
  The reference takes `exp` (the edge line), `log` (the spread of the macro brightness, summed over the 0.45 - 1.2 million texels
  more than 4 ft inside the track) and `pow` (the two arenas, whose exponent is below 1; the ten supercross stadiums clip it to 1)
  from its C library; the port computes the correctly rounded values (see "numpy rules"). Measured on the 12 tracks: of 10,451,385
  log values 41,156 (0.39 %) are not the C library's double (all one unit in the last place apart); the standard deviation summed
  from them, and so the exponent, is nevertheless the same double as numpy's on all 12; all 8,388,608 `v ** k` values of Fairview
  and Lincoln are the same doubles as numpy's. A differing last bit could reach a file only where a texel's value `x * 255 + 0.5`
  lies within about 1e-13 of an integer (the maps are bytes), or through a different exponent; neither happens on the 12 tracks: the
  three maps of every track have the md5 the reference build recorded (`rollout.dirt_look.maps_md5`). `pow` is taken from
  `Math.Pow` first and recomputed exactly where the texel's value is within 1e-6 of such a step (2 texels on Fairview, none on
  Lincoln), so the bytes do not depend on the runtime's `pow` either (the same maps with every value computed exactly).
- `UxPil` and the zlib port were checked against Pillow 13.0.0 and zlib 1.3.1; LANCZOS coefficients use `sin`. Pillow 13 runs the
  vertical resize pass first when the height shrinks and `h - oh > 2 * (w - ow)` (the intermediate image is 8-bit, so the order
  decides bytes); `UxPil.Resize` follows that rule (measured on 5,400 sizes, every filter and mode; `assembly_parts` checks 234
  cases). It applies to tall images (the mip chain of a texture more than twice as high as wide); square and wide ones are unchanged.
- Microsoft's XMemCompress is not reproduced: `UxLzxEncoder` writes its own LZX streams (decoded content equal, package about 6 %
  larger). Previews, contact sheets and logs of the Python stages are not ported.

## Tests

`sh tests/untamed_convert/check/run.sh <scratch> [lzx|half|build|variants|determinism|cancel|all] [--levels norton,lincoln]` with
`UX_DISC_ISO` (or `UX_DISC_FOLDER`) and `UX_REFLEX` (or `UX_CLIENT_LIB`): the player's disc and a Reflex library are all it needs.

- `build` (in `all`): every stadium from the disc through S1 - S8 and the builder; each file it writes is hashed and compared with
  `check/expected.txt`: the seven files of the track folder (`<short>/track/<file>`) and every file of its work store, the stage
  outputs and the builder's records (`<short>/work/<path>`; `build_info.json` without its step times). 126 + 13,133 hashes. The build's
  progress must rise to 1.
- `variants`: the option values kept besides the defaults (the older ground, shore, water and reflection looks; packing `stored` and
  `raw`, whose resources must also decode to the `lzx` build's bytes): 21 builds, their four `.dx9` files against the list.
- `determinism`: three runs of a level (all threads, again without shared state, one thread) write the same files.
- `cancel`: a build cancelled after 0, 1.5, 5 and 9 s ends with an `OperationCanceledException` within 3 s.
- `lzx`, `half` (in `all`, no disc needed): 616 synthetic blobs through `UxLzxEncoder` and back through the client's decoders; every
  float16 round trip.
- `core_lzx_test.sh <scratch>`: the C core's resource reader (`fmt_xmem_resource`) on the blobs `run.sh ... --core-blobs` wrote.

`expected.txt` holds hashes only, no game bytes. It was recorded on 2026-11-12 (Windows, .NET 8), when the output of this port was
confirmed equal to the releases of the conversion it was ported from (that comparison is not part of the repo), and it holds for
both disc forms and both library kinds: the disc image with an original install and the extracted disc with the client's game files
give the same 13,259 files. A change to the converter that changes its output must come with a new list (`run.sh <scratch> all
--write-expected <file>`, then `variants --write-expected <file>`), and `UntamedImporter`'s tested-build table must follow it
(`tests/untamed_import/import` checks that table against the list).
