/* Base event callbacks, isolated native semantic owner. Original proof pending. */
#include "career_single.h"
#include "career_single_events.h"
#include "career_single_vector.h"
#include "career_manager.h"
#include "career_refresh.h"
#include "build_activity_util.h"
#include "career_player_native.h"

static rx_addr event_manager(const rx_career_single_dependencies* d) {
    if(!d||!d->manager)act_unreached(0x413d20,"career manager dependency");
    return d->manager();
}
void rx_career_event_restart(rx_addr event,const rx_career_single_dependencies* d) {
    if(!U32(event,0x14))return;
    rx_career_clear_requested_states(event_manager(d));
    rx_career_zero_standing_scores(event_manager(d),-3);
}

/* 005a8af0 MXSeriesEvent_SetupNextActivityPlayers (stdcall event, reset; RET 8) -> the next phase's riders whose Player List row is not
 * active. With reset: those rows made active (Active 1, their requested activity "none" 0x3d) and every other Player List row that
 * was active made inactive (Active 0, its requests reset: 004de2f0). The riders: the series' player table of the profile
 * ("Player%sSeriesEventPlayers%s", the controller's database) rows whose PlayerState is the next phase's (the event's vf1), found
 * in the Player List by name and type (005aac00). Frame: and esp, -8; sub esp, 0x144; push ebx / esi / edi. Locals: +0xc the row
 * counter / state / type / Player List row / the 0 for a reset, +0x10 the table, +0x14 the count, +0x18 the vector proxy copy, +0x20
 * the vector of the rows still to deactivate (proxy, begin +0xc, end +0x10, capacity +0x14), +0x38 the row iterator, +0x44 the 1 of
 * an activation, +0x48 the table name. */
