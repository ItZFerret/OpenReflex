// TEST ONLY (w-p2scene): the setup page's RIDER tab with the vehicle preview (the 3D window): the default choice, each vehicle type of the event, a pro rider.
using System.Collections;
using System.Linq;
using System.Reflection;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseP2Scene
    {
        const BindingFlags NP = BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public;
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("setup-vehicle", SetupVehicle); }

        static OpenShell Shell() => OpenShell.Instance ?? throw new System.InvalidOperationException("no shell");
        static T Get<T>(string field) => (T)typeof(OpenShell).GetField(field, NP).GetValue(Shell());
        static void Call(string method, params object[] a) => typeof(OpenShell).GetMethod(method, NP).Invoke(Shell(), a);

        static IEnumerator SetupVehicle()
        {
            OpenShell.SkipIntro = true;
            for (int i = 0; i < 900 && Shell().MenuWorld == null; i++) yield return null;   // the 3D menu world is up
            ReflexProfile.Disabled = false;
            if (!CoreGarageProfile.Open()) throw new System.InvalidOperationException(CoreGarageProfile.Error);
            ReflexProfile.Open();
            Shell().ShowBrowser(false); yield return UiShowcase.Frames(40);
            Shell().ShowSetupPage(); yield return UiShowcase.Frames(40);
            Call("SetSetupTab", 1, false);
            yield return UiShowcase.Frames(150);
            yield return UiShowcase.Shot("setup-vehicle-own");
            var setup = Get<OpenSetup>("_setup"); var ev = Get<OpenEvent>("_browseEvent").ev;
            foreach (int type in OpenSetup.VehicleTypes(ev))
            {
                setup.vehicleChoiceExplicit = true; setup.vehicleType = type; setup.vehicleClass = 0; setup.rider = -1;
                if (OpenSetup.Riders(ev, type).All(r => r.id != -1)) setup.rider = OpenSetup.Riders(ev, type).First().id;
                Call("SetupChanged", true);
                yield return UiShowcase.Frames(170);
                yield return UiShowcase.Shot("setup-vehicle-type" + type + (setup.rider >= 0 ? "-pro" : "-own"));
            }
            var pros = OpenSetup.Riders(ev, setup.Type(ev)).Where(r => r.id >= 0).ToList();
            if (pros.Count > 0)
            {
                setup.rider = pros[0].id; setup.vehicleChoiceExplicit = true; Call("SetupChanged", false);
                yield return UiShowcase.Frames(170);
                yield return UiShowcase.Shot("setup-vehicle-pro");
            }
            Call("SetSetupTab", 0, false); yield return UiShowcase.Frames(60);
            yield return UiShowcase.Shot("setup-event-back");
            Shell().ShowMain(); yield return UiShowcase.Frames(5);
        }
    }
}
