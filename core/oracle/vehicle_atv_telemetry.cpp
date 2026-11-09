#include "oracle.h"
extern "C" {
#include "../game/vehicle_atv_telemetry.h"
}
using namespace reflex_port;
namespace {void telemetry_push(reflex_vm::Machine& m,rx_f80 r){reflex_f80::F80 v;v.kind=r.kind;v.sign=r.sign;v.mant=r.mant;v.exp=r.exp;m.fpu.push_back(v);}}
RX_PORT(0x00486620,AtvEngineRpm,"rx_AtvTelemetry",OUT_FPU){telemetry_push(m,rx_atv_engine_rpm(m.r[1]));return m.ret(0);}
RX_PORT(0x00486850,AtvGripFactor,"rx_AtvTelemetry",OUT_FPU){telemetry_push(m,rx_atv_grip_factor(m.r[1],m.arg(0)));return m.ret(4);}
RX_PORT(0x00486cc0,AtvSnapshotVectors,"rx_AtvTelemetry",0){rx_atv_snapshot_vectors(m.r[1],m.arg(0));return m.ret(4);}
RX_PORT(0x00487510,AtvSnapshotValues,"rx_AtvTelemetry",0){rx_atv_snapshot_values(m.r[1],m.arg(0));return m.ret(4);}
extern "C" void rx_vehicle_forward_angular_velocity(rx_addr,rx_addr);
RX_PORT(0x00430250,VehicleForwardAngularVelocity,"rx_AtvTelemetry",0){rx_vehicle_forward_angular_velocity(m.r[1],m.arg(0));return m.ret(4);}
