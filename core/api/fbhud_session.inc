/* fbhud_session.inc -- two port additions of w-fb-hud (playtester feedback), included by rx_session.c (it uses session_ok,
 * session_error, local_player, active_terrain, remap_guard / remap_call and the terrain export cache).
 *
 * CONTROLS IN A RACE (rx_session_remap_apply). rx_session_remap_set takes the bindings only before the event loads: the players'
 * KeyMaps are built from the ControllerRemapping table when the Player List makes them (PlayerSlot_FromPlayerList 0054a7b0 ->
 * Controller_BuildKeyMaps 00547f30), so a table written later changed nothing until the next session. The pause page's OPTIONS >
 * CONTROLS wrote the player's file but the race went on with the old keys (and its RESTART, a new session, took them; the user read that
 * as "a bind changed in game does not save"). rx_session_remap_apply writes the table as rx_session_remap_set does and, once the event is
 * loaded, builds the local human's KeyMaps again with the very call the Player List makes: Controller_BuildKeyMaps(controller, the UI
 * settings row's ControllerPreset +0x18, Handedness +0x30, ControllerMotion +0x2c). KeyMap_SetSlot only writes the elements (no
 * allocation, no draw of the game's generator); each element's analog value starts from 0 (the host calls it while the race is paused).
 *
 * RESET TRACK (rx_session_terrain_reset). The game's own level reset of the deformable terrain, as an activity restart arms it
 * (Activity::vf18 00557de0: the event terrain's +0x44b8 = 1): the next frame's HFTerrainSurface::vf14 (rx_fx_terrain_sync, the
 * session's render boundary) clears both deformation targets to the untouched value, reads the cleared surface back into the grid
 * the physics and the exports read, and resets the three tyre-track sets. The export cache sees the flag and resyncs (the next
 * rx_session_terrain / _tiles reports the whole grid). Nothing else of the restart runs: the riders, the activity, its clock and laps,
 * the mud on the riders and the terrain wetness stay. Any time between frames; the host decides where it is offered (Free Ride,
 * PRACTICE, a race without AI; never online or in a career). */
#include "../game/build_activity_util.h"
#include "../game/build_activity.h"

/* the local human's KeyMaps from the table as it is now (0054a7b0's call); 1 built, 0 no local controller */
static int remap_rebuild_local(void) {
    const rx_addr p = local_player();
    if (!p || U32(p, 4) == 0) return 0;                 /* PlayerLocal */
    const int32_t ctl = I32(p, 0x1f0);                  /* the player's controller */
    if (ctl < 0 || ctl > 3) return 0;
    const rx_addr ui = U32(0, 0x00d92efc);              /* g_UISettings: four device rows of 0x48 bytes at +4 */
    if (!ui) return 0;
    const rx_addr rec = ui + 0x48u * (uint32_t)ctl;
    ACT_FRAME(L, 0, 0);
    uint32_t blk;
    ACT_CALLR(blk, 4, rx_controller_build_key_maps((uint32_t)ctl, U32(rec, 0x18), U32(rec, 0x30), U32(rec, 0x2c)));
    ACT_END(L);
    return blk != 0;
}

int rx_session_remap_apply(struct rx_session* s, const int32_t values[38]) {
    if (!values || !session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (!s->spec_loaded) return rx_session_remap_set(s, values);
    struct remap_call c = {s, NULL, values, 2, -1};
    return remap_guard(&c);
}

int rx_session_terrain_reset(struct rx_session* s) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    const rx_addr t = active_terrain();
    if (!t || !U32(t, 0x240)) { snprintf(s->error, sizeof s->error, "track reset: no deformable terrain"); return 2; }
    SU32(t, 0x44b8, 1);
    terrain_dirty_invalidate(&s->terrain_cache);
    return 0;
}
