/* The product's owner of the career branch of MXUIPostEventOverlay's "Show Activity Results" handler 005f42e0 (the in-game HUD
 * page's PostEventOverlay.lyr, vf3 registers it): in tour mode the overlay sets the current series event's +0x14 = the activity and
 * raises "AdvancePostEvent" on the series manager's channel, whose handler (00592b20) runs the event's vf13 (standings, points,
 * SeriesEventStatus). The product has no UI pages, so this file does exactly that when the original's typed event is raised.
 * Not the complete overlay owner (the page itself is the presenter's); deliberately not registered as 005f42e0. Product only. */
#include "career_owned_results.h"
#ifndef RX_ORACLE
#include "career_properties.h"
#include "build_activity_util.h"
#include "race.h"
#include "fx.h"
#include "world_misc.h"

/* The vtables of the series event classes the product composes (career_composition.c). */
#define EVENT_MOTO_VTABLE   0x0094e2fcu
#define EVENT_SINGLE_VTABLE 0x0094e36cu
#define EVENT_HEAT_VTABLE   0x0094e28cu

static rx_addr owning_manager;

void rx_career_owned_results_bind(rx_addr manager) {
    if (!manager || owning_manager || !rx_owned_properties_active() || manager != rx_fx_series_manager())
        act_unreached(0x5f42e0, "owned career results bind requires actual manager/lifetime");
    owning_manager = manager;
}

void rx_career_owned_results_clear(void) {
    owning_manager = 0;
}

/* The activity's Post sub-state is 2, the results (MXActivityRace / MXActivitySectionRace +0x13e640, MXActivityWaypointRace
 * +0x13e618, MXActivityFreestyle +0x13e5f8). 0 for another class. */
static uint32_t post_results_shown(rx_addr activity) {
    switch (U32(activity, 0)) {
    case 0x00943554:
    case 0x00944a74:
        return U32(activity, 0x13e640) == 2;
    case 0x00943c14:
        return U32(activity, 0x13e618) == 2;
    case 0x009442cc:
        return U32(activity, 0x13e5f8) == 2;
    default:
        return 0;
    }
}

static void observe(rx_addr activity) {
    const rx_addr manager = rx_fx_series_manager();
    if (manager != owning_manager || !rx_owned_properties_active()) act_unreached(0x5f42e0, "stale owned career result binding");
    /* 005f437c: the career branch runs in Tour (1) or Arcade (2) mode */
    if (U32(manager, 0x190) != 1 && U32(manager, 0x190) != 2) act_unreached(0x5f437c, "owned result requires Tour or Arcade");
    /* The current event must be one of the composed classes: the Single event (every Single-class format), the Moto event in
     * its practice (phase 1) or one of its two motos (6, 7), or the Heat event in its practice (1), heat (3), LCQ (4) or main event
     * (5). */
    const rx_addr event = U32(manager, 0x18c);
    if (!event) act_unreached(0x5f448a, "owned result without a current series event");
    const uint32_t vtable = U32(event, 0);
    if (vtable != EVENT_SINGLE_VTABLE && vtable != EVENT_MOTO_VTABLE && vtable != EVENT_HEAT_VTABLE)
        act_unreached(0x5f448a, "owned result supports the Single, Moto and Heat events");
    if (vtable == EVENT_MOTO_VTABLE) {
        const uint32_t phase = U32(event, 0xc);
        if (U32(manager, 0x10) != 0 || U32(manager, 0x190) != 1 || (phase != 1 && phase != 6 && phase != 7))
            act_unreached(0x5f448a, "Moto result source is not a loaded Moto series' practice or moto");
    }
    if (vtable == EVENT_HEAT_VTABLE) {
        const uint32_t phase = U32(event, 0xc);
        if (U32(manager, 0x10) != 1 || U32(manager, 0x190) != 1 || (phase != 1 && phase != 3 && phase != 4 && phase != 5))
            act_unreached(0x5f448a, "Heat result source is not a loaded Heat series' practice, heat, LCQ or main event");
    }
    /* The activity is the current local player's and in its results Post sub-state; it is the race the event set up (the type it
     * recorded, +0x18, and the mode it requested, +0x20). */
    const rx_addr game = U32(0xa946b8, 0);
    uint32_t local_match = 0;
    if (game) {
        for (uint32_t slot = 0; slot < 4; slot++) local_match |= U32(game + 0x7460 + slot * 0x12a0, 0x94) == activity;
    }
    if (!activity || !local_match || !post_results_shown(activity) || U32(activity, 0x21c) != 4 ||
        U32(activity, 0x23c) != U32(event, 0x18) || U32(activity, 0x238) != U32(event, 0x20))
        act_unreached(0x5f4494, "owned career result source is not current genuine post activity");
    const rx_addr channel = U32(manager, 0x180);
    if (!channel) act_unreached(0x5f449f, "career result channel not bound");
    /* 005f42ff..4334: these four keys are erased before the career dispatch */
    rx_owned_property_erase(0x94d028);
    rx_owned_property_erase(0x94d030);
    rx_owned_property_erase(0x94d038);
    rx_owned_property_erase(0x94d040);
    SU32(event, 0x14, activity);                                  /* 005f4497, only at the real result emission */
    race_raise_event(channel, 0x94d71c, 0x9cf3f0, NULL, 0);       /* "AdvancePostEvent" */
}

/* The receiver of the original typed "Show Activity Results" (void(Activity*), a 4-byte payload) after the canonical dispatch
 * 004020c0 (world_veh_entity.c): every activity class raises it there (MXActivityRace and MXActivitySectionRace through 00559a40
 * in race_finish.c, MXActivityWaypointRace at its state-2 tail 0057a500, MXActivityFreestyle in 00563aa0), so this is the only
 * consumer; the registered handler's signature must be the original's. Exhibition events have no owner and return. */
void rx_career_owned_results_dispatch(rx_addr channel, rx_addr name, rx_addr payload, uint32_t bytes) {
    if (!owning_manager || !name || crt_strcmp(name, 0x941c30) || bytes != 4 || !payload) return;
    const rx_addr activity = U32(payload, 0);
    if (!activity || channel != U32(activity, 0x290)) return;
    rx_addr handler;
    ACT_CALLR(handler, 2, rx_world_registry_find(channel, name));
    const rx_addr type = crt_type_info_name(0x9cbd24, 0x9e1a7c);
    if (!handler || !U32(handler, 4) || crt_strcmp(U32(handler, 4), type))
        act_unreached(0x5f42e0, "Show Activity Results event signature mismatch");
    observe(activity);
}
#endif
