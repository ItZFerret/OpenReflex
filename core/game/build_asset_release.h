/* build_asset_release.h -- the asset / AssetIO / collision geometry destruction ports of build_asset_release.c (w-vtunported, w-vtunported2). */
#pragma once
#include "rx_game.h"

rx_addr rx_model_asset_sdtor(rx_addr asset, uint32_t flags);                               /* 007d4010 */
rx_addr rx_model_asset_get_io(rx_addr asset);                                              /* 007d40b0 */
rx_addr rx_table_data_asset_get_io(rx_addr asset);                                         /* 004eef70 */
rx_addr rx_tdf_asset_get_io(rx_addr asset);                                                /* 0053a030 */
rx_addr rx_bink_asset_get_io(rx_addr asset);                                               /* 005940a0 */
rx_addr rx_localization_asset_get_io(rx_addr asset);                                       /* 005c3650 */
rx_addr rx_xenon_forest_asset_get_io(rx_addr asset);                                       /* 00661ee0 */
rx_addr rx_xenon_grass_asset_get_io(rx_addr asset);                                        /* 0066c6f0 */
rx_addr rx_xenon_tree_asset_get_io(rx_addr asset);                                         /* 00672000 */
rx_addr rx_particle_attrib_asset_get_io(rx_addr asset);                                    /* 006937c0 */
rx_addr rx_adv_asset_get_io(rx_addr asset);                                                /* 006ce7c0 */
rx_addr rx_terrain_collision_asset_get_io(rx_addr asset);                                  /* 006d89e0 */
rx_addr rx_xenon_terrain_asset_get_io(rx_addr asset);                                      /* 006d9a60 */
rx_addr rx_lua_asset_get_io(rx_addr asset);                                                /* 0070ccf0 */
rx_addr rx_xenon_morph_asset_get_io(rx_addr asset);                                        /* 007a9ab0 */
rx_addr rx_sub_scene_asset_get_io(rx_addr asset);                                          /* 007d3e60 */
rx_addr rx_banner_asset_get_io(rx_addr asset);                                             /* 007d5550 */
rx_addr rx_particle_asset_get_io(rx_addr asset);                                           /* 007e1a10 */
rx_addr rx_xenon_water_asset_get_io(rx_addr asset);                                        /* 007f02f0 */
rx_addr rx_packed_data_asset_get_io(rx_addr asset);                                        /* 007f86d0 */
rx_addr rx_cell_asset_get_io(rx_addr asset);                                               /* 007fa7a0 */
rx_addr rx_sound_asset_get_io(rx_addr asset);                                              /* 00808640 */
rx_addr rx_sound_bank_asset_get_io(rx_addr asset);                                         /* 00808980 */
rx_addr rx_miles_sound_dat_asset_get_io(rx_addr asset);                                    /* 00808af0 */
rx_addr rx_collision_asset_get_io(rx_addr asset);                                          /* 00818ec0 */
rx_addr rx_action_asset_get_io(rx_addr asset);                                             /* 0082af80 */
rx_addr rx_anim_asset_get_io(rx_addr asset);                                               /* 0082bd70 */
rx_addr rx_performance_asset_get_io(rx_addr asset);                                        /* 0083f600 */
rx_addr rx_iasset_sdtor(rx_addr a, uint32_t flags);                                        /* 007d5530 */
rx_addr rx_iasset_io_sdtor(rx_addr io, uint32_t flags);                                    /* 007f84b0 */
rx_addr rx_performance_asset_sdtor(rx_addr a, uint32_t flags);                             /* 0083f630 */
rx_addr rx_collision_asset_sdtor(rx_addr a, uint32_t flags);                               /* 00818e80 */
rx_addr rx_packed_data_asset_sdtor(rx_addr a, uint32_t flags);                             /* 007f8590 */
rx_addr rx_collision_geometry_sdtor(rx_addr g, uint32_t flags);                            /* 00767ec0 */
rx_addr rx_collision_geometry_box_sdtor(rx_addr g, uint32_t flags);                        /* 00818c00 */
rx_addr rx_collision_geometry_rays_sdtor(rx_addr g, uint32_t flags);                       /* 00818a80 */
rx_addr rx_collision_geometry_triangles_sdtor(rx_addr g, uint32_t flags);                  /* 00818b90 */
rx_addr rx_collision_geometry_split_sdtor(rx_addr g, uint32_t flags);                      /* 00818e60 */
rx_addr rx_collision_geometry_composite_sdtor(rx_addr g, uint32_t flags);                  /* 00819dc0 */
void rx_anim_track_free0(rx_addr e);                                                       /* 00839ec0 */
void rx_anim_track_free10(rx_addr e);                                                      /* 00839f00 */
rx_addr rx_anim_asset_sdtor(rx_addr a, uint32_t flags);                                    /* 0082bbf0 */
uint32_t rx_table_list_remove(rx_addr table);                                              /* 004eed70 */
void rx_table_data_asset_io_destroy(rx_addr io, rx_addr asset);                            /* 004f01b0 */
void rx_bxml_asset_io_destroy(rx_addr io, rx_addr asset);                                  /* 00822250 */
void rx_xml_reader_dtor(rx_addr r);                                                        /* 00824fe0 */
rx_addr rx_bxml_asset_sdtor(rx_addr o, uint32_t flags);                                    /* 00822170 */
