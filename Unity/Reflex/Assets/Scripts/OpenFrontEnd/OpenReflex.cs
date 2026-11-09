// OPEN REFLEX: the Open Unleashed-style front end for the Reflex port, beside the original 1:1 front end (FrontEndManager), and the
// switch between them.
//
// The switch (FRONT END: OPEN / ORIGINAL, PlayerPrefs "ReflexFrontEnd", default OPEN) is applied whenever the front-end scene
// (FrontEnd.unity: its FrontEndManager) loads: at the start and on every return from a race. Nothing in FrontEndManager is
// changed for it; the hook is SceneManager.sceneLoaded (plus AfterSceneLoad for the first scene), which runs after the scene's
// Awake and before FrontEndManager.Start (= Boot):
//   OPEN      the FrontEndManager is disabled before it boots and the shell (OpenShell) takes the screen. The original stays in
//             the scene: the shell hands CAREER (MOTOCAREER) and GARAGE to it (OriginalEmbed: FrontEndManager.ReturnPage = the
//             page, then enabled; OPTIONS are the client's own pages, Reflex.Port.Options.ReflexOptions), and comes back when the
//             original returns to its main menu. A race the original launched itself (a career event, a test drive, a tutorial)
//             comes back to the original the same way (FrontEndManager.ReturnPage set) and then to the shell.
//   ORIGINAL  the FrontEndManager runs as it always did; OriginalStyleKey adds one key to it: F10 (or L3 + R3 held for 1 s on a
//             pad) switches to OPEN at once.
//   both      OriginalOptionsGate holds the FrontEndManager (enabled = false) while the client's options pages are up over it.
// Headless (batch-mode) runs keep the scene's original front end, as the existing front-end tests expect, unless a test sets
// FrontEndSwitch.InBatchMode (or the command line says -reflexFrontEnd open|original; REFLEX_FRONTEND=open|original too).
using System;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;

namespace Reflex.Port.OpenFrontEnd
{
    public static class OpenReflex
    {
        /// The shell's name (a placeholder: rename it here; the wordmark, titles and window texts all use it).
        public const string Name = "OPEN REFLEX";
        /// The shell's own PlayerPrefs keys start with this.
        public const string Prefs = "OpenReflex.";
    }

    public enum FrontEndStyle { Open, Original }

    public static class FrontEndSwitch
    {
        /// The FRONT END setting: "OPEN" or "ORIGINAL" (missing: OPEN). The only place the key is named.
        public const string PrefsKey = "ReflexFrontEnd";
        public const string SceneName = "FrontEnd";
        /// Tests: apply the setting in batch mode too (headless runs keep the scene's original front end otherwise).
        public static bool InBatchMode;

        /// The saved choice.
        public static FrontEndStyle Saved
        {
            get
            {
                string v = ""; try { v = PlayerPrefs.GetString(PrefsKey, "OPEN"); } catch (Exception) { }
                return string.Equals(v, "ORIGINAL", StringComparison.OrdinalIgnoreCase) ? FrontEndStyle.Original : FrontEndStyle.Open;
            }
            set { PlayerPrefs.SetString(PrefsKey, value == FrontEndStyle.Original ? "ORIGINAL" : "OPEN"); PlayerPrefs.Save(); }
        }

        /// The command line / environment override (-reflexFrontEnd open|original, REFLEX_FRONTEND), null when none.
        public static FrontEndStyle? Override
        {
            get
            {
                string v = null;
                var a = Environment.GetCommandLineArgs();
                int i = Array.IndexOf(a, "-reflexFrontEnd"); if (i >= 0 && i + 1 < a.Length) v = a[i + 1];
                if (v == null) v = Environment.GetEnvironmentVariable("REFLEX_FRONTEND");
                if (string.Equals(v, "original", StringComparison.OrdinalIgnoreCase)) return FrontEndStyle.Original;
                if (string.Equals(v, "open", StringComparison.OrdinalIgnoreCase)) return FrontEndStyle.Open;
                return null;
            }
        }

        /// The style the next front-end load uses.
        public static FrontEndStyle Effective
        {
            get
            {
                if (Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex) return FrontEndStyle.Open;
                var o = Override; if (o.HasValue) return o.Value;
                if (Application.isBatchMode && !InBatchMode) return FrontEndStyle.Original;
                return Saved;
            }
        }

        /// Saves the choice; now: reloads the front-end scene so it takes effect at once (else at the next start / return to menu).
        public static void Set(FrontEndStyle style, bool now)
        {
            Saved = style;
            Debug.Log("[OpenReflex] front end: " + style + (now ? " (now)" : " (next start / return to menu)"));
            if (now) Reload();
        }

