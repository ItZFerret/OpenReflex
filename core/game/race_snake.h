/* race_snake.h -- the Snake (MXActivitySnake, activity type 0xa) and Infection (MXActivityInfection, type 0xe) party modes, their
 * drivers, rules and audio logic, and the render-side objects they build (the boundary wall XenonSnakeWallVisibleMesh, the trails'
 * XenonSnakeVisibleMesh). Files: race_snake_wall.c (the wall, shared), race_infection.c (Infection), race_snake.c (Snake),
 * race_snake_driver.c (SnakeDriver, RuleOutOfSnakeBounds), race_snake_trail.c (the trails). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- the wall and its render-side helpers: race_snake_wall.c ---- */
rx_addr  rx_float_image_alloc(rx_addr img, uint32_t w, uint32_t h);              /* 00554e50 FloatImage_Alloc (ESI = img, EAX = w, ECX = h) */
void     rx_texture_asset_create_argb(rx_addr asset, uint32_t w, uint32_t h);    /* 007b3380 (ESI = asset; stdcall w, h; RET 8) */
rx_addr  rx_texture_from_float_image(rx_addr img, uint32_t bits);               /* 006a1c10 (cdecl image, bits) -> the TextureAsset */
void     rx_shader_asset_vertex_shader(rx_addr asset, rx_addr name, rx_addr out, rx_addr out2);   /* 007b12a0 (stdcall; RET 0x10) */
void     rx_shader_asset_pixel_shader(rx_addr asset, rx_addr name, rx_addr out, rx_addr out2);    /* 007b1330 (stdcall; RET 0x10) */
void     rx_snake_wall_shaders(rx_addr m);                                       /* 0057efb0 (stdcall mesh; RET 4) */
rx_addr  rx_snake_wall_construct(rx_addr m, rx_addr color, uint32_t segments);   /* 0057ebb0 (stdcall mesh, colour, segments; RET 0xc) */
void     rx_snake_wall_destroy(rx_addr m);                                       /* 0057f020 (ESI = mesh) */
rx_addr  rx_snake_wall_sdtor(rx_addr m, uint32_t flags);                         /* 0057ef90 slot 0 (RET 4) */
rx_addr  rx_snake_wall_sdtor_o90(rx_addr r, uint32_t flags);                     /* 00592b70 IRenderable slot 0 (ECX = mesh + 0x90) */
void     rx_snake_mesh_scene_add(rx_addr e, rx_addr scene);                      /* 006a0e20 (EAX = entity, ESI = scene) */
void     rx_rotate_xz(float angle, float x, float z, rx_addr out_x, rx_addr out_z);   /* 0055fba0 (xmm1 = angle; stack x, z; ECX / EDX = out) */
uint32_t rx_snake_wall_build_ring(rx_addr m, rx_addr centre, float radius, float width);   /* 0057fc00 (EDI = mesh, EAX = centre; stdcall; RET 8) */

/* ---- MXActivityInfection (0xff1f0 bytes, vtable 0x00944c74): race_infection.c ----
 * Fields added by the class (Activity ends at +0xff070):
 *   +0xff070  the round's start zone (MiniGameStartZone)   +0xff074  the score of an infection (100 / (riders - 1))
 *   +0xff078  the score per second of survival (100 / the activity's +0x2bc)   +0xff07c  the last driver infected
 *   +0xff080  the round (0 ..)   +0xff084  the rounds to win (4; the activity row's NumLaps is not read)   +0xff088  the match is over
 *   +0xff08c  the round's (match's) winner   +0xff090  the wind-down timer (3 s)   +0xff094  a new match
 *   +0xff098  the boundary's radius (the start zone's SnakeAreaRadius, shrinking by BoundaryShrinkRate / 2 per second)
 *   +0xff0a0  the boundary's centre (the start zone's position)   +0xff0b0  BoundaryShrinkRate   +0xff0b4  the radius send timer (online)
 *   +0xff0b8  BoundaryWallPolyWidth   +0xff0c0  the boundary wall (XenonSnakeWallVisibleMesh, 200 segments)   +0xff180  the radius sent
 *   +0xff1e0  the network snake object (online; 0 offline)
 * InfectionDriver (0x11c0 bytes, vtable 0x0093feb4: a NetworkDriver): +0xf10 the activity, +0xf14 infected (byte), +0xf18 the rounds won,
 * +0xf1c the score, +0xf20 a PlaceableParticleEffect ("Cloud_Fire": the infected rider's smoke; +0xf30 the scene, +0xf34 its id),
 * +0xf80 the vehicle's position. RuleInfection (0x10 bytes, vtable 0x0093ff00): +4 the driver, +8 enabled, +0xc the infection radius^2
 * (144). */
