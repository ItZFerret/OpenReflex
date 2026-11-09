// Render capture of the original renderer (boot mode only; nothing here runs in the world runtime or the plugin).
// Hooks the boot's Direct3D 9 recording device (Boot::set_d3d_hook), keeps the device state the game sets (render, sampler
// and texture-stage states, shaders, constants, streams, targets), logs the calls of chosen frames (calls.txt, for
// _re/notes/render-pipeline.md) and hands every draw to the software rasterizer (swr.h), which writes its results into the
// recording device's memory-backed render targets, so later passes sample them as the GPU's would.
#pragma once
#include "boot.h"
#include <cstdint>
#include <memory>
#include <string>

namespace reflex_vm {
struct RenderCaptureImpl;
class RenderCapture {
public:
    // dir: output folder (calls.txt, shaders/, rt/ PNG dumps). Install before Boot::start (the state shadow starts at
    // device creation).
    RenderCapture(Boot& boot, const std::string& dir);
    ~RenderCapture();
    // Rasterize: 0 off (state only), 1 render-to-texture draws only (load-time bakes, shadow maps), 2 everything.
    void set_raster(int mode);
    void after_start();   // after Boot::start: wraps D3DXCompileShader (keeps the sources)
    // Log and dump the next `frames` presented frames (Present to Present).
    void capture_frames(int frames);
    bool capturing() const;
    std::string summary() const;
private:
    std::unique_ptr<RenderCaptureImpl> d;
};
}  // namespace reflex_vm
