#pragma once
#include "career_host_queue.h"
/* Worker-only. All providers are required, actual host services, not defaults.
 * copy_name validates and copies a guest string, including NUL within capacity.
 * route_available validates the actual named route against installed resources.
 * fade_complete dispatches the real event synchronously on the owning worker. */
typedef struct rx_career_presentation_services {
 void *context;
 uint32_t (*copy_name)(void*,uint32_t,char*,size_t);
 uint32_t (*route_available)(void*,const char*);
 void (*fade_complete)(void*);
 /* Synchronous worker dispatch. The name is a transient copied host string. */
 void (*page_wipe_transition)(void*,const char*);
 void (*page_wipe_complete)(void*,const char*);
} rx_career_presentation_services;
typedef struct rx_career_presentation {
 rx_career_host_queue *queue; /* persistent external host storage */
 uint64_t generation;
 rx_career_presentation_services services;
 uint32_t active,dispatching;
} rx_career_presentation;
rx_career_host_result rx_career_presentation_begin(rx_career_presentation*,rx_career_host_queue*,const rx_career_presentation_services*);
rx_career_host_result rx_career_presentation_end(rx_career_presentation*,uint32_t abort_pending);
uint32_t rx_career_presentation_ui_event(void*,uint32_t);
uint32_t rx_career_presentation_ui_route(void*,uint32_t);
uint32_t rx_career_presentation_hub(void*);
uint32_t rx_career_presentation_route_accept(void*,uint32_t);
uint32_t rx_career_presentation_checkpoint(void*,uint32_t);
rx_career_host_result rx_career_presentation_fade(rx_career_presentation*,uint32_t seconds_bits,uint32_t color);
rx_career_host_result rx_career_presentation_page_wipe(rx_career_presentation*,uint32_t guest_name);
/* Reports an actual renderer milestone; never called by a timer or enqueue.
 * Caller starts Out before reporting In and hides before reporting Out, matching
 * PageWipe.lua. Requests and commands are copied across the worker boundary. */
rx_career_host_result rx_career_presentation_wipe_milestone(rx_career_presentation*,
 uint64_t generation,uint64_t id,uint32_t kind,const char*name,uint32_t milestone);
rx_career_host_result rx_career_presentation_next(rx_career_presentation*,uint64_t generation,rx_career_host_request*);
rx_career_host_result rx_career_presentation_ack(rx_career_presentation*,uint64_t generation,uint64_t id,uint32_t kind,uint32_t accepted);
