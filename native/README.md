# Reflex native core

The native core contains the vehicle scheduler's **arithmetic and control
decisions** from `00430ef0`, the all-node **velocity update** from `0085d270`,
**position/orientation integration** from `0085d100`, and **resolved-force
composition** from `0085ca40`, plus the observed **four-block matrix subtraction**
leaf `00857b30` and the four other observed matrix leaves: `00853bd0`,
`008540a0`, `00857750` and `00857a00`, plus fixed-body and sliding-rotor
constraint callbacks `0085bb00`, `0085a7e0`, `0085c0d0` and `0085b5a0`,
the exact Matrix3x3 inverse helper `00859250`, Matrix3x3 product `00853740`,
ordered Matrix6x6 inverse `008597e0`, and the original 4×4 matrix inverse
wrapper `00859340`, plus the rotor's second paired preparation product
`00856b40`, and the complete bounded constraint interval `0085c700` through
`0085cf80` from one initial state, and the complete bounded matrix-preparation
stage `0085cb70`, and the complete bounded mechanism core from `0085c6d0`
through the `0085d270` exit. The isolated v24 candidate adds the complete bike
force stage `0043e100`, consuming supplied contacts and controls. Matrix dimensions here describe the captured dirt-bike
constraint math, not a four-wheeled vehicle implementation.
It builds as `libreflexcore`, with a small versioned C interface and a direct
C# binding in the Unity project. The library has no Unity runtime dependency.

This component computes the scheduled count and carried simulation lead. A
separate function computes interpolation weights from the post-callback state.
The velocity function computes derivative caches and advances linear/angular
velocity using supplied resolved forces and coefficients. The pose function
advances position and orientation, then normalizes every quaternion. The force
composer combines supplied gravity, accumulated forces, impulses and gyroscopic
correction. The matrix leaf subtracts four supplied 3×3 block products from two
existing vec4s. Four concrete callbacks build RHS vectors from captured matrices and forces,
and apply captured solved vectors back to body forces. The complete interval
composes both passes and their callbacks from one initial snapshot, preserving
original graph traversal and immediate shared-force updates. Whole preparation
computes all prepared matrices and persistent slot8/9 products from source joint
matrices, coefficients and runtime globals. The mechanism core now composes
node2 joint generation, preparation, forces, constraints and velocity from one
initial state. The isolated force-stage API additionally generates accumulated
forces from supplied contact/control inputs. These calls do not produce contacts,
or apply presentation interpolation. The existing CrossOver harness
remains the original-game reference.

## Build and check

From the repository root:

```sh
cmake -S native -B native/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build native/build
ctest --test-dir native/build --output-on-failure
```

The C++17 library uses no third-party dependencies. Python is needed for the
reference-model and fixture provenance tests; use `-DBUILD_TESTING=OFF` for a
library-only build. Build output stays under ignored `native/build*/` folders.

The macOS universal build contains both Apple Silicon and Intel slices:

```sh
cmake -S native -B native/build-universal -G Ninja -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build native/build-universal
arch -x86_64 native/build-universal/reflex_core_tests
arch -x86_64 native/build-universal/reflex_core_tests \
  --fixtures native/tests/fixtures/scheduler_live/vectors.tsv
```

Both architecture slices have passed the original capture comparisons and the
5,000-case exact-model comparison. The Intel slice ran under Rosetta on this
Apple Silicon machine. Windows export/compiler settings are included, but no
Windows build has been tested.

## Public interface

[`include/reflex_core.h`](include/reflex_core.h) is valid C and C++. The ABI is
version 1, with fixed-width integers, 32-bit floats, and four-byte struct
alignment. Bindings must check `reflex_core_abi_version()` and
`reflex_core_sizeof()` before calling. The public symbols are:

| Function | Behavior |
| --- | --- |
| `reflex_core_abi_version` | Returns ABI version 1. |
| `reflex_core_sizeof` | Reports the requested public struct size, or 0 for an unknown ID. |
| `reflex_scheduler_evaluate` | Computes count, lead, branch, cap flag and a conditional integration count. |
| `reflex_scheduler_interpolation` | Computes `current = round32(1 - round32(lead / h))`, then `previous = round32(1 - current)`. |
| `reflex_velocity_integrate` | Computes linear/angular derivative caches, then advances both velocities for every supplied node, with separate float32 operations in all four lanes. |
| `reflex_pose_integrate` | Advances every position and quaternion, then normalizes orientations using the original reduction, square-root and division order. |
| `reflex_force_compose` | Combines gravity and accumulated forces, subtracts the original gyroscopic correction, then adds separately rounded impulse/reciprocal-timestep products. |
| `reflex_matrix_subtract` | Subtracts the observed four-block matrix product from two existing destination vec4s, preserving dot reduction and W behavior. |
| `reflex_matrix4_multiply` | Overwrites a vec4 with four native four-lane dot products (`00853bd0`). |
| `reflex_matrix6_multiply` | Overwrites two vec4s with paired three-row block products (`008540a0`). |
| `reflex_matrix4x6_subtract` | Subtracts two four-row xyz block products from a vec4 (`00857750`). |
| `reflex_matrix6x4_subtract` | Subtracts six four-lane dots from two vec4s with the original first-dot W correction (`00857a00`). |
| `reflex_fixed_slot3` | Builds two RHS vec4s from four scaled forces, two captured matrices and bias (`0085bb00`). |
| `reflex_rotor_slot3` | Builds one RHS vec4 with four independent xyz-dot pair sums and bias (`0085a7e0`). |
| `reflex_fixed_slot5` | Applies transposed matrix products of the solved vector to four body forces in original write order (`0085c0d0`). |
| `reflex_rotor_slot5` | Applies transposed four-lane dots, with first-dot W, to four body forces in original write order (`0085b5a0`). |
| `reflex_matrix3_inverse` | Computes the specific cofactor/division/transpose inverse (`00859250`), preserving runtime numerator lanes and third-row signed-zero W. |
| `reflex_matrix3_product` | Computes Q × P with the original three-product reduction and W=Z packing (`00853740`). |
| `reflex_matrix6_inverse` | Composes two inverse3 calls, six products and exact packed subtractions in original order (`008597e0`). |
| `reflex_matrix4_inverse` | Copies the original 4×4 source and runtime globals into an inverse6 input, then extracts all sixteen original result words (`00859340`). |
| `reflex_rotor_product4` | Computes C × transpose(A), then writes all six C rows and both transposed B blocks with B.W=X (`00856b40`). |
| `reflex_constraint_solve` | Executes the complete bounded `0085c700`/`0085cf80` interval from a single graph/matrix/force snapshot, preserving original node/child order and shared parent-force updates. |
| `reflex_prepare_matrices` | Executes complete bounded matrix preparation (`0085cb70`) from source joint matrices, coefficients and globals; returns all ten matrix row sets and persistent slot8/9 products. |
| `reflex_mechanism_core` | Executes bounded node2 → preparation → force → constraint → velocity from one original initial state; returns every observed mutable row. |
| `reflex_bike_force_stage` | Computes the whole original bike force stage from supplied state, contacts, controls and globals; returns all state bytes. |

