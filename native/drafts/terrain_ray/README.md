# Private terrain segment query

This draft ports original7c6cc0 plus its coarse/fine traversal and bilinear intersection helpers over explicitly supplied base heights and coarse bounds. It is unregistered and has no public ABI or Unity changes.

`reflex_terrain_ray_draft(input,16624,output,36)` stages the entire output and supports arbitrary input/output overlap. Return0 is success,1 malformed argument/size,2 unsupported layout/numerical/FP domain; failed calls preserve output. The input includes the original runtime globals and conversion/x87 profile. It contains no hit/miss response field.

ARM64 and x86-64 each match1,000 unchanged original calls (250 numerical inputs×4profiles),496hits/504misses,9,000 complete output words. Each passes nine arbitrary overlaps and analytical/atomic error cases. Native report: `_re/trace/out/terrain-ray-native-draft-original/report.json`.

The exact record, source boundaries, precision rules and live-binding limitations are documented in `_re/notes/terrain-ray-original-math.md`. Actual live terrain selection/deformation and complete bike physics remain outside this private proof.
