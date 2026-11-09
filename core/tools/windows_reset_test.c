/* Standalone PE section/reset contract: build with reset_win.c, rename the two
 * objects' .data/.bss exactly as build_windows.sh does, then run on Windows.
 * No original assets, emulator, or whole core required. */
#include "rx_reset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static volatile unsigned initialized=0x12345678;
static volatile unsigned zeroed __attribute__((section(".bss$reset_zero")));
static _Thread_local volatile unsigned late_tls;
static volatile unsigned keep RX_RESET_KEEP;
/* This isolated fixture does not use the x87 implementation. */
void rx_x87_warm(void){}
int main(void){
    if(!rx_reset_available()||!rx_reset_capture())return 1;
    unsigned char* live=(unsigned char*)malloc(128);
    if(!live)return 2;
    memset(live,0x5a,128);initialized=42;zeroed=73;keep=91;late_tls=33;
    if(rx_reset_dirty()<2||rx_reset_restore()<2)return 3;
    if(initialized!=0x12345678||zeroed!=0||keep!=91||late_tls!=33)return 4;
    for(unsigned i=0;i<128;i++)if(live[i]!=0x5a)return 5;
    free(live);
    /* Snapshot bookkeeping survives a restore and a second reset. */
    initialized=99;zeroed=17;late_tls=44;
    if(!rx_reset_capture()||rx_reset_restore()<2)return 6;
    if(initialized!=0x12345678||zeroed||keep!=91||late_tls!=44)return 7;
    puts("PASS Windows PE core-only statics reset twice; KEEP and CRT heap survive");return 0;
}
