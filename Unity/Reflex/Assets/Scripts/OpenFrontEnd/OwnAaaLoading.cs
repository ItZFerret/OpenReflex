// The AAA loading page of the open product: the original's AAALoading, shown when a Free Ride activation area starts its challenge (the
// pause key in an area: MXUIManager_OnPauseRequest 00598e40 -> the page) and when a challenge's EXIT switches back to the level's Free Ride
// (App.RealWorldSwitchToFreeRide). The page's content is the install's AAALoadingLayer's (InGame.ENG AAALoadingLayer.lyr, script AAALoading.lua): a black
// background, the LoadingText (STR_LOADING) and a spinner turning at the layer's RotationSpeed 180 degrees per second. It is drawn in the
// modern UI Toolkit style (OuModern.uss: the wordmark, a ring spinner, a progress bar), with the original's string and timings.
// Once the level is in (AAALoading:RequestGotoHub: the UI event GotoHub the challenge raised) the page wipes away with the original's
// PageWipe timings (Shared PageWipe.lyr, script PageWipe.lua): ShowPageWipe( "AAALoading" ) plays "In" (a black Background fading in
// over 0.167 s), at the end of "In" the transition is announced (PageWipeTransition: the loading page goes, RoutedGotoHub) and "Out"
// plays (black held, fading out between 0.33 s and 0.5 s of its 0.533 s), at its end the wipe hides (PageWipeComplete). The page waits
// for the next original world the way the open loading screen does (OpenLoading): until it is running (OriginalWorldRuntime.Running)
// or has stopped. The first ride of a setup makes its world from the install (15 to 40 s): the text then also shows the build's
// percentage, as OpenLoading's bar.
using System;
using System.Collections.Generic;
using Reflex.Port.Content;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.OpenFrontEnd
{
    public sealed class OwnAaaLoading : MonoBehaviour
    {
        public static OwnAaaLoading Current { get; private set; }
        /// The page is up and built (tests).
        public bool Built => root != null && root.panel != null && page != null && page.resolvedStyle.display != DisplayStyle.None && page.resolvedStyle.opacity > 0;
        /// The LoadingText now.
        public string Line => loadingText?.text;
        /// The spinner's turn so far in degrees (tests).
        public float SpinnerDegrees => spin;
        /// The world was being built while this was up (tests).
        public bool SawBuild { get; private set; }
        /// A world was seen running after this page came up (it is on its way out).
        public bool Leaving => wipe != 0;
        /// The PageWipe's stage: 0 not started, 1 "In", 2 "Out" (tests).
        public int WipeStage => wipe;
        /// PageWipes that ran to their end in this process (PageWipeComplete), and the transitions announced (tests).
        public static int WipesCompleted { get; private set; }
        public static int WipeTransitions { get; private set; }

        // PageWipe.lyr's animations: "In" 0.167 s, "Out" 0.533 s (held black until 0.33 s, clear at 0.5 s)
        const float WipeInSeconds = 0.167f, WipeOutSeconds = 0.533f;

        VisualElement root, page, spinnerRing, barFill, wipeBlack;
        Label loadingText;
        OriginalWorldRuntime runtime;
        string baseLine;
        float age, spin, progress, wipeTime;
        int wipe;   // 0 waiting for the world, 1 PageWipe "In", 2 "Out"

        /// The career's AAA loading page between a Heat event's rounds (UIRoutingMessage "Select Activity Area"): the same layer, held up
        /// until the career pages release it under the career's own "AAALoading" PageWipe (the career curtain plays that wipe, so this page
        /// plays none of its own).
        public static OwnAaaLoading ShowHeld() { var page = Show(); page.held = true; return page; }
        public bool Held => held;
        public void Release() { if (this && gameObject) Destroy(gameObject); }
        bool held;

        public static OwnAaaLoading Show()
        {
            if (Current) Destroy(Current.gameObject);
            var go = new GameObject("AAA loading");
            if (Application.isPlaying) DontDestroyOnLoad(go);
            var p = go.AddComponent<OwnAaaLoading>();
            Current = p;
            p.Build();
            return p;
        }

        void Build()
        {
            try
            {
                OuUi.EnsureEventSystem();
                root = OuUi.CreateDocument(gameObject, 400);   // over every page and the HUD
                OuTheme.Apply(root);
                page = OuUi.Div("ou-loading2", root);
                OuMotion.Fade(page);
                var center = OuUi.Div("ou-loading2-center", page);
                OuTheme.Logo(OuUi.Div("ou-loading-logo", center), 64, true);
                spinnerRing = OuUi.Div("ou-spinner", center); spinnerRing.style.marginTop = 36;
                baseLine = OpenContent.Str("STR_LOADING", "LOADING");
                loadingText = OuUi.Text(baseLine, "ou-loading2-text ou-label", center); loadingText.enableRichText = false;
                var bar = OuUi.Div("ou-loading2-bar", center); barFill = OuUi.Div("ou-loading2-fill", bar);
                bar.style.display = DisplayStyle.None; bar.name = "aaa-loading-bar";
                wipeBlack = OuUi.Div("ou-overlay", root); wipeBlack.style.backgroundColor = Color.black; wipeBlack.style.opacity = 0; wipeBlack.pickingMode = PickingMode.Ignore;
            }
            catch (Exception e) { Debug.LogWarning("[AAALoading] the page was not built: " + e.Message); root = null; }
        }

        void OnDestroy() { if (Current == this) Current = null; }

        // ------------------------------------------------------------------------------------------------------ the PageWipe
        // PageWipe:Initialize / ShowPageWipe: the layer, hidden; "In" starts at the event.
        void StartWipe()
        {
            wipe = 1; wipeTime = 0;
            if (wipeBlack == null) { Destroy(gameObject); return; }
            Debug.Log("[AAALoading] PageWipeStart: AAALoading (In " + WipeInSeconds.ToString("F3", System.Globalization.CultureInfo.InvariantCulture) + " s, Out " + WipeOutSeconds.ToString("F3", System.Globalization.CultureInfo.InvariantCulture) + " s)");
        }

        void TickWipe(float dt)
        {
            wipeTime += dt;
            if (wipe == 1)
            {
                wipeBlack.style.opacity = Mathf.Clamp01(wipeTime / WipeInSeconds);
                if (wipeTime >= WipeInSeconds)
                {   // PageWipe:OnInAnimationComplete: "Out" plays and PageWipeTransition is sent: AAALoading:OnPageWipeTransition routes on, the page goes
                    wipe = 2; wipeTime = 0; WipeTransitions++;
                    page.style.display = DisplayStyle.None;
                    wipeBlack.style.opacity = 1;
                    Debug.Log("[AAALoading] PageWipeTransition: AAALoading");
                }
            }
            else if (wipe == 2)
            {
                wipeBlack.style.opacity = wipeTime < 0.333f ? 1f : Mathf.Clamp01(1f - (wipeTime - 0.333f) / 0.167f);
                if (wipeTime >= WipeOutSeconds)
                {   // PageWipe:OnOutAnimationComplete: the layer hides, PageWipeComplete
                    WipesCompleted++;
                    Debug.Log("[AAALoading] PageWipeComplete: AAALoading");
                    Destroy(gameObject);
                }
            }
        }

        void Update()
        {
            float dt = Time.unscaledDeltaTime;
            age += dt;
            if (spinnerRing != null)
            {   // UISpinnerComponent: RotationSpeed 180 degrees per second
                spin += 180f * dt;
                spinnerRing.style.rotate = new Rotate(new Angle(spin, AngleUnit.Degree));
            }
            if (!runtime) runtime = FindAnyObjectByType<OriginalWorldRuntime>();
            string line = "";
            if (runtime && runtime.statusLabel) { line = runtime.statusLabel.text ?? ""; int nl = line.IndexOf('\n'); if (nl >= 0) line = line.Substring(0, nl); }
            bool building = line.StartsWith("BUILDING");
            if (building)
            {
                SawBuild = true;
                try { progress = Mathf.Max(progress, ReflexWorldFiles.Progress(out _)); } catch (Exception) { }
                if (loadingText != null) loadingText.text = baseLine + "  " + Mathf.RoundToInt(progress * 100) + "%";
                if (barFill != null) { barFill.parent.style.display = DisplayStyle.Flex; barFill.style.width = Length.Percent(Mathf.Clamp01(progress) * 100f); }
            }
            else if (line.StartsWith("STOPPED") && loadingText != null) loadingText.text = line;
            if (held) return;   // the career pages release it
            bool done = runtime && (runtime.Running || line.StartsWith("STOPPED")) || !runtime && age > 20f;
            if (done && wipe == 0) StartWipe();
            if (wipe != 0) TickWipe(dt);
        }
    }
}
