# Private tire consumer draft

This draft reproduces the bounded `00447f10` tire/contact force consumer by
interpreting pinned instruction data with native, separately rounded float and
double operations. It is separate from the registered Reflex library and Unity
bindings. Contact generation and a continuous bike update are outside its scope.

`program.inc` is generated from the reviewed non-executable JSON reference data.
The native implementation keeps one mutable simulation image for each call,
including the contact block embedded in that image. All stack/register/vector
lane operations follow the original sequence. It explicitly implements SSE
NaN selection, signed zeros, masks and lane packing. The bounded x87 indexed
curve additions use integer significand alignment and rounding to24 or53 bits,
avoiding double-rounding through the host double type.

Run `python3.14 native/drafts/tire/verify.py` from the project root to compile
ARM and Intel binaries and compare the frozen original results. The final proof
is `_re/trace/out/bike-tire-native-draft-original/report.json`: both architectures
match1,256 original calls and46,824 mutable output words, as well as all captured
preserved simulation bytes. Six directed arithmetic checks pass per architecture.
The two source vectors are the accepted inactive256 and active1,000 original-code
cases; each numerical case is repeated under four original precision/conversion
profiles. Replays perform no Wine or game launch.

The native FP environment must already be nearest-even with gradual underflow
and masked exceptions; unsupported controls are rejected. This standalone draft
does not offer an atomic public C API or claim original exception-status parity.
Its generated branch coverage comes from the separately verified reference,
not instrumented branch observation in the original program. The x87 conversion,
copy-sign and exponent-two pow adapters have the same bounded scope described in
`_re/notes/bike-tire-contact.md`.

An independent reviewer checked the exact x87 adapter, mutable alias behavior
and environment guard. Full original output comparisons passed on both CPUs.
A separate complete opcode-by-opcode review of this native interpreter has not
been claimed. The next engineering step can replace its instruction dispatch
with compact typed math while retaining these immutable input/output vectors.
