/* PE counterpart of rx_reset.c. build_windows.sh groups only core object writable
 * statics into .rxdata/.rxbss; CRT and C++ runtime data are deliberately excluded. */
#ifdef _WIN32
#include "rx_reset.h"
#include "rx_x87.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct {unsigned char* data;unsigned char* copy;size_t size;} reset_region;
static reset_region regions[2] RX_RESET_KEEP;
static int captured RX_RESET_KEEP;
int rx_reset_available(void){return 1;}
int rx_reset_capture(void){
    if(captured)return 1;
    HMODULE module=NULL;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(uintptr_t)&rx_reset_capture,&module))return 0;
    unsigned char* base=(unsigned char*)module;
    IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return 0;
    IMAGE_NT_HEADERS64* nt=(IMAGE_NT_HEADERS64*)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return 0;
    rx_x87_warm();
    reset_region r[2]={{0}};IMAGE_SECTION_HEADER* section=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;i++){
        int index=!memcmp(section[i].Name,".rxdata",8)?0:!memcmp(section[i].Name,".rxbss",7)?1:-1;
        if(index<0)continue;
        if(r[index].data)goto fail;
        r[index].size=section[i].Misc.VirtualSize;
        if(!r[index].size)continue;
        r[index].data=base+section[i].VirtualAddress;
        r[index].copy=(unsigned char*)malloc(r[index].size);
        if(!r[index].copy)goto fail;
    }
    /* MinGW may emit every zero-initialized symbol in .data$*; .rxbss
     * is optional, while core initialized data must always be present. */
    if(!r[0].data)goto fail;
    for(unsigned i=0;i<2;i++)if(r[i].size)memcpy(r[i].copy,r[i].data,r[i].size);
    memcpy(regions,r,sizeof r);captured=1;return 1;
fail:
    for(unsigned i=0;i<2;i++)free(r[i].copy);
    return 0;
}
long rx_reset_dirty(void){
    if(!captured)return -1;
    long n=0;for(unsigned i=0;i<2;i++)for(size_t j=0;j<regions[i].size;j++)
        n+=regions[i].data[j]!=regions[i].copy[j];
    return n;
}
long rx_reset_restore(void){
    if(!captured)return -1;
    long n=rx_reset_dirty();
    for(unsigned i=0;i<2;i++)if(regions[i].size)memcpy(regions[i].data,regions[i].copy,regions[i].size);
    return n;
}
#endif
