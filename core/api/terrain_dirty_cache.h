/* Host-only renderer export cache. Never changes guest terrain or physics. */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef RX_TERRAIN_CACHE_ALLOC
#define RX_TERRAIN_CACHE_ALLOC malloc
#define RX_TERRAIN_CACHE_FREE free
#endif
struct terrain_dirty_key { uint32_t word[16]; };
/* w-terrtiles: the changed texels as 32 x 32 tiles, merged into at most TERRAIN_DIRTY_MAX_RECTS rectangles (x1 / z1 exclusive, tile
 * aligned: every edge but a grid-end one is a multiple of TERRAIN_DIRTY_TILE). */
#define TERRAIN_DIRTY_TILE 32
#define TERRAIN_DIRTY_MAX_RECTS 16
#define TERRAIN_DIRTY_WORK_RECTS 48
struct terrain_dirty_rect { int32_t x0,z0,x1,z1; };
struct terrain_dirty_cache {
    uint32_t *previous;
    size_t bytes;
    struct terrain_dirty_key key;
    uint64_t frame;
    uint32_t version;
    int valid;
    int32_t x0,z0,x1,z1;
    /* Host diagnostic counters, useful for focused complexity tests. */
    size_t compared_rows, scanned_words;
    /* the tile rectangles of the last update (nrects 0 = nothing changed); tiles is a byte per tile, all zero between updates */
    uint8_t *tiles; size_t tiles_cap;
    int nrects; struct terrain_dirty_rect rects[TERRAIN_DIRTY_MAX_RECTS];
    size_t changed_tiles;
};
static void terrain_dirty_dispose(struct terrain_dirty_cache *c) {
    RX_TERRAIN_CACHE_FREE(c->previous); RX_TERRAIN_CACHE_FREE(c->tiles); memset(c,0,sizeof *c);
}
/* Drop the old terrain generation while retaining a monotonically changing
 * version: managed consumers compare version before looking at dirty bounds. */
static void terrain_dirty_invalidate(struct terrain_dirty_cache *c) {
    uint32_t version=c->version;terrain_dirty_dispose(c);c->version=version;
}

static int64_t terrain_dirty_area(const struct terrain_dirty_rect *r) { return (int64_t)(r->x1-r->x0)*(int64_t)(r->z1-r->z0); }
static struct terrain_dirty_rect terrain_dirty_bbox(const struct terrain_dirty_rect *a,const struct terrain_dirty_rect *b) {
    struct terrain_dirty_rect r={a->x0<b->x0?a->x0:b->x0,a->z0<b->z0?a->z0:b->z0,a->x1>b->x1?a->x1:b->x1,a->z1>b->z1?a->z1:b->z1};return r;
}
/* Merge down to `target` (>= 1) rectangles: repeatedly the pair whose bounding box adds the least area to the two (ties: the first pair),
 * then any rectangles that overlap. A bounding box may cover texels no tile of the pair did, never fewer. */
static void terrain_dirty_reduce(struct terrain_dirty_rect *r,int *n,int target) {
    while(*n>target) {
        int bi=0,bj=1;int64_t best=INT64_MAX;
        for(int i=0;i<*n;i++)for(int j=i+1;j<*n;j++) {
            const struct terrain_dirty_rect b=terrain_dirty_bbox(&r[i],&r[j]);
            const int64_t add=terrain_dirty_area(&b)-terrain_dirty_area(&r[i])-terrain_dirty_area(&r[j]);
            if(add<best){best=add;bi=i;bj=j;}
        }
        r[bi]=terrain_dirty_bbox(&r[bi],&r[bj]);r[bj]=r[--*n];
    }
    for(int again=1;again;) {   /* overlapping rectangles would upload the same texels twice */
        again=0;
        for(int i=0;i<*n&&!again;i++)for(int j=i+1;j<*n;j++)
            if(r[i].x0<r[j].x1&&r[j].x0<r[i].x1&&r[i].z0<r[j].z1&&r[j].z0<r[i].z1){
                r[i]=terrain_dirty_bbox(&r[i],&r[j]);r[j]=r[--*n];again=1;break;
            }
    }
}
/* The rectangles of the set tiles of a bitmap (tw x th tiles, row-major, grid n x n texels) inside the tile box [tx0,tx1] x [tz0,tz1]:
 * runs of set tiles per tile row, a run extends the rectangle above it that has the same columns (an exact cover, no unset tile
 * inside), then merged down to TERRAIN_DIRTY_MAX_RECTS. Returns the number of rectangles; *changed = the set tiles. */
