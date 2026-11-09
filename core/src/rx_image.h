/* rx_image.h -- exact immutable startup data restored at original arena addresses.
 * Product startup uses core/data/initial_data.inc: EXE .rdata/.data/.tls, CRT .data,
 * four named CRT data objects, and pre-resolved import slot values. It reads no EXE,
 * redistributable or DLL. No EXE .text or DLL instructions are embedded or executed.
 * Provenance, byte hashes and regeneration: core/data/README.md.
 * RX_ORACLE already has the original bytes and import bindings in its machine.
 */
#ifndef RX_IMAGE_H
#define RX_IMAGE_H
#include <stdint.h>
#include "fmt_vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The executable's name in the install root, and the layout of its sections (image base 0x00400000). */
#define RX_IMAGE_EXE        "MXReflex.exe"
#define RX_IMAGE_BASE       0x00400000u
#define RX_RDATA_START      0x00916000u      /* .rdata: 0x00916000 .. 0x009a5000 */
#define RX_DATA_START       0x009a6000u      /* .data (initialized part up to 0x009df600, then zero-initialized) .. 0x00dcf76c */
#define RX_TLS_START        0x00dd0000u      /* .tls: the TLS template (0x811 bytes) */

/* Restore embedded EXE initial data (rx_mem_init first). v is retained for source compatibility
 * and is ignored. Raw section bytes match the supported executable exactly; virtual tails
 * remain zero in the fresh arena. Returns 0, or -1 with fmt_error(). */
int rx_image_load(const fmt_vfs* v);

/* The same from a PE image already in memory (tools / tests). */
int rx_image_load_bytes(const uint8_t* pe, size_t size);

/* Section table of the image last loaded: [i] name / address / raw size / virtual size; returns the count (0 before a load). */
typedef struct rx_image_section { char name[9]; uint32_t addr, raw_size, virtual_size; int loaded; } rx_image_section;
int rx_image_sections(rx_image_section* out, int max);
void rx_image_reset(void);   /* frees the recorded import slots (host memory); the next rx_image_load starts from nothing */

/* Restore embedded CRT initialized globals / named read-only data and bind the EXE and
 * msvcp90 CRT import slots to the original numeric function identifiers. v is ignored.
 * Load initial EXE data first. No DLL code is loaded. The byte loaders and extractor below
 * are development/reference utilities; normal product startup does not call them. */
#define RX_MSVCR90_BASE     0x78520000u
#define RX_MSVCP90_BASE     0x78480000u
int rx_image_load_crt(const fmt_vfs* v);
/* The two DLL files themselves (malloc'ed; tools / tests). 0 or -1. */
int rx_image_extract_crt(const fmt_vfs* v, fmt_buf* msvcr90, fmt_buf* msvcp90);
/* The same from the two DLL files already extracted (tests: the world runtime's cache folder). */
int rx_image_load_crt_bytes(const uint8_t* msvcr90, size_t n1, const uint8_t* msvcp90, size_t n2);

#ifdef __cplusplus
}
#endif
#endif