Every data call returns a status; failures leave output bytes untouched. The
functions do not allocate or read assets. Inputs remain unchanged unless a caller
uses the matrix/callback APIs’ explicitly supported overlapping output buffers. The original
slot-1 enable gate (`00430eb0`, simulation+`b0`) remains the caller's responsibility.

`planned_integrations` assumes the original callbacks leave mode/count
unchanged. Native slot 3 runs before the mode check and loop callbacks can
change the count. Matching captured child counts demonstrates that projection
for the observed calls; it is not a replacement for porting the callback loop.
Likewise, pass the current lead and fixed dt to the weights API after callbacks;
the original reads them at that later boundary.

## Unity binding and capture replay

The Unity project is [`Unity/Reflex`](../Unity/Reflex). Its
[`NativeCore.cs`](../Unity/Reflex/Assets/Scripts/Native/NativeCore.cs) binding
checks ABI version 1 and all forty-five struct layouts before calling the twenty-five C
exports in this staged candidate. `Float4` and the node structs are blittable and retain all four lanes;
the C# layer does not reimplement the numerical arithmetic. The universal Mac
library is installed at `Assets/Plugins/macOS/libreflexcore.dylib` and must be
imported for the macOS Editor and standalone macOS targets. Rebuild and recopy
that binary when changing native code; a loaded native plugin can require an
Editor restart before Unity uses its replacement.

In the live Editor, use **Reflex > Validate Native Capture Replay**, or invoke:

```csharp
Reflex.Port.Editor.NativeCaptureValidation.RunAndSave();
```

The method returns a JSON report and saves
[`_documentation/unity-native-validation.json`](../_documentation/unity-native-validation.json).
It verifies fixture bytes against their manifests and independently replays
669 scheduler, 1,089 velocity, 1,091 pose, 1,082 force, 4,372 v14 matrix leaf calls,
31,004 v15 matrix-family calls, 7,044 v16 concrete callback calls and 15,764 v17
inverse3 calls, plus 53,046 v18 product3, 8,841 inverse6 and 3,492 v19 inverse4
calls, plus 3,280 v20 rotor paired products and 1,460 complete v21 constraint
intervals, plus 1,556 v22 whole-preparation calls and 1,556 whole-core calls
inside Unity's managed domain,
checking every observed output word. The separate interpolation smoke check
is labeled explicitly; these captures do not contain native interpolation
weights. `RunJson()` returns the same report without saving it, while
`Run(repositoryRoot)` accepts an explicit repository location. No scene or
Play mode is required.

The frozen v23 binding and replay runner passed Unity `6000.6.1f1` Edit and Play
on Apple M5, including all fourteen fixture sets and the interpolation smoke
case. The Editor was cleanly closed before plugin replacement and restarted
before guarded validation. The installed universal SHA-256 is
`731542ab6c1136d408b577c4535a406f97f9e55a74a7014a0ca897e2b74bff22`.

Each replay begins with its own captured input state. The whole-core replay
connects its five stages without observed intermediate answers. It does not yet
provide a continuous vehicle simulation or generate contacts. The separately
replayed v24 force stage generates accumulated forces using supplied original
contacts and controls. The binding preserves the finite-domain and FP-control
requirements below: an incompatible Unity thread profile produces an explicit
failure instead of silently changing Unity's floating-point environment.
The fixture source/provenance audits remain part of the native test pipeline.

## Arithmetic contract

The port preserves separate binary32 subtraction, division, integer conversion,
multiplication and subtraction. It widens the **already rounded** quotient for
`ceil`. A normal double-precision accumulator or fused multiply-add can produce
different count/lead bits. Compilation rejects fast-math and disables contraction
and reassociation.

The supported arithmetic profile is round-to-nearest/even, masked exceptions,
and gradual underflow. The arithmetic APIs check caller FP controls and reject unsupported
rounding or flushing without changing them. This includes SSE DAZ/FTZ and ARM
FZ, independent FIZ, and unvalidated alternate handling. Arithmetic can set the
thread's normal sticky exception status flags. The Unity binding honors this
profile without altering controls; another caller must honor it or establish
and restore a compatible context explicitly.

The scheduler’s arithmetic inputs must be finite, fixed dt positive, and max count nonnegative.
Negative frame dt follows the original arithmetic; it is not silently clamped.
Overflow or out-of-range integer conversion returns a range error. The native
early-return guard (`simulation_type == 1 && flag_237c == 0`) runs before these
checks, preserving raw lead and previous count without timing arithmetic.
Interpolation is not clamped, and `previous` must not be simplified to `lead/h`.

## Evidence and limits

[`native-scheduler-validation.json`](../_documentation/native-scheduler-validation.json)
records the validation outcomes and fingerprints of the source, fixtures and
tested libraries at the initial scheduler milestone. Its adjacent CTest log
preserves that five-suite run; the later records below cover the expanded core.

[`tests/fixtures/scheduler_live/manifest.json`](tests/fixtures/scheduler_live/manifest.json)
pins 669 observed scheduler calls from three successful original-game runs,
including 320 active-world calls. Both native architecture builds reproduce
every exit count, lead bit pattern, and observed nested integration count.
These fixtures contain observed data only; no model prediction is labeled as
an original-game result.

The exporter revalidates the original captures, executable and proxy identity,
paired CSVs, successful completion counters, and closed calls. Its manifest
records source/validator hashes and historical report-label normalization.
Full source verification requires the preserved private capture directories:

```sh
python3 _re/trace/export_scheduler_fixtures.py \
  --verify native/tests/fixtures/scheduler_live
```

`--verify-vectors-only` checks the exported fixture bytes and manifest without
claiming a fresh audit of those private sources. CTest runs both checks by
default; `-DREFLEX_VERIFY_CAPTURE_SOURCES=OFF` omits only the full source audit
when private captures are unavailable. The independent Python oracle still
requires the fingerprinted target executable and reference model.

[`tests/fixtures/scheduler_edges.json`](tests/fixtures/scheduler_edges.json)
separately contains native instruction excerpts and 82 static/model cases.
They include adjacent float boundaries, signed zero, cap/zero-step behavior,
mode bypasses, early-return payload preservation, and explicit unsupported
domains/profiles. These cases have **not** been observed live. Interpolation
weights are checked against static instruction order and the exact rational
reference model, because the current trace does not capture native weights.

The Python oracle also compares deterministic randomized cases with
[`scheduler_model.py`](../_re/trace/scheduler_model.py). This broadens arithmetic
coverage without claiming additional native observations. Native C/C++ tests
exercise ABI sizes, error paths, unchanged output on failure, and incompatible
FP controls, including independent input flushing on supported ARM hardware.

## Velocity component

