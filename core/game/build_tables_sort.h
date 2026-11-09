#pragma once
#include "build_tables.h"
/* 0xa0 sort criteria are distinct from the ordinary 0xb0 filter criteria. */
void rx_tbl_sort_criterion_init(rx_addr criterion,rx_addr name,uint32_t direction);
void rx_tbl_sort_criterion_destroy(rx_addr criterion);
void rx_tbl_sort_criterion_push(rx_addr vector,rx_addr criterion);
void rx_tbl_sort_criteria_destroy(rx_addr vector);
void rx_tbl_resolve_sort_columns(rx_addr columns,rx_addr table,rx_addr criteria);
void rx_tbl_rowset_sorted_init(rx_addr set,rx_addr table,rx_addr criteria);
void rx_tbl_rowset_sorted_all(rx_addr set,rx_addr table,uint32_t count,rx_addr criteria);
int rx_tbl_row_less(rx_addr set,uint32_t left,uint32_t right);
