/* build_uiprofile.h -- the UI settings block [0x00d92efc] and the profile bootstrap (build_uiprofile.c): UISettings_Defaults
 * 005b8030, the row constructor / block constructor 005b7d50 / 005b7cd0, the row defaults 005b7d70, UISettings_Apply 005b9af0,
 * UISettings_Reset 005b80b0 (with the ControllerRemapping defaults), UI_SetPrimaryPlayerDeviceIndex 00743030, and the product's
 * startup / profile hooks. */
#pragma once
#include "rx_game.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_uisettings_defaults(rx_addr s);                          /* 005b8030 (EAX = a row; EAX out = it) */
rx_addr rx_uisettings_row_construct(rx_addr row);                   /* 005b7d50 (ECX = a row) */
rx_addr rx_uisettings_construct(rx_addr s);                         /* 005b7cd0 (EDI = the 0x170-byte block; EAX out = it) */
void    rx_uisettings_row_defaults(rx_addr row);                    /* 005b7d70 (stack row) */
void    rx_uisettings_apply(rx_addr s, int32_t device);             /* 005b9af0 (stack settings, device) */
void    rx_uisettings_reset(rx_addr s, int32_t device);             /* 005b80b0 (ECX = settings; stack device) */
void    rx_ui_set_primary_player_device_index(int32_t controller);  /* 00743030 (EAX = controller) */
rx_addr rx_ui_settings_create(void);                                /* 00598640's settings part (alloc + construct when absent) */
void    rx_ui_profile_bootstrap(void);                              /* the shipping guest route: SetPrimaryPlayerDeviceIndex(0) */

/* Product boundary: bounded snapshots of the five real garage tables. Gameplay remains in the original table consumers. */
struct rx_profile_cell;
struct rx_remap_table;
int rx_ui_remap_defaults(struct rx_remap_table* out);
int rx_ui_remap_get(struct rx_remap_table* out, char* error, size_t error_size);
int rx_ui_remap_set(const int32_t values[38], char* error, size_t error_size);
int rx_ui_profile_import(const struct rx_profile_cell* cells, int count, char* error, size_t error_size);
int rx_ui_profile_tune(const int32_t values[8], char* error, size_t error_size);
int rx_ui_profile_tuning_read(int32_t out[8], char* error, size_t error_size);

#ifdef __cplusplus
}
#endif
