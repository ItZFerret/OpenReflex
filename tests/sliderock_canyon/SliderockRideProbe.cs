using System;
using System.Collections;
using System.IO;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using Reflex.Port.OriginalWorld;
using UnityEngine;
using UnityEngine.UIElements;
[DefaultExecutionOrder(32000)]
public sealed class SliderockRideProbe : MonoBehaviour
{
    string shots; int errors; Camera inspectionCamera; Vector3 inspectPosition,inspectTarget;
    void LateUpdate() { if(inspectionCamera) { inspectionCamera.transform.position=inspectPosition; inspectionCamera.transform.LookAt(inspectTarget); } }
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
    static void Guest() { if (Environment.GetEnvironmentVariable("REFLEX_SLIDEROCK_TEST") == "1") ReflexProfile.Disabled = true; }
    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    static void Boot() {
        if (Environment.GetEnvironmentVariable("REFLEX_SLIDEROCK_TEST") != "1" || FindAnyObjectByType<SliderockRideProbe>()) return;
        var go = new GameObject("Sliderock ride acceptance"); DontDestroyOnLoad(go); go.AddComponent<SliderockRideProbe>(); Application.runInBackground = true;
    }
    void OnEnable() { Application.logMessageReceived += Log; }
    void OnDisable() { Application.logMessageReceived -= Log; }
    void Log(string s, string trace, LogType type) { if (type == LogType.Exception) errors++; }
    VisualElement Find(string name) => FindObjectsByType<UIDocument>(FindObjectsSortMode.None).Select(d => d.rootVisualElement?.Q<VisualElement>(name)).FirstOrDefault(v => v != null && v.panel != null);
    IEnumerator Shot(string name) { yield return new WaitForEndOfFrame(); ScreenCapture.CaptureScreenshot(Path.Combine(shots, name + ".png")); yield return new WaitForSecondsRealtime(1); }
    IEnumerator Click(string name) {
        float t = Time.realtimeSinceStartup; VisualElement v;
        while ((v = Find(name)) == null || !v.enabledInHierarchy || v.worldBound.width < 1) {
            if (Time.realtimeSinceStartup-t > 120) { Fail("Missing control " + name); yield break; } yield return null;
        }
        yield return new WaitForSecondsRealtime(2);
        // the menu can rebuild its panel during the wait (slow frames on a loaded machine): pick on the current element
        while ((v = Find(name)) == null || v.panel == null || !v.enabledInHierarchy || v.worldBound.width < 1) {
            if (Time.realtimeSinceStartup-t > 120) { Fail("Missing control " + name); yield break; } yield return null;
        }
        var picked = v.panel.Pick(v.worldBound.center);
        if (picked != v && !v.Contains(picked)) { Fail("Obscured control " + name); yield break; }
        var trigger = new Event { type = EventType.MouseUp, button = 0, clickCount = 1, mousePosition = v.worldBound.center };
        using (var click = PointerEventBase<ClickEvent>.GetPooled(trigger)) { click.target = v; v.SendEvent(click); }
        Debug.Log("[sliderock-test] CLICK " + name); yield return new WaitForSecondsRealtime(2);
    }
    IEnumerator Start() {
        shots=Environment.GetEnvironmentVariable("REFLEX_PREVIEW_SHOTS"); Directory.CreateDirectory(shots);
        yield return new WaitForSecondsRealtime(10);
        while (Find("ou-hub-play") == null) yield return null;
        yield return new WaitForSecondsRealtime(3);
        yield return Shot("main-menu"); yield return Click("ou-hub-play"); yield return Click("ou-hub-freeride"); OpenShell.Instance.SelectBrowse("*alive"); yield return new WaitForSecondsRealtime(1);
        if (OpenShell.Instance.Browse != "*alive" || OpenShell.Instance.VisibleCards != 1 || OpenShell.Instance.SelectedEvent?.Location != "FR_DLC_Hills") { Fail("Alive category/selection mismatch"); yield break; }
        yield return Shot("alive-category"); yield return Click("ou-setup-next"); yield return Shot("setup"); yield return Click("ou-setup-start");
        float t=Time.realtimeSinceStartup; OriginalWorldRuntime rt=null;
        while (!(rt && rt.Running)) {
            rt=FindAnyObjectByType<OriginalWorldRuntime>();
            if (rt && !string.IsNullOrEmpty(rt.FailureMessage)) { Fail(rt.FailureMessage); yield break; }
            if (Time.realtimeSinceStartup-t>300) { Fail("Ride load timed out"); yield break; } yield return null;
        }
        while(rt.Bike.frames < 200) { if(Time.realtimeSinceStartup-t>300){Fail("Countdown stalled");yield break;} yield return null; }
        yield return Shot("spawn");
        var level=FindObjectsByType<OriginalLevel>(FindObjectsSortMode.None).First(x=>x.level=="FR_DLC_Hills");
        Debug.Log("[sliderock-test] ENV forest="+level.Content.forest?.treeCount+" grass="+level.Content.grass?.cells+" water="+ReflexWater.Load(level.Content).Describe());
        Vector3 start = new Vector3(rt.Bike.position[0],rt.Bike.position[1],rt.Bike.position[2]);
        float maxDistance=0,maxSpeed=0, priorReset=0; int resets=0, logged=-1; uint driveStart=rt.Bike.frames; t=Time.realtimeSinceStartup; bool throttling=true;
        while(rt.Bike.frames-driveStart<480) {
            if (!rt.Running) { Fail(rt.FailureMessage); yield break; }
            if(Time.realtimeSinceStartup-t>300){Fail("Ride frame progression stalled");yield break;}
            // throttle for 120 game frames (as the native reset regression: ~58 ft/s), released early at 58 ft/s: on a heavily loaded machine
            // the presentation merges real time into fewer, longer game frames and 120 of them would reach the campsite rocks
            if (throttling && (rt.Bike.frames-driveStart>=120 || rt.Bike.speed>=58f)) throttling=false;
            rt.testHeldKeys=throttling ? new[]{0x11} : new[]{0x1f}; var b=rt.Bike;
            int sec=(int)(Time.realtimeSinceStartup-t); if(sec!=logged) { logged=sec; Debug.Log("[sliderock-test] DRIVE t="+sec+" frames="+b.frames+" speed="+b.speed+" input="+string.Join(",",b.inputs)+" pos="+string.Join(",",b.position)); }
            float reset=rt.Race.reset_timer; if(reset>priorReset+0.1f) resets++; priorReset=reset;
            if (b.position.Any(x=>float.IsNaN(x)||float.IsInfinity(x))) { Fail("Nonfinite bike pose"); yield break; }
            maxDistance=Mathf.Max(maxDistance,Vector3.Distance(start,new Vector3(b.position[0],b.position[1],b.position[2]))); maxSpeed=Mathf.Max(maxSpeed,b.speed);
            if(rt.Bike.frames-driveStart>180 && !File.Exists(Path.Combine(shots,"ride.png"))) yield return Shot("ride");
            yield return null;
        }
        rt.testHeldKeys=Array.Empty<int>(); yield return Shot("ride-end");
        if(errors>0||maxDistance<50||maxSpeed<3||resets>0) { Fail("Ride evidence: exceptions="+errors+" resets="+resets+" distance="+maxDistance+" speed="+maxSpeed); yield break; }
        inspectionCamera=rt.chaseCamera;
        var trees=level.Content.forest.trees; int nearest=0; float best=float.MaxValue;
        for(int i=0;i<trees.Length;i+=8) { float d=Vector3.Distance(new Vector3(1450,650,-1300),new Vector3(trees[i],trees[i+1],trees[i+2])); if(d<best){best=d;nearest=i;} }
        Vector3 tree=new Vector3(trees[nearest],trees[nearest+1],trees[nearest+2]);
        yield return View("vegetation", tree+new Vector3(0,13,-35), tree+new Vector3(0,5,0));
        yield return View("vegetation-close", tree+new Vector3(-6,7,-16), tree+new Vector3(0,6,0));
        yield return View("vegetation-far", tree+new Vector3(-60,60,-260), tree);
        Vector3 bike=new Vector3(rt.Bike.position[0],rt.Bike.position[1],rt.Bike.position[2]);
        Vector3 sun=Shader.GetGlobalVector("_RfxSunDir"); sun.y=0; sun=sun.sqrMagnitude>1e-6f?sun.normalized:Vector3.forward;
        yield return View("grass", bike+new Vector3(0,3,0)-sun*6, bike+new Vector3(0,1,0)+sun*60);
        yield return View("sky-sun", bike+new Vector3(0,8,0), bike+new Vector3(0,40,0)+sun*100);
        yield return View("sky-away", bike+new Vector3(0,8,0), bike+new Vector3(0,30,0)-sun*100);
        yield return View("camp", new Vector3(-1068,693,-676), new Vector3(-1084,688,-697));   // BBQ smoke / flame emitters (ParticleRSData)
        var parts=FindAnyObjectByType<OriginalParticles>();
        Debug.Log("[sliderock-test] PARTICLES live="+(parts?parts.Live:-1)+" normal="+(parts?parts.Normal:-1)+" additive="+(parts?parts.Additive:-1));
        if (parts && parts.LastParticles!=null) {
            var bbq=new Vector3(-1084.4f,684.1f,-697.4f); int near=0; var cells=new System.Collections.Generic.Dictionary<string,int>();
            for (int k=0;k<parts.LastCount&&k<parts.LastParticles.Length;k++) { var q=parts.LastParticles[k]; var pp=new Vector3(q.px,q.py,q.pz);
                if (Vector3.Distance(pp,bbq)<60f) near++; string key=Mathf.Round(q.px/100)*100+","+Mathf.Round(q.pz/100)*100; cells[key]=cells.TryGetValue(key,out int c)?c+1:1; }
            Debug.Log("[sliderock-test] PARTICLES drawn="+parts.LastCount+" within 60 ft of BBQ_Smoke_1="+near+" by 100-ft cell: "+string.Join(" ",cells.OrderByDescending(kv=>kv.Value).Take(8).Select(kv=>kv.Key+"x"+kv.Value)));
        }
        var lods=FindAnyObjectByType<OriginalSceneryLod>(); var rock=lods?.Instances?.Where(x=>x.model!=null&&x.model.name.Contains("Rock")&&x.renderer)
            .OrderBy(x=>Vector3.Distance(x.renderer.bounds.center,bike)).FirstOrDefault();
        if (rock!=null) {
            Vector3 c=rock.renderer.bounds.center, toward=(bike-c); toward.y=0; toward=toward.sqrMagnitude>1e-4f?toward.normalized:Vector3.forward;
            Vector3 eye=c+toward*(rock.renderer.bounds.extents.magnitude*0.9f+20f)+Vector3.up*8f;
            inspectPosition=eye; inspectTarget=c; for(int k=0;k<10;k++) yield return null;
            foreach (var li in lods.Instances.Where(x=>x.model!=null&&x.renderer&&!x.renderer.forceRenderingOff&&Vector3.Distance(x.renderer.bounds.center,eye)<400f))
                Debug.Log("[sliderock-test] LOD "+li.model.name+" lod="+li.lod+"/"+li.model.lodDistances.Length+" dist="+Vector3.Distance(li.renderer.bounds.center,eye).ToString("F0")+" tris="+(li.filter&&li.filter.sharedMesh?li.filter.sharedMesh.triangles.Length/3:0)+" lodDist="+string.Join(",",li.model.lodDistances));
            yield return View("rocks", eye, c);
        } else Debug.Log("[sliderock-test] LOD no rock instance found");
        yield return View("vista", bike+new Vector3(0,45,0)-sun*20, bike+new Vector3(0,20,0)-sun*400);
        yield return View("water", new Vector3(1450,650,-1300), new Vector3(1800,595,-1500));
        yield return View("lake-top", new Vector3(1799,1900,-1500), new Vector3(1800,595,-1512));
        yield return View("water-close", new Vector3(1700,625,-1420), new Vector3(1800,597,-1500));
        inspectionCamera=null;
        Debug.Log("[sliderock-test] PASS actual menu + FreeRide; resets="+resets+" frames="+(rt.Bike.frames-driveStart)+"; distance="+maxDistance+" speed="+maxSpeed); Application.Quit(0);
    }
    IEnumerator View(string name, Vector3 position, Vector3 target) {
        var terrain=Terrain.activeTerrain;   // keep inspection cameras above the ground (a view from inside a hill shows backfaces)
        if (terrain) { float g=terrain.SampleHeight(new Vector3(position.x,0,position.z))+terrain.transform.position.y; if (position.y<g+4f) position.y=g+4f; }
        inspectPosition=position; inspectTarget=target;
        int f0=Time.frameCount; float t0=Time.realtimeSinceStartup;
        while(Time.frameCount-f0<180 && Time.realtimeSinceStartup-t0<20) yield return null;   // 3 s of frames at the 60 fps cap: exposure adapts
        yield return LogAdaptation(name);
        yield return Shot(name);
    }
    // the eye adaptation's adapted luminance (the 1x1 R32F target of the HDR post chain) for the view: exposure = 2 key / it
    IEnumerator LogAdaptation(string name) {
        var pipe=FindObjectsByType<Reflex.Port.Rendering.ReflexRenderPipeline>(FindObjectsSortMode.None).FirstOrDefault(x=>x.isActiveAndEnabled&&x.AdaptedLuminance!=null);
        var rt=pipe?.AdaptedLuminance?.rt; if(!rt) yield break;
        var req=UnityEngine.Rendering.AsyncGPUReadback.Request(rt); float t0=Time.realtimeSinceStartup;
        while(!req.done && Time.realtimeSinceStartup-t0<5) yield return null;
        if(req.done && !req.hasError) Debug.Log("[sliderock-test] ADAPT "+name+" luminance="+req.GetData<float>()[0].ToString("G4")+" key="+pipe.hdr.keyValue+" min="+pipe.hdr.minAdaptation+" max="+pipe.hdr.maxAdaptation);
    }
    void Fail(string reason) { Debug.LogError("[sliderock-test] FAIL " + reason); Application.Quit(1); }
}
