#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_alloc,allocations;
static void *test_alloc(size_t n){++allocations;return fail_alloc?NULL:malloc(n);}
#define RX_TERRAIN_CACHE_ALLOC test_alloc
#define RX_TERRAIN_CACHE_FREE free
#include "../api/terrain_dirty_cache.h"
int main(void){
 struct terrain_dirty_cache c={0};struct terrain_dirty_key k={{1}};
 unsigned char grid[4*24];memset(grid,0,sizeof grid);
 assert(terrain_dirty_update(&c,&k,0,4,grid,24));assert(c.x0==0&&c.z0==0&&c.x1==4&&c.z1==4&&c.version==1);
 assert(terrain_dirty_update(&c,&k,1,4,grid,24));assert(c.x1==-1&&c.compared_rows==4&&c.scanned_words==0);
 grid[24+4]=1;assert(terrain_dirty_update(&c,&k,1,4,grid,24));assert(c.x1==-1); /* idempotent within frame */
 assert(terrain_dirty_update(&c,&k,2,4,grid,24));assert(c.x0==1&&c.x1==2&&c.z0==1&&c.z1==2&&c.scanned_words==4&&c.version==2);
 grid[0]=1;grid[3*24+3*4]=1;assert(terrain_dirty_update(&c,&k,3,4,grid,24));assert(c.x0==0&&c.x1==4&&c.z0==0&&c.z1==4&&c.scanned_words==8);
 grid[4*4]=99; /* row padding must not trigger dirtiness */
 assert(terrain_dirty_update(&c,&k,4,4,grid,24));assert(c.x1==-1);
 k.word[1]=7;assert(terrain_dirty_update(&c,&k,4,4,grid,24));assert(c.x1==4); /* resource reload, same frame */
 k.word[2]=1;assert(terrain_dirty_update(&c,&k,5,4,NULL,0));assert(!c.previous&&c.bytes==0&&c.x1==4);
 assert(terrain_dirty_update(&c,&k,6,4,NULL,0));assert(c.x1==-1);
 k.word[2]=2;fail_alloc=1;struct terrain_dirty_cache old=c;
 assert(!terrain_dirty_update(&c,&k,7,4,grid,24));assert(!memcmp(&old,&c,sizeof c));fail_alloc=0;
 assert(terrain_dirty_update(&c,&k,7,4,grid,24));assert(c.x1==4);
 old=c;assert(!terrain_dirty_update(&c,&k,8,0,grid,24));assert(!terrain_dirty_update(&c,&k,8,4,grid,15));assert(!terrain_dirty_update(&c,&k,8,UINT32_MAX,grid,24));assert(!terrain_dirty_update(&c,&k,8,4,grid,SIZE_MAX));assert(!memcmp(&old,&c,sizeof c));
 unsigned char small[16]={0};k.word[3]=1;assert(terrain_dirty_update(&c,&k,9,2,small,8));assert(c.bytes==16&&c.x1==2);
 /* Raw -0/NaN payload bits are changes, not float-equality shortcuts. */
 small[3]=128;small[12]=1;small[15]=127;assert(terrain_dirty_update(&c,&k,10,2,small,8));assert(c.x0==0&&c.z0==0&&c.x1==2&&c.z1==2);
 uint32_t prior_version=c.version;terrain_dirty_invalidate(&c);assert(!c.previous&&!c.valid&&c.version==prior_version);
 assert(terrain_dirty_update(&c,&k,10,2,small,8));assert(c.version==prior_version+1&&c.x1==2);
 /* Recycled terrain/grid addresses with identical dimensions/data must still
  * publish a new full generation after event/reset invalidation. */
 prior_version=c.version;terrain_dirty_invalidate(&c);terrain_dirty_invalidate(&c);
 assert(terrain_dirty_update(&c,&k,10,2,small,8));assert(c.version==prior_version+1&&c.x1==2);
 terrain_dirty_dispose(&c);assert(!c.previous&&!c.valid&&!c.version);
 /* A single texel missed by the old8-texel coarse sampling must be observed. */
 size_t n=2048;unsigned char *big=calloc(n*n,4);assert(big);
 assert(terrain_dirty_update(&c,&k,0,(uint32_t)n,big,n*4));big[(n+1)*4]=1;
 assert(terrain_dirty_update(&c,&k,1,(uint32_t)n,big,n*4));assert(c.x0==1&&c.z0==1&&c.x1==2&&c.z1==2&&c.compared_rows==n&&c.scanned_words==n);
 int before=allocations;assert(terrain_dirty_update(&c,&k,1,(uint32_t)n,big,n*4));assert(allocations==before);
 /* w-perfgrow: a hint (the rectangle that can have changed) gives the same rectangle, version and previous copy as the full scan, scans only
  * its rows and columns, an empty hint is no change, and verify catches a change outside the hint. */
 {
  struct terrain_dirty_cache h={0};terrain_dirty_dispose(&c);
  unsigned char *g2=calloc(n*n,4);assert(g2);
  assert(terrain_dirty_update(&c,&k,0,(uint32_t)n,g2,n*4));assert(terrain_dirty_update(&h,&k,0,(uint32_t)n,g2,n*4));
  g2[(5*n+7)*4]=1;g2[(9*n+30)*4+1]=2;   /* two texels */
  const int32_t hint[4]={0,0,64,64};int bad=0;
  assert(terrain_dirty_update_hint(&h,&k,1,(uint32_t)n,g2,n*4,hint,1,&bad));assert(!bad);
  assert(terrain_dirty_update(&c,&k,1,(uint32_t)n,g2,n*4));
  assert(h.x0==c.x0&&h.z0==c.z0&&h.x1==c.x1&&h.z1==c.z1&&h.version==c.version&&h.x0==7&&h.x1==31&&h.z0==5&&h.z1==10);
  assert(h.compared_rows==64&&h.scanned_words==2*(size_t)(64)&&c.compared_rows==n);   /* only the hint's rows (and its columns) were looked at */
  const int32_t none[4]={10,10,10,10};   /* an empty hint: no change reported, nothing compared */
  assert(terrain_dirty_update_hint(&h,&k,2,(uint32_t)n,g2,n*4,none,1,&bad));assert(!bad&&h.x1==-1&&h.compared_rows==0&&h.version==c.version);
  g2[(100*n+100)*4]=3;   /* a change outside the hint: verify flags it */
  assert(terrain_dirty_update_hint(&h,&k,3,(uint32_t)n,g2,n*4,hint,1,&bad));assert(bad&&h.x1==-1);
  assert(terrain_dirty_pending(&h,&k,3)==0&&terrain_dirty_pending(&h,&k,4)==1);
  free(g2);terrain_dirty_dispose(&h);
 }
 free(big);terrain_dirty_dispose(&c);puts("PASS exact rectangles/padding/rawbits/2048 subsample/idempotence/reload/presence/dimensions/overflow/allocation failure/teardown/dirty-rectangle hint");return 0;
}