rx_addr  rx_infection_construct(rx_addr act, uint32_t type, rx_addr world);      /* 00572d00 (stack act; ECX = type, EAX = world; RET 4) */
rx_addr  rx_infection_sdtor(rx_addr act, uint32_t flags);                        /* 00572e70 slot 0 (RET 4) */
void     rx_infection_vf2(rx_addr act, rx_addr arg);                             /* 00572eb0 slot 2 (RET 4) */
void     rx_infection_vf3(rx_addr act);                                          /* 00573080 slot 3 */
void     rx_infection_vf4(rx_addr act);                                          /* 00573140 slot 4 */
void     rx_infection_vf15(rx_addr act);                                         /* 00573180 slot 15 */
void     rx_infection_vf16(rx_addr act);                                         /* 00573330 slot 16 */
void     rx_infection_vf18(rx_addr act);                                         /* 00573ab0 slot 18 */
void     rx_infection_vf23(rx_addr act, float dt);                               /* 00573d90 slot 23 (RET 4) */
void     rx_party_vf25(rx_addr act, float dt);                                   /* 00573f90 slot 25 (MXActivityCapture's; RET 4) */
void     rx_infection_vf27(rx_addr act, float dt);                               /* 00574060 slot 27 (RET 4) */
void     rx_infection_vf32(rx_addr act, uint32_t next);                          /* 00574110 slot 32 (RET 4) */
void     rx_infection_vf40(rx_addr act, uint32_t old_state);                     /* 00573b80 slot 40 (RET 4) */
void     rx_infection_vf42(rx_addr act);                                         /* 00574140 slot 42 */
int32_t  rx_infection_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);     /* 00573b20 slot 43 (RET 0xc) */
rx_addr  rx_infection_vf44(rx_addr act, rx_addr player);                        /* 00573af0 slot 44 (RET 4) */
void     rx_infection_vf47(rx_addr act, rx_addr driver);                         /* 00574290 slot 47 (RET 4) */
void     rx_infection_place_round(rx_addr act);                                  /* 00573450 (stdcall act; RET 4) */
void     rx_infection_boundary_reset(rx_addr act);                               /* 00574370 (ESI = act) */
void     rx_infection_boundary_shrink(rx_addr act, float dt);                    /* 005744c0 (EDI = act; stdcall dt; RET 4) */
void     rx_infection_net_removed_listen(rx_addr channel, rx_addr act);          /* 00589bd0 (EAX = channel; stdcall act; RET 4) */
void     rx_infection_net_boundary_listen(rx_addr channel, rx_addr act);         /* 00589c90 */
void     rx_infection_net_removed_unlisten(rx_addr channel, rx_addr act);        /* 00589d50 */
void     rx_infection_net_boundary_unlisten(rx_addr channel, rx_addr act);       /* 00589db0 */
void     rx_party_player_wins(rx_addr manager, rx_addr winner, rx_addr text, rx_addr act);   /* 00589e10 (ESI = manager, ECX = winner, EDX = text; stdcall act; RET 4) */
rx_addr  rx_infection_rule_create(rx_addr driver);                               /* 00589e70 (ECX = driver) */
rx_addr  rx_infection_driver_construct(rx_addr d, rx_addr act, rx_addr player);  /* 00515690 (ECX = driver, EAX = player; stdcall act; RET 4) */
rx_addr  rx_infection_driver_sdtor(rx_addr d, uint32_t flags);                   /* 005156e0 slot 0 (RET 4) */
void     rx_infection_driver_vf1(rx_addr d);                                     /* 00515720 slot 1 */
void     rx_infection_driver_vf2(rx_addr d);                                     /* 005158c0 slot 2 */
void     rx_infection_driver_vf3(rx_addr d);                                     /* 005159d0 slot 3 */
void     rx_infection_driver_vf7(rx_addr d, float dt);                           /* 00515b60 slot 7 (RET 4) */
void     rx_infection_driver_vf9(rx_addr d, rx_addr event);                      /* 00515df0 slot 9 (RET 4) */
void     rx_infection_driver_human(rx_addr d);                                   /* 00515d70 (EAX = driver) */
void     rx_infection_driver_ai(rx_addr d, float dt);                            /* 00516120 (ESI = driver; stdcall dt; RET 4) */
rx_addr  rx_driver_rule_kind9(rx_addr d);                                        /* 0053b0d0 (stdcall driver; RET 4): the first rule of kind 9 */
void     rx_infection_driver_listen(rx_addr channel, rx_addr d);                 /* 0053b170 (EAX = channel; stdcall driver; RET 4) */
void     rx_infection_driver_unlisten(rx_addr channel, rx_addr d);               /* 0053b230 */
void     rx_infection_rule_vf1(rx_addr rule, rx_addr driver);                    /* 0051c880 slot 1 (RET 4) */
uint32_t rx_infection_rule_vf5(rx_addr rule, float dt);                          /* 0051c8a0 slot 5 (RET 4) */
void     rx_infection_audio_vf3(rx_addr logic);                                  /* 00584460 */
void     rx_infection_audio_vf4(rx_addr logic);                                  /* 00584670 */

