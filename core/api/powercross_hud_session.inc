/* Raw fields read by original countdown ownership; presentation is separate. */
int rx_session_powercross_hud(struct rx_session*s,rx_powercross_hud_state*out){
 if(!session_ok(s)||!out||out->size!=sizeof *out||out->version!=2)return 1;
 rx_powercross_hud_state v={0};v.size=sizeof v;v.version=2;
 rx_addr p=local_player(),a=p?U32(p,0x94):0;
 if(a&&U32(a,0)==0x943554&&I32(a,0x23c)==19&&I32(a,0x238)==1){
  v.valid=1;v.activity_type=19;v.activity_mode=1;v.activity_state=I32(a,0x21c);
  v.next_countdown_step=I32(a,0x2a0);v.board_present=U32(a,0x2cc)!=0;v.activity_clock=F32(a,0x294);
  v.component_bound=rx_powercross_countdown_copy(a,&v.visible,&v.displayed_step,&v.exited,&v.hold_remaining,&v.event_count);
 }
 *out=v;return 0;
}