static int terrain_dirty_tile_rects(const uint8_t *tiles,uint32_t tw,uint32_t n,uint32_t tx0,uint32_t tx1,uint32_t tz0,uint32_t tz1,
                                    struct terrain_dirty_rect *out,size_t *changed) {
    struct terrain_dirty_rect w[TERRAIN_DIRTY_WORK_RECTS];int nw=0;size_t count=0;
    for(uint32_t tz=tz0;tz<=tz1;tz++) {
        const uint8_t *row=tiles+(size_t)tz*tw;
        const int32_t z0=(int32_t)(tz*TERRAIN_DIRTY_TILE),z1=(int32_t)((tz+1)*TERRAIN_DIRTY_TILE<n?(tz+1)*TERRAIN_DIRTY_TILE:n);
        for(uint32_t tx=tx0;tx<=tx1;) {
            if(!row[tx]){++tx;continue;}
            uint32_t te=tx;while(te<=tx1&&row[te])++te;
            count+=te-tx;
            const int32_t x0=(int32_t)(tx*TERRAIN_DIRTY_TILE),x1=(int32_t)(te*TERRAIN_DIRTY_TILE<n?te*TERRAIN_DIRTY_TILE:n);
            int hit=-1;
            for(int k=0;k<nw;k++)if(w[k].x0==x0&&w[k].x1==x1&&w[k].z1==z0){hit=k;break;}
            if(hit>=0)w[hit].z1=z1;
            else {
                if(nw==TERRAIN_DIRTY_WORK_RECTS)terrain_dirty_reduce(w,&nw,TERRAIN_DIRTY_MAX_RECTS);
                w[nw].x0=x0;w[nw].z0=z0;w[nw].x1=x1;w[nw].z1=z1;++nw;
            }
            tx=te;
        }
    }
    terrain_dirty_reduce(w,&nw,TERRAIN_DIRTY_MAX_RECTS);
    for(int k=0;k<nw;k++)out[k]=w[k];
    if(changed)*changed=count;
    return nw;
}
/* rows==NULL denotes absent deformation (the exported heights use neutral0.5).
 * Input row storage must span (n-1)*pitch+n*4 bytes. Identity includes presence.
 * Failure leaves the previously committed cache intact. */
/* 1 when terrain_dirty_update would do work for this key and frame (it returns early for the same key and frame). */
static int terrain_dirty_pending(const struct terrain_dirty_cache *c,const struct terrain_dirty_key *key,uint64_t frame) {
    return !(c->valid && !memcmp(&c->key,key,sizeof *key) && c->frame==frame);
}
/* hint (optional, w-perfgrow): texels [hint[0],hint[2]) x [hint[1],hint[3]) are the only ones that can differ from the previous copy
 * (the D3D layer's dirty rectangle, rx_d3d_track_take); NULL = look at every row. Used only when the surface is the same as last time.
 * verify: after the update the previous copy must equal the surface everywhere (no change outside the hint), else *bad = 1. */
static int terrain_dirty_update_hint(struct terrain_dirty_cache *c,const struct terrain_dirty_key *key,
                                uint64_t frame,uint32_t n,const void *rows,size_t pitch,const int32_t *hint,int verify,int *bad);
