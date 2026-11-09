#include "career_player_native.h"
#include "build_activity_util.h"
static void requested(uint32_t row,uint32_t id,rx_addr name){
 ACT_FRAME(F,4,1);const rx_addr argument=F+0xc;
 uint32_t col=rx_tbl_column_index(ACT_PLAYER_LIST,name);
 if(id!=U32(rx_tbl_cell_word(ACT_PLAYER_LIST,col,row),0))SU32(ACT_PLAYER_LIST,0x68,U32(ACT_PLAYER_LIST,0x68)+1);
 if(col>=((U32(ACT_PLAYER_LIST,0x14)-U32(ACT_PLAYER_LIST,0x10))>>2))crt_invalid_parameter_noinfo();
 SU32(argument,0,id);ACT_CALL(3,rx_tbl_set_int(U32(U32(ACT_PLAYER_LIST,0x10),4*col),row,argument));ACT_END(F);
}
void rx_career_player_request_vehicle(uint32_t row,uint32_t id){requested(row,id,0x93df5c);}
RX_FUNCTION(0x004de0b0,rx_career_player_request_vehicle)
void rx_career_player_request_rider(uint32_t row,uint32_t id){requested(row,id,0x93df70);}
RX_FUNCTION(0x004de1d0,rx_career_player_request_rider)
void rx_career_player_reset_requests(uint32_t row,rx_addr zero_out){
 ACT_FRAME(F,0,3);SU32(zero_out,0,0);
 if(act_cell(ACT_PLAYER_LIST,0x93df2c,row)==UINT32_MAX){
  /*Original first retains ActivityArea pointer, then reads ActivityType, then
   * zero-extends the area's low byte before canonical request_activity signextends.*/
  rx_addr area=rx_tbl_cell_word(ACT_PLAYER_LIST,rx_tbl_column_index(ACT_PLAYER_LIST,0x93dda0),row);
  uint32_t type=act_cell(ACT_PLAYER_LIST,0x93dd90,row);
  ACT_CALL(2,rx_player_list_request_activity(row,type,U8(area,0)));
 }
 if(act_cell(ACT_PLAYER_LIST,0x93df5c,row)==UINT32_MAX){uint32_t id=act_cell(ACT_PLAYER_LIST,0x936cb4,row);ACT_CALL(1,rx_career_player_request_vehicle(row,id));}
 if(act_cell(ACT_PLAYER_LIST,0x93df70,row)==UINT32_MAX){uint32_t id=act_cell(ACT_PLAYER_LIST,0x93dec8,row);ACT_CALL(1,rx_career_player_request_rider(row,id));}
 ACT_END(F);
}
RX_FUNCTION(0x004de2f0,rx_career_player_reset_requests)
void rx_career_reset_player_requests(uint32_t row){ACT_FRAME(F,4,0);rx_career_player_reset_requests(row,F);ACT_END(F);}
