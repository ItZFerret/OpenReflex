// PHOTO MODE (ReflexReplay, after Open Unleashed's MxaReplay.Photo): the moment frozen, the camera free (or orbiting the rider), and
// a photographer's settings:
//   CAMERA     FREE / ORBIT, FIELD OF VIEW, ROLL, SUBJECT (the rider the orbit and the focus follow)
//   LENS       FOCUS (OFF / AUTO on the rider / MANUAL), FOCUS DISTANCE, APERTURE (f/1.2 .. f/16: the cinematic pass's bokeh depth of
//              field, ReflexReplay.Cinematic.cs), MOTION BLUR (the shutter, up to 1/20 s)
//   COLOUR     FILTER (black & white, noir, sepia, vivid, warm, cool, cinematic, faded film, sunset, bleach, retro), EXPOSURE,
//              CONTRAST, SATURATION, TEMPERATURE, TINT
//   EFFECTS    LENS EFFECTS (flare, dirt, a subtle lens look), VIGNETTE, FILM GRAIN, CHROMATIC ABERRATION (the cinematic pass), GLOW (bloom)
//   FRAME      ASPECT (the shot cropped to it, the screen masked), GRID, LOGO (the OPEN REFLEX logo: OFF / BOTTOM RIGHT / BOTTOM LEFT
//              / TOP RIGHT, previewed on screen where it will be)
//   SCENE      the local rider and machine, the other riders
//   OUTPUT     SCREEN, 2X, 4K or 8K (within the GPU's texture limit)
// Reflex draws its own original post chain before URP's (ReflexRenderPipeline: the tonemapped image comes out of it). The depth of
// field, motion blur and lens effects are the cinematic pass's (ReflexReplay.Cinematic.cs, Rendering/ReflexCinematic.cs: in the HDR image
// and right after Reflex's tonemap). The colour settings (filter, exposure, white balance, glow) are a URP post-processing volume above
// everything (priority 1000) that grades the finished image: the camera's URP post-processing is switched on only while a setting needs
// it, and the volume holds every other URP effect at its neutral value, so a default volume profile cannot grade the picture a second time.
// The shot: the game camera rendered offscreen at the chosen size (the HUD and this UI are not in it; the ORIGINAL 16:9 ASPECT RATIO's
// black bars neither), cropped to the aspect, the logo blended in, encoded on a worker thread and saved as a PNG in
// Pictures/Open Reflex (else <persistentDataPath>/Photos; REFLEX_PHOTO_DIR / -rxPhotoDir for tests).
using System;
using System.Collections;
using System.IO;
using System.Threading.Tasks;
using Reflex.Port.Options;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace Reflex.Port.Replay
{
    public sealed partial class ReflexReplay
    {
        public sealed class PhotoSettings
        {
            public bool orbit;
            public float roll;
            public int focus = 1;            // 0 OFF, 1 AUTO, 2 MANUAL
            public float focusDistance = 20f;
            public int aperture = 4;         // index into Apertures (f/4: the rider sharp, the background soft)
            public int filter;
            public float exposure, contrast, saturation, temperature, tint;
            public float vignette, grain, chromatic, glow;
            /// CINEMATIC (ReflexReplay.Cinematic.cs): MOTION BLUR is the shutter time as a share of 1/20 s (0 off); LENS EFFECTS adds the lens
            /// flare and dirt and a subtle chromatic aberration, vignette and grain under the sliders. MOTION BLUR starts OFF (w-replayterrain: the
            /// frozen moment is sharp unless the photographer opens the shutter).
            public float motionBlur = 0f;
            public bool lens = true;
            public int aspect, grid, resolution;
            public bool showRider = true, showBike = true, showOthers = true;

            /// LOGO: the corner of the OPEN REFLEX logo (0 OFF); kept between runs.
            public int logo
            {
                get => Mathf.Clamp(ReflexSave.GetInt("OpenReflex.PhotoLogo", 0), 0, LogoNames.Length - 1);
                set { ReflexSave.SetInt("OpenReflex.PhotoLogo", Mathf.Clamp(value, 0, LogoNames.Length - 1)); ReflexSave.Save(); }
            }

            public void ResetLook()
            {
                roll = 0f; focus = 1; focusDistance = 20f; aperture = 4; filter = 0;
                exposure = contrast = saturation = temperature = tint = 0f;
                vignette = grain = chromatic = glow = 0f;
                motionBlur = 0f; lens = true;
                aspect = grid = 0;
                showRider = showBike = showOthers = true;
            }

            /// Something only the post-processing volume can show.
            /// (the depth of field, vignette, grain and chromatic aberration sliders are the cinematic pass's: ReflexCinematic.cs)
            public bool NeedsPost => filter != 0 || exposure != 0f || contrast != 0f || saturation != 0f || temperature != 0f || tint != 0f || glow > 0f;
        }

        public static readonly float[] Apertures = { 1.2f, 1.4f, 2f, 2.8f, 4f, 5.6f, 8f, 11f, 16f };
        public static readonly string[] FocusNames = { "OFF", "AUTO", "MANUAL" };
        public static readonly string[] FilterNames = { "NONE", "BLACK & WHITE", "NOIR", "SEPIA", "VIVID", "WARM", "COOL", "CINEMATIC", "FADED FILM", "SUNSET", "BLEACH", "RETRO" };
        public static readonly string[] AspectNames = { "SCREEN", "16:9", "21:9", "4:3", "1:1", "4:5", "9:16" };
        static readonly float[] AspectRatios = { 0f, 16f / 9f, 2.39f, 4f / 3f, 1f, 0.8f, 9f / 16f };
        public static readonly string[] GridNames = { "OFF", "THIRDS", "CENTRE", "GOLDEN" };
        public static readonly string[] LogoNames = { "OFF", "BOTTOM RIGHT", "BOTTOM LEFT", "TOP RIGHT" };
        public static readonly string[] ResolutionNames = { "SCREEN", "2X", "4K", "8K" };

        /// PHOTO MODE's settings (kept across openings in this run; LOGO between runs).
        public static readonly PhotoSettings Photo = new PhotoSettings();

        public bool Capturing { get; private set; }
        public string LastPhotoPath { get; private set; }
        public Texture2D LastPhoto { get; private set; }
        public event Action<string> PhotoSaved;

        /// The lens and colour settings take effect (URP post-processing on the game camera).
        public bool PostAvailable => camera && camera.GetComponent<UniversalAdditionalCameraData>();

        /// The frame's aspect (width / height) of the setting; 0 = the screen's.
        public static float FrameAspect(int i) => i > 0 && i < AspectRatios.Length ? AspectRatios[i] : 0f;

        public const string LogoResource = "OpenFrontEnd/OpenReflexPhotoLogo";
        static Texture2D logoTexture; static bool logoLooked;
        /// The OPEN REFLEX logo for the photos (Resources/OpenFrontEnd/OpenReflexPhotoLogo.png: OPEN_REFLEX_WHITE cropped to its art).
        public static Texture2D LogoTexture
        {
            get
            {
                if (!logoLooked) { logoLooked = true; logoTexture = Resources.Load<Texture2D>(LogoResource); if (!logoTexture) Debug.LogWarning("[Replay] the photo logo " + LogoResource + " is missing"); }
                return logoTexture;
            }
        }

        /// Where the logo goes in a picture w x h (pixels, origin top left): a fifth of the width of the 16:9 frame of that height (so
        /// it is the same size on a 16:9, an ultrawide and a 4:3 picture of the same height; a tall picture: a fifth of its width), with a
        /// margin of 3.5 % of the short side.
        public static Rect LogoRect(float w, float h, int corner, float logoAspect)
        {
            float basis = Mathf.Min(w, h * 16f / 9f);
            float lw = basis * 0.2f, lh = lw / Mathf.Max(0.1f, logoAspect);
            float margin = Mathf.Min(w, h) * 0.035f;
            float x = corner == 2 ? margin : w - lw - margin;
            float y = corner == 3 ? margin : h - lh - margin;
            return new Rect(x, y, lw, lh);
        }

        public static string PhotoFolder
        {
            get
            {
                var a = Environment.GetCommandLineArgs();
                int k = Array.IndexOf(a, "-rxPhotoDir");
                if (k >= 0 && k + 1 < a.Length) return a[k + 1];
                string env = Environment.GetEnvironmentVariable("REFLEX_PHOTO_DIR");
                if (!string.IsNullOrEmpty(env)) return env;
                string pics = null;
                try { pics = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures); } catch (Exception) { }
                if (string.IsNullOrEmpty(pics) || !Directory.Exists(pics))
                    try { pics = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Pictures"); } catch (Exception) { pics = null; }
                return !string.IsNullOrEmpty(pics) && Directory.Exists(pics) ? Path.Combine(pics, "Open Reflex") : Path.Combine(Application.persistentDataPath, "Photos");
            }
        }

        // ------------------------------------------------------------------------------------------ the volume

        Volume volume;
        VolumeProfile profile;
        DepthOfField dof; ColorAdjustments ca; WhiteBalance wb; Vignette vig; FilmGrain grainFx; ChromaticAberration chroma; Bloom bloom;
        ChannelMixer mixer; SplitToning split; LiftGammaGain lgg; Tonemapping tone; MotionBlur blur; LensDistortion distort; PaniniProjection panini;
        ShadowsMidtonesHighlights smh; ColorLookup lut;
        bool postBefore, postTaken;

        void EnsureVolume()
        {
            if (volume) return;
            var go = new GameObject("Open Reflex photo volume") { hideFlags = HideFlags.DontSave };
            go.transform.SetParent(transform, false);
            // on a layer the camera's volume mask sees
            var data = camera ? camera.GetComponent<UniversalAdditionalCameraData>() : null;
            int mask = data ? data.volumeLayerMask.value : 1;
            for (int l = 0; l < 32; l++) if ((mask & (1 << l)) != 0) { go.layer = l; break; }
            volume = go.AddComponent<Volume>();
            volume.isGlobal = true;
            volume.priority = 1000f;
            profile = ScriptableObject.CreateInstance<VolumeProfile>();
            profile.name = "Open Reflex photo";
            profile.hideFlags = HideFlags.DontSave;
            dof = profile.Add<DepthOfField>(); ca = profile.Add<ColorAdjustments>(); wb = profile.Add<WhiteBalance>(); vig = profile.Add<Vignette>();
            grainFx = profile.Add<FilmGrain>(); chroma = profile.Add<ChromaticAberration>(); bloom = profile.Add<Bloom>(); mixer = profile.Add<ChannelMixer>();
            split = profile.Add<SplitToning>(); lgg = profile.Add<LiftGammaGain>(); tone = profile.Add<Tonemapping>(); blur = profile.Add<MotionBlur>();
            distort = profile.Add<LensDistortion>(); panini = profile.Add<PaniniProjection>(); smh = profile.Add<ShadowsMidtonesHighlights>(); lut = profile.Add<ColorLookup>();
            // the effects this never uses: held neutral (a default volume profile must not grade Reflex's finished image)
            Set(tone.mode, TonemappingMode.None);
            Set(blur.intensity, 0f);
            Set(distort.intensity, 0f);
            Set(panini.distance, 0f);
            Set(smh.shadows, new Vector4(1f, 1f, 1f, 0f)); Set(smh.midtones, new Vector4(1f, 1f, 1f, 0f)); Set(smh.highlights, new Vector4(1f, 1f, 1f, 0f));
            Set(lut.contribution, 0f);
            Set(ca.hueShift, 0f);
            volume.sharedProfile = profile;
            volume.enabled = false;
        }

        void SetPost(bool on)
        {
            var data = camera ? camera.GetComponent<UniversalAdditionalCameraData>() : null;
            if (!data) return;
            if (on && !postTaken) { postTaken = true; postBefore = data.renderPostProcessing; }
            if (postTaken) data.renderPostProcessing = on || postBefore;
            if (volume) volume.enabled = on;
        }

        void PhotoOpen() => EnsureVolume();

        void PhotoClose()
        {
            if (volume) volume.enabled = false;
            if (postTaken)
            {
                var data = camera ? camera.GetComponent<UniversalAdditionalCameraData>() : null;
                if (data) data.renderPostProcessing = postBefore;
                postTaken = false;
            }
        }

        void PhotoDestroy()
        {
            if (profile) Destroy(profile);
            if (LastPhoto) Destroy(LastPhoto);
        }

        /// Each viewer frame: the URP volume (colour: filter, exposure, white balance, glow) from the settings. The depth of field, motion
        /// blur and the lens effects are the cinematic pass's (ReflexReplay.Cinematic.cs / Rendering/ReflexCinematic.cs), not URP's.
        void PhotoFrame()
        {
            EnsureVolume();
            bool want = PostAvailable && Face == View.Photo && Photo.NeedsPost;
            SetPost(want);
            Set(dof.mode, DepthOfFieldMode.Off);
            if (want) Grade(Photo);
        }

        // the filters' changes on top of the sliders
        struct Look
        {
            public float exposure, contrast, saturation, temperature, tint, vignette, grain, chromatic, fadeLift;
            public bool sepia, split;
            public Color filter;
        }

        static Look FilterLook(int f)
        {
            var l = new Look { filter = Color.white };
            switch (f)
            {
                case 1: l.saturation = -100f; l.contrast = 12f; break;                                                                 // BLACK & WHITE
                case 2: l.saturation = -100f; l.contrast = 42f; l.exposure = -0.25f; l.vignette = 0.38f; l.grain = 0.3f; break;         // NOIR
                case 3: l.sepia = true; l.contrast = -6f; l.vignette = 0.22f; l.grain = 0.15f; break;                                 // SEPIA
                case 4: l.saturation = 38f; l.contrast = 16f; break;                                                                    // VIVID
                case 5: l.temperature = 32f; l.tint = 4f; l.saturation = 6f; break;                                                     // WARM
                case 6: l.temperature = -34f; l.tint = -4f; break;                                                                      // COOL
                case 7: l.split = true; l.contrast = 14f; l.saturation = 4f; l.vignette = 0.18f; break;                                // CINEMATIC
                case 8: l.contrast = -24f; l.saturation = -32f; l.fadeLift = 0.08f; l.grain = 0.32f; l.temperature = 10f; break;       // FADED FILM
                case 9: l.temperature = 55f; l.tint = 12f; l.saturation = 16f; l.exposure = 0.1f; l.filter = new Color(1f, 0.93f, 0.85f); break; // SUNSET
                case 10: l.saturation = -58f; l.contrast = 36f; l.exposure = 0.1f; break;                                               // BLEACH
                case 11: l.chromatic = 0.45f; l.saturation = 12f; l.grain = 0.42f; l.vignette = 0.25f; l.contrast = 8f; l.fadeLift = 0.03f; break; // RETRO
            }
            return l;
        }

        /// The colour and effects of the settings (null: neutral, only the depth of field).
        void Grade(PhotoSettings p)
        {
            var f = FilterLook(p != null ? p.filter : 0);
            float exposure = (p?.exposure ?? 0f) + f.exposure, contrast = (p?.contrast ?? 0f) + f.contrast, saturation = (p?.saturation ?? 0f) + f.saturation;
            float temperature = (p?.temperature ?? 0f) + f.temperature, tint = (p?.tint ?? 0f) + f.tint;
            Set(ca.postExposure, exposure);
            Set(ca.contrast, Mathf.Clamp(contrast, -100f, 100f));
            Set(ca.saturation, Mathf.Clamp(saturation, -100f, 100f));
            Set(ca.colorFilter, f.filter);
            Set(wb.temperature, Mathf.Clamp(temperature, -100f, 100f));
            Set(wb.tint, Mathf.Clamp(tint, -100f, 100f));
            float v = Mathf.Clamp01(f.vignette);   // (the sliders are the cinematic pass's: the filters' own looks only)
            Set(vig.intensity, v); Set(vig.smoothness, 0.45f); Set(vig.color, Color.black); Set(vig.rounded, false);
            float g = Mathf.Clamp01(f.grain);
            Set(grainFx.intensity, g); Set(grainFx.type, FilmGrainLookup.Medium3); Set(grainFx.response, 0.75f);
            Set(chroma.intensity, Mathf.Clamp01(f.chromatic));
            Set(bloom.intensity, (p?.glow ?? 0f) * 1.5f); Set(bloom.threshold, 0.9f);
            if (f.sepia)
            {
                Set(mixer.redOutRedIn, 39.3f); Set(mixer.redOutGreenIn, 76.9f); Set(mixer.redOutBlueIn, 18.9f);
                Set(mixer.greenOutRedIn, 34.9f); Set(mixer.greenOutGreenIn, 68.6f); Set(mixer.greenOutBlueIn, 16.8f);
                Set(mixer.blueOutRedIn, 27.2f); Set(mixer.blueOutGreenIn, 53.4f); Set(mixer.blueOutBlueIn, 13.1f);
            }
            else
            {
                Set(mixer.redOutRedIn, 100f); Set(mixer.redOutGreenIn, 0f); Set(mixer.redOutBlueIn, 0f);
                Set(mixer.greenOutRedIn, 0f); Set(mixer.greenOutGreenIn, 100f); Set(mixer.greenOutBlueIn, 0f);
                Set(mixer.blueOutRedIn, 0f); Set(mixer.blueOutGreenIn, 0f); Set(mixer.blueOutBlueIn, 100f);
            }
            if (f.split) { Set(split.shadows, new Color(0.18f, 0.55f, 0.6f)); Set(split.highlights, new Color(1f, 0.62f, 0.3f)); Set(split.balance, -10f); }
            else { Set(split.shadows, Color.grey); Set(split.highlights, Color.grey); Set(split.balance, 0f); }
            Set(lgg.lift, new Vector4(1f, 1f, 1f, f.fadeLift));
            Set(lgg.gamma, new Vector4(1f, 1f, 1f, 0f));
            Set(lgg.gain, new Vector4(1f, 1f, 1f, 0f));
        }

        static void Set<T>(VolumeParameter<T> p, T v) { p.overrideState = true; p.value = v; }

        // ------------------------------------------------------------------------------------------- the shot

        public void TakePhoto()
        {
            if (Capturing || !camera) return;
            StartCoroutine(Capture());
        }

        /// The picture's size for the RESOLUTION setting from the camera's own size on screen (sw x sh), within the GPU's limit.
        public static Vector2Int ShotSize(int sw, int sh, int resolution)
        {
            sw = Mathf.Max(16, sw); sh = Mathf.Max(16, sh);
            float big = Mathf.Max(sw, sh);
            float k = resolution == 1 ? 2f : resolution == 2 ? 3840f / big : resolution == 3 ? 7680f / big : 1f;
            k = Mathf.Min(k, Mathf.Min(SystemInfo.maxTextureSize, 16384) / big);
            return new Vector2Int(Mathf.RoundToInt(sw * k), Mathf.RoundToInt(sh * k));
        }

        IEnumerator Capture()
        {
            Capturing = true;
            if (OffscreenFlip < 0) yield return DetectFlip();
            if (Ui) Ui.CaptureBegin();
            yield return new WaitForEndOfFrame();
            string path = null, error = null;
            Texture2D tex = null;
            RenderTexture rt = null;
            RenderTexture prev = camera.targetTexture;
            Rect prevRect = camera.rect;
            int cw = 0, ch = 0;
            Task write = null;
            try
            {
                // the camera's own picture size (the ORIGINAL 16:9 ASPECT RATIO draws it in a part of the screen)
                var size = ShotSize(camera.pixelWidth, camera.pixelHeight, Photo.resolution);
                int w = size.x, h = size.y;
                rt = new RenderTexture(w, h, 24, RenderTextureFormat.ARGB32) { antiAliasing = 1, name = "Open Reflex photo" };
                rt.Create();
                camera.rect = new Rect(0f, 0f, 1f, 1f);
                camera.targetTexture = rt;
                ShutterRender();   // (the shutter's sub-frames when PHOTO MODE's MOTION BLUR is on: ReflexReplay.Cinematic.cs)
                camera.targetTexture = prev;
                camera.rect = prevRect;
                // the frame's crop, centred (as the screen's mask shows it)
                float a = FrameAspect(Photo.aspect);
                cw = w; ch = h;
                if (a > 0f) { if (w / (float)h > a) cw = Mathf.RoundToInt(h * a); else ch = Mathf.RoundToInt(w / a); }
                var active = RenderTexture.active;
                RenderTexture.active = rt;
                tex = new Texture2D(cw, ch, TextureFormat.RGB24, false);
                tex.ReadPixels(new Rect((w - cw) / 2, (h - ch) / 2, cw, ch), 0, 0);
                tex.Apply(false);
                RenderTexture.active = active;
                if (OffscreenFlip == 1) FlipRows(tex);
                if (Photo.logo > 0) StampLogo(tex, Photo.logo);
                string dir = PhotoFolder;
                try { Directory.CreateDirectory(dir); }
                catch (Exception) { dir = Path.Combine(Application.persistentDataPath, "Photos"); Directory.CreateDirectory(dir); }
                string track = Runtime && !string.IsNullOrEmpty(Runtime.level) ? Runtime.level : "Ride";
                foreach (char c in Path.GetInvalidFileNameChars()) track = track.Replace(c, '-');
                string stem = $"Open Reflex {track.Trim()} {DateTime.Now:yyyy-MM-dd HH.mm.ss}";
                path = Path.Combine(dir, stem + ".png");
                for (int n = 2; File.Exists(path) && n < 100; n++) path = Path.Combine(dir, stem + $" ({n}).png");
                byte[] raw = tex.GetRawTextureData();
                uint pw = (uint)cw, ph = (uint)ch; string target = path;
                // the PNG is encoded off the main thread (an 8K shot takes a while)
                write = Task.Run(() => File.WriteAllBytes(target, ImageConversion.EncodeArrayToPNG(raw, GraphicsFormat.R8G8B8_SRGB, pw, ph, pw * 3)));
            }
            catch (Exception e)
            {
                error = e.Message;
                Debug.LogError("[Replay] photo: " + e);
                path = null;
            }
            finally
            {
                if (camera) { camera.targetTexture = prev; camera.rect = prevRect; }
                if (rt) { rt.Release(); Destroy(rt); }
            }
            if (write != null)
            {
                while (!write.IsCompleted) yield return null;
                if (write.IsFaulted) { error = write.Exception?.GetBaseException().Message; Debug.LogError("[Replay] photo not saved: " + error); path = null; }
            }
            if (path != null)
            {
                Debug.Log($"[Replay] photo: {path} ({cw}x{ch}, {FilterNames[Photo.filter]}, logo {LogoNames[Photo.logo]}, fov {camera.fieldOfView:0.#}, {ResolutionNames[Photo.resolution]})");
                if (LastPhoto) Destroy(LastPhoto);
                LastPhoto = tex;
                LastPhotoPath = path;
                Reflex.Port.OpenFrontEnd.OuSound.Play("Accept");
                PhotoSaved?.Invoke(path);
            }
            else if (tex) Destroy(tex);
            if (Ui) Ui.CaptureEnd(path, error);
            Capturing = false;
        }

        /// The game camera rendered into a texture comes back upside down here (1) or not (0); -1 not known yet. It used to come back
        /// flipped on a UV-starts-at-top API (Metal, D3D; measured on the Mac player) because the display gamma ramp's copy ran on offscreen
        /// renders too (and squashed them to the screen's size); ReflexGammaOutput skips them now, so they come back upright. With a window
        /// the first photo of a run still checks it against the screen (DetectFlip); without one (-batchmode) upright is assumed.
        public static int OffscreenFlip { get; private set; } = -1;

        /// Renders the camera offscreen at the screen's size and compares it, small and grey, with the frame on screen (this UI hidden
        /// for that frame), upright and flipped: the closer one says how offscreen renders come back.
        IEnumerator DetectFlip()
        {
            if (Application.isBatchMode || !camera) { OffscreenFlip = 0; yield break; }   // (offscreen renders come back upright since the display gamma ramp skips them: ReflexGammaOutput)
            bool hidden = Ui && Ui.Root != null && Ui.Root.style.display != UnityEngine.UIElements.DisplayStyle.None;
            if (hidden) Ui.Root.style.display = UnityEngine.UIElements.DisplayStyle.None;
            yield return null;
            yield return new WaitForEndOfFrame();
            Texture2D screen = null, off = null; RenderTexture rt = null;
            try
            {
                screen = ScreenCapture.CaptureScreenshotAsTexture();
                var px = camera.pixelRect;
                int w = Mathf.Max(16, (int)px.width), h = Mathf.Max(16, (int)px.height);
                rt = new RenderTexture(w, h, 24, RenderTextureFormat.ARGB32);
                var prev = camera.targetTexture; var rect = camera.rect;
                camera.rect = new Rect(0, 0, 1, 1); camera.targetTexture = rt; camera.Render(); camera.targetTexture = prev; camera.rect = rect;
                var active = RenderTexture.active; RenderTexture.active = rt;
                off = new Texture2D(w, h, TextureFormat.RGB24, false); off.ReadPixels(new Rect(0, 0, w, h), 0, 0); off.Apply(false);
                RenderTexture.active = active;
                // 64 x 36 grey samples of both (the screen's: inside the camera's own rectangle)
                const int SW = 64, SH = 36;
                double up = 0, down = 0;
                for (int y = 0; y < SH; y++)
                    for (int x = 0; x < SW; x++)
                    {
                        float u = (x + 0.5f) / SW, v = (y + 0.5f) / SH;
                        float a = screen.GetPixelBilinear((px.x + u * px.width) / screen.width, (px.y + v * px.height) / screen.height).grayscale;
                        float b = off.GetPixelBilinear(u, v).grayscale, c = off.GetPixelBilinear(u, 1f - v).grayscale;
                        up += Mathf.Abs(a - b); down += Mathf.Abs(a - c);
                    }
                OffscreenFlip = down < up ? 1 : 0;
                Debug.Log($"[Replay] offscreen renders come back {(OffscreenFlip == 1 ? "upside down" : "upright")} (difference upright {up / (SW * SH):0.000}, flipped {down / (SW * SH):0.000})");
            }
            catch (Exception e) { OffscreenFlip = 0; Debug.LogWarning("[Replay] flip check: " + e.Message); }
            finally
            {
                if (screen) Destroy(screen); if (off) Destroy(off); if (rt) { rt.Release(); Destroy(rt); }
                if (hidden && Ui && Ui.Root != null) Ui.Root.style.display = UnityEngine.UIElements.DisplayStyle.Flex;
            }
        }

        /// Turns the picture's rows over (an RGB24 / RGBA32 texture).
        static void FlipRows(Texture2D tex)
        {
            var raw = tex.GetRawTextureData();
            int row = raw.Length / tex.height;
            var tmp = new byte[row];
            for (int y = 0, z = tex.height - 1; y < z; y++, z--)
            {
                System.Buffer.BlockCopy(raw, y * row, tmp, 0, row);
                System.Buffer.BlockCopy(raw, z * row, raw, y * row, row);
                System.Buffer.BlockCopy(tmp, 0, raw, z * row, row);
            }
            tex.LoadRawTextureData(raw);
            tex.Apply(false);
        }

        /// The OPEN REFLEX logo in a corner of the shot (LogoRect), 92 % opaque, blended over the picture's own pixels.
        void StampLogo(Texture2D tex, int corner)
        {
            var logo = LogoTexture;
            if (!logo) return;
            var r = LogoRect(tex.width, tex.height, corner, logo.width / (float)logo.height);
            int lw = Mathf.RoundToInt(r.width), lh = Mathf.RoundToInt(r.height);
            if (lw < 8 || lh < 4) return;
            int x0 = Mathf.RoundToInt(r.x), y0 = tex.height - Mathf.RoundToInt(r.y) - lh;   // texture rows start at the bottom
            x0 = Mathf.Clamp(x0, 0, tex.width - lw); y0 = Mathf.Clamp(y0, 0, tex.height - lh);
            // the logo texture is not readable: scaled on the GPU (its mips filter it down), read back
            var tmp = RenderTexture.GetTemporary(lw, lh, 0, RenderTextureFormat.ARGB32);
            var active = RenderTexture.active;
            Graphics.Blit(logo, tmp);
            RenderTexture.active = tmp;
            var t = new Texture2D(lw, lh, TextureFormat.RGBA32, false);
            t.ReadPixels(new Rect(0, 0, lw, lh), 0, 0);
            t.Apply(false);
            RenderTexture.active = active;
            RenderTexture.ReleaseTemporary(tmp);
            var src = t.GetPixels32();
            Destroy(t);
            var dst = tex.GetPixels(x0, y0, lw, lh);
            for (int i = 0; i < dst.Length; i++)
            {
                var s = src[i];
                float al = s.a / 255f * 0.92f;
                if (al <= 0.003f) continue;
                var d = dst[i];
                dst[i] = new Color(s.r / 255f * al + d.r * (1f - al), s.g / 255f * al + d.g * (1f - al), s.b / 255f * al + d.b * (1f - al), 1f);
            }
            tex.SetPixels(x0, y0, lw, lh, dst);
            tex.Apply(false);
        }

        /// Opens the photo folder in the system's file browser.
        public static void RevealPhotos()
        {
            string dir = PhotoFolder;
            try { Directory.CreateDirectory(dir); } catch (Exception) { }
            Application.OpenURL(new Uri(dir).AbsoluteUri);
        }
    }
}
