// The Free Ride challenges' rules popup in the open product: the original's PreChallengePopup.
//
// In the original the challenge activity (MXActivityChallenge::vf2 0057ab00) raises the UI event "ShowPreChallengePopup" as it is
// created; the page (Shared.ENG PreChallengePopup.lyr + PreChallengePopup.lua) is the install's own layer:
//   Start:    App.PlayUISound( "PopupShow" ); Populate; App.SetUIVariableInt( "WaitForUI", 1 ) ("activities are not allowed to start");
//             the mouse cursor on;
//   Populate: TitleText = STR_<ACTIVITY>_TITLE, MessageText = STR_<ACTIVITY>_DESCRIPTION (HillClimb, Trailblazer, HitDiscovery,
//             TargetPractice), the Gold / Silver / Bronze Medal Text = the level's criteria (App.GetChallengeAwardCriteria) formatted
//             by the activity's function (App.FormatDistance, App.FormatTime, ChallengeFormatDiscoveries / Targets), and "Record Text" =
//             EMB_CHALLENGE_RECORD + the profile's record of the location (PlayerChallenges Param) when the player has one, else hidden;
//   accept:   OnAcceptKeypress (Enter, pad A, the mouse on the button): App.PlayUISound( "Accept" ), HidePreChallengePopup;
//   back:     OnBackKeypress does nothing (the popup cannot be dismissed);
//   End:      WaitForUI 0 (the activity's Pre update, Activity::vf25 00558ce0, then starts the countdown), the cursor off.
// The open product has no UI manager: the core holds the UI property for the page (rx_session_wait_for_ui, set before the load of a
// challenge world: OpenReflexSimulation) and the page releases it where End sets it to 0 (OriginalWorldRuntime.ReleasePreChallenge). The
// numbers come from the core (rx_session_challenge in Pre: the criteria of the level's MXChallengesSpecEntity and the profile's row of
// the location); nothing is recomputed here. After RESTART (the end page's or the pause page's) the activity re-enters Pre without the
// popup, as in the original (vf2 runs once, at the activity's creation).
// Presentation (modern, UI Toolkit; the strings, numbers and behaviour above are the original's): a dialog over a dimmed world with the
// challenge's title, its description, the three medal criteria (GOLD / SILVER / BRONZE) as tiles, the record and the CONTINUE button; the
// button's key cap shows the device in use (OuGlyphs; OwnKeyGlyphs.ForCaps keeps the glyph record the tests read).
using System;
using System.Collections.Generic;
using System.Globalization;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class OwnPreChallenge : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        ReflexEvents.Event ev;
        GameObject host; VisualElement root, layer, button;
        Label titleText, messageText, recordText, buttonText;
        readonly Label[] medalText = new Label[3];   // gold, silver, bronze
        OwnKeyGlyphs glyphs;
        bool built, failed, open, armed, accepted, cursorLease;
        int openedFrame = -1, shownCount;
        float hideTimer = -1;
        RxChallengeState shown;

        /// The popup is up and waits for the player (the core holds the challenge in Pre; the world takes no input).
        public bool Active => open;
        /// The popup was accepted and the challenge released (this world).
        public bool Accepted => accepted;
        public int ShownCount => shownCount;
        /// The core's snapshot the popup was opened with (Pre: state 0).
        public RxChallengeState Shown => shown;
        public string Title => titleText?.text;
        public string Message => messageText?.text;
        public string Button => buttonText?.text;
        /// The three criteria lines (Gold, Silver, Bronze) and the record line (null when hidden).
        public string[] Criteria => new[] { medalText[0]?.text, medalText[1]?.text, medalText[2]?.text };
        public string Record => recordText != null && recordText.style.display != DisplayStyle.None ? recordText.text : null;
        /// The popup is on screen (built, shown, painted).
        public bool Rendered => open && built && root != null && root.panel != null && layer.resolvedStyle.display != DisplayStyle.None && layer.worldBound.width > 0;
        /// The KeyMap glyphs of the page (tests).
        public OwnKeyGlyphs Glyphs => glyphs;

        public static OwnPreChallenge Attach(OriginalWorldRuntime rt, ReflexEvents.Event e)
        {
            if (!rt || e == null) return null;
            var p = rt.GetComponent<OwnPreChallenge>() ?? rt.gameObject.AddComponent<OwnPreChallenge>();
            p.Close(); p.runtime = rt; p.ev = e; p.accepted = false; p.shownCount = 0; p.shown = default; p.hideTimer = -1;
            rt.preChallenge = p;
            return p;
        }

        // ------------------------------------------------------------------------------------------------------ input
        static bool AcceptDown()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.enterKey.isPressed || kb.numpadEnterKey.isPressed || kb.spaceKey.isPressed)) return true;
            if (Mouse.current?.leftButton.isPressed == true) return true;
            foreach (var pad in Gamepad.all) if (pad.buttonSouth.isPressed) return true;
            return false;
        }
        static bool AcceptPressed()
        {
            var kb = Keyboard.current;
            if (kb != null && (kb.enterKey.wasPressedThisFrame || kb.numpadEnterKey.wasPressedThisFrame || kb.spaceKey.wasPressedThisFrame)) return true;
            foreach (var pad in Gamepad.all) if (pad.buttonSouth.wasPressedThisFrame) return true;
            return false;
        }

        // ------------------------------------------------------------------------------------------------------ the page
        void Update()
        {
            if (!runtime || ev == null) return;
            if (hideTimer >= 0)
            {   // PopupHide plays out after the accept
                hideTimer -= Time.unscaledDeltaTime;
                if (hideTimer < 0) { hideTimer = -1; if (root != null) root.style.display = DisplayStyle.None; }
                return;
            }
            if (open && !runtime.Running) { Close(); return; }
            var ch = runtime.Challenge;
            bool pre = runtime.HasChallenge && ch.valid != 0 && ch.state == 0;
            if (!open)
            {
                if (!accepted && runtime.PreChallengeHeld && pre && runtime.Running && !runtime.paused) Open(ch);
                return;
            }
            if (!pre) { Close(); return; }   // the activity left Pre by another path
            glyphs?.Refresh();
            if (Time.frameCount <= openedFrame) return;
            if (!armed) { if (!AcceptDown()) armed = true; return; }   // what was held when the page appeared does not choose
            if (AcceptPressed()) Accept();   // (a click on the button arrives through its UI Toolkit item)
            // OnBackKeypress: disabled
        }

        void Open(RxChallengeState ch)
        {
            Build();
            if (!built) { Release(); return; }   // no page to wait for: the challenge starts
            shown = ch; open = true; armed = false; openedFrame = Time.frameCount; shownCount++;
            Populate(ch);
            root.style.display = DisplayStyle.Flex;
            OuMotion.Sync(root);
            layer.RemoveFromClassList("ou-in"); OuMotion.Fade(layer);
            button.schedule.Execute(() => { if (open && button.panel != null) button.Focus(); });
            OriginalHud.UiSound(runtime, "play", "PopupShow");           // PreChallengePopup:Start
            glyphs?.Refresh(true);
            cursorLease = true; Reflex.Port.Platform.ReflexCursor.Want(this, true);   // App.ShowMouseCursor( true ): shown for a mouse player only
            Debug.Log("[PreChallenge] " + ev.key + ": kind " + ch.kind + " criteria " + ch.criteria0.ToString("F1", CultureInfo.InvariantCulture) + " / " + ch.criteria1.ToString("F1", CultureInfo.InvariantCulture)
                + " / " + ch.criteria2.ToString("F1", CultureInfo.InvariantCulture) + " record " + (ch.recordValid != 0 ? ch.record.ToString("F2", CultureInfo.InvariantCulture) : "none") + " (WaitForUI held)");
        }

        void Close()
        {
            open = false; armed = false;
            if (root != null) root.style.display = DisplayStyle.None;
            if (cursorLease) { cursorLease = false; Reflex.Port.Platform.ReflexCursor.Want(this, false); }   // App.ShowMouseCursor( false )
        }

        // PreChallengePopup:OnAcceptKeypress: the sound, HidePreChallengePopup (PopupHide); End releases WaitForUI.
        void Accept()
        {
            if (!open || !armed || Time.frameCount <= openedFrame) return;
            OriginalHud.UiSound(runtime, "play", "Accept");
            Debug.Log("[PreChallenge] accepted " + ev.key);
            accepted = true;
            Release();
            open = false; armed = false;
            if (cursorLease) { cursorLease = false; Reflex.Port.Platform.ReflexCursor.Want(this, false); }
            layer.RemoveFromClassList("ou-in");   // PopupHide: the dialog fades out
            hideTimer = OuMotion.Reduced ? 0.01f : 0.3f;
        }

        void Release()
        {
            accepted = true;
            if (!runtime.ReleasePreChallenge(r => { if (r != 0) Debug.LogWarning("[PreChallenge] WaitForUI was not released (" + r + ")"); }))
                Debug.LogWarning("[PreChallenge] no WaitForUI hold to release");
        }

        // ------------------------------------------------------------------------------------------------------ the page
        void Build()
        {
            if (built || failed) return;
            try
            {
                OuUi.EnsureEventSystem();
                host = new GameObject("Open Reflex PreChallengePopup"); host.transform.SetParent(transform, false);
                root = OuUi.CreateDocument(host, 72); OuTheme.Apply(root);
                root.style.display = DisplayStyle.None;
                layer = OuUi.Div("mxa-layer ou-dialog-layer", root);
                var card = OuWidgets.Dialog(layer);
                titleText = OuUi.Text("", "ou-dialog-title ou-display", card); titleText.enableRichText = false;
                messageText = OuUi.Text("", "ou-dialog-lead", card); messageText.enableRichText = false;
                var criteria = OuUi.Div("ou-criteria", card);
                string[] kinds = { "gold", "silver", "bronze" }, names = { "STR_GOLD", "STR_SILVER", "STR_BRONZE" }, defaults = { "Gold", "Silver", "Bronze" };
                for (int i = 0; i < 3; i++)
                {
                    var c = OuUi.Div("ou-criterion ou-criterion-" + kinds[i], criteria);
                    OuUi.Text(Str(names[i], defaults[i]).ToUpperInvariant(), "ou-criterion-label ou-label", c).enableRichText = false;
                    medalText[i] = OuUi.Text("", "ou-criterion-value ou-display", c); medalText[i].enableRichText = false;
                }
                recordText = OuUi.Text("", "ou-eyebrow2 ou-label", card); recordText.enableRichText = false; recordText.style.marginBottom = 18;
                var row = OuUi.Div("ou-button-row", card);
                button = OuUi.Button("", "mxa-main-button ou-primary", Accept, row); button.name = "prechallenge-accept";
                buttonText = button.Q<Label>(); buttonText.enableRichText = false;
                var cap = OuUi.Text("", "mxa-key", button); cap.style.marginLeft = 16;
                glyphs = OwnKeyGlyphs.ForCaps(); glyphs.AddCap("Accept", cap);
                built = true;
            }
            catch (Exception e) { failed = true; Debug.LogWarning("[PreChallenge] the popup was not built: " + e.Message); if (host) Destroy(host); }
        }

        // PreChallengePopup:Start / Populate
        void Populate(RxChallengeState ch)
        {
            titleText.text = OwnPostChallenge.ChallengeTitle(ch.kind);
            messageText.text = Str(DescriptionKey(ch.kind), "");
            buttonText.text = Str("STR_CONTINUE", "CONTINUE");
            medalText[0].text = OwnPostChallenge.Format(ch.kind, ch.criteria2);
            medalText[1].text = OwnPostChallenge.Format(ch.kind, ch.criteria1);
            medalText[2].text = OwnPostChallenge.Format(ch.kind, ch.criteria0);
            bool record = ch.recordValid != 0;
            recordText.style.display = record ? DisplayStyle.Flex : DisplayStyle.None;                // no record: the string is hidden
            if (record) recordText.text = OwnPostChallenge.Embedded("EMB_CHALLENGE_RECORD", "Your record: ").TrimEnd() + " " + OwnPostChallenge.Format(ch.kind, ch.record);
        }

        // m_ChallengeStrings[ activityType ][2]
        static string DescriptionKey(int kind) =>
            kind == 1 ? "STR_HILLCLIMB_DESCRIPTION" : kind == 6 ? "STR_TRAILBLAZER_DESCRIPTION" : kind == 7 ? "STR_HITDISCOVERY_DESCRIPTION" : kind == 8 ? "STR_TARGETPRACTICE_DESCRIPTION" : "STR_EMPTY";

        static string Str(string key, string def) => OwnPostChallenge.Str(key, def);

        void OnDisable() => Close();
        void OnDestroy() { Close(); if (host) Destroy(host); if (runtime && runtime.preChallenge == this) runtime.preChallenge = null; }
    }
}
