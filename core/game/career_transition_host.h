#pragma once
#include "career_rules.h"
#ifndef RX_ORACLE
/* Worker-owned internal page context, not part of the public ABI. The only
 * legitimate caller of entered is a complete native AAALoadingPage vf4 owner,
 * after its real entry succeeds. None is enabled in this candidate. */
typedef struct rx_owned_loading_host {void*context;void(*entered)(void*);} rx_owned_loading_host;
void rx_owned_loading_bind(const rx_owned_loading_host*);
void rx_owned_loading_unbind(void*context);
void rx_owned_loading_native_entry_completed(void);
#endif
