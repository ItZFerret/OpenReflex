# Buggy model pose (004a4270)

Implementation: `game/vehicle_buggy_pose.c`, adapter `oracle/vehicle_buggy_pose.cpp`,
verification group `rx_BuggyPose`. The complete body is present; oracle validation
is pending. This is a hand transliteration of `_re/out/decomp` and the original
SSE/x87 disassembly, not a generated lifted port.

ABI: ECX vehicle, stack float dt, RET4. Frame is PUSH EBP, align16, SUB 0x2b8,
PUSH ESI/EDI. Core frame S is therefore aligned base minus 0x2c0. Cookie S+0x2bc
XOR S+8. Node quaternion scratch is S+0xf0; engine local quaternion S+0x120.
The sim pointer is vehicle+0x15c0. Main model node is +0x1360, model ref +0x1328.

The function has three parts:

- 004a4364..004a7212: independent front arms. Sources sim+0x360/0x400,
  quaternions +0x9a0/+0xa40. Fixed arm offset angles .045, .5325, .5275 half
  angles; all node rotations go through matrix-to-quaternion conversion. Node
  groups +0x1364..+0x1378 and +0x137c..+0x1390. Endpoint writes +0x8740..+0x87b0.
- 004a7217..004ab23a: independent rear two-link solves, sim+0x220/0x2c0,
  quaternions +0x860/+0x900. Steering/offset quaternion sim+0x5620, centre
  sim+0x36e0, attachment sim+0x3f90. Right side mirrors the already-subtracted
  centre-relative anchors. Node groups +0x1394..+0x13b0 and +0x13b4..+0x13d0.
  Endpoint writes +0x87c0..+0x8830 (some left-side endpoints are shared).
- 004ab23f..004ab9d5: engine node +0x13d4, rest quaternion +0x1400, lag filter
  +0x13d8, previous filtered RPM +0x13ec, phase +0x13f0. Input dt is used here.

Exactness details to retain in review: packed XYZ normalization is masked division
by SQRTPS and the positive canonical NaN for infinite squared length. Basis first
row uses scalar SqrtF with the y-square+x-square+z-square sum. Cosine fits are
original range-reduction polynomials. Rear asin is mixed SSE double/single and
uses the authoritative `rider_asin` helper. Left rear atan2 explicitly rounds to
float; right rear thunk0041c4b0 also rounds to float before x87 FSUBR. Engine
packed cosine uses MAXPS(0-x,x), and inserts the negative cosine into W through
the semantics of 0041c4d0. Its normalized axis is (0,0,-1,-1), with W replaced.

Builds and runtime tests are owned by the lead and run serially; this unit has
only received a static C syntax check so far. The first oracle comparison must
check per-site quaternion operand ordering and all endpoint lanes, including W.

The lead's `buggy-fix2` restored-state run verified 74 pose calls with zero
mismatches, covering 30 ordinary frames plus 44 isolated stimuli. Front basis
construction uses direction cross X, while rear fixed-angle quaternions multiply
the basis on the left; these differences matter even for almost planar links.

## Rider publication

`vehicle_buggy_runtime.c/.h` and `oracle/vehicle_buggy_runtime.cpp` implement
004abe20..004ac0ee, group `rx_BuggyRuntime`. It evaluates NDE_RiderLocator, passes
the original S+0x20 matrix to rider virtual +0x40, then reads its possibly changed
translation. It publishes the vehicle-relative velocity, simulation input values,
and reverse movement flag. Its private lazy globals are db46c0/c4/c8 and db46d0;
its simulation pointer is V+15c0. The last rider virtual is a float argument call
(+0x4c, authoritative world_rider.h), so it uses world_vcall_f and a float public
signature. Ordinary integer virtual dispatch would pass a stale host XMM value.
Original x87 FLD/FSTP copies quiet signaling NaNs. Static C/C++ checks pass;
runtime verification is pending with the lead.

## Poststep closure

`vehicle_buggy_poststep.c/.h`, adapter group `rx_BuggyPoststep`, adds vehicle
vf61 004ac520 and simulation helper 004bd860..004be48b. The vehicle's signed
frame phase modulo three selects phases one and two. Simulation poststep calls
0085c6a0 on its embedded mechanism, visits four wheel debug helpers, and resets
sim+8010. Original frame sizes/call stack depths are retained.

Shared `vehicle_poststep_helpers.c/.h`, group `rx_VehiclePoststepHelpers`, owns
0085c6a0 (all mechanism nodes, virtual slot4), 004d7fa0 (offset-wheel debug), and
00459de0 (axis-wheel debug). The two debug routines have no calls and every
store is ESP-relative. Their original aligned0x40 frames and final scratch
vectors are retained; overwritten intermediate spills are not materialized.

Persistent-effect audit of004bd860: calls at004bd873/897/8bb/8c7/8d3;
from004bd8d8 through004be488 every store is ESP-relative except
004be1c3 `MOV [ESI+8010],0`. No calls occur in that interval and no scratch
address escapes. The dormant debug geometry arithmetic and its probe-count
loop are therefore omitted, retaining the latch reset and final tail scratch
vector copied from sim+8830. This is an audited dead-work omission, not a
replacement for an active gameplay computation. Lead authorized this scoped
omission; oracle and reference comparisons remain required.


## Lead verification, 2026-10-21

Accepted reference evidence and current native boundary are recorded in `HANDOFF/SESSION_2026-10-21_VEHICLE_POSES.md` and `builds/sol_vehicle_pose`. The 600-frame UTV/SuperBuggy/ProLite/Pro2 comparisons match, including whole-memory hashes. Restored probes cover articulation and post-step paths absent from natural recordings; the fixed-body node is verified separately on the real MX rider node. Native playability is a separate acceptance and is not implied by these method comparisons.