`reflex_velocity_integrate` accepts a timestep and 0..10 nodes. Each input node
contains six vec4s (96 bytes): linear/angular velocities, resolved forces and
coefficients. Each output node contains four vec4s (64 bytes): both derivative
caches and both advanced velocities. All four lanes participate, including W.
This additive ABI-1 extension uses layout IDs 5..7; existing scheduler IDs are
unchanged. Callers supply exact array byte sizes and four-byte-aligned buffers.
Inputs and outputs must not overlap. Zero nodes performs no arithmetic and
accepts null arrays with zero sizes, even when the unused timestep is nonfinite.

The supported domain uses finite inputs, the same FP profile as the scheduler,
and finite intermediate/results. Negative or zero dt remains valid. Any
nonfinite input, intermediate overflow, incompatible controls or invalid layout
rejects the entire call without changing output. This explicit API boundary is
narrower than the original loop: the original performs no work for negative
counts and does not impose a ten-node cap. NaN payload propagation and exception
status equivalence are unverified. Ten is the verified mechanism capacity.

[`tests/fixtures/velocity_live/manifest.json`](tests/fixtures/velocity_live/manifest.json)
pins two successful v11 original-game captures: 1,089 velocity calls and 4,356
node updates, including 478 active-world calls. Every node retains observed
entry/exit bytes for all eight relevant vec4s. Both Mac architecture builds
reproduce all 69,696 written float32 words exactly. The four source vectors
also remain byte-identical in every original call. All observed simulations
have four nodes and dt bits `3bf5c28f`; broader inputs are model coverage.

```sh
python3 _re/trace/export_velocity_fixtures.py --verify native/tests/fixtures/velocity_live
arch -x86_64 native/build-universal/reflex_velocity_tests \
  --fixtures native/tests/fixtures/velocity_live/vectors.tsv
arch -x86_64 /usr/bin/python3 native/tests/test_velocity_oracle.py \
  --library native/build-universal/libreflexcore.dylib
```

Contract tests cover separate rounding/FMA-sensitive cases, subnormal ties,
signed zero, all supported node counts, buffer boundaries, invalid FP controls
and atomic failure on a late node. The exact rational oracle separately checks
5,000 generated calls (23,372 nodes / 373,952 output words) and 5,309 arithmetic
range rejections on each Mac architecture. These are generated model cases,
not additional original-game observations. The seed is `0x52465832`.

The [velocity validation record](../_documentation/native-velocity-validation.json)
pins that milestone's source, capture, fixture and library fingerprints. The earlier
scheduler record remains a historical snapshot; its observed vectors were
unchanged when its source-audit manifest was refreshed for the extended runner.

## Position/orientation component

`reflex_pose_integrate` adds ABI-1 layout IDs 8/9/10: a 12-byte configuration
containing dt, signed node count and the game-global angular W; 64-byte input
nodes containing position, linear velocity, orientation and angular velocity;
and 32-byte output nodes containing position and normalized orientation.
All vectors have four lanes, with orientation stored x/y/z/w. Exact buffer sizes,
four-byte alignment and nonoverlap are required, as for the velocity component.

The implementation preserves the original quaternion shuffle/sign operations,
separately rounded float32 multiply/add instructions, and reduction
`(w*w + y*y) + (z*z + x*x)`. Every node advances before normalization begins.
The native angular W replacement is explicit configuration; the stored input
angular W is ignored numerically but still must satisfy this API's finite-input
contract. The supported profile includes gradual underflow and nearest-even
rounding. A zero norm yields four positive-zero output words, as the native
mask does; exception-flag equivalence is not claimed because native division
executes before masking. Overflow, including infinite norm-squared, rejects the
whole call atomically. The original's canonical-NaN path for infinite norms and
general NaN propagation remain outside scope.

[`tests/fixtures/pose_live/manifest.json`](tests/fixtures/pose_live/manifest.json)
pins two successful original-game captures containing 1,091 calls / 4,364 node
updates, including 481 active-world calls. Both Mac architecture builds reproduce
all 34,912 position/orientation words exactly. The same number of source velocity
words remains unchanged within the original pose kernel. Observed layouts have
four nodes, dt `3bf5c28f`, global W `00000000` and the established FP profile.

```sh
python3 _re/trace/export_pose_fixtures.py --verify native/tests/fixtures/pose_live
arch -x86_64 native/build-universal/reflex_pose_tests \
  --fixtures native/tests/fixtures/pose_live/vectors.tsv
arch -x86_64 /usr/bin/python3 native/tests/test_pose_oracle.py \
  --library native/build-universal/libreflexcore.dylib
```

Both architecture builds also pass 5,000 generated calls (27,189 nodes /
217,512 output words) and 894 atomic range rejections. The exact model uses
integer square root and rational midpoint comparisons, independent of hardware
sqrt. Contract tests include fixed cases sensitive to quaternion operation
order and norm reduction, signed zero, subnormals, zero norm, negative/zero dt,
global-W substitution, invalid buffers, all supported node counts and FP controls.
These cases expand model coverage; they are not new native observations.

The [pose validation record](../_documentation/native-pose-validation.json) pins
the current source, library and evidence. Earlier scheduler/velocity validation
records remain historical snapshots; their observed fixture vectors remain
byte-identical after fresh provenance audits against the extended runner.

**The numerical components do not yet form a complete vehicle step.** In the
combined original-game capture, velocities change between velocity exit and pose
entry in all 548 parent calls, including fourth-lane values. Two original vehicle
callbacks run in that interval. Those operations must be recovered before the
core can reproduce the complete integration wrapper.

[`integration-kernels.md`](../_re/notes/integration-kernels.md) records the exact
velocity/pose contracts; [`force-kernel.md`](../_re/notes/force-kernel.md) records
the resolved-force boundary below.

## Resolved-force component

