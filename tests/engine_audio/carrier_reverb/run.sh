#!/bin/sh
# run.sh <ABSOLUTE scratch dir> -- builds CarrierReverbProbe.cs into a minimal scratch macOS player (batchmode, Unity 6000.6.1f1, the built-in
# modules only) and runs it with -batchmode -nographics (no window, no focus): prints the master output's level of an ordinary clip source, the
# engine pack's constant-1 carrier source (before the fix: through the reverb zone) and the fixed carrier (zone bypassed + AudioReverbFilter), with no
# reverb and with the first-person camera's, Mountains and Plain reverbs. A pack source must read x1.0 of the clip source's level in every row.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?absolute scratch dir}; U=${UNITY:-/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/MacOS/Unity}
R="$HERE/../../../Unity/Reflex"
mkdir -p "$S/Assets/Editor" "$S/Packages" "$S/ProjectSettings" "$S/out"
cp "$R/ProjectSettings/ProjectVersion.txt" "$R/ProjectSettings/AudioManager.asset" "$S/ProjectSettings/"
python3 - "$R/Packages/manifest.json" "$S/Packages/manifest.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
json.dump({"dependencies": {k: v for k, v in m["dependencies"].items() if k.startswith("com.unity.modules")}}, open(sys.argv[2], "w"))
PY
cp "$HERE/CarrierReverbProbe.cs" "$S/Assets/"
cat > "$S/Assets/Editor/Build.cs" <<'CS'
using UnityEditor;
using UnityEditor.SceneManagement;
public static class Build
{
    public static void Player()
    {
        var sc = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
        EditorSceneManager.SaveScene(sc, "Assets/S.unity");
        var r = BuildPipeline.BuildPlayer(new BuildPlayerOptions { scenes = new[] { "Assets/S.unity" }, locationPathName = System.Environment.GetEnvironmentVariable("PROBE_OUT") + "/probe.app", target = BuildTarget.StandaloneOSX });
        EditorApplication.Exit(r.summary.result == UnityEditor.Build.Reporting.BuildResult.Succeeded ? 0 : 1);
    }
}
CS
PROBE_OUT="$S/out" "$U" -batchmode -nographics -quit -projectPath "$S" -executeMethod Build.Player -logFile "$S/build.log"
"$S"/out/probe.app/Contents/MacOS/* -batchmode -nographics -logFile "$S/run.log" > /dev/null
grep '^PROBE' "$S/run.log"
