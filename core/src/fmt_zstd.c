/* fmt_zstd.c -- the Zstandard library (vendored: src/zstd, zstd 1.5.7, BSD licence in src/zstd/LICENSE) as one translation unit.
 *
 * The same arrangement as zstd's own single-file build (build/single_file_libs/zstd-in.c of the release), written out by hand so
 * every build script that compiles the src C files (core/build.sh, tools/build_windows.sh, the oracle CLI, the test drivers) gets zstd
 * without a list of its files: compressor and decompressor, no dictionary builder, no legacy formats, no worker threads, no
 * assembly (the x86-64 Huffman decoder's .S file is left out; the C decoder is used on every platform). Symbols stay hidden
 * (-fvisibility=hidden); only the archive code (fmt_rxar.c) calls it.
 *
 * Updating zstd: replace src/zstd/{common,compress,decompress}, zstd.h, zstd_errors.h and LICENSE with the new release's lib/
 * files (delete the decompress .S files), then check this list against the release's zstd-in.c. */
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wunused-function"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wunused-function"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

/* nothing of zstd is exported from librxcore / rxcore.dll (zstd.h would mark its API visibility("default")) */
#define ZSTDLIB_VISIBLE
#define ZSTDERRORLIB_VISIBLE
#define DEBUGLEVEL 0
#define MEM_MODULE
#undef  XXH_NAMESPACE
#define XXH_NAMESPACE ZSTD_
#undef  XXH_PRIVATE_API
#define XXH_PRIVATE_API
#undef  XXH_INLINE_ALL
#define XXH_INLINE_ALL
#define ZSTD_LEGACY_SUPPORT 0
#define ZSTD_TRACE 0
#define ZSTD_DISABLE_ASM 1

#define ZSTD_DEPS_NEED_MALLOC
#define ZSTD_DEPS_NEED_MATH64
#include "zstd/common/zstd_deps.h"

#include "zstd/common/debug.c"
#include "zstd/common/entropy_common.c"
#include "zstd/common/error_private.c"
#include "zstd/common/fse_decompress.c"
#include "zstd/common/threading.c"
#include "zstd/common/pool.c"
#include "zstd/common/zstd_common.c"

#include "zstd/compress/fse_compress.c"
#include "zstd/compress/hist.c"
#include "zstd/compress/huf_compress.c"
#include "zstd/compress/zstd_compress_literals.c"
#include "zstd/compress/zstd_compress_sequences.c"
#include "zstd/compress/zstd_compress_superblock.c"
#include "zstd/compress/zstd_preSplit.c"
#include "zstd/compress/zstd_compress.c"
#include "zstd/compress/zstd_double_fast.c"
#include "zstd/compress/zstd_fast.c"
#include "zstd/compress/zstd_lazy.c"
#include "zstd/compress/zstd_ldm.c"
#include "zstd/compress/zstd_opt.c"

#include "zstd/decompress/huf_decompress.c"
#include "zstd/decompress/zstd_ddict.c"
#include "zstd/decompress/zstd_decompress.c"
#include "zstd/decompress/zstd_decompress_block.c"
