using System;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.UI;

namespace Reflex.Port.OriginalWorld
{
    // The MotoSkills tutorial world (WP-T, _re/notes/tutorials.md; native tutorial.{h,cpp}): the front end's UI world (UI_Compound) as
    // the boot left it on the Freeride Single Player Page of the tutorial (ReflexTutorials). Everything the player sees and does there is
    // the game's own, live (OriginalLiveUi): TutorialIntroductionPopup (its narration TUTORIAL_INTRO/VO/INTRO), the Tutorial Pause Menu
    // Page (TutorialPauseMenu.lua: the lessons unlocked by PlayerRider.TutorialsCompleted + 1, its locks, QUIT MOTOSKILLS; Esc there and
    // during a lesson: MXUIManager_OnPauseRequest's GotoTutorialPauseMenuPage), PreTutorialPopup (the lesson's title, DYN_TUTORIAL_DESCRIPTION,
    // its Tutorial<n>.bik), the lesson itself (the TutorialManager's check per lesson in the UI world's FreeRide: lean, hold tight, the turn,
    // the jump, the wreck save, the tricks; the TutorialActivation HUD's label / hints; its narration through TutorialAudioMappings),
    // PostTutorialPopup / FailedTutorialPopup (RETRY: the manager's RetryTutorial, QUIT: the pause menu), TutorialConclusionPopup after the
    // sixth (TUTORIAL_OUTRO/VO/OUTRO). Progress is the game's: TutorialsCompleted (004e8460) saved by the page Lua's App.RequestSaveGame
    // through the saved-game worker's pass ("frame" below) into the player's own savegame.bxml, the achievements TUTORIAL_01 / 02 by
    // MXHighlightTracking. This component does what the world cannot by itself:
    //   * the hooks, once, before the live mirror shows anything ("attach", instead of the live UI's in-game route: the UI is in game);
    //   * the saved-game worker's pass each frame ("frame": a requested save is written after the worker's 1000 ms, the Save Popup goes);
    //   * PreTutorialPopup's "Tutorial Video": the world opens no Bink video (BinkOpen 0, as the boot); its UIVideoComponent's path (+0x270,
    //     ChangeVideoPath "Videos\\Tutorial<n>") is played here through the install's binkw32.dll (OriginalVideo) into the component's box
    //     of the mirrored layer (its 512_256_VideoRenderTarget);
    //   * the UI music: the Tutorial Load Page does not stop it (TutorialLoadingPage.lua has no Start), so a UI theme plays on (FeUiMusic);
    //   * leaving: QUIT MOTOSKILLS (MXUIManager::SwitchToMainMenu) and the conclusion popup (GotoFrontEndMainMenuPage) take the game back to
    //     its Main Menu Page in this same world (the UI world is the front end's own): the port goes back to its front end's Main Menu Page
    //     (the world's saves: ReflexPersistence.Leave when the world goes).
    [DisallowMultipleComponent]
    public sealed class OriginalTutorial : MonoBehaviour
    {
        [System.Runtime.InteropServices.UnmanagedFunctionPointer(System.Runtime.InteropServices.CallingConvention.Cdecl)] delegate int reflex_world_tutorial__fn(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string command, byte[] output, int size);
        static readonly Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_tutorial__fn> reflex_world_tutorial__p = new Reflex.Port.Platform.ReflexNativeLoader.Fn<reflex_world_tutorial__fn>("reflex_world_tutorial");
        static int reflex_world_tutorial(IntPtr world, [MarshalAs(UnmanagedType.LPUTF8Str)] string command, byte[] output, int size) => reflex_world_tutorial__p.Get()(world, command, output, size);

        /// A tutorial command on the world (between its frames); null on error / an older plugin.
        public static string Command(IntPtr world, string command)
        {
            if (world == IntPtr.Zero || missing) return null;
            try
            {
                var b = new byte[1 << 14]; int n = reflex_world_tutorial(world, command, b, b.Length);
                if (n < -1) { b = new byte[-n + 16]; n = reflex_world_tutorial(world, command, b, b.Length); }
                if (n < 0) { Debug.LogWarning("[Tutorial] " + command + ": " + ReflexWorldNative.Error(world, "failed")); return null; }
                return Encoding.UTF8.GetString(b, 0, n);
            }
            catch (EntryPointNotFoundException) { missing = true; Debug.LogWarning("[Tutorial] this libreflexworld has no reflex_world_tutorial: the tutorial world cannot run"); return null; }
        }
        static bool missing;

