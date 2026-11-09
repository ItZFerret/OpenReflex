// Physics props of the original world (props.cpp, _re/names/87_props.tsv, _re/notes/props.md): the PhysicsEntity family (bales,
// tyre stacks, cones, barrels, fences ... knocked by the riders), the pop-up / pop-down bales and the step-up bars; animated
// textures (ScrollingTextureEntity, TextureAnimEntity). Read only: nothing in the guest is written.
#pragma once
#include <cstdint>
#include <vector>

namespace reflex_vm {
class Machine;

// Same layout as ReflexProp (reflex_world.h).
struct PropEntity {
    uint32_t object, entity_id;
    int32_t kind;
    uint32_t flags;
    float matrix[16];
};
struct PropInfo {
    int32_t props, reported, objects, pending;   // live props, entries written, objects found by the scan, changes not written (max too small)
};
// Same layout as ReflexTextureAnim (reflex_world.h).
struct TextureAnimState {
    uint32_t object, entity_id;
    int32_t kind;            // 0 ScrollingTextureEntity, 1 TextureAnimEntity
    uint32_t flags;          // 1 visible (ModelInstance +0x02)
    float offset[2];         // the UV offset the original binds this frame (gTextureOffset)
    float param[4];          // scrolling: U / V periods (seconds for wrap, negative = reversed), U / V accumulators; anim: frame drawn,
                             // frame count, frames per second, mode (1 cycles, 2 ping-pong)
    char target[64];         // TextureAnimEntity: the TextureName it animates (empty for scrolling textures)
};

class PropScene {
public:
    // Props whose world matrix or visibility changed since the last call (all of them with all != 0), at most max.
    void collect(Machine& m, PropEntity* out, uint32_t max, bool all, PropInfo& info);
    void textures(Machine& m, std::vector<TextureAnimState>& out);
    void rescan() { scanned_ = false; }
private:
    struct Object { uint32_t address; int32_t kind; uint32_t flags; float matrix[16], first[16]; bool reported, first_set; };
    std::vector<Object> objects_;
    std::vector<Object> textures_;
    bool scanned_ = false;
    void scan(Machine& m);
};

int prop_kind_count();
const char* prop_kind_name(int kind);
}  // namespace reflex_vm
