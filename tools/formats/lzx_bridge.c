/* Read-only in-memory LZX bridge. Links the separate LGPL-2.1 libmspack sources.
 * Source/provenance and full license are retained under out/libmspack/. */
#include <stdlib.h>
#include <string.h>
#include "mspack.h"
#include "lzx.h"
struct memory_file { unsigned char *data; size_t size, position; };
static int mem_read(struct mspack_file *handle, void *buffer, int length) {
    struct memory_file *file = (struct memory_file *)handle;
    if (length < 0) return -1;
    size_t amount = (size_t)length;
    if (amount > file->size - file->position) amount = file->size - file->position;
    memcpy(buffer, file->data + file->position, amount);
    file->position += amount;
    return (int)amount;
}
static int mem_write(struct mspack_file *handle, void *buffer, int length) {
    struct memory_file *file = (struct memory_file *)handle;
    if (length < 0 || (size_t)length > file->size - file->position) return -1;
    memcpy(file->data + file->position, buffer, (size_t)length);
    file->position += (size_t)length;
    return length;
}
static void *mem_alloc(struct mspack_system *system, size_t amount) { return malloc(amount); }
static void mem_copy(void *source, void *destination, size_t amount) { memcpy(destination, source, amount); }
int reflex_lzx_decode(const unsigned char *source, size_t source_size,
                     unsigned char *destination, size_t destination_size, int window_bits) {
    struct memory_file input = {(unsigned char *)source, source_size, 0};
    struct memory_file output = {destination, destination_size, 0};
    struct mspack_system system = {0};
    system.read = mem_read; system.write = mem_write; system.alloc = mem_alloc;
    system.free = free; system.copy = mem_copy;
    struct lzxd_stream *stream = lzxd_init(&system, (struct mspack_file *)&input,
        (struct mspack_file *)&output, window_bits, 0, 4096, destination_size, 0);
    if (!stream) return -1;
    int result = lzxd_decompress(stream, destination_size);
    lzxd_free(stream);
    if (!result && output.position != destination_size) return -2;
    return result;
}