        /// The race scene's world is the tutorial's (ReflexEventSelect -> OriginalLiveUi.Attach).
        public static OriginalTutorial Attach(OriginalWorldRuntime runtime, OriginalLiveUi live)
        {
            if (!runtime || !live) return null;
            var t = runtime.GetComponent<OriginalTutorial>() ?? runtime.gameObject.AddComponent<OriginalTutorial>();
            t.runtime = runtime; t.live = live;
            live.activate = w => { string r = Command(w, "attach"); lock (t.gate) t.attachAnswer = r; Debug.Log("[Tutorial] the tutorial world attached: " + r); return r != null ? "Tutorial" : null; };
            return t;
        }

        OriginalWorldRuntime runtime; OriginalLiveUi live;
        readonly object gate = new object();
        string attachAnswer; bool asking; int saves;
        public string AttachAnswer { get { lock (gate) return attachAnswer; } }
        /// Saves the world's saved-game worker pass wrote (each "Saved").
        public int Saves { get { lock (gate) return saves; } }
        public static OriginalTutorial Active { get; private set; }
        public bool Leaving => leaving;
        bool leaving;

        void Awake() { Active = this; }
        void OnDestroy() { if (Active == this) Active = null; CloseVideo(); music?.SetEnable(false); }

        FeUiMusic music;
        void Start()
        {
            // the front end's UI music goes on (TutorialLoadingPage.lua has no SetEnableUIMusic( FALSE )): a theme as App.SetEnableUIMusic draws it
            try { music = new FeUiMusic(this); music.SetEnable(true); } catch (Exception e) { Debug.LogWarning("[Tutorial] UI music: " + e.Message); }
        }

        void Update()
        {
            music?.Tick(Time.unscaledDeltaTime);
            if (!runtime || !runtime.Running || runtime.Simulation == null || !live || leaving) return;
            // the saved-game worker's pass ("frame"), once per frame between the world's frames
            bool ask; lock (gate) { ask = !asking; if (ask) asking = true; }
            if (ask) runtime.Simulation.BetweenFrames(w =>
            {
                string r = Command(w, "frame");
                int n = r == null ? 0 : r.Split('\n').Count(l => l == "Saved");
                lock (gate) { saves += n; asking = false; }
                if (n > 0) Debug.Log("[Tutorial] the game's save written (" + ReflexProfile.SaveFile + ")");
            });
            Objects();
            var f = live.Last;
            if (f == null) return;
            Video(f);
            // the game leaves the tutorial: its own Main Menu (Loading) Page in this world
            if (f.layers.Any(l => (l.page == "Main Menu Page" || l.page == "Main Menu Loading Page") && (l.state & 0x10) != 0))
            {
                leaving = true;
                Debug.Log("[Tutorial] the game leaves the tutorial (its Main Menu Page): back to the front end");
                FrontEndManager.ReturnPage = "Main Menu Page";
                CloseVideo();
                OriginalPauseMenu.QuitToFrontEnd();
            }
        }

        // ---------------------------------------------------------------------------------------------- the lessons' scene objects
        // TUT0<n>_* of UI_CompoundFreeRide (road flares, chevron walls, the trajectory arrows of lessons 3 and 4): the TutorialManager shows
        // only the running lesson's (TutorialManager_ShowLessonObjects 00751c40: +0xa2); the level's instances are shown as the game has them.
        float objectsAsked = -1; volatile string objectLines; readonly System.Collections.Generic.Dictionary<int, bool> objectShown = new System.Collections.Generic.Dictionary<int, bool>();
        /// The lessons' objects shown now (entity ids), tests.
        public System.Collections.Generic.IEnumerable<int> ObjectsShown => objectShown.Where(kv => kv.Value).Select(kv => kv.Key);
        public int ObjectsKnown => objectShown.Count;
        void Objects()
        {
            if (Time.unscaledTime - objectsAsked > 0.25f)
            {
                objectsAsked = Time.unscaledTime;
                runtime.Simulation.BetweenFrames(w => { string r = Command(w, "objects"); if (r != null) objectLines = r; });
            }
            string lines = objectLines; if (lines == null) return; objectLines = null;
            var level = FindAnyObjectByType<OriginalLevel>();
            if (!level || level.Content == null) return;
            foreach (var line in lines.Split('\n'))
            {
                var p = line.Split(' '); if (p.Length < 3 || !int.TryParse(p[0], out int id)) continue;
                bool shown = p[1] != "0";
                if (objectShown.TryGetValue(id, out bool was) && was == shown) continue;
                objectShown[id] = shown;
                foreach (var inst in level.Content.instances) if (inst.entityId == id && inst.transform) inst.transform.gameObject.SetActive(shown);
            }
        }

