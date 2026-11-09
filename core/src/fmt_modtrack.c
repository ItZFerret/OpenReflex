#include "fmt_modtrack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <zlib.h>
static float* heights;
static uint8_t* surface;
static float pose[6];
static struct { uint32_t driver; int lap,sector; } lap_gates[32];
static struct { uint32_t driver,node; float time; int attempts; } recoveries[32];
static uint8_t* race_stream; static uint32_t race_size; static float race_gate[10];
static int build_race(const uint8_t* data,uint32_t size,uint8_t** out,uint32_t* bytes,float gate[10]);
int fmt_modtrack_racing(void) { return race_stream != NULL; }
const float* fmt_modtrack_gate(void) { return race_gate; }
const uint8_t* fmt_modtrack_race_stream(uint32_t* size) { *size=race_size;return race_stream; }
static uint8_t* collision; static uint32_t collision_count, collision_world;
int fmt_modtrack_active(void) { return heights != NULL; }
void fmt_modtrack_clear(void) { memset(recoveries,0,sizeof recoveries); memset(lap_gates,0,sizeof lap_gates); free(race_stream);race_stream=NULL;race_size=0; free(heights); heights = NULL; free(surface); surface=NULL; memset(pose,0,sizeof pose); free(collision); collision=NULL; collision_count=collision_world=0; }
float fmt_modtrack_height(uint32_t i) { return heights && i < 2048u*2048u ? heights[i] : 0; }
int fmt_modtrack_open(const char* folder) {
    char path[4096];
    if (!folder || snprintf(path,sizeof path,"%s",folder) >= (int)sizeof path) return -1;
    if(!strstr(path,".rxtrack")) return 0;
    gzFile f=gzopen(path,"rb"); if (!f) return -1;
    uint32_t package[3];
    if(gzread(f,package,12)!=12 || memcmp(package,"RXPK",4) || (package[1]<1 || package[1]>4) || package[2]>65536) { gzclose(f); return -1; }
    char metadata[65536]; if(gzread(f,metadata,package[2])!=(int)package[2]) { gzclose(f); return -1; }
    uint32_t header[4]; float p[6]; float* data=NULL; uint8_t* coll=NULL; uint8_t* surf=NULL; uint32_t chunks=0; int ok=0; uint8_t* race=NULL; uint32_t rsize=0; float gate[10];
    if (gzread(f,header,sizeof header)!=sizeof header || memcmp(header,"RXM1",4) || header[1]!=1 || header[2]!=2048 || header[3]!=0x40000000u) goto done;
    if (gzread(f,p,sizeof p)!=sizeof p) goto done;
    for(int i=0;i<6;i++) if(!isfinite(p[i])) goto done;
    if(p[3]*p[3]+p[5]*p[5]<0.01f) goto done;
    data=malloc(2048u*2048u*sizeof(float)); if(!data) goto done;
    if(gzread(f,data,2048u*2048u*sizeof(float))!=2048u*2048u*sizeof(float)) goto done;
    if(package[1]>=2) {
        uint32_t section[2]; char scratch[8192];
        if(gzread(f,section,8)!=8 || memcmp(section,"SCNE",4) || section[1]<8 || section[1]>128u*1024u*1024u) goto done;
        uint32_t left=section[1];
        while(left) { unsigned count=left>sizeof scratch?sizeof scratch:left; if(gzread(f,scratch,count)!=(int)count) goto done; left-=count; }
        if(gzread(f,section,8)!=8 || memcmp(section,"COLL",4) || section[1]<12 || section[1]>32u*1024u*1024u) goto done;
        coll=malloc(section[1]);if(!coll || gzread(f,coll,section[1])!=(int)section[1]) goto done;
        uint32_t version;memcpy(&version,coll+4,4);memcpy(&chunks,coll+8,4);
        if(memcmp(coll,"RXCO",4)||(version!=1&&version!=2)||chunks>10000) goto done;
        size_t offset=12;
        for(uint32_t chunk=0;chunk<chunks;chunk++) {
            uint32_t count;if(offset+4>section[1])goto done;memcpy(&count,coll+offset,4);offset+=4;
            if(!count || count>48000 || count%3 || offset+(size_t)count*12>section[1])goto done;
            for(uint32_t k=0;k<count*3;k++){float value;memcpy(&value,coll+offset+4*k,4);if(!isfinite(value)||fabsf(value)>100000)goto done;}
            offset+=(size_t)count*12;
            if(version==2) {
                if(offset+(size_t)count*16>section[1])goto done;
                for(uint32_t k=0;k<count*4;k++){float v;memcpy(&v,coll+offset+4*k,4);if(!isfinite(v)||fabsf(v)>100000||(k%4==3&&v!=0&&v!=1))goto done;}
                offset+=(size_t)count*16;
            }
        }
        if(offset!=section[1])goto done;
    }
    if(package[1]>=3) {
        uint32_t section[2], h[3];
        if(gzread(f,section,8)!=8 || memcmp(section,"SURF",4) || section[1]!=12+2048u*2048u*4u) goto done;
        if(gzread(f,h,12)!=12 || memcmp(h,"RXSM",4) || h[1]!=1 || h[2]!=2048) goto done;
        surf=malloc(2048u*2048u*4u);if(!surf || gzread(f,surf,2048u*2048u*4u)!=2048u*2048u*4u)goto done;
        for(uint32_t i=0;i<2048u*2048u;i++)if((uint32_t)surf[4*i]+surf[4*i+1]+surf[4*i+2]>255)goto done;
    }
    if(package[1]>=4) {
        uint32_t section[2],h[3]; char scratch[8192];
        if(gzread(f,section,8)!=8 || memcmp(section,"COLR",4) || section[1]!=12+2048u*2048u*4u) goto done;
        if(gzread(f,h,12)!=12 || memcmp(h,"RXCM",4) || h[1]!=1 || h[2]!=2048) goto done;
        uint32_t left=2048u*2048u*4u;
        while(left) { unsigned count=left>sizeof scratch?sizeof scratch:left; if(gzread(f,scratch,count)!=(int)count) goto done; left-=count; }
    }
    int next=gzgetc(f);
    if(next!=-1) {
        uint32_t section[2]; ((uint8_t*)section)[0]=(uint8_t)next;
        if(gzread(f,(uint8_t*)section+1,7)!=7 || memcmp(section,"RACE",4) || section[1]<100 || section[1]>164000)goto done;
        uint8_t* payload=malloc(section[1]);
        if(!payload)goto done;
        int valid=gzread(f,payload,section[1])==(int)section[1] && build_race(payload,section[1],&race,&rsize,gate);
        free(payload);if(!valid || gzgetc(f)!=-1)goto done;
    }
    if(!gzeof(f))goto done;
    for(uint32_t i=0;i<2048u*2048u;i++) if(!isfinite(data[i]) || fabsf(data[i])>100000) goto done;
    fmt_modtrack_clear();race_stream=race;race=NULL;race_size=rsize;if(rsize)memcpy(race_gate,gate,sizeof gate); surface=surf;surf=NULL; heights=data; data=NULL; collision=coll;coll=NULL;collision_count=chunks; memcpy(pose,p,sizeof pose); ok=1;
 done: free(race); free(surf); free(coll); free(data); gzclose(f); return ok?1:-1;
}
void fmt_modtrack_start(float out[16]) {
    float length=sqrtf(pose[3]*pose[3]+pose[5]*pose[5]), x=pose[3]/length,z=pose[5]/length;
    memset(out,0,16*sizeof(float)); out[0]=z;out[2]=-x;out[5]=1;out[8]=x;out[10]=z;out[15]=1;
    out[12]=pose[0];out[13]=pose[1];out[14]=pose[2];
}

