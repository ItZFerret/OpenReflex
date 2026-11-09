# Four-wheel controller publication

`004d9460` (Truck/UTV) and `004be490` (Buggy) are simulation virtual slot23, standard thiscall with one controller index on the stack, RET4. Their 39-byte bodies differ solely in destination offsets. Both call the existing exact `004308a0` controller-input port with EAX=controller and EDI=simulation+1c. They then load both raw 32-bit shift axes from simulation+4c/+50 and store them at Truck/UTV+56f0/+56f4 or Buggy+56e0/+56e4. No float conversion occurs.

The implementation reserves the originals' two saved-register stack slots and uses the exact helper return address. Oracle group `rx_FourWheelControllerBind` explicitly publishes final EAX and ECX from the source fields; the core interface is void because callers consume memory publication. Header: `vehicle_fourwheel_controller_bind.h`.

Source syntax check passes. Restored oracle probes and native-boundary acceptance belong to the integrating agent; this note does not claim runtime acceptance.

## Acceptance (2026-10-21)

Four real vehicle worlds, 600 frames each, Verify/On/reference: each target received 612 positive comparisons (600 natural controller updates plus 12 restored probes), zero mismatches. All 48 restored publication cases reported zero errors, including raw bit-copy values and live controller gate combinations. On/reference state and mapped-memory hashes matched for every class. Evidence: `builds/sol_ground_reset/bind-reference-acceptance.json`.


## Final axis-integrated acceptance (2026-10-21)

`builds/sol_ground_reset/axis-reference-acceptance.json` supersedes the earlier 12-case publication-only evidence for the integrated input path. Each of four 600-frame real vehicle worlds has 616 wrapper comparisons (600 natural plus 16 restored wrapper calls), four direct axis comparisons, and four direct compound-availability comparisons, all with zero mismatches. Every Verify/On/reference run reports 24 restored calls and zero publication/axis errors. The probe covers the original four raw-copy cases, eight live controller gate combinations, and four real-child availability combinations; each added combination also invokes the axis and availability entry points directly. On/reference state and whole-memory hashes match for all classes. Original bindings/device values are retained, and guest memory, added mappings, host allocator and CPU are restored after every probe call. These checks establish controller publication and gated axis dispatch; they do not establish four-wheel driving acceptance.
