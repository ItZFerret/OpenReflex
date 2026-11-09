#pragma once
#include "rx_game.h"
#include <stddef.h>
int rx_player_snapshot_export(void* output,uint32_t capacity,uint32_t* required,char* error,size_t error_size);
int rx_player_snapshot_import(const void* input,uint32_t bytes,char* error,size_t error_size);