int fmt_modtrack_collision_claim(uint32_t world) { if(!collision || collision_world==world)return 0;collision_world=world;return 1; }
uint32_t fmt_modtrack_collision_count(void) { return collision_count; }
const float* fmt_modtrack_collision_chunk(uint32_t index,uint32_t* vertices) {
    if(!collision||index>=collision_count)return NULL;
    size_t offset=12;uint32_t count=0;
    for(uint32_t k=0;k<=index;k++){memcpy(&count,collision+offset,4);offset+=4;if(k!=index)offset+=(size_t)count*(collision[4]==2?28:12);}
    *vertices=count;return (const float*)(collision+offset);
}

void fmt_modtrack_softness(uint32_t x,uint32_t z,float out[3]) {
    if(!surface || x>=2048 || z>=2048){out[0]=out[1]=out[2]=1;return;}
    const uint8_t* p=surface+4*(z*2048+x);float grass=p[0]/255.f,rock=p[2]/255.f;
    out[0]=.85f*(1-grass-rock)+.30f*grass+.05f*rock;
    out[1]=.65f*(1-grass-rock)+.25f*grass+.05f*rock;out[2]=1;
}

const float* fmt_modtrack_collision_anchors(uint32_t index) {
    uint32_t count;const float* points=fmt_modtrack_collision_chunk(index,&count);
    return points && collision[4]==2 ? points+count*3 : NULL;
}
void fmt_modtrack_scale_point(const float* point,const float* anchor,float scale,float out[3]) {
    for(int k=0;k<3;k++)out[k]=anchor && anchor[3]==1 ? point[k]+anchor[k]*(scale-1) : point[k]*scale;
}