/* ---- RuleOutOfSnakeBounds (0x60 bytes, vtable 0x0093fe00, kind 9) and the boundary tests: race_snake_rules.c ---- */
rx_addr  rx_snake_bounds_rule_create(rx_addr d);                                 /* 00589120 (ECX = driver) */
void     rx_snake_bounds_rule_vf1(rx_addr rule, rx_addr driver);                 /* 0051bb30 slot 1 (RET 4) */
void     rx_snake_bounds_rule_vf4(rx_addr rule, uint32_t on);                    /* 0051bb50 slot 4 (RET 4) */
void     rx_snake_bounds_rule_vf5(rx_addr rule, float dt);                       /* 0051bb70 slot 5 (RET 4) */
uint32_t rx_snake_out_of_bounds(rx_addr d, rx_addr act);                         /* 0056bf20 (EAX = driver, ECX = activity) */
uint32_t rx_infection_out_of_bounds(rx_addr d, rx_addr act);                     /* 00574300 (EAX = driver, ESI = activity) */
void     rx_infection_audio_listen_round_started(rx_addr channel, rx_addr logic);                         /* 0058cb70 */
void     rx_infection_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058cc30 */
void     rx_infection_audio_listen_eliminated(rx_addr channel, rx_addr logic);                            /* 0058ccf0 */
void     rx_infection_audio_listen_player_wins(rx_addr channel, rx_addr logic);                           /* 0058cdb0 */
void     rx_infection_audio_unlisten_round_started(rx_addr channel, rx_addr logic);                       /* 0058ce70 */
void     rx_infection_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058ced0 */
void     rx_infection_audio_unlisten_eliminated(rx_addr channel, rx_addr logic);                          /* 0058cf30 */
void     rx_infection_audio_unlisten_player_wins(rx_addr channel, rx_addr logic);                         /* 0058cf90 */
void     rx_infection_audio_infected(rx_addr logic, rx_addr act, rx_addr driver);                         /* 00584880 (RET 8) */