uint32_t rx_career_event_setup_next_players(rx_addr event,uint32_t reset,
                                           const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F,8,0x144,3);
    const uint32_t initial_count=rx_tbl_row_count(ACT_PLAYER_LIST);   /* 004021d0 */
    const rx_addr remaining=F+0x20;
    rx_tbl_crit_vec_init(remaining);
    if(reset) {
        ACT_CALL(1,rx_tbl_vector_reserve_ptr(remaining,initial_count));
        for(uint32_t row=0;row<initial_count;row++) {
            SU32(F,0xc,row);
            ACT_CALL(1,rx_tbl_vector_push_ptr(remaining,F+0xc));
        }
    }
    SU32(F,0x14,0);
    (void)event_manager(d);
    ACT_CALL(0,rx_career_apply_player_states(event_manager(d)));      /* 005b39a0 (EDX = manager) */
    const uint32_t phase=U32(event,0x10);
    SU32(F,0xc,act_vcall(event,4,1,&phase));                           /* vf1: the next phase's PlayerState */
    const rx_addr m=event_manager(d),name=F+0x48;
    SU8(name,0,0);
    crt_memset(name+1,0,0xff);
    {   /* "Player%sSeriesEventPlayers%s" with the profile kind (0x009d23bc) and the series owner's name (0x009d2374 [mode]): the
         * pushed arguments (owner, kind, format) below the frame, 0078fd60 (ESI = size, EDI = buffer; cdecl) */
        SU32(F,-4,U32(0,0x9d2374+4*U32(0,0x9e2318)));
        SU32(F,-8,U32(0,0x9d23bc));
        SU32(F,-0xc,0x94d9d8);
        rx_set_sp(F-0xc);                                               /* the three pushes */
        rx_format_buffer(name,0x100,0x94d9d8,F-8);                      /* 0078fd60 */
        rx_set_sp(F);
    }
    rx_addr table;
    ACT_CALLR(table,1,rx_tbl_find(0xae10f0+0x1f4*U32(m,0x1c),name)); /* Database_FindTable */
    SU32(F,0x10,table);
    const rx_addr selected=rx_tbl_rows(act_column(table,0x94d650),I32(F,0xc));   /* 00418e50: PlayerState */
    SU32(F,0x40,selected);
    for(rx_addr n=LEFT_OF(selected);n!=U32(selected,0x18);n=rx_tbl_rowset_next(n)) {
        SU32(F,0x38,U32(selected,0));
        SU32(F,0x3c,n);
        const uint32_t row=U32(n,0xc);
        SU32(F,0xc,act_cell(table,0x936ef4,row));                      /* PlayerType */
        const rx_addr str=rx_tbl_cell_string(act_column(table,0x936f00),row);
        int32_t player;
        ACT_CALLR(player,2,(int32_t)rx_career_find_player_row(rx_tbl_str_cstr(str),U32(F,0xc)));   /* 005aac00 (cdecl) */
        SU32(F,0xc,(uint32_t)player);
        if(player<0)continue;
        if(act_cell(ACT_PLAYER_LIST,0x93def8,(uint32_t)player)==0)SU32(F,0x14,U32(F,0x14)+1);   /* Active */
        if(!reset)continue;
        SU32(F,0x44,1);
        {   /* the inline Table_SetCellInt: a change counts in the Player List's revision (0x00ae0e70) */
            const uint32_t column=rx_tbl_column_index(ACT_PLAYER_LIST,0x93def8);
            if(U32(rx_tbl_cell_word(ACT_PLAYER_LIST,column,(uint32_t)player),0)!=1)SU32(0,0xae0e70,U32(0,0xae0e70)+1);
            if(column>=((U32(ACT_PLAYER_LIST,0x14)-U32(ACT_PLAYER_LIST,0x10))>>2))crt_invalid_parameter_noinfo();
            ACT_CALL(3,rx_tbl_set_int(U32(U32(ACT_PLAYER_LIST,0x10),4*column),(uint32_t)player,F+0x44));   /* 004f5330 */
        }
        ACT_CALL(2,rx_player_list_request_activity((uint32_t)player,0x3d,0));   /* 004dde70 (ESI = row) */
        SU32(F,0x18,U32(remaining,0));
        rx_addr p=U32(remaining,0xc);
        const rx_addr end=U32(remaining,0x10);
        while(p!=end&&U32(p,0)!=(uint32_t)player)p+=4;
        /* Already removed identities leave the vector unchanged (the original's end branch). */
        if(p==end)continue;
        const uint32_t bytes=end-p-4;
        if(bytes) {
            crt_memmove_s(p,bytes,p+4,bytes);                              /* [0x00916200] memmove_s (cdecl, 4 arguments) */
        }
        SU32(remaining,0x10,end-4);
    }
    if(reset) {
        SU32(F,0x18,U32(remaining,0));
        for(rx_addr p=U32(remaining,0xc);p!=U32(remaining,0x10);p+=4) {
            SU32(F,0x1c,p);
            const uint32_t player=U32(p,0);
            if((int32_t)act_cell(ACT_PLAYER_LIST,0x93def8,player)<=0)continue;
            SU32(F,0xc,0);
            const uint32_t column=rx_tbl_column_index(ACT_PLAYER_LIST,0x93def8);
            if(U32(rx_tbl_cell_word(ACT_PLAYER_LIST,column,player),0)!=0)SU32(0,0xae0e70,U32(0,0xae0e70)+1);
            if(column>=((U32(ACT_PLAYER_LIST,0x14)-U32(ACT_PLAYER_LIST,0x10))>>2))crt_invalid_parameter_noinfo();
            ACT_CALL(3,rx_tbl_set_int(U32(U32(ACT_PLAYER_LIST,0x10),4*column),player,F+0xc));   /* 004f5330 */
            ACT_CALL(0,rx_career_player_reset_requests(player,F+0xc));                         /* 004de2f0 (ECX = row, EAX = &0) */
        }
    }
    career_id_vector_destroy(remaining);
    const uint32_t inactive=U32(F,0x14);
    ACT_END(F);
    return inactive;
}
/* 005aad40 MXSeriesEvent_OnAdvanceNextSubEvent (thiscall event): every RequestedPlayerState 7 / 10 / 14 (a local rider's LCQ / out /
 * ...) becomes 6 / 9 / 13 (the inline compare counts a change in the table's revision +0x68, then 004f5330 sets the cell), then
 * the next players (005a8af0, reset 0): none to add -> the event's vf9 (the gate selection), else UIRoutingMessage "Select Activity
 * Area" and the hub (the in-game graph's MXUIAAALoading page). Frame: and esp, -8; sub esp, 8; push ebx / ebp / esi / edi: the value
 * at +0x10, the event at +0x14. */