/* Translate the bounded, portable RXRA route to an in-memory stream for the
 * original Reflex parser. This is not stored in or resolved from a database. */
typedef struct { uint8_t* data; uint32_t pos; } race_writer;
static void rw_u(race_writer* w,uint32_t v) { v=__builtin_bswap32(v);memcpy(w->data+w->pos,&v,4);w->pos+=4; }
static void rw_f(race_writer* w,float f) { uint32_t v;memcpy(&v,&f,4);rw_u(w,v); }
static void rw_tag(race_writer* w,uint32_t t,uint32_t n) { rw_u(w,t);rw_u(w,n); }
static void rw_word(race_writer* w,uint32_t t,uint32_t v) { rw_tag(w,t,4);rw_u(w,v); }
static void rw_float(race_writer* w,uint32_t t,float v) { rw_tag(w,t,4);rw_f(w,v); }
static void rw_vec(race_writer* w,uint32_t t,float x,float y,float z) { rw_tag(w,t,12);rw_f(w,x);rw_f(w,y);rw_f(w,z); }
static void rw_grid(race_writer* w,uint32_t t,uint32_t v) { rw_tag(w,t,136);rw_u(w,8);rw_u(w,4);for(int i=0;i<32;i++)rw_u(w,v); }
static void rw_marker(race_writer* w,uint32_t t,const float* p,const float* d,float width,uint32_t sec,uint32_t node) {
    rw_tag(w,t,48);
    rw_f(w,p[0]);rw_f(w,5001);rw_f(w,p[2]);rw_u(w,0);
    rw_f(w,d[0]);rw_f(w,0);rw_f(w,d[2]);rw_u(w,0);
    rw_f(w,width);rw_u(w,0x01000000); /* enabled byte is not byte-swapped by the native reader */
    rw_u(w,sec);rw_u(w,node);
}
static float route_length(const float* p,uint32_t n) {
    float total=0;for(uint32_t i=1;i<n;i++)total+=hypotf(p[5*i]-p[5*i-5],p[5*i+2]-p[5*i-3]);return total;
}
static int build_race(const uint8_t* data,uint32_t size,uint8_t** out,uint32_t* bytes,float gate[10]) {
    if(size<100||memcmp(data,"RXRA",4))return 0;
    uint32_t h[4];memcpy(h,data+4,16);
    const uint32_t n=h[2],m=h[3];
    if(h[0]!=1||h[1]>1||n<10||m<2||n>4096||m>4096||size!=100+20*(n+m))return 0;
    const float* g=(const float*)(data+20);const float* lap=(const float*)(data+100);const float* lead=lap+5*n;
    for(uint32_t i=0;i<20+5*(n+m);i++)if(!isfinite(g[i])||fabsf(g[i])>100000)return 0;
    for(int i=0;i<2;i++)if(g[10*i+9]<10||g[10*i+9]>300||fabsf(hypotf(g[10*i+3],g[10*i+5])-1)>.01f)return 0;
    for(uint32_t i=0;i<n+m;i++)if(lap[5*i+3]<6||lap[5*i+3]>300||lap[5*i+4]<1||lap[5*i+4]>200)return 0;
    if(hypotf(lap[0]-lap[5*(n-1)],lap[2]-lap[5*(n-1)+2])>.1f || hypotf(lap[0]-lead[5*(m-1)],lap[2]-lead[5*(m-1)+2])>.1f)return 0;
    const float lengths[2]={route_length(lap,n),route_length(lead,m)};
    if(lengths[0]<100||lengths[1]<1)return 0;
    race_writer w={malloc((n+m)*1800u+8192u),0};if(!w.data)return 0;
    rw_u(&w,n+m);rw_u(&w,2);rw_u(&w,4);rw_u(&w,4);
    rw_word(&w,0x1f,h[1]);
    rw_vec(&w,1,2047,5001,2047);rw_vec(&w,2,-2047,-1000,-2047);rw_vec(&w,3,4094,6001,4094);

    rw_word(&w,7,1); /* Start chute arms the lap clock; it is not a completed lap. */
    rw_float(&w,4,lengths[1]);rw_float(&w,5,lengths[0]);
    rw_marker(&w,9,lap,g+13,lap[3],0,n-1); /* finish */
    rw_marker(&w,8,lead,g+3,lead[3],1,0); /* start */
    for(uint32_t s=0;s<2;s++) {
        const float* p=s?lead:lap;uint32_t count=s?m:n;
        rw_tag(&w,11,0);rw_float(&w,13,lengths[s]);rw_word(&w,17,s?4:0);
        rw_grid(&w,0x13,195); /* enabled class/lane choices, as native authored sections */
        rw_word(&w,12,1);rw_tag(&w,14,8);rw_u(&w,0);rw_u(&w,0);
        rw_word(&w,15,s?0:2);
        if(!s)for(uint32_t j=0;j<2;j++){rw_tag(&w,16,8);rw_u(&w,j);rw_u(&w,j);}
        float remaining=lengths[s];
        for(uint32_t i=0;i<count;i++) {
            const float* q=p+5*i; const float* next=i+1<count?q+5:q;
            float dx=next[0]-q[0],dz=next[2]-q[2],len=hypotf(dx,dz);
            if(i+1==count){dx=lap[5]-lap[0];dz=lap[7]-lap[2];}
            float norm=hypotf(dx,dz);if(norm<.001f){free(w.data);return 0;}dx/=norm;dz/=norm;
            const float* prev=i?q-5:(s?q:lap+5*(n-2));
            if(i+1==count)prev=lap+5*(n-2);
            float px=q[0]-prev[0],pz=q[2]-prev[2],pn=hypotf(px,pz);
            float ex=dx+(pn>.001f?px/pn:dx),ez=dz+(pn>.001f?pz/pn:dz),en=hypotf(ex,ez);
            if(en<.001f){ex=dx;ez=dz;}else{ex/=en;ez/=en;}
            rw_tag(&w,20,0);rw_vec(&w,21,q[0],q[1],q[2]);
            /* Reflex's left/right edge winding: left = (-forward.z, forward.x). */
            rw_vec(&w,22,q[0]-ez*q[3]*.5f,q[1],q[2]+ex*q[3]*.5f);
            rw_vec(&w,23,q[0]+ez*q[3]*.5f,q[1],q[2]-ex*q[3]*.5f);
            rw_vec(&w,26,dx,0,dz);rw_float(&w,24,len);rw_float(&w,25,fmaxf(0,remaining));
            rw_grid(&w,0x1c,264);float speed=fminf(90,q[4]);uint32_t bits;memcpy(&bits,&speed,4);rw_grid(&w,0x1d,bits);
            remaining-=len;
        }
    }
    memcpy(gate,g,40);*out=w.data;*bytes=w.pos;return 1;
}

