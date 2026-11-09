#pragma once
#include "rx_game.h"
/* Exact finite source-site sets, not address ranges or generic dispatch. */
static inline int rx_family_gear_site(rx_addr ra){
 static const rx_addr entries[]={0x4d3350,0x4d35c0,0x4b8ba0,0x4b8e10,0x455200,0x455470};
 for(unsigned i=0;i<6;i++)if(ra==entries[i]+0x1b7||ra==entries[i]+0x1e9)return 1;
 return 0;
}
static inline int rx_family_contact_site(rx_addr ra,rx_addr target){
 static const rx_addr entries[]={0x452a90,0x4b54c0};
 static const unsigned curves[]={0x6e,0x87,0x158,0x171,0x242,0x25b,0x32c,0x345};
 static const unsigned forces[]={0x492,0x49d,0x4a8,0x4b3,0x4be};
 for(unsigned f=0;f<2;f++){
  if(target==0x4390d0)for(unsigned i=0;i<8;i++)if(ra==entries[f]+curves[i])return 1;
  if(target==0x85c770)for(unsigned i=0;i<5;i++)if(ra==entries[f]+forces[i])return 1;
  if(target==0x8f9efe&&ra==entries[f]+0x50a)return 1;
 }
 return 0;
}
