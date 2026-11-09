# Private complete contact-response draft

This isolated executable models the original `004360b0` contact-response parent
and executes `00402dc0`, `0075c580`, and `0042bd90` from the pinned instruction
program. It does not register a public native API, modify Unity, or implement the
rest of simulation slot15.

Both ARM and Intel compare exactly against 1,000 original generated calls,
including nonidentity body orientations, object and terrain contacts, event
queue capacity99/100, absent bindings, and events already emitted. The generated
set contains 31,636 affected output words and 11,463,000 complete record words;
affected words include writes that leave the existing value unchanged. Both
architectures also match the earlier 32-call pilot. Evidence is
`_re/trace/out/bike-contact-response-native-draft-original/report.json`.

Input and output are complete 45,852-byte records described by
`_re/trace/bike_contact_response_layout.json`: simulation `0x9700`, six runtime
spans totaling48 bytes, then sixteen explicit external regions. The graph has
real scene selection flags, world registry identity and metadata, two selected
scene objects, a100-record event queue, a contact binding/handle/owner, and three
`0xe0` indexed records. The queue is the only mutable external region. There is
no supplied selector result or fabricated event callback; all three original
helpers execute. An invalid original vector-index path is rejected explicitly.

This draft keeps shared raw memory and original instruction order. Its bounded
profile supports1..10 bodies, up to12 supplied object contacts and25 supplied
terrain contact entries, and the fixed explicit graph described above. Those
are the harness profile bounds, not a claim about every original constructor or
all possible contact layouts. Unmapped reads and unowned writes fail. The
compiler uses separate binary32 operations with contraction/fast math disabled.
The closure contains no x87 or CRT calls on accepted inputs; copied x87 cases in
the private interpreter switch are unreachable through the pinned program.

`verify.py` checks the actual input/original hashes against each accepted oracle
report, pins source/vector bytes before and after, compiles both architectures,
and compares every final byte. Generated original cases remain independent
calls; this does not establish live contact or consecutive-step equivalence.