void rx_career_event_next_sub_event(rx_addr event,const rx_career_single_dependencies* d) {
    ACT_FRAME_EBP(F,8,8,4);
    SU32(F,0x14,event);
    const rx_addr m=event_manager(d);
    if(!d->player_table)act_unreached(0x5b6840,"career event player table");
    const rx_addr table=d->player_table(m,2);
    const rx_addr value=F+0x10;
    for(uint32_t row=0;row<rx_tbl_row_count(table);row++) {
        const uint32_t state=act_cell(table,0x94d670,row);   /* RequestedPlayerState */
        if(state!=7&&state!=10&&state!=14)continue;
        SU32(value,0,state-1);
        const uint32_t column=rx_tbl_column_index(table,0x94d670);
        if(U32(rx_tbl_cell_word(table,column,row),0)!=state-1)SU32(table,0x68,U32(table,0x68)+1);
        if(column>=((U32(table,0x14)-U32(table,0x10))>>2))crt_invalid_parameter_noinfo();
        ACT_CALL(3,rx_tbl_set_int(U32(U32(table,0x10),4*column),row,value));   /* 004f5330 */
    }
    uint32_t inactive;
    ACT_CALLR(inactive,2,rx_career_event_setup_next_players(event,0,d));       /* 005a8af0 */
    if(!inactive)act_vcall(event,0x24,0,0);
    else {
        if(!d->ui_string||!d->ui_goto_hub)act_unreached(0x5db500,"career area routing");
        d->ui_string(0x94cc74,0x94c334); /*UIRoutingMessage, Select Activity Area*/
        d->ui_goto_hub();
    }
    ACT_END(F);
}

/* 005a9ce0 AdvanceGateSelection (thiscall event). Frame: push ecx (the page wipe's payload slot); push esi. */
void rx_career_event_advance_gate(rx_addr event,const rx_career_single_dependencies* d) {
    ACT_FRAME(F,0,2);
    ACT_CALL(0,rx_career_event_process_gate(event,d));                          /* 005a9a30 */
    if(U32(event,0x24)){ACT_END(F);return;}
    for(unsigned i=0;i<16;i++)SU32(event,0x70+4*i,0x7fffffffu);
    for(unsigned i=0;i<12;i++)SU32(event,0x40+4*i,0xffffffffu);
    SU32(event,0x34,0);SU32(event,0x38,0);SU32(event,0x3c,0);
    rx_set_sp(F-4);                                                             /* push edi */
    ACT_CALL(1,rx_career_subscribe_page_wipe(event));                           /* 005bd9e0 (EAX = name, ECX = channel; event) */
    rx_career_emit_series_page_wipe();                                          /* "ShowPageWipe" ("SeriesEvent") */
    ACT_END(F);
}
void rx_career_event_assign_gate(rx_addr event,uint32_t device,int32_t gate) {
    const int32_t count=I32(event,0x3c);
    int32_t selected=0;
    while(selected<count&&I32(event,0x70+4*selected)!=gate)++selected;
    if(selected>=count)return;
    SU32(event,0x70+4*selected,0x7fffffffu);
    SU32(event,0x3c,U32(event,0x3c)-1);
    const rx_addr rows=rx_tbl_rows(act_column(ACT_PLAYER_LIST,0x93ded0),(int32_t)device);
    const rx_addr proxy=U32(rows,0),node=LEFT_OF(rows);
    if(!proxy||node==U32(U32(proxy,0),0x18))
        act_unreached(0x5a9e00,"gate device has no original player row");
    ACT_FRAME_EBP(F,8,8,3);
    SU32(F,4,(uint32_t)gate);
    rx_tbl_set_cell_int(ACT_PLAYER_LIST,0x93df00,U32(node,0xc),F+4);
    rx_career_sort_gate_values(event+0x70,event+0xb0,16);
    SU32(event,0x34,U32(event,0x34)+1);
    ACT_END(F);
}
