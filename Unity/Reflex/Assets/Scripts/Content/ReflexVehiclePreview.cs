using UnityEngine;

namespace Reflex.Port.Content
{
    // Inspection scene for the run-time bike and rider (BikeDiscovery): builds the MarsF Lite and the Answer-red rider from the
    // player's install at the origin (the rider beside the bike, in its bind pose) and frames the camera on them.
    public sealed class ReflexVehiclePreview : MonoBehaviour
    {
        public Shader vehicleShader;
        public bool showRider = true;
        ReflexBikeModel bike; ReflexRiderModel rider;

        void Start()
        {
            if (vehicleShader) ReflexVehicleShader.Override = vehicleShader;
            bike = ReflexBikeModel.Build();
            Bounds bounds = default; bool any = false;
            void Grow(GameObject g) { foreach (var r in g.GetComponentsInChildren<Renderer>()) { if (!any) { bounds = r.bounds; any = true; } else bounds.Encapsulate(r.bounds); } }
            Grow(bike.chassis); Grow(bike.frontWheel.gameObject); Grow(bike.rearWheel.gameObject);
            if (showRider)
            {
                rider = ReflexRiderModel.Build();
                rider.rider.ApplyBoneMatrices(rider.rider.bindWorldMatrices, OriginalWorld.OriginalRider.OriginalBoneCount);   // the bind pose
                rider.root.transform.position = new Vector3(-bounds.extents.x - 2.5f, 0, 0);
                Grow(rider.root);
            }
            var cam = Camera.main;
            if (cam)
            {
                float size = bounds.size.magnitude;
                cam.transform.position = bounds.center + new Vector3(1.1f, 0.36f, 0.65f) * size; cam.transform.LookAt(bounds.center);
                var dc = cam.GetComponent<DiscoveryCamera>();
                if (dc) { dc.overviewPosition = cam.transform.position; dc.overviewTarget = bounds.center; dc.moveSpeed = size * 0.4f; }
            }
        }

        void OnDestroy() { bike?.Destroy(); rider?.Destroy(); VehicleCatalog.ReleaseTextures(); }
    }
}
