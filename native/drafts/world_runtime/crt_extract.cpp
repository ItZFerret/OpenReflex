// The VC90 SP1 runtime (msvcr90.dll / msvcp90.dll 9.00.30729.1) from the install's own redistributable:
// Installers/vcredist_x86.exe is an IExpress package whose CAB (LZX:21) holds vc_red.cab (LZX:21) with the
// "msvc?90.dll.30729.01.Microsoft_VC90_CRT_x86.SP" members. LZX: libmspack's lzxd (LGPL-2.1, lib/libmspack).
#include "reflex_world.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>
extern "C" {
#include "lib/libmspack/mspack.h"
#include "lib/libmspack/lzx.h"
}

namespace {
struct MemFile { const unsigned char* data; size_t size, pos; unsigned char* out; size_t out_size, out_pos; };
int mem_read(mspack_file* f, void* buf, int n) { auto* m = reinterpret_cast<MemFile*>(f); size_t k = std::min<size_t>(size_t(n), m->size - m->pos); std::memcpy(buf, m->data + m->pos, k); m->pos += k; return int(k); }
int mem_write(mspack_file* f, void* buf, int n) { auto* m = reinterpret_cast<MemFile*>(f); if (m->out_pos + size_t(n) > m->out_size) return -1; std::memcpy(m->out + m->out_pos, buf, size_t(n)); m->out_pos += size_t(n); return n; }
void* mem_alloc(mspack_system*, size_t n) { return std::malloc(n); }
void mem_free(void* p) { std::free(p); }
void mem_copy(void* s, void* d, size_t n) { std::memcpy(d, s, n); }

template <class T> T rd(const std::vector<unsigned char>& b, size_t o) { if (o + sizeof(T) > b.size()) throw std::runtime_error("CAB out of range"); T v; std::memcpy(&v, b.data() + o, sizeof v); return v; }

// name -> bytes; throws std::runtime_error
std::map<std::string, std::vector<unsigned char>> cab(const std::vector<unsigned char>& b, size_t o) {
    if (o + 36 > b.size() || std::memcmp(b.data() + o, "MSCF", 4)) throw std::runtime_error("CAB signature");
    uint32_t files_off = rd<uint32_t>(b, o + 16); uint16_t folders = rd<uint16_t>(b, o + 26), files = rd<uint16_t>(b, o + 28), flags = rd<uint16_t>(b, o + 30);
    size_t p = o + 36; unsigned folder_res = 0, data_res = 0;
    if (flags & 4) { uint16_t h = rd<uint16_t>(b, p); folder_res = b[p + 2]; data_res = b[p + 3]; p += 4 + h; }
    std::vector<std::vector<unsigned char>> data;
    for (unsigned f = 0; f < folders; f++, p += 8 + folder_res) {
        uint32_t first = rd<uint32_t>(b, p); uint16_t blocks = rd<uint16_t>(b, p + 4), type = rd<uint16_t>(b, p + 6);
        if ((type & 0xf) != 3) throw std::runtime_error("CAB compression is not LZX");
        std::vector<unsigned char> in; size_t total = 0;
        if (blocks == 0 || blocks > 20000) throw std::runtime_error("CAB folder blocks");
        for (size_t k = 0, d = o + first; k < blocks; k++) {
            uint16_t packed = rd<uint16_t>(b, d + 4), size = rd<uint16_t>(b, d + 6); d += 8 + data_res;
            if (d + packed > b.size()) throw std::runtime_error("CAB truncated");
            in.insert(in.end(), b.begin() + long(d), b.begin() + long(d + packed)); d += packed; total += size;
        }
        if (total > (256u << 20)) throw std::runtime_error("CAB folder too large");
        std::vector<unsigned char> out(total);
        MemFile mf{in.data(), in.size(), 0, out.data(), out.size(), 0};
        mspack_system sys{}; sys.read = mem_read; sys.write = mem_write; sys.alloc = mem_alloc; sys.free = mem_free; sys.copy = mem_copy;
        lzxd_stream* s = lzxd_init(&sys, reinterpret_cast<mspack_file*>(&mf), reinterpret_cast<mspack_file*>(&mf), (type >> 8) & 0x1f, 0, 4096, off_t(total), 0);
        if (!s) throw std::runtime_error("lzxd_init");
        int r = lzxd_decompress(s, off_t(total)); lzxd_free(s);
        if (r != 0 || mf.out_pos != total) throw std::runtime_error("CAB LZX decoding");
        data.push_back(std::move(out));
    }
    std::map<std::string, std::vector<unsigned char>> result;
    for (size_t i = 0, q = o + files_off; i < files; i++) {
        uint32_t size = rd<uint32_t>(b, q), off = rd<uint32_t>(b, q + 4); uint16_t folder = rd<uint16_t>(b, q + 8); q += 16;
        if (q >= b.size()) throw std::runtime_error("CAB out of range");
        size_t e = q; while (e < b.size() && b[e]) e++;
        if (e >= b.size()) throw std::runtime_error("CAB out of range");
        std::string name(reinterpret_cast<const char*>(b.data() + q), e - q); q = e + 1;
        while (!name.empty() && (name[0] == '.' || name[0] == '\\')) name.erase(0, 1);
        if (folder < data.size() && size_t(off) + size <= data[folder].size()) result[name].assign(data[folder].begin() + off, data[folder].begin() + off + size);
    }
    return result;
}
std::string crt_error;
}  // namespace

extern "C" int reflex_world_extract_crt(const char* installer, const char* out_dir) {
    try {
        std::ifstream f(installer, std::ios::binary);
        if (!f) throw std::runtime_error(std::string("cannot open ") + installer);
        std::vector<unsigned char> exe((std::istreambuf_iterator<char>(f)), {});
        std::map<std::string, std::vector<unsigned char>> outer;
        for (size_t at = 0; at + 4 <= exe.size() && outer.empty(); at++) {
            if (std::memcmp(exe.data() + at, "MSCF", 4)) continue;
            try { auto o = cab(exe, at); if (o.count("vc_red.cab")) outer = std::move(o); } catch (const std::exception&) {}
        }
        if (outer.empty()) throw std::runtime_error("no vc_red.cab in the redistributable");
        auto inner = cab(outer["vc_red.cab"], 0);
        for (const char* n : {"msvcr90.dll", "msvcp90.dll"}) {
            auto it = inner.find(std::string(n) + ".30729.01.Microsoft_VC90_CRT_x86.SP");
            if (it == inner.end()) throw std::runtime_error(std::string(n) + " 9.0.30729.1 not in vc_red.cab");
            std::string path = std::string(out_dir) + "/" + n;
            std::ofstream o(path, std::ios::binary); o.write(reinterpret_cast<const char*>(it->second.data()), std::streamsize(it->second.size()));
            if (!o) throw std::runtime_error("cannot write " + path);
        }
        crt_error.clear(); return 0;
    } catch (const std::exception& e) { crt_error = e.what(); return 1; }
}
extern "C" const char* reflex_world_extract_crt_error(void) { return crt_error.c_str(); }
