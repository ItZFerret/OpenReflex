// PE32 image loading shared by the world runtime and the boot process (private draft).
#pragma once
#include "vm.h"
#include <cctype>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace reflex_vm {

inline std::vector<uint8_t> slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw VmError("cannot open " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}
template <class T> inline T rd(const std::vector<uint8_t>& b, size_t off) { T v; std::memcpy(&v, b.data() + off, sizeof v); return v; }

struct Pe {
    std::vector<uint8_t> file, image;
    uint32_t image_base = 0, size = 0, nt = 0;
    struct Section { uint32_t va, vsize, raw, rawsize, flags; };
    std::vector<Section> sections;
    explicit Pe(std::vector<uint8_t> bytes) : file(std::move(bytes)) {
        nt = rd<uint32_t>(file, 0x3c);
        if (rd<uint32_t>(file, nt) != 0x4550) throw VmError("not a PE file");
        uint16_t nsec = rd<uint16_t>(file, nt + 6), optsize = rd<uint16_t>(file, nt + 20);
        image_base = rd<uint32_t>(file, nt + 24 + 28); size = rd<uint32_t>(file, nt + 24 + 56);
        uint32_t headers = rd<uint32_t>(file, nt + 24 + 60);
        image.assign(size, 0);
        std::memcpy(image.data(), file.data(), std::min<size_t>(headers, file.size()));
        uint32_t sec = nt + 24 + optsize;
        for (int i = 0; i < nsec; i++, sec += 40) {
            Section s{rd<uint32_t>(file, sec + 12), rd<uint32_t>(file, sec + 8), rd<uint32_t>(file, sec + 20), rd<uint32_t>(file, sec + 16), rd<uint32_t>(file, sec + 36)};
            sections.push_back(s);
            if (s.rawsize) std::memcpy(image.data() + s.va, file.data() + s.raw, std::min(s.rawsize, s.vsize ? std::max(s.vsize, s.rawsize) : s.rawsize));
        }
    }
    uint32_t dir(int i, uint32_t* sz = nullptr) const {
        if (sz) *sz = rd<uint32_t>(image, nt + 24 + 96 + 8 * i + 4);
        return rd<uint32_t>(image, nt + 24 + 96 + 8 * i);
    }
    void relocate(uint32_t new_base) {
        uint32_t sz, rva = dir(5, &sz); int64_t delta = int64_t(new_base) - int64_t(image_base);
        for (uint32_t used = 0; used < sz;) {
            uint32_t page = rd<uint32_t>(image, rva + used), block = rd<uint32_t>(image, rva + used + 4);
            for (uint32_t e = 8; e < block; e += 2) {
                uint16_t entry = rd<uint16_t>(image, rva + used + e);
                if ((entry >> 12) == 3) { uint32_t at = page + (entry & 0xfff); uint32_t v = rd<uint32_t>(image, at) + uint32_t(delta); std::memcpy(image.data() + at, &v, 4); }
                else if ((entry >> 12) != 0) throw VmError("relocation type");
            }
            used += block;
        }
        image_base = new_base;
    }
    std::map<std::string, uint32_t> exports() const {
        std::map<std::string, uint32_t> out; uint32_t rva = dir(0);
        if (!rva) return out;
        uint32_t n = rd<uint32_t>(image, rva + 24), funcs = rd<uint32_t>(image, rva + 28), names = rd<uint32_t>(image, rva + 32), ords = rd<uint32_t>(image, rva + 36);
        for (uint32_t i = 0; i < n; i++) {
            std::string name(reinterpret_cast<const char*>(image.data() + rd<uint32_t>(image, names + 4 * i)));
            uint16_t ord = rd<uint16_t>(image, ords + 2 * i);
            out[name] = image_base + rd<uint32_t>(image, funcs + 4 * ord);
        }
        return out;
    }
    struct Import { std::string dll, name; uint32_t iat; };
    std::vector<Import> imports() const {
        std::vector<Import> out; uint32_t rva = dir(1);
        for (uint32_t d = rva; rd<uint32_t>(image, d + 12); d += 20) {
            std::string dll(reinterpret_cast<const char*>(image.data() + rd<uint32_t>(image, d + 12)));
            for (auto& c : dll) c = char(std::tolower(c));
            uint32_t names = rd<uint32_t>(image, d) ? rd<uint32_t>(image, d) : rd<uint32_t>(image, d + 16), iat = rd<uint32_t>(image, d + 16);
            for (uint32_t k = 0;; k++) {
                uint32_t thunk = rd<uint32_t>(image, names + 4 * k);
                if (!thunk) break;
                std::string name = (thunk & 0x80000000u) ? "#" + std::to_string(thunk & 0xffff) : std::string(reinterpret_cast<const char*>(image.data() + thunk + 2));
                out.push_back({dll, name, image_base + iat + 4 * k});
            }
        }
        return out;
    }
};

}  // namespace reflex_vm
