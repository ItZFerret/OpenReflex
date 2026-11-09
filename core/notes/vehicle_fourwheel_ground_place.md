# Four-wheel ground placement

`004cc980` (UTV/Truck) and `004b2180` (Buggy) are simulation virtual slot20, thiscall with five guest pointers: input position, input direction, output position, output forward, output up; RET0x14. Ghidra's final float annotation is incorrect. Authoritative declarations are in `vehicle_fourwheel_ground_place.h`; oracle group `rx_FourWheelGroundPlace`.

The complete 4,366-byte function pair was compared by decompile and disassembly. Instruction streams agree after relocating code addresses and the eight radius loads (four per contact pass): Truck/UTV `+8090`, Buggy `+8080`, stride150. Wheel center rows are `+190` stridea0. Their shared implementation preserves this sole class layout difference. Unlike ATV placement, this pair does not multiply either reset quaternion by the `ad66b0` correction quaternion.

Stack: entry saves EBP, aligns down16, allocates284, pushes three saved registers. Locals retain the resulting guest addresses. Main position c0, quaternion b0, terrain3c, down vector40, normals80, flags90, projected forwarda0, four rays d0 (stride20), ground points150 (stride10), body hits1c0 (stride30). Resets and terrain/body-ray calls use matching guest frames and return addresses; the second active-world getter sees the four already-pushed ray arguments. Output buffers are published only after both complete four-wheel contact sets; the initially horizontalized heading is written before the first reset.

Scalar square-root arguments retain the reference's x87 float load/store NaN conversion. SSE math uses the exact helpers and audited operation associations. Quaternion diagonal selection, tie branches, first/second basis operand order and final up-vector sum order are preserved. No fallback to original code is introduced.

Source-only syntax check passes. Oracle/reference verification and native runtime acceptance remain the integrating agent's responsibility; this note is not an acceptance claim.

## Acceptance (2026-10-21)

All four captured real vehicle worlds passed 600-frame Verify/On/reference checks. Eighteen restored actual-terrain cases each (six headings times below/current/above height) produced 72 positive comparisons total, zero mismatches, and identical On/reference state and full mapped-memory hashes. All cases published the three outputs and completed both wheel placement passes. This does not establish terrain-miss or body-ray-hit branch coverage. Evidence: `builds/sol_ground_reset/ground-reference-acceptance.json`.

The `live.tsv` mask `10005` is the conservative address-taken-function default (`native/game/tools/liveness.py`), not observed return use. Audited vf20 callers `00460020`, `0050dfa0`, `00516a30`, `00518b70`, `00550860`, `0055b610`, `0055d120`, `00561f80`, `0057cbb0`, `0074be10` immediately consume the three vector outputs through `0045f600`; no EAX, EDX, or x87 return is consumed. The void C interface and adapter output mask zero are intentional.
