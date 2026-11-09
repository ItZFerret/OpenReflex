# Continuous dirt-bike test runtime

This isolated library composes the recovered bike controls, forces, mechanism
solve, rider correction, contact response, pose updates and real terrain contact
generation. Each update starts from the previous computed state. The seed supplies
configuration and one initial pose; there is no sequence of motion frames.

The canonical `reflexcore` ABI and installed library are unchanged. This private
`reflexbike` API exists for the first controllable Unity test. It does **not**
establish complete original-game equivalence or a finished port.

## Current world and limits

- Four dynamic bodies: bike, rider, front wheel, rear wheel; all twelve original
  body probes and real wheel contacts remain active.
- Explicit flat 32×32 grid, spacing 10, width/depth 310 native units, base height
  zero, deformation 0.5 with zero gain, and zero coarse ray bounds.
- Unity resets outside ±140 because the original body and wheel contact paths
  exclude outer grid strips. This is a finite test pad, not recovered Alpine
  collision. Moving objects and dynamic collision descriptors are explicitly
  absent.
- Two fixed substeps of 0.0075 seconds per 0.015-second world update; poststep runs
  once per world update. No recorded callback results are injected.
- Keyboard controls are sampled into the recovered input fields. Stunt and manual
  shift pulses are zero. The original brake-hold latch is preserved.
- A failed update retains the complete previous context. Unity stops and offers
  reset; complete crash/recovery behavior remains unvalidated.
- Rider rendering is proxy art. Do not describe it as a recovered animated rider.

## Evidence

`_re/trace/out/bike-runtime-continuous/arm64.json` compares every full 38,656-byte
state after 1,520 world updates / 3,040 substeps with the independent Python
composition. Eight scenarios cover idle, throttle, front brake, rear brake,
opposite steering and opposite rider lean. Their states diverge with input, and
all remain inside the test boundary. The Intel report matches every state hash.
The Python composition calls the separately validated native stage kernels but
uses independent Python pose bridges and core state marshalling. This checks the
composition/adapters, not a whole-step original-executable oracle.

`_re/trace/out/bike-runtime-contracts/report.json` pins the tested source and
libraries. ARM and Intel each pass 522 successful updates, 42 rejected malformed
seeds, deterministic fresh-context runs, full-state/frame atomic failures and
recovery, all four numerical profiles, and preservation of floating-point
controls and exception flags. `contract_test.cpp` is the reusable test runner.

Stage-specific original executable evidence is retained separately for controls,
ray queries, slot12, slot13, slot15 and the mechanism. The five step bridges match
549 live original observations each. The whole integration still needs an
original end-to-end comparison over a shared constructed world.

The earlier `bike-runtime-connected-first.log` is intentionally retained as a
failed boundary experiment: it matched 297 world updates, then exceeded the pad
and stopped on a guarded pose calculation. It is not a passing 300-update test.

## Rebuild and verify

Run from the repository root:

```sh
_re/.venv/bin/python native/drafts/bike_runtime/build_seed.py
_re/.venv/bin/python native/drafts/bike_runtime/build.py arm64
_re/.venv/bin/python native/drafts/bike_runtime/build.py x86_64
_re/.venv/bin/python native/drafts/bike_runtime/verify_composition.py --library native/drafts/bike_runtime/libreflexbike-arm64.dylib --output _re/trace/out/bike-runtime-continuous/arm64.json
arch -x86_64 /usr/bin/python3 native/drafts/bike_runtime/verify_composition.py --library native/drafts/bike_runtime/libreflexbike-x86_64.dylib --expected _re/trace/out/bike-runtime-continuous/arm64.json --output _re/trace/out/bike-runtime-continuous/x86_64.json
```

The combined library contains all stage implementations and links only system
libraries. Package both architectures with `lipo` and ad-hoc sign the result.
Install as `Unity/Reflex/Assets/Plugins/macOS/libreflexbike.dylib`; seed location is
`Unity/Reflex/Assets/StreamingAssets/bike-test-seed.bin`. Quit/restart Unity before
replacing a version already loaded by that process.

The public header documents the 44-byte input and 156-byte frame. The diagnostic
state export exists for validation. The outer native call saves/restores the
caller's floating-point environment and rejects unsupported rounding controls.