        /// The front-end scene again (the switch decides what it shows).
        public static void Reload()
        {
            if (Application.CanStreamedLevelBeLoaded(SceneName)) SceneManager.LoadScene(SceneName);
            else SceneManager.LoadScene(SceneManager.GetActiveScene().buildIndex);
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Init()
        {
            SceneManager.sceneLoaded -= OnSceneLoaded;
            SceneManager.sceneLoaded += OnSceneLoaded;
            OpenShell.ResetStatics();
        }

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void FirstScene() => Attach(SceneManager.GetActiveScene());

        static void OnSceneLoaded(Scene s, LoadSceneMode mode) => Attach(s);

        /// The scene's FrontEndManager, if it has one.
        public static FrontEndManager ManagerIn(Scene s)
        {
            if (!s.IsValid() || !s.isLoaded) return null;
            foreach (var go in s.GetRootGameObjects())
            {
                var fm = go.GetComponentInChildren<FrontEndManager>(true);
                if (fm) return fm;
            }
            return null;
        }

        static void Attach(Scene s)
        {
            var fm = ManagerIn(s);
            if (!fm || fm.GetComponent<FrontEndStyleApplied>()) return;
            fm.gameObject.AddComponent<FrontEndStyleApplied>();
            fm.gameObject.AddComponent<OriginalOptionsGate>();
            if (Effective == FrontEndStyle.Original) { fm.gameObject.AddComponent<OriginalStyleKey>(); return; }
            string back = FrontEndManager.ReturnPage;
            if (!Reflex.Port.RxCore.Bridge.OpenReflexSelector.UseOpenReflex && !string.IsNullOrEmpty(back) && back != "Main Menu Page" && back != "Event Select Page")
            {   // a race the original launched itself (career, garage test drive, challenges): back into the original, then the shell
                Debug.Log("[OpenReflex] back from the original's race: the original front end at " + back);
                OriginalEmbed.Attach(fm, OriginalEmbed.Purpose.Return);
                return;
            }
            if (OriginalIntro.Wanted())
            {   // the game's startup sequence (legal screen, publisher / studio logos, intro video, loading) runs in the original front end once, then the shell
                OriginalIntro.Played = true;
                Debug.Log("[OpenReflex] the original startup sequence, then the shell");
                OriginalEmbed.Attach(fm, OriginalEmbed.Purpose.Intro);
                return;
            }
            FrontEndManager.ReturnPage = null;
            fm.enabled = false;   // before its Start: the original front end never boots
            var go = new GameObject(OpenReflex.Name);
            go.AddComponent<OpenShell>().original = fm;
        }
    }

    /// The original's startup sequence before the shell, once per run of the game (FrontEndIntroPageGraph.pag: legal screen, Nordic /
    /// Rainbow / Double Helix logo videos, the intro video, the loading page, then its Title Page, FrontEndVideo.cs). OFF by default (the
    /// project lead: the build starts straight at the shell's hub, no intro videos and no title "start" page); REFLEX_INTRO=1 or
    /// OpenReflex.Intro = 1 turns it on. Tests and probes (any REFLEX_* test variable, batch mode) and REFLEX_SKIP_INTRO always skip it.
    public static class OriginalIntro
    {
        public const string PrefsKey = Prefs + "Intro";
        const string Prefs = OpenReflex.Prefs;
        /// Set when the sequence was started in this run.
        public static bool Played;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
        static void Reset() => Played = false;   // domain reload is off

        // variables a player's launcher sets (where things are, pacing), not a test
        static readonly string[] Plain = { "REFLEX_GAME_DIR", "REFLEX_REPO", "REFLEX_RXCORE_PATH", "REFLEX_OPEN_PLAYER", "REFLEX_SYMBOLS_DIR", "REFLEX_FFMPEG",
                                           "REFLEX_SAVE_DIR", "REFLEX_SIM_QOS", "REFLEX_CONTENT_WORKERS", "REFLEX_FPS_CAP" };

        public static bool Wanted()
        {
            if (Played || Application.isBatchMode) return false;
            if (Environment.GetEnvironmentVariable("REFLEX_INTRO") == "1") return true;
            if (!string.IsNullOrEmpty(Environment.GetEnvironmentVariable("REFLEX_SKIP_INTRO"))) return false;
            try { if (PlayerPrefs.GetInt(PrefsKey, 0) == 0) return false; } catch (Exception) { return false; }
            try
            {
                foreach (System.Collections.DictionaryEntry e in Environment.GetEnvironmentVariables())
                {
                    string k = e.Key as string ?? "";
                    if (k.StartsWith("REFLEX_", StringComparison.Ordinal) && Array.IndexOf(Plain, k) < 0) return false;   // a probe / test run
                    if (k.EndsWith("_SHOTS", StringComparison.Ordinal)) return false;
                }
            }
            catch (Exception) { }
            return true;
        }
    }

    /// Marks a front-end scene whose style was applied (once per load).
    public sealed class FrontEndStyleApplied : MonoBehaviour { }

