// Ambient animated entities of the original world (ambient.cpp, _re/names/76_ambient.tsv): crew chiefs, pit crews and score-tower
// people, flaggers, looping / spline / scenario animations (walking crowds, the Alpine hovercraft), roaming people, the instanced
// grandstand crowd, billboard crowds. Read only: nothing in the guest is written.
#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace reflex_vm {
class Machine;

// Same layout as ReflexAmbientEntity (reflex_world.h).
struct AmbientEntity {
    uint32_t object, entity_id;
    int32_t kind;
    uint32_t flags;
    int32_t bones, first_matrix, source;
    float lod_distance;
    float param[4];
    float matrix[16];
    char model[64];
    char texture[64], texture_package[64];   // crew chief: the shirt texture replacing CrewChief_ShirtColor_Default, its package
};
struct AmbientInfo {
    int32_t entities, matrices, objects, reserved0;
    float fade_start, lod_divisor, reserved[2];   // LOD fade begins at fade_start * lod_distance^2 (0x9c4b4c); renderer distance divisor
};

class AmbientScene {
public:
    // Every ambient entity after the last frame; node world matrices appended to `matrices` (16 floats each, row vectors).
    void collect(Machine& m, std::vector<AmbientEntity>& out, std::vector<float>& matrices, AmbientInfo& info);
    void rescan() { scanned_ = false; }
private:
    struct Object { uint32_t address; int32_t kind; };
    std::vector<Object> objects_;
    std::unordered_map<uint32_t, uint32_t> texture_handles_;   // TextureAsset data -> its TextureAssetHandle
    bool scanned_ = false;
    void scan(Machine& m);
};

// Class table: index = ReflexAmbientEntity.kind; the first 10 are the classes World::set_ambient_animation skips (bit i of
// World::set_ambient_mask), the others always run.
int ambient_kind_count();
uint32_t ambient_kind_vtable(int kind);
const char* ambient_kind_name(int kind);
}  // namespace reflex_vm