static int terrain_dirty_update(struct terrain_dirty_cache *c,const struct terrain_dirty_key *key,
                                uint64_t frame,uint32_t n,const void *rows,size_t pitch) {
    return terrain_dirty_update_hint(c,key,frame,n,rows,pitch,NULL,0,NULL);
}
static int terrain_dirty_update_hint(struct terrain_dirty_cache *c,const struct terrain_dirty_key *key,
                                uint64_t frame,uint32_t n,const void *rows,size_t pitch,const int32_t *hint,int verify,int *bad) {
    if(!n || n>INT32_MAX || (size_t)n>SIZE_MAX/4 || (size_t)n>SIZE_MAX/((size_t)n*4))return 0;
    size_t row_bytes=(size_t)n*4, bytes=(size_t)n*row_bytes;
    if(rows && (pitch<row_bytes || (n>1 && pitch>(SIZE_MAX-row_bytes)/(n-1))))return 0;
    int same=c->valid && !memcmp(&c->key,key,sizeof *key);
    if(same && c->frame==frame)return 1;
    uint32_t *next=c->previous;
    if(rows && (!next || c->bytes!=bytes)) {
        next=(uint32_t*)RX_TERRAIN_CACHE_ALLOC(bytes);
        if(!next)return 0;
    }
    int32_t x0=INT32_MAX,z0=INT32_MAX,x1=-1,z1=-1;
    size_t compared=0,scanned=0;
    /* the tile bitmap (kept all zero between updates); without memory the tiles are skipped and the changed box is one rectangle */
    const uint32_t tw=(n+TERRAIN_DIRTY_TILE-1)/TERRAIN_DIRTY_TILE;const size_t tile_bytes=(size_t)tw*tw;
    uint8_t *tiles=c->tiles;size_t tiles_cap=c->tiles_cap;
    if(same && rows && tile_bytes>tiles_cap) {
        uint8_t *t=(uint8_t*)RX_TERRAIN_CACHE_ALLOC(tile_bytes);
        if(t){memset(t,0,tile_bytes);RX_TERRAIN_CACHE_FREE(tiles);tiles=t;tiles_cap=tile_bytes;}
    }
    uint8_t *use=tile_bytes<=tiles_cap?tiles:NULL;   /* NULL: no bitmap this update */
    if(!same) {
        x0=z0=0;x1=z1=(int32_t)n;
        if(rows)for(uint32_t z=0;z<n;z++)memcpy(next+(size_t)z*n,(const unsigned char*)rows+(size_t)z*pitch,row_bytes);
    } else if(rows) {
        uint32_t zlo=0,zhi=n,xlo=0,xhi=n;   /* the rows / columns that can differ: all, or the hint */
        if(hint) {
            int32_t a=hint[0]<0?0:hint[0],b=hint[1]<0?0:hint[1],e=hint[2]>(int32_t)n?(int32_t)n:hint[2],f=hint[3]>(int32_t)n?(int32_t)n:hint[3];
            if(e<=a||f<=b){zlo=zhi=0;} else {xlo=(uint32_t)a;xhi=(uint32_t)e;zlo=(uint32_t)b;zhi=(uint32_t)f;}
        }
        const size_t span=(size_t)(xhi-xlo)*4;
        for(uint32_t z=zlo;z<zhi;z++) {
            const unsigned char *r=(const unsigned char*)rows+(size_t)z*pitch;
            uint32_t *p=next+(size_t)z*n;++compared;
            if(!memcmp(p+xlo,r+(size_t)xlo*4,span))continue;
            if((int32_t)z<z0)z0=(int32_t)z;if((int32_t)z+1>z1)z1=(int32_t)z+1;
            for(uint32_t x=xlo;x<xhi;x++) {
                uint32_t v;memcpy(&v,r+(size_t)x*4,4);++scanned;
                if(p[x]!=v){if((int32_t)x<x0)x0=(int32_t)x;if((int32_t)x+1>x1)x1=(int32_t)x+1;if(use)use[(size_t)(z/TERRAIN_DIRTY_TILE)*tw+x/TERRAIN_DIRTY_TILE]=1;}
            }
            memcpy(p+xlo,r+(size_t)xlo*4,span);
        }
        if(verify && bad)for(uint32_t z=0;z<n;z++)
            if(memcmp(next+(size_t)z*n,(const unsigned char*)rows+(size_t)z*pitch,row_bytes)){*bad=1;break;}
    }
    if(next!=c->previous)RX_TERRAIN_CACHE_FREE(c->previous);
    if(!rows){RX_TERRAIN_CACHE_FREE(next);next=NULL;bytes=0;}
    uint32_t version=c->version;
    if(!c->valid || x1>=0){++version;if(!version)version=1;}
    c->previous=next;c->bytes=bytes;c->key=*key;c->frame=frame;c->version=version;c->valid=1;
    c->x0=x0;c->z0=z0;c->x1=x1;c->z1=z1;c->compared_rows=compared;c->scanned_words=scanned;
    c->tiles=tiles;c->tiles_cap=tiles_cap;c->nrects=0;c->changed_tiles=0;
    if(!same) {   /* a new generation: everything */
        c->rects[0].x0=0;c->rects[0].z0=0;c->rects[0].x1=(int32_t)n;c->rects[0].z1=(int32_t)n;c->nrects=1;c->changed_tiles=(size_t)tw*tw;
    } else if(x1>=0) {
        const uint32_t tx0=(uint32_t)x0/TERRAIN_DIRTY_TILE,tx1=(uint32_t)(x1-1)/TERRAIN_DIRTY_TILE,tz0=(uint32_t)z0/TERRAIN_DIRTY_TILE,tz1=(uint32_t)(z1-1)/TERRAIN_DIRTY_TILE;
        if(use) {
            c->nrects=terrain_dirty_tile_rects(use,tw,n,tx0,tx1,tz0,tz1,c->rects,&c->changed_tiles);
            for(uint32_t tz=tz0;tz<=tz1;tz++)memset(use+(size_t)tz*tw+tx0,0,tx1-tx0+1);
        } else {   /* no bitmap: the tile-aligned box around the change */
            c->rects[0].x0=(int32_t)(tx0*TERRAIN_DIRTY_TILE);c->rects[0].z0=(int32_t)(tz0*TERRAIN_DIRTY_TILE);
            c->rects[0].x1=(int32_t)((tx1+1)*TERRAIN_DIRTY_TILE<n?(tx1+1)*TERRAIN_DIRTY_TILE:n);
            c->rects[0].z1=(int32_t)((tz1+1)*TERRAIN_DIRTY_TILE<n?(tz1+1)*TERRAIN_DIRTY_TILE:n);c->nrects=1;
        }
    }
    return 1;
}
