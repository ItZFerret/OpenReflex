// loop_engine.cpp -- oracle adapters for core/game/loop_engine.c (group "rx_loop").
// Thread entries (Scene_TickerThread, Engine_TaskThread, JobPool_WorkerThread): the emulator's SetEvent runs a worker from its
// entry until it blocks again on its "go" event (runtime.cpp: the blocking WaitForSingleObject returns pc 0 without popping). The
// core runs one wake; the adapter then leaves the thread as the original leaves it blocked there: ESP at the wait's return slot
// and the callee-saved registers the original holds at that point, and returns pc 0 (the end of the worker's run).
#include "oracle.h"
#include "fx_oracle.h"   /* FxBind: the register model of the dead stack JobPool_Submit leaves (loop_engine.c) */
extern "C" {
#include "../game/loop_engine.h"
}
using namespace reflex_port;

RX_PORT(0x0084ea80, Engine_UpdateTiming, "rx_loop", OUT_FPU) { rx_engine_update_timing(m.r[6]); return m.ret(0); }
RX_PORT(0x0084ed20, Engine_Update, "rx_loop", OUT_EAX | OUT_FPU) { m.r[0] = rx_engine_update(m.arg(0)); return m.ret(4); }
RX_PORT(0x00881f00, JobPool_Submit, "rx_loop", OUT_FPU) { FxBind fb_(m); rx_jobpool_submit(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00881e20, JobPool_WaitIdle, "rx_loop", OUT_FPU) { rx_jobpool_wait_idle(m.r[6]); return m.ret(0); }
RX_PORT(0x00881d80, JobPool_Recover, "rx_loop", 0) { rx_jobpool_recover(m.arg(0)); return m.ret(4); }

// the original blocked in WaitForSingleObject(go, INFINITE): its return address `ra` and the two arguments on the stack at `esp`
static uint32_t blocked(reflex_vm::Machine& m, uint32_t esp, uint32_t ra, uint32_t go) {
    m.mem.write(esp, 4, ra); m.mem.write(esp + 4, 4, go); m.mem.write(esp + 8, 4, 0xffffffffu);
    m.r[4] = esp; m.r[0] = go;
    return 0;
}
// 007d22a0 (scene): blocks at 007d22b9 with ESP = entry - 0x24; EBX = entities ticked (0 when none), ESI = their count,
// EDI = scene, EBP = scene + 0x1c; its local copy of the scaled time sits in its argument slot ([esp + 0x1c]).
RX_PORT_RAW(0x007d22a0, Scene_TickerThread, "rx_loop", 0) {
    const uint32_t entry = m.r[4], scene = m.arg(0);
    rx_scene_ticker_run(scene);
    m.mem.write(entry + 4, 4, m.mem.read(m.mem.read(m.mem.read(0x00a95844, 4) + 0x10, 4) + 0x40, 4));   // the scaled-time local in its argument slot
    const int32_t n = int32_t(m.mem.read(scene + 0x2c94, 4) - m.mem.read(scene + 0x2c90, 4)) >> 2;
    m.r[3] = n > 0 ? uint32_t(n) : 0; m.r[6] = uint32_t(n); m.r[7] = scene; m.r[5] = scene + 0x1c;
    return blocked(m, entry - 0x24, 0x007d22bf, uint32_t(m.mem.read(scene + 0x2ca4, 4)));
}
// 0084f320 (engine): blocks at 0084f339 with ESP = entry - 0x1c; EBX = engine, EBP = the WaitForSingleObject pointer,
// ESI = tasks run (0 when none), EDI = engine + 0x98 + 4 x tasks (unchanged when none). A count of -1 returns 0.
RX_PORT_RAW(0x0084f320, Engine_TaskThread, "rx_loop", OUT_EAX) {
    const uint32_t entry = m.r[4], engine = m.arg(0), r3 = m.r[3], r5 = m.r[5], r6 = m.r[6], r7 = m.r[7];
    const uint32_t count = uint32_t(m.mem.read(engine + 0xc0, 4));
    if (!rx_engine_tasks_run(engine)) {
        m.r[3] = r3; m.r[5] = r5; m.r[6] = r6; m.r[7] = r7;
        m.r[0] = 0; return m.ret(0);
    }
    m.r[3] = engine; m.r[5] = uint32_t(m.mem.read(0x00916008, 4));
    m.r[6] = count; m.r[7] = count ? engine + 0x98 + 4 * count : r7;
    return blocked(m, entry - 0x1c, 0x0084f33b, uint32_t(m.mem.read(engine + 0x90, 4)));
}
// 00881900 (worker index): blocks at 0088197a with EBP = entry - 4, ESP = ((entry - 4) & ~7) - 0x5c; ESI = index, EDI = 1,
// EBX = the last job's function (unchanged when none ran). Quit: signals and returns 0.
RX_PORT_RAW(0x00881900, JobPool_WorkerThread, "rx_loop", OUT_EAX) {
    const uint32_t entry = m.r[4], i = m.arg(0), r3 = m.r[3], r5 = m.r[5], r6 = m.r[6], r7 = m.r[7];
    const uint32_t pool = 0x00d94ae0;
    const uint32_t last_job = rx_jobpool_worker_run(i);
    if (m.mem.read(pool + 0x88c4 + 4 * i, 4) != 0) {
        m.r[3] = r3; m.r[5] = r5; m.r[6] = r6; m.r[7] = r7;
        m.r[0] = 0; return m.ret(0);
    }
    m.r[5] = entry - 4; m.r[6] = i; m.r[7] = 1;
    m.r[3] = last_job ? last_job : r3;
    return blocked(m, ((entry - 4) & ~7u) - 0x5c, 0x00881980, uint32_t(m.mem.read(pool + 0x887c + 4 * i, 4)));
}
// 007e0c50 Render_SyncWorkerJob (cdecl container; runs on a render-sync worker's stack). Void.
RX_PORT(0x007e0c50, Render_SyncWorkerJob, "rx_loop", OUT_FPU) { rx_render_sync_job(m.arg(0)); return m.ret(0); }
// 007e0fc0 Render_SyncRenderData (thiscall container, frame; RET 4). Wakes the four workers (Verify: unverifiable, it switches).
RX_PORT(0x007e0fc0, Render_SyncRenderData, "rx_loop", OUT_FPU) { rx_render_sync_data(m.r[1], m.arg(0)); return m.ret(4); }