/* ---- MXActivitySnake (0xff220 bytes, vtable 0x00944884): race_snake.c ----
 * Fields added by the class (Activity ends at +0xff070):
 *   +0xff070  the trails' spatial hash (race_snake_trail.c; +0x14 its buckets, +0x28 0)   +0xff09c  the rounds played
 *   +0xff0a0  the rounds to win (the activity row's NumLaps)   +0xff0a4  the match is over   +0xff0a8  the round's (match's) winner
 *   +0xff0ac  the wind-down timer (3 s)   +0xff0b0  a new match   +0xff0b4  team mode (never set offline)   +0xff0b8 / +0xff0bc  the
 *   teams' round wins   +0xff0c0  the last team (-1)   +0xff0c4  the NetSnake object (online; 0 offline)   +0xff0c8  the host's round
 *   end (online)   +0xff0cc  the round's start zone   +0xff0d0  UseWaterAsBoundary of the zone   +0xff0d4 / +0xff0d8  the water's rise
 *   clock / time (60 s)   +0xff0dc  the boundary's radius (300; the zone's SnakeAreaRadius)   +0xff0e0  its centre   +0xff0f0
 *   BoundaryShrinkRate   +0xff0f4  the radius send timer (online)   +0xff0f8  BoundaryWallPolyWidth   +0xff100  the boundary wall
 *   (XenonSnakeWallVisibleMesh, 250 segments)   +0xff19c  not water   +0xff1c0  the radius sent
 * SnakeDriver (0xfe0 bytes, vtable 0x0093fdb4): race_snake_driver.c's header. */
rx_addr  rx_snake_construct(rx_addr act, uint32_t type, rx_addr world);          /* 0056b050 (stack act; ECX = type, EAX = world; RET 4) */
void     rx_snake_destroy(rx_addr act);                                          /* 0056b230 (EDI = act) */
rx_addr  rx_snake_sdtor(rx_addr act, uint32_t flags);                            /* 0056b210 slot 0 (RET 4) */
void     rx_snake_vf1(rx_addr act, rx_addr ref);                                 /* 0056d340 slot 1 (RET 4) */
void     rx_snake_vf2(rx_addr act, rx_addr arg);                                 /* 0056b310 slot 2 (RET 4) */
void     rx_snake_vf3(rx_addr act);                                              /* 0056bc00 slot 3 */
void     rx_snake_vf4(rx_addr act);                                              /* 0056b530 slot 4 */
void     rx_snake_vf5(rx_addr act);                                              /* 0056b2b0 slot 5 */
void     rx_snake_vf6(rx_addr act, uint32_t flag);                               /* 0056b2d0 slot 6 (RET 4) */
void     rx_snake_vf15(rx_addr act);                                             /* 0056ce80 slot 15 */
void     rx_snake_vf16(rx_addr act);                                             /* 0056c200 slot 16 */
void     rx_snake_vf18(rx_addr act);                                             /* 0056bd60 slot 18 */
void     rx_snake_vf23(rx_addr act, float dt);                                   /* 0056bfd0 slot 23 (RET 4) */
void     rx_snake_vf25(rx_addr act, float dt);                                   /* 0056c460 slot 25 (RET 4) */
void     rx_snake_vf26(rx_addr act, float dt);                                   /* 0056c530 slot 26 (RET 4) */
void     rx_snake_vf27(rx_addr act, float dt);                                   /* 0056c700 slot 27 (RET 4) */
void     rx_snake_vf32(rx_addr act, uint32_t next);                              /* 0056bec0 slot 32 (RET 4) */
void     rx_snake_vf34(rx_addr act, uint32_t arg);                               /* 0056caa0 slot 34 (RET 4) */
void     rx_snake_vf39(rx_addr act, uint32_t old_state);                         /* 0056c550 slot 39 (RET 4) */
void     rx_snake_vf40(rx_addr act, uint32_t old_state);                         /* 0056c5c0 slot 40 (RET 4) */
void     rx_snake_vf42(rx_addr act);                                             /* 0056cae0 slot 42 */
int32_t  rx_snake_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);         /* 0056be60 slot 43 (RET 0xc) */
rx_addr  rx_snake_vf44(rx_addr act, rx_addr player);                            /* 0056bda0 slot 44 (RET 4) */
void     rx_snake_vf47(rx_addr act, rx_addr driver);                             /* 0056be10 slot 47 (RET 4) */
void     rx_snake_place_round(rx_addr act);                                      /* 0056b5c0 (stdcall act; RET 4) */
void     rx_snake_boundary_reset(rx_addr act);                                   /* 0056cd20 (ESI = act) */
void     rx_snake_boundary_shrink(rx_addr act, float dt);                        /* 0056d080 (EDI = act; stdcall dt; RET 4) */
void     rx_snake_water_rise(rx_addr act, float dt);                             /* 0056cf80 (EDI = act; stdcall dt; RET 4): not ported (no data) */
void     rx_snake_net_removed_listen(rx_addr channel, rx_addr act);              /* 00588dc0 (EAX = channel; stdcall act; RET 4) */
void     rx_snake_net_boundary_listen(rx_addr channel, rx_addr act);             /* 00588e80 */
void     rx_snake_net_round_listen(rx_addr channel, rx_addr act);                /* 00588f40 */
void     rx_snake_net_removed_unlisten(rx_addr channel, rx_addr act);            /* 00589000 */
void     rx_snake_net_boundary_unlisten(rx_addr channel, rx_addr act);           /* 00589060 */
void     rx_snake_net_round_unlisten(rx_addr channel, rx_addr act);              /* 005890c0 */
void     rx_snake_audio_vf3(rx_addr logic);                                      /* 00583550 */
void     rx_snake_audio_vf4(rx_addr logic);                                      /* 005837c0 */
void     rx_snake_audio_listen_round_started(rx_addr channel, rx_addr logic);    /* 0058bf10 */
void     rx_snake_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);   /* 0058bfd0 (ECX = channel, EAX = name; RET 8) */
void     rx_snake_audio_listen_eliminated(rx_addr channel, rx_addr logic);       /* 0058c090 */
void     rx_snake_audio_listen_player_wins(rx_addr channel, rx_addr logic);      /* 0058c150 */
void     rx_snake_audio_listen_trail_hit(rx_addr channel, rx_addr logic);        /* 0058c210 */
void     rx_snake_audio_unlisten_round_started(rx_addr channel, rx_addr logic);  /* 0058c2d0 */
void     rx_snake_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058c330 */
void     rx_snake_audio_unlisten_eliminated(rx_addr channel, rx_addr logic);     /* 0058c390 */
void     rx_snake_audio_unlisten_player_wins(rx_addr channel, rx_addr logic);    /* 0058c3f0 */
void     rx_snake_audio_unlisten_trail_hit(rx_addr channel, rx_addr logic);      /* 0058c450 */
void     rx_snake_audio_trail_hit(rx_addr logic, rx_addr act, rx_addr driver, rx_addr pos);     /* 00583a90 (RET 0xc) */

