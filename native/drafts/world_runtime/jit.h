// Block JIT for the interpreter (private draft): hot straight-line x86 code is translated to
// AArch64 host code with exactly the interpreter's semantics (vm.cpp / fast.inc are the
// reference; the translated forms are the most frequent ones, everything else is executed by
// calling the interpreter's own handler for that instruction from the translated block).
#pragma once
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <vector>

namespace reflex_vm {
class Machine;
struct Insn;

void jit_print_forms(int top);   // debugging: the most frequent operand forms run by the generic helper

class Jit {
public:
    using BlockFn = uint64_t (*)(Machine*);   // returns the next guest pc, or (1 << 32) | pc after a fault (m.jit_exception)
    explicit Jit(Machine& m);
    ~Jit();
    bool ok() const { return code_ != nullptr; }
    // The translated block at pc (translating it once it is hot), or nullptr to interpret.
    BlockFn lookup(uint32_t pc);
    void flush();
    uint64_t blocks() const { return nblocks_; }
private:
    Machine& m_;
    uint8_t* code_ = nullptr; size_t cap_ = 0, used_ = 0;
    std::unordered_map<uint32_t, BlockFn> map_;
    std::unordered_map<uint32_t, uint64_t> body_;                   // guest pc -> host address of the block body (chained entry)
    std::unordered_map<uint32_t, std::vector<uint64_t*>> waiting_;  // guest pc -> chain slots to fill when it is translated
    std::vector<std::unique_ptr<uint64_t[]>> slots_; size_t slot_used_ = 0;
    struct CacheEntry { uint32_t pc, pad; BlockFn fn; uint64_t body, pad2; };   // 32 bytes: generated code indexes it
    std::unique_ptr<CacheEntry[]> cache_;
    uint64_t* new_slot();
    void init_cache();
    std::unordered_map<uint32_t, uint32_t> heat_;
    std::unordered_map<uint32_t, uint8_t> refused_;
    uint64_t nblocks_ = 0;
    bool flushed_ = false;
    BlockFn translate(uint32_t pc);
};
}  // namespace reflex_vm
