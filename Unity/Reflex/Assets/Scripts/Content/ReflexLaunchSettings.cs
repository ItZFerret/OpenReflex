using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace Reflex.Port.Content
{
    // What the PC game reads at start before its window exists (ReflexWinMain 008814a0), from the files MXSettings.exe (the launcher's
    // "Video Options") writes next to MXReflex.exe: graphics.cfg (GraphicsConfig_Load 00883500, GraphicsConfig_FindKey 00883820) and
    // language.txt (3 letters, 005e0100). _re/notes/input-settings.md. The install is never written: a per-user copy
    // (ReflexInstall.CacheDir/graphics.cfg, written by Save) takes precedence when it exists.
    //
    // What each value is in the game (XenonCore +0x3c display settings, vf4 / vf22):
    //   Fullscreen     +0x00 byte. 0: windowed, the back buffer is the largest 16:9 rect in ScreenWidth x ScreenHeight (FUN_007ad3c0);
    //                  1: the mode ScreenWidth x ScreenHeight @ ScreenRefresh, drawn into a centred 16:9 viewport (+0x70.. letterbox).
    //   ScreenWidth / ScreenHeight (defaults 1280 / 720, minimum 320 x 180), ScreenRefresh (+0x20, fullscreen only), AdaptorOrdinal (+0x1c).
    //   Antialiasing   +0x30: 0 none, 1 D3DMULTISAMPLE_2_SAMPLES, 2 D3DMULTISAMPLE_4_SAMPLES (if CheckDeviceMultiSampleType allows; 007ad550).
    //   MotionBlur     +0x01 byte (default 1): the post chain's motion blur pass (PostProcessMotionBlurPS, 007af510).
    //   VSync          +0x24 byte: PresentationInterval DEFAULT (vsync) when 1, IMMEDIATE when 0.
    //   TextureQuality +0x28 (default 2): at texture load (XenonTextureAsset 007b2820) a power-of-two, mipped texture whose name has
    //                  none of disp / terrain / heightmapasset / ui_comp_disp / tree / forest / eco / hftrn drops top mips while it is
    //                  larger than 128 (quality 0, at most 5 levels) or 512 (quality 1, at most 2 levels); 2 = full size (TextureMipDrop).
    //   ShadowQuality  +0x2c (default 2): row of the shadow table 0092e334 (FUN_00794fb0): 0 {192, 96, 192, 160}, 1 {384, 192, 384, 320},
    //                  2 {768, 384, 768, 640} (the shadow map sizes the renderer allocates; ShadowSizes).
    // language.txt: ENG JAP GER FRE SPA ITA KOR CHT (009c4ee4) -> [00a94078]; missing / unknown: 0 (ENG). The UI then picks that
    // language among UIRegistry's (FUN_005e01d0, else ENG). The game's own options have no language row.
    public sealed class ReflexLaunchSettings
    {
        public bool loaded;                       // graphics.cfg found (else the game's built-in defaults below)
        public string source;                     // the file read
        public bool fullscreen;                   // the game's default without a file is windowed
        public int width = 1280, height = 720, refresh, adapter;
        public int antialiasing;                  // 0 / 1 / 2
        public bool motionBlur = true, vsync;
        public int textureQuality = 2, shadowQuality = 2;
        public string language = "ENG";           // language.txt (ENG when missing)
        public int languageIndex;                 // its index in the game's table (0 ENG)
        public readonly Dictionary<string, int> values = new Dictionary<string, int>(StringComparer.Ordinal);

        public static readonly string[] Languages = { "ENG", "JAP", "GER", "FRE", "SPA", "ITA", "KOR", "CHT" };
        static readonly int[,] ShadowTable = { { 192, 96, 192, 160 }, { 384, 192, 384, 320 }, { 768, 384, 768, 640 }, { 128, 128, 512, 512 } };
        static readonly string[] FullSizeNames = { "disp", "terrain", "heightmapasset", "ui_comp_disp", "tree", "forest", "eco", "hftrn" };   // 009b75b8
        static readonly string[] FullSizeDatabases = { "mxui", "enginedependencies", "uiscene" };   // mxui covers mxui_dlc00n, mxui_streaming, mxuipatch

        static ReflexLaunchSettings current;
        /// The settings of this run (read once, as the game reads them at start).
        public static ReflexLaunchSettings Current => current ??= Load();
        /// True after Apply ran this session.
        public static bool Applied { get; private set; }

        /// MSAA samples for Antialiasing (1 none, 2, 4).
        public int MsaaSamples => antialiasing == 1 ? 2 : antialiasing == 2 ? 4 : 1;
        /// The shadow table row for ShadowQuality (four sizes, 0092e334).
        public int[] ShadowSizes { get { int q = Mathf.Clamp(shadowQuality, 0, 3); return new[] { ShadowTable[q, 0], ShadowTable[q, 1], ShadowTable[q, 2], ShadowTable[q, 3] }; } }
        /// ASPECT RATIO FILL SCREEN (ReflexSettings.AspectFill, the default): the whole screen or window, not the game's 16:9 picture.
        static bool Fill => Reflex.Port.Options.ReflexSettings.AspectFill == 1;
        /// The back buffer the game makes: fullscreen the mode itself, windowed the largest 16:9 rect (multiples of 16 x 9);
        /// with FILL SCREEN the window is ScreenWidth x ScreenHeight as chosen.
        public Vector2Int BackBuffer { get { var v = View16x9(out _); return fullscreen || Fill ? new Vector2Int(Mathf.Max(width, 320), Mathf.Max(height, 180)) : v; } }
        /// The game's viewport inside the back buffer, normalized (a centred 16:9 rect; the whole buffer when windowed or with FILL SCREEN).
        public Rect ViewportRect
        {
            get
            {
                var v = View16x9(out _); var bb = BackBuffer;
                if (!fullscreen || Fill) return new Rect(0, 0, 1, 1);
                float x = (bb.x - v.x) * 0.5f / bb.x, y = (bb.y - v.y) * 0.5f / bb.y;
                return new Rect(x, y, v.x / (float)bb.x, v.y / (float)bb.y);
            }
        }
        // FUN_007ad3c0: k = w / 16 (or h / 9 when wider than 16:9), the viewport 16k x 9k.
        Vector2Int View16x9(out int k)
        {
            int w = Mathf.Max(width, 320), h = Mathf.Max(height, 180);
            float f = (w <= h || (float)w / h <= 1.7777778f) ? w * 0.0625f : h * 0.11111111f;
            k = (int)f; return new Vector2Int(k * 16, k * 9);
        }

        /// How many top mips the game drops for a texture at load (XenonTextureAsset 007b2820, TextureQuality 0 / 1); 0 = full size.
        /// database: the database the texture's package loads from (FUN_0084c6b0 clears the reduction flag [00a956fa] for mxui,
        /// mxui_dlc*, mxui_streaming, mxuipatch, enginedependencies and uiscene; a bike mod's database by its own name, as the game
        /// loads a drop-in mod: "mod:<folder>>MXs" is MXs, whatever the folder is called); name: the texture asset's name.
        public int TextureMipDrop(string database, string name, int w, int h, int levels)
        {
            int q = textureQuality; if (q >= 2 || q < 0) return 0;
            string db = (BikeMods.BaseDb(database) ?? "").ToLowerInvariant();
            foreach (var n in FullSizeDatabases) if (db.Contains(n)) return 0;
            string lower = (name ?? "").ToLowerInvariant();
            foreach (var n in FullSizeNames) if (lower.Contains(n)) return 0;
            if ((w & (w - 1)) != 0 || (h & (h - 1)) != 0 || levels <= 1) return 0;
            int maxW = q == 0 ? 128 : 512, maxH = maxW, maxDrop = q == 0 ? 5 : 2, drop = 0;
            while (maxW < w && h > maxH && drop < levels && drop < maxDrop) { w >>= 1; h >>= 1; drop++; }
            return drop;
        }

        public static string UserFile => Path.Combine(ReflexInstall.CacheDir, "graphics.cfg");

        public static ReflexLaunchSettings Load()
        {
            var s = new ReflexLaunchSettings();
            string install = null;
            try { install = ReflexInstall.Dir; } catch (Exception) { }
            string file = null;
            try { if (File.Exists(UserFile)) file = UserFile; } catch (Exception) { }
            if (file == null && !string.IsNullOrEmpty(install)) { string f = Path.Combine(install, "graphics.cfg"); if (File.Exists(f)) file = f; }
            if (file != null)
            {
                try { s.Parse(File.ReadAllBytes(file)); s.loaded = true; s.source = file; }
                catch (Exception e) { Debug.LogWarning("graphics.cfg not read: " + e.Message); }
            }
            if (!string.IsNullOrEmpty(install))
            {
                string lt = Path.Combine(install, "language.txt");
                try
                {
                    if (File.Exists(lt))
                    {
                        // fgets(buf, 4): at most three characters, upper-cased, matched against the table
                        string code = System.Text.Encoding.ASCII.GetString(File.ReadAllBytes(lt));
                        int nl = code.IndexOfAny(new[] { '\r', '\n' }); if (nl >= 0) code = code.Substring(0, nl);
                        if (code.Length > 3) code = code.Substring(0, 3);
                        int i = Array.IndexOf(Languages, code.ToUpperInvariant());
                        if (i >= 0) { s.languageIndex = i; s.language = Languages[i]; }
                    }
                }
                catch (Exception) { }
            }
            return s;
        }

        // GraphicsConfig_Load: "Key=Value" per line, '#' starts a comment; values are integers.
        void Parse(byte[] bytes)
        {
            string text = System.Text.Encoding.ASCII.GetString(bytes);
            foreach (var raw in text.Split('\n'))
            {
                string line = raw; int hash = line.IndexOf('#'); if (hash >= 0) line = line.Substring(0, hash);
                int eq = line.IndexOf('='); if (eq <= 0) continue;
                string key = line.Substring(0, eq).Trim(), val = line.Substring(eq + 1).Trim();
                int n = 0, sign = 1, k = 0;
                if (k < val.Length && (val[k] == '-' || val[k] == '+')) { sign = val[k] == '-' ? -1 : 1; k++; }
                while (k < val.Length && char.IsDigit(val[k])) n = n * 10 + (val[k++] - '0');
                values[key] = n * sign;
            }
            int V(string key, int d) => values.TryGetValue(key, out int v) ? v : d;
            fullscreen = V("Fullscreen", 0) != 0;
            width = V("ScreenWidth", 1280); height = V("ScreenHeight", 720);
            adapter = V("AdaptorOrdinal", 0); refresh = V("ScreenRefresh", 0);
            antialiasing = V("Antialiasing", 0);
            textureQuality = V("TextureQuality", 2); shadowQuality = V("ShadowQuality", 2);
            motionBlur = V("MotionBlur", 1) != 0; vsync = V("VSync", 0) != 0;
        }

        /// Writes these values to the per-user graphics.cfg (never the install), in MXSettings.exe's key order.
        public void Save()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(UserFile));
            File.WriteAllText(UserFile, "#---------------------------------------------------------------------------------------------\r\n" +
                "# The port's per-user copy of the install's graphics.cfg (MXSettings.exe keys).\r\n" +
                "#---------------------------------------------------------------------------------------------\r\n\r\n" +
                $"Fullscreen={(fullscreen ? 1 : 0)}\r\nScreenWidth={width}\r\nScreenHeight={height}\r\nScreenRefresh={refresh}\r\nAdaptorOrdinal={adapter}\r\n" +
                $"Antialiasing={antialiasing}\r\nMotionBlur={(motionBlur ? 1 : 0)}\r\nVSync={(vsync ? 1 : 0)}\r\nTextureQuality={textureQuality}\r\nShadowQuality={shadowQuality}\r\n");
            current = null;
        }

        public override string ToString() =>
            $"{(loaded ? source : "no graphics.cfg (game defaults)")}: {(fullscreen ? "fullscreen" : "windowed")} {width}x{height}@{refresh} adapter {adapter}, " +
            $"back buffer {BackBuffer.x}x{BackBuffer.y}, viewport {ViewportRect}, AA {antialiasing} ({MsaaSamples}x), motion blur {(motionBlur ? 1 : 0)}, vsync {(vsync ? 1 : 0)}, " +
            $"texture quality {textureQuality}, shadow quality {shadowQuality} ({string.Join("/", ShadowSizes)}), language {language}";

        // At start, as the game does before its window: the display mode, vsync and MSAA of a player build. In the editor nothing is
        // changed (QualitySettings / the URP asset are project settings there); the values stay readable (Current) for the render
        // pipeline (MotionBlur, ShadowSizes, TextureMipDrop, ViewportRect).
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void ApplyAtStart()
        {
            if (Application.isEditor || Application.isBatchMode) return;
            try { Current.Apply(); } catch (Exception e) { Debug.LogWarning("graphics.cfg not applied: " + e.Message); }
        }

        public void Apply()
        {
            var bb = BackBuffer;
            var mode = fullscreen ? FullScreenMode.FullScreenWindow : FullScreenMode.Windowed;
            if (!ReflexWindowOverride.TryApply())
            {
                // The display's own refresh rate, as ApplyDisplay (OPTIONS) does: the original's MXSettings writes ScreenRefresh=60, and passing
                // it switched a 120 Hz display to a 60 Hz mode at start, so VSync locked every frame to 60 (the borderless window needs no mode).
                Screen.SetResolution(bb.x, bb.y, mode);
            }
            QualitySettings.vSyncCount = vsync ? 1 : 0;
            if (Reflex.Port.Options.ReflexClientLive.LaunchFrameRateCap > 0 || Reflex.Port.Options.ReflexSettings.FpsCap > 0)
                Reflex.Port.Options.ReflexClientLive.ApplyFrameRate();
            QualitySettings.antiAliasing = MsaaSamples > 1 ? MsaaSamples : 0;
            if (UnityEngine.Rendering.GraphicsSettings.currentRenderPipeline is UnityEngine.Rendering.Universal.UniversalRenderPipelineAsset urp) urp.msaaSampleCount = MsaaSamples;
            Applied = true;
            Debug.Log("graphics.cfg applied: " + this);
        }
    }
}
