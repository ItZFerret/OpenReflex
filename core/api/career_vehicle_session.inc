/* Product read-only identity/compatibility boundary, canonical original model
 * resolution. Never reconstructs profile, equips a default or mutates tables. */
static int cp_vehicle_identity(struct rx_session*s,rx_career_vehicle_identity*out){
 if(s->career_phase<RX_CAREER_PHASE_CONFIGURED||s->career_phase>RX_CAREER_PHASE_STARTED)return RX_CAREER_BAD_STATE;
 rx_career_vehicle_identity v={0};v.size=sizeof v;v.version=RX_CAREER_VEHICLE_VERSION;v.generation=s->career_presentation.generation;
 v.authored_class=v.resolved_class=-1;int32_t player=-1;
 for(uint32_t row=0;row<rx_tbl_row_count(ACT_PLAYER_LIST);row++){
  if(cp_word(ACT_PLAYER_LIST,0x936ef4,row)!=0||cp_word(ACT_PLAYER_LIST,0x93ded0,row)!=s->career_controller)continue;
  if(player>=0)return RX_CAREER_CONFLICT;player=(int32_t)row;
 }
 if(player<0)return RX_CAREER_NOT_FOUND;v.player_row=player;
 uint32_t vehicle=cp_word(ACT_PLAYER_LIST,0x93df5c,(uint32_t)player);
 if(vehicle==UINT32_MAX)vehicle=cp_word(ACT_PLAYER_LIST,0x936cb4,(uint32_t)player);
 if(vehicle==UINT32_MAX)return RX_CAREER_NOT_FOUND;
 int32_t row=cp_find(ACT_VEHICLE_LIST,0x936cb4,(int32_t)vehicle);if(row<0)return RX_CAREER_NOT_FOUND;
 v.vehicle_list_id=(int32_t)vehicle;v.owner=(int32_t)cp_word(ACT_VEHICLE_LIST,0x93df84,(uint32_t)row);
 v.owner_vehicle_id=(int32_t)cp_word(ACT_VEHICLE_LIST,0x93df94,(uint32_t)row);
 if(v.owner!=(int32_t)s->career_controller&&v.owner!=4)return RX_CAREER_UNSUPPORTED;
 uint32_t model;ACT_CALLR(model,1,rx_career_vehicle_model_id(U32(0,0xa946b8)+0x154b0,vehicle));
 if(model==UINT32_MAX)return RX_CAREER_NOT_FOUND;v.model_id=(int32_t)model;
 rx_addr models=cp_table(RX_TBL_DEFAULT_DB,0x93e180);row=cp_find(models,0x93e170,(int32_t)model);if(row<0)return RX_CAREER_NOT_FOUND;
 v.vehicle_type=(int32_t)cp_word(models,0x936ce4,(uint32_t)row);
 if(s->career_phase!=RX_CAREER_PHASE_CONFIGURED){v.authored_class=I32(0x9e2188,0x68);v.resolved_class=I32(0x9e2188,0x64);}
 *out=v;return RX_CAREER_OK;
}
/* RaceAndRideEvents:OnAcceptSeries: the equipped vehicle's type must be one of VehicleClassType's types for the series' class
 * ("isValidType", else ShowInvalidRaceRequirementsPopup). Every type a career class names (MX Lite 2, MX 3, ATV 4, UTV 11,
 * Super Buggy 12, Pro Lite 14, Pro 2 15) is simulated by the core. */
static int cp_vehicle_supported(const rx_career_vehicle_identity* v) {
    if (v->authored_class < 0 || v->vehicle_type < 0) return 0;
    const rx_addr types = cp_table(RX_TBL_DEFAULT_DB, 0x936cc0);   /* "VehicleClassType" */
    for (uint32_t row = 0; row < rx_tbl_row_count(types); row++) {
        if ((int32_t)cp_word(types, 0x936cd4, row) == v->authored_class && (int32_t)cp_word(types, 0x936ce4, row) == v->vehicle_type) return 1;
    }
    return 0;
}
static int cp_vehicle_same(const rx_career_vehicle_identity*a,const rx_career_vehicle_identity*b){
 return a->generation==b->generation&&a->player_row==b->player_row&&a->vehicle_list_id==b->vehicle_list_id&&a->owner==b->owner&&a->owner_vehicle_id==b->owner_vehicle_id&&a->model_id==b->model_id&&a->vehicle_type==b->vehicle_type&&a->authored_class==b->authored_class;
}
