#pragma once
#include "career_start.h"
#include "career_routes.h"
struct rx_session;
/* Internal product composition, NOT a new public/exported general career API.
 * Same session worker, pre-event, one event per session. Caller has initialized
 * the genuine manager/progress/roster and active owned properties. All event,
 * manager and package providers must be real audited implementations.
 *0=actual state3 request accepted,1=terminal/session failure,2=unsupported or
 * incomplete preflight; no rollback of guest work after start is promised. */
int rx_career_session_start_owned(struct rx_session* session,rx_addr manager,
 const rx_career_start_dependencies* start,const rx_career_route_dependencies* routes);
