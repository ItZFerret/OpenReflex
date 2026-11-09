/* Targeted 00621640 contract. Source-evidenced fixtures from its COMISS/JB
 * instruction sequence (no independent oracle claim). Link after normal core build:
 * clang -std=c11 -Icore/src -Icore/game -c core/tools/rider_stance_test.c -o /tmp/rider_stance_test.o
 * clang++ -arch arm64 /tmp/rider_stance_test.o <active core objects from core/build.sh> -lz -o /tmp/rider_stance_test
 * /tmp/rider_stance_test
 */
#include "rx_mem.h"
#include "anim_scenario.h"
#include "race_beh1.h"
#include <stdio.h>
#include <string.h>
typedef struct test_case {
 const char* name;uint32_t mode,xlo,xhi,ylo,yhi,source_y,expected_x,expected_y;
 int null_x,null_y;
} test_case;
static const test_case cases[]={
 {"motorcycle unchanged",0,0xbf800000,0x3f800000,0xbf800000,0x3f800000,0x3f400000,0x3f000000,0x3f400000,0,0},
 {"ATV unchanged",1,0xbf800000,0x3f800000,0xbf800000,0x3f800000,0x3f000000,0x3ecccccd,0x3f000000,0,0},
 {"other mode zeros",2,0xbf800000,0x3f800000,0xbf800000,0x3f800000,0x3f400000,0,0,0,0},
 {"upper bounds",0,0xbf800000,0x3e800000,0xbf800000,0x3e000000,0x3f400000,0x3e800000,0x3e000000,0,0},
 {"lower bounds",1,0x3f19999a,0x3f800000,0x3f666666,0x3f800000,0x3f000000,0x3f19999a,0x3f666666,0,0},
 {"inverted bounds use lower",0,0x3f800000,0x3e800000,0x3f800000,0x3e000000,0x3f400000,0x3f800000,0x3f800000,0,0},
 {"NaN upper is unordered",0,0xbf800000,0x7fc12345,0xbf800000,0x7fc54321,0x3f400000,0x3f000000,0x3f400000,0,0},
 {"NaN lower is unordered",0,0x7fc12345,0x3f800000,0x7fc54321,0x3f800000,0x3f400000,0x3f000000,0x3f400000,0,0},
 {"NaN source payload retained",0,0xbf800000,0x3f800000,0xbf800000,0x3f800000,0x7fc54321,0x3f000000,0x7fc54321,0,0},
 {"signed zero bound selection",0,0x80000000,0x00000000,0x80000000,0x00000000,0x3f400000,0x80000000,0x80000000,0,0},
 {"null outputs",1,0xbf800000,0x3f800000,0xbf800000,0x3f800000,0x3f000000,0,0,1,1},
};
int main(void) {
 if(!rx_mem_init())return 2;
 const rx_addr ra=0x02000000,out_x=0x02100004,out_y=0x02100014;
 for(size_t i=0;i<sizeof cases/sizeof *cases;i++) {
  const test_case* c=cases+i;
  memset(rx_base+ra,0xa5,0x5bac0);
  SU32(ra,0x5bab8,c->mode);SU32(ra,0x3ec,c->xlo);SU32(ra,0x3f0,c->xhi);SU32(ra,0x3f8,c->ylo);SU32(ra,0x3fc,c->yhi);
  SU32(ra,0x3f4,c->null_x?0:out_x);SU32(ra,0x400,c->null_y?0:out_y);
  SU32(0,0x009d78a0,c->source_y);SU32(0,0x009d78b0,c->source_y);SU32(0,0x00d94ab0,0x7fa12345);
  for(int off=-4;off<=4;off+=4){SU32(out_x,off,0x12345678);SU32(out_y,off,0x87654321);}
  rx_rider_animation_stance2_outputs(ra);
  if(U32(out_x,0)!=(c->null_x?0x12345678:c->expected_x)||U32(out_y,0)!=(c->null_y?0x87654321:c->expected_y)) {
   fprintf(stderr,"FAIL %s outputs %08x / %08x\n",c->name,U32(out_x,0),U32(out_y,0));return 1;
  }
  const uint32_t expected_p=c->mode<2?0x3f000000:0;
  const uint32_t expected_q=c->mode==0?0x7fa12345:c->mode==1?0x3ecccccd:0;
  const uint32_t expected_on=c->mode<2?0x3f800000:0;
  if(U32(ra,0x3b4)!=expected_p||U32(ra,0x304)!=expected_q||U32(ra,0x35c)!=expected_on||U32(out_x,-4)!=0x12345678||U32(out_x,4)!=0x12345678||U32(out_y,-4)!=0x87654321||U32(out_y,4)!=0x87654321) {fprintf(stderr,"FAIL %s parameters/canaries\n",c->name);return 1;}
  printf("PASS %s\n",c->name);
 }
 /* The Dalton AI gate-target accessor copies raw x87-loaded floats, including
  * NaN quieting and sequential overlap. Keep these fixture expectations literal. */
 const uint32_t source[4][2]={{0x3f800000,0xc0200000},{0,0x80000000},{0x7f800000,0xff800000},{0x7fa12345,0xff812345}};
 const uint32_t expected[4][2]={{0x3f800000,0xc0200000},{0,0x80000000},{0x7f800000,0xff800000},{0x7fe12345,0xffc12345}};
 const rx_addr rule=0x02200000,pair=0x02201004;
 for(int i=0;i<4;i++) {
  SU32(rule,0x48,source[i][0]);SU32(rule,0x4c,source[i][1]);SU32(pair,-4,0x12345678);SU32(pair,8,0x87654321);
  rx_gate_rule_target(pair,rule);
  if(U32(pair,0)!=expected[i][0]||U32(pair,4)!=expected[i][1]||U32(pair,-4)!=0x12345678||U32(pair,8)!=0x87654321){fprintf(stderr,"FAIL gate target fixture %d\n",i);return 1;}
 }
 SU32(rule,0x48,0x7fa12345);SU32(rule,0x4c,0x3f800000);rx_gate_rule_target(rule+0x4c,rule);
 if(U32(rule,0x4c)!=0x7fe12345||U32(rule,0x50)!=0x7fe12345){puts("FAIL gate target sequential overlap");return 1;}
 puts("PASS gate target finite/zero/infinity/SNaN/canaries/overlap");
 rx_mem_shutdown();puts("PASS rider stance and gate target contract complete");return 0;
}
