// The front end's Bink videos (Content/ReflexVideo.cs plays them with the install's binkw32.dll):
//
// * UIVideoComponent (MXReflex.exe 005cdaa0 .. 005ce2b4), a UIImageComponent drawing its TextureAsset render target: properties
//   (bitfield +0x2b4) AutoStart, Loop, LoadToMemory, MuteAudio, VsyncEnabled, LoadInInitialize (default TRUE), Video, DoneEvent,
//   TextureAsset. Activation (vf3 005ce090) plays an AutoStart video, deactivation (vf5 005ce0c0) closes it; Play (005cdf20) opens
//   it if needed, starts it (XenonBinkVideo::vf1) and, with MuteAudio, sets 0xa95820 = 1 (Stop clears it; its reader is not
//   identified: here the UI-music mute, _re/notes/bink.md).
//   Its update (vf11 005ce0e0) follows the video's state: Done (2) -> the script event "UIVideoComponent::VideoDone" and the
//   DoneEvent UI event, Looped (0) / LoopStart (1). The video volume is the game's sound volume product (0xd679f4 * 0xd679f8 *
//   0xd67a0c, the options' EffectsVolume slider at 0xd679f8 via 005b9bc7; here ReflexAudio.master).
// * The layers' triggers the videos use: UIEventTrigger (TriggerEventName -> Event / ScriptFunction: the DoneEvents),
//   UIKeypressTrigger with an Event (skipping a logo, leaving Attract / Credits), UIElapsedTimeTrigger with an Event
//   (LegalScreen, 6 s) and the Title's 30 s OnAttractModeTrigger.
// * The startup sequence (Intro.<LANG> IntroPageGraph -> FrontEndIntro.<LANG> FrontEndIntroPageGraph.pag, RELEASE path of
//   InitialIntroScreen:OnGameContentInstallChanged): Legal Screen Page (6 s) -> Publisher Video Page (Nordic_Logo_wSound, not
//   skippable) -> Studio Video Page (Rainbow_Logo_wSound) -> Studio Video Page2 (DHLogo) -> Intro Video Page (Attract) -> Loading
//   Screen Page (InitialLoadLayer:CheckForAdvance each second: "GotoTitlePage") -> the Title Page (PageIn).
// * The scripts of those layers: StudioVideoLayer / StudioVideo2Layer / IntroVideoLayer / LegalScreen / Attract / Credits.
using System;
using System.Collections.Generic;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;

namespace Reflex.Port.FrontEnd
{
    /// A UIVideoComponent on the canvas.
    public sealed class FeVideo
    {
        public readonly FeNode node; public readonly FeImage image;
        public string path, doneEvent, textureAsset;
        public bool autoStart, loop, loadToMemory, muteAudio, loadInInitialize = true;
        public OriginalVideo player;
        bool wasActive, muting, failed;
        static readonly List<FeVideo> all = new List<FeVideo>();
        static readonly Dictionary<FeNode, FeVideo> byNode = new Dictionary<FeNode, FeVideo>();

        FeVideo(FeNode n, FeImage img) { node = n; image = img; }
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics() { all.Clear(); byNode.Clear(); }   // domain reload is off
        public static FeVideo Of(FeNode n) => n != null && byNode.TryGetValue(n, out var v) ? v : null;

        /// FeLayer.Build: the component's image (its render target once the video has a frame).
        public static FeVideo Attach(FeNode n, GameObject go, Material material)
        {
            var img = go.AddComponent<FeImage>(); img.raycastTarget = false; img.material = material; img.enabled = false;
            n.image = img;
            var v = new FeVideo(n, img)
            {
                path = n.P("Video"), doneEvent = n.P("DoneEvent"), textureAsset = n.P("TextureAsset") ?? n.P("VisibleMaterial"),
                autoStart = n.props.GetValueOrDefault("AutoStart") is bool a && a, loop = n.props.GetValueOrDefault("Loop") is bool l && l,
                loadToMemory = n.props.GetValueOrDefault("LoadToMemory") is bool m && m, muteAudio = n.props.GetValueOrDefault("MuteAudio") is bool mu && mu,
                loadInInitialize = !(n.props.GetValueOrDefault("LoadInInitialize") is bool li && !li),
            };
            all.Add(v); byNode[n] = v;
            return v;
        }

        /// ChangeVideoPath (PreTutorialPopup): a new video closes the old one (005cde50).
        public void ChangeVideoPath(string p) { if (p == path) return; Close(); path = p; }

