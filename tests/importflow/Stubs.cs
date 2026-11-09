// TEST ONLY (tests/importflow): the few game types the import's readers name, as the game defines them, so they build on .NET.
using System;

namespace Reflex.Port.Content
{
    internal static class BikeMods
    {
        public const string NamespaceMark = "mod:";
    }
    internal static class AliveLevels { public const string Registry = "alive-levels.json"; }
}
namespace Reflex.Port.RxCore.Bridge
{
    public static class OpenReflexSelector { public static bool UseOpenReflex = true; }
}
