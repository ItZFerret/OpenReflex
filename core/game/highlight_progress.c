/* Local highlight persistence from 006d7980 (983 bytes) and 006d7d60 (239 bytes).
 * Native functional candidate: not an acceptance claim for incidental ECX/EDX,
 * x87 or complete original stack residue. Uses canonical Rdb owners, including
 * indexed writes, version counters and criteria/rowset destruction.
 *
 * build_mxgame's native achievements_request_stats never creates a Steam client
 * or sets statsReceived. This is the original's SteamUserStats()==NULL path:
 * progress at its goal may receive a new UnlockedOrder on subsequent calls.
 * Do not silently substitute a local "already achieved" cache for that behavior.
 */
#include "phys_studio.h"
#include "map_scale.h"
#include "highlight_progress.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "fx.h"
#include "fx_util.h"
#include "race_extern.h"
#ifdef RX_ORACLE
#error "Highlight candidate needs a Steam provider and full output observer before oracle registration"
#endif

enum { HIGHLIGHT=0x961fd8, PLAYER_HIGHLIGHT=0x961fe4, HIGHLIGHT_ID=0x961ff4,
       GOAL=0x962000, PROGRESS=0x962008, UNLOCKED_ORDER=0x962018 };

static rx_addr lookup(rx_addr table, rx_addr key) {
    rx_addr ref;
    RACE_CORE_CALL(1, ref=rx_fx_table_row_by(table,HIGHLIGHT_ID,key));
    return ref;
}
static uint32_t cell(rx_addr ref, rx_addr name) {
    rx_addr value;
    RACE_CORE_CALL(1, value=rx_fx_row_cell(ref,name));
    return U32(value,0);
}
static int valid(rx_addr ref) { return U32(ref,8)!=0 && U32(ref,0)!=0xffffffffu; }
static void criterion(rx_addr vec,rx_addr crit,rx_addr name,uint32_t op,uint32_t value) {
    RACE_CORE_CALL(1,rx_tbl_crit_init_int(crit,name,op,value));
    RACE_CORE_CALL(1,rx_tbl_crit_push(vec,crit));
    RACE_CORE_CALL(0,rx_tbl_crit_destroy(crit));
}
/* Keep the goal cache shared across controllers, as in the original. */
static rx_addr tables_and_goal(rx_addr tracking,uint32_t controller,uint32_t index,rx_addr key) {
    if(U32(tracking,4)==0)return 0;
    rx_addr goals,player;
    RACE_CORE_CALL(1,goals=rx_tbl_find(RX_TBL_DEFAULT_DB,HIGHLIGHT));
    RACE_CORE_CALL(1,player=rx_tbl_find(RX_TBL_PLAYER_DB(controller),PLAYER_HIGHLIGHT));
    if(!goals||!player)return 0;
    if(U32(tracking,8+4u*index)==0xffffffffu){
        const rx_addr ref=lookup(goals,key);
        if(!valid(ref))return 0;
        SU32(tracking,8+4u*index,cell(ref,GOAL));
    }
    return player;
}
uint32_t rx_highlight_add_progress(rx_addr tracking,uint32_t controller,uint32_t index) {
    if(rx_ms_records_off())return 0;
    if(rx_ms_custom_track())return 0;   /* MODS/TRACKS (port, map_scale.h): nor on a custom track */
    if(rx_ps_records_off())return 0;   /* PHYSICS STUDIO (port): nor with changed physics */   /* MAP SCALE (port, map_scale.h): no highlight (achievement) progress on a scaled track */
    RACE_FRAME_PLAIN(F,0x118,4);
    const rx_addr key=F+0x14,copy=F+0x20,vec=F+0x2c,set=F+0x44,crit=F+0x78;
    uint32_t result=0,progress;
    SU32(key,0,index+1u);
    const rx_addr player=tables_and_goal(tracking,controller,index,key);
    if(!player)goto done;
    rx_addr ref=lookup(player,key);
    for(uint32_t offset=0;offset<12;offset+=4)SU32(copy,offset,U32(ref,offset));
    if(!valid(copy)){
        progress=1;
        RACE_CORE_CALL(0,rx_tbl_crit_vec_init(vec));
        criterion(vec,crit,HIGHLIGHT_ID,0,index+1u);
        criterion(vec,crit,PROGRESS,0,progress);
        RACE_CORE_CALL(2,rx_tbl_add_row(player,copy,vec));
        RACE_CORE_CALL(0,rx_tbl_crit_vec_destroy(vec));
    }else{
        progress=cell(copy,PROGRESS);
        if((int32_t)progress<I32(tracking,8+4u*index)){
            ++progress;SU32(key,0,progress);
            RACE_CORE_CALL(2,rx_tbl_set_cell_int(U32(copy,8),PROGRESS,U32(copy,0),key));
        }
        /* With no Steam client, original 006d7c31 continues to the goal check. */
    }
    if((int32_t)progress<I32(tracking,8+4u*index))goto done;
    RACE_CORE_CALL(0,rx_tbl_crit_vec_init(vec));
    criterion(vec,crit,UNLOCKED_ORDER,4,0); /* original operator 4: greater than */
    RACE_CORE_CALL(1,rx_tbl_rowset_init(set,0));
    RACE_CORE_CALL(1,rx_tbl_select(player,set,vec));
    int32_t greatest=0;
    const rx_addr head=U32(set,0x18);
    for(rx_addr node=U32(head,0);node!=head;node=rx_tbl_rowset_next(node)){
        const uint32_t column=rx_tbl_column_index(player,UNLOCKED_ORDER);
        const int32_t value=I32(rx_tbl_cell_word(player,column,U32(node,0xc)),0);
        if(greatest<=value)greatest=value;
    }
    SU32(key,0,(uint32_t)greatest+1u);
    RACE_CORE_CALL(2,rx_tbl_set_cell_int(U32(copy,8),UNLOCKED_ORDER,U32(copy,0),key));
    /* Steam unlock dispatch remains the existing offline platform boundary. */
    RACE_CORE_CALL(0,rx_tbl_rowset_destroy(set));
    RACE_CORE_CALL(0,rx_tbl_crit_vec_destroy(vec));
    result=1;
done:
    RACE_FRAME_END(F);
    return result;
}
RX_FUNCTION(0x006d7980,rx_highlight_add_progress)

uint32_t rx_highlight_is_unlocked(rx_addr tracking,uint32_t controller,uint32_t index) {
    RACE_FRAME_PLAIN(F,0x10,4);
    const rx_addr key=F+0x2c; /* original reuses its third argument slot */
    uint32_t result=0;
    SU32(key,0,index+1u);
    const rx_addr player=tables_and_goal(tracking,controller,index,key);
    if(player){
        const rx_addr ref=lookup(player,key);
        const int32_t progress=valid(ref)?(int32_t)cell(ref,PROGRESS):0;
        result=progress>=I32(tracking,8+4u*index);
    }
    RACE_FRAME_END(F);
    return result;
}
RX_FUNCTION(0x006d7d60,rx_highlight_is_unlocked)