        /// The TextureAsset's size: the render targets are named by it (1280_720_VideoRenderTarget, 512_256_VideoRenderTarget).
        Vector2Int TargetSize()
        {
            var m = System.Text.RegularExpressions.Regex.Match(textureAsset ?? "", @"^(\d+)_(\d+)_");
            return m.Success ? new Vector2Int(int.Parse(m.Groups[1].Value), int.Parse(m.Groups[2].Value)) : default;
        }

        /// 005cdf20: opens (BinkOpen) when needed, XenonBinkVideo::vf1, the UI-music mute with MuteAudio.
        public void Play(FrontEndManager fe)
        {
            if (string.IsNullOrEmpty(path)) return;
            if (!player)
            {
                player = OriginalVideo.Create(node.rt.gameObject, path, loadToMemory, loop, TargetSize());
                player.Done += () => OnDone(fe);
            }
            player.loop = loop; failed = false;
            player.SetVolume(Volume);
            player.Play();
            if (muteAudio && !muting) { muting = true; fe?.MuteUiMusic(true); }
        }
        /// 005ce030: XenonBinkVideo::vf3 (stop), the UI-music mute cleared.
        public void Stop(FrontEndManager fe)
        {
            if (player) player.Stop();
            if (muting) { muting = false; fe?.MuteUiMusic(false); }
        }
        /// 005cdfe0: stop and BinkClose.
        public void Close(FrontEndManager fe = null)
        {
            Stop(fe);
            if (player) { player.Close(); if (Application.isPlaying) UnityEngine.Object.Destroy(player); else UnityEngine.Object.DestroyImmediate(player); player = null; }
            image.enabled = false; image.texture = null;
        }
        public bool Playing => player && player.IsPlaying;

        static float Volume => Mathf.Clamp01(PlayerPrefs.GetFloat("ReflexAudio.master", 1f));   // EffectsVolume (0xd679f8)

        void OnDone(FrontEndManager fe)
        {
            if (muting) { muting = false; fe?.MuteUiMusic(false); }
            if (node.layer == null || !node.layer.active) return;   // vf11 runs on active components only (a skipped page's late end)
            node.layer.script?.Call("UIVideoComponent::VideoDone");
            if (!string.IsNullOrEmpty(doneEvent)) fe?.SendUIEvent(doneEvent);
        }

        /// Each front-end frame: activation / deactivation of the layers (vf3 / vf5) and the picture.
        public static void Tick(FrontEndManager fe)
        {
            foreach (var v in all.ToList())
            {
                if (!v.node.rt) { all.Remove(v); byNode.Remove(v.node); continue; }   // a front end of an earlier scene
                if (v.node.layer == null) continue;
                bool active = v.node.layer.active;
                if (active && !v.wasActive && v.autoStart) v.Play(fe);
                if (!active && v.wasActive) { if (v.autoStart) v.Close(fe); else v.Stop(fe); }
                v.wasActive = active;
                if (v.player && !Application.isPlaying) v.player.Pump();   // edit-mode benches step the front end themselves
                if (v.player && v.player.Failed && !v.failed) { v.failed = true; v.OnDone(fe); }   // no picture: the page goes on as after the last frame
                if (v.player && v.player.HasFrame && v.player.Texture)
                {
                    if (v.image.texture != v.player.Texture) { v.image.texture = v.player.Texture; v.image.Dirty(); }
                    if (!v.image.enabled) v.image.enabled = true;
                }
            }
        }

        /// A DoneEvent of a video of this layer.
        public static bool IsDoneEvent(FeLayer l, string evt) => all.Any(v => v.node.layer == l && v.doneEvent == evt);
    }

    /// The video layers' triggers and the startup sequence.
    public static class FeVideoFlow
    {
        public const string IntroGraph = "FrontEndIntroPageGraph.pag", IntroFirstPage = "Legal Screen Page", IntroLoadingPage = "Loading Screen Page";
        static readonly Dictionary<(FeLayer, object), float> elapsed = new Dictionary<(FeLayer, object), float>();
        static readonly HashSet<FeLayer> wasActive = new HashSet<FeLayer>();
        static string page; static float onPage;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void ResetStatics() { elapsed.Clear(); wasActive.Clear(); page = null; onPage = 0; }

        /// SendUIEvent: the active layers' UIEventTriggers for this event (an Event redirect, or the script function of a video's
        /// DoneEvent). True when one fired (the event is then not looked up as a page edge).
        public static bool OnUIEvent(FrontEndManager fe, IEnumerable<FeLayer> active, string evt)
        {
            bool hit = false;
            foreach (var l in active.ToList())
                foreach (var t in l.root.triggers ?? new List<object>())
                {
                    if (OriginalUi.S(t, "type") != "UIEventTrigger" || OriginalUi.S(t, "TriggerEventName") != evt) continue;
                    if (OriginalUi.S(t, "EventName") is string e) { hit = true; fe.SendUIEvent(e); }
                    else if (OriginalUi.S(t, "ScriptFunction") is string fn && FeVideo.IsDoneEvent(l, evt)) { hit = true; l.script?.Call(fn); }
                }
            return hit;
        }

