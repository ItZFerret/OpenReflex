using Reflex.Port.OriginalWorld;
using Reflex.Port.Platform;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    // Recovery UI remains independent of the failed native session and imported page graph.
    public sealed class OpenSessionError : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        VisualElement root;
        OuMenuInput input;
        OuNav nav;
        public bool Shown => root != null;

        void Awake() => runtime = GetComponent<OriginalWorldRuntime>();
        void Update()
        {
            if (!runtime || string.IsNullOrEmpty(runtime.FailureMessage)) return;
            if (OpenLoadArena.Riding && GetComponent<OwnLoadArena>()) return;   // (the arena's ride: its card says it, the event still loads and START enters it)
            if (root == null) Show();
            nav.Update(input);
        }

        void Show()
        {
            OuUi.EnsureEventSystem();
            root = OuUi.CreateDocument(gameObject, 110);
            OuTheme.Apply(root);
            var layer = OuUi.Div("mxa-layer mxa-screen-install mxa-dim", root);
            OuTheme.Header(layer, "EVENT STOPPED", OpenReflex.Name);
            var panel = OuUi.Div("mxa-panel mxa-install", layer);
            OuUi.Text("This event could not continue.", "mxa-screen-title", panel);
            // (w-fb-customtrack) what stopped it: a custom track (Mods/Tracks) names itself and the file it could not read
            string why = (runtime.FailureMessage ?? "").Split('\n')[0].Trim();
            if (why.StartsWith("STOPPED: ")) why = why.Substring(9);
            var track = Content.TrackMods.Bound;
            if (track != null)
                OuUi.Text("The custom track \u201c" + track.name + "\u201d (Mods/Tracks/" + track.id + ", " + track.level + ") did not load: its files hold something this version cannot read."
                          + " Check that the download is complete (extract it again, or download it again), or try another track.", "mxa-install-text", panel);
            if (why.Length > 0) OuUi.Text(why.Length > 300 ? why.Substring(0, 300) + "\u2026" : why, "mxa-note", panel);
            OuUi.Text("Return to the menu to choose another event. A support report includes this session's error and logs so we can fix it.", "mxa-install-text", panel);
            var status = OuUi.Text("", "mxa-note", panel);
            var back = OuUi.Button("RETURN TO MENU", "mxa-main-button", FrontEndSwitch.Reload, panel);
            back.name = "event-error-back";
            OuUi.Button("SAVE SUPPORT REPORT", "mxa-main-button", () => status.text = ReflexSupport.CreateReportForPlayer(), panel).name = "event-error-report";
            input = new OuMenuInput();
            nav = new OuNav { Screen = layer, DefaultItem = () => back, OnCancel = () => { FrontEndSwitch.Reload(); return true; } };
            nav.Focus(back);
            Reflex.Port.Platform.ReflexCursor.Want(this, true);   // (released when this page goes: ReflexCursor drops destroyed owners)
        }

        void OnDestroy() => input?.Dispose();
    }
}
