# UTV runtime leaves and rider publication

Hand ports from `_re/out/decomp/reflex_002_004b32a0.c`, checked against each
original disassembly. Group `rx_UtvRuntime` has 16 registrations:

- `004c6780` V35 rider publication: original aligned 0x58 frame and matrix at
  ESP+20. Rider virtual +40 receives the evaluated NDE_RiderLocator matrix.
  Scalar velocity sums retain their differing x/y/z term order; movement uses
  (dz²+dy²)+dx². The original only initializes db46a0/a4/a8 and never updates
  them here. Input values and dt passed through x87 loads/stores quiet sNaNs.
- `004c6ec0..004c6f60` six full vector copies at exact aligned frame addresses.
- `004c6f80` engine RPM: x87 extended return, PC53 main-thread multiplication.
  `004c6fa0` max RPM: FLD extended return. Remaining integer leaves are
  `004c6fb0`, `004c6fc0`, `004c6fd0`, `004c6fe0`, `004c7040`.
- `004cdb70`: common position projection applied to every body, retaining w.
- `004cdbe0`: chassis/wheel angular-velocity clamps, length order (x²+z²)+y²,
  all-lane scaling, and three-lane comparison-mask sum. Chassis is examined
  even if the body's stored count is zero. Original 0x14 aligned frame retained.

`004c7020` already belongs to build_vehicle_wheeled.c and is deliberately not
registered again. `004c6c40` already belongs to vehicle_fourwheel_place.c.

Validation at handoff: C11 and C++17 syntax checks pass. Lead owns serial
oracle/product builds and bit-exact evidence.