        /// Each front-end frame (FrontEndManager.Step): the videos, the elapsed-time triggers of the video layers and the intro's
        /// loading page.
        public static void Tick(FrontEndManager fe, IEnumerable<FeLayer> active, float dt)
        {
            FeVideo.Tick(fe);
            var now = new HashSet<FeLayer>(active);
            foreach (var l in wasActive.Where(l => !now.Contains(l)).ToList()) foreach (var k in elapsed.Keys.Where(k => k.Item1 == l).ToList()) elapsed.Remove(k);
            wasActive.Clear(); wasActive.UnionWith(now);
            foreach (var l in now)
                foreach (var t in l.root.triggers ?? new List<object>())
                {
                    if (OriginalUi.S(t, "type") != "UIElapsedTimeTrigger") continue;
                    string ev = OriginalUi.S(t, "EventName"), fn = OriginalUi.S(t, "ScriptFunction");
                    if (ev == null && fn != "OnAttractModeTrigger") continue;   // the others: their layers' scripts own them
                    float ms = OriginalUi.F(t, "Time"); bool once = t is Dictionary<string, object> d && d.GetValueOrDefault("FireOnce") is bool b && b;
                    var key = (l, (object)t); elapsed.TryGetValue(key, out float e); if (e < 0) continue;
                    e += dt * 1000f;
                    if (e >= ms && ms > 0)
                    {
                        e = once ? -1 : e - ms;
                        elapsed[key] = e;
                        if (ev != null) fe.SendUIEvent(ev); else l.script?.Call(fn);
                    }
                    else elapsed[key] = e;
                }
            // Loading Screen Page: UIElapsedTimeTrigger 1000 ms -> InitialLoadLayer:CheckForAdvance (the front end is loaded) -> GotoTitlePage
            if (fe.CurrentPage != page) { page = fe.CurrentPage; onPage = 0; }
            onPage += dt;
            if (page == IntroLoadingPage && onPage >= 1f && !fe.Busy) { onPage = float.NegativeInfinity; fe.vars["UILoaded"] = true; fe.SendUIEvent("GotoTitlePage"); }
        }
    }

    // StudioVideoLayer.lua / StudioVideo2Layer.lua / IntroVideoLayer.lua: a click (MouseActivation_Container) skips to the next page; the
    // layers' own triggers do the rest (Accept / Back -> the DoneEvent or the next page's event).
    public sealed class VideoSkipScript : FeScript
    {
        readonly string clickEvent;
        public VideoSkipScript(string clickEvent) { this.clickEvent = clickEvent; }
        public override void Initialize() { functions["MouseActivation_Container"] = () => fe.SendUIEvent(clickEvent); }
    }

    // LegalScreen.lua: the Quazal line only in the demo.
    public sealed class LegalScreenScript : FeScript
    {
        public override void Initialize() { if (C("QuazalText") is FeNode q) q.Visible = false; }
    }

    // Attract.lua: the UI music muted while it plays; a click returns to the title (the layer's triggers: Accept / Start / Back and
    // IntroVideoDone -> "Title Page").
    public sealed class AttractScript : FeScript
    {
        public override void Initialize() { functions["MouseActivation_Container"] = () => fe.SendUIEvent("Title Page"); }
        public override void Start() => fe.MuteUiMusic(true);
        public override void End() => fe.MuteUiMusic(false);
    }

    // Credits.lua: a click travels back (the layer's triggers: Accept / Back -> TravelBack; the video's DoneEvent is TravelBack).
    public sealed class CreditsScript : FeScript
    {
        public override void Initialize() { functions["MouseActivation_Container"] = () => fe.SendUIEvent("TravelBack"); }
    }

    public static class FeVideoScripts
    {
        public static FeScript Create(string script)
        {
            switch (script)
            {
                case "StudioVideoLayer": return new VideoSkipScript("GotoStudioVideo2Page");
                case "StudioVideo2Layer": return new VideoSkipScript("GotoIntroVideoPage");
                case "IntroVideoLayer": return new VideoSkipScript("GotoLoadingScreenPage");
                case "LegalScreen": return new LegalScreenScript();
                case "Attract": return new AttractScript();
                case "Credits": return new CreditsScript();
                default: return null;
            }
        }
    }
}
