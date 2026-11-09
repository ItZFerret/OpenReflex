/* crt_atexit.h -- the CRT's _atexit (0x008f9fbf) at the call sites of the game's function-local statics (the guard, the constructor, then
 * _atexit(destructor)). The core never exits (the host unloads the library), so the product registers nothing (build_crt_atexit.c keeps the
 * address registered for callers that reach it through the registry). The oracle build runs the original (msvcr90 keeps the table in its own
 * data, 0x785bd660), so Verify compares clean. */
#pragma once
#include "rx_game.h"

#ifdef RX_ORACLE
#define RX_ATEXIT(fn) do { uint32_t a_[1] = {(fn)}; rx_call_original(0x008f9fbf, RX_CDECL, 0, 1, a_); } while (0)
#else
#define RX_ATEXIT(fn) ((void)(fn))
#endif