/* ---- SnakeDriver: race_snake_driver.c ---- */
rx_addr  rx_snake_driver_construct(rx_addr d, rx_addr act, rx_addr player, uint32_t team);   /* 00513910 (ECX = driver, EAX = player; stdcall act, team; RET 8) */
rx_addr  rx_network_driver_sdtor(rx_addr d, uint32_t flags);                     /* 0050df60 NetworkDriver::sdtor (RET 4) */
void     rx_snake_driver_listen(rx_addr channel, rx_addr d);                     /* 0053aef0 (EAX = channel; stdcall driver; RET 4) */
void     rx_snake_driver_unlisten(rx_addr channel, rx_addr d);                   /* 0053afb0 */
void     rx_snake_driver_vf1(rx_addr d);                                         /* 00513990 slot 1 */
void     rx_snake_driver_vf2(rx_addr d);                                         /* 00513b30 slot 2 */
void     rx_snake_driver_vf3(rx_addr d);                                         /* 00513c50 slot 3 */
void     rx_snake_driver_vf7(rx_addr d, float dt);                               /* 00513f30 slot 7 (RET 4) */
void     rx_snake_driver_vf9(rx_addr d, rx_addr event);                          /* 005148f0 slot 9 (RET 4) */
void     rx_snake_driver_vf16(rx_addr d, uint32_t kind, rx_addr object);         /* 00514c70 slot 16 (RET 8) */
void     rx_snake_driver_vf17(rx_addr d);                                        /* 00514cc0 slot 17 */
void     rx_snake_driver_human(rx_addr d);                                       /* 00514790 (EAX = driver) */
void     rx_snake_driver_keep_state(rx_addr d);                                  /* 005149b0 (ECX = driver) */
void     rx_snake_trail_hit_raise(rx_addr manager, rx_addr driver, rx_addr pos, rx_addr act);   /* 0053b070 (ESI = manager, ECX = driver, EDX = pos; stdcall act; RET 4) */

#ifdef __cplusplus
}
#endif
