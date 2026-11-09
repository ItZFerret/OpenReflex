#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void*live_allocations[1024];static unsigned live_count,unknown_frees;
static void remember(void*p){if(!p)return;for(unsigned i=0;i<1024;i++)if(!live_allocations[i]){live_allocations[i]=p;++live_count;return;}abort();}
static void*observe_malloc(size_t n){void*p=malloc(n);remember(p);return p;}
static void*observe_calloc(size_t n,size_t z){void*p=calloc(n,z);remember(p);return p;}
static void observe_free(void*p){if(p){unsigned i=0;for(;i<1024;i++)if(live_allocations[i]==p){live_allocations[i]=NULL;--live_count;break;}if(i==1024)++unknown_frees;}free(p);}
#define malloc observe_malloc
#define calloc observe_calloc
#define free observe_free
/* The actual production owner is the test target. Including it exposes its private metadata list,
 * without exporting a test API or replacing capture/reload/release logic. */
#ifndef MX_BRUSH_CANDIDATE_SOURCE
#define MX_BRUSH_CANDIDATE_SOURCE "../game/fx_deform.c"
#endif
#include MX_BRUSH_CANDIDATE_SOURCE
#undef malloc
#undef calloc
#undef free
static unsigned checks;
#define REQUIRE(x,label) do{if(!(x)){fprintf(stderr,"FAIL %s\n",label);return 1;}++checks;}while(0)
static unsigned count(void){unsigned n=0;for(brush_binding*b=bindings;b;b=b->next)++n;return n;}
static unsigned char*readall(const char*path,size_t*n){FILE*f=fopen(path,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);long z=ftell(f);rewind(f);if(z<1){fclose(f);return NULL;}unsigned char*p=malloc((size_t)z);if(!p||fread(p,1,(size_t)z,f)!=(size_t)z){fclose(f);free(p);return NULL;}fclose(f);*n=(size_t)z;return p;}
int main(int argc,char**argv){
 REQUIRE(argc==3,"real material and valid multisampler fixture arguments");size_t n=0,mn=0;unsigned char*raw=readall(argv[1],&n),*multi=readall(argv[2],&mn);REQUIRE(raw&&multi,"read test bytes");
 fmt_material*parsed=fmt_material_parse(raw,n);REQUIRE(parsed&&parsed->num_passes==1&&parsed->ntextures==1,"actual material parses one pass/one sampler");REQUIRE(parsed->pixel_entry&&!strcmp(parsed->pixel_entry,"ps_hft_deform"),"actual shader entry");REQUIRE(!strcmp(parsed->textures[0].name,"HFT_Deform_Default"),"actual serialized binding name");fmt_material_free(parsed);
 REQUIRE(bindings==NULL,"fresh host metadata list empty");REQUIRE(find_brush(0x101010,0)==0,"unknown handle does not guess or touch guest arena");
 const rx_addr a=0x101010,b=0x202020;
 rx_fx_material_binding_capture(a,raw,n);REQUIRE(count()==1&&bindings->material==a&&!strcmp(bindings->texture,"HFT_Deform_Default"),"actual binding captured at actual API");
 rx_fx_material_binding_capture(a,raw,n);REQUIRE(count()==1,"same-handle valid reload replaces rather than duplicates");
 rx_fx_material_binding_capture(b,raw,n);REQUIRE(count()==2,"separate handles coexist");
 parsed=fmt_material_parse(raw,n-1);REQUIRE(!parsed,"truncated bytes really malformed");rx_fx_material_binding_capture(a,raw,n-1);REQUIRE(count()==1&&bindings->material==b,"malformed reload invalidates only old matching binding");REQUIRE(find_brush(a,0)==0,"invalidated handle cannot guess binding");
 unsigned char*wrong=malloc(n);REQUIRE(wrong,"fixture allocation");memcpy(wrong,raw,n);unsigned hits=0;for(size_t i=0;i+13<=n;i++)if(!memcmp(wrong+i,"ps_hft_deform",13)){wrong[i+3]='x';++hits;}REQUIRE(hits==1,"exact real shader string mutation");parsed=fmt_material_parse(wrong,n);REQUIRE(parsed&&parsed->pixel_entry&&!strcmp(parsed->pixel_entry,"ps_xft_deform"),"incompatible shader fixture remains valid BXML/material");fmt_material_free(parsed);
 rx_fx_material_binding_capture(a,raw,n);REQUIRE(count()==2,"valid reload before incompatible case");rx_fx_material_binding_capture(a,wrong,n);REQUIRE(count()==1&&bindings->material==b,"incompatible shader reload invalidates old matching binding");
 parsed=fmt_material_parse(multi,mn);REQUIRE(parsed&&parsed->num_passes==1&&parsed->ntextures==2&&!strcmp(parsed->pixel_entry,"ps_hft_deform"),"multisampler fixture valid and same supported shader");fmt_material_free(parsed);
 rx_fx_material_binding_capture(a,raw,n);rx_fx_material_binding_capture(a,multi,mn);REQUIRE(count()==1&&bindings->material==b,"multisampler reload invalidates old binding");
 rx_fx_deform_release();REQUIRE(bindings==NULL,"session release unlinks all metadata");REQUIRE(live_count==0&&unknown_frees==0,"actual candidate allocations all freed at session release");REQUIRE(g_model.mask==NULL&&g_model.mips==0,"session release resets existing model cache");REQUIRE(find_brush(b,0)==0,"released handle no longer resolves metadata");rx_fx_deform_release();REQUIRE(bindings==NULL,"release idempotent");
 for(unsigned i=0;i<128;i++){rx_fx_material_binding_capture(a,raw,n);rx_fx_material_binding_capture(a,wrong,n);rx_fx_material_binding_capture(b,raw,n);rx_fx_deform_release();REQUIRE(bindings==NULL&&live_count==0&&unknown_frees==0,"repeated sessions free actual captured allocations");}
 free(wrong);free(raw);free(multi);printf("PASS brush_metadata checks=%u cycles=128 canonicalTextureLookup=not-exercised(no-session)\n",checks);return 0;
}
