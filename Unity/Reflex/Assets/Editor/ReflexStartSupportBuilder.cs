using UnityEngine;

namespace Reflex.Port.Editor
{
    // The Alpine hovercraft under the free-ride start (HC_Static) is a static entity of the FreeRide scene; OriginalLevel loads it from
    // the install at run time together with the rest of the level's scenery (the FreeRide activity scene is merged with the Base scene).
    public static class ReflexStartSupportBuilder
    {
        // kept for the legacy validation scripts, which describe the former baked assets

        /// Ensures the scene has the Alpine OriginalLevel (which includes the hovercraft); returns its game object.
        public static GameObject AddOriginalHovercraft() => ReflexSceneryBuilder.EnsureLevel("FR_Alpine").gameObject;
    }
}