int fmt_modtrack_lap_gate(uint32_t driver,int laps,float fraction) {
    if(laps<0) {
        for(int i=0;i<32;i++)if(lap_gates[i].driver==driver)lap_gates[i].driver=0;
        return 1; /* the first finish crossing only arms the clock */
    }
    for(int i=0;i<32;i++)if(lap_gates[i].driver==driver || !lap_gates[i].driver) {
        if(lap_gates[i].driver!=driver || lap_gates[i].lap!=laps){lap_gates[i].driver=driver;lap_gates[i].lap=laps;lap_gates[i].sector=0;}
        int sector=lap_gates[i].sector;
        if(sector<3 && fraction>=(sector+1)*.25f && fraction<(sector+1)*.25f+.15f)lap_gates[i].sector++;
        return lap_gates[i].sector==3;
    }
    return 0;
}

int fmt_modtrack_recovery_retry(uint32_t driver,uint32_t node,float time) {
    for(int i=0;i<32;i++)if(recoveries[i].driver==driver || !recoveries[i].driver) {
        /* Adjacent nodes can alternate while a bike is trapped on a jump lip.
         * A sustained interval without another crash, rather than a node change,
         * ends this recovery sequence. */
        if(recoveries[i].driver!=driver || time<recoveries[i].time || time-recoveries[i].time>8.f)recoveries[i].attempts=0;
        recoveries[i].driver=driver;recoveries[i].node=node;recoveries[i].time=time;
        return ++recoveries[i].attempts>=3;
    }
    return 0;
}