        // ---------------------------------------------------------------------------------------------- PreTutorialPopup's video
        OriginalVideo player; RawImage picture; string playing; uint videoObject; bool asked;
        /// The video playing now ("Videos\\Tutorial1"), null when none.
        public string PlayingVideo => playing;
        public OriginalVideo Player => player;
        void Video(OriginalLiveUi.Frame f)
        {
            var pop = f.layers.FirstOrDefault(l => l.page == "PreTutorialPopup" && (l.state & 0x10) != 0);
            var node = pop?.nodes.FirstOrDefault(n => n.name == "Tutorial Video" && (n.state & 0x10) != 0);
            if (node == null) { CloseVideo(); return; }
            if (node.obj != videoObject && !asked)
            {
                // the component's path (+0x270, 0x30 chars: UIVideoComponent_ChangeVideoPath 005cde50), between frames
                asked = true; uint obj = node.obj;
                runtime.Simulation.BetweenFrames(w =>
                {
                    var b = new byte[0x30]; string path = null;
                    if (ReflexWorldNative.reflex_world_read(w, obj + 0x270, b, (uint)b.Length) == 0) { int z = Array.IndexOf(b, (byte)0); path = Encoding.ASCII.GetString(b, 0, z < 0 ? b.Length : z); }
                    lock (gate) { pendingPath = path; pendingObject = obj; }
                });
            }
            string p; uint o; lock (gate) { p = pendingPath; o = pendingObject; pendingPath = null; }
            if (p != null) { asked = false; videoObject = o; if (p != playing) StartVideo(pop.name, p); }
            if (player && picture && player.HasFrame && player.Texture && picture.texture != player.Texture) { picture.texture = player.Texture; picture.enabled = true; }
        }
        string pendingPath; uint pendingObject;

        void StartVideo(string layer, string path)
        {
            CloseVideo();
            var built = live.BuiltNode(layer, "Tutorial Video");
            if (built == null || !built.rect) { Debug.LogWarning("[Tutorial] PreTutorialPopup's Tutorial Video is not built"); return; }
            bool loop = built.json != null && built.json.TryGetValue("Loop", out var lv) && lv is bool lb && lb;
            var go = new GameObject("Tutorial Video (Bink)", typeof(RectTransform)); go.transform.SetParent(built.rect, false);
            var rt = (RectTransform)go.transform; rt.anchorMin = Vector2.zero; rt.anchorMax = Vector2.one; rt.offsetMin = rt.offsetMax = Vector2.zero;
            picture = go.AddComponent<RawImage>(); picture.raycastTarget = false; picture.enabled = false;
            // the 512_256_VideoRenderTarget (PreTutorialPopup:Start SetMaterial / ChangeVideoTexture): the 512x256 video fills it
            player = OriginalVideo.Create(go, path, false, loop, new Vector2Int(512, 256));
            player.SetVolume(Mathf.Clamp01(PlayerPrefs.GetFloat("ReflexAudio.master", 1f)));
            player.Play();
            playing = path;
            Debug.Log("[Tutorial] PreTutorialPopup video " + path + (loop ? " (loop)" : ""));
        }
        void CloseVideo()
        {
            if (player) { player.Close(); Destroy(player); }
            if (picture) Destroy(picture.gameObject);
            player = null; picture = null; playing = null; videoObject = 0;
        }

        // ---------------------------------------------------------------------------------------------- tests
        /// The world's tutorial state (tutorial "state" JSON) and the player's progress ("progress": completed, entered), between frames.
        public void Ask(Action<string, string> answer)
        {
            var sim = runtime ? runtime.Simulation : null;
            if (sim == null) { answer(null, null); return; }
            sim.BetweenFrames(w => answer(Command(w, "state"), Command(w, "progress")));
        }
    }
}