    /// While the client's options pages (Reflex.Port.Options.ReflexOptions) are up, the original front end under them is held (its
    /// Update, so its input polling, stops: FrontEndManager reads the keyboard and pad itself), and let go the frame after they close.
    /// Only a manager this gate stopped is started again (the OPEN style keeps it off on purpose).
    public sealed class OriginalOptionsGate : MonoBehaviour
    {
        FrontEndManager fm; bool held;
        void Awake() => fm = GetComponent<FrontEndManager>();
        void LateUpdate()
        {
            if (!fm) return;
            bool up = Options.ReflexOptions.IsOpen || Options.ReflexOptions.ClosedFrame >= Time.frameCount - 1;
            if (up && fm.enabled && !held) { held = true; fm.enabled = false; }
            else if (!up && held) { held = false; fm.enabled = true; }
        }
        /// Holding it now (tests).
        public bool Holding => held;
    }

    /// ORIGINAL style: one added key on the original front end. F10 (pad: L3 + R3 held for a second) switches to OPEN at once.
    public sealed class OriginalStyleKey : MonoBehaviour
    {
        FrontEndManager fm; float held;
        void Awake() => fm = GetComponent<FrontEndManager>();
        void Update()
        {
            if (!fm || fm.Busy) { held = 0; return; }
            var kb = Keyboard.current; var gp = Gamepad.current;
            bool key = kb != null && !kb.ctrlKey.isPressed && kb.f10Key.wasPressedThisFrame;
            held = gp != null && gp.leftStickButton.isPressed && gp.rightStickButton.isPressed ? held + Time.unscaledDeltaTime : 0;
            if (key || held > 1f) { held = -100; FrontEndSwitch.Set(FrontEndStyle.Open, true); }
        }
    }

    /// The original front end running for the shell (CAREER, GARAGE, OPTIONS) or after a race it launched: back to the shell when it
    /// reaches its main menu (or its title) with nothing more to do.
    public sealed class OriginalEmbed : MonoBehaviour
    {
        public enum Purpose { Career, Garage, Options, Return, Intro }
        public Purpose purpose;
        FrontEndManager fm;
        bool titleAccepted, careerSent, garageSent, away;
        float since; string lastPage;
        /// The page the original was entered at (tests).
        public string EnteredAt { get; private set; }

        /// Enters the original front end at page (null: its start page) for purpose; the shell must be gone already.
        public static OriginalEmbed Attach(FrontEndManager fm, Purpose purpose, string page = null)
        {
            var e = fm.GetComponent<OriginalEmbed>() ?? fm.gameObject.AddComponent<OriginalEmbed>();
            e.fm = fm; e.purpose = purpose; e.EnteredAt = page ?? FrontEndManager.ReturnPage;
            if (page != null) FrontEndManager.ReturnPage = page;
            fm.enabled = true;   // its Start boots it at ReturnPage (Main Menu Page in the history), or the title
            Debug.Log("[OpenReflex] the original front end for " + purpose + " at " + (page ?? FrontEndManager.ReturnPage ?? "its start page"));
            return e;
        }

        static bool SignedIn => ReflexProfile.Current != null && ReflexProfile.Current.signedIn;

        void Update()
        {
            if (!fm || fm.assets == null || fm.Busy) return;
            string page = fm.CurrentPage;
            if (page != lastPage) { lastPage = page; since = 0; } else since += Time.unscaledDeltaTime;
            if (since < 0.4f || page == null) return;
            if (page == "Title Page")
            {
                if (purpose == Purpose.Intro) { Back(); return; }   // the startup sequence is over (the title is the shell's hub)
                // not signed in: the original's own sign-in / MOTOCARD creation (the title's Accept: TitleLoadGame), once
                if (!titleAccepted && fm.TopPopup == null) { titleAccepted = true; fm.DispatchKey("Accept"); return; }
                if (away) Back();
                return;
            }
            if (page != "Main Menu Page") { away = true; return; }
            if (fm.TopPopup != null) return;
            if (purpose == Purpose.Career && !careerSent)
            {   // MainMenu:CareerMode (FrontEndScripts.MainMenuScript): no profile -> the profile required popup
                careerSent = true;
                if (!SignedIn) { fm.SendUIEvent("ShowProfileRequiredPopup"); return; }
                fm.vars["PlayerMode"] = "SinglePlayer"; fm.vars["InCareerMode"] = true;
                fm.SendUIEvent(ReflexProfile.Current.EventStatus("CareerWelcome") != 0 ? "CareerModeMenuPage" : "CareerWelcomePage");
                fm.PlaySound("Accept");
                return;
            }
            if (purpose == Purpose.Garage && !garageSent)
            {   // signed in through the title's flow just now: the MOTOCARD page (its GARAGE), as the main menu's MOTOCARD item
                garageSent = true;
                if (SignedIn && EnteredAt == null) { fm.SendUIEvent("GotoMotocardPage"); fm.PlaySound("Accept"); return; }
            }
            Back();
        }

        void Back()
        {
            enabled = false;
            Debug.Log("[OpenReflex] the original front end is back at its " + fm.CurrentPage + ": the shell again");
            FrontEndManager.ReturnPage = null;
            FrontEndSwitch.Reload();
        }
    }
}
