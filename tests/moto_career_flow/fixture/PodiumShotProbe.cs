using System.Collections;
using System.IO;
using Reflex.Port.OriginalWorld;
using UnityEngine;

// Test only (tests/moto_career_flow, w-podium, w-cast): screenshots of the scenario cast. Boots itself when REFLEX_PREVIEW_SHOTS is set
// (run.py sets it for every probe) and only watches: while the activity's finish scenario plays (OriginalPodium.scenario 1) it logs each
// step of the game's scenario (7 the finish replay, 8 the winner's mound, 9 / 11 the walks, 10 / 12 the podium) with the podium riders
// drawn, and captures "podium-<n>-step<step>.png" once per step, 0.8 s (real time) after the step began, so the scenario camera has
// moved onto its first shot. The same for the intro scenario (scenario 3: the steps 1 overview, 2 / 3 hero shots, 4 / 5 crew chief shots,
// 6 the board) as "intro-<n>-step<step>.png". It presses nothing and writes no game state.
public sealed class PodiumShotProbe : MonoBehaviour
{
    const string Tag = "[podium-shots] ";
    string directory; int lastScenario, lastStep, lastDrawn = -1, scenarios, intros; float stepStarted; bool stepShot; int stepSeries;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot()
    {
        string d = System.Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS");
        if (string.IsNullOrEmpty(d)) return;
        var go = new GameObject("Podium shot probe (test)"); DontDestroyOnLoad(go);
        go.AddComponent<PodiumShotProbe>().directory = d;
    }

    void Update()
    {
        var rt = FindAnyObjectByType<OriginalWorldRuntime>();
        var podium = rt ? rt.podium : null;
        int scenario = podium ? podium.scenario : 0, step = podium ? podium.step : 0, drawn = podium ? podium.drawn : 0;
        if (scenario != lastScenario || step != lastStep)
        {
            if (scenario == 1 && lastScenario != 1) scenarios++;
            if (scenario == 3 && lastScenario != 3) intros++;
            Debug.Log(Tag + "SCENARIO " + scenario + " step " + step + " post " + (rt ? rt.EventUi.post_state : -1) + " podium riders drawn " + drawn
                      + " built " + (podium ? podium.characters : 0) + " race " + (rt ? rt.Race.state : -1) + " n " + scenarios);
            lastScenario = scenario; lastStep = step; stepStarted = Time.unscaledTime; stepShot = false; stepSeries = 0;
        }
        if (drawn != lastDrawn) { Debug.Log(Tag + "DRAWN " + drawn + " (scenario " + scenario + " step " + step + ")"); lastDrawn = drawn; }
        // one shot per step 0.8 s after it began; the finish replay (step 7, ~5 s of game time: ~1.3 s real at --speed 4) also every 0.2 s
        if (scenario == 1 && step > 0 && !stepShot && Time.unscaledTime - stepStarted > 0.8f)
        {
            stepShot = true;
            StartCoroutine(Capture("podium-" + scenarios + "-step" + step, drawn));
        }
        if (scenario == 3 && step > 0 && !stepShot && Time.unscaledTime - stepStarted > 0.8f)
        {
            stepShot = true;
            StartCoroutine(Capture("intro-" + intros + "-step" + step, drawn));
        }
        if (scenario == 1 && step == 7 && stepSeries < 8 && Time.unscaledTime - stepStarted > 0.2f * (stepSeries + 1))
        {
            stepSeries++;
            StartCoroutine(Capture("podium-" + scenarios + "-step7-" + stepSeries, drawn));
        }
    }

    IEnumerator Capture(string name, int drawn)
    {
        yield return new WaitForEndOfFrame();
        ScreenCapture.CaptureScreenshot(Path.Combine(directory, name + ".png"));
        var cam = Camera.main;
        Debug.Log(Tag + "screenshot " + name + " cast riders drawn " + drawn + (cam ? " camera " + cam.transform.position + " fwd " + cam.transform.forward + " fov " + cam.fieldOfView.ToString("0.0") : ""));
    }
}
