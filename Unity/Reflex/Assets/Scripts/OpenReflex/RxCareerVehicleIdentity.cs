using System.Runtime.InteropServices;
namespace Reflex.Port.RxCore
{
    // Copied imported native identity, not a managed garage/global selection.
    [StructLayout(LayoutKind.Sequential)]
    public struct RxCareerVehicleIdentity
    {
        public uint size, version;
        public ulong generation;
        public int player_row, vehicle_list_id, owner, owner_vehicle_id, model_id, vehicle_type;
        public int authored_class, resolved_class;
        public static RxCareerVehicleIdentity Create() => new RxCareerVehicleIdentity { size = 48, version = 1 };
    }
}
