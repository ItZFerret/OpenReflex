/* Includes actual production sampler; no game/session execution. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../game/fx_deform.c"
static unsigned checks;
static void eq(float a,float b){assert(rx_fb(a)==rx_fb(b));++checks;}
static void near(float a,double b){assert(fabs((double)a-b)<0.0000002);++checks;}
int main(void){
 unsigned char equal[16*16*3];for(unsigned i=0;i<sizeof equal;i++)equal[i]=(unsigned char)(i*37u+19u);
 memset(&g_model,0,sizeof g_model);g_model.mask=equal;g_model.mask_w=g_model.mask_h=16;g_model.mask_mips=1;g_model.softness[0]=(mip){16,16,equal};
 for(int y=0;y<16;y++)for(int x=0;x<16;x++){float got[3];softness_sample(16,x,y,got);for(int c=0;c<3;c++)eq(got[c],(float)((double)equal[(y*16+x)*3+c]/255.0));}
 unsigned char small[]={0,0,0,64,64,64,128,128,128,255,255,255};g_model.mask=small;g_model.mask_w=g_model.mask_h=2;g_model.softness[0]=(mip){2,2,small};
 float out[3];softness_sample(4,0,0,out);for(int c=0;c<3;c++)eq(out[c],0.f);
 softness_sample(4,3,3,out);for(int c=0;c<3;c++)eq(out[c],1.f);
 softness_sample(4,1,1,out);for(int c=0;c<3;c++)near(out[c],(double)(float)(64.0/255)*.1875+(double)(float)(128.0/255)*.1875+.0625);
 /* All four texels influence normalized-UV centre; differs from old fallback. */
 softness_sample(1,0,0,out);for(int c=0;c<3;c++)near(out[c],((double)(float)(64.0/255)+(double)(float)(128.0/255)+1.0)*.25);
 unsigned char l0[8*8*3],l1[4*4*3],l2[2*2*3];memset(l0,10,sizeof l0);memset(l1,50,sizeof l1);memset(l2,90,sizeof l2);
 g_model.mask=l0;g_model.mask_w=g_model.mask_h=8;g_model.mask_mips=3;g_model.softness[0]=(mip){8,8,l0};g_model.softness[1]=(mip){4,4,l1};g_model.softness[2]=(mip){2,2,l2};
 softness_sample(2,0,0,out);for(int c=0;c<3;c++)eq(out[c],(float)(90.0/255));
 g_model.mask_mips=2;softness_sample(2,1,1,out);for(int c=0;c<3;c++)eq(out[c],(float)(50.0/255));
 g_model.mask_mips=3;softness_sample(6,0,0,out);for(int c=0;c<3;c++)eq(out[c],(float)(10.0/255)); /* log2(8/6)<.5 */
 softness_sample(5,0,0,out);for(int c=0;c<3;c++)eq(out[c],(float)(50.0/255)); /* log2(8/5)>.5 */
 /* Non-square map selects from max footprint dimension, then clamps edges. */
 g_model.mask_w=8;g_model.mask_h=4;g_model.softness[2]=(mip){2,1,small};
 softness_sample(2,0,1,out);for(int c=0;c<3;c++)eq(out[c],0.f);
 softness_sample(2,1,0,out);for(int c=0;c<3;c++)eq(out[c],(float)(64.0/255));
 printf("PASS softness_sampling checks=%u equalSizeBitExact=768 noSession=1\n",checks);return 0;
}
