// OPTIONS > VIDEO > QUALITY: ANTI-ALIASING (OFF / MSAA 2X / MSAA 4X: graphics.cfg Antialiasing as the game's own option; TAA: ours, LIGHTING
// ENHANCED only, Rendering/ReflexTemporalAA.cs), SHARPENING (the TAA image's contrast-adaptive sharpening) and UPSCALING (TAA renders the
// world below the screen's resolution and rebuilds the full image over frames). F9 in a race cycles ANTI-ALIASING (this test build).
using UnityEngine;
using UnityEngine.UIElements;
using Reflex.Port.Rendering;

namespace Reflex.Port.Options
{
    public sealed partial class ReflexOptionsMenu
    {
        void BuildAntiAliasing(VisualElement panel)
        {
            Named(OptUi.Spinner("ANTI-ALIASING", () => ReflexAntiAliasing.Label, ReflexAntiAliasing.Cycle, panel), "antialiasing");
            Named(OptUi.Slider("SHARPENING", () => ReflexAntiAliasing.Sharpening / 100f, v => ReflexAntiAliasing.Sharpening = Mathf.RoundToInt(v * 100f), panel, 0f, 1f, 0.1f), "sharpening");
            Named(OptUi.Spinner("UPSCALING", () => ReflexAntiAliasing.UpscalingNames[ReflexAntiAliasing.Upscaling]
                                                 + (ReflexAntiAliasing.Upscaling > 0 ? $"  ({Mathf.RoundToInt(ReflexAntiAliasing.UpscalingScales[ReflexAntiAliasing.Upscaling] * 100f)}%)" : ""),
                d => ReflexAntiAliasing.Upscaling = ((ReflexAntiAliasing.Upscaling + d) % 3 + 3) % 3, panel), "upscaling");
            Note(panel, "ANTI-ALIASING smooths the jagged edges and the shimmer of fences, cables, bikes, grass and distant hills. TAA (temporal, with LIGHTING ENHANCED; the default there) " +
                        "combines several slightly shifted frames into one clean picture and follows the bikes and riders as they move; MSAA 2X / 4X are the game's own (graphics.cfg), " +
                        "smoothing geometry edges only. With LIGHTING ORIGINAL, TAA draws as OFF (exactly the original). SHARPENING restores the fine texture detail after TAA (0% none, 50% default). " +
                        "UPSCALING (TAA only) draws the world at 67% (QUALITY) or 58% (BALANCED) of the resolution and rebuilds the full picture over frames: faster, a little softer. " +
                        "Apply at once; CTRL+F9 in a race cycles ANTI-ALIASING. Ours.");
        }
    }
}
