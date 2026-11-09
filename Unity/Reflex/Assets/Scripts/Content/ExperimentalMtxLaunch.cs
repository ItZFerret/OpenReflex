using System;
using System.Collections;
using System.Linq;
using Reflex.Port.OriginalWorld;
using UnityEngine;
namespace Reflex.Port.Content
{
    // Opt-in entry point for the isolated experimental desktop launcher and its player smoke test.
    public sealed class ExperimentalMtxLaunch : MonoBehaviour
    {
        bool smoke; int errors;
        public static bool HoldThrottle { get; private set; }
        static bool Has(string flag) => Array.IndexOf(Environment.GetCommandLineArgs(),flag)>=0;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Guest() { if(Has("-reflexMtxSmoke") || Has("-reflexMtxLightShots")) ReflexProfile.Disabled=true; }
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
        static void Boot()
        {
            if(!Has("-reflexMtxExperiment") || FindAnyObjectByType<ExperimentalMtxLaunch>()) return;
            var go=new GameObject("Experimental MTX launcher"); DontDestroyOnLoad(go);go.AddComponent<ExperimentalMtxLaunch>();
        }
        void OnEnable() => Application.logMessageReceived+=Log;
        void OnDisable() => Application.logMessageReceived-=Log;
        void Log(string message,string stack,LogType type) { if(type==LogType.Exception) errors++; }
        IEnumerator Start()
        {
            bool lightShots=Has("-reflexMtxLightShots");
            smoke=Has("-reflexMtxSmoke") || lightShots; Application.runInBackground=true;
            yield return new WaitForSecondsRealtime(5);
            var args=Environment.GetCommandLineArgs(); int trackArg=Array.IndexOf(args,"-reflexMtxTrack");
            bool race=Has("-reflexMtxRace");
            string track=trackArg>=0 && trackArg+1<args.Length ? args[trackArg+1] : "McKennaRock";
            // tests: -reflexLightEvent <event key> shoots a stock event the same way (the reference for -reflexMtxLightShots)
            int stockArg=Array.IndexOf(args,"-reflexLightEvent");
            if(lightShots && stockArg>=0 && stockArg+1<args.Length)
            {
                var stock=ReflexEvents.Find(args[stockArg+1]);
                if(stock==null) { Debug.LogError("[MTX] no event "+args[stockArg+1]); Application.Quit(1); yield break; }
                ReflexEventSelect.Launch(stock.key,race?2:0,remember:false,laps:race?3:0);
            }
            else
            {
            var ev=TrackMods.Events.FirstOrDefault(e=>e.trackMod?.experimentalTerrain==true && e.trackMod.Ok && (race ? e.spec.type!="FreeRide" : e.spec.type=="FreeRide") && string.Equals(e.trackMod.id,track,StringComparison.OrdinalIgnoreCase));
            if(ev==null) { Debug.LogError("[MTX] No valid experimental .rxtrack installed"); if(smoke)Application.Quit(1); yield break; }
            // Trial: enlarge the entire imported world while retaining Reflex vehicle size and physics.
            // The debug scale overlay and its Ctrl+Shift+[ / ] hotkeys are hidden unless asked for with -reflexMtxScaleUi.
            if(Has("-reflexMtxScaleUi") || Has("-reflexMtxScaleSmoke")) gameObject.AddComponent<ExperimentalMtxScale>().EventKey=ev.key;
            ReflexEventSelect.Launch(ev.key,race?2:0,remember:false,laps:race?3:0,mapScale:ExperimentalMtxScale.PercentFor(ev.trackMod)/100f);
            Debug.Log($"[MTX] Experimental track scale: {ExperimentalMtxScale.PercentFor(ev.trackMod)} %");
            }
            if(!smoke) yield break;
            float deadline=Time.realtimeSinceStartup+300; OriginalWorldRuntime rt=null;
            while(!(rt && rt.Running))
            {
                if(Time.realtimeSinceStartup>deadline){Debug.LogError("[MTX] smoke timeout");Application.Quit(1);yield break;}
                rt=FindAnyObjectByType<OriginalWorldRuntime>();yield return null;
            }
            if(race)
            {
                deadline=Time.realtimeSinceStartup+90;
                while(rt.Race.state!=2 && Time.realtimeSinceStartup<deadline)yield return null;
                if(rt.Race.state!=2 || rt.Race.riders<2 || rt.Race.num_laps!=3) { Debug.LogError("[MTX] Race start FAIL");Application.Quit(1);yield break; }
                Debug.Log($"[MTX] Race start PASS: {rt.Race.riders} riders, {rt.Race.num_laps} laps");
            }
            if(lightShots) { yield return LightShots(rt); yield break; }
            yield return new WaitForSecondsRealtime(3);
            // Verify static relief before driving: live tire ribbons alone must not pass this check.
            var bump=FindAnyObjectByType<Reflex.Port.Rendering.ReflexRTBump>();
            int reliefPixels=0;int minHeight=255,maxHeight=0;
            if(bump && bump.Target && bump.draws.Count>=3)
            {
                var previous=RenderTexture.active;RenderTexture.active=bump.Target;
                var probe=new Texture2D(bump.Target.width,bump.Target.height,TextureFormat.RGBA32,false,true);
                probe.ReadPixels(new Rect(0,0,probe.width,probe.height),0,0);probe.Apply();
                foreach(var pixel in probe.GetPixels32())
                {
                    if(Mathf.Abs(pixel.r-128)>4 || pixel.b>4)reliefPixels++;
                    minHeight=Mathf.Min(minHeight,pixel.r);maxHeight=Mathf.Max(maxHeight,pixel.r);
                }
                RenderTexture.active=previous;Destroy(probe);
            }
            bool depthOk=reliefPixels>1000 && maxHeight-minHeight>15;
            Debug.Log($"[MTX] Static terrain relief {(depthOk?"PASS":"FAIL")}: {bump?.draws.Count} layers, {reliefPixels} textured pixels, height range {minHeight}–{maxHeight}");
            string shot=Environment.GetEnvironmentVariable("REFLEX_MTX_SCREENSHOT");
            if(!string.IsNullOrEmpty(shot)) { ScreenCapture.CaptureScreenshot(shot+".start.png");yield return new WaitForSecondsRealtime(1); }
            var scenery=FindAnyObjectByType<ExperimentalMtxView>();
            var helicopter=scenery ? scenery.transform.Find("lobj_newscopter") : null;
            var helicopterStart=helicopter ? helicopter.position : Vector3.zero;
            double first=rt.PresentedGameFrame;
            rt.TryGetBike(out var start,out _);
            HoldThrottle=true;
            yield return new WaitForSecondsRealtime(4);
            if(!string.IsNullOrEmpty(shot))ScreenCapture.CaptureScreenshot(shot+".riding.png");
            yield return new WaitForSecondsRealtime(8);
            HoldThrottle=false;rt.TryGetBike(out var end,out _);
            var terrain=FindAnyObjectByType<OriginalTerrain>();
            bool animationOk=!helicopter || Vector3.Distance(helicopterStart,helicopter.position)>1;
            bool sceneryOk=scenery && scenery.GetComponentsInChildren<MeshRenderer>().Length>100;
            var tracks=terrain ? terrain.GetComponent<OriginalTracks>() : null;
            int markedPixels=0;
            if(tracks && tracks.Triangles>0 && terrain.UnityTerrain.materialTemplate.GetTexture("_TrackMap") is RenderTexture trackMap)
            {
                var previous=RenderTexture.active;RenderTexture.active=trackMap;
                var probe=new Texture2D(trackMap.width,trackMap.height,TextureFormat.RGBA32,false,true);
                probe.ReadPixels(new Rect(0,0,trackMap.width,trackMap.height),0,0);probe.Apply();
                foreach(var pixel in probe.GetPixels32())if(pixel.r<250 || pixel.g<250 || pixel.b<250)markedPixels++;
                RenderTexture.active=previous;Destroy(probe);
            }
            bool marksOk=tracks && tracks.Triangles>0 && markedPixels>20;
            Debug.Log($"[MTX] Tire marks {(marksOk?"PASS":"FAIL")}: {tracks?.Triangles} triangles, {markedPixels} marked texels");
            bool raceOk=!race || (rt.Race.valid!=0 && rt.Race.state==2 && rt.Race.race_time>5 && rt.Race.riders>=2);
            bool ok=raceOk && depthOk && marksOk && animationOk && sceneryOk && errors==0 && rt.Running && rt.PresentedGameFrame>first+120 && Vector3.Distance(start,end)>20 && terrain && terrain.Built;
            if(!string.IsNullOrEmpty(shot)) { ScreenCapture.CaptureScreenshot(shot);yield return new WaitForSecondsRealtime(2); }
            Debug.Log($"[MTX] smoke {(ok?"PASS":"FAIL")}: frames {first} -> {rt.PresentedGameFrame}, distance {Vector3.Distance(start,end)}, exceptions {errors}, terrain {terrain?.Built}, scenery {sceneryOk}, animation {animationOk}");
            if(ok && Has("-reflexMtxDepthSmoke") && !string.IsNullOrEmpty(shot))
            {
                var course=ExperimentalTerrain.Info(TrackMods.Bound.folder).course;
                float scale=Reflex.Port.RxCore.RxMapScale.Current;
                var target=new Vector3(course[20],0,course[22])*scale;
                target.y=terrain.CoreHeightAt(target.x,target.z,out _);
                var forward=new Vector3(course[25]-course[15],0,course[27]-course[17]).normalized;
                var side=new Vector3(forward.z,0,-forward.x);
                var inspection=rt.chaseCamera.gameObject.AddComponent<ExperimentalMtxInspection>();
                inspection.target=target;inspection.position=target-forward*90+side*65+Vector3.up*85;
                yield return new WaitForSecondsRealtime(4);
                ScreenCapture.CaptureScreenshot(shot+".ruts.png");yield return new WaitForSecondsRealtime(2);
                Destroy(inspection);yield return null;
            }
            if(ok && Has("-reflexMtxScaleSmoke"))
            {
                var scaler=GetComponent<ExperimentalMtxScale>();
                var keyboard=UnityEngine.InputSystem.InputSystem.AddDevice<UnityEngine.InputSystem.Keyboard>();
                foreach(int target in new[]{ExperimentalMtxScale.Percent+5,ExperimentalMtxScale.Percent})
                {
                    var previous=rt;
                    var key=target>ExperimentalMtxScale.Percent ? UnityEngine.InputSystem.Key.RightBracket : UnityEngine.InputSystem.Key.LeftBracket;
                    UnityEngine.InputSystem.InputSystem.QueueStateEvent(keyboard,new UnityEngine.InputSystem.LowLevel.KeyboardState(UnityEngine.InputSystem.Key.LeftCtrl,UnityEngine.InputSystem.Key.LeftShift,key));
                    yield return null;yield return null;
                    UnityEngine.InputSystem.InputSystem.QueueStateEvent(keyboard,new UnityEngine.InputSystem.LowLevel.KeyboardState());
                    deadline=Time.realtimeSinceStartup+180;
                    do
                    {
                        yield return null;rt=FindAnyObjectByType<OriginalWorldRuntime>();
                    }while((!rt || rt==previous || !rt.Running) && Time.realtimeSinceStartup<deadline);
                    terrain=FindAnyObjectByType<OriginalTerrain>();scenery=FindAnyObjectByType<ExperimentalMtxView>();
                    bool scaleOk=rt && rt!=previous && rt.Running && terrain && terrain.Built && scenery && errors==0
                        && ExperimentalMtxScale.Percent==target
                        && Mathf.Abs(terrain.Info.spacing-2f*target/100f)<.001f
                        && Mathf.Abs(scenery.transform.localScale.x-target/100f)<.001f;
                    Debug.Log($"[MTX] Scale hotkey {(scaleOk?"PASS":"FAIL")}: {target} %, terrain spacing {terrain?.Info.spacing}, scenery scale {scenery?.transform.localScale.x}");
                    if(!scaleOk){ok=false;break;}
                    yield return new WaitForSecondsRealtime(2);
                }
                UnityEngine.InputSystem.InputSystem.RemoveDevice(keyboard);
                if(!string.IsNullOrEmpty(shot)){ScreenCapture.CaptureScreenshot(shot+".scale.png");yield return new WaitForSecondsRealtime(1);}
            }
            Application.Quit(ok?0:1);
        }
        /// Tests (-reflexMtxLightShots, REFLEX_MTX_SCREENSHOT=<prefix>): the chase view at the start and two raised views around it, in
        /// LIGHTING ORIGINAL then ENHANCED, each after the eye adaptation settled; then quits. The bike stands still at the start.
        IEnumerator LightShots(OriginalWorldRuntime rt)
        {
            string shot=Environment.GetEnvironmentVariable("REFLEX_MTX_SCREENSHOT");
            if(string.IsNullOrEmpty(shot)) { Debug.LogError("[MTX] REFLEX_MTX_SCREENSHOT not set"); Application.Quit(1); yield break; }
            var cam=rt.chaseCamera;
            yield return new WaitForSecondsRealtime(5);
            var level=FindAnyObjectByType<OriginalLevel>(); var pipe=cam ? cam.GetComponent<Reflex.Port.Rendering.ReflexRenderPipeline>() : null;
            if(level && level.Content!=null)
            {
                var l=level.Content.lighting; var h=pipe ? pipe.hdr : l.hdr;
                Debug.Log($"[MTX] light of {level.Content.level}: sun {l.sunDirection:F3} colour {l.sunColor:F3}, fills {l.lightColors[0]:F3} {l.lightColors[1]:F3} {l.lightColors[2]:F3} {l.lightColors[3]:F3} at {l.lightDirectionOffset:F0} deg, bounce {l.bounceColor:F3}, "
                    +$"fog {l.fogColorStart} -> {l.fogColorEnd} amount {l.fogAmountStart:F3}..{l.fogAmountEnd:F3} over {l.fogDistanceStart:F0}..{l.fogDistanceEnd:F0}; HDR key {h.keyValue:F3} adaptation {h.minAdaptation:F3}..{h.maxAdaptation:F3} "
                    +$"rate {h.adaptationRate:F3} bloom attn {h.bloomAttenuation:F2} lum {h.minBloomLuminance:F2}..{h.maxBloomLuminance:F2} whole {h.wholeSceneBloomAmount:F2}");
            }
            rt.TryGetBike(out var bike,out _);
            var fwd=cam ? cam.transform.forward : Vector3.forward; fwd.y=0; fwd=fwd.sqrMagnitude>1e-4f ? fwd.normalized : Vector3.forward;
            var side=new Vector3(fwd.z,0,-fwd.x);
            var views=new (string name,Vector3 pos,Vector3 target)[]
            {
                ("chase",Vector3.zero,Vector3.zero),
                ("over",bike-fwd*22+side*34+Vector3.up*24,bike+fwd*90),
                ("high",bike-fwd*45+Vector3.up*70,bike+fwd*160),
            };
            foreach(var mode in new[]{0,1})
            {
                Reflex.Port.Rendering.ReflexEnhancedLighting.Override=mode;
                foreach(var v in views)
                {
                    ExperimentalMtxInspection inspection=null;
                    if(v.name!="chase" && cam){inspection=cam.gameObject.AddComponent<ExperimentalMtxInspection>();inspection.position=v.pos;inspection.target=v.target;}
                    yield return new WaitForSecondsRealtime(4);
                    string path=$"{shot}.{(mode==0?"orig":"enh")}.{v.name}.png";
                    ScreenCapture.CaptureScreenshot(path);yield return new WaitForSecondsRealtime(1);
                    Debug.Log("[MTX] light shot "+path);
                    if(inspection)Destroy(inspection);
                    yield return null;
                }
            }
            Application.Quit(errors==0?0:1);
        }
    }
    // Test-only view; the rider simulation keeps advancing so the native bump fit follows this camera.
    // (LightShots, ExperimentalMtxLaunch: before / after lighting renders from fixed views around the start.)
    [DefaultExecutionOrder(30000)]
    public sealed class ExperimentalMtxInspection : MonoBehaviour
    {
        public Vector3 position,target;
        void LateUpdate() { transform.position=position;transform.LookAt(target); }
    }

}
