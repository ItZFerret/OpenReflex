// Bink 1 video playback with the install's own decoder (<install>/binkw32.dll, RAD Bink 1.9-era "BIKi"), run unchanged
// in the interpreter / JIT on a small Machine of its own per player (_re/notes/bink.md). The game's own player
// (XenonBinkVideo, MXReflex.exe 0x5950b0..0x595880) is reproduced call for call:
//   open        BinkSetSoundSystem(open, 0) (PlatformVideoManager ctor 005957b0), BinkOpen(path, 0x100400 | preload 0x2000)
//               (XenonBinkVideoManager::vf1 00595bca: BINKNOFRAMEBUFFERS | BINKALPHA [| BINKPRELOADALL for LoadToMemory]),
//               BinkGetFrameBuffersInfo + BinkRegisterFrameBuffers over our own plane buffers (XenonBinkVideo::vf9 005950b0).
//   per update  XenonBinkVideo::vf8 00595470: when a frame is due (00595690) BinkRegisterFrameBuffers, BinkDoFrame,
//               while BinkShouldSkip { BinkNextFrame; BinkDoFrame }, the dirty rects of BinkGetRects copied from the decoded
//               plane set into the "textures" (005948c0), BinkNextFrame; then BinkWait decides whether the next one is due.
//   picture     the four A8 planes (Y, cR, cB at half size, A) exactly as the game uploads them; the game's ps_2_0 shaders
//               (HLSL text in the exe at 0x934c88 / 0x935138) turn them into RGB(A) - Unity's Reflex/Bink Video shader.
//   sound       the BINKSND interface the DLL expects (_re/notes/bink.md: callbacks at +0x158..+0x174) implemented here; the
//               PCM the decoder hands to Unlock is queued for the caller (Unity's AudioSource).
// Clock: RADTimerRead (QueryPerformanceCounter) reads the caller's time, so BinkWait paces the frames exactly as in the game.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace reflex_bink {

struct Info {
    uint32_t width = 0, height = 0, frames = 0, fps_num = 0, fps_den = 1;
    uint32_t has_alpha = 0;                 // the file carries an alpha plane (decoded into plane A)
    uint32_t audio_rate = 0, audio_channels = 0, audio_bits = 0;   // 0 rate: no sound track
    uint32_t ywidth = 0, yheight = 0;       // Y / A plane size (YABufferWidth / Height)
    uint32_t cwidth = 0, cheight = 0;       // chroma plane size (BINKFRAMEBUFFERS cRcBBufferWidth / Height)
};

class Player {
public:
    // dll: the install's binkw32.dll; file: the .bik; preload: BINKPRELOADALL (UIVideoComponent LoadToMemory).
    static std::unique_ptr<Player> open(const std::string& dll, const std::string& file, bool preload, bool jit, std::string* error);
    ~Player();
    const Info& info() const;
    // The game's per-frame step at time `ms` (the caller's clock, milliseconds since its start): 1 when a new frame was
    // decoded, 0 otherwise. *done is set when the shown frame is the last one (XenonBinkVideo::vf8's VideoDone test).
    int update(double ms, bool* done);
    uint32_t frame() const;                 // BINK.FrameNum of the frame shown (1-based, 0 before the first)
    // Planes as the game's textures hold them, tightly packed: Y and A ywidth x yheight, cR / cB cwidth x cheight.
    const std::vector<uint8_t>& plane(int i) const;   // 0 Y, 1 cR, 2 cB, 3 A
    // Interleaved PCM handed to the sound system so far and not yet taken (16-bit; 8-bit tracks are widened).
    size_t take_audio(int16_t* out, size_t max_samples);
    size_t audio_queued() const;
    void go_to(uint32_t frame);             // BinkGoto(frame, 0) (XenonBinkVideo::vf4 005953b0 always passes frame 1)
    void play();                            // XenonBinkVideo::vf1 00595210: BinkGoto(1), sound on, the first frame due at once
    void pause(bool on);                    // BinkPause (vf2 005952b0)
    void set_sound(bool on);                // BinkSetSoundOnOff (MuteAudio)
    void set_volume(uint32_t volume);       // BinkSetVolume(track 0, 0..65536) (vf5 005953f0); stored, applied by the caller
    uint32_t volume() const;
    uint64_t steps() const;                 // guest instructions executed (diagnostics)
    struct Impl;
private:
    explicit Player(Impl* d);
    Impl* d;
};

}  // namespace reflex_bink
