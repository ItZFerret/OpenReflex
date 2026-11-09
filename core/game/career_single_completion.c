/* Actual completion result/reward lifecycle; no invented placing or advancement. */
#include "career_single.h"
#include "career_single_vector.h"
#include "career_players.h"
#include "career_rewards.h"
#include "build_activity_util.h"
#include "build_tables_sort.h"

static rx_addr completion_manager(const rx_career_single_dependencies* d) {
    if(!d||!d->manager)act_unreached(0x413d20,"career completion manager");
    return d->manager();
}
int32_t rx_career_event_player_by_name(rx_addr name,rx_addr table) {
    const int32_t count=(int32_t)rx_tbl_row_count(table);
    for(int32_t row=0;row<count;row++) {
        const rx_addr cell=rx_tbl_cell_string(act_column(table,0x936f00),(uint32_t)row);
        if(!crt_stricmp(name,rx_tbl_str_cstr(cell)))return row;
    }
    return -1;
}
void rx_career_event_result_sort_columns(rx_addr vector) {
    ACT_FRAME(F,0xa4,1);
    rx_tbl_sort_criterion_init(F+8,0x94d6b8,1); /*Points, descending*/
    rx_tbl_sort_criterion_push(vector,F+8);
    rx_tbl_sort_criterion_destroy(F+8);
    ACT_END(F);
}
void rx_career_event_select_standings(rx_addr sorted,int32_t series,
                                     const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F,8,0xd4,3);
    const rx_addr v=F+0x10,c=F+0x28;
    rx_tbl_crit_vec_init(v);
    const rx_addr m=completion_manager(d);
    rx_tbl_crit_init_int(c,0x94c990,0,(uint32_t)series);
    rx_tbl_crit_push(v,c);rx_tbl_crit_destroy(c);
    ACT_CALL(1,rx_tbl_select(m+0x560,sorted,v));
    rx_tbl_crit_vec_destroy(v);
    ACT_END(F);
}
void rx_career_event_complete_results(rx_addr event,const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F,8,0xc4,3);
    rx_addr m=completion_manager(d);
    if(!d->player_table)act_unreached(0x5b6840,"career completion player table");
    const rx_addr players=d->player_table(m,2);
    const rx_addr sort=F+0x40,set=F+0x90,ids=F+0x18;
    rx_tbl_crit_vec_init(sort);
    act_vcall(event,0x58,1,&sort); /*actual eventvf22 controls its sorting columns*/
    m=completion_manager(d);
    ACT_CALL(2,rx_tbl_rowset_sorted_init(set,m+0x560,sort));
    ACT_CALL(2,rx_career_event_select_standings(set,-3,d));
    rx_tbl_crit_vec_init(ids);
    m=completion_manager(d);
    ACT_CALL(1,rx_career_series_ai_ids(ids,U32(m,0x20)));
    /*Original also constructs an unused empty RowSet for checked-iterator lifetime.*/
    rx_tbl_rowset_init(F+0x5c,0);
    int32_t place=0,locals=0;
    for(rx_addr n=LEFT_OF(set);n!=U32(set,0x18);n=rx_tbl_rowset_next(n),++place) {
        const rx_addr table=U32(n,0x14);
        const uint32_t row=U32(n,0xc);
        const rx_addr str=rx_tbl_cell_string(act_column(table,0x936f00),row);
        const rx_addr name=rx_tbl_str_cstr(str);
        const uint32_t type=act_cell(table,0x936ef4,row);
        const int32_t player=rx_career_event_player_by_name(name,players);
        if(type==0) {
            uint32_t state=18;
            if(I32(event,0xe8)<=(int32_t)act_cell(table,0x94d6b8,row)) {
                if(place==0)state=15;
                else if(place==1)state=16;
                else if(place==2)state=17;
            }
            m=completion_manager(d);
            const uint32_t controller=U32(m,0x1c);
            m=completion_manager(d);
            ACT_CALL(2,rx_career_rewards_calculate(m+0x78,controller,place));
            SU32(F,0xc,state);
            uint32_t c=rx_tbl_column_index(players,0x94d670);if(U32(rx_tbl_cell_word(players,c,(uint32_t)player),0)!=state)SU32(players,0x68,U32(players,0x68)+1);
            ACT_CALL(3,rx_tbl_set_int(U32(U32(players,0x10),4*c),(uint32_t)player,F+0xc));
            if(!locals) {
                m=completion_manager(d);
                if(place<I32(m,0x24))SU32(m,0x24,(uint32_t)place);
            }
            ++locals;
        }
        /*Retain authored-ID removal bookkeeping for every ranked row, including AI.*/
        const uint32_t id=act_cell(players,0x94d6c0,(uint32_t)player);
        rx_addr p=U32(ids,0xc);
        while(p!=U32(ids,0x10)) {
            if(U32(p,0)!=id){p+=4;continue;}
            const rx_addr end=U32(ids,0x10);
            const uint32_t bytes=end-p-4;
            if(bytes)crt_memmove_s(p,bytes,p+4,bytes);
            SU32(ids,0x10,end-4);
        }
    }
    rx_tbl_rowset_destroy(F+0x5c);
    career_id_vector_destroy(ids);
    rx_tbl_rowset_destroy(set);
    rx_tbl_sort_criteria_destroy(sort);
    ACT_END(F);
}