float fmt_modtrack_ride_height(float x,float z,float source_y,float terrain_y,float scale) {
    float best=terrain_y,distance=fabsf(terrain_y-source_y);
    for(uint32_t chunk=0;chunk<collision_count;chunk++) {
        uint32_t count;const float* v=fmt_modtrack_collision_chunk(chunk,&count);
        const float* anchors=fmt_modtrack_collision_anchors(chunk);
        for(uint32_t i=0;i<count;i+=3) {
            float p[3][3];
            for(int j=0;j<3;j++)fmt_modtrack_scale_point(v+3*(i+j),anchors?anchors+4*(i+j):NULL,scale,p[j]);
            if(x<fminf(p[0][0],fminf(p[1][0],p[2][0]))||x>fmaxf(p[0][0],fmaxf(p[1][0],p[2][0]))||z<fminf(p[0][2],fminf(p[1][2],p[2][2]))||z>fmaxf(p[0][2],fmaxf(p[1][2],p[2][2])))continue;
            float ax=p[1][0]-p[0][0],az=p[1][2]-p[0][2],bx=p[2][0]-p[0][0],bz=p[2][2]-p[0][2],den=ax*bz-az*bx;
            if(fabsf(den)<.0001f)continue;
            float u=((x-p[0][0])*bz-(z-p[0][2])*bx)/den,w=(ax*(z-p[0][2])-az*(x-p[0][0]))/den;
            if(u<0||w<0||u+w>1)continue;
            float y=p[0][1]+u*(p[1][1]-p[0][1])+w*(p[2][1]-p[0][1]),d=fabsf(y-source_y);
            if(d<distance){best=y;distance=d;}
        }
    }
    return best;
}
