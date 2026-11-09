using System.Collections.Generic;
using Reflex.Port.Content;
using UnityEngine;
using UnityEngine.Rendering;

namespace Reflex.Port.Rendering
{
    // The game's display gamma ramp (UISettings_Apply 005b9af0 -> 007aff40 builds 3 x 256 16-bit entries at renderer +0x7c and flags
    // +0x67c; the frame start 007af510 hands them to IDirect3DDevice9::SetGammaRamp (vtable +0x54, swap chain 0, D3DSGR_NO_CALIBRATION)
    // when flagged). D3D9 applies a device gamma ramp in fullscreen only, to everything presented (scene, HUD, front end). The port:
    // after all cameras of a frame (UI overlays included) the back buffer goes through the same table (ReflexScreenAdjustment.Ramp,
    // _ReflexGammaRamp), only when graphics.cfg asks for fullscreen. Off in the editor's game view unless REFLEX_GAMMA_RAMP=1.
    public static class ReflexGammaOutput
    {
        static Material material;
        static readonly int TempId = Shader.PropertyToID("_ReflexGammaOutputCopy");
        public static bool Enabled { get; set; } = true;

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Init()
        {
            RenderPipelineManager.endContextRendering -= OnEnd;
            RenderPipelineManager.endContextRendering += OnEnd;
        }

        static bool Active()
        {
            if (!Enabled) return false;
            string env = System.Environment.GetEnvironmentVariable("REFLEX_GAMMA_RAMP");   // "0" off, "1" on (also in the editor)
            if (env == "0" || (Application.isEditor && env != "1")) return false;
            var s = ReflexLaunchSettings.Current;
            return s != null && s.fullscreen;
        }

        static void OnEnd(ScriptableRenderContext ctx, List<Camera> cameras)
        {
            if (!Active()) return;
            // only a context that draws to the screen: the device ramp applies to what is presented. An offscreen render (PHOTO MODE, the
            // replay probe, tests: Camera.Render() into a target) also ends a context, and there CameraTarget is that target - the copy below
            // went through a Screen-sized texture with point filtering and the ramp's row flip, so a 4K photo came back as the screen's
            // resolution stretched (and upside down) (w-weather)
            if (!OnScreen(cameras)) return;
            if (!material)
            {
                var sh = Shader.Find("Hidden/Reflex/GammaRamp");
                if (!sh || !sh.isSupported) { Enabled = false; Debug.LogWarning("ReflexGammaOutput: Hidden/Reflex/GammaRamp unavailable; the display gamma ramp is off."); return; }
                material = new Material(sh) { hideFlags = HideFlags.HideAndDontSave };
                material.SetFloat("_FlipY", SystemInfo.graphicsUVStartsAtTop ? 1f : 0f);
            }
            if (Shader.GetGlobalTexture("_ReflexGammaRamp") == null) return;
            var cmd = CommandBufferPool.Get("Reflex display gamma ramp");
            cmd.GetTemporaryRT(TempId, Screen.width, Screen.height, 0, FilterMode.Point, RenderTextureFormat.ARGB32, RenderTextureReadWrite.sRGB);
            cmd.Blit(BuiltinRenderTextureType.CameraTarget, TempId);
            cmd.Blit(TempId, BuiltinRenderTextureType.CameraTarget, material);
            cmd.ReleaseTemporaryRT(TempId);
            ctx.ExecuteCommandBuffer(cmd);
            ctx.Submit();
            CommandBufferPool.Release(cmd);
        }

        static bool OnScreen(List<Camera> cameras)
        {
            if (cameras == null) return false;
            foreach (var c in cameras) if (c && c.targetTexture == null) return true;
            return false;
        }
    }
}
