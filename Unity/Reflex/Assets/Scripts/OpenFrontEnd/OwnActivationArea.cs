// The Free Ride activation areas in the open product (core/game/race_activation.c; native snapshot rx_session_activation).
//
// The Race & Ride challenges Free Ride (ReflexModes.InChallengesFreeRide: InCareerMode set) has "StartActivity" triggers, one per
// challenge. Riding into one, the game's own handler (MXUIManager_OnTriggerEnter 005a3b60) marks the area in the local MXPlayer
// (+0xa0 / +0xa4 the activity / +0x1a8 the track); leaving it (005a3d00) clears it. The original then shows the AAA notification
// (InGame's SinglePlayer_AAA_Activation.lyr, AAANotification.lua: NotificationContainer while EventTypeActivation is not FreeRide,
// HelpContainer with the required vehicle class while ChallengeRequiredVehicleClass >= 0) and its pause handler
// (MXUIManager_OnPauseRequest 00598e40) starts the area's challenge (EventTypeActivation = the activity, TrackId = the track) through
// the AAA loading page instead of opening the pause page. This component does the same from the copied state:
//   * the notification: the install's SinglePlayer_AAA_Activation layer is the model (a NotificationContainer with STR_CHALLENGE_ACTIVATION and
//     the Start key, a HelpContainer with STR_CHALLENGE_VEHICLE_REQUIREMENT and the class's name), drawn in the career menu's original-inspired
//     style (UI Toolkit: OuCareerKit / OuCareer.uss: an orange title bar, dark glass, the dotted patch, the key cap of the device in use, the
//     class's vehicle silhouettes), its two containers shown from the snapshot; AAANotification:ToggleNotification plays the narration ACTIVITY_FREERIDE/ACTIVITY/CHALLENGE_APPEAR
//     (App.PlayUILocalizedSoundEvent, AudioLocalizedActivity / Audio_ACTIVITY_FREERIDE) when the player enters an area and stops it
//     (App.StopUILocalizedSoundEvent) when the area is left, on the race's own event player (ReflexUiVoice); the Start button's KeyMap
//     image shows a glyph only with ReflexKeyHints (OwnKeyGlyphs: the PC game's KeyMap images are empty);
//   * the pause key (Esc / pad Start) in an area: the area's challenge (ReflexModes.ChallengeAt) launched as ReflexModes.Challenges
//     launches it (EventType FreeRide, EventTypeActivation, TrackId, InCareerMode), behind the AAA loading page (OwnAaaLoading: the
//     install's AAALoadingLayer); the pause page stays closed (OriginalPauseMenu.Allowed);
//   * EXIT: in a challenge started that way, the pause page's QUIT goes back to the Free Ride it was started in (the original's
//     PauseMenu:OnQuitEventPopupResult -> App.QuitToFreeRide for an activated event), and so does PostChallengePopup's EXIT
//     (OwnPostChallenge: App.RealWorldSwitchToFreeRide), both behind the AAA loading page.
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.FrontEnd;
using System.Linq;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    [DisallowMultipleComponent]
    public sealed class OwnActivationArea : MonoBehaviour
    {
        OriginalWorldRuntime runtime;
        ReflexEvents.Event ev;
        OwnActivationNotice notice;
        bool failed, launching, voicePlaying;

        /// The Free Ride a challenge was started in (its EXIT goes back there); null before the first start.
        public static string ReturnFreeRide { get; private set; }
        /// The last challenge an area started (tests).
        public static string LastStarted { get; private set; }

        public ReflexEvents.Event Event => ev;
        /// This world is a challenge (an activated event), not the Free Ride.
        public bool IsChallenge => ev != null && !string.IsNullOrEmpty(ev.spec.activation);
        /// The local player is in an activation area (the game's MXPlayer +0xa0).
        public bool InArea => runtime && runtime.HasActivation && runtime.Activation.active != 0;
        /// The KeyMap glyphs of the notification (tests; null before the first area).
        public OwnKeyGlyphs Glyphs => notice?.Glyphs;
        /// The area's narration was requested and is playing (tests).
        public bool VoicePlaying => voicePlaying;
        /// The notification is up (tests).
        public bool NotificationShown => notice != null && notice.ActivationShown;
        /// The notification's texts (tests): the activation line and the class requirement (title and class name; null while hidden).
        public string NotificationText => notice?.ActivationText;
        public string HelpText => notice?.RequirementText;
        /// The pause key belongs to the area (OriginalPauseMenu leaves it alone), also in the frames the challenge is being started.
        public bool ClaimsPause => !IsChallenge && (InArea || launching);

        public static OwnActivationArea Attach(OriginalWorldRuntime rt, ReflexEvents.Event e)
        {
            if (!rt || e == null) return null;
            var a = rt.GetComponent<OwnActivationArea>() ?? rt.gameObject.AddComponent<OwnActivationArea>();
            a.runtime = rt; a.ev = e; a.launching = false;
            Debug.Log("[Activation] " + e.key + (a.IsChallenge ? ": a challenge (QUIT goes back to its Free Ride)" : ": the challenges Free Ride (activation areas on)"));
            if (a.IsChallenge) { OwnPreChallenge.Attach(rt, e); OwnPostChallenge.Attach(rt, e); }   // the challenge's rules popup (before it starts) and end page (medal, RESTART / EXIT)
            return a;
        }

        static bool PausePressed()
        {
            if (Keyboard.current?.escapeKey.wasPressedThisFrame == true) return true;
            foreach (var pad in Gamepad.all) if (pad.startButton.wasPressedThisFrame) return true;
            return false;
        }

        void Update()
        {
            if (!runtime || ev == null || IsChallenge) { Hide(); return; }
            bool menus = runtime.paused || runtime.OwnMenuActive || (runtime.pauseMenu && runtime.pauseMenu.IsOpen) || Reflex.Port.Options.ReflexOptions.IsOpen;
            Voice(InArea);
            ShowNotification(!menus);
            if (InArea && !launching && runtime.Running && !menus && PausePressed()) StartChallenge();
        }

        // ------------------------------------------------------------------------------------------------- the AAA notification
        void ShowNotification(bool allowed)
        {
            var a = runtime.Activation;
            bool area = InArea, needs = runtime.HasActivation && a.required_class >= 0;
            bool show = allowed && !Reflex.Port.Options.ReflexSettings.HudHidden && (area || needs);
            if (show && notice == null && !failed)
            {
                try { notice = new OwnActivationNotice(transform); }
                catch (Exception e) { failed = true; Debug.LogWarning("[Activation] the AAA notification was not built: " + e.Message); }
            }
            if (notice == null) return;
            if (!show) { notice.Hide(); return; }
            notice.Show(area, needs ? a.required_class : -1);   // ToggleNotification: EventTypeActivation ~= FreeRide; ToggleHelp: ChallengeRequiredVehicleClass >= 0
        }

        void Hide() => notice?.Hide();

        // AAANotification:ToggleNotification: in an area (EventTypeActivation ~= FreeRide) the narration plays, back in the Free Ride it stops.
        void Voice(bool area)
        {
            if (area == voicePlaying) return;
            voicePlaying = area;
            var audio = runtime.gameAudio;
            if (!audio) return;
            if (area)
            {
                ReflexUiVoice.Stop(audio, ReflexUiVoice.AaaWrapper);   // UILocSound_Play replaces the owner's request
                bool ok = ReflexUiVoice.Play(audio, "AudioLocalizedActivity", "Audio_ACTIVITY_FREERIDE", "ACTIVITY_FREERIDE/ACTIVITY/CHALLENGE_APPEAR", ReflexUiVoice.AaaWrapper);
                Debug.Log("[Activation] CHALLENGE_APPEAR narration " + (ok ? "requested" : "not available (the package is not installed)"));
            }
            else ReflexUiVoice.Stop(audio, ReflexUiVoice.AaaWrapper);
        }

        // ------------------------------------------------------------------------------------------------- the pause in an area
        void OnDisable() => Hide();
        void OnDestroy() { notice?.Dispose(); notice = null; }

        void StartChallenge()
        {
            var a = runtime.Activation;
            var challenge = ReflexModes.ChallengeAt(ev.location, a.Activity, a.track_id);
            if (challenge == null) { Debug.LogWarning("[Activation] no challenge for the area " + a.Activity + " track " + a.track_id + " at " + ev.location); return; }
            launching = true;
            ReturnFreeRide = ev.key; LastStarted = challenge.key;
            Debug.Log("[Activation] the pause in the area " + a.Activity + " track " + a.track_id + ": " + challenge.key);
            var garage = runtime.garage;
            OwnAaaLoading.Show();
            runtime.StopSimulation();
            ReflexEventSelect.Launch(challenge.key, 0, false, "Reflex", 0, garage);
        }

        /// EXIT of a challenge started in an area (OriginalPauseMenu's QUIT): the Free Ride it was started in. False when this world is
        /// not such a challenge (QUIT then goes to the front end as before).
        public static bool QuitToFreeRide(OriginalWorldRuntime rt)
        {
            if (!rt || !rt.TryGetComponent<OwnActivationArea>(out var area) || !area.IsChallenge) return false;
            string back = ReturnFreeRide ?? ReflexModes.FreeRideOf(area.ev.key);
            var target = back != null ? ReflexEvents.Find(back) : null;
            if (target == null) return false;
            Debug.Log("[Activation] EXIT " + area.ev.key + ": back to " + target.key);
            area.launching = true;
            var garage = rt.garage;
            OwnAaaLoading.Show();
            AudioListener.pause = false;
            rt.StopSimulation();
            ReflexEventSelect.Launch(target.key, 0, false, "Reflex", 0, garage);
            return true;
        }
    }

    /// The AAA notification's picture (OwnActivationArea feeds it from the core's snapshot; the showcase and tests draw it directly).
    public sealed class OwnActivationNotice : IDisposable
    {
        readonly GameObject host; readonly VisualElement root, layer, notification, help, helpArt;
        readonly Label notificationText, helpTitle, helpClass;
        int shownClass = int.MinValue; bool visible;
        public OwnKeyGlyphs Glyphs { get; }

        public OwnActivationNotice(Transform parent)
        {
            OuUi.EnsureEventSystem();
            host = new GameObject("Open Reflex AAA notification"); host.transform.SetParent(parent, false);
            try
            {
                root = OuUi.CreateDocument(host, 30); OuTheme.Apply(root);
                root.style.display = DisplayStyle.None; root.pickingMode = PickingMode.Ignore;
                layer = OuUi.Div("mxa-layer oc-aaa-layer", root); layer.pickingMode = PickingMode.Ignore;
                var col = OuUi.Div("oc-aaa-col", layer); col.pickingMode = PickingMode.Ignore;
                // NotificationContainer: the Start key and STR_CHALLENGE_ACTIVATION: an orange key block on a dotted dark patch
                notification = OuUi.Div("oc-aaa-note", col); notification.pickingMode = PickingMode.Ignore;
                var keyBlock = OuUi.Div("oc-aaa-keyblock", notification); keyBlock.pickingMode = PickingMode.Ignore;
                var cap = OuUi.Text("", "mxa-key oc-aaa-key", keyBlock); cap.enableRichText = false;
                var patch = OuUi.Div("oc-aaa-patch", notification); patch.pickingMode = PickingMode.Ignore; patch.style.backgroundImage = OcTex.Tile;
                notificationText = OuUi.Text(OwnPostChallenge.Str("STR_CHALLENGE_ACTIVATION", "Activate Challenge").ToUpperInvariant(), "oc-aaa-text", patch); notificationText.enableRichText = false;
                // HelpContainer: STR_CHALLENGE_VEHICLE_REQUIREMENT, the class's name (Vehicle Text) and the silhouettes of its vehicles
                help = OcUi.Panel(col, OwnPostChallenge.Str("STR_CHALLENGE_VEHICLE_REQUIREMENT", "This Challenge requires one of the following:").ToUpperInvariant(), out var body, out helpTitle, "oc-aaa-help");
                help.pickingMode = PickingMode.Ignore; body.pickingMode = PickingMode.Ignore;
                helpArt = OuUi.Div("oc-aaa-art", body); helpArt.pickingMode = PickingMode.Ignore;
                helpClass = OuUi.Text("", "oc-aaa-class", body); helpClass.enableRichText = false;
                notification.style.display = help.style.display = DisplayStyle.None;   // AAANotification:Start
                Glyphs = OwnKeyGlyphs.ForCaps(); Glyphs.AddCap("Start", cap);          // MapKey_UI_Start: the pause request that starts the challenge
            }
            catch { UnityEngine.Object.Destroy(host); throw; }
        }

        public bool ActivationShown => visible && root.style.display != DisplayStyle.None && notification.style.display != DisplayStyle.None;
        public string ActivationText => ActivationShown ? notificationText.text : null;
        public string RequirementText => visible && help.style.display != DisplayStyle.None ? helpTitle.text + " " + helpClass.text : null;

        /// The notification up: in an area (the activation line), and / or with a vehicle class required (>= 0).
        public void Show(bool area, int requiredClass)
        {
            notification.style.display = area ? DisplayStyle.Flex : DisplayStyle.None;
            bool needs = requiredClass >= 0;
            help.style.display = needs ? DisplayStyle.Flex : DisplayStyle.None;
            if (needs && shownClass != requiredClass)
            {   // App.BuildVehicleClassComponent: the class's name
                shownClass = requiredClass;
                helpClass.text = OpenContent.ClassName(requiredClass);
                SetSilhouettes(requiredClass);
            }
            if (!visible)
            {
                visible = true;
                root.style.display = DisplayStyle.Flex;
                OuMotion.Sync(root);
                layer.RemoveFromClassList("ou-in"); OuMotion.Fade(layer);
            }
            Glyphs.Refresh();
        }

        public void Hide()
        {
            if (!visible) return;
            visible = false;
            root.style.display = DisplayStyle.None;
        }

        // the class's vehicle types as the career hub's silhouettes (IconVehicles0 of the install)
        void SetSilhouettes(int vehicleClass)
        {
            helpArt.Clear();
            var kinds = new List<string>();
            foreach (var r in FeTables.Shared.Rows("VehicleClassType"))
                if (FeTables.I(r, "VehicleClassId") == vehicleClass) { string k = OcUi.SilhouetteOf(FeTables.I(r, "VehicleTypeId")); if (k != null && !kinds.Contains(k)) kinds.Add(k); }
            foreach (var k in kinds.Take(3))
            {
                var bg = OcUi.Silhouette(k);
                if (bg.sprite == null && bg.texture == null) continue;
                var art = OuUi.Div("oc-aaa-sil", helpArt); art.pickingMode = PickingMode.Ignore; art.style.backgroundImage = bg;
            }
            helpArt.style.display = helpArt.childCount > 0 ? DisplayStyle.Flex : DisplayStyle.None;
        }

        public void Dispose() { if (host) UnityEngine.Object.Destroy(host); }
    }
}
