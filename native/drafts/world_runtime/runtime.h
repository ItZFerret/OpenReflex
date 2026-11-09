#pragma once
#include <cstdint>
#include <set>
#include <string>

namespace reflex_vm {
class Machine;

struct TerrainInfo {
    uint32_t samples, stride; float spacing, min_x, min_z, max_x, max_z, gain;
    uint32_t deform_version; int32_t dirty_x0, dirty_z0, dirty_x1, dirty_z1;
};
struct WorldPad { uint16_t buttons; uint8_t left_trigger, right_trigger; int16_t lx, ly, rx, ry; };

// One original Reflex world: the original executable and VC90 CRT executing over a
// world image. frame() runs XenonInputManager::vf43(dt) then 00769b80(dt).
class World {
public:
    World();
    ~World();
    void load(const std::string& exe, const std::string& crt, const std::string& world_image, const std::string& deform_grid);
    void frame(uint32_t dt_bits, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask);
    void engine_frame(uint64_t ticks, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask);
    void engine_frame_at(uint64_t counter, const uint8_t keys[256], const WorldPad* pads, uint32_t pad_mask);
    uint32_t header_dt() const;
    uint32_t sim() const;
    uint32_t rider() const;
    bool terrain_info(TerrainInfo& t);
    bool terrain_heights(float* out, int x0, int z0, int w, int h);
    // Deformation surface values (R32F, 0.5 = undeformed) for the same grid rect as terrain_heights.
    // Engine/vehicle sound events (opt-in): the game's FMOD event commands (type 0x13) for an
    // audio client, and the client's "one-shot finished" reports fed back to the game.
    struct AudioCommand { int32_t sub; uint32_t wrapper, serial; float value; float position[3]; int32_t has_position, state; char path[96]; char param[32]; };
    void set_audio(bool enabled);
    size_t drain_audio(AudioCommand* out, size_t max);
    void audio_finished(uint32_t wrapper, uint32_t serial);
    // Tyre-track decal triangles (6 floats per vertex, see runtime.cpp) and a change counter.
    size_t tracks(std::vector<float>& out);
    uint32_t tracks_version() const;
    bool terrain_deform(float* out, int x0, int z0, int w, int h);
    void clear_terrain_dirty();
    void set_ambient_animation(bool simulate);
    bool ambient_animation() const;
    // Classes (bit i = the i-th skipped class, reflex_world.h REFLEX_AMBIENT_*) that keep running with ambient animation off.
    void set_ambient_mask(uint32_t keep);
    uint32_t ambient_mask() const;
    // Particles (particles.cpp): set_particles(true) simulates the particle managers (00698510) even while the other
    // ambient skips stay; particle_export() returns the live particles as the original renderer draws them (normal
    // manager far to near, then additive), see reflex_world.h ReflexParticle / ReflexParticleInfo.
    struct Particle { float position[3], size[2], pivot[2], scale, rotation; uint32_t color, color1; float uv[4]; uint32_t flags; };
    struct ParticleInfo { char texture[64]; float luminance[4], luminance_delta[4], fade[4]; int32_t soft, normal, additive, live; float near_plane, far_plane, reserved[2]; };
    void set_particles(bool simulate);
    bool particles_simulated() const;
    void particle_export(std::vector<Particle>& out, ParticleInfo& info);
    bool whole_frames() const;     // frame-boundary world image: engine_frame() runs whole original frames        // RiderAnimation object recorded in a paged world image (0 if none)          // bike simulation recorded in a paged world image (0 if none)
    Machine& machine();
    uint32_t resource() const;
    const std::set<uint32_t>& keyboards() const;
private:
    struct Impl;
    Impl* d;
    void install_stand_ins(const std::string& deform_path);
};
}  // namespace reflex_vm
