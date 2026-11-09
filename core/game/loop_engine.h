/* loop_engine.h -- the engine frame (loop_engine.c): Engine::Update / UpdateTiming, the scene ticker, engine task and job pool
 * worker threads (run synchronously), the job pool. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_engine_update_timing(rx_addr engine);                  /* 0084ea80 Engine::UpdateTiming (ESI = engine) */
uint32_t rx_engine_update(rx_addr engine);                         /* 0084ed20 Engine::Update */
uint32_t rx_engine_frame(rx_addr engine);                          /* 0084f260 Engine::Frame (EAX = engine): Update, render, the tail */
/* the product's render (Engine::Render's place in Engine::Frame): fn(ctx) runs there; the oracle build runs the original render */
void     rx_engine_set_render_hook(void (*fn)(void* ctx), void* ctx);
/* product render boundary: every character entity of the active scenes stamped as drawn this frame (CharacterEntity::vf2_o90's
 * +0x90 + 0x2b0 = [engine + 0x54]); returns how many */
uint32_t rx_engine_render_stamps(rx_addr engine);
void     rx_scene_ticker_run(rx_addr scene);                       /* 007d22a0 Scene_TickerThread: one wake */
uint32_t rx_engine_tasks_run(rx_addr engine);                      /* 0084f320 Engine_TaskThread: one wake (0: thread ends) */
rx_addr  rx_jobpool_worker_run(uint32_t index);                    /* 00881900 JobPool_WorkerThread: one wake (-> the last job fn) */
void     rx_jobpool_submit(rx_addr pool, rx_addr job, rx_addr fn); /* 00881f00 JobPool_Submit */
void     rx_jobpool_wait_idle(rx_addr pool);                       /* 00881e20 JobPool_WaitIdle (ESI = pool) */
/* product: until the next rx_jobpool_wait_idle, submissions queue without running the worker (loop_engine.c, w-lcgdraws) */
void     rx_jobpool_defer_wakes(void);
void     rx_jobpool_recover(rx_addr pool);                         /* 00881d80 JobPool_Recover (stdcall pool; RET 4) */
void     rx_render_sync_job(rx_addr container);                    /* 007e0c50 Render_SyncWorkerJob (cdecl) */
void     rx_render_sync_data(rx_addr container, uint32_t frame);   /* 007e0fc0 Render_SyncRenderData (thiscall; RET 4) */
#ifdef __cplusplus
}
#endif