`reflex_force_compose` adds ABI-1 IDs 11/12/13, preserving all previous layouts
and symbols. Its 40-byte configuration contains dt, signed node count, shared
gravity vec4 and live helper-global vec4 (`00d61670`). Each 112-byte input node
contains mass, accumulated linear force, linear impulse, angular velocity,
angular coefficient (`M+680`, distinct from velocity's `M+690`), accumulated
angular force and angular impulse. The 32-byte output contains the resolved
linear and angular force vec4s. Array, alignment, nonoverlap, count and atomic
failure requirements match the other node APIs.

The reciprocal timestep rounds once before both passes. The first pass forms
`mass * gravity + accumulated_linear_force`, then subtracts a matrix/vector
gyroscopic correction from accumulated angular force. The helper builds its
negative lanes using literal `+0 - angular_velocity`, retains the live global
diagonal, and reduces each dot as `(product_x + product_y) + product_z`.
Its final output is `(dot_x, dot_y, dot_z, dot_z)`: angular-force W receives the
same correction as Z. Only after all nodes complete that pass does the second
pass add `impulse * round32(1 / dt)` to both force vectors. Replacing this with
direct division, fused operations, a conventional W=0 cross product or sign-bit
negation can change output bits.

Nonempty calls require finite input words and nonzero dt; negative dt is valid.
Contributing intermediate overflow rejects the whole call. Original packed
operations also compute momentum W and helper products W whose values are
discarded. Those discarded intermediates may overflow without rejecting this
API; their arithmetic and FP exception effects are outside the data-output
contract. All supplied input W words must still be finite. A zero-node call
skips the original pre-loop reciprocal and FP checks, as documented explicitly;
general NaN propagation and exception-status equivalence remain unverified.

[`tests/fixtures/force_live/manifest.json`](tests/fixtures/force_live/manifest.json)
pins two successful v13 original-game runs containing **1,082 calls / 4,328 node
updates**, including 479 active-world calls. Both Mac architecture slices
reproduce **34,624 output float32 words** exactly. The seven source vec4s remain
unchanged within each original call, and shared gravity/global values are
recorded at this precise boundary. All observed calls use four nodes and dt
bits `3bf5c28f`; broader domain behavior is covered separately by model cases.

```sh
python3 _re/trace/export_force_fixtures.py --verify native/tests/fixtures/force_live
arch -x86_64 native/build-universal/reflex_force_tests \
  --fixtures native/tests/fixtures/force_live/vectors.tsv
arch -x86_64 /usr/bin/python3 native/tests/test_force_oracle.py \
  --library native/build-universal/libreflexcore.dylib
```

Each architecture also passes 5,000 generated calls (27,100 nodes / 216,800
output words), 1,943 atomic arithmetic-range rejections, and 86 invalid zero-dt
rejections against the exact rational model (seed `0x52465834`). Coverage
includes 450 calls with deliberate discarded-W overflow, nonzero helper
globals, subnormal outputs and signed zeros. Directed tests distinguish
reciprocal rounding, zero subtraction, duplicated W correction and each
contributing overflow stage. These generated cases are not additional native
observations. The Intel build was exercised under Rosetta.

Force composition still is not force generation or a complete vehicle step.
The original executes `0085c700` and `0085cf80` between force composition and
velocity integration; their node/constraint callbacks may change the resolved
inputs. They must remain separate boundaries until recovered and validated.


## Matrix subtraction leaf

`reflex_matrix_subtract` ports the observed `00857b30` numeric leaf. Additive
ABI-1 IDs 14/15 describe a 256-byte input and 32-byte output. The input contains
twelve row vec4s, two source vec4s and two old destination vec4s. Rows are in
native address order, grouped into four three-row blocks: `A+20/30/40`,
`+60/70/80`, `+a0/b0/c0`, and `+e0/f0/100`. Source and destination snapshots
correspond to the original objects’ `+10` and `+20` vec4s. No timestep, node
count or shape dispatch is invented for this leaf.

Each block dot uses separately rounded products and `((x+y)+z)`, producing
`(dot0,dot1,dot2,dot2)`. Blocks 0/2 use source 0 and blocks 1/3 use source 1.
The outputs are `oldY0 - round32(D0+D1)` and `oldY1 - round32(D2+D3)`, with
separate subtraction rounding in all four lanes. Destination W is real data;
it need not equal Z even though their matrix corrections are identical.

All 64 supplied float words must be finite, including unused row/source W.
The same supported FP controls apply. Every contributing product, partial dot,
block sum and output must remain finite; errors leave the entire output
unchanged. Discarded packed W products are not computed, so their overflow and
exception effects are outside the value contract. General NaN propagation and
exception-status equivalence remain outside scope.

The input is copied before arithmetic and output is published only after the
whole call succeeds. Unlike the node APIs, this leaf permits output/input
overlap at any four-byte-aligned offset within valid storage. This preserves
before-call source values when a native call aliases its objects. The original
object metadata and raw alias identities are retained in the capture audit;
the public value API does not recreate native object headers or pointers.

[`tests/fixtures/matrix_live/manifest.json`](tests/fixtures/matrix_live/manifest.json)
pins two successful v14 original-game captures: **4,372 leaf calls**, including
**1,924 active-world calls**, with **34,976 exact output words** on both Mac
architectures. Every observed call selects shape 6×6, owns the fixed-body
node’s matrix, and occurs within the audited force-to-velocity interval.
Other dispatch branches remain observed but unported. The fixture retains the
complete bounded matrix/source/destination bytes before and after each call,
plus source/proxy/validator fingerprints and a frozen observer-source snapshot.

```sh
python3 _re/trace/export_matrix_fixtures.py --verify native/tests/fixtures/matrix_live
arch -x86_64 native/build-universal/reflex_matrix_tests \
  --fixtures native/tests/fixtures/matrix_live/vectors.tsv
arch -x86_64 /usr/bin/python3 native/tests/test_matrix_oracle.py \
  --library native/build-universal/libreflexcore.dylib
```

Each architecture also passes 5,000 generated calls / 40,000 output words and
727 atomic range rejections against the exact rational model (seed
`0x52465835`). Coverage includes 1,163 overlapping-output calls, 397 discarded-W
overflow cases, 2,552 negative-zero words and 2,416 subnormal words. Directed
contracts check 65 aligned alias offsets, all 64 nonfinite input positions,
operation grouping and every contributing overflow stage. ARM disassembly uses
36 separate multiplies, 32 additions and eight subtractions with no fused
multiply-add instruction. Generated coverage does not broaden the original
capture’s observed dispatch shapes.

The v14 milestone completed this arithmetic leaf. The four other observed
leaves and four concrete node callbacks are covered below; prepared matrix
generation, force generation and complete vehicle-step equivalence remain work ahead.

The [matrix validation record](../_documentation/native-matrix-validation.json)
retains architecture outcomes, source/library hashes and the final **25/25**
passing native CTest run. Historical fixture vectors remain unchanged after
fresh source audits for the extended observer.


## Remaining observed matrix leaves

Four typed exports add ABI-1 IDs 16..20 while retaining every previous layout
and export. Their inputs contain only original arithmetic sources. In particular,
multiply inputs omit old destination values: the original overwrites them and
can replace arbitrary old bits, unless those same bytes are a real aliased source.

| Original leaf / export | Input ID / bytes | Output ID / bytes | Input row offsets within original A |
| --- | --- | --- | --- |
| `00853bd0` / `reflex_matrix4_multiply` | 16 / 80 | 17 / 16 | `10,20,30,40` |
| `008540a0` / `reflex_matrix6_multiply` | 18 / 224 | 15 / 32 | `20,30,40;60,70,80;a0,b0,c0;e0,f0,100` |
| `00857750` / `reflex_matrix4x6_subtract` | 19 / 176 | 17 / 16 | `20,30,40,50;70,80,90,a0` |
| `00857a00` / `reflex_matrix6x4_subtract` | 20 / 144 | 15 / 32 | `20,30,40;60,70,80` |

Offsets in the table are hexadecimal. Each row occupies a full vec4. The input
structs then hold one or two source vec4s, followed only for subtraction by one
or two old destination vec4s. Exact offsets and declarations are in the public
header. Every supplied input word must be finite, even discarded W lanes; the
old output memory of a multiply is never inspected. All four APIs snapshot input
before computing, permit aligned output/input overlap within valid storage, and
publish output only after the entire operation succeeds.

The two four-lane-dot leaves (`00853bd0`, `00857a00`) reduce separately rounded
products as **`(py + pw) + (px + pz)`**. Their native packed instructions also
compute unused additions involving source X; overflow in those unselected lanes
does not reject this finite contributing-value API. The two xyz-dot leaves use
**`(px + py) + pz`**, with discarded W-product effects outside scope. Every
contributing product, partial sum, block sum and subtraction must remain finite.
FP control checks, signed zero, gradual underflow and atomic rejection match the
existing numerical components.

Output packing is specific to each original function:

- `00853bd0` writes four distinct dot products; source W contributes.
- `008540a0` adds four three-row blocks in pairs and duplicates each result’s Z into W.
- `00857750` subtracts four distinct paired xyz-dot results, including its fourth row.
- `00857a00` subtracts corrections `(d0,d1,d2,d0)` and `(d3,d4,d5,d3)`. W repeats the **first** dot in each triple.

All original matrix/source reads finish before the first output store. The
snapshot contract therefore handles the observed in-place multiplies without
using a different update order. Prepared matrices and dispatch selection remain
external inputs/control flow. Four concrete node callbacks are covered below.

[`tests/fixtures/matrix_family_live/manifest.json`](tests/fixtures/matrix_family_live/manifest.json)
pins two successful v15 runs, preserving all five observed leaves, their raw
object bytes, alias identities, direct calls and dispatcher parents:

| Leaf | Captured calls | Output words |
| --- | ---: | ---: |
| `00853bd0` | 2,696 | 10,784 |
| `008540a0` | 12,132 | 97,056 |
| `00857750` | 5,392 | 21,568 |
| `00857a00` | 5,392 | 43,136 |
| `00857b30` | 5,392 | 43,136 |
| **Total** | **31,004** | **215,680** |

The four new leaves account for 25,612 calls / 172,544 words; the retained
`00857b30` v15 observations are additional to its unchanged v14 fixture. All
31,004 calls match on Apple Silicon and Intel under Rosetta. There are 12,121
active-world calls across 1,348 total captured integration steps. Four direct
`008540a0` calls occur per integration outside the shared dispatcher; they remain
nested within the audited pass and are retained without assigning an unsupported
matrix-owner identity.

Each new export also passes 5,000 deterministic generated cases on both Mac
architectures: **20,000 calls / 120,000 output words**, plus 2,476 atomic range
rejections. Coverage includes 4,867 overlapping outputs, 1,690 cases whose unused
native intermediates overflow, 7,540 negative-zero words and 7,704 subnormal
words. Seeds derive from `0x52465836 XOR leaf_rva`. Directed contracts distinguish
the reduction orders and W packing, test every input position for nonfinite
rejection, every aligned alias offset, and all contributing overflow stages.
These generated inputs are model coverage, separate from the live observations.

```sh
python3 _re/trace/export_matrix_family_fixtures.py \
  --verify native/tests/fixtures/matrix_family_live
arch -x86_64 native/build-universal/reflex_matrix_family_tests \
  --fixtures native/tests/fixtures/matrix_family_live/vectors.tsv
arch -x86_64 /usr/bin/python3 native/tests/test_matrix_family_oracle.py \
  --library native/build-universal/libreflexcore.dylib
```

The C# binding/replay compiles against the installed Unity references and
adds `matrix_family` totals plus `matrix_family_leaves` to the JSON report.
The historical v15 universal plugin SHA-256 was
`0988ddf53a3a31eec2dc67454933a388924e8041a02858eb2b97d5605347564e`.
After a controlled plugin replacement and Editor restart, the family replay
passes all 31,004 calls / 215,680 words in both Unity Edit and Play modes,
alongside every historical fixture.

The observed dispatcher leaves are now numerically implemented within their
finite-value contracts. This does not implement the complete constraint solve:
prepared matrix generation, remaining vector operations and the vehicle pipeline
still need their own evidence and ports. The next section covers four concrete callbacks.

The [family validation record](../_documentation/native-matrix-family-validation.json)
pins the final **30/30** native checks, architecture outcomes, sources and tested
libraries. [Original instruction notes](../_re/notes/matrix-family-kernels.md)
record the detailed reduction, write order and alias evidence.

## Concrete callback boundary

The four callback inputs use layouts 21–24 (544, 400, 496 and 352 bytes), with
body-force output layout 25 (64 bytes). Slot 3 reuses existing matrix outputs
15/17. All supplied float words must be finite; contributing overflow rejects
atomically. Matrix row order and exact arithmetic are documented in
[`callback-kernels.md`](../_re/notes/callback-kernels.md). Inputs retain every
stored lane, including finite W values discarded by downstream xyz products.
Discarded W overflow and unselected SIMD exception effects are outside scope.

Slot 5 includes four canonical alias indices. An index refers to the first
occurrence of the same body destination, whose initial force words must agree.
Updates occur sequentially, and all returned aliases contain the final force.
Input snapshots permit valid output overlap; errors publish no partial bytes.
The API does not accept or dereference original-game pointers.

The new frozen dataset contains 7,044 callback calls from two complete v16
captures: 1,174 fixed slot-3, 2,348 rotor slot-3, 1,174 fixed slot-5 and 2,348
rotor slot-5 calls. All 75,136 observed output words match both architecture
slices. Each slice also passed 20,000 accepted exact-model generated cases
(5,000 per callback), 220,000 output words and 3,199 atomic range rejections.
The tests cover transpose order, W packing, signed zero, gradual underflow,
canonical body aliases, every aligned input/output overlap offset, incompatible
FP controls, nonfinite inputs and contributing overflow before cancellation.

The [callback validation record](../_documentation/native-callback-validation.json)
pins the final **35/35** native checks, both architecture results, frozen fixture,
source/library hashes and Unity replay. All 296 trace tests also pass. The
[fixture manifest](tests/fixtures/callback_live/manifest.json) includes fresh
whole-stream audits and the nested callback/matrix source joins; all six older
fixture datasets retain byte-identical observed vectors.

## Exact Matrix3x3 inverse

`reflex_matrix3_inverse` adds ABI 1 layouts 26 (84-byte input) and 27 (48-byte
output). Input is three stored vec4 rows, a runtime numerator vec4, four unsigned
mask words and an unsigned transpose RVA. The verified mask is
`ffffffff,ffffffff,ffffffff,00000000`; the target identity is `00457c90`.
Unsupported mask or target is rejected, and no captured address is invoked.

The cofactor products/subtractions, determinant `(px+py)+pz`, each numerator
lane's division, and scaled cofactors round separately to binary32. The final
transpose copies Z into W only for rows 0/1. Row 2 W is positive zero multiplied
by the W quotient, preserving its sign. There is no implicit numerator of one,
pivoting, determinant tolerance, clamping or singular fallback.

All supplied floats must be finite. A zero determinant or nonfinite
contributing intermediate, any quotient lane, or output returns a numeric-range
error without modifying output. Native masked-W cofactor overflow and exception
status are outside this value contract. All input bytes are snapshotted before
any output write, so every valid four-byte-aligned input/output overlap works.
Raw capture validation additionally preserves original source/destination alias
semantics and checks the actual destination transpose target. See the
[instruction and contract notes](../_re/notes/inverse3-kernels.md).

Both architecture slices pass the inverse3 API contract and 5,000 accepted
exact-model generated cases / 60,000 output words each, plus 1,146 atomic
singular/range rejections. Coverage includes 1,145 overlapping outputs, 430
masked-W overflow cases, 5,700 negative-zero words, 4,943 subnormal words and
nonuniform numerator lanes.

The [inverse3 fixture](tests/fixtures/inverse3_live/manifest.json) contains two
complete accepted v17 captures: 1,126 preparations, 15,764 inverse3 calls and
189,168 output words, including 7,056 active-world calls. Every output word
matches both native slices. Observed source/destination pairs are disjoint;
same-pointer and partial raw aliases are additional contract tests. Frozen
source audits include the preparation/solver/physics intervals. The separate
preparation-to-consumer matrix joins remain independent root evidence rather
than a claim that this helper implements the whole preparation routine.


The [inverse3 validation record](../_documentation/native-inverse3-validation.json)
pins the final **40/40** passing native checks, both architecture results,
source/fixture hashes and tested libraries. All seven earlier fixture datasets
retain byte-identical TSVs after fresh provenance audits. That inverse3 milestone
library SHA-256 was
`9728d8bc19f99146798ab844577ee79401483dc8d52e25a5fdd0bf2aacc22a2d`;
it passed all captured datasets in guarded Unity Edit and Play modes before
the later factorization update. Both modes matched all 15,764 inverse3 calls /
189,168 output words. The
record also pins the final 354 passing trace regressions and both raw Unity
command results.


## Matrix3 product and ordered Matrix6 inverse

The two v18 APIs add layouts 28–30 while preserving ABI 1 and all prior
layouts/exports: the interface now has **19 exports / 30 layouts**. Product
input is 100 bytes (P48 + Q48 + transpose RVA4), reusing the 48-byte inverse3
output. P is the original ECX right input; output is Q × P. Inverse6 input is
240 bytes (four blocks192 + numerator (16 bytes) + mask (16 bytes) + four target RVAs16), and
output retains all 192 bytes / 48 words. C# names are `ProductMatrix3` and
`InverseMatrix6`.

The product computes each XYZ dot as `((x+y)+z)` with separate binary32
rounding, then copies Z into W. The inverse executes E=inv(A), T=C×E, U=E×B,
V=C×U, K=V−D, H=inv(K), B'=U×H, C'=H×T, R=U×C', A'=E−R, D'=+0−H.
The subtraction includes all four lanes. A.row2.W can differ from Z;
D.row2.W is +0. Runtime numerator lanes, the exact XYZ mask and four concrete
virtual target identities remain explicit. No pivot or singular fallback is
introduced.

Both APIs snapshot inputs and atomically commit outputs, including every
valid four-byte-aligned overlap before or after input. Finite input and
contributing intermediates are required, including K.W before the existing
inverse3 component. Discarded W arithmetic and exception-status equivalence
remain outside the value contract. The [instruction and scope notes](../_re/notes/factorization-kernels.md)
detail target identities, raw alias/header rules and the observed child graph.

Both architecture slices pass 5,000 exact-model calls for each new API
(300,000 output words per architecture), plus 591 product and 1,095 inverse6
atomic range/singular rejections. Tests preserve signed zeros, subnormals,
nonuniform numerator lanes, noncommuting blocks, unsupported target checks,
all aligned output overlap offsets and K.W range failures.

The accepted v18 fixture contains 8,841 inverse6 and 53,046 product3 calls:
**61,887 new calls / 1,060,920 output words**. The full interval audit also
joins their 17,682 already-ported inverse3 children, yielding 70,728 arithmetic
child joins and 1,273,104 observed words across both complete drives. The
frozen TSV exports original outputs, never model predictions. This is the
whole observed inverse6 boundary; the following v19 component adds inverse4
embedding. Full preparation remains outside both components' claims.

The [factorization validation record](../_documentation/native-factorization-validation.json)
pins **45/45 passing native checks**, both architecture comparisons, 408
trace regressions and the fresh full/portable fixture audit. All eight earlier
fixture TSVs remain byte-identical after renewed provenance audits. The
universal library SHA-256 used for that v18 milestone was
`5c482153d5067b4dca465efd407a0950fb21c8012bc25a8037563bfdfd412a25`.
After a clean Editor exit, replacement and restart, guarded Unity Edit and Play
replays both match all 61,887 new calls / 1,060,920 words and every historical
dataset. These are independent captured-state replays; the composed inverse
does not yet provide full preparation or a complete vehicle step.


## Original 4×4 matrix inverse wrapper

`reflex_matrix4_inverse` adds layout 31 `reflex_inverse4_input` (160 bytes)
and layout 32 `reflex_inverse4_output` (64 bytes), retaining ABI 1 and every
previous layout/export. The interface now has **20 exports / 32 layouts**.
Input stores four rows (64 bytes), zero, axis_x, axis_y, axis_z and numerator
(16 bytes each), and a uint32 mask (16 bytes). The C# method is `InverseMatrix4`.

Embedding and extraction are literal bit copies around the already verified
inverse6 component. A preserves source rows 0–2; B holds their W lanes in X
with captured zero.YZW; C is source row 3 plus two captured zero rows; D is
(source row 3 W, axis_x.YZW), axis_y, axis_z. Output rows 0–2 take result.A.XYZ
and result.B.X; row 3 takes result.C.row0.XYZ and result.D.row0.X. All sixteen
words, including signed zero, are compared. Runtime globals remain inputs;
their ordinary initialized values are not substituted.

The finite-input contract also checks overwritten axis_x.X. Singular leading
or Schur blocks, contributing overflow and K.W overflow retain the inverse6
atomic range-error behavior. There is no generic inverse, pivoting, tolerance
or fallback. All valid four-byte-aligned input/output overlaps are supported,
including output starting before input; errors leave output untouched. Raw
capture checks preserve typed entry headers, unchanged destination headers,
source-header overwrite only through actual output overlap, stable globals
and every byte outside destination rows. The parent audit proves the complete
48-word embedding, one child inverse6 and sixteen-word extraction with exact
scratch ownership; [the kernel notes](../_re/notes/inverse4-kernels.md) detail
this boundary and its limits.

Both architecture slices pass 5,000 generated finite calls / 80,000 words,
plus 1,067 atomic singular/range rejections. Coverage includes nondefault
runtime globals, nonuniform numerator lanes, dense nonsymmetric matrices,
subnormals, signed zeros, all aligned overlap offsets and invalid mask/FP
profiles. Two complete accepted dirt-bike captures contain **3,492 calls /
55,872 observed output words**, including 1,856 active-world calls. Full parent
audits check all 167,616 embedded words and the 3,492 inverse6 child bindings;
the frozen fixture requires the observed motocross simulation vtable.
The [inverse4 validation record](../_documentation/native-inverse4-validation.json)
pins **50/50 passing native checks** and **458 passing trace tests**. All nine
earlier fixture TSVs remain byte-identical after fresh full and portable
provenance audits. Both native architecture slices reproduce all 55,872
original output words. The separate
[consumer-link report](../_documentation/inverse4-consumer-links.json) joins
all 3,492 complete results to the prepared node matrices and their later
original matrix-vector consumers.

After a clean Editor exit, plugin replacement and restart, guarded Unity
Edit and Play replays match all 3,492 inverse4 calls / 55,872 words and every
historical dataset. The universal plugin used for that v19 milestone has SHA-256
`b83327168ec4dfc7de92b0a370ed88e98d6dfcd4fc05c4df540b281a6e850c6e`.
The validation record pins both raw Unity reports and the saved Play report.
The current milestone remains on the observed motocross physics path and
adds no ATV-specific behavior or complete physics-step claim.

## Rotor paired preparation product

`reflex_rotor_product4` ports the specific `00856b40` leaf. Input layout 33
contains four A rows and six old C rows (160 bytes; C offset 64). Output
layout 34 contains two four-row B blocks and six C rows (224 bytes; C offset
128). The interface remains ABI 1 with **21 exports / 34 layouts**. Old B
is unread in the original function and absent from numerical input.

The six-by-four result is C × transpose(A); each four-lane dot uses separately
rounded `(pY+pW)+(pX+pZ)`. B's two XYZ blocks transpose that result and repeat
X in every stored W. All 56 output words are retained. A need not be symmetric.
The 40 supplied floats and contributing arithmetic must be finite; original
unselected SIMD additions may overflow without changing the selected result,
and their exception effects remain outside value equivalence.

Raw original A/B/C spans must be pairwise-disjoint because the original
interleaves stores with later input loads. Only B/C data rows may change.
The normalized C API snapshots input and atomically commits both results,
allowing all valid aligned overlaps between its own input/output buffers.
That convenience does not broaden the original-object alias domain.
[Kernel notes](../_re/notes/rotor-product4-kernels.md) give the exact layouts,
operation order, header checks and preserved bytes.

ARM64 and Intel-under-Rosetta each pass **5,000 generated calls / 280,000
output words** and 1,340 atomic range rejections, with identical logs. Tests
cover nonsymmetric matrices, reduction-order cancellation, B.W=X, signed
zeros, subnormals, discarded-lane overflow, every aligned normalized output
offset and FP-control guards. Two accepted motocross runs contain **3,280
calls / 183,680 original output words**, including 1,770 active-world calls.
Both architectures reproduce every captured word. The full parent audit
joins all 52,480 preceding inverse4 matrix words and both prepared outputs.
All ten historical fixture TSVs remain byte-identical after fresh audits. All
**55 native checks** and the frozen v20 run's **503 trace tests** pass. The
[rotor-product validation record](../_documentation/native-rotor-product4-validation.json)
pins the source, original fixture, architecture logs and raw Unity reports.
After a clean plugin replacement and restart, guarded Unity Edit and Play
replay all eleven datasets successfully, including all 3,280 new paired calls.
The installed universal library SHA-256 is
`a2102ec8507f46636d27ecfae74ef439cc4d4d0255754136160b6010641fff2b`.
This leaf remains one bounded piece of
matrix preparation, with full preparation and continuous dirt-bike driving
still incomplete.


## Complete bounded constraint interval

`reflex_constraint_solve` composes the original slot-3 pass and reverse/root/
forward tree solve from one starting state. It uses the existing five matrix
leaves and four concrete callbacks. It never injects captured intermediate
solutions or callback outputs. Prepared matrices, coefficients, bias, runtime
G and initial forces/scratch remain caller inputs.

ABI-1 layouts **35–37** are config20, node input1760 and node output128 bytes;
the interface now has **22 exports / 37 layouts**. Count is 1–10, with root at
index0 and at most five stored-order children per node. Parent indices may
follow children, so original numeric traversal and initial solution state are
preserved. Every workspace is filled with two copies of runtime G. Slot-5
callbacks update shared parent forces immediately in ascending node order.
[Kernel notes](../_re/notes/constraint-solve-kernels.md) specify each byte offset,
shape, arithmetic dependency and output word count.

The finite domain covers values consumed by the invoked numerical components;
overwritten scratch and unused matrix/vector capacity can retain arbitrary
bits. Exact sizes, four-byte alignment, valid rooted graphs and supported FP
controls are required. All normalized inputs are snapshotted and every output
is committed together; valid aligned overlaps are supported and all failures
are atomic. The original raw domain is separately constrained to constructor
objects, with only the root's exact .40/.68 vector alias admitted.

Both architecture slices match **1,460 original motocross intervals / 140,160
raw output words**, including 684 active calls. Each also matches **2,500
generated intervals / 13,518 nodes / 432,576 normalized output words** and
385 atomic range rejects. Directed tests cover shared force accumulation,
stored child order, nonmonotonic trees, runtime G, ignored NaNs and overlap/FP
contracts. C# staging compiles and verifies all37 layouts plus native node-array
marshalling. Guarded Unity Edit and Play both reproduce all 1,460 whole
intervals and every historical dataset after a clean plugin replacement and
restart. All **60 native checks** pass (250.77 seconds), and the frozen v21
trace suite passes **569 tests**. All eleven historical fixture TSVs remain
byte-identical after fresh full and portable audits. The final evidence is in
[`native-constraint-solve-validation.json`](../_documentation/native-constraint-solve-validation.json).

This closes the bounded constraint interval for the reviewed node classes.
The complete preparation extension is described below; force/contact generation
and continuous dirt-bike driving remain separate work.


## Complete bounded matrix preparation

`reflex_prepare_matrices` composes `0085cb70` from each node's two source joint
matrices, own coefficients, ordered graph and seven runtime globals. No captured
prepared output is an input. It preserves the two descending passes, child
order, runtime axis/zero rows, both node products, paired products and exact
inverse arithmetic. [Kernel notes](../_re/notes/preparation-kernels.md) specify
the lane mappings and raw joins.

ABI 1 now has **23 exports / 40 layouts**. Layouts38–40 are config116,
node448 and output2304 bytes. Output holds ten matrix row sets and persistent
slot8/9 products; meaningful source rows are copied unchanged and unused
normalized capacity is zero. The graph has 1–10 nodes, root0/parent−1, at most
five children and parent-before-child indices. All supplied globals and
coefficients and meaningful source rows must be finite; only contributing
intermediates enter the inherited inverse/range domain. Root source capacity
and unused row tails are ignored. Supported FP controls and four-byte alignment
are required. Snapshots and one staged commit allow normalized overlaps and
make every error atomic; the original raw domain remains disjoint constructor
objects. Original discarded SIMD exception effects are outside value parity.

Two new accepted motocross runs contain **1,556 calls / 6,224 nodes / 768 active
calls**. Both architecture slices exactly match **1,842,304 original output
words**, including 348,544 previously unobserved slot8/9 result words; another
348,544 source words remain bit-identical. The fixture stores actual preparation
entry/exit and enclosing core entry/exit bytes, after a fresh complete core and
nested-stage audit. Native replay compares every normalized output lane, while
the evidence count includes only meaningful original writes.

The directed contracts cover analytical signed-zero inverses, all241 aligned
root-output placements, config/node/output overlap, source W finite guards,
ignored rotor tails, singular and overflow rejection, graph bounds and FP
controls. Both architectures additionally match **1,000 generated calls / 5,484 nodes /
3,158,784 normalized words**, with 104 atomic range rejections, non-default
globals, mixed branching graphs and 58,066 negative-zero words. Staged C# passes
all40 ABI checks and native array marshalling. Guarded Unity Edit and Play
reproduce every dataset, including all1,556 whole-preparation calls, after the
clean restart. The final trace suite passes **612 tests**; all **65 native checks** pass (433.35 seconds), including every frozen
fixture and fresh-source audit. All twelve historical TSVs remain unchanged.
The [native preparation validation record](../_documentation/native-preparation-validation.json)
pins architecture logs, source evidence and guarded Unity reports. Continuous
vehicle stepping, force/contact generation and post-integration corrections
remain separate work.


## Complete bounded mechanism core

`reflex_mechanism_core` connects ascending node2 joint generation, whole matrix
preparation, resolved-force composition, both constraint passes, and velocity
integration from one original entry snapshot. Prepared matrices, RHS, bias,
resolved force, solutions and acceleration are all predicted internally. The
public ABI remains version1 with **24 exports /43 layouts**; new layouts41–43
are config216, node656 and output2528 bytes. The output includes every prepared
matrix and persistent slot8/9 product plus bias, scratch and changed body rows.
[Kernel notes](../_re/notes/mechanism-core-kernels.md) describe all fields and
raw preservation rules.

The graph has1–10 nodes, root0/parent−1, parent-before-child indices and at most
five stored-order children. The finite contributing domain, inverse singularity
bounds and supported FP controls are inherited from the components. Finite
nonzero dt includes negative values. Ignored root state and overwritten source
rows are not numerically validated. Configuration and all nodes are snapshotted;
all output commits atomically, including normalized overlaps. Original raw
constructor aliases and preserved bytes are separately audited.

Both Mac architecture slices reproduce **1,556 original motocross intervals /
2,390,016 unique output words**, with768 active calls. The native raw projection
also matches **16,306,880 complete snapshot bytes**, including **6,746,816
preserved bytes**. Every normalized output lane is checked:3,933,568 words.
These original intervals overlap the preceding preparation evidence; counts
are not independent new gameplay samples. Both slices pass1,000 generated
supported graphs and121 atomic range rejections. Directed contracts cover
analytic connected outputs,293 aligned placements, three-way overlap, unused
input bits, late-stage errors and FP guards. C# compilation and all43-layout
native array marshalling checks pass.

All **70 native checks pass** (555.17 seconds), including every historical
fixture and new full-source/portable audit. Guarded Unity Edit and Play both
reproduce all fourteen fixture sets and the complete core after clean plugin
replacement and restart. The [native mechanism-core validation record](../_documentation/native-mechanism-core-validation.json)
pins the original observations, generated comparisons and live Unity reports.
The final current trace suite passes **648 tests**. Continuous
vehicle stepping still needs gameplay force/contact generation, post-core
callbacks, pose advancement and post-integration corrections.

## Whole dirt-bike force stage (isolated v24)

`reflex_bike_force_stage` computes original `0043e100` from one complete entry
snapshot. It runs the timer/gear/engine/control-torque branch, both suspension
paths, damping, four runtime-G resets, front tire, rear tire, and final torque
copy. Every operation reads the prior operations' writes, including the shared
wheel-contact alias at `S+8810`. No captured intermediate result is supplied.
The public ABI remains version 1 and now has **25 exports / 45 layouts**.

| Field | Input offset | Bytes |
|---|---:|---:|
| Complete simulation `state_bits[9664]` | 0 | 38656 |
| `control_bits[64]` | 38656 | 256 |
| Axis X, axis Y, numerator, G (four raw vec4s) | 38912 | 64 |
| `dt_bits` | 38976 | 4 |
| Raw `conversion_flag` | 38980 | 4 |
| `x87_control_word` | 38984 | 4 |

Layout 44 is 38,988 bytes. Layout 45 returns the complete 38,656-byte simulation
state. Every field is a `uint32_t` bit word; opaque integers and pointer words
are preserved without dereferencing them. Four-byte aligned input and output
may overlap arbitrarily in valid caller storage. All input is copied before
calculation and all output is committed together. Errors leave output bytes
unchanged. The caller's floating-point controls and status flags are restored
on success and error, including late rear-tire failures.

Supported original profiles use raw conversion flag 0 or 1 and x87 CW `007f`
or `027f`, with the core's supported host floating-point controls and finite,
nonnegative timestep. Handwritten arithmetic checks its bounded contributing
inputs/intermediates. The packed tire translation retains original masked
invalid-lane behavior and checks final written floating-point words. Its five
written integer flags (`8850`, `8854`, `89a0`, `89a4`, `8da0`) are checked as
integer booleans; interpreter-only pointer plumbing is excluded. Unread raw
state is never blanket-checked as floating point. Unsupported indexed domains,
nonfinite final numeric stores and other bounded execution errors fail atomically.

The fixed tire instruction table and exact 24/53-bit x87 addition adapter are
private implementation details, not additional public physics APIs. The engine
uses host double power rounded to binary32. Recorded original outputs and the
expanded power tests establish tested bit parity, **not universal VC90 CRT
power equivalence**; the API does not whitelist only observed blend words.

Two separately accepted original-bike captures contain **1,261 calls / 499
active calls / 149,388 mutable output words**. All **12,186,304 full simulation
words** also compare exactly, covering preserved headers and opaque fields.
Both captures use the reviewed v24 observer and actual loaded `MSVCR90.dll`
SHA-256 `cf420aced0d810e1d75f6811dd986f2d9fded2fbb8d61fc9a7024520c475febb`.
The frozen JSONL contains actual entry/exit bytes; full fixture verification
reruns strict numerical, parent, drive, runtime and source-continuity audits.
Portable verification checks pinned bytes and numerics without claiming a
fresh original-source audit.

ARM64 and Intel additionally reproduce **1,320 generated original calls /
12,756,480 full-state words each**, including active, inactive and mixed-contact
cases across all four supported profiles. Contracts exercise **19,412 aligned
overlap placements**, early and late atomic failures, opaque NaN/pointer words,
profile gates and floating-point status restoration. The managed binding checks
45 ABI layouts, guards all fixed-array lengths and passes original full-state
marshalling. The editor runner compares every state word and reports mutable
versus full-state counts separately. The canonical library now passes all 15 fixture groups inside Unity
6000.6.1f1 in both Edit and Play modes, including these 1,261 force calls and
12,186,304 full-state words. The additive completion record is
`_documentation/native-bike-force-stage-unity-completion.json`; the earlier
native evidence remains byte-for-byte preserved.

This stage generates forces using supplied original contacts and controls.
It does not generate world contacts or controls and does not by itself establish
a connected complete vehicle step. Previous milestone evidence is unchanged.
