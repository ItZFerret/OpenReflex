/* jobthreads_watch_sims.c (w-jobthreads3) -- jobthreads_hash.c's driver (same arguments and output) with the dependence check of the cross-simulation census of the solver loop (marks 10 + u / 9 / 8: world_phys.c).
 * Development only: links against a -DRX_WATCH build of the core objects (every guest memory access reports to rx_watch_access):
 *   RX_BUILD_EXTRA_CFLAGS="-DRX_WATCH -g" bash core/build.sh <out>; then link this file with <out>'s objects (core/plan/QUEUE.md
 *   w-jobthreads3 gives the commands). Run with RX_JOB_THREADS=0 (the in-place order is what is checked). The report goes to stderr.
 jobthreads_hash.c (w-jobthreads) -- one event through the product session API with a fake clock, for the bit-exactness checks of
 * the parallel worker jobs (core/game/loop_jobs.h; core/tools/jobthreads_check.sh runs it per RX_JOB_THREADS mode and compares):
 *   HASH frame <n> all <h> image <h> heap <h> dll <h> stack <h> high <h>   the whole arena every JT_HASH race frames (0: never)
 *   G <n> <h>                                                             JT_POSE=1: every slot's exported rider / bike pose and the game LCG, per frame
 *   PERF frame <n> core <ms>/frame                                         every 300 race frames (rx_session_advance wall time)
 *   jobthreads_hash <install root> <type> <typeId> <level> <locationId> <trackId> <vehicleClass> <duration> <durationType> <riders> <ai>
 *                   <activation|-> <load frames max> <race frames> [keys-held scan codes, comma separated]
 * env JT_EXCL="lo-hi,..." hex ranges left out of every hash, JT_MAP=1 the touched 64 KB chunks of 0x70000000..0x80000000 at the end,
 * JT_DUMP=<race frame>:<file> every non-zero page (u32 address + 4 KB) at that frame (0 = after the load), then exit.
 * Links against the product objects (rx_base): tools/product_link.sh. */