void rx_career_event_advance_victory(rx_addr event,const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(V,8,0xc,3);
    if(!d||!d->ui_get_int)act_unreached(0x5dc0c0,"career completion UI state");
    const uint32_t complete=d->ui_get_int(0x94d6a4,0);
    if(complete) {
        rx_addr m=completion_manager(d);
        const uint32_t controller=U32(m,0x1c);
        m=completion_manager(d);
        ACT_CALL(2,rx_career_rewards_apply(m+0x78,controller));
        ACT_CALL(0,rx_career_event_complete_results(event,d));
        if(!d->ui_int)act_unreached(0x5dbfb0,"career completion UI state");
        d->ui_int(0x94d6a4,0);
        ACT_CALL(1,rx_tbl_table_clear_rows(ACT_ACTIVITY_TBL));
        ACT_CALL(0,rx_career_gate_reset_actors());
    }
    const rx_addr m=completion_manager(d);
    if(!d->player_table)act_unreached(0x5b6840,"career completion player table");
    const rx_addr table=d->player_table(m,2);
    static const uint32_t states[]={15,16,17,18,14};
    for(unsigned i=0;i<5;i++) {
        const rx_addr rows=rx_tbl_rows(act_column(table,0x94d670),(int32_t)states[i]);
        if(!U32(rows,0x1c))continue;
        /*Actual005aa1c0 validates only the first iterator and returns. It does
         * not infer success, change event phase, or launch another event here.*/
        const rx_addr proxy=U32(rows,0),node=LEFT_OF(rows);
        if(!proxy||node==U32(U32(proxy,0),0x18))
            act_unreached(0x5aa3c4,"invalid completion iterator");
        if(i==4)(void)U32(node,0xc); /*004fc5c0 checked dereference*/
        ACT_END(V);return;
    }
    ACT_END(V);
}

void rx_career_event_post_result(rx_addr event,const rx_career_single_dependencies* d) {
    (void)event;
    ACT_FRAME(F,0x10,3);
    const rx_addr m=completion_manager(d);
    if(!d->player_table)act_unreached(0x5b6840,"career result player table");
    const rx_addr table=d->player_table(m,2);
    const rx_addr heats=rx_tbl_rows(act_column(table,0x94d670),7);
    const rx_addr lcq=rx_tbl_rows(act_column(table,0x94d670),10);
    rx_addr selected;uint32_t state;
    if(!U32(lcq,0x1c)&&!U32(heats,0x1c)) {
        selected=rx_tbl_rows(act_column(table,0x94d670),14);
        if(!U32(selected,0x1c)){ACT_END(F);return;}
        state=13;
    } else {
        if(!d->ui_event)act_unreached(0x5db500,"career advance popup event");
        d->ui_event(0x94d688);
        /*The original rereads live set sizes after the event callback.*/
        if(U32(lcq,0x1c)){selected=lcq;state=9;}
        else if(U32(heats,0x1c)){selected=heats;state=6;}
        else {ACT_END(F);return;}
    }
    const rx_addr proxy=U32(selected,0),node=LEFT_OF(selected);
    if(!proxy||node==U32(U32(proxy,0),0x18))
        act_unreached(0x5aa12e,"invalid requested result iterator");
    SU32(F,0xc,state);
    rx_tbl_set_cell_int(table,0x94d670,U32(node,0xc),F+0xc);
    ACT_END(F);
}
