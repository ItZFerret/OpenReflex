/* Small independent arena equivalence check; no game build, simulation or VFS.
 * clang -std=c11 -O1 -Icore/src core/tools/initial_data_test.c core/src/rx_image.c
 *       core/src/fmt_common.c -lz -o /tmp/rx_initial_data_test
 * /tmp/rx_initial_data_test <MXReflex.exe> <msvcr90.dll> <msvcp90.dll>
 */
#define _DARWIN_C_SOURCE 1
#include "rx_image.h"
#include "rx_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
uint8_t* rx_base;
/* The product path must not touch a VFS or decode any redistributable. */
int fmt_vfs_read_file(const fmt_vfs* v,const char* name,fmt_buf* b) { (void)v;(void)b;fprintf(stderr,"Unexpected VFS read: %s\n",name);abort(); }
int fmt_lzx_decode_once(int bits,const uint8_t* in,size_t n,uint8_t* out,size_t size) { (void)bits;(void)in;(void)n;(void)out;(void)size;abort(); }
typedef struct rx_embedded_span { uint32_t image, addr, size; size_t packed_size; const unsigned char* packed; } rx_embedded_span;
typedef struct rx_embedded_binding { uint32_t slot,value; } rx_embedded_binding;
#include "../data/initial_data.inc"
static fmt_buf read_file(const char* name) {
    fmt_buf b={0}; FILE* f=fopen(name,"rb");if(!f) return b;
    if(fseek(f,0,SEEK_END)|| (long)(b.size=(size_t)ftell(f))<0 || fseek(f,0,SEEK_SET)) {fclose(f);return b;}
    b.data=malloc(b.size);if(!b.data||fread(b.data,1,b.size,f)!=b.size){free(b.data);b.data=NULL;}fclose(f);return b;
}
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    rx_base=mmap(NULL,(size_t)UINT64_C(0x100000000),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(rx_base==MAP_FAILED)return 2;
    fmt_buf pe[3];for(int i=0;i<3;++i){pe[i]=read_file(argv[i+1]);if(!pe[i].data)return 2;}
    if(rx_image_load(NULL)||rx_image_load_crt(NULL)){puts(fmt_error());return 1;}
    size_t n=sizeof rx_initial_spans/sizeof *rx_initial_spans;
    uint8_t** saved=calloc(n,sizeof *saved);
    for(size_t i=0;i<n;++i){saved[i]=malloc(rx_initial_spans[i].size);memcpy(saved[i],rx_base+rx_initial_spans[i].addr,rx_initial_spans[i].size);}
    /* Reference helpers map the actual source bytes and perform PE import/export resolution. */
    if(rx_image_load_bytes(pe[0].data,pe[0].size)||rx_image_load_crt_bytes(pe[1].data,pe[1].size,pe[2].data,pe[2].size)){puts(fmt_error());return 1;}
    for(size_t i=0;i<n;++i) if(memcmp(saved[i],rx_base+rx_initial_spans[i].addr,rx_initial_spans[i].size)){fprintf(stderr,"Different initial span %08x\n",rx_initial_spans[i].addr);return 1;}
    for(size_t i=0;i<sizeof rx_initial_bindings/sizeof *rx_initial_bindings;++i) if(rx_rd32(rx_initial_bindings[i].slot)!=rx_initial_bindings[i].value){fprintf(stderr,"Different bound slot %08x\n",rx_initial_bindings[i].slot);return 1;}
    /* Clear all original code loaded only by the reference DLL mapper, then repeat product restore. */
    memset(rx_base+RX_IMAGE_BASE+0x1000,0,0x514a5a);
    memset(rx_base+0x78521000,0,0x95804);memset(rx_base+0x78481000,0,0x832bc);
    rx_image_reset();if(rx_image_load(NULL)||rx_image_load_crt(NULL))return 1;
    for(size_t i=0;i<n;++i)if(memcmp(saved[i],rx_base+rx_initial_spans[i].addr,rx_initial_spans[i].size))return 1;
    for(uint32_t i=0;i<0x514a5a;++i)if(rx_base[RX_IMAGE_BASE+0x1000+i])return 1;
    const uint32_t crt_start[2]={0x78521000,0x78481000},crt_size[2]={0x95804,0x832bc};
    for(int d=0;d<2;++d)for(uint32_t k=0;k<crt_size[d];++k) {
        uint32_t a=crt_start[d]+k; if(!rx_base[a])continue; int allowed=0;
        for(size_t i=0;i<n;++i)if(a>=rx_initial_spans[i].addr&&a-rx_initial_spans[i].addr<rx_initial_spans[i].size){allowed=1;break;}
        if(!allowed)for(size_t i=0;i<sizeof rx_initial_bindings/sizeof *rx_initial_bindings;++i)if(a>=rx_initial_bindings[i].slot&&a-rx_initial_bindings[i].slot<4){allowed=1;break;}
        if(!allowed){fprintf(stderr,"Unexpected CRT .text byte %08x\n",a);return 1;}
    }
    printf("PASS: %zu embedded arena spans and %zu bound slots match source PE loaders; repeat restore has no EXE code or CRT bytes outside named data/IAT\n",n,sizeof rx_initial_bindings/sizeof *rx_initial_bindings);
    for(size_t i=0;i<n;++i)free(saved[i]);free(saved);for(int i=0;i<3;++i)fmt_buf_free(pe+i);
    rx_image_reset();munmap(rx_base,(size_t)UINT64_C(0x100000000));return 0;
}
