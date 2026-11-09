#include "replay_presentation.h"
#include "replay_clip.h"
#include "build_activity_util.h"
#include "scene_fx_entities.h"
#include <string.h>
static rx_replay_presentation replay_host;
int rx_replay_presentation_install(const rx_replay_presentation *host) {
    if(!host||!host->generation||!host->viewport||!host->fade||replay_host.generation)return -1;
    replay_host=*host;return 0;
}
void rx_replay_presentation_clear(void){memset(&replay_host,0,sizeof replay_host);}
/* 00757660 MXGame viewports (stdcall: game, count; RET 8), called by 006aa8d0 with count 1.
 * The original: count 3 becomes 4; 007575a0 releases the viewports' screen effects (render objects at viewport + 0x6c); the
 * renderer's split flag [ctx + 0x10] + 0x5c = (count > 1); when the count (+0x15b24) or the revision (+0x15b2c against +0x14)
 * changed, the three scenes' viewport lists are rebuilt ("Shared Viewport", "ViewPort %d") with their render targets, effect
 * objects and the cameras' aspect, then 00598a60 (UI) and the new count stored; last, 00757490 re-attaches the screen-dust /
 * mud-splatter effects (entities 0x1066 / 0x1067) to the active world's viewports.
 * Product: the viewport list part is the port in scene_viewports.c (the FX distance gate and the replay fade read the list),
 * called exactly as rx_players_sync's product branch does. The render targets, screen effects, split flag and UI page are the
 * renderer's (PRODUCT BOUNDARY, nothing the game reads): the copied request goes to the host presenter (replay_host_bridge.c,
 * ReplayWorker on the Unity side), which applies the single replay viewport and acknowledges it before the next frame. In a
 * single-player session the count is already 1 and the revision unchanged, so the original takes its early return there and
 * the list call changes nothing. */
void rx_replay_require_viewport(rx_addr game,int32_t count) {
#ifdef RX_ORACLE
    const uint32_t args[2]={game,(uint32_t)count};
    act_call_fn(0x00757660,RX_STDCALL,0,2,args);
#else
    const rx_addr sp=rx_sp();
    rx_set_sp(sp-12); /* actual two stack args plus return, once only */
    int32_t views = count;
    if (views == 3) views = 4;                       /* 007576ae: three views are laid out as four */
    /* (007575a0: the viewports' screen-effect render objects released: renderer) */
    /* 007576c2: the engine timing's split flag, read by TerrainTracks_FlushPending (rx_fx_tracks_flush: a quarter ring) */
    SU32(U32(U32(0, 0x00a95844), 0x10), 0x5c, views > 1 ? 1u : 0u);
    rx_scene_viewports_create(game, views > 0 ? (uint32_t)views : 0u);   /* the list part (early return when unchanged) */
    /* (00757490: the screen-dust / mud-splatter effects re-attached to the viewports: renderer) */
    int rejected=!replay_host.generation||!replay_host.viewport
        ||replay_host.viewport(replay_host.context,replay_host.generation,game,count);
    rx_set_sp(sp);
    if(rejected)act_unreached(0x00757660,"owned replay viewport request bridge required");
#endif
}

static void replay_fade_request(uint32_t kind) {
    float duration=F32(0,0x00977654);
    rx_addr context=U32(0,0x00a95844),scene=U32(context,0x1c+4*U32(context,4));
    rx_addr renderer=U32(scene,0x19b8);
    if(!renderer){
#ifdef RX_ORACLE
        return;
#else
        act_unreached(0x006ab6d0,"owned replay requires actual scene view parameters");
#endif
    }
    rx_addr effect=U32(renderer,0x70);
#ifndef RX_ORACLE
    if(!effect)act_unreached(0x006ab6d0,"owned replay existing effect parameters missing");
#endif
    SU32(effect,0,0);effect+=0x10;
    if(!effect)return;
#ifndef RX_ORACLE
    /* Owned renderer currently implements the actual standard-alpha, unmasked
     * path only. 7b75d0 reads effect34/38 (ctor794ca0 block44/48 defaults0).
     * Never force these to0 or acknowledge an unsupported render state. */
    if(U32(effect,0x34)!=0||U32(effect,0x38)!=0)
        act_unreached(kind==2?0x006ab6d0:0x006aaab0,"owned replay unsupported effect blend/mask");
#endif
    /* Same original writes into the EXISTING parameter block in both builds.
     * No synthetic renderer allocation; subsequent FadeIn retains real RGB. */
    SU32(effect,0x30,1);
    if(kind==3) {
        SU32(effect,0x20,U32(0,0x009cadf0));SU32(effect,0x24,U32(0,0x009cadf4));
        SU32(effect,0x28,U32(0,0x009cadf8));SU32(effect,0x2c,U32(0,0x00931f24));
    }
    SF32(effect,0xc,duration);SU32(effect,4,1);SU32(effect,0x10,0);SU32(effect,8,kind);SU32(effect,0x18,0);
#ifndef RX_ORACLE
    uint32_t rgba[4]={U32(0,0x009cadf0),U32(0,0x009cadf4),U32(0,0x009cadf8),U32(0,0x00931f24)};
    /* Explicit product bridge, not a synthesized legacy effect allocation.
     * Session bridge copies this request and blocks the NEXT simulation frame
     * until actual renderer receipts. Teardown clears provider and queue. */
    if(!replay_host.generation||!replay_host.fade||replay_host.fade(replay_host.context,replay_host.generation,kind,duration,kind==3?rgba:NULL))
        act_unreached(kind==2?0x006ab6d0:0x006aaab0,"owned replay fade request bridge required");
#endif
}
void rx_replay_fade_in(void){replay_fade_request(2);}
RX_FUNCTION(0x006ab6d0,rx_replay_fade_in)
void rx_replay_fade_out(void) {
    rx_addr clip=0x00d323c0+U32(0,0x009c4070)*0x35d0;
    float threshold=rx_subs(rx_subs(F32(clip,0x35b4),F32(clip,0x35b0)),F32(0,0x00977654));
    if(F32(clip,0x35bc)>threshold)replay_fade_request(3);
}
RX_FUNCTION(0x006aaab0,rx_replay_fade_out)
