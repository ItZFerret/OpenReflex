// TEST ONLY: the ORIGINAL HUD's compass and challenge components of the open core's Free Ride (OriginalFreeRideHud), drawn from the install's
// layers with SYNTHETIC frames (no world: a procedural backdrop, a camera at the origin, marks at chosen bearings), and checked against the original's
// rules (the heading, the strip's scroll, the letters, the 45 degree clamp, the alpha of the area icons and the targets, the 3 s counter).
//   hudrest-compass       the Free Ride page, heading 0 / 40 / 200: the ten challenge icons around the camera (discovery mode), the letters, the strip
//   hudrest-nodiscovery   the Free Ride outside the Race and Ride challenges: the strip and the letters only
//   hudrest-trailblazer   the Trailblazer page: the timer counting up, the compass with the FinishLine icon and its distance label (imperial, metric)
//   hudrest-targets       the Target Practice page: 12 targets, 3 hit, the Target Hit Component "4/12" after the next hit, the timer
//   hudrest-hillclimb     the Hill Climb page: the timer, PositionWatermark + the height achieved
//   hudrest-hide          a menu / the other HUD style / H hide every page at once
using System;
using System.Collections;
using Reflex.Port.OriginalWorld;
using Reflex.Port.RxCore;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseHudRest
    {
        const string Tag = "[ui-showcase] [hudrest] ";

        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register()
        {
            UiShowcase.Register("hudrest-compass", Compass);
            UiShowcase.Register("hudrest-nodiscovery", NoDiscovery);
            UiShowcase.Register("hudrest-trailblazer", Trailblazer);
            UiShowcase.Register("hudrest-targets", Targets);
            UiShowcase.Register("hudrest-hillclimb", HillClimb);
            UiShowcase.Register("hudrest-hide", Hide);
        }

        static Vector3 At(float bearingDeg, float feet) { float r = bearingDeg * Mathf.Deg2Rad; return new Vector3(Mathf.Sin(r) * feet, 0f, Mathf.Cos(r) * feet); }
        static Vector3 Forward(float headingDeg) => At(headingDeg, 1f);

        static OriginalFreeRideHud.Frame Frame(OriginalFreeRideHud.Page page, float heading, RxCompassState st, bool has = true)
            => new OriginalFreeRideHud.Frame { dt = 1f / 60f, allowed = true, page = page, viewPosition = Vector3.zero, viewForward = Forward(heading), compass = st, hasCompass = has };

        static IEnumerator Settle(OriginalFreeRideHud hud, OriginalFreeRideHud.Frame f, float seconds)
        {
            for (float t = 0; t < seconds; t += 1f / 60f) { hud.Tick(f); yield return null; }
        }

        // ten areas, one per challenge type, spread around the camera at different distances (the layer's icon order)
        static RxCompassState Areas(bool discovery)
        {
            var s = RxCompassState.Create(); s.valid = 1; s.discoveryMode = discovery ? 1u : 0u;
            float[] bearing = { -60f, -35f, -18f, -6f, 8f, 20f, 33f, 50f, 140f, 300f };   // BigAir .. Wheelie; the last three are outside the +-45 degrees
            float[] feet = { 400f, 900f, 1400f, 200f, 2300f, 1700f, 600f, 1100f, 800f, 2600f };   // the last is beyond 2500 ft: alpha 0
            for (int i = 0; i < 10; i++) s.Set(i, RxCompassState.TypeArea, i, 1, At(bearing[i], feet[i]).x, 0f, At(bearing[i], feet[i]).z);
            return s;
        }

        static IEnumerator Compass()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD compass gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            var st = Areas(true);
            var f = Frame(OriginalFreeRideHud.Page.FreeRide, 0f, st);
            yield return Settle(hud, f, 0.4f);
            var c = hud.Compass;
            UiShowcaseNav.Check(hud.Built(OriginalFreeRideHud.Page.FreeRide) && c != null, "hudrest", "the Free Ride layer's compass was not built");
            UiShowcaseNav.Check(Mathf.Abs(c.heading) < 0.01f, "hudrest", "heading 0 reads " + c.heading);
            Debug.Log(Tag + "compass strip tile " + (c.tile ? c.tile.texture.name + " " + c.tileWidth : "(none)") + " scroll " + c.scroll + " tileUv " + (c.tile ? c.tile.uvRect.ToString() : ""));
            // the letters: N under the centre indicator at heading 0, E 90 degrees to the right
            float nx = c.north.rect.anchoredPosition.x, ex = c.east.rect.anchoredPosition.x;
            UiShowcaseNav.Check(Mathf.Abs(nx - 320f) < 0.01f && Mathf.Abs(ex - (320f + 90f * 2560f / 360f)) < 0.01f, "hudrest", "the letters at heading 0: N " + nx + " E " + ex);
            // the icons: the area at 8 degrees (StepUp) is right of centre by 8 degrees, the 20 degree one (Stoppie) farther; -60 is clamped to -45
            float stepUp = c.challenge[4].rect.anchoredPosition.x, bigAir = c.challenge[0].rect.anchoredPosition.x;
            UiShowcaseNav.Check(Mathf.Abs(stepUp - (320f + 8f * 2560f / 360f)) < 0.2f, "hudrest", "StepUp icon at " + stepUp);
            UiShowcaseNav.Check(Mathf.Abs(bigAir - (320f - 45f * 2560f / 360f)) < 0.01f, "hudrest", "BigAir (-60 degrees) is not clamped to the strip's edge: " + bigAir);
            UiShowcaseNav.Check(c.challengeUp[0] && c.challengeUp[9], "hudrest", "area icons missing");
            var wheelie = c.challenge[9].graphic.color.a;
            UiShowcaseNav.Check(wheelie < 0.01f, "hudrest", "an area beyond 2500 ft is not transparent (" + wheelie + ")");
            var pathfinder = c.challenge[3].graphic.color.a; float expect = c.challenge[3].baseColor.a * (1f - 200f * 200f / 6.25e6f);
            UiShowcaseNav.Check(Mathf.Abs(pathfinder - expect) < 0.01f, "hudrest", "Pathfinder (200 ft) alpha " + pathfinder + " expected " + expect);
            Debug.Log(Tag + "icons x: " + string.Join(" ", System.Linq.Enumerable.Select(c.challenge, n => n.rect.anchoredPosition.x.ToString("F0"))));
            yield return UiShowcase.Shot("hudrest-compass-north");

            f = Frame(OriginalFreeRideHud.Page.FreeRide, 40f, st);
            yield return Settle(hud, f, 0.2f);
            UiShowcaseNav.Check(Mathf.Abs(c.heading - 40f) < 0.01f, "hudrest", "heading 40 reads " + c.heading);
            float n40 = c.north.rect.anchoredPosition.x;
            UiShowcaseNav.Check(Mathf.Abs(n40 - (320f - 40f * 2560f / 360f)) < 0.01f, "hudrest", "N at heading 40: " + n40);
            yield return UiShowcase.Shot("hudrest-compass-ne");

            f = Frame(OriginalFreeRideHud.Page.FreeRide, 200f, st);
            yield return Settle(hud, f, 0.2f);
            UiShowcaseNav.Check(Mathf.Abs(c.heading - 200f) < 0.01f, "hudrest", "heading 200 reads " + c.heading);
            float s200 = c.south.rect.anchoredPosition.x;
            UiShowcaseNav.Check(Mathf.Abs(s200 - (320f + (180f - 200f) * 2560f / 360f)) < 0.01f, "hudrest", "S at heading 200: " + s200);
            UiShowcaseNav.Check(c.challengeUp[8], "hudrest", "the TargetPractice area (140 degrees) is behind the camera but its icon must still be placed on the strip");
            yield return UiShowcase.Shot("hudrest-compass-ssw");

            // west side heading (the 360 - acos branch)
            f = Frame(OriginalFreeRideHud.Page.FreeRide, 315f, st);
            yield return Settle(hud, f, 0.1f);
            UiShowcaseNav.Check(Mathf.Abs(c.heading - 315f) < 0.05f, "hudrest", "heading 315 reads " + c.heading);
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator NoDiscovery()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD compass gallery 2");
            var hud = new OriginalFreeRideHud(host.transform);
            yield return Settle(hud, Frame(OriginalFreeRideHud.Page.FreeRide, 120f, Areas(false)), 0.3f);
            var c = hud.Compass;
            bool any = false; for (int i = 0; i < 10; i++) any |= c.challengeUp[i];
            UiShowcaseNav.Check(!any, "hudrest", "area icons shown outside the discovery mode");
            UiShowcaseNav.Check(c.east != null && Mathf.Abs(c.east.rect.anchoredPosition.x - (320f - 30f * 2560f / 360f)) < 0.01f, "hudrest", "E at heading 120");
            yield return UiShowcase.Shot("hudrest-compass-free-ride");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator Trailblazer()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD trailblazer gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            var st = RxCompassState.Create(); st.valid = 1; st.challengeKind = 6;
            var finish = At(15f, 1320f);
            st.Set(0, RxCompassState.TypeFinish, 0, 0, finish.x, 0f, finish.z);
            var f = Frame(OriginalFreeRideHud.Page.Trailblazer, 0f, st);
            f.timerShown = true; f.timerValue = 47.31f;
            yield return Settle(hud, f, 0.4f);
            var c = hud.Compass;
            UiShowcaseNav.Check(hud.Built(OriginalFreeRideHud.Page.Trailblazer) && c != null, "hudrest", "the Trailblazer layer was not built");
            UiShowcaseNav.Check(c.finishUp && Mathf.Abs(c.finish.rect.anchoredPosition.x - (320f + 15f * 2560f / 360f)) < 0.2f, "hudrest", "the finish line icon at " + c.finish.rect.anchoredPosition.x);
            UiShowcaseNav.Check(c.labelText == "1320 ft", "hudrest", "the distance label '" + c.labelText + "'");
            UiShowcaseNav.Check(hud.ViewOf(OriginalFreeRideHud.Page.Trailblazer).timerText == "00:47.31", "hudrest", "the timer '" + hud.ViewOf(OriginalFreeRideHud.Page.Trailblazer).timerText + "'");
            yield return UiShowcase.Shot("hudrest-trailblazer");
            f.metric = true; f.viewForward = Forward(-30f); f.timerValue = 49.9f;
            yield return Settle(hud, f, 0.2f);
            UiShowcaseNav.Check(c.labelText == "402 m", "hudrest", "the metric distance label '" + c.labelText + "'");
            // behind the camera: pinned to the strip's edge
            f.viewForward = Forward(180f);
            yield return Settle(hud, f, 0.1f);
            UiShowcaseNav.Check(c.finishUp, "hudrest", "the finish icon left the strip behind the camera");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator Targets()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD targets gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            var st = RxCompassState.Create(); st.valid = 1; st.challengeKind = 8;
            for (int i = 0; i < 12; i++) { var p = At(-70f + i * 14f, 120f + 35f * i); st.Set(i, RxCompassState.TypeTarget, i, i < 3 ? 1 : 0, p.x, 0f, p.z); }
            var f = Frame(OriginalFreeRideHud.Page.TargetPractice, 0f, st);
            f.timerShown = true; f.timerValue = 41.2f; f.valueValid = true; f.value = 3f;
            yield return Settle(hud, f, 0.4f);
            var c = hud.Compass;
            UiShowcaseNav.Check(hud.Built(OriginalFreeRideHud.Page.TargetPractice) && c != null, "hudrest", "the TargetPractice layer was not built");
            int up = 0; for (int i = 0; i < 16; i++) if (c.targetUp[i]) up++;
            UiShowcaseNav.Check(up == 9 && !c.targetUp[0] && !c.targetUp[2] && c.targetUp[3], "hudrest", up + " target icons up (9 expected: 3 of 12 are hit)");
            UiShowcaseNav.Check(!hud.Shown, "hudrest", "the Target Hit Component shows before a hit");
            float a3 = c.target[3].graphic.color.a, expect = 1f - Mathf.Pow(120f + 35f * 3, 2) / 250000f;
            UiShowcaseNav.Check(Mathf.Abs(a3 - expect) < 0.01f, "hudrest", "target 3 alpha " + a3 + " expected " + expect);
            yield return UiShowcase.Shot("hudrest-targets-before");
            f.value = 4f;   // the 4th target hit
            var p3 = At(-70f + 3 * 14f, 120f + 35f * 3); st.Set(3, RxCompassState.TypeTarget, 3, 1, p3.x, 0f, p3.z);
            f.compass = st; f.timerValue = 40.1f;
            yield return Settle(hud, f, 0.5f);
            UiShowcaseNav.Check(hud.Shown && hud.Text == "4/12", "hudrest", "a hit did not show 4/12 (" + hud.Shown + ", '" + hud.Text + "')");
            UiShowcaseNav.Check(!c.targetUp[3], "hudrest", "a hit target's icon stayed on the strip");
            yield return UiShowcase.Shot("hudrest-targets-hit");
            yield return Settle(hud, f, 3.3f);
            UiShowcaseNav.Check(!hud.Shown, "hudrest", "the Target Hit Component stayed past 3 s");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator HillClimb()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD hill climb gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            var f = Frame(OriginalFreeRideHud.Page.HillClimb, 0f, default, false);
            f.timerShown = true; f.timerValue = 33.5f; f.valueValid = true; f.value = 187.4f;
            yield return Settle(hud, f, 0.4f);
            var v = hud.ViewOf(OriginalFreeRideHud.Page.HillClimb);
            UiShowcaseNav.Check(hud.Built(OriginalFreeRideHud.Page.HillClimb) && v.heightAchieved != null, "hudrest", "the Hill Climb layers were not built");
            string h = v.heightAchieved.text.Text;
            UiShowcaseNav.Check(h == "187 ft", "hudrest", "the height '" + h + "'");
            yield return UiShowcase.Shot("hudrest-hillclimb");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }

        static IEnumerator Hide()
        {
            UiShowcase.ShellVisible(false);
            var backdrop = HudShowcase.Backdrop(5);
            var host = new GameObject("original HUD hide gallery");
            var hud = new OriginalFreeRideHud(host.transform);
            var f = Frame(OriginalFreeRideHud.Page.FreeRide, 0f, Areas(true));
            yield return Settle(hud, f, 0.2f);
            UiShowcaseNav.Check(hud.Canvas.enabled && hud.Current == OriginalFreeRideHud.Page.FreeRide, "hudrest", "the Free Ride page is not up");
            f.allowed = false;   // a menu, MODERN, H
            yield return Settle(hud, f, 0.05f);
            UiShowcaseNav.Check(!hud.Canvas.enabled && hud.Current == OriginalFreeRideHud.Page.None, "hudrest", "allowed = false left the page up");
            f.allowed = true; f.page = OriginalFreeRideHud.Page.None;   // a mode without a layer
            yield return Settle(hud, f, 0.05f);
            UiShowcaseNav.Check(!hud.Canvas.enabled, "hudrest", "a mode without a layer left the page up");
            hud.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backdrop);
            yield return UiShowcase.Frames(3);
        }
    }
}
