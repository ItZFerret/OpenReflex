// TEST ONLY: the Free Ride activation notification (OwnActivationNotice) over a stand-in for the world.
using System;
using System.Collections;
using System.Linq;
using Reflex.Port.Content;
using Reflex.Port.OpenFrontEnd;
using UnityEngine;
using UnityEngine.UIElements;

namespace Reflex.Port.Showcase
{
    static class UiShowcaseActivation
    {
        [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
        static void Register() { UiShowcase.Register("activation", Activation); }

        // The Free Ride activation notification (OwnActivationArea draws it from the core's snapshot; here OwnActivationNotice directly): in an
        // area with the class requirement, in an area alone, and the requirement alone, over a stand-in for the world.
        static IEnumerator Activation()
        {
            UiShowcase.ShellVisible(false);
            var host = new GameObject("showcase activation");
            var backHost = new GameObject("showcase activation backdrop"); var back = OuUi.CreateDocument(backHost, 10);
            var sky = OuUi.Div("", back); sky.style.position = Position.Absolute; sky.style.left = 0; sky.style.right = 0; sky.style.top = 0; sky.style.bottom = 0;
            sky.style.backgroundColor = new Color(0.36f, 0.45f, 0.30f);
            var horizon = OuUi.Div("", back); horizon.style.position = Position.Absolute; horizon.style.left = 0; horizon.style.right = 0; horizon.style.top = 0; horizon.style.height = Length.Percent(38);
            horizon.style.backgroundColor = new Color(0.55f, 0.68f, 0.82f);
            var notice = new OwnActivationNotice(host.transform);
            var classes = CareerEventCatalog.Read().Select(e => e.VehicleClass).Distinct().ToList();
            int one = classes.Count > 0 ? classes[0] : 1, two = classes.Count > 4 ? classes[4] : one;
            notice.Show(true, one);
            yield return UiShowcase.Frames(50);
            Debug.Log("[ui-showcase] activation: " + notice.ActivationText + " | " + notice.RequirementText + " | caps " + notice.Glyphs.Describe());
            yield return UiShowcase.Shot("activation");
            notice.Show(true, two);
            yield return UiShowcase.Frames(30);
            Debug.Log("[ui-showcase] activation: " + notice.ActivationText + " | " + notice.RequirementText);
            yield return UiShowcase.Shot("activation-class2");
            notice.Show(true, -1);
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("activation-area");
            notice.Show(false, two);
            yield return UiShowcase.Frames(30);
            yield return UiShowcase.Shot("activation-requirement");
            notice.Dispose(); UnityEngine.Object.Destroy(host); UnityEngine.Object.Destroy(backHost);
            yield return UiShowcase.Frames(3);
        }
    }
}
