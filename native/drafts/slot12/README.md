# Private whole rider correction

`reflex_bike_slot12_draft` runs the recovered `00443610` callback, including its grounded, airborne and alternate branches, against one mutable owned state. This is an unregistered record interface; the public Unity plugin ABI is unchanged.

Input is 39,104 bytes: full simulation `0x9700`, control `0x100`, three 12-byte filter records, 148 bytes of address-sorted runtime spans, then raw `dt` and profile words. Output is the same first 39,096 bytes, including every preserved byte. Pointers bind simulation `10000000`, control `20000000`, and filters `30000000`; coherent aliases within those regions retain literal execution order. Input/output buffers may overlap arbitrarily. Errors leave output unchanged and restore the host floating-point environment.

The accepted fresh original proof is `_re/trace/out/original-math-slot12-final-generated/report.json`: 1,000 calls over all four profiles, including 336 grounded, 332 airborne and 332 alternate calls. Both ARM and Intel callable replays match all 9,774,000 original output words per architecture, including 56,904 mutable words. Nine buffer-overlap placements and early/late atomic errors pass. See `_re/trace/out/bike-slot12-native-draft-original/report.json`.

The first 1,000-case model mismatch is retained in `original-math-slot12-generated`. It exposed a missing CL-to-ECX low-byte alias in the isolated interpreter; the corrected model first matched those original bytes offline, then passed a fresh original run. The final native proof binds input/output bytes to the accepted report before replay.

The host `atan2` adapter has tested bit parity against the actual Microsoft VC90 `_CIatan2` import for these cases. It does not claim universal CRT equivalence. The x87 adapter accepts its documented finite bounded representation; whole consecutive gameplay remains a separate composition proof.
