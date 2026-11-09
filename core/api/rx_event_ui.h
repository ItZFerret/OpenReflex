/* Internal scalar reader, separate from session lifetime/slot lookup. */
#pragma once
#include "rx_api.h"
#include "../src/rx_mem.h"
void rx_event_ui_snapshot(rx_addr activity, rx_event_ui_state* out);
