/*005ab3e0: event startup selection, not activity creation. Candidate only.*/
#include "career_single.h"
#include "career_single_vector.h"
#include "career_players.h"
#include "build_activity_util.h"
#include "build_tables_sort.h"

static void filter(rx_addr vec,rx_addr c,rx_addr name,uint32_t value) {
    rx_tbl_crit_init_int(c,name,0,value);
    rx_tbl_crit_push(vec,c);
    rx_tbl_crit_destroy(c);
}
void rx_career_event_prepare(rx_addr event,const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F,8,0x250,4);
    if(!d||!d->manager||!d->player_table||!d->ui_get_int)
        act_unreached(0x005ab3e0,"career preparation dependencies");
    /*Both original eager database lookups remain, despite their unused results.*/
    (void)rx_tbl_find(0xae0910,0x94063c); /*AIRider*/
    const rx_addr manager=d->manager(),table=d->player_table(manager,2);
    (void)rx_tbl_find(0xae0910,0x940718); /*AIVehicle*/
    const uint32_t rows=rx_tbl_row_count(table);
    SU32(F,0x10,0);
    for(uint32_t row=0;row<rows;row++) {
        rx_tbl_set_cell_int(table,0x94d650,row,F+0x10);
        rx_tbl_set_cell_int(table,0x94d670,row,F+0x10);
    }
    const int32_t vehicle_class=(int32_t)d->ui_get_int(0x935df8,0xffffffffu);
    const int32_t event_limit=(int32_t)rx_career_event_rider_limit(U32(event,0x1c),d);
    int32_t limit=(int32_t)rx_career_class_rider_limit(vehicle_class);
    if(event_limit<=limit)limit=event_limit;
    uint32_t requested=act_vcall(event,4,1,(uint32_t[]){U32(event,0x10)});
    if(!requested)requested=4;
    int32_t used=0;
    const rx_addr human=rx_tbl_rows(act_column(table,0x936ef4),0);
    for(rx_addr n=LEFT_OF(human);n!=U32(human,0x18);n=rx_tbl_rowset_next(n)) {
        SU32(F,0x10,requested);
        rx_tbl_set_cell_int(table,0x94d670,U32(n,0xc),F+0x10);
        ++used;
    }
    const rx_addr ids=F+0x18,criteria=F+0x30,c=F+0xc0,sort=F+0x48,sc=F+0x170,set=F+0x60;
    rx_tbl_crit_vec_init(ids); /*same proxy/begin/end/cap vector layout*/
    rx_career_series_ai_ids(ids,U32(manager,0x20));
    rx_tbl_crit_vec_init(criteria);
    filter(criteria,c,0x936ef4,1);
    filter(criteria,c,0x94d650,0);
    rx_tbl_crit_vec_init(sort);
    rx_tbl_sort_criterion_init(sc,0x94d6cc,0);
    rx_tbl_sort_criterion_push(sort,sc);
    rx_tbl_sort_criterion_destroy(sc);
    rx_tbl_rowset_sorted_init(set,table,sort);
    /*Authored AIRiderIDs have priority; duplicate IDs preserve original repeated writes/count.*/
    if(U32(ids,0x10)!=U32(ids,0xc)) {
        rx_tbl_select(table,set,criteria);
        for(rx_addr id=U32(ids,0xc);id!=U32(ids,0x10);id+=4) {
            for(rx_addr n=LEFT_OF(set);n!=U32(set,0x18);n=rx_tbl_rowset_next(n)) {
                const uint32_t row=U32(n,0xc);
                if(act_cell(table,0x94d6c0,row)!=U32(id,0))continue;
                SU32(F,0x10,used<limit?requested:5);
                rx_tbl_set_cell_int(table,0x94d670,row,F+0x10);
                ++used;
                break;
            }
        }
    }
    rx_tbl_crit_vec_clear(criteria);
    filter(criteria,c,0x936ef4,1);
    filter(criteria,c,0x94d670,0);
    rx_tbl_select(table,set,criteria);
    for(rx_addr n=LEFT_OF(set);n!=U32(set,0x18);n=rx_tbl_rowset_next(n)) {
        SU32(F,0x10,used<limit?requested:5);
        rx_tbl_set_cell_int(table,0x94d670,U32(n,0xc),F+0x10);
        ++used;
    }
    rx_tbl_rowset_destroy(set);
    rx_tbl_sort_criteria_destroy(sort);
    rx_tbl_crit_vec_destroy(criteria);
    career_id_vector_destroy(ids);
    ACT_END(F);
}
